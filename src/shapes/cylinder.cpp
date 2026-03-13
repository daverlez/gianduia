#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/warp.h>
#include <gianduia/math/constants.h>
#include <cmath>
#include <stdexcept>

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

            if (std::abs(isect.p.z() - m_zMax) < Epsilon) {
                isect.n = Normal3f(0.0f, 0.0f, 1.0f);
                float phi = std::atan2(isect.p.y(), isect.p.x());
                if (phi < 0.0f) phi += 2.0f * M_PI;
                float r = std::sqrt(isect.p.x() * isect.p.x() + isect.p.y() * isect.p.y());

                isect.uv = Point2f(phi * Inv2Pi, r / m_radius);
                isect.dpdu = Vector3f(-isect.p.y() * 2.0f * M_PI, isect.p.x() * 2.0f * M_PI, 0.0f);

            } else if (std::abs(isect.p.z() - m_zMin) < Epsilon) {
                isect.n = Normal3f(0.0f, 0.0f, -1.0f);
                float phi = std::atan2(isect.p.y(), isect.p.x());
                if (phi < 0.0f) phi += 2.0f * M_PI;
                float r = std::sqrt(isect.p.x() * isect.p.x() + isect.p.y() * isect.p.y());

                isect.uv = Point2f(phi * Inv2Pi, r / m_radius);
                isect.dpdu = Vector3f(-isect.p.y() * 2.0f * M_PI, isect.p.x() * 2.0f * M_PI, 0.0f);

            } else {
                isect.n = Normalize(Normal3f(isect.p.x(), isect.p.y(), 0.0f));
                float phi = std::atan2(isect.p.y(), isect.p.x());
                if (phi < 0.0f) phi += 2.0f * M_PI;
                isect.uv = Point2f(phi * Inv2Pi, (isect.p.z() - m_zMin) / (m_zMax - m_zMin));
                isect.dpdu = Vector3f(-isect.p.y() * 2.0f * M_PI, isect.p.x() * 2.0f * M_PI, 0.0f);
            }
        }

        Bounds3f getBounds() const override {
            return Bounds3f(
                Point3f(-m_radius, -m_radius, m_zMin),
                Point3f(m_radius, m_radius, m_zMax)
            );
        }

        float sampleSurface(const Point3f& ref, const Point2f &sample, SurfaceInteraction &info) const override {
            float areaSide = 2.0f * M_PI * m_radius * (m_zMax - m_zMin);
            float areaCap = M_PI * m_radius * m_radius;
            float areaTotal = areaSide + 2.0f * areaCap;

            float pdfSide = areaSide / areaTotal;
            float pdfCap = areaCap / areaTotal;

            Point2f s = sample;
            if (s.x() < pdfSide) {
                // Sample the side
                s.x() /= pdfSide;
                float phi = s.x() * 2.0f * M_PI;
                float z = m_zMin + s.y() * (m_zMax - m_zMin);
                info.p = Point3f(m_radius * std::cos(phi), m_radius * std::sin(phi), z);
                info.n = Normal3f(std::cos(phi), std::sin(phi), 0.0f);
                info.uv = Point2f(s.x(), s.y());
            } else if (s.x() < pdfSide + pdfCap) {
                // Sample top cap
                s.x() = (s.x() - pdfSide) / pdfCap;
                Point2f d = Warp::squareToUniformDisk(s);
                info.p = Point3f(d.x() * m_radius, d.y() * m_radius, m_zMax);
                info.n = Normal3f(0.0f, 0.0f, 1.0f);

                float phi = std::atan2(info.p.y(), info.p.x());
                if (phi < 0.0f) phi += 2.0f * M_PI;
                float r = std::sqrt(info.p.x() * info.p.x() + info.p.y() * info.p.y());
                info.uv = Point2f(phi * Inv2Pi, r / m_radius);

            } else {
                // Sample bottom cap
                s.x() = (s.x() - pdfSide - pdfCap) / pdfCap;
                Point2f d = Warp::squareToUniformDisk(s);
                info.p = Point3f(d.x() * m_radius, d.y() * m_radius, m_zMin);
                info.n = Normal3f(0.0f, 0.0f, -1.0f);

                float phi = std::atan2(info.p.y(), info.p.x());
                if (phi < 0.0f) phi += 2.0f * M_PI;
                float r = std::sqrt(info.p.x() * info.p.x() + info.p.y() * info.p.y());
                info.uv = Point2f(phi * Inv2Pi, r / m_radius);
            }
            return 1.0f / areaTotal;
        }

        float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const override {
            float areaTotal = 2.0f * M_PI * m_radius * (m_zMax - m_zMin) + 2.0f * M_PI * m_radius * m_radius;
            return 1.0f / areaTotal;
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