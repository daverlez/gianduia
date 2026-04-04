#pragma once
#include <gianduia/shapes/shape.h>
#include <gianduia/math/geometry.h>

namespace gnd {

    class Curve : public Shape {
    public:
        Curve(const PropertyList& props);

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate = false) const override;
        void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override;
        Bounds3f getBounds() const override;

        float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& info) const override { return 0.0f; }
        float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const override { return 0.0f; }

        std::string toString() const override { return "Curve[]"; }

    private:
        Point3f m_p[4];
        float m_width[2];

        Point3f evaluateBezier(float u) const;
        Vector3f evaluateDerivative(float u) const;
        static void subdivideBezier(const Point3f cp[4], Point3f cp0[4], Point3f cp1[4]);
        bool recursiveIntersect(const Ray& ray, const Point3f cp[4], const Transform& rayToObj,
                                float u0, float u1, int depth, int maxDepth, float& tMax, SurfaceInteraction& isect) const;
    };

}