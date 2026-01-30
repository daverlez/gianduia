#pragma once

#include <glm/glm.hpp>
#include <cmath>

namespace gnd {

    struct Point3f;
    struct Normal3f;

    struct Vector3f {
        glm::vec3 v;

        Vector3f() : v(0.f) {}
        explicit Vector3f(float x) : v(x) {}
        Vector3f(float x, float y, float z) : v(x, y, z) {}
        explicit Vector3f(const glm::vec3& _v) : v(_v) {}

        float operator[](int i) const { return v[i]; }
        float& operator[](int i) { return v[i]; }

        Vector3f operator+(const Vector3f& other) const { return Vector3f(v + other.v); }
        Vector3f operator-(const Vector3f& other) const { return Vector3f(v - other.v); }
        Vector3f operator*(float s) const { return Vector3f(v * s); }
        Vector3f operator/(float s) const { return Vector3f(v / s); }

        Vector3f& operator+=(const Vector3f& other) { v += other.v; return *this; }
        Vector3f& operator-=(const Vector3f& other) { v -= other.v; return *this; }
        Vector3f& operator*=(float s) { v *= s; return *this;}
        Vector3f& operator/=(float s) { v /= s; return *this; }

        Vector3f operator-() const { return Vector3f(-v); }

        float lengthSquared() const {
            return glm::dot(v, v);
        }
        float length() const {
            return glm::length(v);
        }
        bool hasNaNs() const {
            return (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z));
        }
    };

    inline Vector3f operator*(float s, const Vector3f& v) { return v * s; }


    struct Point2f {
        glm::vec2 p;

        Point2f() : p(0.f) {}
        Point2f(float x, float y) : p(x, y) {}
        explicit Point2f(const glm::vec2& _p) : p(_p) {}

        float operator[](int i) const { return p[i]; }
        float& operator[](int i) { return p[i]; }

        Point2f operator*(float s) const { return Point2f(p * s); }
        Point2f& operator*=(float s) { p *= s; return *this; }
        Point2f operator/(float s) const { return Point2f(p / s); }
        Point2f& operator/=(float s) { p /= s; return *this; }
    };

    inline Point2f operator*(float s, const Point2f& p) { return p * s; }


    struct Point3f {
        glm::vec3 p;

        Point3f() : p(0.f) {}
        explicit Point3f(float x) : p(x) {}
        Point3f(float x, float y, float z) : p(x, y, z) {}
        explicit Point3f(const glm::vec3& _p) : p(_p) {}

        float operator[](int i) const { return p[i]; }
        float& operator[](int i) { return p[i]; }

        Point3f operator+(const Vector3f& v) const { return Point3f(p + v.v); }
        Point3f operator-(const Vector3f& v) const { return Point3f(p - v.v); }

        Point3f& operator+=(const Vector3f& v) { p += v.v; return *this; }
        Point3f& operator-=(const Vector3f& v) { p -= v.v; return *this; }

        Vector3f operator-(const Point3f& other) const { return Vector3f(p - other.p); }

        /* Utility operators for weighted average (e.g. interpolation) */

        Point3f operator+(const Point3f& other) const { return Point3f(p + other.p); }
        Point3f operator*(float s) const { return Point3f(p * s); }
        Point3f& operator*=(float s) { p *= s; return *this; }
        Point3f operator/(float s) const { return Point3f(p / s); }
        Point3f& operator/=(float s) { p /= s; return *this; }

        bool hasNaNs() const {
            return (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z));
        }
    };

    inline Point3f operator*(float s, const Point3f& p) { return p * s; }


    struct Normal3f {
        glm::vec3 n;

        Normal3f() : n(0.f) {}
        Normal3f(float x, float y, float z) : n(x, y, z) {}
        explicit Normal3f(const glm::vec3& _n) : n(_n) {}
        explicit Normal3f(const Vector3f& v) : n(v.v) {}

        float operator[](int i) const { return n[i]; }
        float& operator[](int i) { return n[i]; }

        Normal3f operator+(const Normal3f& other) const { return Normal3f(n + other.n); }
        Normal3f operator-(const Normal3f& other) const { return Normal3f(n - other.n); }
        Normal3f operator*(float s) const { return Normal3f(n * s); }
        Normal3f operator/(float s) const { return Normal3f(n / s); }

        Normal3f& operator+=(const Normal3f& other) { n += other.n; return *this; }
        Normal3f& operator-=(const Normal3f& other) { n -= other.n; return *this; }
        Normal3f& operator*=(float s) { n *= s; return *this; }
        Normal3f& operator/=(float s) { n /= s; return *this; }

        float lengthSquared() const { return glm::dot(n, n); }
        float length() const { return glm::length(n); }
    };


    /* Geometry utility functions */

    inline float Dot(const Vector3f& v1, const Vector3f& v2) { return glm::dot(v1.v, v2.v); }
    inline float Dot(const Normal3f& n1, const Vector3f& v2) { return glm::dot(n1.n, v2.v); }
    inline float Dot(const Vector3f& v1, const Normal3f& n2) { return glm::dot(v1.v, n2.n); }
    inline float Dot(const Normal3f& n1, const Normal3f& n2) { return glm::dot(n1.n, n2.n); }
    inline float AbsDot(const Normal3f& n, const Vector3f& v) { return std::abs(Dot(n, v)); }

    inline Vector3f Cross(const Vector3f& v1, const Vector3f& v2) { return Vector3f(glm::cross(v1.v, v2.v)); }
    inline Vector3f Cross(const Vector3f& v1, const Normal3f& n2) { return Vector3f(glm::cross(v1.v, n2.n)); }
    inline Vector3f Cross(const Normal3f& n1, const Vector3f& v2) { return Vector3f(glm::cross(n1.n, v2.v)); }

    inline Vector3f Normalize(const Vector3f& v) { return Vector3f(glm::normalize(v.v)); }
    inline Normal3f Normalize(const Normal3f& n) { return Normal3f(glm::normalize(n.n)); }

    inline float Distance(const Point3f& p1, const Point3f& p2) { return (p1 - p2).length(); }
    inline float DistanceSquared(const Point3f& p1, const Point3f& p2) { return (p1 - p2).lengthSquared(); }

    inline Point3f Lerp(float t, const Point3f& p1, const Point3f& p2) {
        return (1.f - t) * p1 + t * p2;
    }

}