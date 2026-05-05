#include "gianduia/math/distribution.h"
#include <algorithm>

namespace gnd {

    Distribution1D::Distribution1D(const float* f, int n) : func(f, f + n), cdf(n + 1) {
        cdf[0] = 0.0f;
        for (int i = 1; i < n + 1; ++i) {
            cdf[i] = cdf[i - 1] + func[i - 1] / n;
        }
        funcInt = cdf[n];

        if (funcInt == 0.0f) {
            for (int i = 1; i < n + 1; ++i) cdf[i] = float(i) / float(n);
        } else {
            for (int i = 1; i < n + 1; ++i) cdf[i] /= funcInt;
        }
    }

    float Distribution1D::SampleContinuous(float u, float* pdf, int* off) const {
        auto it = std::upper_bound(cdf.begin(), cdf.end(), u);
        int offset = std::clamp<int>(std::distance(cdf.begin(), it) - 1, 0, cdf.size() - 2);
        
        if (off) *off = offset;
        
        float du = u - cdf[offset];
        if ((cdf[offset + 1] - cdf[offset]) > 0.0f) {
            du /= (cdf[offset + 1] - cdf[offset]);
        }
        
        if (pdf) *pdf = (funcInt > 0.0f) ? func[offset] / funcInt : 0.0f;
        
        return (offset + du) / Count();
    }

    int Distribution1D::SampleDiscrete(float u, float* pdf, float* uRemapped) const {
        auto it = std::upper_bound(cdf.begin(), cdf.end(), u);
        int offset = std::clamp<int>(std::distance(cdf.begin(), it) - 1, 0, cdf.size() - 2);
        
        if (pdf) *pdf = (funcInt > 0.0f) ? (func[offset] / (funcInt * Count())) : 0.0f;
        if (uRemapped) {
            *uRemapped = (u - cdf[offset]) / (cdf[offset + 1] - cdf[offset]);
        }
        return offset;
    }

    float Distribution1D::DiscretePDF(int index) const {
        if (funcInt == 0.0f) return 0.0f;
        return func[index] / (funcInt * Count());
    }

    Distribution2D::Distribution2D(const float* data, int nu, int nv) {
        pConditionalV.reserve(nv);
        for (int v = 0; v < nv; ++v) {
            pConditionalV.push_back(std::make_unique<Distribution1D>(&data[v * nu], nu));
        }
        
        std::vector<float> marginalFunc;
        marginalFunc.reserve(nv);
        for (int v = 0; v < nv; ++v) {
            marginalFunc.push_back(pConditionalV[v]->funcInt);
        }

        pMarginal = std::make_unique<Distribution1D>(marginalFunc.data(), nv);
    }

    Point2f Distribution2D::SampleContinuous(const Point2f& u, float* pdf) const {
        float pdfs[2];
        int v;
        float d1 = pMarginal->SampleContinuous(u.y(), &pdfs[1], &v);
        float d0 = pConditionalV[v]->SampleContinuous(u.x(), &pdfs[0]);
        
        *pdf = pdfs[0] * pdfs[1];
        return Point2f(d0, d1);
    }

    float Distribution2D::Pdf(const Point2f& p) const {
        int iu = std::clamp<int>(p.x() * pConditionalV[0]->Count(), 0, pConditionalV[0]->Count() - 1);
        int iv = std::clamp<int>(p.y() * pMarginal->Count(), 0, pMarginal->Count() - 1);
        return pConditionalV[iv]->func[iu] / pMarginal->funcInt;
    }

}