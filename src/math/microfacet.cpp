#include <gianduia/math/microfacet.h>

namespace gnd {

    inline float cos2Theta(const Vector3f& w) { return w.z() * w.z(); }
    inline float sin2Theta(const Vector3f& w) { return std::max(0.0f, 1.0f - cos2Theta(w)); }
    inline float tan2Theta(const Vector3f& w) { return sin2Theta(w) / cos2Theta(w); }

    // D(wh) = alpha^2 / (PI * ((alpha^2 - 1) * cos^2(theta_h) + 1)^2)
    float TrowbridgeReitzDistribution::D(const Vector3f& wh) const {
        float cos2 = cos2Theta(wh);
        if (cos2 <= 0.0f) return 0.0f;

        float alpha2 = m_alpha * m_alpha;
        float denom = (alpha2 - 1.0f) * cos2 + 1.0f;

        return alpha2 * InvPi / (denom * denom);
    }

    float TrowbridgeReitzDistribution::G1(const Vector3f& w) const {
        float cos2 = cos2Theta(w);
        if (cos2 <= 0.0f) return 0.0f;

        float tan2 = std::max(0.0f, 1.0f - cos2) / cos2;
        if (tan2 == 0.0f) return 1.0f;

        float alpha2 = m_alpha * m_alpha;
        return 2.0f / (1.0f + std::sqrt(1.0f + alpha2 * tan2));
    }

    float TrowbridgeReitzDistribution::G(const Vector3f& wo, const Vector3f& wi) const {
        return G1(wo) * G1(wi);
    }

    Vector3f TrowbridgeReitzDistribution::sample_wh(const Vector3f& wo, const Point2f& sample) const {
        float alpha2 = m_alpha * m_alpha;

        float cos2Theta = (1.0f - sample.y()) / (1.0f + (alpha2 - 1.0f) * sample.y());
        float cosTheta = std::sqrt(cos2Theta);
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cos2Theta));

        float phi = 2.0f * M_PI * sample.x();

        Vector3f wh(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);

        if (wh.z() * wo.z() < 0.0f) {
            wh = -wh;
        }

        return wh;
    }

    float TrowbridgeReitzDistribution::pdf(const Vector3f& wo, const Vector3f& wh) const {
        return D(wh) * std::abs(wh.z());
    }

}