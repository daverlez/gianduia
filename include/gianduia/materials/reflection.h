#pragma once
#include "bxdf.h"
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

}
