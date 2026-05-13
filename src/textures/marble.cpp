#include <gianduia/textures/marble.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatMarble = MarbleTexture<float>;
    using ColorMarble = MarbleTexture<Color3f>;

    GND_REGISTER_CLASS(FloatMarble, "marble_float");
    GND_REGISTER_CLASS(ColorMarble, "marble_color");

}