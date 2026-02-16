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
        } else {
            throw std::runtime_error("Primitive: invalid child class! Expected Shape.");
        }
    }

    void Primitive::activate() {
        if (!m_shape)
            throw std::runtime_error("Primitive: defined without a Shape!");
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
    }

    Bounds3f Primitive::getWorldBounds() const {
        return m_objectToWorld(m_shape->getBounds());
    }

    EClassType Primitive::getClassType() const {
        return EPrimitive;
    }

    std::string Primitive::toString() const {
        return std::format(
            "Primitive[\n"
            "  position = {},\n"
            "  shape = \n{}\n"
            "]",
            m_objectToWorld.getPosition().toString(),
            indent(m_shape->toString(), 2)
        );
    }

    GND_REGISTER_CLASS(Primitive, "primitive");
}