#pragma once

#include "gianduia/math/bounds.h"
#include "gianduia/math/color.h"
#include <vector>
#include <cstdint>

namespace gnd {

    struct STreeNode {
        int childIndex = -1;
        uint8_t splitAxis = 0;

        float estimate = 0.0f;     

        float sumRadiance = 0.0f;
        int count = 0;

        bool isLeaf() const { return childIndex == -1; }
    };

    class STree {
    public:
        STree(const Bounds3f& sceneBounds);

        /// Returns the estimate radiance at point p. Meant to be used in Integrators during Russian Roulette.
        float estimateRadiance(const Point3f& p) const;

        /// Accumulates pat energy in a node. NOT thread safe.
        void addSample(const Point3f& p, float radiance);

        /// Refinement phase to be called between training passes.
        void refine(int maxSamplesPerLeaf = 4000);

        // Utilities

        size_t getNodeCount() const { return m_nodes.size(); }
        void clearAccumulators();

    private:
        Bounds3f m_bounds;
        std::vector<STreeNode> m_nodes;

        void refineRecursive(int nodeIdx, const Bounds3f& nodeBounds, int maxSamplesPerLeaf);
        int findLeafIndex(const Point3f& p) const;
    };

}