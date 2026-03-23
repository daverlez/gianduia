#include <gianduia/materials/material.h>
#include <gianduia/core/factory.h>

namespace gnd {

    class SubsurfaceMaterial : public Material {
    public:
        SubsurfaceMaterial(const PropertyList& props) {
        }

        void computeScatteringFunctions(SurfaceInteraction& isect, MemoryArena& arena) const override {
            applyNormalMap(isect);
            applyMediums(isect);

            // Leaving isect.bsdf = nullptr, so shadow rays pass through it.
            isect.bsdf = nullptr;
        }

        std::string toString() const override {
            return "SubsurfaceMaterial[\n"
                   "  normal = " + (m_normalMap ? m_normalMap->toString() : "null") + "\n"
                   "]";
        }
    };

    GND_REGISTER_CLASS(SubsurfaceMaterial, "subsurface");
}
