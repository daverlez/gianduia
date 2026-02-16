#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <cmath>

namespace gnd {

    class Cylinder : public Shape {
    public:
        Cylinder(const PropertyList& props) : Shape(props) {
            m_radius = props.getFloat("radius", 1.0f);
            m_zMin = props.getFloat("zMin", -1.0f);
            m_zMax = props.getFloat("zMax", 1.0f);
        }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const override {
            float A = ray.d.x() * ray.d.x() + ray.d.y() * ray.d.y();
            float B = 2.0f * (ray.d.x() * ray.o.x() + ray.d.y() * ray.o.y());
            float C = ray.o.x() * ray.o.x() + ray.o.y() * ray.o.y() - m_radius * m_radius;

            float t0, t1;
            bool hitSide = false;
            float tSide = std::numeric_limits<float>::infinity();

            if (SolveQuadratic(A, B, C, t0, t1)) {
                if (t0 > ray.tMin && t0 < ray.tMax) {
                    float z = ray.o.z() + t0 * ray.d.z();
                    if (z >= m_zMin && z <= m_zMax) {
                        tSide = t0;
                        hitSide = true;
                        if (predicate) return true;
                    }
                }
                if (!hitSide && t1 > ray.tMin && t1 < ray.tMax) {
                    float z = ray.o.z() + t1 * ray.d.z();
                    if (z >= m_zMin && z <= m_zMax) {
                        tSide = t1;
                        hitSide = true;
                        if (predicate) return true;
                    }
                }
            }

            // Caps intersection
            bool hitCap = false;
            float tCap = std::numeric_limits<float>::infinity();
            Normal3f capNormal(0.0f);

            auto intersectCap = [&](float zLimit, const Normal3f& n) {
                if (std::abs(ray.d.z()) < 1e-6) return;
                float t = (zLimit - ray.o.z()) / ray.d.z();
                if (t > ray.tMin && t < ray.tMax) {
                    float x = ray.o.x() + t * ray.d.x();
                    float y = ray.o.y() + t * ray.d.y();
                    if (x * x + y * y <= m_radius * m_radius) {
                        if (t < tCap) {
                            tCap = t;
                            capNormal = n;
                            hitCap = true;
                        }
                    }
                }
            };

            intersectCap(m_zMin, Normal3f(0, 0, -1));
            intersectCap(m_zMax, Normal3f(0, 0, 1));
            if (hitCap && predicate) return true;

            if (!hitSide && !hitCap) return false;

            if (hitSide && tSide < tCap)
                isect.t = tSide;
            else
                isect.t = tCap;

            return true;
        }

        void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override {
            isect.p = ray.o + ray.d * isect.t;

            if (std::abs(isect.p.z() - m_zMax) < Epsilon)
                isect.n = Normal3f(0.0f, 0.0f, 1.0f);
            else if (std::abs(isect.p.z() - m_zMin) < Epsilon)
                isect.n = Normal3f(0.0f, 0.0f, -1.0f);
            else
                isect.n = Normalize(Normal3f(isect.p.x(), isect.p.y(), 0.0f));
        }

        Bounds3f getBounds() const override {
            return Bounds3f(
                Point3f(-m_radius, -m_radius, m_zMin),
                Point3f(m_radius, m_radius, m_zMax)
            );
        }

        std::string toString() const override {
            return std::format(
                "Cylinder[\n"
                "  radius = {:.2f},\n"
                "  zMin = {:.2f},\n"
                "  zMax = {:.2f},\n"
                "]",
                m_radius, m_zMin, m_zMax
            );
        }

    private:
        float m_radius, m_zMin, m_zMax;
    };

    GND_REGISTER_CLASS(Cylinder, "cylinder");
}