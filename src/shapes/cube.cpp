#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <cmath>
#include <algorithm>

namespace gnd {

    class Cube : public Shape {
    public:
        explicit Cube(const PropertyList& props) : Shape(props) {
            m_extents = props.getVector3("extents", Vector3f(1.0f));
            m_min = Point3f(-m_extents.x(), -m_extents.y(), -m_extents.z());
            m_max = Point3f(m_extents.x(), m_extents.y(), m_extents.z());
        }

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const override {
            float t0 = ray.tMin;
            float t1 = ray.tMax;

            for (int i = 0; i < 3; ++i) {
                float invRayDir = 1.0f / ray.d[i];
                float tNear = (m_min[i] - ray.o[i]) * invRayDir;
                float tFar  = (m_max[i] - ray.o[i]) * invRayDir;

                if (tNear > tFar) std::swap(tNear, tFar);

                t0 = tNear > t0 ? tNear : t0;
                t1 = tFar  < t1 ? tFar  : t1;

                if (t0 > t1) return false;
            }

            float tHit = t0;

            if (tHit < ray.tMin) {
                tHit = t1;

                if (tHit < ray.tMin)
                    return false;
            }

            if (predicate) return true;

            isect.t = tHit;
            return true;
        }

        void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override {
            isect.p = ray.o + ray.d * isect.t;

            Vector3f pDistMin = isect.p - m_min;
            Vector3f pDistMax = m_max - isect.p;

            float minDist = std::numeric_limits<float>::max();
            int minAxis = 0;
            float sign = 1.0f;

            for (int i = 0; i < 3; ++i) {
                if (pDistMin[i] < minDist) { minDist = pDistMin[i]; minAxis = i; sign = -1.0f; }
                if (pDistMax[i] < minDist) { minDist = pDistMax[i]; minAxis = i; sign = 1.0f; }
            }

            isect.n = Normal3f(0.0f);
            isect.n[minAxis] = sign;

            if (minAxis == 0) {
                isect.uv = Point2f((isect.p.z() - m_min.z()) / (2.0f * m_extents.z()),
                                   (isect.p.y() - m_min.y()) / (2.0f * m_extents.y()));
                isect.dpdu = Vector3f(0.0f, 0.0f, 2.0f * m_extents.z() * sign);
            }
            else if (minAxis == 1) {
                isect.uv = Point2f((isect.p.x() - m_min.x()) / (2.0f * m_extents.x()),
                                   (isect.p.z() - m_min.z()) / (2.0f * m_extents.z()));
                isect.dpdu = Vector3f(2.0f * m_extents.x() * sign, 0.0f, 0.0f);
            }
            else {
                isect.uv = Point2f((isect.p.x() - m_min.x()) / (2.0f * m_extents.x()),
                                   (isect.p.y() - m_min.y()) / (2.0f * m_extents.y()));
                isect.dpdu = Vector3f(2.0f * m_extents.x() * sign, 0.0f, 0.0f);
            }
        }

        Bounds3f getBounds() const override {
            return Bounds3f(m_min, m_max);
        }

        float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& info) const override {
            float areaX = 4.0f * m_extents.y() * m_extents.z();
            float areaY = 4.0f * m_extents.x() * m_extents.z();
            float areaZ = 4.0f * m_extents.x() * m_extents.y();

            float totalArea = 2.0f * (areaX + areaY + areaZ);

            float faceProb[6] = {
                areaX / totalArea, areaX / totalArea,
                areaY / totalArea, areaY / totalArea,
                areaZ / totalArea, areaZ / totalArea
            };

            Point2f u = sample;
            int face = 0;
            float cdf = 0.0f;

            for (int i = 0; i < 6; ++i) {
                cdf += faceProb[i];
                if (u.x() < cdf || i == 5) {
                    face = i;
                    float prevCdf = cdf - faceProb[i];
                    u.x() = (u.x() - prevCdf) / faceProb[i];
                    break;
                }
            }

            float sign = (face % 2 == 0) ? 1.0f : -1.0f;

            if (face == 0 || face == 1) {
                float y = m_min.y() + u.x() * (2.0f * m_extents.y());
                float z = m_min.z() + u.y() * (2.0f * m_extents.z());
                info.p = Point3f(m_extents.x() * sign, y, z);
                info.n = Normal3f(sign, 0.0f, 0.0f);

                info.uv = Point2f((info.p.z() - m_min.z()) / (2.0f * m_extents.z()),
                                  (info.p.y() - m_min.y()) / (2.0f * m_extents.y()));
                info.dpdu = Vector3f(0.0f, 0.0f, 2.0f * m_extents.z() * sign);

            }
            else if (face == 2 || face == 3) {
                float x = m_min.x() + u.x() * (2.0f * m_extents.x());
                float z = m_min.z() + u.y() * (2.0f * m_extents.z());
                info.p = Point3f(x, m_extents.y() * sign, z);
                info.n = Normal3f(0.0f, sign, 0.0f);

                info.uv = Point2f((info.p.x() - m_min.x()) / (2.0f * m_extents.x()),
                                  (info.p.z() - m_min.z()) / (2.0f * m_extents.z()));
                info.dpdu = Vector3f(2.0f * m_extents.x() * sign, 0.0f, 0.0f);

            }
            else {
                float x = m_min.x() + u.x() * (2.0f * m_extents.x());
                float y = m_min.y() + u.y() * (2.0f * m_extents.y());
                info.p = Point3f(x, y, m_extents.z() * sign);
                info.n = Normal3f(0.0f, 0.0f, sign);

                info.uv = Point2f((info.p.x() - m_min.x()) / (2.0f * m_extents.x()),
                                  (info.p.y() - m_min.y()) / (2.0f * m_extents.y()));
                info.dpdu = Vector3f(2.0f * m_extents.x() * sign, 0.0f, 0.0f);
            }

            return 1.0f / totalArea;
        }

        float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const override {
            Vector3f d = m_max - m_min;
            float area = 2.0f * (d.x() * d.y() + d.x() * d.z() + d.y() * d.z());
            return 1.0f / area;
        }

        std::string toString() const override {
            return std::format("Cube[extents={}]", m_extents.toString());
        }

    private:
        Vector3f m_extents;
        Point3f m_min;
        Point3f m_max;
    };

    GND_REGISTER_CLASS(Cube, "cube");
}