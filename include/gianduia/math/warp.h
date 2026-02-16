#pragma once

#include <gianduia/math/geometry.h>

namespace gnd {

    class Warp {
    public:
        static Point2f squareToUniformDisk(const Point2f& sample);
        static float squareToUniformDiskPdf(const Point2f& p);

        static Vector3f squareToUniformSphereCap(const Point2f& sample, float cosThetaMax);
        static float squareToUniformSphereCapPdf(const Vector3f& v, float cosThetaMax);

        static Vector3f squareToUniformSphere(const Point2f& sample);
        static float squareToUniformSpherePdf(const Vector3f& v);

        static Vector3f squareToUniformHemisphere(const Point2f& sample);
        static float squareToUniformHemispherePdf(const Vector3f& v);

        static Vector3f squareToCosineHemisphere(const Point2f& sample);
        static float squareToCosineHemispherePdf(const Vector3f& v);
    };

}