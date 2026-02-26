#pragma once
#include "bxdf.h"
#include "gianduia/math/constants.h"

namespace gnd {

    class SpecularTransmission : public BxDF {
    public:
        SpecularTransmission(Color3f T, float etaExt, float etaInt) :
            BxDF(BxDFType(BSDF_TRANSMISSION | BSDF_SPECULAR)), m_T(T), m_etaExt(etaExt), m_etaInt(etaInt){}

        Color3f f(const Vector3f &wo, const Vector3f &wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi,
                        const Point2f& sample, float& pdf,
                                    BxDFType* sampledType = nullptr) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
    private:
        Color3f m_T;
        float m_etaExt;     // external index of refraction
        float m_etaInt;     // internal index of refraction
    };

}
