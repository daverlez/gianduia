#include <gtest/gtest.h>
#include <gianduia/math/photon.h>
#include <cmath>

using namespace gnd;

class PhotonTest : public ::testing::Test {
protected:
    void SetUp() override {
        Photon::initTables();
    }
};

TEST_F(PhotonTest, MemoryFootprint) {
    EXPECT_EQ(sizeof(Photon), 20);
    EXPECT_EQ(sizeof(typename std::vector<Photon>::value_type), 20);
}

TEST_F(PhotonTest, BlackPowerCompression) {
    Point3f pos(0.0f, 0.0f, 0.0f);
    Vector3f dir(0.0f, 0.0f, 1.0f);
    Color3f black(0.0f, 0.0f, 0.0f);

    Photon p(pos, black, dir);
    Color3f unpacked = p.getPower();

    EXPECT_FLOAT_EQ(unpacked.r(), 0.0f);
    EXPECT_FLOAT_EQ(unpacked.g(), 0.0f);
    EXPECT_FLOAT_EQ(unpacked.b(), 0.0f);

    EXPECT_EQ(p.rgbe[3], 0);
}

TEST_F(PhotonTest, PowerCompressionRoundTrip) {
    Point3f pos(0.0f, 0.0f, 0.0f);
    Vector3f dir(0.0f, 0.0f, 1.0f);

    std::vector<Color3f> testColors = {
        Color3f(1.0f, 0.5f, 0.25f),
        Color3f(1500.0f, 300.0f, 0.1f),
        Color3f(0.001f, 0.005f, 0.002f)
    };

    for (const auto& original : testColors) {
        Photon p(pos, original, dir);
        Color3f unpacked = p.getPower();

        float maxVal = std::max({original.r(), original.g(), original.b()});
        float tolerance = maxVal * 0.01f;

        EXPECT_NEAR(unpacked.r(), original.r(), tolerance);
        EXPECT_NEAR(unpacked.g(), original.g(), tolerance);
        EXPECT_NEAR(unpacked.b(), original.b(), tolerance + 1e-4f);
    }
}

TEST_F(PhotonTest, DirectionCompressionRoundTrip) {
    Point3f pos(0.0f, 0.0f, 0.0f);
    Color3f power(1.0f, 1.0f, 1.0f);

    std::vector<Vector3f> testDirs = {
        Vector3f(0.0f, 0.0f, 1.0f),
        Vector3f(0.0f, 0.0f, -1.0f),
        Vector3f(1.0f, 0.0f, 0.0f),
        Vector3f(0.0f, 1.0f, 0.0f),
        Normalize(Vector3f(0.577f, 0.577f, 0.577f)),
        Normalize(Vector3f(-0.8f, 0.1f, -0.5f))
    };

    for (const auto& original : testDirs) {
        Photon p(pos, power, original);
        Vector3f unpacked = p.getDirection();

        EXPECT_NEAR(unpacked.length(), 1.0f, 1e-5f);

        float dotProduct = original.x() * unpacked.x() +
                           original.y() * unpacked.y() +
                           original.z() * unpacked.z();

        dotProduct = std::clamp(dotProduct, -1.0f, 1.0f);
        float angleDiffDegrees = std::acos(dotProduct) * (180.0f / M_PI);

        EXPECT_LT(angleDiffDegrees, 1.5f);
    }
}