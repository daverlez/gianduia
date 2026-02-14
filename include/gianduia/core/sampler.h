#pragma once

#include "gianduia/core/object.h"
#include <memory>

namespace gnd {

    class Sampler : public GndObject {
    public:
        virtual ~Sampler() = default;

        /// Clones this instance of the sampler. This allows different render threads to have their own sampler.
        virtual std::unique_ptr<Sampler> clone() const = 0;

        virtual void seed(uint64_t seedOffset) = 0;

        virtual float next1D() = 0;

        virtual Point2f next2D() = 0;

        virtual size_t getSampleCount() const { return m_sampleCount; }

        EClassType getClassType() const override { return ESampler; }

    protected:
        size_t m_sampleCount;
    };

}