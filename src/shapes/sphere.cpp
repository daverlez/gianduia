#include <gianduia/core/shape.h>
#include <gianduia/core/factory.h>

namespace gnd {

    class Sphere : public Shape {
    public:
        explicit Sphere(const PropertyList& props) : Shape(props) {
            m_radius = props.getFloat("radius", 1.0f);
        }

        bool rayIntersect(const Ray& ray, float& tHit) const override {
            Ray rLocal = m_objectToWorld.inverse()(ray);
            rLocal.d = Normalize(rLocal.d);

            Vector3f centerDistance = Point3f(0.0f) - rLocal.o;        // ray origin -> center vector
            float rayDistance = Dot(centerDistance, rLocal.d);           // ray distance closest to sphere center

            // Early exit if ray points away and it's outside the sphere
            if (rayDistance < 0.f && centerDistance.length() > m_radius)
                return false;

            float squaredDist = centerDistance.lengthSquared() - rayDistance * rayDistance;
            if (squaredDist > m_radius * m_radius)
                return false;

            float t_1 = rayDistance - std::sqrt(m_radius * m_radius - squaredDist);
            float t_2 = rayDistance + std::sqrt(m_radius * m_radius - squaredDist);

            if (t_1 >= ray.tMin && t_1 < ray.tMax) {
                tHit = t_1;
                return true;
            }

            if (t_2 >= ray.tMin && t_2 < ray.tMax) {
                tHit = t_2;
                return true;
            }

            return false;
        }

        Bounds3f getBounds() const override {
            Bounds3f objectBounds(
                Point3f(-m_radius),
                Point3f(m_radius)
            );

            return m_objectToWorld(objectBounds);
        }

        std::string toString() const override {
            return "Sphere[radius=" + std::to_string(m_radius) + "]";
        }

    private:
        float m_radius;
    };

    GND_REGISTER_CLASS(Sphere, "sphere");
}