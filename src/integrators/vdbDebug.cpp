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

        Color3f Li(const Ray& primaryRay, Scene& scene, Sampler& sampler, MemoryArena& arena) const override {
            Ray r = primaryRay;
            float accumulatedDensity = 0.0f;
            
            // Allow stepping through boundary surfaces (shapes with null BSDFs)
            int bounces = 0;
            while (bounces < 10) { // Safety iteration limit
                SurfaceInteraction isect;
                bool hit = scene.rayIntersect(r, isect);

                // Truncate ray traversal to the closest surface intersection
                if (hit) {
                    r.tMax = isect.t;
                }

                if (r.medium) {
                    // March through the current medium segment via fixed step size
                    float t = r.tMin;
                    float tEnd = hit ? r.tMax : std::min(r.tMax, 1000.0f); // Protect against infinite bounds
                    
                    while (t < tEnd) {
                        Point3f p = r.o + r.d * t;
                        
                        // NOTE: You will need to expose a density query method in your Medium 
                        // interface or cast r.medium to your specific VDB Medium implementation.
                        float density = r.medium->getDensity(p);
                        
                        //float density = 1.0f; // TODO: Replace this with your OpenVDB density lookup
                        
                        accumulatedDensity += density * stepSize;
                        t += stepSize;
                    }
                }

                if (!hit) break;

                // Check if we hit a medium boundary (null BSDF)
                isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);
                if (!isect.bsdf) {
                    r = Ray(isect.p, r.d);
                    r.medium = isect.getMedium(r.d);
                } else {
                    // Hit a solid surface, stop accumulating density.
                    break;
                }
                bounces++;
            }

            // Map accumulated density (optical thickness) to a grayscale visualization.
            // Returns opacity (1.0 = completely dense, 0.0 = completely empty).
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