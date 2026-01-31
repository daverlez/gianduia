#pragma once

#include <cmath>
#include <limits>

namespace gnd {

    constexpr float Infinity = std::numeric_limits<float>::infinity();
    constexpr float MachineEpsilon = std::numeric_limits<float>::epsilon() * 0.5f;

    constexpr float Epsilon = 0.0001f;

    constexpr float Pi = 3.14159265358979323846f;
    constexpr float InvPi = 0.31830988618379067154f;
    constexpr float Inv2Pi = 0.15915494309189533577f;
    constexpr float Inv4Pi = 0.07957747154594766788f;
    constexpr float PiOver2 = 1.57079632679489661923f;
    constexpr float PiOver4 = 0.78539816339744830961f;
    constexpr float Sqrt2 = 1.41421356237309504880f;


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

}