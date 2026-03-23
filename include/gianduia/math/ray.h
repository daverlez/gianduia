#pragma once

#include <gianduia/math/constants.h>
#include <gianduia/math/geometry.h>

#include <limits>

namespace gnd {

    class Medium;

    class Ray {
    public:
        Point3f o;
        Vector3f d;
        mutable float tMin;
        mutable float tMax;
        const Medium* medium = nullptr;

        Ray() : tMax(std::numeric_limits<float>::infinity()), tMin(Epsilon) {}

        Ray(const Point3f& origin, const Vector3f& direction, float _tMax = Infinity)
            : o(origin), d(direction), tMax(_tMax), tMin(Epsilon) {}
        Ray(const Point3f& o, const Vector3f& d, const Medium* medium = nullptr)
            : o(o), d(d), tMax(Infinity), tMin(Epsilon), medium(medium) {}

        Point3f operator()(float t) const {
            return o + d * t;
        }

        bool hasNaNs() const {
            return (o.hasNaNs() || d.hasNaNs() || std::isnan(tMax));
        }
    };

}
