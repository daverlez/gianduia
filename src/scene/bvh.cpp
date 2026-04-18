#include <gianduia/scene/bvh.h>
#include <gianduia/scene/scene.h>
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

        // ---- Tree Collapsing (da binario a BVH4) ----
        m_nodes4.clear();
        m_nodes4.reserve(nodes.size());

        std::function<int(int)> buildBVH4 = [&](int binNodeIdx) -> int {
            int bvh4Idx = (int)m_nodes4.size();
            m_nodes4.push_back(BVHNode4());

            std::vector<int> candidates = { binNodeIdx };

            while(candidates.size() < 4) {
                int bestIdx = -1;
                float bestArea = -1.0f;

                for(size_t i = 0; i < candidates.size(); ++i) {
                    const BVHNode& c = nodes[candidates[i]];
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
                const BVHNode& expandNode = nodes[expandIdx];

                candidates.push_back(expandIdx + 1);
                candidates.push_back(expandNode.rightChildOffset);
            }

            struct ChildData {
                uint8_t type;
                uint32_t offset;
                uint32_t primCount;
                Bounds3f bounds;
            };
            std::vector<ChildData> childrenData;

            for(int candIdx : candidates) {
                const BVHNode& c = nodes[candIdx];
                if (c.nPrimitives > 0) {
                    childrenData.push_back({2, (uint32_t)c.primitivesOffset, (uint32_t)c.nPrimitives, c.bounds});
                } else {
                    int child4Idx = buildBVH4(candIdx);
                    childrenData.push_back({1, (uint32_t)child4Idx, 0, c.bounds});
                }
            }

            BVHNode4& node4 = m_nodes4[bvh4Idx];
            for(int i = 0; i < 4; ++i) {
                if (i < childrenData.size()) {
                    node4.childType[i] = childrenData[i].type;
                    node4.offset[i]    = childrenData[i].offset;
                    node4.primCount[i] = childrenData[i].primCount;
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

        if (!nodes.empty()) {
            buildBVH4(0);
        }
    }

    bool BVH::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool shadowRay) const {
        if (m_nodes4.empty()) return false;

        bool hitAny = false;
        isect.t = std::numeric_limits<float>::max();

        int stack[64];
        int stackPtr = 0;
        stack[stackPtr++] = 0;

        hn::FixedTag<float, 4> d;
        using V = hn::Vec<decltype(d)>;
        using M = hn::Mask<decltype(d)>;

        V r_ox = hn::Set(d, ray.o.x());
        V r_oy = hn::Set(d, ray.o.y());
        V r_oz = hn::Set(d, ray.o.z());

        Vector3f invDir = {1.0f / ray.d.x(), 1.0f / ray.d.y(), 1.0f / ray.d.z()};
        V r_invdx = hn::Set(d, invDir.x());
        V r_invdy = hn::Set(d, invDir.y());
        V r_invdz = hn::Set(d, invDir.z());

        V rayTMin = hn::Set(d, ray.tMin);
        V t_max_simd = hn::Set(d, ray.tMax);

        while (stackPtr > 0) {
            int nodeIdx = stack[--stackPtr];
            const BVHNode4& node = m_nodes4[nodeIdx];

            V minX = hn::Load(d, node.minX); V minY = hn::Load(d, node.minY); V minZ = hn::Load(d, node.minZ);
            V maxX = hn::Load(d, node.maxX); V maxY = hn::Load(d, node.maxY); V maxZ = hn::Load(d, node.maxZ);

            V t1x = hn::Mul(hn::Sub(minX, r_ox), r_invdx);
            V t2x = hn::Mul(hn::Sub(maxX, r_ox), r_invdx);
            V tminx = hn::Min(t1x, t2x); V tmaxx = hn::Max(t1x, t2x);

            V t1y = hn::Mul(hn::Sub(minY, r_oy), r_invdy);
            V t2y = hn::Mul(hn::Sub(maxY, r_oy), r_invdy);
            V tminy = hn::Min(t1y, t2y); V tmaxy = hn::Max(t1y, t2y);

            V t1z = hn::Mul(hn::Sub(minZ, r_oz), r_invdz);
            V t2z = hn::Mul(hn::Sub(maxZ, r_oz), r_invdz);
            V tminz = hn::Min(t1z, t2z); V tmaxz = hn::Max(t1z, t2z);

            V tmin = hn::Max(hn::Max(tminx, tminy), hn::Max(tminz, rayTMin));
            V tmax = hn::Min(hn::Min(tmaxx, tmaxy), hn::Min(tmaxz, t_max_simd));

            M validBox = hn::Le(tmin, tmax);

            if (!hn::AllFalse(d, validBox)) {
                uint8_t mask_bytes[8] = {0};
                hn::StoreMaskBits(d, validBox, mask_bytes);

                for (int i = 0; i < 4; ++i) {
                    if ((mask_bytes[0] & (1 << i)) == 0) continue;

                    if (node.childType[i] == 1) {
                        stack[stackPtr++] = node.offset[i];
                    } else if (node.childType[i] == 2) {
                        int primOffset = node.offset[i];
                        int pCount = node.primCount[i];

                        for (int p = 0; p < pCount; ++p) {
                            SurfaceInteraction its;
                            if (m_primitives[primOffset + p]->rayIntersect(ray, its, shadowRay)) {
                                if (shadowRay) return true;

                                if (its.t < isect.t) {
                                    ray.tMax = its.t;
                                    t_max_simd = hn::Set(d, ray.tMax);
                                    isect = its;
                                    hitAny = true;
                                }
                            }
                        }
                    }
                }
            }
        }

        if (hitAny) {
            isect.primitive->fillInteraction(ray, isect);
        }

        return hitAny;
    }

}