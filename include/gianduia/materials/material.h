#pragma once
#include <gianduia/core/object.h>
#include <gianduia/core/arena.h>
#include <gianduia/materials/medium.h>
#include <gianduia/textures/texture.h>

namespace gnd {

    struct SurfaceInteraction;

    class Material : public GndObject {
    public:
        Material(const PropertyList& props) {
            m_bumpScale = props.getFloat("bump_scale", 1.0f);
        }
        virtual ~Material() = default;

        /// Basing on surface interaction infos, populates the fields on scattering functions.
        /// @param isect input record containing surface interactions information
        /// @param arena memory arena allocator to instantiate scattering functions
        virtual void computeScatteringFunctions(SurfaceInteraction& isect,
                                                MemoryArena& arena) const = 0;

        virtual Color3f getAlbedo(const SurfaceInteraction& isect) const {
            return Color3f(0.0f);
        }

        virtual float getRoughness(const SurfaceInteraction& isect) const {
            return 0.0f;
        }

        virtual float getMetallic(const SurfaceInteraction& isect) const {
            return 0.0f;
        }

        virtual void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() == ETexture && child->getName() == "normal") {
                if (m_normalMap)
                    throw std::runtime_error("Material: normal map already defined!");
                m_normalMap = std::static_pointer_cast<Texture<Normal3f>>(child);
            }
            else if (child->getClassType() == ETexture && child->getName() == "bump") {
                if (m_bumpMap) throw std::runtime_error("Material: bump map already defined!");
                m_bumpMap = std::static_pointer_cast<Texture<float>>(child);
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
        std::shared_ptr<Texture<float>> m_bumpMap;
        float m_bumpScale;

        std::shared_ptr<Medium> m_inside;
        std::shared_ptr<Medium> m_outside;

        void applyNormalOrBump(SurfaceInteraction& isect) const {
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
            else if (m_bumpMap) {
                Vector3f dpdu = isect.dpdu;
                Vector3f dpdv = Normalize(Cross(isect.n, dpdu));

                float du = 0.001f;
                float dv = 0.001f;

                float hBase = m_bumpMap->evaluate(isect);

                SurfaceInteraction isectDu = isect;
                isectDu.p = isect.p + dpdu * du;
                isectDu.uv.x() += du;
                float hDu = m_bumpMap->evaluate(isectDu);

                SurfaceInteraction isectDv = isect;
                isectDv.p = isect.p + dpdv * dv;
                isectDv.uv.y() += dv;
                float hDv = m_bumpMap->evaluate(isectDv);

                float dhdu = (hDu - hBase) / du;
                float dhdv = (hDv - hBase) / dv;
                dhdu *= m_bumpScale;
                dhdv *= m_bumpScale;

                Vector3f newDpdu = dpdu + Vector3f(isect.n) * dhdu;
                Vector3f newDpdv = dpdv + Vector3f(isect.n) * dhdv;

                isect.n = Normal3f(Normalize(Cross(newDpdu, newDpdv)));

                Vector3f dpduProj = newDpdu - Vector3f(isect.n) * Dot(isect.n, newDpdu);
                isect.dpdu = Normalize(dpduProj);
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
