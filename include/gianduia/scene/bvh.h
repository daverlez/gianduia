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
        std::vector<BVHNode4> m_nodes4;

        std::vector<std::shared_ptr<Primitive>> m_primitives;
    };

}