#pragma once

#include <gianduia/shapes/primitive.h>
#include <gianduia/core/bvhBuilder.h>
#include <vector>
#include <memory>

namespace gnd {

    struct SurfaceInteraction;

    class BVH {
    public:
        BVH() {}
        BVH(std::vector<std::shared_ptr<Primitive>>& primitives) : m_primitives{primitives} {}

        void build();

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const;

        size_t getSizeBytes() const { return m_nodes.size() * sizeof(BVHNode); }

    private:
        // Linear BVH
        std::vector<BVHNode> m_nodes;
        std::vector<std::shared_ptr<Primitive>> m_primitives;
    };

}