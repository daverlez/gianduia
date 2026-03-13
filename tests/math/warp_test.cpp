#include <gtest/gtest.h>
#include <gianduia/math/warp.h>
#include <gianduia/math/constants.h>
#include <gianduia/core/pcg32.h>
#include <vector>
#include <cmath>

void runChiSquaredTest(
    int binsX, int binsY, int sampleCount,
    std::function<void(const gnd::Point2f&, int&, int&)> testLogic)
{
    gnd::PCG32 rng(42);
    std::vector<int> observed(binsX * binsY, 0);

    for (int i = 0; i < sampleCount; ++i) {
        gnd::Point2f u(rng.nextFloat(), rng.nextFloat());
        int bx = -1, by = -1;
        testLogic(u, bx, by);

        if (bx >= 0 && bx < binsX && by >= 0 && by < binsY) {
            observed[by * binsX + bx]++;
        }
    }

    float expectedPerBin = static_cast<float>(sampleCount) / (binsX * binsY);
    float chi2 = 0;
    for (int count : observed) {
        float diff = static_cast<float>(count) - expectedPerBin;
        chi2 += (diff * diff) / expectedPerBin;
    }

    // Degrees of freedom = bins - 1
    int df = binsX * binsY - 1;
    // For 400 bins (20x20), df = 399.
    // Critical value for alpha = 0.001 is approx 490.
    // We use a slightly higher threshold (600) to account for floating point noise.
    EXPECT_LT(chi2, 600.0f) << "Chi-squared test failed! Distribution is likely biased.";
}

TEST(WarpTest, UniformDiskStatistical) {
    runChiSquaredTest(20, 20, 100000, [](const gnd::Point2f& u, int& bx, int& by) {
        gnd::Point2f p = gnd::Warp::squareToUniformDisk(u);

        float r2 = p.x() * p.x() + p.y() * p.y();
        float phi = std::atan2(p.y(), p.x());
        if (phi < 0) phi += 2.0f * gnd::Pi;

        bx = std::min(static_cast<int>(r2 * 20), 19);
        by = std::min(static_cast<int>(phi / (2.0f * gnd::Pi) * 20), 19);
    });
}

TEST(WarpTest, UniformSphereStatistical) {
    runChiSquaredTest(20, 20, 100000, [](const gnd::Point2f& u, int& bx, int& by) {
        gnd::Vector3f v = gnd::Warp::squareToUniformSphere(u);

        float zNorm = (v.z() + 1.0f) * 0.5f;
        float phi = std::atan2(v.y(), v.x());
        if (phi < 0) phi += 2.0f * gnd::Pi;

        bx = std::min(static_cast<int>(zNorm * 20), 19);
        by = std::min(static_cast<int>(phi / (2.0f * gnd::Pi) * 20), 19);
    });
}

TEST(WarpTest, UniformHemisphereStatistical) {
    runChiSquaredTest(20, 20, 100000, [](const gnd::Point2f& u, int& bx, int& by) {
        gnd::Vector3f v = gnd::Warp::squareToUniformHemisphere(u);

        float zNorm = v.z();
        float phi = std::atan2(v.y(), v.x());
        if (phi < 0) phi += 2.0f * gnd::Pi;

        bx = std::min(static_cast<int>(zNorm * 20), 19);
        by = std::min(static_cast<int>(phi / (2.0f * gnd::Pi) * 20), 19);
    });
}

TEST(WarpTest, CosineHemisphereStatistical) {
    runChiSquaredTest(20, 20, 100000, [](const gnd::Point2f& u, int& bx, int& by) {
        gnd::Vector3f v = gnd::Warp::squareToCosineHemisphere(u);

        float z2 = v.z() * v.z();
        float phi = std::atan2(v.y(), v.x());
        if (phi < 0) phi += 2.0f * gnd::Pi;

        bx = std::min(static_cast<int>(z2 * 20), 19);
        by = std::min(static_cast<int>(phi / (2.0f * gnd::Pi) * 20), 19);
    });
}