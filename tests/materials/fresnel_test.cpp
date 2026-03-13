#include <gtest/gtest.h>
#include <gianduia/materials/fresnel.h>
#include <gianduia/math/geometry.h>
#include <cmath>

TEST(FresnelTest, FrDielectric) {
    float etaExt = 1.0f;
    float etaInt = 1.5f;

    EXPECT_FLOAT_EQ(gnd::FrDielectric(1.0f, etaExt, etaInt), 0.04f);
    EXPECT_FLOAT_EQ(gnd::FrDielectric(-1.0f, etaExt, etaInt), 0.04f);
    EXPECT_NEAR(gnd::FrDielectric(0.0f, etaExt, etaInt), 1.0f, 1e-6f);

    float cosThetaInsideTIR = -0.5f;
    EXPECT_FLOAT_EQ(gnd::FrDielectric(cosThetaInsideTIR, etaExt, etaInt), 1.0f);
}

TEST(FresnelTest, Refract) {
    gnd::Vector3f wi(0.0f, 0.0f, -1.0f);
    gnd::Normal3f n(0.0f, 0.0f, 1.0f);
    gnd::Vector3f wt;

    bool refracted = gnd::Refract(wi, n, 1.5f, &wt);
    EXPECT_TRUE(refracted);
    EXPECT_FLOAT_EQ(wt.x(), 0.0f);
    EXPECT_FLOAT_EQ(wt.y(), 0.0f);
    EXPECT_FLOAT_EQ(wt.z(), -1.0f);

    wi = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, -1.0f));
    float etaRatio = 1.0f / 1.5f;

    refracted = gnd::Refract(wi, n, etaRatio, &wt);
    EXPECT_TRUE(refracted);

    float sinThetaI = wi.x();
    float expectedSinThetaT = etaRatio * sinThetaI;

    EXPECT_NEAR(wt.length(), 1.0f, 1e-5f);
    EXPECT_NEAR(std::abs(wt.x()), expectedSinThetaT, 1e-5f);

    gnd::Vector3f wiInside = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Normal3f nInside(0.0f, 0.0f, -1.0f);
    float etaRatioOut = 1.5f / 1.0f;

    refracted = gnd::Refract(wiInside, nInside, etaRatioOut, &wt);
    EXPECT_FALSE(refracted);
}

TEST(FresnelTest, FrConductor) {
    gnd::Color3f etaI(1.0f);
    gnd::Color3f etaT(1.5f);
    gnd::Color3f k(2.0f);

    gnd::FresnelConductor fresnel(etaI, etaT, k);

    gnd::Color3f rNormal = fresnel.evaluate(1.0f);
    EXPECT_NEAR(rNormal.r(), 0.414634f, 1e-5f);
    EXPECT_NEAR(rNormal.g(), 0.414634f, 1e-5f);
    EXPECT_NEAR(rNormal.b(), 0.414634f, 1e-5f);

    gnd::Color3f rGrazing = fresnel.evaluate(0.0f);
    EXPECT_FLOAT_EQ(rGrazing.r(), 1.0f);
    EXPECT_FLOAT_EQ(rGrazing.g(), 1.0f);
    EXPECT_FLOAT_EQ(rGrazing.b(), 1.0f);
}