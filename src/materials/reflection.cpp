#include <gianduia/materials/reflection.h>
#include <gianduia/math/warp.h>

namespace gnd {

    // ---- Lambertian reflection

    Color3f LambertianReflection::f(const Vector3f& wo, const Vector3f& wi) const {
        return m_albedo * M_1_PI;
    }

    Color3f LambertianReflection::sample(const Vector3f &wo, Vector3f &wi,
                                            const Point2f &sample, float &pdf,
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
                                            const Point2f& sample, float& pdf,
                                            BxDFType* sampledType) const {
        wi = Vector3f(-wi.x(), -wi.y(), wi.z());
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
                                           const Point2f& sample, float& pdf,
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
        float etaT = entering ? m_etaExt : m_etaInt;

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

}