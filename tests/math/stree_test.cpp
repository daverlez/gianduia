#include <gtest/gtest.h>
#include "gianduia/math/stree.h"
#include "gianduia/math/bounds.h"

using namespace gnd;

class STreeTest : public ::testing::Test {
protected:
    Bounds3f rootBounds;

    void SetUp() override {
        rootBounds = Bounds3f(Point3f(0.0f, 0.0f, 0.0f), Point3f(10.0f, 10.0f, 10.0f));
    }
};

TEST_F(STreeTest, Initialization) {
    STree tree(rootBounds);

    EXPECT_EQ(tree.getNodeCount(), 1);
    EXPECT_FLOAT_EQ(tree.estimateRadiance(Point3f(5.0f, 5.0f, 5.0f)), 0.0f);
}

TEST_F(STreeTest, AccumulateAndRefineNoSplit) {
    STree tree(rootBounds);

    Point3f testPoint(5.0f, 5.0f, 5.0f);
    tree.addSample(testPoint, 5.0f);
    tree.addSample(testPoint, 5.0f);
    tree.addSample(testPoint, 5.0f);
    tree.addSample(testPoint, 5.0f);

    EXPECT_FLOAT_EQ(tree.estimateRadiance(testPoint), 0.0f);
    tree.refine(10);

    EXPECT_FLOAT_EQ(tree.estimateRadiance(testPoint), 5.0f);
    EXPECT_EQ(tree.getNodeCount(), 1);
}

TEST_F(STreeTest, SplitCondition) {
    STree tree(rootBounds);
    Point3f testPoint(5.0f, 5.0f, 5.0f);

    for(int i = 0; i < 6; ++i) {
        tree.addSample(testPoint, 1.0f);
    }

    tree.refine(5);

    EXPECT_EQ(tree.getNodeCount(), 3);
}

TEST_F(STreeTest, SpatialRadianceDistribution) {
    STree tree(rootBounds);

    Point3f darkPoint(2.0f, 5.0f, 5.0f);
    Point3f brightPoint(8.0f, 5.0f, 5.0f);

    for(int i = 0; i < 6; ++i) {
        tree.addSample(darkPoint, 0.0f);
        tree.addSample(brightPoint, 10.0f);
    }

    tree.refine(10);
    tree.clearAccumulators();

    for(int i = 0; i < 20; ++i) {
        tree.addSample(darkPoint, 0.0f);
        tree.addSample(brightPoint, 10.0f);
    }

    tree.refine(100);

    float darkEstimate = tree.estimateRadiance(Point3f(1.0f, 5.0f, 5.0f));
    float brightEstimate = tree.estimateRadiance(Point3f(9.0f, 5.0f, 5.0f));

    EXPECT_LT(darkEstimate, 1.0f);
    EXPECT_GT(brightEstimate, 9.0f);
    EXPECT_GE(tree.getNodeCount(), 3);
}

TEST_F(STreeTest, OutOfBoundsClamp) {
    STree tree(rootBounds);
    Point3f outOfBoundsPoint(15.0f, 15.0f, -5.0f);

    tree.addSample(outOfBoundsPoint, 10.0f);
    tree.refine(5);

    EXPECT_FLOAT_EQ(tree.estimateRadiance(outOfBoundsPoint), 10.0f);
}

TEST_F(STreeTest, DTreeLazyAllocation) {
    STree tree(rootBounds);
    Point3f testPoint(5.0f, 5.0f, 5.0f);

    const STree& constTree = tree;
    EXPECT_EQ(constTree.getDTree(testPoint), nullptr);

    DTree* dtree = tree.getDTree(testPoint);
    ASSERT_NE(dtree, nullptr);

    EXPECT_NE(constTree.getDTree(testPoint), nullptr);
}

TEST_F(STreeTest, DTreeInheritanceOnSplit) {
    STree tree(rootBounds);
    Point3f testPoint(5.0f, 5.0f, 5.0f);

    for(int i = 0; i < 6; ++i)
        tree.addSample(testPoint, 1.0f);

    DTree* rootDTree = tree.getDTree(testPoint);
    rootDTree->addSample(Vector3f(0.0f, 0.0f, 1.0f), 42.0f);

    tree.refine(5);

    Point3f child1Point(2.0f, 5.0f, 5.0f);
    Point3f child2Point(8.0f, 5.0f, 5.0f);

    DTree* child1Tree = tree.getDTree(child1Point);
    DTree* child2Tree = tree.getDTree(child2Point);

    ASSERT_NE(child1Tree, nullptr);
    ASSERT_NE(child2Tree, nullptr);

    EXPECT_NE(child1Tree, child2Tree);

    EXPECT_FLOAT_EQ(child1Tree->getStatisticalWeight(), 42.0f);
    EXPECT_FLOAT_EQ(child2Tree->getStatisticalWeight(), 42.0f);
}
