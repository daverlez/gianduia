#pragma once

#include "gianduia/math/geometry.h"
#include <vector>
#include <cstdint>
#include <cmath>

namespace gnd {

    struct DTreeNode {
        int childIndex = -1;

        float estimate = 0.0f;

        float sumEnergy = 0.0f;

        bool isLeaf() const { return childIndex == -1; }
    };

    class DTree {
    public:
        DTree();

        // Training phase methods

        /// Adds radiance gathered from a specific direction.
        void addSample(const Vector3f& dir, float radiance);

        /// Refinements phase to split direction accumulating too much energy.
        void refine(float splitThreshold = 0.01f);
        
        void clearAccumulators();

        // Rendering phase

        /// Samples a direction from the D-Tree, taking two uniform samples and returning the PDF.
        Vector3f sample(const Point2f& rnd, float& outPdf) const;

        /// Returns the PDF of sampling a given direction.
        float pdf(const Vector3f& dir) const;

        // Utilities

        /// Returns the statistical weight, i.e. the sum of energy seen from the D-Tree.
        float getStatisticalWeight() const { return m_statisticalWeight; }

    private:
        std::vector<DTreeNode> m_nodes;
        float m_statisticalWeight = 0.0f;

        static Point2f dirToUV(const Vector3f& dir);
        static Vector3f uvToDir(const Point2f& uv);

        void refineRecursive(int oldNodeIdx, int newNodeIdx, float threshold, int depth, std::vector<DTreeNode>& newNodes);
        void splitNewNode(int newNodeIdx, float threshold, int depth, std::vector<DTreeNode>& newNodes);
    };

}