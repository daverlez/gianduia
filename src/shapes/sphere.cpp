#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/warp.h>
#include <gianduia/math/constants.h>
#include <cmath>

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

            if (t0 > ray.tMin && t0 < ray.tMax) {
                tHit = t0;
            } else if (t1 > ray.tMin && t1 < ray.tMax) {
                tHit = t1;
            } else {
                return false;
            }

            if (predicate) return true;

            isect.t = tHit;
            return true;
        }

        void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override {
            isect.p = ray.o + ray.d * isect.t;
            isect.n = Normal3f(Normalize(isect.p - Point3f(0.0f)));

            float phi = std::atan2(isect.n.x(), isect.n.z());
            if (phi < 0.0f)
                phi += 2.0f * M_PI;
            float u = phi * Inv2Pi;

            float theta = std::acos(std::clamp(isect.n.y(), -1.0f, 1.0f));
            float v = theta * InvPi;

            isect.uv = Point2f(u, v);
            isect.dpdu = Vector3f(isect.p.z() * 2.0f * M_PI, 0.0f, -isect.p.x() * 2.0f * M_PI);
        }

        Bounds3f getBounds() const override {
            return Bounds3f(
                Point3f(-m_radius),
                Point3f(m_radius)
            );
        }

        float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& info) const override {
            Vector3f res = Warp::squareToUniformSphere(sample);
            info.p = m_radius * Point3f(res.x(), res.y(), res.z());
            info.n = Normal3f(Normalize(info.p - Point3f(0.0f)));

            float phi = std::atan2(info.n.x(), info.n.z());
            if (phi < 0.0f)
                phi += 2.0f * M_PI;
            float u = phi * Inv2Pi;

            float theta = std::acos(std::clamp(info.n.y(), -1.0f, 1.0f));
            float v = theta * InvPi;

            info.uv = Point2f(u, v);
            info.dpdu = Vector3f(info.p.z() * 2.0f * M_PI, 0.0f, -info.p.x() * 2.0f * M_PI);

            return Inv4Pi / (m_radius * m_radius);
        }

        float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const override {
            return Inv4Pi / (m_radius * m_radius);
        }

        std::string toString() const override {
            return std::format("Sphere[radius={:.2f}]", m_radius);
        }

    private:
        float m_radius;
    };

    GND_REGISTER_CLASS(Sphere, "sphere");
}