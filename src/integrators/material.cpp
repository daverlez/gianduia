#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class MaterialPreview : public SamplerIntegrator {
    public:
        MaterialPreview(const PropertyList& props) : SamplerIntegrator(props) {}

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena,
            AOVRecord* aovs = nullptr) const override {
            SurfaceInteraction isect;

            if (!scene.rayIntersect(ray, isect))
                return Color3f(0.0f);

            isect.primitive->getMaterial()->computeScatteringFunctions(isect, arena);
            return isect.bsdf->f(-ray.d, isect.bsdf->frame.z);
        }

        std::string toString() const override {
            return "MaterialPreviewIntegrator[]";
        }

    private:
        float m_length;
    };

    GND_REGISTER_CLASS(MaterialPreview, "materialPreview");
}
