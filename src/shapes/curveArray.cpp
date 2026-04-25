#include "gianduia/shapes/curveArray.h"
#include <gianduia/core/factory.h>
#include <gianduia/core/fileResolver.h>
#include <fstream>
#include <cstring>
#include <algorithm>
#include <functional>
#include <iostream>
#include <limits>

#include <hwy/highway.h>
namespace hn = hwy::HWY_NAMESPACE;

namespace gnd {

    Point3f CurveSegment::evaluateBezier(float u) const {
        float compU = 1.0f - u;
        return p[0] * (compU * compU * compU) +
               p[1] * (3.0f * compU * compU * u) +
               p[2] * (3.0f * compU * u * u) +
               p[3] * (u * u * u);
    }

    Vector3f CurveSegment::evaluateDerivative(float u) const {
        float compU = 1.0f - u;
        return (p[1] - p[0]) * (3.0f * compU * compU) +
               (p[2] - p[1]) * (6.0f * compU * u) +
               (p[3] - p[2]) * (3.0f * u * u);
    }

    Bounds3f CurveSegment::getBounds() const {
        Bounds3f b(p[0], p[1]);
        b = Union(b, p[2]);
        b = Union(b, p[3]);
        float maxRadius = std::max(w[0], w[1]) * 0.5f;
        b.pMin -= Vector3f(maxRadius, maxRadius, maxRadius);
        b.pMax += Vector3f(maxRadius, maxRadius, maxRadius);
        return b;
    }

    CurveArray::CurveArray(const PropertyList& props) : Shape(props) {
        std::string filename = FileResolver::resolve(props.getString("filename"));
        loadBinary(filename);
    }

    void CurveArray::loadBinary(const std::string& filename) {
        std::ifstream is(filename, std::ios::binary);
        if (!is) {
            throw std::runtime_error("CurveArray: could not open file " + filename);
        }

        char magic[4];
        is.read(magic, 4);
        if (std::strncmp(magic, "HAIR", 4) != 0) {
            throw std::runtime_error("CurveArray: invalid file format for " + filename);
        }

        uint32_t count = 0;
        is.read(reinterpret_cast<char*>(&count), sizeof(uint32_t));

        m_segments.resize(count);
        is.read(reinterpret_cast<char*>(m_segments.data()), count * sizeof(CurveSegment));
    }

    void CurveArray::activate() {
        std::vector<BVHBuildInfo> buildData(m_segments.size());
        for (size_t i = 0; i < m_segments.size(); ++i) {
            buildData[i].bounds = m_segments[i].getBounds();
            buildData[i].centroid = buildData[i].bounds.centroid();
            buildData[i].originalIndex = i;
        }

        BVHBuildResult result = BVHBuilder::build(buildData);
        m_nodes = std::move(result.nodes);
        m_orderedIndices = std::move(result.orderedIndices);

        if (!m_nodes.empty()) {
            m_bounds = m_nodes[0].bounds;
        }

        // Tree Collapsing (binary to BVH4)
        m_nodes4.clear();
        m_nodes4.reserve(m_nodes.size());

        std::function<int(int)> buildBVH4 = [&](int binNodeIdx) -> int {
            int bvh4Idx = (int)m_nodes4.size();
            m_nodes4.push_back(BVHNode4());

            std::vector<int> candidates = { binNodeIdx };

            while(candidates.size() < 4) {
                int bestIdx = -1;
                float bestArea = -1.0f;

                for(size_t i = 0; i < candidates.size(); ++i) {
                    const BVHNode& c = m_nodes[candidates[i]];
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
                const BVHNode& expandNode = m_nodes[expandIdx];

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
                const BVHNode& c = m_nodes[candIdx];
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

        if (!m_nodes.empty()) {
            buildBVH4(0);
            m_nodes.clear();
            m_nodes.shrink_to_fit();
        }

        std::cout << "CurveArray: BVH Collapsed into " << m_nodes4.size() << " Wide-BVH4 nodes." << std::endl;
    }

    Bounds3f CurveArray::getBounds() const {
        return m_bounds;
    }

    bool CurveArray::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const {
        if (m_nodes4.empty()) return false;

        bool hit = false;
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

        float tHitMax = ray.tMax;
        V t_max_simd = hn::Set(d, tHitMax);

        int bestSegmentIndex = -1;

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

                    if (node.childType[i] == 1) stack[stackPtr++] = node.offset[i];

                    else if (node.childType[i] == 2) {
                        int offset = node.offset[i];
                        int count = node.packCount[i];

                        for (int p = 0; p < count; ++p) {
                            int segIdx = m_orderedIndices[offset + p];

                            if (intersectSegment(ray, segIdx, tHitMax, isect)) {
                                hit = true;
                                bestSegmentIndex = segIdx;

                                t_max_simd = hn::Set(d, tHitMax);

                                if (predicate) return true;
                            }
                        }
                    }
                }
            }
        }

        if (hit) isect.primIndex = bestSegmentIndex;

        return hit;
    }

    void CurveArray::fillInteraction(const Ray& ray, SurfaceInteraction& isect) const {
        int segIdx = isect.primIndex;
        const CurveSegment& seg = m_segments[segIdx];

        float uHit = isect.uv.x();

        isect.p = ray(isect.t);
        isect.time = ray.time;
        isect.dpdu = seg.evaluateDerivative(uHit);

        Point3f curvePointObj = seg.evaluateBezier(uHit);
        Vector3f normalRad = isect.p - curvePointObj;

        float dpduLength2 = isect.dpdu.lengthSquared();
        if (dpduLength2 > 0.0f) {
            normalRad = normalRad - isect.dpdu * (Dot(normalRad, isect.dpdu) / dpduLength2);
        }

        if (normalRad.lengthSquared() < 1e-6f) {
            normalRad = Vector3f(0.0f, 1.0f, 0.0f);
        }

        isect.n = Normal3f(Normalize(normalRad));
        isect.dpdu = Normalize(isect.dpdu);
    }

    bool CurveArray::intersectSegment(const Ray& ray, int segmentIndex, float& tMax, SurfaceInteraction& isect) const {
        const CurveSegment& seg = m_segments[segmentIndex];

        float rayLength = ray.d.length();
        if (rayLength < 1e-6f) return false;

        Vector3f dx;
        Frame localCoord(Normalize(ray.d));
        dx = localCoord.x;

        Transform rayToObj = Transform::LookAt(ray.o, ray.o - ray.d, dx);
        Transform objToRay = rayToObj.inverse();

        Point3f cp[4];
        for (int i = 0; i < 4; ++i) {
            cp[i] = objToRay(seg.p[i]);
            cp[i].z() /= rayLength;
        }

        float maxWidth = std::max(seg.w[0], seg.w[1]);
        Bounds3f curveBounds(cp[0], cp[1]);
        curveBounds = Union(curveBounds, cp[2]);
        curveBounds = Union(curveBounds, cp[3]);

        curveBounds.pMin -= Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;
        curveBounds.pMax += Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;

        if (curveBounds.pMin.x() > 0.0f || curveBounds.pMax.x() < 0.0f ||
            curveBounds.pMin.y() > 0.0f || curveBounds.pMax.y() < 0.0f) {
            return false;
        }

        if (curveBounds.pMax.z() < ray.tMin || curveBounds.pMin.z() > tMax) {
            return false;
        }

        float L0 = 0.0f;
        for (int i = 0; i < 2; ++i) {
            float dx = std::abs(cp[i].x() - 2.0f * cp[i + 1].x() + cp[i + 2].x());
            float dy = std::abs(cp[i].y() - 2.0f * cp[i + 1].y() + cp[i + 2].y());
            float dz = std::abs(cp[i].z() - 2.0f * cp[i + 1].z() + cp[i + 2].z());
            L0 = std::max({L0, dx, dy, dz});
        }

        float eps = std::max(seg.w[0], seg.w[1]) * 0.05f;
        int maxDepth = 0;
        if (L0 > 0.0f) {
            float fr0 = std::log(1.41421356237f * 12.0f * L0 / (8.0f * eps)) * 0.7213475108f;
            maxDepth = std::clamp((int)std::round(fr0), 0, 10);
        }

        return recursiveIntersect(ray, cp, rayToObj, seg.w, 0.0f, 1.0f, 0, maxDepth, tMax, isect);
    }

    void CurveArray::subdivideBezier(const Point3f cp[4], Point3f cp0[4], Point3f cp1[4]) {
        Point3f p01 = (cp[0] + cp[1]) * 0.5f;
        Point3f p12 = (cp[1] + cp[2]) * 0.5f;
        Point3f p23 = (cp[2] + cp[3]) * 0.5f;

        Point3f p012 = (p01 + p12) * 0.5f;
        Point3f p123 = (p12 + p23) * 0.5f;
        Point3f p0123 = (p012 + p123) * 0.5f;

        cp0[0] = cp[0]; cp0[1] = p01; cp0[2] = p012; cp0[3] = p0123;
        cp1[0] = p0123; cp1[1] = p123; cp1[2] = p23; cp1[3] = cp[3];
    }

    bool CurveArray::recursiveIntersect(const Ray& ray, const Point3f cp[4], const Transform& rayToObj, const float widths[2],
                                        float u0, float u1, int depth, int maxDepth, float& tMax, SurfaceInteraction& isect) const {
        float maxWidth = std::max(widths[0], widths[1]);

        Bounds3f curveBounds(cp[0], cp[1]);
        curveBounds = Union(curveBounds, cp[2]);
        curveBounds = Union(curveBounds, cp[3]);
        curveBounds.pMin -= Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;
        curveBounds.pMax += Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;

        if (curveBounds.pMin.x() > 0.0f || curveBounds.pMax.x() < 0.0f ||
            curveBounds.pMin.y() > 0.0f || curveBounds.pMax.y() < 0.0f) return false;
        if (curveBounds.pMax.z() < ray.tMin || curveBounds.pMin.z() > tMax) return false;

        if (depth == maxDepth) {
            Vector2f pa(cp[0].x(), cp[0].y());
            Vector2f pb(cp[3].x(), cp[3].y());
            Vector2f v = pb - pa;
            float len2 = v.x() * v.x() + v.y() * v.y();

            float tHitUnclamped = 0.0f;
            if (len2 > 1e-6f) tHitUnclamped = -(pa.x() * v.x() + pa.y() * v.y()) / len2;
            float tHit = std::clamp(tHitUnclamped, 0.0f, 1.0f);

            Vector2f pProj = pa + v * tHit;
            float d2 = pProj.x() * pProj.x() + pProj.y() * pProj.y();

            float uHit = u0 * (1.0f - tHitUnclamped) + u1 * tHitUnclamped;
            uHit = std::clamp(uHit, 0.0f, 1.0f);
            float radius = (widths[0] * (1.0f - uHit) + widths[1] * uHit) * 0.5f;

            if (d2 <= radius * radius) {
                float zHitCenter = cp[0].z() * (1.0f - tHit) + cp[3].z() * tHit;
                float dz = std::sqrt(radius * radius - d2);
                float zHit = zHitCenter - dz / ray.d.length();

                if (zHit > ray.tMin && zHit < tMax) {
                    tMax = zHit;
                    isect.t = zHit;
                    float hHit = std::sqrt(d2) / radius;
                    if (pProj.x() < 0.0f) hHit = -hHit;

                    isect.uv = Point2f(uHit, (hHit + 1.0f) * 0.5f);
                    return true;
                }
            }
            return false;
        }

        Point3f cp0[4], cp1[4];
        subdivideBezier(cp, cp0, cp1);
        float uMid = (u0 + u1) * 0.5f;

        bool hit = false;
        if (recursiveIntersect(ray, cp0, rayToObj, widths, u0, uMid, depth + 1, maxDepth, tMax, isect)) hit = true;
        if (recursiveIntersect(ray, cp1, rayToObj, widths, uMid, u1, depth + 1, maxDepth, tMax, isect)) hit = true;

        return hit;
    }

    GND_REGISTER_CLASS(CurveArray, "curve_array");
}