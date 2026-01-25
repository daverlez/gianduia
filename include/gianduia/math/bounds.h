#pragma once

#include <gianduia/math/geometry.h>
#include <gianduia/math/ray.h>

#include <limits>
#include <algorithm>

namespace gnd {

    class Bounds3f {
    public:
        Point3f pMin, pMax;

        Bounds3f() {
            float minNum = std::numeric_limits<float>::lowest();
            float maxNum = std::numeric_limits<float>::max();
            pMin = Point3f(maxNum, maxNum, maxNum);
            pMax = Point3f(minNum, minNum, minNum);
        }

        explicit Bounds3f(const Point3f& p) : pMin(p), pMax(p) {}

        Bounds3f(const Point3f& p1, const Point3f& p2) {
            pMin = Point3f(std::min(p1.p.x, p2.p.x), std::min(p1.p.y, p2.p.y), std::min(p1.p.z, p2.p.z));
            pMax = Point3f(std::max(p1.p.x, p2.p.x), std::max(p1.p.y, p2.p.y), std::max(p1.p.z, p2.p.z));
        }

        /* Corners access operators */

        const Point3f& operator[](int i) const {
            return (i == 0) ? pMin : pMax;
        }
        Point3f& operator[](int i) {
            return (i == 0) ? pMin : pMax;
        }

        Point3f corner(int corner) const {
            return Point3f((*this)[(corner & 1)].p.x,
                           (*this)[(corner & 2) ? 1 : 0].p.y,
                           (*this)[(corner & 4) ? 1 : 0].p.z);
        }

        /* Geometry operations */

        Vector3f diagonal() const { return pMax - pMin; }

        float surfaceArea() const {
            Vector3f d = this->diagonal();
            return 2.f * (d.v.x * d.v.y + d.v.x * d.v.z + d.v.y * d.v.z);
        }

        float volume() const {
            Vector3f d = this->diagonal();
            return d.v.x * d.v.y * d.v.z;
        }

        /// Identifies the index of the longest axis.
        /// @return X for 0; Y for 1; Z for 2
        int maximumExtent() const {
            Vector3f d = this->diagonal();
            if (d.v.x > d.v.y && d.v.x > d.v.z)
                return 0;
            if (d.v.y > d.v.z)
                return 1;
            return 2;
        }

        /// Checks for intersection with the box (slab method).
        /// @param ray The ray in world space.
        /// @param hitt0  If hit, stores the entry distance. Can be nullptr.
        /// @param hitt1  If hit, stores the exit distance. Can be nullptr.
        /// @return True if the ray intersects, False otherwise.
        inline bool rayIntersect(const Ray& ray, float* hitt0 = nullptr, float* hitt1 = nullptr) const;
    };


    /* Global functions to handle bounds */

    inline Bounds3f Union(const Bounds3f& b1, const Bounds3f& b2) {
        Bounds3f ret;
        ret.pMin = Point3f(std::min(b1.pMin.p.x, b2.pMin.p.x),
                           std::min(b1.pMin.p.y, b2.pMin.p.y),
                           std::min(b1.pMin.p.z, b2.pMin.p.z));
        ret.pMax = Point3f(std::max(b1.pMax.p.x, b2.pMax.p.x),
                           std::max(b1.pMax.p.y, b2.pMax.p.y),
                           std::max(b1.pMax.p.z, b2.pMax.p.z));
        return ret;
    }

    inline Bounds3f Union(const Bounds3f& b, const Point3f& p) {
        Bounds3f ret;
        ret.pMin = Point3f(std::min(b.pMin.p.x, p.p.x),
                           std::min(b.pMin.p.y, p.p.y),
                           std::min(b.pMin.p.z, p.p.z));
        ret.pMax = Point3f(std::max(b.pMax.p.x, p.p.x),
                           std::max(b.pMax.p.y, p.p.y),
                           std::max(b.pMax.p.z, p.p.z));
        return ret;
    }

    inline bool Bounds3f::rayIntersect(const Ray& ray, float* hitt0, float* hitt1) const {
        float t0 = 0.f; 
        float t1 = ray.tMax;

        for (int i = 0; i < 3; ++i) {
            float invRayDir = 1.f / ray.d.v[i];
            float tNear = (pMin.p[i] - ray.o.p[i]) * invRayDir;
            float tFar  = (pMax.p[i] - ray.o.p[i]) * invRayDir;

            if (tNear > tFar) std::swap(tNear, tFar);

            t0 = tNear > t0 ? tNear : t0;
            t1 = tFar  < t1 ? tFar  : t1;

            if (t0 > t1) return false;
        }
        
        if (hitt0) *hitt0 = t0;
        if (hitt1) *hitt1 = t1;
        return true;
    }

}