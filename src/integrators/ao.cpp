
#include "gianduia/core/factory.h"
#include "gianduia/core/integrator.h"
#include "gianduia/math/warp.h"

namespace gnd {
    class AmbientOcclusion : public SamplerIntegrator {
    public:
        AmbientOcclusion(const PropertyList& props) : SamplerIntegrator(props) {
            m_length = props.getFloat("length", 5.0f);
        }

        Color3f Li(const Ray& ray, Scene& scene, Sampler& sampler, MemoryArena& arena,
            Color3f* outAlbedo, Normal3f* outNormal, float* outDepth = nullptr) const override {
            SurfaceInteraction isect;

            if (!scene.rayIntersect(ray, isect))
                return Color3f(1.0f);

            Vector3f localDir = Warp::squareToCosineHemisphere(sampler.next2D());

            Normal3f n = isect.n;
            Vector3f s, t;

            if (std::abs(n.x()) > 0.1f) {
                s = Normalize(Cross(Vector3f(0.0f, 1.0f, 0.0f), n));
            } else {
                s = Normalize(Cross(Vector3f(1.0f, 0.0f, 0.0f), n));
            }
            t = Cross(n, s);

            Vector3f worldDir = s * localDir.x() + t * localDir.y() + n * localDir.z();

            Ray secondary(isect.p, worldDir, m_length);

            if (scene.rayIntersect(secondary))
                return Color3f(0.0f);

            return Color3f(1.0f);
        }

        std::string toString() const override {
            return std::format("AmbientOcclusionIntegrator[length={}]", m_length);
        }

    private:
        float m_length;
    };

    GND_REGISTER_CLASS(AmbientOcclusion, "ao");
}
