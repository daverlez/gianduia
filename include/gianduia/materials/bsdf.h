#pragma once
#include <gianduia/materials/bxdf.h>
#include <gianduia/math/frame.h>

namespace gnd {

    struct SurfaceInteraction;

    class BSDF {
    public:
        BSDF(const SurfaceInteraction& isect, float eta = 1.0f);

        /// Adds a lobe previously allocated in the memory arena
        void add(BxDF* bxdf, float sampleWeight = 1.0f) {
            if (m_numBxDFs < MaxBxDFs) {
                m_bxdfs[m_numBxDFs] = bxdf;
                m_weights[m_numBxDFs] = sampleWeight;
                m_numBxDFs++;
            }
        }

        /// Returns the number of lobes matching the specified flag.
        int numComponents(BxDFType flags = BSDF_ALL) const;

        /// Evaluates the function given directions in world space. Both directions point away from the
        /// evaluation point.
        /// @param woWorld outgoing direction (follows path towards camera)
        /// @param wiWorld incident direction (follows path towards light)
        /// @param flags restricts evaluation to the specified lobe flags
        /// @return value given the specified directions
        Color3f f(const Vector3f& woWorld, const Vector3f& wiWorld,
                  BxDFType flags = BSDF_ALL) const;

        /// Samples a new direction wi given the following parameters. All specified directions point away from the
        /// evaluation point.
        /// @param woWorld known direction
        /// @param wiWorld sampled direction
        /// @param sample 2D uniform sample in [0,1] x [0,1]
        /// @param uc 1D uniform sample in [0, 1]
        /// @param pdf probability of sampling wi given wo (same result from pdf(wo, wi))
        /// @param sampledType specifies the sampled lobe type
        /// @param type restricts sampling to the specified lobe flags
        /// @return the value of f computed on the two directions, accounting for the foreshortening term and divided
        /// by the pdf with respect to solid angle.
        Color3f sample(const Vector3f& woWorld, Vector3f* wiWorld,
                         const Point2f& sample, float uc, float* pdf,
                         BxDFType* sampledType = nullptr,
                         BxDFType type = BSDF_ALL) const;

        /// Computes the PDF of sampling wi given wo, given the specified flags.
        float pdf(const Vector3f& woWorld, const Vector3f& wiWorld,
                  BxDFType flags = BSDF_ALL) const;

        const float eta;
        Frame frame;

    private:
        static constexpr int MaxBxDFs = 8;
        int m_numBxDFs = 0;
        BxDF* m_bxdfs[MaxBxDFs];
        float m_weights[MaxBxDFs];
    };

}