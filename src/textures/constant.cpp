#include <gianduia/textures/constant.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using ConstFloatTexture = ConstantTexture<float>;
    using ConstColorTexture = ConstantTexture<Color3f>;

    GND_REGISTER_CLASS(ConstFloatTexture, "constant_float");
    GND_REGISTER_CLASS(ConstColorTexture, "constant_color");

}