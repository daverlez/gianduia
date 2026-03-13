#include <gtest/gtest.h>
#include <gianduia/materials/transmission.h>
#include <gianduia/materials/fresnel.h>
#include <gianduia/math/geometry.h>

TEST(TransmissionTest, SpecularTransmission) {
    gnd::Color3f T(0.9f);
    float etaExt = 1.0f;
    float etaInt = 1.5f;
    gnd::SpecularTransmission glassTrans(T, etaExt, etaInt);

    gnd::Vector3f wo_enter(0.0f, 0.0f, 1.0f);
    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::BxDFType sampledType;
    gnd::Point2f sample(0.5f, 0.5f);

    EXPECT_FLOAT_EQ(glassTrans.f(wo_enter, gnd::Vector3f(0.0f, 0.0f, -1.0f)).r(), 0.0f);
    EXPECT_FLOAT_EQ(glassTrans.pdf(wo_enter, gnd::Vector3f(0.0f, 0.0f, -1.0f)), 0.0f);

    gnd::Color3f weight = glassTrans.sample(wo_enter, wi_sampled, sample, 0.5f, pdf, &sampledType);

    EXPECT_FLOAT_EQ(wi_sampled.x(), 0.0f);
    EXPECT_FLOAT_EQ(wi_sampled.y(), 0.0f);
    EXPECT_FLOAT_EQ(wi_sampled.z(), -1.0f);
    EXPECT_FLOAT_EQ(pdf, 1.0f); // Dirac delta PDF
    EXPECT_TRUE(sampledType & gnd::BSDF_TRANSMISSION);
    EXPECT_TRUE(sampledType & gnd::BSDF_SPECULAR);

    float expected_F = gnd::FrDielectric(1.0f, etaExt, etaInt);
    float etaRatioEnter = etaExt / etaInt;
    float expected_weight_enter = T.r() * (1.0f - expected_F) * (etaRatioEnter * etaRatioEnter);
    EXPECT_FLOAT_EQ(weight.r(), expected_weight_enter);

    gnd::Vector3f wo_exit(0.0f, 0.0f, -1.0f);
    weight = glassTrans.sample(wo_exit, wi_sampled, sample, 0.5f, pdf, &sampledType);

    EXPECT_FLOAT_EQ(wi_sampled.x(), 0.0f);
    EXPECT_FLOAT_EQ(wi_sampled.y(), 0.0f);
    EXPECT_FLOAT_EQ(wi_sampled.z(), 1.0f);

    float etaRatioExit = etaInt / etaExt;
    float expected_weight_exit = T.r() * (1.0f - expected_F) * (etaRatioExit * etaRatioExit);
    EXPECT_FLOAT_EQ(weight.r(), expected_weight_exit);

    gnd::Vector3f wo_exit_shallow = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, -0.1f));
    gnd::Color3f weight_tir = glassTrans.sample(wo_exit_shallow, wi_sampled, sample, 0.5f, pdf, &sampledType);

    EXPECT_FLOAT_EQ(weight_tir.r(), 0.0f);
    EXPECT_FLOAT_EQ(weight_tir.g(), 0.0f);
    EXPECT_FLOAT_EQ(weight_tir.b(), 0.0f);
}