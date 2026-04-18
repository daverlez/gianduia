#pragma once

#include "gianduia/shapes/primitive.h"
#include "gianduia/core/bvhBuilder.h"

#include <vector>
#include <memory>

#include <hwy/highway.h>

namespace gnd {

    struct SurfaceInteraction;

    class BVH {
    public:
        BVH() {}
        BVH(std::vector<std::shared_ptr<Primitive>>& primitives) : m_primitives{primitives} {}

        void build();

        bool rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const;

        size_t getSizeBytes() const { return m_nodes4.size() * sizeof(BVHNode4); }

    private:
        // SoA (4 bboxes) for parallel bbox intersection tests
        struct BVHNode4 {
            float minX[4], minY[4], minZ[4];
            float maxX[4], maxY[4], maxZ[4];

            // 0 = Empty node, 1 = Internal node, 2 = Leaf node
            uint8_t childType[4];

            // If internal, it's the index of the child in m_nodes4, otherwise it's the index of the first primitive
            uint32_t offset[4];

            // If leaf: number of primitives to iterate
            uint32_t primCount[4];
        };
        std::vector<BVHNode4> m_nodes4;

        std::vector<std::shared_ptr<Primitive>> m_primitives;
    };

}