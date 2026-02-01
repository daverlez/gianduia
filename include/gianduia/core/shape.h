#pragma once

#include <gianduia/core/object.h>

namespace gnd {

    // Forward declaration
    class Primitive;

    /// Record containing data on hit surface point.
    struct SurfaceInteraction {
        float t;            // Distance along the ray
        Point3f p;          // Intersection point (world coordinates)
        Normal3f n;         // Surface normal (world space)
        Point2f uv;         // UV coordinates
        const Primitive* primitive;

        SurfaceInteraction() : t(0.0f), p(0.0f), uv(0.0f), primitive(nullptr) {}

        bool isValid() const { return t > 0.0f; }
    };

    /// Record indicating the status of a boundary event (i.e. ray distance, getting inside/outside the primitive).
    /// This is meant for CSG shapes, which require a listing of ray distances intersecting a shape.
    struct BoundaryEvent {
        float t;
        bool isEntry;
        const Primitive* hitPrimitive;
        bool flipNormal = false;

        bool operator<(const BoundaryEvent& other) const {
            return t < other.t;
        }
    };

    class Shape : public GndObject {
    public:
        Shape(const PropertyList& props) { }
        virtual ~Shape() { }

        virtual bool rayIntersect(const Ray& ray, SurfaceInteraction& isect) const = 0;

        /// Fills the hits vector with BoundaryEvent records. This is meant for methods requiring a listing of
        /// different ray distances for intersection testing, e.g. CSG shapes.
        virtual void getAllIntersections(const Ray& ray, std::vector<BoundaryEvent>& hits) const = 0;

        /// Fills the SurfaceInteraction record with information about the hit point. This is done by default in
        /// the rayIntersect method, and it's meant only for methods testing different ray distances (using
        /// getAllIntersections method) before actually filling the SurfaceInteraction record. When used in CSG trees,
        /// this method is meant to be called on leaf nodes (actual geometric primitives), NOT on intermediate CSG nodes.
        virtual void fillInteraction(const Ray& ray, float t, SurfaceInteraction& isect) const = 0;

        virtual Bounds3f getBounds() const = 0;

        EClassType getClassType() const override { return EShape; }

        void addChild(std::shared_ptr<GndObject> child) override { }
    };
}
