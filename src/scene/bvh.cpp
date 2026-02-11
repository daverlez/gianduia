#include <gianduia/scene/bvh.h>
#include <gianduia/scene/scene.h>
#include <algorithm>
#include <tbb/parallel_invoke.h>
#include <tbb/task_group.h>

namespace gnd {

    // Dual representation of BVHNode used only in construction phase
    struct BVHBuildNode {
        Bounds3f bounds;
        BVHBuildNode *children[2] = {nullptr, nullptr};
        int splitAxis = 0;
        int firstPrimOffset = 0;
        int nPrimitives = 0;

        void initLeaf(int first, int n, const Bounds3f& b) {
            firstPrimOffset = first;
            nPrimitives = n;
            bounds = b;
            children[0] = children[1] = nullptr;
        }

        void initInterior(int axis, BVHBuildNode* c0, BVHBuildNode* c1) {
            children[0] = c0;
            children[1] = c1;
            bounds = Union(c0->bounds, c1->bounds);
            splitAxis = axis;
            nPrimitives = 0;
        }
    };

    // SAH bucket
    struct BucketInfo {
        int count = 0;
        Bounds3f bounds;
    };

    // Support structure for primitives info during construction
    struct PrimitiveInfo {
        size_t originalIndex;
        Bounds3f bounds;
        Point3f centroid;

        PrimitiveInfo(size_t idx, const Bounds3f& b)
            : originalIndex(idx), bounds(b), centroid(b.pMin * 0.5f + b.pMax * 0.5f) {}
    };

    int flattenBVH(BVHBuildNode* node, int* offset, std::vector<BVHNode>& linearNodes) {
        BVHNode* linearNode = &linearNodes[*offset];
        linearNode->bounds = node->bounds;
        int currOffset = (*offset)++;

        if (node->nPrimitives > 0) {
            // Leaf node
            linearNode->primitivesOffset = node->firstPrimOffset;
            linearNode->nPrimitives = node->nPrimitives;
        } else {
            // Internal node
            linearNode->axis = node->splitAxis;
            linearNode->nPrimitives = 0;

            // Left child: subsequent in memory
            flattenBVH(node->children[0], offset, linearNodes);

            // Right child: saving offset
            linearNode->rightChildOffset = flattenBVH(node->children[1], offset, linearNodes);
        }
        return currOffset;
    }

    void BVH::build() {
        if (m_primitives.empty()) return;

        std::vector<PrimitiveInfo> primInfo;
        primInfo.reserve(m_primitives.size());
        for (size_t i = 0; i < m_primitives.size(); ++i) {
            primInfo.emplace_back(i, m_primitives[i]->getWorldBounds());
        }

        // Recursive construction
        int totalNodes = 0;
        std::vector<std::shared_ptr<Primitive>> orderedPrims;
        orderedPrims.reserve(m_primitives.size());

        std::function<BVHBuildNode*(int, int, int)> recursiveBuild =
            [&](int start, int end, int depth) -> BVHBuildNode* {

            BVHBuildNode* node = new BVHBuildNode();
            totalNodes++;

            Bounds3f bounds;
            for (int i = start; i < end; ++i)
                bounds = Union(bounds, primInfo[i].bounds);

            int nPrimitives = end - start;

            // Min primitives or max depth -> leaf node
            if (nPrimitives == 1 || depth == 63) {
                int firstPrimOffset = orderedPrims.size();
                for (int i = start; i < end; ++i) {
                    int primNum = primInfo[i].originalIndex;
                    orderedPrims.push_back(m_primitives[primNum]);
                }
                node->initLeaf(firstPrimOffset, nPrimitives, bounds);
                return node;
            }

            Bounds3f centroidBounds;
            for (int i = start; i < end; ++i)
                centroidBounds = Union(centroidBounds, primInfo[i].centroid);
            int dim = centroidBounds.maximumExtent();

            // Centroids clamped -> leaf node
            if (centroidBounds.pMax[dim] == centroidBounds.pMin[dim]) {
                int firstPrimOffset = orderedPrims.size();
                for (int i = start; i < end; ++i) {
                    orderedPrims.push_back(m_primitives[primInfo[i].originalIndex]);
                }
                node->initLeaf(firstPrimOffset, nPrimitives, bounds);
                return node;
            }

            // SAH: partitioning space by filling 12 buckets
            constexpr int nBuckets = 12;
            BucketInfo buckets[nBuckets];

            for (int i = start; i < end; ++i) {
                int b = nBuckets * (primInfo[i].centroid - centroidBounds.pMin)[dim];
                if (b == nBuckets) b = nBuckets - 1;
                buckets[b].count++;
                buckets[b].bounds = Union(buckets[b].bounds, primInfo[i].bounds);
            }

            // Computing cost for any axis aligned split on buckets
            float cost[nBuckets - 1];
            for (int i = 0; i < nBuckets - 1; ++i) {
                Bounds3f b0, b1;
                int count0 = 0, count1 = 0;
                for (int j = 0; j <= i; ++j) {
                    b0 = Union(b0, buckets[j].bounds);
                    count0 += buckets[j].count;
                }
                for (int j = i + 1; j < nBuckets; ++j) {
                    b1 = Union(b1, buckets[j].bounds);
                    count1 += buckets[j].count;
                }
                cost[i] = 1 + (count0 * b0.surfaceArea() + count1 * b1.surfaceArea()) / bounds.surfaceArea();
            }

            float minCost = cost[0];
            int minCostSplitBucket = 0;
            for (int i = 1; i < nBuckets - 1; ++i) {
                if (cost[i] < minCost) {
                    minCost = cost[i];
                    minCostSplitBucket = i;
                }
            }

            float leafCost = (float)nPrimitives;
            if (nPrimitives > 4 || minCost < leafCost) {
                PrimitiveInfo* pmid = std::partition(&primInfo[start], &primInfo[end - 1] + 1,
                    [=](const PrimitiveInfo& pi) {
                        int b = nBuckets * (pi.centroid - centroidBounds.pMin)[dim];
                        if (b == nBuckets) b = nBuckets - 1;
                        return b <= minCostSplitBucket;
                    });
                int mid = pmid - &primInfo[0];

                // Launching separate threads if workload justifies the overhead
                BVHBuildNode *c0, *c1;
                if (end - start > 4096) {
                    tbb::parallel_invoke(
                        [&] { c0 = recursiveBuild(start, mid, depth + 1); },
                        [&] { c1 = recursiveBuild(mid, end, depth + 1); }
                    );
                } else {
                    c0 = recursiveBuild(start, mid, depth + 1);
                    c1 = recursiveBuild(mid, end, depth + 1);
                }

                node->initInterior(dim, c0, c1);
            } else {
                // Inconvenient split -> fall back to leaf
                int firstPrimOffset = orderedPrims.size();
                for (int i = start; i < end; ++i) {
                    orderedPrims.push_back(m_primitives[primInfo[i].originalIndex]);
                }
                node->initLeaf(firstPrimOffset, nPrimitives, bounds);
            }

            return node;
        };

        BVHBuildNode* root = recursiveBuild(0, primInfo.size(), 0);

        // Flattening to linear BVH
        m_primitives = std::move(orderedPrims);
        m_nodes.resize(totalNodes);
        int offset = 0;
        flattenBVH(root, &offset, m_nodes);

        // Build nodes clean up
        std::function<void(BVHBuildNode*)> deleteTree = [&](BVHBuildNode* node) {
            if (node->children[0]) deleteTree(node->children[0]);
            if (node->children[1]) deleteTree(node->children[1]);
            delete node;
        };
        deleteTree(root);
    }

    bool BVH::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const {
        if (m_nodes.empty()) return false;

        bool hitAny = false;
        isect.t = std::numeric_limits<float>::max();

        int toVisitOffset = 0;
        int currentNodeIndex = 0;
        int nodesToVisit[64];

        while (true) {
            const BVHNode* node = &m_nodes[currentNodeIndex];

            if (node->bounds.rayIntersect(ray)) {
                if (node->nPrimitives > 0) {
                    // Leaf node
                    for (int i = 0; i < node->nPrimitives; ++i) {
                        SurfaceInteraction its;
                        if (m_primitives[node->primitivesOffset + i]->rayIntersect(ray, its)) {
                            if (shadowRay)
                                return true;
                            if (its.t < isect.t) {
                                ray.tMax = its.t;
                                isect = its;
                                hitAny = true;
                            }
                        }
                    }
                    if (toVisitOffset == 0) break;
                    currentNodeIndex = nodesToVisit[--toVisitOffset];
                } else {
                    // Internal node: visiting left child first
                    nodesToVisit[toVisitOffset++] = node->rightChildOffset;
                    currentNodeIndex = currentNodeIndex + 1;
                }
            } else {
                if (toVisitOffset == 0) break;
                currentNodeIndex = nodesToVisit[--toVisitOffset];
            }
        }

        if (hitAny)
            isect.primitive->fillInteraction(ray, isect);

        return hitAny;
    }

}