#include <gianduia/core/bvhBuilder.h>
#include <algorithm>
#include <tbb/parallel_invoke.h>
#include <numeric>

namespace gnd {

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

    struct BucketInfo {
        int count = 0;
        Bounds3f bounds;
    };

    int flattenBVH(BVHBuildNode* node, int* offset, std::vector<BVHNode>& linearNodes) {
        BVHNode* linearNode = &linearNodes[*offset];
        linearNode->bounds = node->bounds;
        int currOffset = (*offset)++;

        if (node->nPrimitives > 0) {
            linearNode->primitivesOffset = node->firstPrimOffset;
            linearNode->nPrimitives = node->nPrimitives;
        } else {
            linearNode->axis = node->splitAxis;
            linearNode->nPrimitives = 0;
            flattenBVH(node->children[0], offset, linearNodes);
            linearNode->rightChildOffset = flattenBVH(node->children[1], offset, linearNodes);
        }
        return currOffset;
    }

    void deleteBuildTree(BVHBuildNode* node) {
        if (node->children[0]) deleteBuildTree(node->children[0]);
        if (node->children[1]) deleteBuildTree(node->children[1]);
        delete node;
    }

    BVHBuildResult BVHBuilder::build(std::vector<BVHBuildInfo>& buildData) {
        BVHBuildResult result;
        if (buildData.empty()) return result;

        std::atomic<int> totalNodes{0};
        std::vector<BVHBuildInfo> orderedBuildData;
        orderedBuildData.resize(buildData.size());
        std::atomic<int> atomicOrderedOffset{0};

        std::function<BVHBuildNode*(int, int, int)> recursiveBuild =
            [&](int start, int end, int depth) -> BVHBuildNode* {

            BVHBuildNode* node = new BVHBuildNode();
            ++totalNodes;

            Bounds3f bounds;
            for (int i = start; i < end; ++i)
                bounds = Union(bounds, buildData[i].bounds);

            int nPrimitives = end - start;

            auto createLeaf = [&]() {
                int myOffset = atomicOrderedOffset.fetch_add(nPrimitives);

                for (int i = 0; i < nPrimitives; ++i) {
                    orderedBuildData[myOffset + i] = buildData[start + i];
                }

                node->initLeaf(myOffset, nPrimitives, bounds);
                return node;
            };

            if (nPrimitives == 1 || depth == 63) {
                return createLeaf();
            }

            Bounds3f centroidBounds;
            for (int i = start; i < end; ++i)
                centroidBounds = Union(centroidBounds, buildData[i].centroid);
            int dim = centroidBounds.maximumExtent();

            if (centroidBounds.pMax[dim] == centroidBounds.pMin[dim]) {
                return createLeaf();
            }

            // SAH Logic
            constexpr int nBuckets = 12;
            BucketInfo buckets[nBuckets];

            float denominator = centroidBounds.pMax[dim] - centroidBounds.pMin[dim];
            if (denominator == 0) denominator = 1.0f; // Safety catch

            for (int i = start; i < end; ++i) {
                int b = nBuckets * (buildData[i].centroid - centroidBounds.pMin)[dim] / denominator;
                if (b == nBuckets) b = nBuckets - 1;
                buckets[b].count++;
                buckets[b].bounds = Union(buckets[b].bounds, buildData[i].bounds);
            }

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
                auto pmid = std::partition(&buildData[start], &buildData[end - 1] + 1,
                    [=](const BVHBuildInfo& pi) {
                        int b = nBuckets * (pi.centroid - centroidBounds.pMin)[dim] / denominator;
                        if (b == nBuckets) b = nBuckets - 1;
                        return b <= minCostSplitBucket;
                    });
                int mid = pmid - &buildData[0];

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
                return createLeaf();
            }
            return node;
        };

        BVHBuildNode* root = recursiveBuild(0, buildData.size(), 0);

        result.nodes.resize(totalNodes);
        int offset = 0;
        flattenBVH(root, &offset, result.nodes);

        result.orderedIndices.reserve(orderedBuildData.size());
        for (const auto& info : orderedBuildData) {
            result.orderedIndices.push_back(info.originalIndex);
        }

        deleteBuildTree(root);
        return result;
    }

    std::vector<BVHNode4> BVHBuilder::buildWide(const std::vector<BVHNode>& binaryNodes) {
        if (binaryNodes.empty()) return {};

        std::vector<BVHNode4> nodes4;
        nodes4.reserve(binaryNodes.size() / 2);

        std::function<int(int)> collapse = [&](int binNodeIdx) -> int {
            int bvh4Idx = (int)nodes4.size();
            nodes4.push_back(BVHNode4());

            std::vector<int> candidates = { binNodeIdx };

            while(candidates.size() < 4) {
                int bestIdx = -1;
                float bestArea = -1.0f;

                for(size_t i = 0; i < candidates.size(); ++i) {
                    const BVHNode& c = binaryNodes[candidates[i]];
                    if (c.nPrimitives == 0) {
                        float area = c.bounds.surfaceArea();
                        if (area > bestArea) {
                            bestArea = area;
                            bestIdx = (int)i;
                        }
                    }
                }

                if (bestIdx == -1) break;

                int expandIdx = candidates[bestIdx];
                candidates.erase(candidates.begin() + bestIdx);
                const BVHNode& expandNode = binaryNodes[expandIdx];

                candidates.push_back(expandIdx + 1);
                candidates.push_back(expandNode.rightChildOffset);
            }

            struct ChildData {
                uint8_t type;
                uint32_t offset;
                uint32_t count;
                Bounds3f bounds;
            };
            std::vector<ChildData> childrenData;

            for(int candIdx : candidates) {
                const BVHNode& c = binaryNodes[candIdx];
                if (c.nPrimitives > 0) {
                    childrenData.push_back({2, (uint32_t)c.primitivesOffset, (uint32_t)c.nPrimitives, c.bounds});
                } else {
                    int child4Idx = collapse(candIdx);
                    childrenData.push_back({1, (uint32_t)child4Idx, 0, c.bounds});
                }
            }

            BVHNode4& node4 = nodes4[bvh4Idx];
            for(int i = 0; i < 4; ++i) {
                if (i < childrenData.size()) {
                    node4.childType[i] = childrenData[i].type;
                    node4.offset[i]    = childrenData[i].offset;
                    node4.packCount[i] = childrenData[i].count;
                    node4.minX[i] = childrenData[i].bounds.pMin.x();
                    node4.minY[i] = childrenData[i].bounds.pMin.y();
                    node4.minZ[i] = childrenData[i].bounds.pMin.z();
                    node4.maxX[i] = childrenData[i].bounds.pMax.x();
                    node4.maxY[i] = childrenData[i].bounds.pMax.y();
                    node4.maxZ[i] = childrenData[i].bounds.pMax.z();
                } else {
                    node4.childType[i] = 0;
                    node4.minX[i] = node4.minY[i] = node4.minZ[i] = std::numeric_limits<float>::infinity();
                    node4.maxX[i] = node4.maxY[i] = node4.maxZ[i] = -std::numeric_limits<float>::infinity();
                }
            }
            return bvh4Idx;
        };

        collapse(0);
        return nodes4;
    }
}
