#pragma once

#include <gianduia/core/object.h>

namespace gnd {

    struct SurfaceInteraction {
        float t;            // Distance along the ray
        Point3f p;          // Intersection point (world coordinates)
        Normal3f n;         // Surface normal (world space)
        Point3f uv;         // UV coordinates
        // const Shape* shape;
        // const Material* mat;

        SurfaceInteraction() : t(0.0f), p(0.0f), n(), uv(0.0f) {}

        bool isValid() const { return t > 0.0f; }
    };

    class Shape : public GndObject {
    public:
        Shape(const PropertyList& props) { }
        virtual ~Shape() { }

        virtual bool rayIntersect(const Ray& ray, SurfaceInteraction& isect) const = 0;

        virtual Bounds3f getBounds() const = 0;

        EClassType getClassType() const override { return EShape; }

        void addChild(std::shared_ptr<GndObject> child) override { }
    };
}