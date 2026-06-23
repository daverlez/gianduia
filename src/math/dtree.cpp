#include "gianduia/math/dtree.h"
#include "gianduia/math/constants.h"
#include <algorithm>
#include <numeric>

namespace gnd {


    DTree::DTree() {
        m_nodes.emplace_back();
    }

    Point2f DTree::dirToUV(const Vector3f& d) {
        float z = std::clamp(d.z(), -1.0f, 1.0f);
        float v = (z + 1.0f) * 0.5f;

        float phi = std::atan2(d.y(), d.x());
        if (phi < 0.0f) phi += 2.0f * Pi;
        float u = phi / (2.0f * Pi);

        return {std::clamp(u, 0.0f, 1.0f), std::clamp(v, 0.0f, 1.0f)};
    }

    Vector3f DTree::uvToDir(const Point2f& uv) {
        float z = 2.0f * uv.y() - 1.0f;
        float phi = 2.0f * Pi * uv.x();
        
        float sinTheta = std::sqrt(std::max(0.0f, 1.0f - z * z));
        float x = sinTheta * std::cos(phi);
        float y = sinTheta * std::sin(phi);

        return Normalize(Vector3f(x, y, z));
    }

    void DTree::addSample(const Vector3f& dir, float radiance) {
        if (m_nodes.empty() || radiance <= 0.0f) return;

        Point2f uv = dirToUV(dir);
        int nodeIdx = 0;

        Point2f minUV(0.0f, 0.0f);
        Point2f maxUV(1.0f, 1.0f);

        while (!m_nodes[nodeIdx].isLeaf()) {
            m_nodes[nodeIdx].sumEnergy += radiance;

            Point2f midUV = (minUV + maxUV) * 0.5f;

            int childOffset = 0;
            if (uv.x() > midUV.x()) { childOffset += 1; minUV.x() = midUV.x(); } else { maxUV.x() = midUV.x(); }
            if (uv.y() > midUV.y()) { childOffset += 2; minUV.y() = midUV.y(); } else { maxUV.y() = midUV.y(); }

            nodeIdx = m_nodes[nodeIdx].childIndex + childOffset;
        }

        m_nodes[nodeIdx].sumEnergy += radiance;
        m_statisticalWeight += radiance;
    }

    void DTree::refine(float splitThreshold) {
        if (m_nodes.empty() || m_statisticalWeight <= 0.0f) return;

        float threshold = m_statisticalWeight * splitThreshold;

        std::vector<DTreeNode> newNodes;
        newNodes.reserve(m_nodes.size());
        newNodes.emplace_back();
        refineRecursive(0, 0, threshold, 0, newNodes);

        m_nodes = std::move(newNodes);
    }

    void DTree::refineRecursive(int oldNodeIdx, int newNodeIdx, float threshold, int depth, std::vector<DTreeNode>& newNodes) {
        const DTreeNode& oldNode = m_nodes[oldNodeIdx];

        float updatedEstimate = 0.0f;
        if (oldNode.estimate == 0.0f) {
            updatedEstimate = oldNode.sumEnergy;
        } else {
            updatedEstimate = 0.5f * oldNode.estimate + 0.5f * oldNode.sumEnergy;
        }
        updatedEstimate += Epsilon;

        newNodes[newNodeIdx].estimate = updatedEstimate;
        newNodes[newNodeIdx].sumEnergy = oldNode.sumEnergy;

        bool shouldSplit = (oldNode.sumEnergy > threshold) && (depth < 20);
        if (shouldSplit) {
            int childIdx = static_cast<int>(newNodes.size());
            newNodes[newNodeIdx].childIndex = childIdx;

            for (int i = 0; i < 4; ++i) {
                newNodes.emplace_back();
            }

            if (oldNode.isLeaf()) {
                float initialChildEstimate = newNodes[newNodeIdx].estimate * 0.25f;
                float initialChildEnergy = oldNode.sumEnergy * 0.25f;

                for (int i = 0; i < 4; ++i) {
                    newNodes[childIdx + i].estimate = initialChildEstimate;
                    newNodes[childIdx + i].sumEnergy = initialChildEnergy;
                    splitNewNode(childIdx + i, threshold, depth + 1, newNodes);
                }
            } else {
                float totalChildEstimate = 0.0f;
                for (int i = 0; i < 4; ++i) {
                    refineRecursive(oldNode.childIndex + i, childIdx + i, threshold, depth + 1, newNodes);
                    totalChildEstimate += newNodes[childIdx + i].estimate;
                }
                newNodes[newNodeIdx].estimate = totalChildEstimate;
            }
        } else {
            newNodes[newNodeIdx].childIndex = -1;
        }
    }

    void DTree::splitNewNode(int newNodeIdx, float threshold, int depth, std::vector<DTreeNode>& newNodes) {
        if (newNodes[newNodeIdx].sumEnergy > threshold && depth < 20) {
            int childIdx = static_cast<int>(newNodes.size());
            newNodes[newNodeIdx].childIndex = childIdx;

            for (int i = 0; i < 4; ++i) {
                newNodes.emplace_back();
            }

            float initialChildEstimate = newNodes[newNodeIdx].estimate * 0.25f;
            float initialChildEnergy = newNodes[newNodeIdx].sumEnergy * 0.25f;

            for (int i = 0; i < 4; ++i) {
                newNodes[childIdx + i].estimate = initialChildEstimate;
                newNodes[childIdx + i].sumEnergy = initialChildEnergy;

                splitNewNode(childIdx + i, threshold, depth + 1, newNodes);
            }
        }
    }

    float DTree::pdf(const Vector3f& dir) const {
        if (m_nodes.empty() || m_nodes[0].estimate <= 0.0f) return Inv4Pi;

        Point2f uv = dirToUV(dir);
        int nodeIdx = 0;
        float pdfVal = 1.0f;
        float factor = 4.0f;

        Point2f minUV(0.0f, 0.0f);
        Point2f maxUV(1.0f, 1.0f);

        while (!m_nodes[nodeIdx].isLeaf()) {
            int firstChild = m_nodes[nodeIdx].childIndex;

            Point2f midUV = (minUV + maxUV) * 0.5f;
            int childOffset = 0;
            if (uv.x() > midUV.x()) { childOffset += 1; minUV.x() = midUV.x(); } else { maxUV.x() = midUV.x(); }
            if (uv.y() > midUV.y()) { childOffset += 2; minUV.y() = midUV.y(); } else { maxUV.y() = midUV.y(); }

            int selectedChild = firstChild + childOffset;
            float childEstimate = m_nodes[selectedChild].estimate;

            float sumEstimates = m_nodes[firstChild].estimate + m_nodes[firstChild+1].estimate +
                                 m_nodes[firstChild+2].estimate + m_nodes[firstChild+3].estimate;

            float prob = sumEstimates > 0.0f ? childEstimate / sumEstimates : 0.25f;
            if (prob <= 0.0f) return 0.0f;

            pdfVal *= prob * factor;
            nodeIdx = selectedChild;
        }

        return pdfVal * Inv4Pi;
    }

    Vector3f DTree::sample(const Point2f& rnd, float& outPdf) const {
        if (m_nodes.empty() || m_nodes[0].estimate <= 0.0f) {
            outPdf = Inv4Pi;
            float z = 1.0f - 2.0f * rnd.x();
            float r = std::sqrt(std::max(0.0f, 1.0f - z * z));
            float phi = 2.0f * Pi * rnd.y();
            return Vector3f(r * std::cos(phi), r * std::sin(phi), z);
        }

        int nodeIdx = 0;
        Point2f uvMin(0.0f, 0.0f);
        Point2f uvMax(1.0f, 1.0f);

        outPdf = 1.0f;
        float factor = 4.0f;
        float rX = rnd.x();
        float rY = rnd.y();

        while (!m_nodes[nodeIdx].isLeaf()) {
            int firstChild = m_nodes[nodeIdx].childIndex;

            float e[4] = {
                m_nodes[firstChild].estimate,
                m_nodes[firstChild + 1].estimate,
                m_nodes[firstChild + 2].estimate,
                m_nodes[firstChild + 3].estimate
            };

            float eY0 = e[0] + e[1];
            float eY1 = e[2] + e[3];
            float sumY = eY0 + eY1;
            float probY0 = sumY > 0.0f ? std::clamp(eY0 / sumY, 0.0f, 1.0f) : 0.5f;

            int childOffset = 0;
            float pY = 0.0f;

            if (rY < probY0) {
                rY = probY0 > 0.0f ? rY / probY0 : 0.0f;
                pY = probY0;
                uvMax.y() = (uvMin.y() + uvMax.y()) * 0.5f;
            } else {
                rY = probY0 < 1.0f ? (rY - probY0) / (1.0f - probY0) : 0.0f;
                pY = 1.0f - probY0;
                childOffset += 2;
                uvMin.y() = (uvMin.y() + uvMax.y()) * 0.5f;
            }

            float eX0 = e[childOffset];
            float eX1 = e[childOffset + 1];
            float sumX = eX0 + eX1;
            float probX0 = sumX > 0.0f ? std::clamp(eX0 / sumX, 0.0f, 1.0f) : 0.5f;

            float pX = 0.0f;
            if (rX < probX0) {
                rX = probX0 > 0.0f ? rX / probX0 : 0.0f;
                pX = probX0;
                uvMax.x() = (uvMin.x() + uvMax.x()) * 0.5f;
            } else {
                rX = probX0 < 1.0f ? (rX - probX0) / (1.0f - probX0) : 0.0f;
                pX = 1.0f - probX0;
                childOffset += 1;
                uvMin.x() = (uvMin.x() + uvMax.x()) * 0.5f;
            }

            outPdf *= (pY * pX) * factor;
            nodeIdx = firstChild + childOffset;
        }

        outPdf *= Inv4Pi;

        Point2f finalUV(
            uvMin.x() + rX * (uvMax.x() - uvMin.x()),
            uvMin.y() + rY * (uvMax.y() - uvMin.y())
        );

        return uvToDir(finalUV);
    }

    void DTree::clearAccumulators() {
        for (auto& node : m_nodes) {
            node.sumEnergy = 0.0f;
        }
        m_statisticalWeight = 0.0f;
    }

}