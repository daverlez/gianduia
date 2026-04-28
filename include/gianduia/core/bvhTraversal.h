#pragma once

#include <gianduia/core/bvhBuilder.h>
#include <gianduia/shapes/shape.h>
#include <vector>
#include <concepts>

#include <hwy/highway.h>

namespace hn = hwy::HWY_NAMESPACE;

namespace gnd {

    // The leaf visitor is invoked with an offset (int) and a count (int); it returns a boolean.
    template <typename F>
    concept BVHLeafIntersector = std::invocable<F, int, int> && 
                                 std::same_as<std::invoke_result_t<F, int, int>, bool>;

    template <BVHLeafIntersector Intersector>
    inline bool traverseBVH4(
        const std::vector<BVHNode4>& nodes4, 
        const Ray& ray, 
        bool shadowRay, 
        Intersector&& intersectLeaf) 
    {
        if (nodes4.empty()) return false;

        bool hitAny = false;

        int stack[64];
        int stackPtr = 0;
        stack[stackPtr++] = 0;

        hn::FixedTag<float, 4> d;
        using V = hn::Vec<decltype(d)>;
        using M = hn::Mask<decltype(d)>;

        V r_ox = hn::Set(d, ray.o.x());
        V r_oy = hn::Set(d, ray.o.y());
        V r_oz = hn::Set(d, ray.o.z());

        Vector3f invDir(1.0f / ray.d.x(), 1.0f / ray.d.y(), 1.0f / ray.d.z());
        V r_invdx = hn::Set(d, invDir.x());
        V r_invdy = hn::Set(d, invDir.y());
        V r_invdz = hn::Set(d, invDir.z());

        V rayTMin = hn::Set(d, ray.tMin);
        V tMax_simd = hn::Set(d, ray.tMax);

        while (stackPtr > 0) {
            int nodeIdx = stack[--stackPtr];
            const BVHNode4& node = nodes4[nodeIdx];

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
            V tmax = hn::Min(hn::Min(tmaxx, tmaxy), hn::Min(tmaxz, tMax_simd));

            M validBox = hn::Le(tmin, tmax);

            if (!hn::AllFalse(d, validBox)) {
                uint8_t mask_bytes[8] = {0};
                hn::StoreMaskBits(d, validBox, mask_bytes);

                for (int i = 0; i < 4; ++i) {
                    if ((mask_bytes[0] & (1 << i)) == 0) continue;

                    if (node.childType[i] == 1) {
                        stack[stackPtr++] = node.offset[i];
                    } 
                    else if (node.childType[i] == 2) {
                        bool hitLocal = intersectLeaf(node.offset[i], node.packCount[i]);

                        if (hitLocal) {
                            hitAny = true;
                            if (shadowRay) return true;
                            tMax_simd = hn::Set(d, ray.tMax);
                        }
                    }
                }
            }
        }

        return hitAny;
    }

}