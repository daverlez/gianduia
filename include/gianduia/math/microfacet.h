#pragma once
#include <gianduia/math/geometry.h>
#include <gianduia/math/constants.h>
#include <cmath>
#include <algorithm>

namespace gnd {

    class MicrofacetDistribution {
    public:
        virtual ~MicrofacetDistribution() = default;

        /// Normal Distribution Function
        virtual float D(const Vector3f& wh) const = 0;

        /// Masking-Shadowing Function
        virtual float G(const Vector3f& wo, const Vector3f& wi) const = 0;

        virtual float G1(const Vector3f& w) const = 0;

        virtual Vector3f sample_wh(const Vector3f& wo, const Point2f& sample) const = 0;

        /// PDF of sampling wh given known direction wo
        virtual float pdf(const Vector3f& wo, const Vector3f& wh) const = 0;
    };


    class TrowbridgeReitzDistribution : public MicrofacetDistribution {
    public:
        TrowbridgeReitzDistribution(float alpha) : m_alpha(std::max(1e-4f, alpha)) {}

        float D(const Vector3f& wh) const override;
        float G(const Vector3f& wo, const Vector3f& wi) const override;
        float G1(const Vector3f& w) const override;
        Vector3f sample_wh(const Vector3f& wo, const Point2f& sample) const override;
        float pdf(const Vector3f& wo, const Vector3f& wh) const override;

        static float roughnessToAlpha(float roughness) {
            roughness = std::max(roughness, 1e-3f);
            return roughness * roughness;
        }

    private:
        float m_alpha;
    };

}