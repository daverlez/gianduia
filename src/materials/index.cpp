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
            return std::format(
                "IndexMaterial[\n"
                   "  normal = {}\n"
                   "  inside medium = \n{}\n"
                   "]",
                m_normalMap ? m_normalMap->toString() : "null",
                m_inside ? indent(m_inside->toString(), 2) : "  null");
        }
    };

    GND_REGISTER_CLASS(IndexMaterial, "index");
}
