#pragma once

#include <gianduia/shapes/shape.h>
#include <gianduia/core/bvhBuilder.h>
#include <vector>
#include <string>

namespace gnd {

    struct CurveSegment {
        Point3f p[4];
        float w[2];

        Point3f evaluateBezier(float u) const;
        Vector3f evaluateDerivative(float u) const;
        Bounds3f getBounds() const;
    };

    class CurveArray : public Shape {
    public:
        CurveArray(const PropertyList& props);
        virtual ~CurveArray() = default;

        virtual void activate() override;

        virtual bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const override;
        virtual void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override;
        virtual Bounds3f getBounds() const override;

        virtual float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& info) const override { return 0.0f; }
        virtual float pdfSurface(const Point3f& ref, const SurfaceInteraction& info) const override { return 0.0f; }
        virtual std::string toString() const override { return "CurveArray[]"; }

    private:
        void loadBinary(const std::string& filename);

        bool intersectSegment(const Ray& ray, int segmentIndex, float& tMax, SurfaceInteraction& isect) const;
        bool recursiveIntersect(const Ray& ray, const Point3f cp[4], const Transform& rayToObj, const float widths[2],
                                float u0, float u1, int depth, int maxDepth, float& tMax, SurfaceInteraction& isect) const;
        static void subdivideBezier(const Point3f cp[4], Point3f cp0[4], Point3f cp1[4]);

    private:
        std::vector<CurveSegment> m_segments;

        std::vector<BVHNode> m_nodes;

        struct BVHNode4 {
            float minX[4], minY[4], minZ[4];
            float maxX[4], maxY[4], maxZ[4];

            // 0 = Empty node, 1 = Internal node, 2 = Leaf node
            uint8_t childType[4];
            uint32_t offset[4];
            uint32_t packCount[4];
        };
        std::vector<BVHNode4> m_nodes4;

        std::vector<int> m_orderedIndices;
        Bounds3f m_bounds;
    };

}