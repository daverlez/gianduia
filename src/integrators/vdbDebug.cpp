#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/materials/medium.h"

namespace gnd {

    class VdbDebugIntegrator : public SamplerIntegrator {
    public:
        VdbDebugIntegrator(const PropertyList& props) : SamplerIntegrator(props) {
            stepSize = props.getFloat("stepSize", 0.1f);
            densityScale = props.getFloat("densityScale", 1.0f);
        }

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena, Color3f* outAlbedo, Color3f* outNormal) const override {
            Ray r = primaryRay;
            float accumulatedDensity = 0.0f;

            int bounces = 0;
            while (bounces < 10) {
                SurfaceInteraction isect;
                bool hit = scene.rayIntersect(r, isect);

                if (hit) {
                    r.tMax = isect.t;
                }

                if (r.medium) {
                    float t = r.tMin;
                    float tEnd = hit ? r.tMax : std::min(r.tMax, 1000.0f);
                    
                    while (t < tEnd) {
                        Point3f p = r.o + r.d * t;
                        float density = r.medium->getDensity(p);
                        
                        accumulatedDensity += density * stepSize;
                        t += stepSize;
                    }
                }

                if (!hit) break;

                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);
                if (!isect.bsdf) {
                    r = Ray(isect.p, r.d);
                    r.medium = isect.getMedium(r.d);
                } else {
                    break;
                }
                bounces++;
            }

            float transmittance = std::exp(-accumulatedDensity * densityScale);
            return Color3f(1.0f - transmittance);
        }

        std::string toString() const override {
            return "VdbDebugIntegrator[stepSize=" + std::to_string(stepSize) + "]";
        }

    private:
        float stepSize;
        float densityScale;
    };

    GND_REGISTER_CLASS(VdbDebugIntegrator, "vdbdebug");
}