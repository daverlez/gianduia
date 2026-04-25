#include "gianduia/shapes/curveArray.h"
#include <gianduia/core/factory.h>
#include <gianduia/core/fileResolver.h>
#include <fstream>
#include <cstring>
#include <algorithm>

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
            throw std::runtime_error("CurveArraY: invalid file format for " + filename);
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
    }

    Bounds3f CurveArray::getBounds() const {
        return m_bounds;
    }

    bool CurveArray::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const {
        if (m_nodes.empty()) return false;

        bool hit = false;
        Vector3f invDir(1.0f / ray.d.x(), 1.0f / ray.d.y(), 1.0f / ray.d.z());
        int dirIsNeg[3] = { invDir.x() < 0.0f, invDir.y() < 0.0f, invDir.z() < 0.0f };

        int nodesToVisit[64];
        int toVisitOffset = 0;
        int currentNodeIndex = 0;
        float tHitMax = ray.tMax;

        int bestSegmentIndex = -1;

        while (true) {
            const BVHNode& node = m_nodes[currentNodeIndex];

            if (node.bounds.rayIntersect(ray, invDir)) {
                if (node.nPrimitives > 0) {
                    for (int i = 0; i < node.nPrimitives; ++i) {
                        int segIdx = m_orderedIndices[node.primitivesOffset + i];
                        if (intersectSegment(ray, segIdx, tHitMax, isect)) {
                            hit = true;
                            bestSegmentIndex = segIdx;
                            if (predicate) return true;
                        }
                    }
                    if (toVisitOffset == 0) break;
                    currentNodeIndex = nodesToVisit[--toVisitOffset];
                } else {
                    if (dirIsNeg[node.axis]) {
                        nodesToVisit[toVisitOffset++] = currentNodeIndex + 1;
                        currentNodeIndex = node.rightChildOffset;
                    } else {
                        nodesToVisit[toVisitOffset++] = node.rightChildOffset;
                        currentNodeIndex = currentNodeIndex + 1;
                    }
                }
            } else {
                if (toVisitOffset == 0) break;
                currentNodeIndex = nodesToVisit[--toVisitOffset];
            }
        }

        if (hit && !predicate) {
            isect.primIndex = bestSegmentIndex;
        }

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