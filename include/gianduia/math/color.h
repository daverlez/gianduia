#pragma once

#include <glm/glm.hpp>
#include <cmath>
#include <algorithm>
#include <iostream>
#include <format>

namespace gnd {

    struct Color3f {
        glm::vec3 v;

        Color3f() : v(0.f) {}
        Color3f(float val) : v(val) {}
        Color3f(float r, float g, float b) : v(r, g, b) {}
        explicit Color3f(const glm::vec3& _v) : v(_v) {}

        float r() const { return v.x; }
        float g() const { return v.y; }
        float b() const { return v.z; }
        
        float& r() { return v.x; }
        float& g() { return v.y; }
        float& b() { return v.z; }

        float operator[](int i) const { return v[i]; }
        float& operator[](int i) { return v[i]; }

        Color3f operator-() const { return Color3f(-v); }

        Color3f operator+(const Color3f& other) const { return Color3f(v + other.v); }
        Color3f& operator+=(const Color3f& other) { v += other.v; return *this; }

        Color3f operator-(const Color3f& other) const { return Color3f(v - other.v); }
        Color3f& operator-=(const Color3f& other) { v -= other.v; return *this; }

        Color3f operator*(float s) const { return Color3f(v * s); }
        Color3f& operator*=(float s) { v *= s; return *this; }

        Color3f operator/(float s) const { float inv = 1.0f / s; return Color3f(v * inv); }
        Color3f& operator/=(float s) { float inv = 1.0f / s; v *= inv; return *this; }

        Color3f operator*(const Color3f& other) const { return Color3f(v * other.v); }
        Color3f& operator*=(const Color3f& other) { v *= other.v; return *this; }


        float luminance() const {
            return v.x * 0.212671f + v.y * 0.715160f + v.z * 0.072169f;
        }

        bool isBlack() const {
            return v.x <= 0.0f && v.y <= 0.0f && v.z <= 0.0f;
        }

        bool hasNaNs() const {
            return std::isnan(v.x) || std::isnan(v.y) || std::isnan(v.z) ||
                   std::isinf(v.x) || std::isinf(v.y) || std::isinf(v.z);
        }

        /* Utilities for gamma correction and others */

        Color3f sqrt() const {
            return Color3f(std::sqrt(v.x), std::sqrt(v.y), std::sqrt(v.z));
        }

        Color3f exp() const {
            return Color3f(std::exp(v.x), std::exp(v.y), std::exp(v.z));
        }

        Color3f pow(float exp) const {
            return Color3f(std::pow(v.x, exp), std::pow(v.y, exp), std::pow(v.z, exp));
        }

        Color3f clamp(float minVal = 0.0f, float maxVal = 1.0f) const {
            return {
                std::clamp(v.x, minVal, maxVal),
                std::clamp(v.y, minVal, maxVal),
                std::clamp(v.z, minVal, maxVal)
            };
        }

        // Conversion from linear RGB to sRGB
        Color3f toSRGB() const {
            Color3f ret;
            for (int i = 0; i < 3; ++i) {
                float x = v[i];
                if (x <= 0.0031308f) {
                    ret[i] = 12.92f * x;
                } else {
                    ret[i] = 1.055f * std::pow(x, 1.0f / 2.4f) - 0.055f;
                }
            }
            return ret;
        }

        // Conversion from sRGB to linear RGB
        Color3f toLinear() const {
            Color3f ret;
            for (int i = 0; i < 3; ++i) {
                float x = v[i];

                if (x <= 0.04045f) {
                    ret[i] = x * (1.0f / 12.92f);
                } else {
                    ret[i] = std::pow((x + 0.055f) * (1.0f / 1.055f), 2.4f);
                }
            }
            return ret;
        }

        std::string toString() const {
            return std::format("[{:.2f}, {:.2f}, {:.2f}]", v.x, v.y, v.z);
        }
    };

    inline Color3f operator*(float s, const Color3f& c) {
        return c * s;
    }

    inline std::ostream& operator<<(std::ostream& os, const Color3f& c) {
        os << "[" << c.r() << ", " << c.g() << ", " << c.b() << "]";
        return os;
    }

}