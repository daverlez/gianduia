#pragma once

#include <gianduia/shapes/shape.h>
#include <gianduia/core/bvhBuilder.h>
#include <hwy/highway.h>
#include <vector>
#include <string>

namespace gnd {

    class Mesh : public Shape {
    public:
        Mesh(const PropertyList& props);
        virtual ~Mesh() {}

        virtual void activate() override;

        virtual bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const override;

        virtual void fillInteraction(const Ray& ray, SurfaceInteraction& isect) const override;

        virtual Bounds3f getBounds() const override;

        virtual float sampleSurface(const Point3f& ref, const Point2f& sample, SurfaceInteraction& isect) const override;

        virtual float pdfSurface(const Point3f& ref, const SurfaceInteraction& isect) const override;

        virtual std::string toString() const override;

    private:
        void loadOBJ(const std::string& filename);

        bool intersectTriangle(const Ray& ray, uint32_t triIndex, float& t, float& u, float& v) const;

    private:
        // SoA (4 triangles) for parallel intersection tests
        struct TrianglePack4 {
            float v0x[4], v0y[4], v0z[4];
            float v1x[4], v1y[4], v1z[4];
            float v2x[4], v2y[4], v2z[4];

            uint32_t primIndex[4];
        };
        std::vector<TrianglePack4> m_trianglePacks;

        // Mesh data
        std::string m_filename;

        std::vector<Point3f> m_positions;
        std::vector<Normal3f> m_normals;
        std::vector<Point2f> m_uvs;

        std::vector<uint32_t> m_indices;

        // BVH (BLAS)
        std::vector<BVHNode> m_nodes;
        Bounds3f m_bounds;

        // Info for uniform area sampling
        std::vector<float> m_cdf;
        float m_totalArea = 0.0f;
    };

}