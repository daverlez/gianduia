#pragma once

#include <gianduia/core/object.h>

namespace gnd {

    // Forward declaration
    class Primitive;

    /// Record containing data on hit surface point.
    struct SurfaceInteraction {
        float t;                        // Distance along the ray
        Point3f p;                      // Intersection point (world coordinates)
        Normal3f n;                     // Surface normal (world space)
        Point2f uv;                     // UV coordinates
        const Primitive* primitive;
        uint32_t primIndex = 0;         // Triangle indexing in meshes

        SurfaceInteraction() : t(0.0f), p(0.0f), uv(0.0f), primitive(nullptr) {}

        bool isValid() const { return t > 0.0f; }
    };

    class Shape : public GndObject {
    public:
        Shape(const PropertyList& props) { }
        virtual ~Shape() { }

        virtual bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate = false) const = 0;

        virtual void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const = 0;

        virtual Bounds3f getBounds() const = 0;

        EClassType getClassType() const override { return EShape; }

        void addChild(std::shared_ptr<GndObject> child) override { }
    };
}
