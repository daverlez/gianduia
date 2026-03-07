#include <gianduia/scene/bvh.h>
#include <gianduia/scene/scene.h>
#include <algorithm>

namespace gnd {

    void BVH::build() {
        if (m_primitives.empty()) return;

        std::vector<BVHBuildInfo> buildData;
        buildData.reserve(m_primitives.size());

        for (size_t i = 0; i < m_primitives.size(); ++i) {
            // Exclude invisible area emitters
            if (m_primitives[i]->getEmitter() && !m_primitives[i]->getEmitter()->isVisible())
                continue;

            Bounds3f b = m_primitives[i]->getWorldBounds();
            buildData.push_back({ b, b.pMin * 0.5f + b.pMax * 0.5f, (int)i });
        }

        BVHBuildResult result = BVHBuilder::build(buildData);

        m_nodes = std::move(result.nodes);

        std::vector<std::shared_ptr<Primitive>> orderedPrims;
        orderedPrims.resize(m_primitives.size());

        for (size_t i = 0; i < result.orderedIndices.size(); ++i) {
            orderedPrims[i] = m_primitives[result.orderedIndices[i]];
        }
        m_primitives = std::move(orderedPrims);
    }

    bool BVH::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const {
        if (m_nodes.empty()) return false;

        Vector3f invDir = {1.0f / ray.d.x(), 1.0f / ray.d.y(), 1.0f / ray.d.z()};

        bool hitAny = false;
        isect.t = std::numeric_limits<float>::max();

        int toVisitOffset = 0;
        int currentNodeIndex = 0;
        int nodesToVisit[64];

        while (true) {
            const BVHNode* node = &m_nodes[currentNodeIndex];

            if (node->bounds.rayIntersect(ray, invDir)) {
                if (node->nPrimitives > 0) {
                    // Leaf node
                    for (int i = 0; i < node->nPrimitives; ++i) {
                        SurfaceInteraction its;
                        if (m_primitives[node->primitivesOffset + i]->rayIntersect(ray, its, shadowRay)) {
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

        if (hitAny) {
            isect.primitive->fillInteraction(ray, isect);
        }

        return hitAny;
    }

}