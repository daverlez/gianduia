#pragma once
#include <gianduia/math/geometry.h>
#include <gianduia/math/color.h>

namespace gnd {

    enum BxDFType {
        BSDF_REFLECTION   = 1 << 0,
        BSDF_TRANSMISSION = 1 << 1,
        BSDF_DIFFUSE      = 1 << 2,
        BSDF_GLOSSY       = 1 << 3,
        BSDF_SPECULAR     = 1 << 4,
        BSDF_ALL          = BSDF_REFLECTION | BSDF_TRANSMISSION |
                            BSDF_DIFFUSE | BSDF_GLOSSY | BSDF_SPECULAR
    };

    class BxDF {
    public:
        explicit BxDF(BxDFType type) : type(type) {}
        virtual ~BxDF() = default;

        bool matchesFlags(BxDFType t) const { return (type & t) == type; }

        /// Evaluates the function given directions in local coordinates. Both directions point away from the
        /// evaluation point.
        /// @param wo outgoing direction (follows path towards camera)
        /// @param wi incident direction (follows path towards light)
        /// @return lobe value given the specified directions
        virtual Color3f f(const Vector3f& wo, const Vector3f& wi) const = 0;

        /// Samples a new direction wi given the following parameters. All specified directions point away from the
        /// evaluation point.
        /// @param wo known direction
        /// @param wi sampled direction
        /// @param sample 2D uniform sample in [0,1] x [0,1]
        /// @param pdf probability of sampling wi given wo (same result from pdf(wo, wi))
        /// @param sampledType specifies the sampled lobe type
        /// @return the value of f computed on the two directions, accounting for the foreshortening term and divided
        /// by the pdf with respect to solid angle.
        virtual Color3f sample(const Vector3f& wo, Vector3f& wi,
                                 const Point2f& sample, float& pdf,
                                 BxDFType* sampledType = nullptr) const = 0;

        /// Computes the PDF of sampling wi given wo.
        virtual float pdf(const Vector3f& wo, const Vector3f& wi) const = 0;

        const BxDFType type;
    };

}