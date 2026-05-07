#include <gianduia/math/photon.h>
#include <cmath>

namespace gnd {

    float Photon::m_cosTheta[256];
    float Photon::m_sinTheta[256];
    float Photon::m_cosPhi[256];
    float Photon::m_sinPhi[256];
    float Photon::m_expTable[256];

    void Photon::initTables() {
        static bool initialized = false;
        if (initialized) return;

        for (int i = 0; i < 256; i++) {
            float angleTheta = (i + 0.5f) * (Pi / 256.0f);
            m_cosTheta[i] = std::cos(angleTheta);
            m_sinTheta[i] = std::sin(angleTheta);

            float anglePhi = (i + 0.5f) * (2.0f * Pi / 256.0f);
            m_cosPhi[i] = std::cos(anglePhi);
            m_sinPhi[i] = std::sin(anglePhi);

            m_expTable[i] = std::ldexp(1.0f, i - 136);
        }

        m_expTable[0] = 0.0f;

        initialized = true;
    }

}