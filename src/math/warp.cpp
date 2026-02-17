#include <gianduia/math/warp.h>
#include <gianduia/math/constants.h>
#include <cmath>

namespace gnd {

    Point2f Warp::squareToUniformDisk(const Point2f &sample) {
        Vector2f p = 2.0f * sample - Point2f(1.0f);
        float r, theta;

        if (std::abs(p.x()) < Epsilon && std::abs(p.y()) < Epsilon) {
            return {0, 0};
        }

        // Concentric mapping
        if (std::abs(p.x()) > std::abs(p.y())) {
            r = p.x();
            theta = (M_PI / 4.0f) * (p.y() / p.x());
        } else {
            r = p.y();
            theta = (M_PI / 2.0f) - (M_PI / 4.0f) * (p.x() / p.y());
        }

        return {r * std::cos(theta), r * std::sin(theta)};
    }

    float Warp::squareToUniformDiskPdf(const Point2f &p) {
        if (p.lengthSquared() > 1.0f)
            return 0.0f;

        return InvPi;
    }

    Vector3f Warp::squareToUniformSphereCap(const Point2f& sample, float cosThetaMax) {
        float theta = std::acos(1 - (1-cosThetaMax) * sample.x());
        float phi = 2.0f * M_PI * sample.y();

        float x = std::sin(theta) * std::cos(phi);
        float y = std::sin(theta) * std::sin(phi);
        float z = std::cos(theta);

        return {x, y, z};
    }

    float Warp::squareToUniformSphereCapPdf(const Vector3f &v, float cosThetaMax) {
        if (Dot(v, Vector3f(0.0f, 0.0f, 1.0f)) < cosThetaMax || std::abs(1-v.lengthSquared()) > Epsilon)
            return 0.0f;

        return 1 / (2.0f * Pi * (1 - cosThetaMax));
    }

    Vector3f Warp::squareToUniformSphere(const Point2f &sample) {
        return squareToUniformSphereCap(sample, -1.0f);
    }

    float Warp::squareToUniformSpherePdf(const Vector3f &v) {
        return squareToUniformSphereCapPdf(v, -1.0f);
    }

    Vector3f Warp::squareToUniformHemisphere(const Point2f &sample) {
        return squareToUniformSphereCap(sample, 0.0f);
    }

    float Warp::squareToUniformHemispherePdf(const Vector3f &v) {
        return squareToUniformSphereCapPdf(v, 0.0f);
    }

    Vector3f Warp::squareToCosineHemisphere(const Point2f &sample) {
        Point2f pointOnDisk = squareToUniformDisk(sample);
        float x = pointOnDisk.x();
        float y = pointOnDisk.y();

        float r = pointOnDisk.lengthSquared();
        float z = std::sqrt(1 - r);

        return {x, y, z};
    }

    float Warp::squareToCosineHemispherePdf(const Vector3f &v) {
        if (v.z() <= 0.0f || std::abs(1-v.lengthSquared()) > Epsilon)
            return 0.0f;

        return InvPi * v.z();
    }
}
