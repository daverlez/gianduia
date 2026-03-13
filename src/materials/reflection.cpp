#include <gianduia/materials/reflection.h>
#include <gianduia/math/warp.h>

namespace gnd {

    // ---- Lambertian reflection

    Color3f LambertianReflection::f(const Vector3f& wo, const Vector3f& wi) const {
        return m_albedo * M_1_PI;
    }

    Color3f LambertianReflection::sample(const Vector3f &wo, Vector3f &wi,
                                            const Point2f &sample, float uc, float &pdf,
                                            BxDFType *sampledType) const {
        if (wo.z() <= 0.0f) {
            pdf = 0.0f;
            return Color3f(0.0f);
        }

        wi = Warp::squareToCosineHemisphere(sample);
        pdf = Warp::squareToCosineHemispherePdf(wi);

        if (sampledType) *sampledType = type;

        // f(wi, wo) * cosTheta / pdf == m_albedo
        return m_albedo;
    }

    float LambertianReflection::pdf(const Vector3f& wo, const Vector3f& wi) const {
        return Warp::squareToCosineHemispherePdf(wi);
    }


    // ---- Specular reflection

    Color3f SpecularReflection::f(const Vector3f &wo, const Vector3f &wi) const {
        return 0.0f;        // Dirac's delta function: only sampling, no arbitrary evaluation
    }

    Color3f SpecularReflection::sample(const Vector3f& wo, Vector3f& wi,
                                            const Point2f& sample, float uc, float& pdf,
                                            BxDFType* sampledType) const {
        wi = Vector3f(-wo.x(), -wo.y(), wo.z());
        pdf = 1.0f;
        if (sampledType) *sampledType = type;

        return m_R * m_fresnel->evaluate(wo.z());
    }

    float SpecularReflection::pdf(const Vector3f& wo, const Vector3f& wi) const {
        return 0.0f;           // Dirac's delta function: no probability to sample the actual reflection direction
    }


    // ---- Fresnel specular (joint specular reflection and transmission)

    Color3f FresnelSpecular::f(const Vector3f &wo, const Vector3f &wi) const {
        return Color3f(0.0f);
    }

    Color3f FresnelSpecular::sample(const Vector3f& wo, Vector3f& wi,
                                           const Point2f& sample, float uc, float& pdf,
                                           BxDFType* sampledType) const {
        float F = FrDielectric(wo.z(), m_etaExt, m_etaInt);

        if (sample.x() < F) {
            // Sampling reflection lobe
            wi = Vector3f(-wo.x(), -wo.y(), wo.z());
            if (sampledType) *sampledType = BxDFType(BSDF_SPECULAR | BSDF_REFLECTION);
            pdf = F;

            return m_R;
        }

        // Sampling transmission lobe
        bool entering = wo.z() > 0;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Normal3f n(0, 0, 1);
        if (!entering) n = Normal3f(0, 0, -1);

        Vector3f wi_refracted;
        if (!Refract(wo, n, etaI / etaT, &wi_refracted)) {
            return Color3f(0.f);
        }

        wi = wi_refracted;
        if (sampledType) *sampledType = BxDFType(BSDF_SPECULAR | BSDF_TRANSMISSION);
        pdf = 1.f - F;

        float etaRatio = etaI / etaT;
        return m_T * etaRatio * etaRatio;
    }

    float FresnelSpecular::pdf(const Vector3f &wo, const Vector3f &wi) const {
        return 0.0f;
    }


    // ---- Microfacet reflection

    Color3f MicrofacetReflection::f(const Vector3f &wo, const Vector3f &wi) const {
        float cosThetaO = std::abs(wo.z()), cosThetaI = std::abs(wi.z());
        if (cosThetaO == 0.0f || cosThetaI == 0.0f) return Color3f(0.0f);
        if (wo.z() * wi.z() <= 0.0f) return Color3f(0.0f);

        Vector3f wh = wo + wi;
        if (wh.x() == 0.0f && wh.y() == 0.0f && wh.z() == 0.0f) return Color3f(0.0f);
        wh = Normalize(wh);
        if (wh.z() < 0.0f) wh = -wh;

        if (Dot(wo, wh) * wo.z() < 0.0f || Dot(wi, wh) * wi.z() < 0.0f)
            return Color3f(0.0f);

        Color3f F = m_fresnel->evaluate(Dot(wi, wh));
        float D = m_distribution->D(wh);
        float G = m_distribution->G(wo, wi);

        return m_R * D * G * F / (4.0f * cosThetaI * cosThetaO);
    }

    Color3f MicrofacetReflection::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample, float uc, float& pdf, BxDFType* sampledType) const {
        if (std::abs(wo.z()) < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        Vector3f wh = m_distribution->sample_wh(wo, sample);
        float dotO = Dot(wo, wh);
        if (dotO <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        wi = -wo + wh * 2.0f * dotO;

        if (wo.z() * wi.z() <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        pdf = this->pdf(wo, wi);
        if (sampledType) *sampledType = type;

        return f(wo, wi) * std::abs(wi.z()) / pdf;
    }

    float MicrofacetReflection::pdf(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() * wi.z() <= 0.0f) return 0.0f;
        Vector3f wh = Normalize(wo + wi);
        if (wh.z() < 0.0f) wh = -wh;

        return m_distribution->pdf(wo, wh) / (4.0f * Dot(wo, wh));
    }


    // ---- Microfacet Fresnel (joint microfacet reflection and transmission)

    Color3f MicrofacetFresnel::f(const Vector3f &wo, const Vector3f &wi) const {
        bool reflect = (wo.z() * wi.z() > 0.0f);

        bool entering = wo.z() > 0.0f;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Vector3f wh;
        if (reflect) {
            wh = Normalize(wo + wi);
        } else {
            wh = Normalize(wo * etaI + wi * etaT);
        }
        if (wh.z() < 0.0f) wh = -wh;

        // Discard backfacing microfacets
        if (Dot(wo, wh) * wo.z() < 0.0f || Dot(wi, wh) * wi.z() < 0.0f)
            return Color3f(0.0f);

        // Cosines with respect to halfway vector (microfacet normal)
        float dotO = Dot(wo, wh);
        float dotI = Dot(wi, wh);

        float F = FrDielectric(dotO, m_etaExt, m_etaInt);
        float D = m_distribution->D(wh);
        float G = m_distribution->G(wo, wi);

        if (reflect)
            return m_R * D * G * F / (4.0f * std::abs(wo.z() * wi.z()));

        if (dotO * dotI > 0.0f) return Color3f(0.0f);

        float denom = (etaI * dotO + etaT * dotI);
        denom *= denom;
        float factor = std::abs(dotO * dotI) / std::abs(wo.z() * wi.z());

        return m_T * factor * (etaI * etaI) * D * G * (1.0f - F) / denom;
    }

    Color3f MicrofacetFresnel::sample(const Vector3f &wo, Vector3f &wi,
                                        const Point2f &sample, float uc, float &pdf, BxDFType *sampledType) const {
        if (std::abs(wo.z()) < Epsilon) return Color3f(0.0f);

        Vector3f wh = m_distribution->sample_wh(wo, sample);
        float dotO = Dot(wo, wh);
        if (dotO <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }

        float F = FrDielectric(dotO, m_etaExt, m_etaInt);

        if (uc < F) {
            wi = -wo + wh * 2.0f * dotO;
            if (wo.z() * wi.z() <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }
            if (sampledType) *sampledType = BxDFType(BSDF_REFLECTION | BSDF_GLOSSY);
        } else {
            bool entering = wo.z() > 0.0f;
            float etaI = entering ? m_etaExt : m_etaInt;
            float etaT = entering ? m_etaInt : m_etaExt;

            Normal3f n_wh(wh.x(), wh.y(), wh.z());
            if (!Refract(wo, n_wh, etaI / etaT, &wi)) { pdf = 0.0f; return Color3f(0.0f); }
            if (wo.z() * wi.z() > 0.0f) { pdf = 0.0f; return Color3f(0.0f); }
            if (sampledType) *sampledType = BxDFType(BSDF_TRANSMISSION | BSDF_GLOSSY);
        }

        pdf = this->pdf(wo, wi);

        return f(wo, wi) * std::abs(wi.z()) / pdf;
    }

    float MicrofacetFresnel::pdf(const Vector3f &wo, const Vector3f &wi) const {
        bool reflect = (wo.z() * wi.z() > 0.0f);

        bool entering = wo.z() > 0.0f;
        float etaI = entering ? m_etaExt : m_etaInt;
        float etaT = entering ? m_etaInt : m_etaExt;

        Vector3f wh;
        if (reflect) wh = Normalize(wo + wi);
        else wh = Normalize(wo * etaI + wi * etaT);
        if (wh.z() < 0.0f) wh = -wh;

        float dotO = Dot(wo, wh);
        float dotI = Dot(wi, wh);

        float F = FrDielectric(dotO, m_etaExt, m_etaInt);
        float pdf_wh = m_distribution->pdf(wo, wh);

        if (reflect)
            return F * pdf_wh / (4.0f * std::abs(dotO));

        if (dotO * dotI > 0.0f) return 0.0f;

        float denom = (etaI * dotO + etaT * dotI);
        float dwh_dwi = (etaT * etaT * std::abs(dotI)) / (denom * denom);
        return (1.0f - F) * pdf_wh * dwh_dwi;
    }


    // ---- Smooth Plastic

    Color3f SmoothPlasticBxDF::f(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() < Epsilon || wi.z() < Epsilon) return Color3f(0.0f);

        float Fo = FrDielectric(wo.z(), 1.0f, m_eta);
        float Fi = FrDielectric(wi.z(), 1.0f, m_eta);
        return m_Kd * M_1_PI * (1.0f - Fo) * (1.0f - Fi);
    }

    Color3f SmoothPlasticBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample, float uc, float& pdf, BxDFType* sampledType) const {
        if (wo.z() < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        float probSpec = FrDielectric(wo.z(), 1.0f, m_eta);

        if (uc < probSpec) {
            wi = Vector3f(-wo.x(), -wo.y(), wo.z());
            pdf = probSpec;
            if (sampledType) *sampledType = BxDFType(BSDF_REFLECTION | BSDF_SPECULAR);
            //(Ks * F) / probSpec = Ks
            return m_Ks;
        }

        wi = Warp::squareToCosineHemisphere(sample);
        if (wi.z() < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }
        pdf = (1.0f - probSpec) * Warp::squareToCosineHemispherePdf(wi);
        if (sampledType) *sampledType = BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE);
        return f(wo, wi) * std::abs(wi.z()) / pdf;
    }

    float SmoothPlasticBxDF::pdf(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() < Epsilon || wi.z() < Epsilon) return 0.0f;
        float Fo = FrDielectric(wo.z(), 1.0f, m_eta);
        return (1.0f - Fo) * Warp::squareToCosineHemispherePdf(wi);
    }


    // ---- Rough Plastic

    Color3f RoughPlasticBxDF::f(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() < Epsilon || wi.z() < Epsilon) return Color3f(0.0f);

        float Fo = FrDielectric(wo.z(), 1.0f, m_eta);
        float Fi = FrDielectric(wi.z(), 1.0f, m_eta);
        Color3f diffuse = m_Kd * M_1_PI * (1.0f - Fo) * (1.0f - Fi);

        Vector3f wh = Normalize(wo + wi);
        if (wh.z() < 0.0f) wh = -wh;
        if (Dot(wo, wh) * wo.z() < 0.0f || Dot(wi, wh) * wi.z() < 0.0f)
            return Color3f(0.0f);

        float Fh = FrDielectric(Dot(wo, wh), 1.0f, m_eta);
        float D = m_distrib->D(wh);
        float G = m_distrib->G(wo, wi);
        Color3f specular = m_Ks * D * G * Fh / (4.0f * wo.z() * wi.z());

        return diffuse + specular;
    }

    Color3f RoughPlasticBxDF::sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample, float uc, float& pdf, BxDFType* sampledType) const {
        if (wo.z() < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }

        float probSpec = FrDielectric(wo.z(), 1.0f, m_eta);

        if (uc < probSpec) {
            Vector3f wh = m_distrib->sample_wh(wo, sample);
            float dotO = Dot(wo, wh);
            if (dotO <= 0.0f) { pdf = 0.0f; return Color3f(0.0f); }
            wi = -wo + wh * 2.0f * dotO;
            if (wi.z() < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }
            if (sampledType) *sampledType = BxDFType(BSDF_REFLECTION | BSDF_GLOSSY);
        } else {
            wi = Warp::squareToCosineHemisphere(sample);
            if (wi.z() < Epsilon) { pdf = 0.0f; return Color3f(0.0f); }
            if (sampledType) *sampledType = BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE);
        }

        pdf = this->pdf(wo, wi);
        return f(wo, wi) * std::abs(wi.z()) / pdf;
    }

    float RoughPlasticBxDF::pdf(const Vector3f &wo, const Vector3f &wi) const {
        if (wo.z() < Epsilon || wi.z() < Epsilon) return 0.0f;
        Vector3f wh = Normalize(wo + wi);
        if (wh.z() < 0.0f) wh = -wh;

        float probSpec = FrDielectric(wo.z(), 1.0f, m_eta);

        float pdfSpec = m_distrib->pdf(wo, wh) / (4.0f * Dot(wo, wh));
        float pdfDiff = Warp::squareToCosineHemispherePdf(wi);

        return probSpec * pdfSpec + (1.0f - probSpec) * pdfDiff;
    }

}