#pragma once

#include <gianduia/math/geometry.h>
#include <gianduia/math/color.h>
#include <cmath>
#include <algorithm>

#include "constants.h"

namespace gnd {

struct Photon {
    Point3f p;           // 12 byte: position
    uint8_t rgbe[4];     // 4 byte: Power in RGBE format
    uint8_t theta;       // 1 byte: Theta direction in [0, 255]
    uint8_t phi;         // 1 byte: Phi direction [0, 255]
    // 2 byte of automatic padding for 4 byte alignment

    Photon() = default;

    Photon(const Point3f& pos, const Color3f& power, const Vector3f& dir) : p(pos) {
        float v = std::max({power.r(), power.g(), power.b()});
        if (v < 1e-32f) {
            rgbe[0] = rgbe[1] = rgbe[2] = rgbe[3] = 0;
        } else {
            int e;
            float f = std::frexp(v, &e);
            v = f * 256.0f / v;
            rgbe[0] = static_cast<uint8_t>(power.r() * v);
            rgbe[1] = static_cast<uint8_t>(power.g() * v);
            rgbe[2] = static_cast<uint8_t>(power.b() * v);
            rgbe[3] = static_cast<uint8_t>(e + 128);
        }

        float acosTheta = std::clamp(dir.z(), -1.0f, 1.0f);
        float th = std::acos(acosTheta);
        float ph = std::atan2(dir.y(), dir.x());
        if (ph < 0.0f) ph += 2.0f * Pi;

        theta = static_cast<uint8_t>(std::clamp(th * (256.0f / Pi), 0.0f, 255.0f));
        phi   = static_cast<uint8_t>(std::clamp(ph * (256.0f / (2.0f * Pi)), 0.0f, 255.0f));
    }

    Color3f getPower() const {
        if (rgbe[3] == 0) return Color3f(0.0f);
        float f = m_expTable[rgbe[3]];
        return Color3f(rgbe[0] * f, rgbe[1] * f, rgbe[2] * f);
    }

    Vector3f getDirection() const {
        return Vector3f(
            m_sinTheta[theta] * m_cosPhi[phi],
            m_sinTheta[theta] * m_sinPhi[phi],
            m_cosTheta[theta]
        );
    }

    static void initTables();

private:
    static float m_cosTheta[256];
    static float m_sinTheta[256];
    static float m_cosPhi[256];
    static float m_sinPhi[256];
    static float m_expTable[256];
};

}