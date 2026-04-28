#include "gianduia/materials/disney.h"

namespace gnd {

    // Diffuse lobe

    Color3f DisneyDiffuseBxDF::f(const Vector3f &wo, const Vector3f &wi) const {
        float cosThetaO = Frame::cosTheta(wo);
        float cosThetaI = Frame::cosTheta(wi);

        float Fo = SchlickWeight(cosThetaO);
        float Fi = SchlickWeight(cosThetaI);

        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        // Base diffuse
        float FD90 = 0.5f + 2.0f * m_roughness * cosThetaD * cosThetaD;
        float fDiffuse = (1.0f + (FD90 - 1.0f) * Fo) * (1.0f + (FD90 - 1.0f) * Fi);

        // Subsurface scattering approximation (Hanrahan - Krueger)
        float FSS90 = m_roughness * cosThetaD * cosThetaD;
        float Fss = (1.0f + (FSS90 - 1.0f) * Fi) * (1.0f + (FSS90 - 1.0f) * Fo);
        float fSubsurface = 1.25f * (Fss * (1.0f / (cosThetaO + cosThetaI) - 0.5f) + 0.5f);

        return InvPi * m_R * Lerp(m_subsurface, fDiffuse, fSubsurface);
    }

    float DisneyDiffuseBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        return Warp::squareToCosineHemispherePdf(wo);
    }

    Color3f DisneyDiffuseBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const override {
        if (wo.z() <= 0.0f) {
            pdf = 0.0f;
            return Color3f(0.0f);
        }

        wi = Warp::squareToCosineHemisphere(sample);
        pdf = Warp::squareToCosineHemispherePdf(wi);

        if (sampledType) *sampledType = type;

        // f(wi, wo) * cosTheta / pdf == m_albedo
        return m_R;
    }


    // Sheen lobe

    Color3f DisneySheenBxDF::f(const Vector3f& wo, const Vector3f& wi) const {
        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        float lum = m_R.luminance();
        Color3f tintColor = lum > 0.0f ? m_R / lum : Color3f(1.0f);

        return Lerp(m_tint, Color3f(1.0f), tintColor) * SchlickWeight(cosThetaD);
    }

    float DisneySheenBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        // Not registered for sampling in the overall model
        return 0.0f;
    }

    Color3f DisneySheenBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const override {
        // Not registered for sampling in the overall model
        pdf = 0.0f;
        return Color3f(0.0f);
    }


    // Clearcoat lobe

    Color3f DisneyClearcoatBxDF::f(const Vector3f& wo, const Vector3f& wi) const {
        Vector3f wh = Normalize(wo + wi);
        float cosThetaD = Dot(wi, wh);

        float D = m_distrib->D(wh);
        float F = FrSchlick(0.04f, cosThetaD);
        float G = m_distrib->G(wo, wi);
        float invDen = 4.0f * std::abs(Dot(wi, wo));

        return Color3f(m_weight * D * F * G * invDen);
    }

    float DisneyClearcoatBxDF::pdf(const Vector3f& wo, const Vector3f& wi) const {
        Vector3f wh = Normalize(wo + wi);
        float invDen = 4.0f * std::abs(Dot(wi, wo));

        return m_distrib->pdf(wo, wh) * invDen;
    }

    Color3f DisneyClearcoatBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                                        float uc, float& pdf, BxDFType* sampledType) const override {
        if (std::abs(wo.z()) < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        Vector3f wh = m_distrib->sample_wh(wo, sample);
        float dotO = Dot(wo, wh);
        if (dotO <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        wi = -wo + wh * 2.0f * dotO;
        if (wo.z() * wi.z() <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        pdf = this->pdf(wo, wi);
        if (sampledType) *sampledType = type;

        return f(wo, wi) * std::abs(wi.z()) / pdf;
    }
}
