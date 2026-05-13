#include <gianduia/textures/cellular.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatCellular = CellularTexture<float>;
    using ColorCellular = CellularTexture<Color3f>;

    GND_REGISTER_CLASS(FloatCellular, "cellular_float");
    GND_REGISTER_CLASS(ColorCellular, "cellular_color");

}