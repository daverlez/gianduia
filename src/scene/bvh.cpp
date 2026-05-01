#include "gianduia/scene/bvh.h"
#include "gianduia/scene/scene.h"
#include "gianduia/core/bvhTraversal.h"
#include <algorithm>

namespace hn = hwy::HWY_NAMESPACE;

namespace gnd {

    void BVH::build() {
        if (m_primitives.empty()) return;

        std::vector<BVHBuildInfo> buildData;
        buildData.reserve(m_primitives.size());

        for (size_t i = 0; i < m_primitives.size(); ++i) {
            Bounds3f b = m_primitives[i]->getWorldBounds();
            buildData.push_back({ b, b.pMin * 0.5f + b.pMax * 0.5f, (int)i });
        }

        BVHBuildResult result = BVHBuilder::build(buildData);
        std::vector<BVHNode> nodes = std::move(result.nodes);

        std::vector<std::shared_ptr<Primitive>> orderedPrims;
        orderedPrims.resize(m_primitives.size());

        for (size_t i = 0; i < result.orderedIndices.size(); ++i) {
            orderedPrims[i] = m_primitives[result.orderedIndices[i]];
        }
        m_primitives = std::move(orderedPrims);

        if (!nodes.empty())
            m_nodes4 = BVHBuilder::buildWide(nodes);
    }

    bool BVH::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const {
        isect.t = std::numeric_limits<float>::max();

        auto leafIntersector = [&](int offset, int count) -> bool {
            bool hitLocal = false;

            for (int p = 0; p < count; ++p) {
                SurfaceInteraction its;

                if (m_primitives[offset + p]->rayIntersect(ray, its, shadowRay)) {
                    if (shadowRay) return true;

                    if (its.t < isect.t) {
                        ray.tMax = its.t;
                        isect = its;
                        hitLocal = true;
                    }
                }
            }
            return hitLocal;
        };

        bool hitAny = traverseBVH4(m_nodes4, ray, shadowRay, leafIntersector);

        if (hitAny && !shadowRay)
            isect.primitive->fillInteraction(ray, isect);

        return hitAny;
    }

    void BVH::getDebugNodes(std::vector<BvhDebugNode>& outNodes, int nodeIdx, int depth) const {
        if (m_nodes4.empty() || nodeIdx >= m_nodes4.size()) return;

        const BVHNode4& node = m_nodes4[nodeIdx];

        for (int i = 0; i < 4; ++i) {
            if (node.childType[i] == 0) continue;

            Bounds3f childBounds(
                Point3f(node.minX[i], node.minY[i], node.minZ[i]),
                Point3f(node.maxX[i], node.maxY[i], node.maxZ[i])
            );

            bool isLeaf = (node.childType[i] == 2);
            outNodes.push_back({ childBounds, depth, isLeaf, false });

            if (!isLeaf) {
                getDebugNodes(outNodes, node.offset[i], depth + 1);
            } else {
                int primOffset = node.offset[i];
                int packCount = node.packCount[i];

                for (int p = 0; p < packCount; ++p) {
                    std::shared_ptr<Primitive> prim = m_primitives[primOffset + p];
                    Transform toWorld = prim->getToWorld(0.0f);
                    std::vector<BvhDebugNode> localNodes;
                    if (prim->getShape()) {
                        prim->getShape()->getBvhDebugNodes(localNodes, 0, depth + 1);
                    }

                    for (auto& localNode : localNodes) {
                        localNode.bounds = toWorld(localNode.bounds);
                        outNodes.push_back(localNode);
                    }
                }
            }
        }
    }

}