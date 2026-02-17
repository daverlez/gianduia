#include <gianduia/textures/checkerboard.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatCheckerboard = CheckerboardTexture<float>;
    using ColorCheckerboard = CheckerboardTexture<Color3f>;

    GND_REGISTER_CLASS(FloatCheckerboard, "checkerboard_float");
    GND_REGISTER_CLASS(ColorCheckerboard, "checkerboard_color");

}