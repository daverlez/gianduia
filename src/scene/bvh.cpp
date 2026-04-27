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

}