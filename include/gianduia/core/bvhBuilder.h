#pragma once

#include <gianduia/math/bounds.h>
#include <vector>

namespace gnd {

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

    struct BVHBuildInfo {
        Bounds3f bounds;
        Point3f centroid;
        int originalIndex;
    };

    struct BVHBuildResult {
        std::vector<BVHNode> nodes;
        std::vector<int> orderedIndices;
    };

    class BVHBuilder {
    public:
        static BVHBuildResult build(std::vector<BVHBuildInfo>& buildData);
    };
}