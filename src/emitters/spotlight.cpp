#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/shapes/primitive.h"
#include <cmath>

namespace gnd {

    class SpotLight : public Emitter {
    public:
        SpotLight(const PropertyList& props) : Emitter(EMITTER_DELTA) {
            m_position = props.getPoint3("position");
            m_direction = Normalize(props.getVector3("direction", Vector3f(0.0f, 0.0f, -1.0f)));
            m_power = props.getColor("power", Color3f(10.0f));

            float spotSize = props.getFloat("spot_size", 45.0f);
            float blend = props.getFloat("blend", 0.15f);

            float halfAngle = spotSize * 0.5f;
            float innerAngle = halfAngle * (1.0f - blend);

            m_cosTotalWidth = std::cos(Radians(halfAngle));
            m_cosFalloffStart = std::cos(Radians(innerAngle));
        }

        Color3f eval(const SurfaceInteraction &isect, const Vector3f &w) const override {
            return Color3f(0.0f);
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const override {

            Vector3f wi = Normalize(m_position - ref.p);
            float dist = (m_position - ref.p).length();
            float squaredDist = dist * dist;

            info.p = m_position;
            pdf = 1.0f;
            shadowRay.o = ref.p;
            shadowRay.d = wi;
            shadowRay.tMin = Epsilon;
            shadowRay.tMax = dist - Epsilon;
            shadowRay.time = ref.time;

            float cosTheta = Dot(-wi, m_direction);
            if (cosTheta <= m_cosTotalWidth) {
                return Color3f(0.0f);
            }

            float falloff = 1.0f;

            if (cosTheta < m_cosFalloffStart) {
                float t = (cosTheta - m_cosTotalWidth) / (m_cosFalloffStart - m_cosTotalWidth);
                falloff = (t * t) * (3.0f - 2.0f * t);
            }

            Color3f I = m_power * Inv4Pi;
            return (I * falloff) / squaredDist;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const override {
            return 0.0f;
        }

        std::string toString() const override {
            return std::format(
                "SpotLight[\n"
                "  position = {},\n"
                "  direction = {},\n"
                "  power = {},\n"
                "  cos_width = {},\n"
                "  cos_falloff = {}\n"
                "]",
                m_position.toString(), 
                m_direction.toString(), 
                m_power.toString(),
                m_cosTotalWidth,
                m_cosFalloffStart);
        }

    private:
        Point3f m_position;
        Vector3f m_direction;
        Color3f m_power;
        float m_cosTotalWidth;
        float m_cosFalloffStart;
    };

    GND_REGISTER_CLASS(SpotLight, "spot");
}