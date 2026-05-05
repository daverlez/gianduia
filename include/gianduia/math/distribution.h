#pragma once

#include <vector>
#include <memory>
#include "gianduia/math/geometry.h"

namespace gnd {

    class Distribution1D {
    public:
        Distribution1D(const float* f, int n);
        
        int Count() const { return func.size(); }

        float SampleContinuous(float u, float* pdf, int* off = nullptr) const;
        int SampleDiscrete(float u, float* pdf = nullptr, float* uRemapped = nullptr) const;
        
        float DiscretePDF(int index) const;

        std::vector<float> func, cdf;
        float funcInt;
    };

    class Distribution2D {
    public:
        Distribution2D(const float* data, int nu, int nv);

        Point2f SampleContinuous(const Point2f& u, float* pdf) const;
        float Pdf(const Point2f& p) const;

    private:
        std::vector<std::unique_ptr<Distribution1D>> pConditionalV;
        std::unique_ptr<Distribution1D> pMarginal;
    };

}