#include <gianduia/materials/bsdf.h>
#include <gianduia/shapes/shape.h>

namespace gnd {

    BSDF::BSDF(const SurfaceInteraction& isect, float eta) : eta(eta) {
        Vector3f tangent = isect.dpdu;
        Vector3f bitangent = Normalize(Cross(isect.n, isect.dpdu));
        frame = Frame(tangent, bitangent, static_cast<Vector3f>(isect.n));
    }

    int BSDF::numComponents(BxDFType flags) const {
        int num = 0;
        for (int i = 0; i < m_numBxDFs; ++i)
            if (m_bxdfs[i]->matchesFlags(flags)) num++;
        return num;
    }

    Color3f BSDF::f(const Vector3f& woWorld, const Vector3f& wiWorld, BxDFType flags) const {
        Vector3f wo = frame.toLocal(woWorld);
        Vector3f wi = frame.toLocal(wiWorld);

        if (wo.z() == 0.0f) return Color3f(0.0f);

        Color3f result(0.0f);
        for (int i = 0; i < m_numBxDFs; ++i)
            if (m_bxdfs[i]->matchesFlags(flags))
                result += m_bxdfs[i]->f(wo, wi);

        return result;
    }

    Color3f BSDF::sample(const Vector3f& woWorld, Vector3f* wiWorld,
                           const Point2f& sample, float uc, float* pdf,
                           BxDFType* sampledType, BxDFType type) const {

        int matchingComps = numComponents(type);
        if (matchingComps == 0) {
            *pdf = 0.0f;
            return Color3f(0.0f);
        }

        float matchingWeightSum = 0.0f;
        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(type)) {
                matchingWeightSum += m_weights[i];
            }
        }

        if (matchingWeightSum == 0.0f) {
            *pdf = 0.0f;
            return Color3f(0.0f);
        }

        BxDF* bxdf = nullptr;
        float cdf = 0.0f;
        float selectedProb = 0.0f;

        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(type)) {
                float prob = m_weights[i] / matchingWeightSum;
                cdf += prob;
                if (uc <= cdf || i == m_numBxDFs - 1) {
                    bxdf = m_bxdfs[i];
                    selectedProb = prob;
                    break;
                }
            }
        }

        Vector3f wo = frame.toLocal(woWorld);
        if (wo.z() == 0.0f) return Color3f(0.0f);

        Vector3f wi;
        float lobePdf = 0.0f;

        Color3f f_val = bxdf->sample(wo, wi, sample, uc, lobePdf, sampledType);

        if (lobePdf == 0.0f || f_val.isBlack()) {
            *pdf = 0.0f;
            return Color3f(0.0f);
        }

        *wiWorld = frame.toWorld(wi);

        if (!(bxdf->type & BSDF_SPECULAR) && matchingComps > 1) {
            *pdf = this->pdf(woWorld, *wiWorld, type);
            return this->f(woWorld, *wiWorld, type);
        } else {
            *pdf = lobePdf * selectedProb;
            return f_val;
        }
    }

    float BSDF::pdf(const Vector3f& woWorld, const Vector3f& wiWorld, BxDFType flags) const {
        int matchingComps = numComponents(flags);
        if (matchingComps == 0) return 0.0f;

        Vector3f wo = frame.toLocal(woWorld);
        Vector3f wi = frame.toLocal(wiWorld);

        if (wo.z() == 0.0f) return 0.0f;

        float matchingWeightSum = 0.0f;
        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(flags)) {
                matchingWeightSum += m_weights[i];
            }
        }

        if (matchingWeightSum == 0.0f) return 0.0f;

        float pdfSum = 0.0f;
        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(flags)) {
                float prob = m_weights[i] / matchingWeightSum;
                pdfSum += prob * m_bxdfs[i]->pdf(wo, wi);
            }
        }

        return pdfSum;
    }

}