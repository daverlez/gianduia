#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/arena.h>

namespace gnd {

    struct SurfaceInteraction;

    class Material : public GndObject {
    public:
        virtual ~Material() = default;

        /// Basing on surface interaction infos, populates the fields on scattering functions.
        /// @param isect input record containing surface interactions information
        /// @param arena memory arena allocator to instantiate scattering functions
        virtual void computeScatteringFunctions(SurfaceInteraction& isect,
                                                MemoryArena& arena) const = 0;

        EClassType getClassType() const override { return EMaterial; }
    };

}