#include <gtest/gtest.h>
#include <gianduia/materials/reflection.h>
#include <gianduia/materials/fresnel.h>
#include <gianduia/math/constants.h>
#include <gianduia/math/microfacet.h>

TEST(ReflectionTest, Lambertian) {
    gnd::Color3f albedo(0.9f, 0.1f, 0.1f);
    gnd::LambertianReflection lambert(albedo);

    gnd::Vector3f wo(0.0f, 0.0f, 1.0f);
    gnd::Vector3f wi = gnd::Normalize(gnd::Vector3f(1.0f, 1.0f, 1.0f));

    gnd::Color3f f_val = lambert.f(wo, wi);
    EXPECT_FLOAT_EQ(f_val.r(), albedo.r() * gnd::InvPi);
    EXPECT_FLOAT_EQ(f_val.g(), albedo.g() * gnd::InvPi);
    EXPECT_FLOAT_EQ(f_val.b(), albedo.b() * gnd::InvPi);

    float expected_pdf = std::max(0.0f, wi.z() * gnd::InvPi);
    EXPECT_FLOAT_EQ(lambert.pdf(wo, wi), expected_pdf);

    gnd::Vector3f wi_below(0.0f, 0.0f, -1.0f);
    EXPECT_FLOAT_EQ(lambert.pdf(wo, wi_below), 0.0f);
}

TEST(ReflectionTest, Specular) {
    gnd::Color3f R(0.8f);
    gnd::FresnelNoOp fresnel;
    gnd::SpecularReflection specular(R, &fresnel);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Vector3f wi_dummy(0.0f, 0.0f, 1.0f);

    EXPECT_FLOAT_EQ(specular.f(wo, wi_dummy).r(), 0.0f);
    EXPECT_FLOAT_EQ(specular.pdf(wo, wi_dummy), 0.0f);

    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::Point2f sample(0.5f, 0.5f);
    gnd::BxDFType sampledType;

    gnd::Color3f f_sampled = specular.sample(wo, wi_sampled, sample, 0.5f, pdf, &sampledType);

    EXPECT_FLOAT_EQ(wi_sampled.x(), -wo.x());
    EXPECT_FLOAT_EQ(wi_sampled.y(), -wo.y());
    EXPECT_FLOAT_EQ(wi_sampled.z(), wo.z());

    EXPECT_FLOAT_EQ(pdf, 1.0f);

    EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);
    EXPECT_TRUE(sampledType & gnd::BSDF_SPECULAR);
}

TEST(ReflectionTest, FresnelSpecular) {
    gnd::Color3f R(1.0f), T(1.0f);
    gnd::FresnelSpecular glass(R, T, 1.0f, 1.5f);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::BxDFType sampledType;

    glass.sample(wo, wi_sampled, gnd::Point2f(0.0f, 0.5f), 0.5f, pdf, &sampledType);

    EXPECT_FLOAT_EQ(wi_sampled.x(), -wo.x());
    EXPECT_FLOAT_EQ(wi_sampled.y(), -wo.y());
    EXPECT_FLOAT_EQ(wi_sampled.z(), wo.z());

    EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);
    EXPECT_TRUE(sampledType & gnd::BSDF_SPECULAR);

    EXPECT_FLOAT_EQ(glass.f(wo, wi_sampled).r(), 0.0f);
    EXPECT_FLOAT_EQ(glass.pdf(wo, wi_sampled), 0.0f);
}

TEST(ReflectionTest, MicrofacetReflection) {
    gnd::Color3f R(0.8f);
    gnd::TrowbridgeReitzDistribution distrib(0.5f);
    gnd::FresnelNoOp fresnel;

    gnd::MicrofacetReflection microfacet(R, &distrib, &fresnel);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Vector3f wi = gnd::Normalize(gnd::Vector3f(-1.0f, 0.0f, 1.0f));

    gnd::Color3f f_forward = microfacet.f(wo, wi);
    gnd::Color3f f_backward = microfacet.f(wi, wo);
    EXPECT_FLOAT_EQ(f_forward.r(), f_backward.r());
    EXPECT_GT(f_forward.r(), 0.0f);

    gnd::Vector3f wi_sampled;
    float pdf;

    gnd::BxDFType sampledType = gnd::BxDFType(0);

    gnd::Point2f random_sample(0.1f, 0.1f);

    gnd::Color3f weight = microfacet.sample(wo, wi_sampled, random_sample, 0.5f, pdf, &sampledType);

    if (pdf > 0.0f) {
        EXPECT_GT(wi_sampled.z() * wo.z(), 0.0f);
        EXPECT_TRUE(sampledType & gnd::BSDF_GLOSSY);
        EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);

        gnd::Color3f expected_f = microfacet.f(wo, wi_sampled);
        float expected_pdf = microfacet.pdf(wo, wi_sampled);

        EXPECT_FLOAT_EQ(pdf, expected_pdf);

        gnd::Color3f expected_weight = expected_f * std::abs(wi_sampled.z()) / pdf;
        EXPECT_FLOAT_EQ(weight.r(), expected_weight.r());
    }
}

TEST(ReflectionTest, MicrofacetFresnel) {
    gnd::Color3f R(0.8f), T(0.8f);
    gnd::TrowbridgeReitzDistribution distrib(0.5f);
    gnd::MicrofacetFresnel microfacet(R, T, 1.0f, 1.5f, &distrib);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::BxDFType sampledType;
    gnd::Point2f random_sample(0.1f, 0.1f);

    microfacet.sample(wo, wi_sampled, random_sample, 0.01f, pdf, &sampledType);
    if (pdf > 0.0f) {
        EXPECT_TRUE(sampledType & gnd::BSDF_GLOSSY);
        EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);
        EXPECT_FLOAT_EQ(pdf, microfacet.pdf(wo, wi_sampled));
    }

    microfacet.sample(wo, wi_sampled, random_sample, 0.99f, pdf, &sampledType);
    if (pdf > 0.0f) {
        EXPECT_TRUE(sampledType & gnd::BSDF_GLOSSY);
        EXPECT_TRUE(sampledType & gnd::BSDF_TRANSMISSION);
        EXPECT_FLOAT_EQ(pdf, microfacet.pdf(wo, wi_sampled));
    }
}

TEST(ReflectionTest, SmoothPlastic) {
    gnd::Color3f Kd(0.5f), Ks(0.5f);
    gnd::SmoothPlasticBxDF plastic(Kd, Ks, 1.5f);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(0.0f, 0.0f, 1.0f));
    gnd::Vector3f wi = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));

    float Fo = gnd::FrDielectric(wo.z(), 1.0f, 1.5f);
    float Fi = gnd::FrDielectric(wi.z(), 1.0f, 1.5f);
    gnd::Color3f expected_f = Kd * gnd::InvPi * (1.0f - Fo) * (1.0f - Fi);

    EXPECT_FLOAT_EQ(plastic.f(wo, wi).r(), expected_f.r());

    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::BxDFType sampledType;

    plastic.sample(wo, wi_sampled, gnd::Point2f(0.5f, 0.5f), 0.01f, pdf, &sampledType);
    EXPECT_TRUE(sampledType & gnd::BSDF_SPECULAR);
    EXPECT_FLOAT_EQ(wi_sampled.x(), -wo.x());

    plastic.sample(wo, wi_sampled, gnd::Point2f(0.5f, 0.5f), 0.99f, pdf, &sampledType);
    EXPECT_TRUE(sampledType & gnd::BSDF_DIFFUSE);
}

TEST(ReflectionTest, RoughPlastic) {
    gnd::Color3f Kd(0.5f), Ks(0.5f);
    gnd::TrowbridgeReitzDistribution distrib(0.5f);
    gnd::RoughPlasticBxDF plastic(Kd, Ks, 1.5f, &distrib);

    gnd::Vector3f wo = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));
    gnd::Vector3f wi_sampled;
    float pdf;
    gnd::BxDFType sampledType = gnd::BxDFType(0);

    plastic.sample(wo, wi_sampled, gnd::Point2f(0.2f, 0.2f), 0.01f, pdf, &sampledType);
    if (pdf > 0.0f) {
        EXPECT_TRUE(sampledType & gnd::BSDF_GLOSSY);
        EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);
        EXPECT_FLOAT_EQ(pdf, plastic.pdf(wo, wi_sampled));
    }

    plastic.sample(wo, wi_sampled, gnd::Point2f(0.5f, 0.5f), 0.99f, pdf, &sampledType);
    if (pdf > 0.0f) {
        EXPECT_TRUE(sampledType & gnd::BSDF_DIFFUSE);
        EXPECT_TRUE(sampledType & gnd::BSDF_REFLECTION);
        EXPECT_FLOAT_EQ(pdf, plastic.pdf(wo, wi_sampled));
    }
}