#pragma once

#include <concepts>
#include <vector>
#include <algorithm>
#include <cstdint>

namespace gnd {
    template<typename T>
    concept SpatialPoint = requires(T a)
    {
        { a.p.x() } -> std::convertible_to<float>;
        { a.p.y() } -> std::convertible_to<float>;
        { a.p.z() } -> std::convertible_to<float>;
    };

    template<SpatialPoint T>
    class KdTree {
    public:
        static constexpr uint32_t InvalidNode = 0x1FFFFFFF;

        struct Node {
            T data;
            // 31-30:   axis
            // 39:      child/internal node flag
            // 28-0:    right child index
            uint32_t rightChildAndFlags;

            Node(const T& d) : data(d), rightChildAndFlags(InvalidNode) {}

            void setRightChild(uint32_t index) {
                rightChildAndFlags = (rightChildAndFlags & 0xE0000000) | (index & 0x1FFFFFFF);
            }

            uint32_t getRightChild() const {
                return rightChildAndFlags & 0x1FFFFFFF;
            }

            void setAxis(int axis) {
                rightChildAndFlags = (rightChildAndFlags & 0x3FFFFFFF) | ((uint32_t)axis << 30);
            }

            int getAxis() const {
                return (rightChildAndFlags >> 30) & 0x03;
            }

            void setLeaf(bool isLeaf) {
                if (isLeaf) rightChildAndFlags |= 0x20000000;
                else        rightChildAndFlags &= ~0x20000000;
            }

            bool isLeaf() const {
                return (rightChildAndFlags & 0x20000000) != 0;
            }
        };

        KdTree() = default;

        void build(std::vector<T> items) {
            m_nodes.clear();
            if (items.empty()) return;

            m_nodes.reserve(items.size());
            buildRecursive(items, 0, items.size(), 0);
        }

        const std::vector<Node> &getNodes() const { return m_nodes; }
        bool isEmpty() const { return m_nodes.empty(); }

        template<typename Visitor>
        void searchRadius(const T &queryPoint, float radius, Visitor visitor) const {
            if (m_nodes.empty()) return;
            searchRadiusRecursive(0, queryPoint, radius * radius, visitor);
        }

    private:
        std::vector<Node> m_nodes;

        static float getAxis(const T &item, int axis) {
            if (axis == 0) return item.p.x();
            if (axis == 1) return item.p.y();
            return item.p.z();
        }

        uint32_t buildRecursive(std::vector<T>& items, size_t start, size_t end, int depth) {
            if (start >= end) return InvalidNode;

            int axis = depth % 3;
            size_t mid = start + (end - start) / 2;

            std::nth_element(
                items.begin() + start,
                items.begin() + mid,
                items.begin() + end,
                [axis](const T& a, const T& b) {
                    return getAxis(a, axis) < getAxis(b, axis);
                }
            );

            uint32_t nodeIndex = m_nodes.size();
            m_nodes.emplace_back(Node(items[mid]));

            bool isLeaf = (end - start == 1);
            m_nodes[nodeIndex].setAxis(axis);
            m_nodes[nodeIndex].setLeaf(isLeaf);

            if (!isLeaf) {
                buildRecursive(items, start, mid, depth + 1);

                uint32_t rightChild = buildRecursive(items, mid + 1, end, depth + 1);
                m_nodes[nodeIndex].setRightChild(rightChild);
            }

            return nodeIndex;
        }

        template <typename Visitor>
        void searchRadiusRecursive(uint32_t nodeIdx, const T& query, float radius2, Visitor& visitor) const {
            if (nodeIdx == InvalidNode) return;

            const Node& node = m_nodes[nodeIdx];

            float dx = query.p.x() - node.data.p.x();
            float dy = query.p.y() - node.data.p.y();
            float dz = query.p.z() - node.data.p.z();
            float dist2 = dx*dx + dy*dy + dz*dz;

            if (dist2 <= radius2) visitor(node.data, dist2);
            if (node.isLeaf()) return;

            float axisDist = getAxis(query, node.getAxis()) - getAxis(node.data, node.getAxis());

            uint32_t firstChild = nodeIdx + 1;
            uint32_t secondChild = node.getRightChild();

            if (axisDist > 0.0f)
                std::swap(firstChild, secondChild);

            if (firstChild != InvalidNode)
                searchRadiusRecursive(firstChild, query, radius2, visitor);

            if (secondChild != InvalidNode && (axisDist * axisDist) <= radius2)
                searchRadiusRecursive(secondChild, query, radius2, visitor);
        }
    };
}
