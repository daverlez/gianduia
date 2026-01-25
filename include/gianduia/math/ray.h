#pragma once

#include <gianduia/math/geometry.h>
#include <limits>

#include "constants.h"

namespace gnd {

    class Ray {
    public:
        Point3f o;
        Vector3f d;
        mutable float tMin;
        mutable float tMax; 

        Ray() : tMax(std::numeric_limits<float>::infinity()) {}

        Ray(const Point3f& origin, const Vector3f& direction, float _tMax = std::numeric_limits<float>::infinity())
            : o(origin), d(direction), tMax(_tMax), tMin(Epsilon) {}

        Point3f operator()(float t) const {
            return o + d * t;
        }

        bool hasNaNs() const {
            return (o.hasNaNs() || d.hasNaNs() || std::isnan(tMax));
        }
    };

}
