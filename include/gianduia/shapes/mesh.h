#pragma once

#include <gianduia/shapes/shape.h>
#include <gianduia/core/bvhBuilder.h>
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

        virtual std::string toString() const override;

    private:
        void loadOBJ(const std::string& filename);

        bool intersectTriangle(const Ray& ray, uint32_t triIndex, float& t, float& u, float& v) const;

    private:
        std::string m_filename;

        std::vector<Point3f> m_positions;
        std::vector<Normal3f> m_normals;
        std::vector<Point2f> m_uvs;

        std::vector<uint32_t> m_indices;

        // BVH (BLAS)
        std::vector<BVHNode> m_nodes;
        Bounds3f m_bounds;
    };

}