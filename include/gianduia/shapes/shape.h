#pragma once

#include <gianduia/core/object.h>

#include "gianduia/materials/bsdf.h"

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

        BSDF* bsdf = nullptr;           // BSDF at intersection point

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

        /// Samples a point on the surface of the shape.
        /// @param ref Point from which sampling is queried
        /// @param sample 2D uniform sample
        /// @param info [out] Information about sampled point (namely p, n and uv).
        /// @return Pdf of the sampled point with respect to area
        virtual float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& info) const = 0;

        /// Returns pdf with respect to area of the sampled point.
        /// @param info sampled point information
        virtual float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const = 0;
    };
}
