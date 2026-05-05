#pragma once

#include <concepts>
#include "gianduia/core/sampler.h"

namespace gnd {

    template<typename T>
    concept ReservoirPayload = std::default_initializable<T> && std::copyable<T>;

    template <ReservoirPayload T>
    struct Reservoir {
        T sample;
        float w_sum = 0.0f;
        float target_pdf = 0.0f;
        int M = 0;

        Reservoir() = default;

        bool add(const T& candidate, float candidateTargetPdf, float candidateSourcePdf, float u) {
            M++;

            float weight = 0.0f;
            if (candidateSourcePdf > 0.0f) {
                weight = candidateTargetPdf / candidateSourcePdf;
            }

            w_sum += weight;

            if (u * w_sum < weight) {
                sample = candidate;
                target_pdf = candidateTargetPdf;
                return true;
            }
            
            return false;
        }

        float getFinalWeight() const {
            if (target_pdf == 0.0f || M == 0) return 0.0f;
            return w_sum / (static_cast<float>(M) * target_pdf);
        }

        void reset() {
            w_sum = 0.0f;
            target_pdf = 0.0f;
            M = 0;
        }
    };

}