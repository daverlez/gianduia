#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <format>

namespace gnd {

    struct Point3f;
    struct Normal3f;

    struct Vector2f {
        glm::vec2 v;

        Vector2f() : v(0.f) {}
        explicit Vector2f(float x) : v(x) {}
        Vector2f(float x, float y) : v(x, y) {}
        explicit Vector2f(const glm::vec2& _v) : v(_v) {}

        float operator[](int i) const { return v[i]; }
        float& operator[](int i) { return v[i]; }

        Vector2f operator+(const Vector2f& other) const { return Vector2f(v + other.v); }
        Vector2f operator-(const Vector2f& other) const { return Vector2f(v - other.v); }
        Vector2f operator*(float s) const { return Vector2f(v * s); }
        Vector2f operator/(float s) const { return Vector2f(v / s); }

        Vector2f& operator+=(const Vector2f& other) { v += other.v; return *this; }
        Vector2f& operator-=(const Vector2f& other) { v -= other.v; return *this; }
        Vector2f& operator*=(float s) { v *= s; return *this;}
        Vector2f& operator/=(float s) { v /= s; return *this; }

        Vector2f operator-() const { return Vector2f(-v); }

        const float& x() const  { return v[0]; }
        float& x()              { return v[0]; }
        const float& y() const  { return v[1]; }
        float& y()              { return v[1]; }

        float lengthSquared() const {
            return glm::dot(v, v);
        }
        float length() const {
            return glm::length(v);
        }
        bool hasNaNs() const {
            return (std::isnan(v.x) || std::isnan(v.y));
        }

        std::string toString() const {
            return std::format("({}, {})", v[0], v[1]);
        }
    };

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

        const float& x() const  { return v[0]; }
        float& x()              { return v[0]; }
        const float& y() const  { return v[1]; }
        float& y()              { return v[1]; }
        const float& z() const  { return v[2]; }
        float& z()              { return v[2]; }

        float lengthSquared() const {
            return glm::dot(v, v);
        }
        float length() const {
            return glm::length(v);
        }
        bool hasNaNs() const {
            return (std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z));
        }

        std::string toString() const {
            return std::format("({}, {}, {})", v[0], v[1], v[2]);
        }
    };

    inline Vector3f operator*(float s, const Vector3f& v) { return v * s; }


    struct Point2f {
        glm::vec2 p;

        Point2f() : p(0.f) {}
        explicit Point2f(float x) : p(x) {}
        Point2f(float x, float y) : p(x, y) {}
        explicit Point2f(const glm::vec2& _p) : p(_p) {}

        float operator[](int i) const { return p[i]; }
        float& operator[](int i) { return p[i]; }

        Point2f operator+(const Vector2f& v) const { return Point2f(p + v.v); }
        Point2f operator-(const Vector2f& v) const { return Point2f(p - v.v); }

        Point2f& operator+=(const Vector2f& v) { p += v.v; return *this; }
        Point2f& operator-=(const Vector2f& v) { p -= v.v; return *this; }

        Point2f operator*(float s) const { return Point2f(p * s); }
        Point2f& operator*=(float s) { p *= s; return *this; }
        Point2f operator/(float s) const { return Point2f(p / s); }
        Point2f& operator/=(float s) { p /= s; return *this; }

        Point2f operator+(Point2f other) const { return Point2f(p + other.p); }
        Vector2f operator-(Point2f other) const { return Vector2f(p - other.p); }

        const float& x() const  { return p[0]; }
        float& x()              { return p[0]; }
        const float& y() const  { return p[1]; }
        float& y()              { return p[1]; }

        float lengthSquared() const {
            return glm::dot(p, p);
        }
        float length() const {
            return glm::length(p);
        }

        std::string toString() const {
            return std::format("({}, {})", p[0], p[1]);
        }
    };

    inline Point2f operator*(float s, const Point2f& p) { return p * s; }


    struct Point3f {
        glm::vec3 p;

        Point3f() : p(0.f) {}
        explicit Point3f(float x) : p(x) {}
        Point3f(float x, float y, float z) : p(x, y, z) {}
        explicit Point3f(const glm::vec3& _p) : p(_p) {}

        explicit operator Vector3f() const { return Vector3f(p); }

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

        const float& x() const  { return p[0]; }
        float& x()              { return p[0]; }
        const float& y() const  { return p[1]; }
        float& y()              { return p[1]; }
        const float& z() const  { return p[2]; }
        float& z()              { return p[2]; }

        bool hasNaNs() const {
            return (std::isnan(p.x) || std::isnan(p.y) || std::isnan(p.z));
        }

        std::string toString() const {
            return std::format("({}, {}, {})", p[0], p[1], p[2]);
        }
    };

    inline Point3f operator*(float s, const Point3f& p) { return p * s; }

    struct Normal3f {
        glm::vec3 n;

        Normal3f() : n(0.f) {}
        explicit Normal3f(float x) : n(x) {}
        Normal3f(float x, float y, float z) : n(x, y, z) {}
        explicit Normal3f(const glm::vec3& _n) : n(_n) {}
        explicit Normal3f(const Vector3f& v) : n(v.v) {}

        explicit operator Vector3f() const { return Vector3f(n); }

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

        Normal3f operator-() const { return Normal3f(-n); }

        const float& x() const  { return n[0]; }
        float& x()              { return n[0]; }
        const float& y() const  { return n[1]; }
        float& y()              { return n[1]; }
        const float& z() const  { return n[2]; }
        float& z()              { return n[2]; }

        float lengthSquared() const { return glm::dot(n, n); }
        float length() const { return glm::length(n); }

        std::string toString() const {
            return std::format("({}, {}, {})", n[0], n[1], n[2]);
        }
    };

    inline Vector3f operator+(const Vector3f& v, const Normal3f& n) { return Vector3f(v.v + n.n); }
    inline Vector3f operator+(const Normal3f& n, const Vector3f& v) { return Vector3f(v.v + n.n); }
    inline Vector3f operator-(const Vector3f& v, const Normal3f& n) { return Vector3f(v.v - n.n); }
    inline Vector3f operator-(const Normal3f& n, const Vector3f& v) { return Vector3f(n.n - v.v); }

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
