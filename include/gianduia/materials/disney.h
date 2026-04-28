#pragma once
#include <gianduia/materials/material.h>
#include <gianduia/materials/bxdf.h>
#include <gianduia/math/microfacet.h>
#include <gianduia/materials/fresnel.h>

namespace gnd {

    class DisneyDiffuseBxDF : public BxDF {
    public:
        DisneyDiffuseBxDF(const Color3f& R, float roughness, float subsurface)
            : BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE)), m_R(R),
              m_roughness(roughness), m_subsurface(subsurface) {}

        Color3f f(const Vector3f& wo, const Vector3f& wi) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                       float uc, float& pdf, BxDFType* sampledType) const override;

    private:
        Color3f m_R;
        float m_roughness, m_subsurface;
    };

    class DisneySheenBxDF : public BxDF {
    public:
        DisneySheenBxDF(const Color3f& R, float tint)
            : BxDF(BxDFType(BSDF_REFLECTION | BSDF_DIFFUSE)), m_R(R), m_tint(tint) {}

        Color3f f(const Vector3f& wo, const Vector3f& wi) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                       float uc, float& pdf, BxDFType* sampledType) const override;
    private:
        Color3f m_R;
        float m_tint;
    };

    class DisneyClearcoatBxDF : public BxDF {
    public:
        DisneyClearcoatBxDF(float weight, float gloss)
            : BxDF(BxDFType(BSDF_REFLECTION | BSDF_GLOSSY)), m_weight(weight), m_gloss(gloss) {
            m_distrib = GTR1Distribution(std::lerp(0.1f, 0.001f, gloss));
        }

        Color3f f(const Vector3f& wo, const Vector3f& wi) const override;
        float pdf(const Vector3f& wo, const Vector3f& wi) const override;
        Color3f sample(const Vector3f& wo, Vector3f& wi, const Point2f& sample,
                       float uc, float& pdf, BxDFType* sampledType) const override;
    private:
        float m_weight, m_gloss;
        MicrofacetDistribution* m_distrib;
    };

}