#pragma once
#include "bxdf.h"
#include "fresnel.h"
#include "gianduia/math/constants.h"

namespace gnd {

    class LambertianReflection : public BxDF {
    public:
        LambertianReflection(const Color3f& albedo) :
            BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE)), m_albedo(albedo) {}

        Color3f f(const Vector3f &wo, const Vector3f &wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi,
                        const Point2f& sample, float& pdf,
                        BxDFType* sampledType = nullptr) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
    private:
        Color3f m_albedo;
    };

    class SpecularReflection : public BxDF {
    public:
        SpecularReflection(Color3f R, const Fresnel* fresnel) :
            BxDF(BxDFType(BSDF_REFLECTION | BSDF_SPECULAR)), m_R(R), m_fresnel(fresnel) {}

        Color3f f(const Vector3f &wo, const Vector3f &wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi,
                        const Point2f& sample, float& pdf,
                        BxDFType* sampledType = nullptr) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
    private:
        Color3f m_R;
        const Fresnel* m_fresnel;
    };

    class FresnelSpecular : public BxDF {
    public:
        FresnelSpecular(const Color3f& R, const Color3f& T, float etaExt, float etaInt)
            : BxDF(BxDFType(BSDF_REFLECTION | BSDF_TRANSMISSION | BSDF_SPECULAR)),
              m_R(R), m_T(T), m_etaExt(etaExt), m_etaInt(etaInt) {}

        Color3f f(const Vector3f &wo, const Vector3f &wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi,
                       const Point2f& sample, float& pdf,
                       BxDFType* sampledType = nullptr) const override;
        float pdf(const Vector3f &wo, const Vector3f &wi) const override;
    private:
        Color3f m_R;
        Color3f m_T;
        float m_etaExt;
        float m_etaInt;
    };

}
