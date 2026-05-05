#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/shapes/primitive.h"

namespace gnd {

    class DirectionalLight : public Emitter {
    public:
        DirectionalLight(const PropertyList& props) : Emitter(EMITTER_DELTA) {
            m_direction = -Normalize(props.getVector3("direction", Vector3f(0.0f, -1.0f, 0.0f)));
            m_irradiance = props.getColor("irradiance", Color3f(1.0f));
        }

        Color3f eval(const SurfaceInteraction &isect, const Vector3f &w) const override {
            return Color3f(0.0f);
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const override {
            Vector3f wi = m_direction;

            info.p = ref.p + wi * 1e5f; 
            info.time = ref.time;
            
            pdf = 1.0f;

            shadowRay.o = ref.p;
            shadowRay.d = wi;
            shadowRay.tMin = Epsilon;
            shadowRay.tMax = std::numeric_limits<float>::max();
            shadowRay.time = ref.time;

            return m_irradiance;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const override {
            return 0.0f;
        }

        std::string toString() const override {
            return std::format(
                "DirectionalLight[\n"
                        "  direction_to_light = {}\n"
                        "  irradiance = {}\n"
                        "]",
                        m_direction.toString(),
                        m_irradiance.toString());
        }

    private:
        Vector3f m_direction;
        Color3f m_irradiance;
    };

    GND_REGISTER_CLASS(DirectionalLight, "directional");
}