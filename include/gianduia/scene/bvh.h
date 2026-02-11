#pragma once

#include <gianduia/math/bounds.h>
#include <gianduia/core/primitive.h>
#include <vector>
#include <memory>

namespace gnd {

    struct SurfaceInteraction;

    struct alignas(32) BVHNode {
        Bounds3f bounds;                    // BBox of the node

        union {
            uint32_t primitivesOffset;      // Leaf node: index in sorted primitives array
            uint32_t rightChildOffset;      // Internal node: index of right child (left is always this + 1)
        };

        uint16_t nPrimitives;               // Primitives count; 0 for internal nodes
        uint8_t axis;                       // Split axis
        uint8_t pad;                        // Explicit 1 byte padding for 32 bytes alignment
    };

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