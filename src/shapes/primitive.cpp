#include <gianduia/core/primitive.h>
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

    bool Primitive::rayIntersect(const Ray& rWorld, SurfaceInteraction& isect) const {
        Transform worldToObject = m_objectToWorld.inverse();
        Ray rLocal = worldToObject(rWorld);

        if (!m_shape->rayIntersect(rLocal, isect)) {
            return false;
        }

        isect.p = m_objectToWorld(isect.p);
        isect.n = Normalize(m_objectToWorld(isect.n));

        return true;
    }

    Bounds3f Primitive::getWorldBounds() const {
        if (!m_shape) return Bounds3f();
        return m_objectToWorld(m_shape->getBounds());
    }

    EClassType Primitive::getClassType() const {
        return EPrimitive;
    }

    std::string Primitive::toString() const {
        return "Primitive[\n  shape = " +
               m_shape->toString() +
               "\n]";
    }

    GND_REGISTER_CLASS(Primitive, "primitive");
}