#pragma once

#include <cmath>
#include <limits>
#include <sstream>

namespace gnd {

    constexpr float Infinity = std::numeric_limits<float>::infinity();
    constexpr float MachineEpsilon = std::numeric_limits<float>::epsilon() * 0.5f;

    constexpr float Epsilon = 0.0001f;

    constexpr float Pi = 3.14159265358979323846f;
    constexpr float TwoPi = 2.f * Pi;
    constexpr float InvPi = 0.31830988618379067154f;
    constexpr float Inv2Pi = 0.15915494309189533577f;
    constexpr float Inv4Pi = 0.07957747154594766788f;
    constexpr float PiOver2 = 1.57079632679489661923f;
    constexpr float PiOver4 = 0.78539816339744830961f;
    constexpr float Sqrt2 = 1.41421356237309504880f;
    constexpr float SqrtPiOver8 = 0.6266570686577501f;


    inline float Radians(float deg) {
        return (Pi / 180.f) * deg;
    }

    inline float Degrees(float rad) {
        return (180.f / Pi) * rad;
    }

    template <typename T>
    inline T Clamp(T val, T low, T high) {
        if (val < low) return low;
        if (val > high) return high;
        return val;
    }

    template <typename T>
    inline T Lerp(float t, T v1, T v2) {
        return (1.f - t) * v1 + t * v2;
    }

    inline bool SolveQuadratic(float a, float b, float c, float& r0, float& r1) {
        float discrim = b * b - 4.0f * a * c;
        if (discrim < 0) return false;

        float rootDiscrim = std::sqrt(discrim);
        r0 = (-b - rootDiscrim) / (2.0f * a);
        r1 = (-b + rootDiscrim) / (2.0f * a);
        return true;
    };

    /// Indents every line in a string.
    /// @param amount Numer of indentation levels (one level == two spaces)
    inline std::string indent(const std::string& str, int amount = 1) {
        if (str.empty()) return "";

        std::string result;
        std::string spaces(amount * 2, ' ');
        std::stringstream ss(str);
        std::string line;
        bool first = true;

        while (std::getline(ss, line)) {
            if (!first) result += "\n";
            result += (line.empty() ? "" : spaces) + line;
            first = false;
        }

        if (!str.empty() && str.back() == '\n') result += "\n";

        return result;
    }

    /// Yields the spherical coordinates of the given vector.
    /// Theta is the first coordinate, Phi is the second.
    inline Point2f SphericalCoordinates(const Vector3f& w) {
        Point2f result(std::acos(std::clamp(w.z(), -1.0f, 1.0f)), std::atan2(w.y(), w.x()));
        if (result.y() < 0.0f) result.y() += 2.0f * Pi;

        return result;
    }

    inline Vector3f SphericalDirection(const Point2f& p) {
        float cosTheta = std::cos(p.x());
        float sinTheta = std::sin(p.x());
        float cosPhi = std::cos(p.y());
        float sinPhi = std::sin(p.y());

        Vector3f result(
            sinTheta * cosPhi,
            sinTheta * sinPhi,
            cosTheta
        );

        return result;
    }

}