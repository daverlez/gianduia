#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>

namespace gnd {

    template <typename T>
    concept ValidTextureValue = std::same_as<T, float> || std::same_as<T, Color3f> || std::same_as<T, Normal3f>;

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
