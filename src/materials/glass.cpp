#include <gianduia/materials/material.h>

#include "gianduia/core/factory.h"
#include "gianduia/materials/reflection.h"
#include "gianduia/textures/texture.h"

namespace gnd {

    class Glass : public Material {
    public:
        Glass(const PropertyList& props) {
            if (props.hasColor("R")) {
                PropertyList p;
                p.setColor("value", props.getColor("R"));
                m_R = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
            if (props.hasColor("T")) {
                PropertyList p;
                p.setColor("value", props.getColor("T"));
                m_T = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
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
            if (child->getClassType() != ETexture)
                throw std::runtime_error("Glass: cannot add specified child!");

            if (child->getName() == "R") {
                if (m_R)
                    throw std::runtime_error("Glass: there's already a reflection texture defined!");
                m_R = std::static_pointer_cast<Texture<Color3f>>(child);
            }

            if (child->getName() == "T") {
                if (m_T)
                    throw std::runtime_error("Glass: there's already a transmission texture defined!");
                m_T = std::static_pointer_cast<Texture<Color3f>>(child);
            }

            if (child->getName() == "eta") {
                if (m_eta)
                    throw std::runtime_error("Glass: there's already a eta texture defined!");
                m_eta = std::static_pointer_cast<Texture<float>>(child);
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
            if (!m_T) {
                PropertyList p;
                p.setColor("value", Color3f(1.0f));
                m_T = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
                m_T->activate();
            }
            if (!m_eta) {
                PropertyList p;
                p.setFloat("value", 1.5f);
                m_eta = std::static_pointer_cast<Texture<float>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_float", p)));
                m_eta->activate();
            }
        }

        void computeScatteringFunctions(SurfaceInteraction &isect, MemoryArena &arena) const override {
            Color3f r = m_R->evaluate(isect);
            Color3f t = m_T->evaluate(isect);
            float eta = m_eta->evaluate(isect);

            isect.bsdf = arena.create<BSDF>(isect);
            isect.bsdf->add(arena.create<FresnelSpecular>(r, t, 1.0f, eta));
        }

        std::string toString() const override {
            return std::format(
                "Glass[\n"
                        "  reflectance =\n{}\n"
                        "  transmittance =\n{}\n"
                        "  eta =\n{}\n"
                        "]",
                        indent(m_R->toString(), 2),
                        indent(m_T->toString(), 2),
                        indent(m_eta->toString(), 2));
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_R;
        std::shared_ptr<Texture<Color3f>> m_T;
        std::shared_ptr<Texture<float>> m_eta;
    };

    GND_REGISTER_CLASS(Glass, "glass");

}
