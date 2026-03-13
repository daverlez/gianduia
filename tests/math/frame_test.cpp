#include <gtest/gtest.h>
#include <gianduia/math/frame.h>

TEST(FrameTest, DefaultConstructor) {
    gnd::Frame f;

    EXPECT_FLOAT_EQ(f.x.x(), 1.0f);
    EXPECT_FLOAT_EQ(f.x.y(), 0.0f);
    EXPECT_FLOAT_EQ(f.x.z(), 0.0f);

    EXPECT_FLOAT_EQ(f.y.x(), 0.0f);
    EXPECT_FLOAT_EQ(f.y.y(), 1.0f);
    EXPECT_FLOAT_EQ(f.y.z(), 0.0f);

    EXPECT_FLOAT_EQ(f.z.x(), 0.0f);
    EXPECT_FLOAT_EQ(f.z.y(), 0.0f);
    EXPECT_FLOAT_EQ(f.z.z(), 1.0f);
}

TEST(FrameTest, FromNormal) {
    gnd::Vector3f n = gnd::Normalize(gnd::Vector3f(1.0f, 2.0f, 3.0f));
    gnd::Frame f(n);

    EXPECT_FLOAT_EQ(f.z.x(), n.x());
    EXPECT_FLOAT_EQ(f.z.y(), n.y());
    EXPECT_FLOAT_EQ(f.z.z(), n.z());

    EXPECT_NEAR(f.x.length(), 1.0f, 1e-6f);
    EXPECT_NEAR(f.y.length(), 1.0f, 1e-6f);

    EXPECT_NEAR(gnd::Dot(f.x, f.y), 0.0f, 1e-6f);
    EXPECT_NEAR(gnd::Dot(f.x, f.z), 0.0f, 1e-6f);
    EXPECT_NEAR(gnd::Dot(f.y, f.z), 0.0f, 1e-6f);
}

TEST(FrameTest, Transformations) {
    gnd::Vector3f n(0.0f, 1.0f, 0.0f);
    gnd::Frame f(n);

    gnd::Vector3f worldDir = gnd::Normalize(gnd::Vector3f(1.0f, 1.0f, 0.0f));
    gnd::Vector3f localDir = f.toLocal(worldDir);

    gnd::Vector3f backToWorld = f.toWorld(localDir);

    EXPECT_NEAR(backToWorld.x(), worldDir.x(), 1e-6f);
    EXPECT_NEAR(backToWorld.y(), worldDir.y(), 1e-6f);
    EXPECT_NEAR(backToWorld.z(), worldDir.z(), 1e-6f);
}

TEST(FrameTest, Trigonometry) {
    gnd::Vector3f w = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));

    float expectedCos = 1.0f / std::sqrt(2.0f);

    EXPECT_NEAR(gnd::Frame::cosTheta(w), expectedCos, 1e-6f);
    EXPECT_NEAR(gnd::Frame::sin2Theta(w), 0.5f, 1e-6f);
    EXPECT_NEAR(gnd::Frame::cosPhi(w), 1.0f, 1e-6f);
    EXPECT_NEAR(gnd::Frame::sinPhi(w), 0.0f, 1e-6f);
}

TEST(FrameTest, SameHemisphere) {
    gnd::Vector3f w1(0.1f, 0.2f, 0.5f);
    gnd::Vector3f w2(-0.1f, 0.5f, 0.1f);
    gnd::Vector3f w3(0.0f, 0.0f, -0.5f);

    EXPECT_TRUE(gnd::Frame::sameHemisphere(w1, w2));
    EXPECT_FALSE(gnd::Frame::sameHemisphere(w1, w3));
}