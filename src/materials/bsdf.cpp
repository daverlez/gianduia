#include <gianduia/materials/bsdf.h>
#include <gianduia/shapes/shape.h>

#include <algorithm>

namespace gnd {

    BSDF::BSDF(const SurfaceInteraction& isect, float eta)
        : eta(eta), frame(Vector3f(isect.n.x(), isect.n.y(), isect.n.z())) { }

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
                           const Point2f& sample, float* pdf,
                           BxDFType* sampledType, BxDFType type) const {

        int matchingComps = numComponents(type);
        if (matchingComps == 0) {
            *pdf = 0.0f;
            return Color3f(0.0f);
        }

        int comp = std::min((int)(sample.x() * matchingComps), matchingComps - 1);

        BxDF* bxdf = nullptr;
        int count = comp;
        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(type)) {
                if (count == 0) {
                    bxdf = m_bxdfs[i];
                    break;
                }
                count--;
            }
        }

        Vector3f wo = frame.toLocal(woWorld);
        if (wo.z() == 0.0f) return Color3f(0.0f);

        Point2f remappedSample(sample.x() * matchingComps - comp, sample.y());

        Vector3f wi;
        float lobePdf = 0.0f;
        Color3f f_val = bxdf->sample(wo, wi, remappedSample, lobePdf, sampledType);

        if (lobePdf == 0.0f || f_val.isBlack()) {
            *pdf = 0.0f;
            return Color3f(0.0f);
        }

        *wiWorld = frame.toWorld(wi);

        if (!(bxdf->type & BSDF_SPECULAR) && matchingComps > 1) {
            *pdf = lobePdf;
            for (int i = 0; i < m_numBxDFs; ++i) {
                if (m_bxdfs[i] != bxdf && m_bxdfs[i]->matchesFlags(type)) {
                    *pdf += m_bxdfs[i]->pdf(wo, wi);
                    f_val += m_bxdfs[i]->f(wo, wi);
                }
            }
        } else if (matchingComps > 1) {
            *pdf = lobePdf;
        }

        *pdf /= matchingComps;

        return f_val;
    }

    float BSDF::pdf(const Vector3f& woWorld, const Vector3f& wiWorld, BxDFType flags) const {
        int matchingComps = numComponents(flags);
        if (matchingComps == 0) return 0.0f;

        Vector3f wo = frame.toLocal(woWorld);
        Vector3f wi = frame.toLocal(wiWorld);

        if (wo.z() == 0.0f) return 0.0f;

        float pdfSum = 0.0f;
        for (int i = 0; i < m_numBxDFs; ++i) {
            if (m_bxdfs[i]->matchesFlags(flags)) {
                pdfSum += m_bxdfs[i]->pdf(wo, wi);
            }
        }

        return pdfSum / matchingComps;
    }

}