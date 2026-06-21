#include <gtest/gtest.h>
#include "gianduia/math/dtree.h"
#include "gianduia/math/constants.h"

using namespace gnd;


TEST(DTreeTest, InitializationAndUniformFallback) {
    DTree dtree;
    EXPECT_FLOAT_EQ(dtree.getStatisticalWeight(), 0.0f);

    Vector3f up(0.0f, 0.0f, 1.0f);
    EXPECT_FLOAT_EQ(dtree.pdf(up), Inv4Pi);

    float pdfOut;
    Vector3f sampledDir = dtree.sample(Point2f(0.5f, 0.5f), pdfOut);
    EXPECT_FLOAT_EQ(pdfOut, Inv4Pi);

    EXPECT_NEAR(sampledDir.x(), -1.0f, 1e-4f);
    EXPECT_NEAR(sampledDir.y(), 0.0f, 1e-4f);
    EXPECT_NEAR(sampledDir.z(), 0.0f, 1e-4f);
}

TEST(DTreeTest, AccumulateAndRefine) {
    DTree dtree;
    Vector3f lightDir(0.0f, 0.0f, 1.0f);

    for (int i = 0; i < 100; ++i) dtree.addSample(lightDir, 1.0f);
    EXPECT_FLOAT_EQ(dtree.getStatisticalWeight(), 100.0f);

    dtree.refine(0.01f);
    dtree.clearAccumulators();

    for (int i = 0; i < 100; ++i) dtree.addSample(lightDir, 1.0f);
    dtree.refine(0.01f);

    float pdfUp = dtree.pdf(lightDir);
    float pdfDown = dtree.pdf(Vector3f(0.0f, 0.0f, -1.0f));

    EXPECT_GT(pdfUp, pdfDown);
    EXPECT_GT(pdfUp, Inv4Pi);
}

TEST(DTreeTest, ClearAccumulators) {
    DTree dtree;
    dtree.addSample(Vector3f(0.0f, 1.0f, 0.0f), 50.0f);
    EXPECT_FLOAT_EQ(dtree.getStatisticalWeight(), 50.0f);
    
    dtree.clearAccumulators();
    EXPECT_FLOAT_EQ(dtree.getStatisticalWeight(), 0.0f);
}

TEST(DTreeTest, SamplingFavorsEnergy) {
    DTree dtree;
    Vector3f lightDir(1.0f, 0.0f, 0.0f); 

    for (int i = 0; i < 100; ++i) {
        dtree.addSample(lightDir, 10.0f);
    }

    dtree.refine(0.01f);
    dtree.clearAccumulators();
    for (int i = 0; i < 100; ++i) {
        dtree.addSample(lightDir, 10.0f);
    }
    dtree.refine(0.01f);

    float outPdf;
    Vector3f sampledDir = dtree.sample(Point2f(0.25, 0.25), outPdf);

    EXPECT_GT(sampledDir.x(), 0.0f);
    EXPECT_GT(outPdf, Inv4Pi);
}