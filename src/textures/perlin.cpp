#include <gianduia/textures/perlin.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatPerlin = PerlinNoiseTexture<float>;
    using ColorPerlin = PerlinNoiseTexture<Color3f>;

    GND_REGISTER_CLASS(FloatPerlin, "perlin_float");
    GND_REGISTER_CLASS(ColorPerlin, "perlin_color");

}