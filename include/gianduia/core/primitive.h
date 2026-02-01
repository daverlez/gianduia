#pragma once

#include <gianduia/core/object.h>
#include <gianduia/core/shape.h>
#include <memory>

namespace gnd {

    class Primitive : public GndObject {
    public:
        Primitive(const PropertyList& props);
        virtual ~Primitive() {}

        void addChild(std::shared_ptr<GndObject> child) override;
        void activate() override;

        bool rayIntersect(const Ray& rWorld, SurfaceInteraction& isect) const;
        void getAllIntersections(const Ray& rWorld, std::vector<BoundaryEvent>& hits) const;
        void fillInteraction(const Ray& rWorld, float t, SurfaceInteraction& isect) const;

        Bounds3f getWorldBounds() const;

        EClassType getClassType() const override;
        std::string toString() const override;

    private:
        std::shared_ptr<Shape> m_shape;
        Transform m_objectToWorld;
        // std::shared_ptr<Material> m_material;
    };
}