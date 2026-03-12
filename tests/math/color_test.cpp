#include <gtest/gtest.h>
#include <gianduia/math/color.h>

// Test initialization and clamp behavior
TEST(Color3fTest, InitializationAndClamp) {
    gnd::Color3f c1;
    EXPECT_TRUE(c1.isBlack()); // Should be [0,0,0] by default

    gnd::Color3f c2(1.5f, -0.5f, 0.5f);
    gnd::Color3f clamped = c2.clamp();

    EXPECT_FLOAT_EQ(clamped.r(), 1.0f);
    EXPECT_FLOAT_EQ(clamped.g(), 0.0f);
    EXPECT_FLOAT_EQ(clamped.b(), 0.5f);
}

// Test luminance using the exact weights defined in color.h
TEST(Color3fTest, Luminance) {
    gnd::Color3f white(1.0f);
    EXPECT_NEAR(white.luminance(), 1.0f, 1e-5f);

    gnd::Color3f red(1.0f, 0.0f, 0.0f);
    EXPECT_FLOAT_EQ(red.luminance(), 0.212671f);
}

// Test the reversibility of the sRGB -> Linear -> sRGB conversion
TEST(Color3fTest, SRGBLinearConversion) {
    gnd::Color3f original(0.5f, 0.1f, 0.9f);

    // Convert back and forth
    gnd::Color3f srgb = original.toSRGB();
    gnd::Color3f convertedBack = srgb.toLinear();

    EXPECT_NEAR(convertedBack.r(), original.r(), 1e-4f);
    EXPECT_NEAR(convertedBack.g(), original.g(), 1e-4f);
    EXPECT_NEAR(convertedBack.b(), original.b(), 1e-4f);
}