#pragma once
#include <gianduia/core/film.h>

namespace gnd {
    class Denoiser {
    public:
        Denoiser() = default;

        void execute(Film* film);
    };
}