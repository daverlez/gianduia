#include <gianduia/materials/material.h>
#include <gianduia/core/factory.h>

namespace gnd {

    class IndexMaterial : public Material {
    public:
        IndexMaterial(const PropertyList& props) {
        }

        void computeScatteringFunctions(SurfaceInteraction& isect, MemoryArena& arena) const override {
            applyNormalMap(isect);
            applyMediums(isect);

            // Leaving isect.bsdf = nullptr, so shadow rays pass through it.
            isect.bsdf = nullptr;
        }

        std::string toString() const override {
            return "IndexMaterial[\n"
                   "  normal = " + (m_normalMap ? m_normalMap->toString() : "null") + "\n"
                   "]";
        }
    };

    GND_REGISTER_CLASS(IndexMaterial, "index");
}
