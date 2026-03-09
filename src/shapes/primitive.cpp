#include <gianduia/shapes/primitive.h>
#include <gianduia/core/factory.h>

namespace gnd {

    Primitive::Primitive(const PropertyList& props) {
        m_objectToWorld = props.getTransform("toWorld", Transform());
    }

    void Primitive::addChild(std::shared_ptr<GndObject> child) {
        if (child->getClassType() == EShape) {
            if (m_shape)
                throw std::runtime_error("Primitive: shape already registered!");

            m_shape = std::static_pointer_cast<Shape>(child);
        } else if (child->getClassType() == EMaterial) {
            if (m_material)
                throw std::runtime_error("Primitive: material already registered!");

            m_material = std::static_pointer_cast<Material>(child);
        } else if (child->getClassType() == EEmitter) {
            if (m_emitter)
                throw std::runtime_error("Primitive: emitter already registered!");

            m_emitter = std::static_pointer_cast<Emitter>(child);
            m_emitter->setPrimitive(this);
        } else {
            throw std::runtime_error("Primitive: invalid child class! Expected Shape.");
        }
    }

    void Primitive::activate() {
        if (!m_shape)
            throw std::runtime_error("Primitive: defined without a Shape!");
        if (!m_material) {
            // Fallback: missing texture matte
            PropertyList p;
            m_material = std::static_pointer_cast<Material>(
                        std::shared_ptr<GndObject>(
                            GndFactory::getInstance()->createInstance("matte", p)));
            m_material->activate();
        }
    }

    bool Primitive::rayIntersect(const Ray& rWorld, SurfaceInteraction& isect, bool predicate) const {
        Transform worldToObject = m_objectToWorld.inverse();
        Ray rLocal = worldToObject(rWorld);

        if (!m_shape->rayIntersect(rLocal, isect, predicate)) {
            return false;
        }

        isect.primitive = this;

        return true;
    }

    void Primitive::fillInteraction(const Ray& rWorld, SurfaceInteraction& isect) const {
        Transform worldToObject = m_objectToWorld.inverse();
        Ray rLocal = worldToObject(rWorld);

        m_shape->fillInteraction(rLocal, isect);

        isect.p = m_objectToWorld(isect.p);
        isect.n = Normalize(m_objectToWorld(isect.n));
        isect.dpdu = Normalize(m_objectToWorld(isect.dpdu));
    }

    Bounds3f Primitive::getWorldBounds() const {
        return m_objectToWorld(m_shape->getBounds());
    }

    std::shared_ptr<Shape> Primitive::getShape() const {
        return m_shape;
    }

    const Transform& Primitive::getToWorld() const {
        return m_objectToWorld;
    }

    std::shared_ptr<Material> Primitive::getMaterial() const {
        return m_material;
    }

    std::shared_ptr<Emitter> Primitive::getEmitter() const {
        return m_emitter;
    }

    EClassType Primitive::getClassType() const {
        return EPrimitive;
    }

    std::string Primitive::toString() const {
        return std::format(
            "Primitive[\n"
            "  position = {},\n"
            "  shape = \n{}\n"
            "  material = \n{}\n"
            "  emitter = \n{}\n"
            "]",
            m_objectToWorld.getPosition().toString(),
            indent(m_shape->toString(), 2),
            indent(m_material->toString(), 2),
            indent(m_emitter ? m_emitter->toString() : "null", 2)
        );
    }

    GND_REGISTER_CLASS(Primitive, "primitive");
}