#include "gianduia/math/stree.h"
#include <algorithm>

namespace gnd {

    STree::STree(const Bounds3f& sceneBounds) : m_bounds(sceneBounds) {
        m_nodes.emplace_back();
    }

    int STree::findLeafIndex(const Point3f& p) const {
        int currentNodeIdx = 0;
        Bounds3f currentBounds = m_bounds;

        while (!m_nodes[currentNodeIdx].isLeaf()) {
            const STreeNode& node = m_nodes[currentNodeIdx];

            float splitPos = (currentBounds.pMin[node.splitAxis] + currentBounds.pMax[node.splitAxis]) * 0.5f;

            if (p[node.splitAxis] < splitPos) {
                currentNodeIdx = node.childIndex;
                currentBounds.pMax[node.splitAxis] = splitPos;
            } else {
                currentNodeIdx = node.childIndex + 1;
                currentBounds.pMin[node.splitAxis] = splitPos;
            }
        }
        return currentNodeIdx;
    }

    float STree::estimateRadiance(const Point3f& p) const {
        if (m_nodes.empty()) return 0.0f;

        Point3f clampedP = m_bounds.clamp(p);
        int leafIdx = findLeafIndex(clampedP);
        
        return m_nodes[leafIdx].estimate;
    }

    void STree::addSample(const Point3f& p, float radiance) {
        if (m_nodes.empty()) return;

        Point3f clampedP = m_bounds.clamp(p);
        int leafIdx = findLeafIndex(clampedP);

        // Not thread safe by design. The integrator shall accumulate records per thread, and finally
        // pass them to this function at the end of the pass, in a single-threaded block.
        m_nodes[leafIdx].sumRadiance += radiance;
        m_nodes[leafIdx].count += 1;
    }

    void STree::refine(int maxSamplesPerLeaf) {
        if (m_nodes.empty()) return;
        refineRecursive(0, m_bounds, maxSamplesPerLeaf);

        for (auto& dtree : m_dtrees) {
            dtree.refine(0.01f);
        }
    }

    void STree::refineRecursive(int nodeIdx, const Bounds3f& nodeBounds, int maxSamplesPerLeaf) {
        STreeNode& node = m_nodes[nodeIdx];

        if (node.isLeaf()) {
            if (node.count > 0) {
                node.estimate = node.sumRadiance / node.count; 
            }

            if (node.count > maxSamplesPerLeaf) {
                Vector3f diag = nodeBounds.diagonal();
                uint8_t splitAxis = 0;
                if (diag.y() > diag.x() && diag.y() > diag.z()) splitAxis = 1;
                else if (diag.z() > diag.x() && diag.z() > diag.y()) splitAxis = 2;

                node.splitAxis = splitAxis;

                int leftChildIdx = static_cast<int>(m_nodes.size());
                m_nodes.emplace_back();
                m_nodes.emplace_back();

                m_nodes[nodeIdx].childIndex = leftChildIdx;
                m_nodes[nodeIdx].splitAxis = splitAxis;

                m_nodes[leftChildIdx].estimate = m_nodes[nodeIdx].estimate;
                m_nodes[leftChildIdx + 1].estimate = m_nodes[nodeIdx].estimate;

                int parentDTreeIdx = m_nodes[nodeIdx].dTreeIndex;
                if (parentDTreeIdx >= 0) {
                    int leftDTreeIdx;
                    if (!m_freeDTreeIndices.empty()) {
                        leftDTreeIdx = m_freeDTreeIndices.back();
                        m_freeDTreeIndices.pop_back();
                        if (leftDTreeIdx >= m_dtrees.size()) m_dtrees.resize(leftDTreeIdx + 1);
                    } else {
                        leftDTreeIdx = static_cast<int>(m_dtrees.size());
                        m_dtrees.emplace_back();
                    }

                    int rightDTreeIdx;
                    if (!m_freeDTreeIndices.empty()) {
                        rightDTreeIdx = m_freeDTreeIndices.back();
                        m_freeDTreeIndices.pop_back();
                        if (rightDTreeIdx >= m_dtrees.size()) m_dtrees.resize(rightDTreeIdx + 1);
                    } else {
                        rightDTreeIdx = static_cast<int>(m_dtrees.size());
                        m_dtrees.emplace_back();
                    }

                    m_dtrees[leftDTreeIdx] = m_dtrees[parentDTreeIdx];
                    m_dtrees[rightDTreeIdx] = m_dtrees[parentDTreeIdx];

                    m_nodes[leftChildIdx].dTreeIndex = leftDTreeIdx;
                    m_nodes[leftChildIdx + 1].dTreeIndex = rightDTreeIdx;

                    m_freeDTreeIndices.push_back(parentDTreeIdx);
                    m_nodes[nodeIdx].dTreeIndex = -1;
                }
            }
        } else {
            Bounds3f leftBounds = nodeBounds;
            Bounds3f rightBounds = nodeBounds;
            
            float splitPos = (nodeBounds.pMin[node.splitAxis] + nodeBounds.pMax[node.splitAxis]) * 0.5f;
            
            leftBounds.pMax[node.splitAxis] = splitPos;
            rightBounds.pMin[node.splitAxis] = splitPos;

            int leftChildIdx = node.childIndex;
            
            refineRecursive(leftChildIdx, leftBounds, maxSamplesPerLeaf);
            refineRecursive(leftChildIdx + 1, rightBounds, maxSamplesPerLeaf);

            STreeNode& updatedNode = m_nodes[nodeIdx];
            updatedNode.estimate = 0.5f * (m_nodes[leftChildIdx].estimate + m_nodes[leftChildIdx + 1].estimate);
        }
    }

    void STree::clearAccumulators() {
        for (auto& node : m_nodes) {
            node.sumRadiance = 0.0f;
            node.count = 0;
        }
        for (auto& dtree : m_dtrees) {
            dtree.clearAccumulators();
        }
    }

    const DTree* STree::getDTree(const Point3f& p) const {
        if (m_nodes.empty()) return nullptr;
        int leafIdx = findLeafIndex(m_bounds.clamp(p));
        int dTreeIdx = m_nodes[leafIdx].dTreeIndex;

        if (dTreeIdx >= 0) return &m_dtrees[dTreeIdx];
        return nullptr;
    }

    DTree* STree::getDTree(const Point3f& p) {
        if (m_nodes.empty()) return nullptr;
        int leafIdx = findLeafIndex(m_bounds.clamp(p));
        int dTreeIdx = m_nodes[leafIdx].dTreeIndex;

        if (dTreeIdx < 0) {
            if (!m_freeDTreeIndices.empty()) {
                dTreeIdx = m_freeDTreeIndices.back();
                m_freeDTreeIndices.pop_back();
                m_dtrees[dTreeIdx] = DTree();
            } else {
                dTreeIdx = static_cast<int>(m_dtrees.size());
                m_dtrees.emplace_back();
            }
            m_nodes[leafIdx].dTreeIndex = dTreeIdx;
        }
        return &m_dtrees[dTreeIdx];
    }

}