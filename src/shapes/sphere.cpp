#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>

namespace gnd {

    class Sphere : public Shape {
    public:
        explicit Sphere(const PropertyList& props) : Shape(props) {
            m_radius = props.getFloat("radius", 1.0f);
        }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const override {
            Vector3f originVector = ray.o - Point3f(0.0f);

            float A = ray.d.lengthSquared();
            float B = 2.0f * Dot(originVector, ray.d);
            float C = originVector.lengthSquared() - (m_radius * m_radius);

            float t0, t1;
            if (!SolveQuadratic(A, B, C, t0, t1)) return false;

            float tHit;

            if (t0 >= ray.tMin && t0 < ray.tMax) {
                tHit = t0;
                if (predicate) return true;
            } else if (t1 >= ray.tMin && t1 < ray.tMax) {
                tHit = t1;
                if (predicate) return true;
            } else
                return false;

            isect.t = tHit;
            isect.p = ray.o + ray.d * tHit;
            isect.n = Normal3f(Normalize(isect.p - Point3f(0.0f)));

            return true;
        }

        void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override {
            isect.p = ray.o + ray.d * isect.t;
            isect.n = Normal3f(Normalize(isect.p - Point3f(0.0f)));
        }

        Bounds3f getBounds() const override {
            Bounds3f objectBounds(
                Point3f(-m_radius),
                Point3f(m_radius)
            );

            return objectBounds;
        }

        std::string toString() const override {
            return std::format("Sphere[radius={:.2f}]", m_radius);
        }

    private:
        float m_radius;
    };

    GND_REGISTER_CLASS(Sphere, "sphere");
}