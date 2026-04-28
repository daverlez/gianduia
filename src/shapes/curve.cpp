#include <gianduia/shapes/curve.h>
#include <gianduia/core/factory.h>

namespace gnd {
    Curve::Curve(const PropertyList& props): Shape(props) {
        m_p[0] = props.getPoint3("p0");
        m_p[1] = props.getPoint3("p1");
        m_p[2] = props.getPoint3("p2");
        m_p[3] = props.getPoint3("p3");

        m_width[0] = props.getFloat("w0");
        m_width[1] = props.getFloat("w1");
    }

    Point3f Curve::evaluateBezier(float u) const {
        float compU = 1.0f - u;
        return m_p[0] * (compU * compU * compU) +
               m_p[1] * (3.0f * compU * compU * u) +
               m_p[2] * (3.0f * compU * u * u) +
               m_p[3] * (u * u * u);
    }

    Vector3f Curve::evaluateDerivative(float u) const {
        float compU = 1.0f - u;
        return (m_p[1] - m_p[0]) * (3.0f * compU * compU) +
               (m_p[2] - m_p[1]) * (6.0f * compU * u) +
               (m_p[3] - m_p[2]) * (3.0f * u * u);
    }

    Bounds3f Curve::getBounds() const {
        Bounds3f b(m_p[0], m_p[1]);
        b = Union(b, m_p[2]);
        b = Union(b, m_p[3]);

        float maxRadius = std::max(m_width[0], m_width[1]) * 0.5f;
        
        b.pMin -= Vector3f(maxRadius, maxRadius, maxRadius);
        b.pMax += Vector3f(maxRadius, maxRadius, maxRadius);
        return b;
    }

    bool Curve::rayIntersect(const Ray& ray, SurfaceInteraction& isect, bool predicate) const {
        float rayLength = ray.d.length();
        if (rayLength < 1e-6f) return false;

        Vector3f dx;
        Frame localCoord(Normalize(ray.d));
        dx = localCoord.x;

        Transform rayToObj = Transform::LookAt(ray.o, ray.o - ray.d, dx);
        Transform objToRay = rayToObj.inverse();

        Point3f cp[4];
        for (int i = 0; i < 4; ++i) {
            cp[i] = objToRay(m_p[i]);
            cp[i].z() /= rayLength;
        }

        float maxWidth = std::max(m_width[0], m_width[1]);
        Bounds3f curveBounds(cp[0], cp[1]);
        curveBounds = Union(curveBounds, cp[2]);
        curveBounds = Union(curveBounds, cp[3]);

        curveBounds.pMin -= Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;
        curveBounds.pMax += Vector3f(maxWidth, maxWidth, 0.0f) * 0.5f;

        if (curveBounds.pMin.x() > 0.0f || curveBounds.pMax.x() < 0.0f ||
            curveBounds.pMin.y() > 0.0f || curveBounds.pMax.y() < 0.0f) {
            return false;
        }

        if (curveBounds.pMax.z() < ray.tMin || curveBounds.pMin.z() > ray.tMax) {
            return false;
        }

        float L0 = 0.0f;
        for (int i = 0; i < 2; ++i) {
            float dx = std::abs(cp[i].x() - 2.0f * cp[i + 1].x() + cp[i + 2].x());
            float dy = std::abs(cp[i].y() - 2.0f * cp[i + 1].y() + cp[i + 2].y());
            float dz = std::abs(cp[i].z() - 2.0f * cp[i + 1].z() + cp[i + 2].z());
            L0 = std::max({L0, dx, dy, dz});
        }

        float eps = std::max(m_width[0], m_width[1]) * 0.05f;

        int maxDepth = 0;
        // Note: same approach as PBRT-v3 to determine max depth
        if (L0 > 0.0f) {
            float fr0 = std::log(1.41421356237f * 12.0f * L0 / (8.0f * eps)) * 0.7213475108f;
            maxDepth = std::clamp((int)std::round(fr0), 0, 10);
        }

        float tMax = ray.tMax;
        bool hit = recursiveIntersect(ray, cp, rayToObj, 0.0f, 1.0f, 0, maxDepth, tMax, isect);

        return hit;
    }

    void Curve::fillInteraction(const Ray& ray, SurfaceInteraction& isect) const {
        float uHit = isect.uv.x();

        isect.p = ray(isect.t);
        isect.time = ray.time;

        isect.dpdu = evaluateDerivative(uHit);

        Point3f curvePointObj = evaluateBezier(uHit);

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

    void Curve::subdivideBezier(const Point3f cp[4], Point3f cp0[4], Point3f cp1[4]) {
        Point3f p01 = (cp[0] + cp[1]) * 0.5f;
        Point3f p12 = (cp[1] + cp[2]) * 0.5f;
        Point3f p23 = (cp[2] + cp[3]) * 0.5f;

        Point3f p012 = (p01 + p12) * 0.5f;
        Point3f p123 = (p12 + p23) * 0.5f;

        Point3f p0123 = (p012 + p123) * 0.5f;

        cp0[0] = cp[0];
        cp0[1] = p01;
        cp0[2] = p012;
        cp0[3] = p0123;

        cp1[0] = p0123;
        cp1[1] = p123;
        cp1[2] = p23;
        cp1[3] = cp[3];
    }

    bool Curve::recursiveIntersect(const Ray& ray, const Point3f cp[4], const Transform& rayToObj,
                                   float u0, float u1, int depth, int maxDepth, float& tMax, SurfaceInteraction& isect) const {
        float maxWidth = std::max(m_width[0], m_width[1]);

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

        if (depth == maxDepth) {
            Vector2f pa(cp[0].x(), cp[0].y());
            Vector2f pb(cp[3].x(), cp[3].y());
            Vector2f v = pb - pa;
            float len2 = v.x() * v.x() + v.y() * v.y();

            float tHitUnclamped = 0.0f;
            if (len2 > 1e-6f) {
                tHitUnclamped = -(pa.x() * v.x() + pa.y() * v.y()) / len2;
            }
            float tHit = std::clamp(tHitUnclamped, 0.0f, 1.0f);

            Vector2f pProj = pa + v * tHit;
            float d2 = pProj.x() * pProj.x() + pProj.y() * pProj.y();

            float uHit = u0 * (1.0f - tHitUnclamped) + u1 * tHitUnclamped;
            uHit = std::clamp(uHit, 0.0f, 1.0f);

            float radius = (m_width[0] * (1.0f - uHit) + m_width[1] * uHit) * 0.5f;

            if (d2 <= radius * radius) {
                float zHitCenter = cp[0].z() * (1.0f - tHit) + cp[3].z() * tHit;
                float dz = std::sqrt(radius * radius - d2);
                float zHit = zHitCenter - dz / ray.d.length();

                if (zHit > ray.tMin && zHit < tMax) {
                    tMax = zHit;
                    isect.t = zHit;
                    isect.uv = Point2f(uHit, 0.0f);

                    return true;
                }
            }
            return false;
        }

        Point3f cp0[4], cp1[4];
        subdivideBezier(cp, cp0, cp1);
        float uMid = (u0 + u1) * 0.5f;

        bool hit = false;

        if (recursiveIntersect(ray, cp0, rayToObj, u0, uMid, depth + 1, maxDepth, tMax, isect)) {
            hit = true;
        }
        if (recursiveIntersect(ray, cp1, rayToObj, uMid, u1, depth + 1, maxDepth, tMax, isect)) {
            hit = true;
        }

        return hit;
    }

    GND_REGISTER_CLASS(Curve, "curve");
}
