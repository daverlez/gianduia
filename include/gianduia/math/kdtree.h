#pragma once

#include <concepts>
#include <vector>
#include <algorithm>
#include <cstdint>
#include <cmath>

namespace gnd {
    template<typename T>
    concept SpatialPoint = requires(T a)
    {
        { a.p.x() } -> std::convertible_to<float>;
        { a.p.y() } -> std::convertible_to<float>;
        { a.p.z() } -> std::convertible_to<float>;
        { a.getAxis() } -> std::convertible_to<int>;
        { a.setAxis(0) };
    };

    template<SpatialPoint T>
    class KdTree {
    public:
        KdTree() = default;

        void build(std::vector<T> items) {
            m_nodes.clear();
            if (items.empty()) return;

            m_nodes.resize(items.size());
            buildRecursive(items, 0, items.size(), 0, 0);
        }

        const std::vector<T>& getNodes() const { return m_nodes; }
        bool isEmpty() const { return m_nodes.empty(); }

        template<typename Visitor>
        void searchRadius(const T &queryPoint, float radius, Visitor visitor) const {
            if (m_nodes.empty()) return;
            searchRadiusRecursive(0, queryPoint, radius * radius, visitor);
        }

    private:
        std::vector<T> m_nodes;

        static float getAxis(const T &item, int axis) {
            if (axis == 0) return item.p.x();
            if (axis == 1) return item.p.y();
            return item.p.z();
        }

        size_t getLeftSubtreeSize(size_t n) const {
            if (n <= 1) return 0;
            size_t m = 1;
            while (2 * m <= n) m *= 2;
            size_t k = m - 1;
            size_t leftExceptBottom = (k - 1) / 2;
            size_t bottomNodes = n - k;
            size_t leftBottom = std::min(bottomNodes, m / 2);
            return leftExceptBottom + leftBottom;
        }

        void buildRecursive(std::vector<T>& items, size_t start, size_t end, size_t targetIdx, int depth) {
            if (start >= end) return;

            int axis = depth % 3;
            size_t n = end - start;
            size_t leftSize = getLeftSubtreeSize(n);
            size_t mid = start + leftSize;

            std::nth_element(
                items.begin() + start,
                items.begin() + mid,
                items.begin() + end,
                [axis](const T& a, const T& b) {
                    return getAxis(a, axis) < getAxis(b, axis);
                }
            );

            T node = items[mid];
            node.setAxis(axis);
            m_nodes[targetIdx] = node;

            buildRecursive(items, start, mid, 2 * targetIdx + 1, depth + 1);
            buildRecursive(items, mid + 1, end, 2 * targetIdx + 2, depth + 1);
        }

        template <typename Visitor>
        void searchRadiusRecursive(size_t nodeIdx, const T& query, float radius2, Visitor& visitor) const {
            if (nodeIdx >= m_nodes.size()) return;

            const T& node = m_nodes[nodeIdx];

            float dx = query.p.x() - node.p.x();
            float dy = query.p.y() - node.p.y();
            float dz = query.p.z() - node.p.z();
            float dist2 = dx*dx + dy*dy + dz*dz;

            if (dist2 <= radius2) visitor(node, dist2);

            int axis = node.getAxis();
            float axisDist = getAxis(query, axis) - getAxis(node, axis);

            size_t firstChild = 2 * nodeIdx + 1;
            size_t secondChild = 2 * nodeIdx + 2;

            if (axisDist > 0.0f) std::swap(firstChild, secondChild);

            if (firstChild < m_nodes.size())
                searchRadiusRecursive(firstChild, query, radius2, visitor);

            if (secondChild < m_nodes.size() && (axisDist * axisDist) <= radius2)
                searchRadiusRecursive(secondChild, query, radius2, visitor);
        }
    };
}