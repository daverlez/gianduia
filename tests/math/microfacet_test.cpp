#include <gtest/gtest.h>
#include <gianduia/math/microfacet.h>
#include <gianduia/math/constants.h>


TEST(MicrofacetTest, RoughnessToAlpha) {
    EXPECT_FLOAT_EQ(gnd::TrowbridgeReitzDistribution::roughnessToAlpha(0.5f), 0.25f);
    EXPECT_FLOAT_EQ(gnd::TrowbridgeReitzDistribution::roughnessToAlpha(1.0f), 1.0f);

    EXPECT_FLOAT_EQ(gnd::TrowbridgeReitzDistribution::roughnessToAlpha(0.0f), 1e-6f);
    EXPECT_FLOAT_EQ(gnd::TrowbridgeReitzDistribution::roughnessToAlpha(1e-5f), 1e-6f);
}

TEST(MicrofacetTest, NormalDistributionFunction) {
    gnd::TrowbridgeReitzDistribution distrib(0.5f);

    gnd::Vector3f normal(0.0f, 0.0f, 1.0f);

    float expectedD = 4.0f * gnd::InvPi;
    EXPECT_FLOAT_EQ(distrib.D(normal), expectedD);

    gnd::Vector3f below(0.0f, 0.0f, -0.5f);
    EXPECT_FLOAT_EQ(distrib.D(below), 0.0f);
}

TEST(MicrofacetTest, MaskingShadowing) {
    gnd::TrowbridgeReitzDistribution distrib(0.5f);

    gnd::Vector3f normal(0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(distrib.G1(normal), 1.0f);
    EXPECT_FLOAT_EQ(distrib.G(normal, normal), 1.0f);

    gnd::Vector3f grazing = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 0.01f));
    EXPECT_LT(distrib.G1(grazing), 0.1f);
}

TEST(MicrofacetTest, SamplingAndPdf) {
    gnd::TrowbridgeReitzDistribution distrib(0.5f);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 1.0f, 1.0f));
    gnd::Point2f sample(0.3f, 0.7f);

    gnd::Vector3f wh = distrib.sample_wh(wo, sample);

    EXPECT_NEAR(wh.length(), 1.0f, 1e-5f);
    EXPECT_GE(wh.z() * wo.z(), 0.0f);

    float calculatedPdf = distrib.pdf(wo, wh);
    float expectedPdf = distrib.D(wh) * std::abs(wh.z());

    EXPECT_FLOAT_EQ(calculatedPdf, expectedPdf);
}