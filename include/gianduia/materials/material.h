#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/arena.h>
#include <gianduia/materials/medium.h>
#include <gianduia/textures/texture.h>

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

        virtual Color3f getAlbedo(const SurfaceInteraction& isect) const {
            return Color3f(0.0f);
        }

        virtual void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() == ETexture && child->getName() == "normal") {
                if (m_normalMap)
                    throw std::runtime_error("Material: normal map already defined!");
                m_normalMap = std::static_pointer_cast<Texture<Normal3f>>(child);
            }
            else if (child->getClassType() == EMedium && child->getName() == "inside") {
                if (m_inside)
                    throw std::runtime_error("Material: inside medium already defined!");
                m_inside = std::static_pointer_cast<Medium>(child);
            }
            else if (child->getClassType() == EMedium && child->getName() == "outside") {
                if (m_outside)
                    throw std::runtime_error("Material: outside medium already defined!");
                m_outside = std::static_pointer_cast<Medium>(child);
            }
            else {
                throw std::runtime_error("Material: cannot add specified child (" + child->getName() + ")!");
            }
        }

        EClassType getClassType() const override { return EMaterial; }

    protected:
        std::shared_ptr<Texture<Normal3f>> m_normalMap;
        std::shared_ptr<Medium> m_inside;
        std::shared_ptr<Medium> m_outside;

        void applyNormalMap(SurfaceInteraction& isect) const {
            if (m_normalMap) {
                Normal3f normal = m_normalMap->evaluate(isect);

                Vector3f dpdv = Normalize(Cross(isect.n, isect.dpdu));
                Vector3f perturbedN = isect.dpdu * normal.x() +
                                      dpdv * normal.y() +
                                      Vector3f(isect.n) * normal.z();

                isect.n = Normal3f(Normalize(perturbedN));

                Vector3f newDpdu = isect.dpdu - Vector3f(isect.n) * Dot(isect.n, isect.dpdu);
                isect.dpdu = Normalize(newDpdu);
            }
        }

        void applyMediums(SurfaceInteraction& isect) const {
            if (m_inside || m_outside) {
                isect.mediumInterface = MediumInterface(
                    m_inside ? m_inside.get() : nullptr,
                    m_outside ? m_outside.get() : nullptr
                );
            }
        }
    };

}
