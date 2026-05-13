#include <gianduia/materials/material.h>
#include "gianduia/core/factory.h"
#include "gianduia/materials/reflection.h"
#include "gianduia/textures/texture.h"

namespace gnd {

    class Plastic : public Material {
    public:
        Plastic(const PropertyList& props) : Material(props) {
            if (props.hasColor("Kd")) {
                PropertyList p;
                p.setColor("value", props.getColor("Kd"));
                m_Kd = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
            if (props.hasColor("Ks")) {
                PropertyList p;
                p.setColor("value", props.getColor("Ks"));
                m_Ks = std::static_pointer_cast<Texture<Color3f>>(
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
            if (props.hasFloat("eta")) {
                PropertyList p;
                p.setFloat("value", props.getFloat("eta"));
                m_eta = std::static_pointer_cast<Texture<float>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_float", p)));
            }
        }

        void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() != ETexture) return;

            if (child->getName() == "Kd") m_Kd = std::static_pointer_cast<Texture<Color3f>>(child);
            else if (child->getName() == "Ks") m_Ks = std::static_pointer_cast<Texture<Color3f>>(child);
            else if (child->getName() == "roughness") m_roughness = std::static_pointer_cast<Texture<float>>(child);
            else if (child->getName() == "eta") m_eta = std::static_pointer_cast<Texture<float>>(child);
            else Material::addChild(child);
        }

        void activate() override {
            if (!m_Kd) {
                PropertyList p; p.setColor("value", Color3f(0.25f));
                m_Kd = std::static_pointer_cast<Texture<Color3f>>(std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_color", p)));
                m_Kd->activate();
            }
            if (!m_Ks) {
                PropertyList p; p.setColor("value", Color3f(0.25f));
                m_Ks = std::static_pointer_cast<Texture<Color3f>>(std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_color", p)));
                m_Ks->activate();
            }
            if (!m_roughness) {
                PropertyList p; p.setFloat("value", 0.1f);
                m_roughness = std::static_pointer_cast<Texture<float>>(std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_float", p)));
                m_roughness->activate();
            }
            if (!m_eta) {
                PropertyList p; p.setFloat("value", 1.5f);
                m_eta = std::static_pointer_cast<Texture<float>>(std::shared_ptr<GndObject>(GndFactory::getInstance()->createInstance("constant_float", p)));
                m_eta->activate();
            }
        }

        void computeScatteringFunctions(SurfaceInteraction &isect, MemoryArena &arena) const override {
            applyNormalOrBump(isect);

            Color3f kd = m_Kd->evaluate(isect);
            Color3f ks = m_Ks->evaluate(isect);
            float roughness = m_roughness->evaluate(isect);
            float eta = m_eta->evaluate(isect);

            isect.bsdf = arena.create<BSDF>(isect);

            if (roughness == 0.0f) {
                isect.bsdf->add(arena.create<SmoothPlasticBxDF>(kd, ks, eta));
            } else {
                float alpha = TrowbridgeReitzDistribution::roughnessToAlpha(roughness);
                MicrofacetDistribution* distribution = arena.create<TrowbridgeReitzDistribution>(alpha);
                isect.bsdf->add(arena.create<RoughPlasticBxDF>(kd, ks, eta, distribution));
            }
        }

        std::string toString() const override {
            return std::format("Plastic[\n"
                               "  Kd = \n{}\n "
                               "  Ks = \n{}\n "
                               "  roughness = \n{}\n "
                               "  eta = \n{}\n"
                               "  normal = \n{}\n"
                               "]",
                               indent(m_Kd->toString(), 2),
                               indent(m_Ks->toString(), 2),
                               indent(m_roughness->toString(), 2),
                               indent(m_eta->toString(), 2),
                               m_normalMap ? indent(m_normalMap->toString(), 2) : "  null");
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_Kd;
        std::shared_ptr<Texture<Color3f>> m_Ks;
        std::shared_ptr<Texture<float>> m_roughness;
        std::shared_ptr<Texture<float>> m_eta;
    };

    GND_REGISTER_CLASS(Plastic, "plastic");
}