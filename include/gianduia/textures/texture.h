#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>

namespace gnd {

    template <typename T>
    class Texture : public GndObject {
    public:
        virtual ~Texture() = default;

        /// Evaluates the texture at the specified SurfaceInteraction.
        /// @param isect surface interaction record.
        /// @return Texture value at the surface interaction.
        virtual T evaluate(const SurfaceInteraction& isect) const = 0;

        EClassType getClassType() const override { return ETexture; }
    };

}
