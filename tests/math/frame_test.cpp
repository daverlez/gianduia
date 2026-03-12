#include <gtest/gtest.h>
#include <gianduia/math/frame.h>

// Test the default constructor (identity frame)
TEST(FrameTest, DefaultConstructor) {
    gnd::Frame f;

    // Default X should be [1, 0, 0]
    EXPECT_FLOAT_EQ(f.x.x(), 1.0f);
    EXPECT_FLOAT_EQ(f.x.y(), 0.0f);
    EXPECT_FLOAT_EQ(f.x.z(), 0.0f);

    // Default Y should be [0, 1, 0]
    EXPECT_FLOAT_EQ(f.y.x(), 0.0f);
    EXPECT_FLOAT_EQ(f.y.y(), 1.0f);
    EXPECT_FLOAT_EQ(f.y.z(), 0.0f);

    // Default Z (Normal) should be [0, 0, 1]
    EXPECT_FLOAT_EQ(f.z.x(), 0.0f);
    EXPECT_FLOAT_EQ(f.z.y(), 0.0f);
    EXPECT_FLOAT_EQ(f.z.z(), 1.0f);
}

// Test orthogonal basis generation from a single normal
TEST(FrameTest, FromNormal) {
    // Test with a random normalized vector
    gnd::Vector3f n = gnd::Normalize(gnd::Vector3f(1.0f, 2.0f, 3.0f));
    gnd::Frame f(n);

    // The Z axis must be the normal
    EXPECT_FLOAT_EQ(f.z.x(), n.x());
    EXPECT_FLOAT_EQ(f.z.y(), n.y());
    EXPECT_FLOAT_EQ(f.z.z(), n.z());

    // Check if the basis is orthonormal
    EXPECT_NEAR(f.x.length(), 1.0f, 1e-6f);
    EXPECT_NEAR(f.y.length(), 1.0f, 1e-6f);

    // Check orthogonality (dot products should be zero)
    EXPECT_NEAR(gnd::Dot(f.x, f.y), 0.0f, 1e-6f);
    EXPECT_NEAR(gnd::Dot(f.x, f.z), 0.0f, 1e-6f);
    EXPECT_NEAR(gnd::Dot(f.y, f.z), 0.0f, 1e-6f);
}

// Test transformations between local and world space
TEST(FrameTest, Transformations) {
    gnd::Vector3f n(0.0f, 1.0f, 0.0f); // Normal pointing along Y
    gnd::Frame f(n);

    gnd::Vector3f worldDir = gnd::Normalize(gnd::Vector3f(1.0f, 1.0f, 0.0f));
    gnd::Vector3f localDir = f.toLocal(worldDir);

    // In this frame, the world Y axis is the local Z axis.
    // The world X axis is either local X or Y depending on basis generation.
    // However, toWorld(toLocal(v)) must always return v.
    gnd::Vector3f backToWorld = f.toWorld(localDir);

    EXPECT_NEAR(backToWorld.x(), worldDir.x(), 1e-6f);
    EXPECT_NEAR(backToWorld.y(), worldDir.y(), 1e-6f);
    EXPECT_NEAR(backToWorld.z(), worldDir.z(), 1e-6f);
}

// Test local trigonometric utilities
TEST(FrameTest, Trigonometry) {
    // Vector at 45 degrees between Z and X axes in local space
    gnd::Vector3f w = gnd::Normalize(gnd::Vector3f(1.0f, 0.0f, 1.0f));

    // cos(45 deg) = 1 / sqrt(2)
    float expectedCos = 1.0f / std::sqrt(2.0f);

    EXPECT_NEAR(gnd::Frame::cosTheta(w), expectedCos, 1e-6f);
    EXPECT_NEAR(gnd::Frame::sin2Theta(w), 0.5f, 1e-6f);
    EXPECT_NEAR(gnd::Frame::cosPhi(w), 1.0f, 1e-6f); // On the XZ plane, phi is 0
    EXPECT_NEAR(gnd::Frame::sinPhi(w), 0.0f, 1e-6f);
}

// Test hemisphere check
TEST(FrameTest, SameHemisphere) {
    gnd::Vector3f w1(0.1f, 0.2f, 0.5f);  // Upper
    gnd::Vector3f w2(-0.1f, 0.5f, 0.1f); // Upper
    gnd::Vector3f w3(0.0f, 0.0f, -0.5f); // Lower

    EXPECT_TRUE(gnd::Frame::sameHemisphere(w1, w2));
    EXPECT_FALSE(gnd::Frame::sameHemisphere(w1, w3));
}