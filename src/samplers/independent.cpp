#include "gianduia/core/factory.h"
#include "gianduia/core/sampler.h"
#include "gianduia/core/pcg32.h"

namespace gnd {

    class IndependentSampler : public Sampler {
    public:
        IndependentSampler(const PropertyList& props) {
            m_sampleCount = props.getInteger("sampleCount", 1);
        }

        IndependentSampler(const IndependentSampler& other)
            : m_rng(other.m_rng) {
            m_sampleCount = other.m_sampleCount;
        }

        std::unique_ptr<Sampler> clone() const override {
            return std::make_unique<IndependentSampler>(*this);
        }

        void seed(uint64_t seedOffset) override {
            m_rng.seed(seedOffset);
        }

        float next1D() override {
            return m_rng.nextFloat();
        }

        Point2f next2D() override {
            return Point2f(m_rng.nextFloat(), m_rng.nextFloat());
        }

        std::string toString() const override {
            return std::format("IndependentSampler[sampleCount={}]", m_sampleCount);
        }

    private:
        PCG32 m_rng;
    };

    GND_REGISTER_CLASS(IndependentSampler, "independent");

}