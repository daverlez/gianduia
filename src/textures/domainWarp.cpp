#include <gianduia/textures/domainWarp.h>
#include <gianduia/core/factory.h>

namespace gnd {

    using FloatDomainWarp = DomainWarpTexture<float>;
    using ColorDomainWarp = DomainWarpTexture<Color3f>;

    GND_REGISTER_CLASS(FloatDomainWarp, "domainWarp_float");
    GND_REGISTER_CLASS(ColorDomainWarp, "domainWarp_color");

}