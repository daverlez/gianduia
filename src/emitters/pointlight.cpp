#include "gianduia/core/emitter.h"
#include "gianduia/core/factory.h"
#include "gianduia/shapes/primitive.h"
#include "gianduia/textures/texture.h"

namespace gnd {

    class PointLight : public Emitter {
    public:
        PointLight(const PropertyList& props) : Emitter(EMITTER_DELTA) {
            m_position = props.getPoint3("position");
            m_power = props.getColor("power", Color3f(10.0f));
        }

        Color3f eval(const SurfaceInteraction &isect, const Vector3f &w) const override {
            return Color3f(0.0f);   // Delta emitters evaluate to zero
        }

        virtual Color3f sample(const SurfaceInteraction& ref, const Point2f& sample,
                               SurfaceInteraction& info, float& pdf, Ray& shadowRay) const {
            Color3f I = m_power * Inv4Pi;
            float squaredDist = (m_position - ref.p).lengthSquared();

            info.p = m_position;
            pdf = 1.0f;
            shadowRay.o = ref.p;
            shadowRay.d = Normalize(info.p - ref.p);
            shadowRay.tMin = Epsilon;
            shadowRay.tMax = (info.p - ref.p).length() - Epsilon;

            return I / squaredDist;
        }

        virtual float pdf(const SurfaceInteraction& ref, const SurfaceInteraction& info) const {
            return 0.0f;                // Shall check if the emitter is delta instead of evaluating the pdf
        }

        std::string toString() const override {
            return std::format(
                "AreaLight[\n"
                        "  power = {}\n"
                        "]",
                        m_power.toString());
        }

    private:
        Point3f m_position;
        Color3f m_power;
    };

    GND_REGISTER_CLASS(PointLight, "point");
}
