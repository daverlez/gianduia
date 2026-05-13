#include <gianduia/core/factory.h>
#include <gianduia/textures/texture.h>
#include <gianduia/materials/material.h>
#include <gianduia/materials/reflection.h>

namespace gnd {

    class Matte : public Material {
    public:
        Matte(const PropertyList& props) : Material(props) {
            if (props.hasColor("albedo")) {
                PropertyList p;
                p.setColor("value", props.getColor("albedo"));
                m_albedo = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("constant_color", p)));
            }
        }

        void addChild(std::shared_ptr<GndObject> child) override {
            if (child->getClassType() != ETexture)
                throw std::runtime_error("Matte: cannot add specified child!");

            if (child->getName() == "albedo") {
                if (m_albedo)
                    throw std::runtime_error("Matte: there's already an albedo defined!");
                m_albedo = std::static_pointer_cast<Texture<Color3f>>(child);
            } else
                Material::addChild(child);
        }

        void activate() override {
            if (!m_albedo) {
                // Default: missing texture checkerboard pattern
                PropertyList p;
                p.setColor("value1", Color3f(0.0f));
                p.setColor("value2", Color3f(1.0f, 0.0f, 1.0f));
                p.setVector2("scale", Vector2f(0.1f, 0.1f));
                m_albedo = std::static_pointer_cast<Texture<Color3f>>(
                            std::shared_ptr<GndObject>(
                                GndFactory::getInstance()->createInstance("checkerboard_color", p)));
                m_albedo->activate();
            }
        }

        void computeScatteringFunctions(SurfaceInteraction& isect, MemoryArena& arena) const override {
            applyNormalOrBump(isect);

            isect.bsdf = arena.create<BSDF>(isect);

            Color3f r = m_albedo->evaluate(isect);
            isect.bsdf->add(arena.create<LambertianReflection>(r));
        }

        Color3f getAlbedo(const SurfaceInteraction& isect) const override {
            return m_albedo->evaluate(isect);
        }

        std::string toString() const override {
            return std::format(
                "Matte[\n"
                        "  albedo =\n{}\n"
                        "  normal =\n{}\n"
                        "]",
                        indent(m_albedo->toString(), 2),
                        m_normalMap ? indent(m_normalMap->toString(), 2) : "  null");
        }

    private:
        std::shared_ptr<Texture<Color3f>> m_albedo;
    };

    GND_REGISTER_CLASS(Matte, "matte");

}
