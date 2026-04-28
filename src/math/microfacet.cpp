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
        // Heitz (2018): Sampling the GGX distribution of visible normals

        bool flip = wo.z() < 0.0f;
        Vector3f wo_h = flip ? -wo : wo;

        Vector3f Vh = Normalize(Vector3f(m_alphaX * wo_h.x(), m_alphaY * wo_h.y(), wo_h.z()));

        float lensq = Vh.x() * Vh.x() + Vh.y() * Vh.y();
        Vector3f T1 = lensq > 0.0f ? Vector3f(-Vh.y(), Vh.x(), 0.0f) / std::sqrt(lensq) : Vector3f(1.0f, 0.0f, 0.0f);
        Vector3f T2 = Cross(Vh, T1);

        float r = std::sqrt(sample.x());
        float phi = 2.0f * Pi * sample.y();
        float t1 = r * std::cos(phi);
        float t2 = r * std::sin(phi);
        float s = 0.5f * (1.0f + Vh.z());
        t2 = (1.0f - s) * std::sqrt(std::max(0.0f, 1.0f - t1 * t1)) + s * t2;

        Vector3f Nh = t1 * T1 + t2 * T2 + std::sqrt(std::max(0.0f, 1.0f - t1 * t1 - t2 * t2)) * Vh;

        Vector3f wh = Normalize(Vector3f(m_alphaX * Nh.x(), m_alphaY * Nh.y(), std::max(0.0f, Nh.z())));

        if (flip) wh = -wh;

        return wh;
    }

    float TrowbridgeReitzDistribution::pdf(const Vector3f& wo, const Vector3f& wh) const {
        if (wo.z() * wh.z() < 0.0f) return 0.0f;

        float cosThetaO = std::abs(wo.z());
        if (cosThetaO == 0.0f) return 0.0f;

        float dotOH = std::abs(Dot(wo, wh));
        return D(wh) * G1(wo) * dotOH / cosThetaO;
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