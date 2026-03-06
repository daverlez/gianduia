#include <gianduia/materials/material.h>

#include "gianduia/core/factory.h"
#include "gianduia/materials/reflection.h"
#include "gianduia/textures/texture.h"

namespace gnd {

    class Conductor : public Material {
    public:
        Conductor(const PropertyList& props) {
            if (props.hasColor("R")) {
                PropertyList p;
                p.setColor("value", props.getColor("R"));
                m_R = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
            if (props.hasColor("eta")) {
                PropertyList p;
                p.setColor("value", props.getColor("eta"));
                m_eta = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
            if (props.hasColor("k")) {
                PropertyList p;
                p.setColor("value", props.getColor("k"));
                m_k = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
            if (props.hasFloat("roughness")) {
                PropertyList p;
                p.setFloat("value", props.getFloat("roughness"));
                m_roughness = std::static_pointer_cast<Texture<float>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_float", p)));
            }
        }

        void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() != ETexture)
                throw std::runtime_error("Conductor: cannot add specified child!");

            if (child->getName() == "R") {
                if (m_R)
                    throw std::runtime_error("Conductor: there's already a reflection texture defined!");
                m_R = std::static_pointer_cast<Texture<Color3f>>(child);
            }

            if (child->getName() == "eta") {
                if (m_eta)
                    throw std::runtime_error("Conductor: there's already an eta texture defined!");
                m_eta = std::static_pointer_cast<Texture<Color3f>>(child);
            }

            if (child->getName() == "k") {
                if (m_k)
                    throw std::runtime_error("Conductor: there's already a k texture defined!");
                m_k = std::static_pointer_cast<Texture<Color3f>>(child);
            }

            if (child->getName() == "roughness") {
                if (m_roughness)
                    throw std::runtime_error("Conductor: there's already a roughness texture defined!");
                m_roughness = std::static_pointer_cast<Texture<float>>(child);
            }
        }

        void activate() override {
            if (!m_R) {
                PropertyList p;
                p.setColor("value", Color3f(1.0f));
                m_R = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
                m_R->activate();
            }
            if (!m_eta) {
                PropertyList p;
                p.setColor("value", Color3f(1.0f));
                m_eta = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
                m_eta->activate();
            }
            if (!m_k) {
                PropertyList p;
                p.setColor("value", Color3f(1.0f));
                m_k = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
                m_k->activate();
            }
            if (!m_roughness) {
                PropertyList p;
                p.setFloat("value", 0.0f);
                m_roughness = std::static_pointer_cast<Texture<float>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_float", p)));
                m_roughness->activate();
            }
        }

        void computeScatteringFunctions(SurfaceInteraction &isect, MemoryArena &arena) const override {
            Color3f r = m_R->evaluate(isect);
            Color3f eta = m_eta->evaluate(isect);
            Color3f k = m_k->evaluate(isect);
            float roughness = m_roughness->evaluate(isect);

            isect.bsdf = arena.create<BSDF>(isect);

            Fresnel* fresnel = arena.create<FresnelConductor>(Color3f(1.0f), eta, k);

            if (roughness == 0.0f) {
                isect.bsdf->add(arena.create<SpecularReflection>(r, fresnel));
            } else {
                float alpha = TrowbridgeReitzDistribution::roughnessToAlpha(roughness);
                MicrofacetDistribution* distribution = arena.create<TrowbridgeReitzDistribution>(alpha);
                isect.bsdf->add(arena.create<MicrofacetReflection>(r, distribution, fresnel));
            }
        }

        std::string toString() const override {
            return std::format(
                "Conductor[\n"
                        "  reflectance =\n{}\n"
                        "  eta =\n{}\n"
                        "  k =\n{}\n"
                        "  roughness = \n{}\n"
                        "]",
                        indent(m_R->toString(), 2),
                        indent(m_eta->toString(), 2),
                        indent(m_k->toString(), 2),
                        indent(m_roughness->toString(), 2));
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_R;
        std::shared_ptr<Texture<Color3f>> m_eta;
        std::shared_ptr<Texture<Color3f>> m_k;
        std::shared_ptr<Texture<float>> m_roughness;
    };

    GND_REGISTER_CLASS(Conductor, "conductor");

}