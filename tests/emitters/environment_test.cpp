#include <gtest/gtest.h>
#include <gianduia/core/factory.h>
#include <gianduia/core/emitter.h>
#include <gianduia/math/warp.h>
#include <gianduia/math/constants.h>
#include <gianduia/core/pcg32.h>
#include <vector>
#include <cmath>
#include <algorithm>
#include <filesystem>

#include <OpenEXR/ImfOutputFile.h>
#include <OpenEXR/ImfChannelList.h>
#include <OpenEXR/ImfHeader.h>
#include <OpenEXR/ImfFrameBuffer.h>

void generateTestEnvMap(const std::string& filename, int width, int height) {
    std::vector<gnd::Color3f> pixels(width * height);

    for (int y = 0; y < height; ++y) {
        for (int x = 0; x < width; ++x) {
            float u = static_cast<float>(x) / width;
            float v = static_cast<float>(y) / height;

            float val = (u > 0.4f && u < 0.6f && v > 0.4f && v < 0.6f) ? 10.0f : 0.1f;
            pixels[y * width + x] = gnd::Color3f(val, val, val);
        }
    }

    Imf::Header header(width, height);
    header.channels().insert("R", Imf::Channel(Imf::FLOAT));
    header.channels().insert("G", Imf::Channel(Imf::FLOAT));
    header.channels().insert("B", Imf::Channel(Imf::FLOAT));

    Imf::OutputFile file(filename.c_str(), header);
    Imf::FrameBuffer frameBuffer;

    char* base = reinterpret_cast<char*>(pixels.data());
    size_t xStride = sizeof(gnd::Color3f);
    size_t yStride = width * sizeof(gnd::Color3f);

    frameBuffer.insert("R", Imf::Slice(Imf::FLOAT, base, xStride, yStride));
    frameBuffer.insert("G", Imf::Slice(Imf::FLOAT, base + sizeof(float), xStride, yStride));
    frameBuffer.insert("B", Imf::Slice(Imf::FLOAT, base + 2 * sizeof(float), xStride, yStride));

    file.setFrameBuffer(frameBuffer);
    file.writePixels(height);
}

/*
TEST(EnvironmentMapTest, ImportanceSamplingStatistical) {
    std::string testExrPath = "generated_test_envmap.exr";

    generateTestEnvMap(testExrPath, 16, 8);

    gnd::PropertyList props;
    props.setString("filename", testExrPath);
    props.setFloat("strength", 1.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("environment", props);
    gnd::Emitter* envMap = dynamic_cast<gnd::Emitter*>(obj.get());
    ASSERT_NE(envMap, nullptr) << "Failed to instantiate environment map.";

    gnd::PCG32 rng(42);
    int binsTheta = 16;
    int binsPhi = 32;
    int numBins = binsTheta * binsPhi;

    std::vector<int> observed(numBins, 0);
    std::vector<float> expectedProb(numBins, 0.0f);

    gnd::SurfaceInteraction ref;
    ref.p = gnd::Point3f(0.0f);

    // Computing expected probability
    int mcIntegrationSamples = 1000000;
    for (int i = 0; i < mcIntegrationSamples; ++i) {
        gnd::Point2f u(rng.nextFloat(), rng.nextFloat());
        gnd::Vector3f w = gnd::Warp::squareToUniformSphere(u);

        float theta = std::acos(std::clamp(w.z(), -1.0f, 1.0f));
        float phi = std::atan2(w.y(), w.x());
        if (phi < 0.0f) phi += 2.0f * gnd::Pi;

        int bTheta = std::clamp(static_cast<int>((theta / gnd::Pi) * binsTheta), 0, binsTheta - 1);
        int bPhi = std::clamp(static_cast<int>((phi / (2.0f * gnd::Pi)) * binsPhi), 0, binsPhi - 1);

        gnd::SurfaceInteraction info;
        info.p = ref.p + w * 1e5f;
        info.n = gnd::Normal3f(-w);

        float pdfVal = envMap->pdf(ref, info);
        float weight = (pdfVal * 4.0f * gnd::Pi) / mcIntegrationSamples;
        expectedProb[bTheta * binsPhi + bPhi] += weight;
    }

    float totalProb = 0.0f;
    for (float p : expectedProb) totalProb += p;
    for (float& p : expectedProb) p /= totalProb;

    // Sampling env map
    int sampleCount = 500000;
    int validSamples = 0;

    for (int i = 0; i < sampleCount; ++i) {
        gnd::Point2f u(rng.nextFloat(), rng.nextFloat());
        gnd::SurfaceInteraction info;
        float pdf;
        gnd::Ray shadowRay;

        gnd::Color3f L = envMap->sample(ref, u, info, pdf, shadowRay);

        if (pdf > 0.0f && !L.isBlack()) {
            gnd::Vector3f w = gnd::Normalize(info.p - ref.p);

            float theta = std::acos(std::clamp(w.z(), -1.0f, 1.0f));
            float phi = std::atan2(w.y(), w.x());
            if (phi < 0.0f) phi += 2.0f * gnd::Pi;

            int bTheta = std::clamp(static_cast<int>((theta / gnd::Pi) * binsTheta), 0, binsTheta - 1);
            int bPhi = std::clamp(static_cast<int>((phi / (2.0f * gnd::Pi)) * binsPhi), 0, binsPhi - 1);

            observed[bTheta * binsPhi + bPhi]++;
            validSamples++;
        }
    }

    // Chi-Squared test
    float chi2 = 0.0f;
    int pooledBins = 0;

    for (int i = 0; i < numBins; ++i) {
        float expectedCount = expectedProb[i] * validSamples;

        if (expectedCount > 10.0f) {
            float diff = static_cast<float>(observed[i]) - expectedCount;
            chi2 += (diff * diff) / expectedCount;
            pooledBins++;
        }
    }

    int df = std::max(1, pooledBins - 1);
    float threshold = df + 6.0f * std::sqrt(2.0f * df);

    std::filesystem::remove(testExrPath);

    EXPECT_LT(chi2, threshold)
        << "Chi-squared test failed! Importance sampling does not match PDF.\n"
        << "Chi2 = " << chi2 << " | Threshold = " << threshold << " | Valid Bins = " << pooledBins;
}
*/
