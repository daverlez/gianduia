#pragma once

#include <gianduia/core/object.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/materials/material.h>
#include <gianduia/core/emitter.h>
#include <gianduia/math/animated.h>
#include <memory>

namespace gnd {

    class Primitive : public GndObject {
    public:
        Primitive(const PropertyList& props);
        virtual ~Primitive() {}

        void addChild(std::shared_ptr<GndObject> child) override;
        void activate() override;

        bool rayIntersect(const Ray& rWorld, SurfaceInteraction& isect, bool predicate) const;
        void fillInteraction(const Ray& rWorld, SurfaceInteraction& isect) const;

        Bounds3f getWorldBounds() const;
        std::shared_ptr<Shape> getShape() const;
        Transform getToWorld(float time) const;
        std::shared_ptr<Material> getMaterial() const;
        std::shared_ptr<Emitter> getEmitter() const;

        EClassType getClassType() const override;
        std::string toString() const override;

    private:
        std::shared_ptr<Shape> m_shape;
        AnimatedTransform m_objectToWorld;
        std::shared_ptr<Material> m_material;
        std::shared_ptr<Emitter> m_emitter;
    };

}
