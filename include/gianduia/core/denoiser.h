#pragma once
#include <gianduia/core/bitmap.h>

namespace gnd {
    class Denoiser {
    public:
        Denoiser() = default;

        void execute(Bitmap* film);
    };
}