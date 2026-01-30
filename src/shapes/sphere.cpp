#include <gianduia/core/shape.h>
#include <gianduia/core/factory.h>

namespace gnd {

    class Sphere : public Shape {
    public:
        explicit Sphere(const PropertyList& props) : Shape(props) {
            m_radius = props.getFloat("radius", 1.0f);
        }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect) const override {
            Vector3f originVector = ray.o - Point3f(0.0f);

            float A = ray.d.lengthSquared();
            float B = 2.0f * Dot(originVector, ray.d);
            float C = originVector.lengthSquared() - (m_radius * m_radius);

            float discrim = B * B - 4.0f * A * C;
            if (discrim < 0.0f)
                return false;

            float rootDiscrim = std::sqrt(discrim);
            float q = (B < 0) ? -0.5f * (B - rootDiscrim) : -0.5f * (B + rootDiscrim);

            float t0 = q / A;
            float t1 = C / q;

            if (t0 > t1) std::swap(t0, t1);

            float tHit;

            if (t0 >= ray.tMin && t0 < ray.tMax)
                tHit = t0;
            else if (t1 >= ray.tMin && t1 < ray.tMax)
                tHit = t1;
            else
                return false;

            isect.t = tHit;
            isect.p = ray.o + ray.d * tHit;
            isect.n = Normal3f(Normalize(isect.p - Point3f(0.0f)));

            return true;
        }

        Bounds3f getBounds() const override {
            Bounds3f objectBounds(
                Point3f(-m_radius),
                Point3f(m_radius)
            );

            return objectBounds;
        }

        std::string toString() const override {
            return "Sphere[radius=" + std::to_string(m_radius) + "]";
        }

    private:
        float m_radius;
    };

    GND_REGISTER_CLASS(Sphere, "sphere");
}