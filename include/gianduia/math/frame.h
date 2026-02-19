#pragma once
#include <gianduia/math/geometry.h>
#include <cmath>
#include <algorithm>

namespace gnd {

    class Frame {
    public:
        Vector3f x, y, z; // z := normal

        Frame() : x(1, 0, 0), y(0, 1, 0), z(0, 0, 1) {}

        Frame(const Vector3f& x, const Vector3f& y, const Vector3f& z)
            : x(x), y(y), z(z) {}

        explicit Frame(const Vector3f& n) {
            z = n; // Assuming n is normalized
            float sign = std::copysign(1.0f, z.z());
            const float a = -1.0f / (sign + z.z());
            const float b = z.x() * z.y() * a;

            x = Vector3f(1.0f + sign * z.x() * z.x() * a, sign * b, -sign * z.x());
            y = Vector3f(b, sign + z.y() * z.y() * a, -z.y());
        }

        Vector3f toLocal(const Vector3f& v) const {
            return Vector3f(Dot(v, x), Dot(v, y), Dot(v, z));
        }

        Vector3f toWorld(const Vector3f& v) const {
            return x * v.x() + y * v.y() + z * v.z();
        }

        // Local trigonometric functions

        static float cosTheta(const Vector3f& w) { return w.z(); }
        static float cos2Theta(const Vector3f& w) { return w.z() * w.z(); }
        static float absCosTheta(const Vector3f& w) { return std::abs(w.z()); }

        static float sin2Theta(const Vector3f& w) {
            return std::max(0.0f, 1.0f - cos2Theta(w));
        }

        static float sinTheta(const Vector3f& w) {
            return std::sqrt(sin2Theta(w));
        }

        static float tanTheta(const Vector3f& w) {
            float sin2 = sin2Theta(w);
            if (sin2 <= 0.0f) return 0.0f;
            return std::sqrt(sin2) / cosTheta(w);
        }

        static float cosPhi(const Vector3f& w) {
            float sinT = sinTheta(w);
            return (sinT == 0.0f) ? 1.0f : std::clamp(w.x() / sinT, -1.0f, 1.0f);
        }

        static float sinPhi(const Vector3f& w) {
            float sinT = sinTheta(w);
            return (sinT == 0.0f) ? 0.0f : std::clamp(w.y() / sinT, -1.0f, 1.0f);
        }

        static bool sameHemisphere(const Vector3f& w, const Vector3f& wp) {
            return w.z() * wp.z() > 0.0f;
        }
    };

}
