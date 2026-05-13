#include <gianduia/textures/wood.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatWood = WoodTexture<float>;
    using ColorWood = WoodTexture<Color3f>;

    GND_REGISTER_CLASS(FloatWood, "wood_float");
    GND_REGISTER_CLASS(ColorWood, "wood_color");

}