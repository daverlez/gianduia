#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"

namespace gnd {

    class NormalIntegrator : public SamplerIntegrator {
    public:
        NormalIntegrator(const PropertyList& props) : SamplerIntegrator(props) {}

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena, Color3f* outAlbedo, Normal3f* outNormal) const override {
            SurfaceInteraction isect;

            if (scene.rayIntersect(ray, isect)) {
                return Color3f(
                    isect.n.x() * 0.5f + 0.5f,
                    isect.n.y() * 0.5f + 0.5f,
                    isect.n.z() * 0.5f + 0.5f
                );
            }

            return Color3f(0.0f);
        }

        std::string toString() const override {
            return "NormalIntegrator[]";
        }
    };

    GND_REGISTER_CLASS(NormalIntegrator, "normals");

}