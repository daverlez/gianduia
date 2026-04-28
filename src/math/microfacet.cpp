#include <gianduia/math/microfacet.h>

namespace gnd {

    // Trowbridge-Reitz (GTR2)

    float TrowbridgeReitzDistribution::D(const Vector3f& wh) const {
        if (wh.z() <= 0.0f) return 0.0f;

        float d = (wh.x() * wh.x()) / (m_alphaX * m_alphaX) +
                  (wh.y() * wh.y()) / (m_alphaY * m_alphaY) +
                  (wh.z() * wh.z());

        return 1.0f / (Pi * m_alphaX * m_alphaY * d * d);
    }

    float TrowbridgeReitzDistribution::G1(const Vector3f& w) const {
        if (w.z() <= 0.0f) return 0.0f;

        float a2tan2 = (w.x() * w.x() * m_alphaX * m_alphaX + w.y() * w.y() * m_alphaY * m_alphaY) / (w.z() * w.z());
        float lambda = 0.5f * (-1.0f + std::sqrt(1.0f + a2tan2));

        return 1.0f / (1.0f + lambda);
    }

    float TrowbridgeReitzDistribution::G(const Vector3f& wo, const Vector3f& wi) const {
        return G1(wo) * G1(wi);
    }

    Vector3f TrowbridgeReitzDistribution::sample_wh(const Vector3f& wo, const Point2f& sample) const {
        float phi = 2.0f * Pi * sample.x();

        Vector3f st(m_alphaX * std::cos(phi), m_alphaY * std::sin(phi), 0.0f);
        float amp = st.lengthSquared();

        float cos2Theta = (1.0f - sample.y()) / (1.0f + sample.y() * (amp - 1.0f));
        float cosTheta = std::sqrt(std::max(0.0f, cos2Theta));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cos2Theta));

        float normalizedAmp = std::sqrt(amp);
        if (normalizedAmp > 0.0f) {
            st.x() /= normalizedAmp;
            st.y() /= normalizedAmp;
        } else {
            st.x() = 1.0f;
            st.y() = 0.0f;
        }

        Vector3f wh(sinTheta * st.x(), sinTheta * st.y(), cosTheta);
        if (wh.z() * wo.z() < 0.0f) wh = -wh;
        return wh;
    }

    float TrowbridgeReitzDistribution::pdf(const Vector3f& wo, const Vector3f& wh) const {
        return D(wh) * std::abs(wh.z());
    }


    // Berry (GTR1)

    float GTR1Distribution::D(const Vector3f& wh) const {
        if (wh.z() <= 0.0f) return 0.0f;

        float alpha2 = m_alpha * m_alpha;
        float cos2 = wh.z() * wh.z();

        float num = alpha2 - 1.0f;
        float den = Pi * std::log(alpha2) * (1.0f + (alpha2 - 1.0f) * cos2);

        return num / den;
    }

    float GTR1Distribution::G1(const Vector3f& w) const {
        if (w.z() <= 0.0f) return 0.0f;

        // Burley (2012): roughness = 0.25f -> alpha = 0.25^2 = 0.0625f
        float alpha2 = 0.0625f;
        float cos2 = w.z() * w.z();
        float tan2 = std::max(0.0f, 1.0f - cos2) / cos2;

        float lambda = 0.5f * (-1.0f + std::sqrt(1.0f + alpha2 * tan2));
        return 1.0f / (1.0f + lambda);
    }

    float GTR1Distribution::G(const Vector3f& wo, const Vector3f& wi) const {
        return G1(wo) * G1(wi);
    }

    Vector3f GTR1Distribution::sample_wh(const Vector3f& wo, const Point2f& sample) const {
        float alpha2 = m_alpha * m_alpha;

        float cos2Theta = (1.0f - std::pow(alpha2, 1.0f - sample.y())) / (1.0f - alpha2);
        float cosTheta = std::sqrt(std::max(0.0f, cos2Theta));
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - cos2Theta));

        float phi = 2.0f * Pi * sample.x();

        Vector3f wh(sinTheta * std::cos(phi), sinTheta * std::sin(phi), cosTheta);
        if (wh.z() * wo.z() < 0.0f) wh = -wh;
        return wh;
    }

    float GTR1Distribution::pdf(const Vector3f& wo, const Vector3f& wh) const {
        return D(wh) * std::abs(wh.z());
    }

}