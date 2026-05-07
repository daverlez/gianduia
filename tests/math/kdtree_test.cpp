#include <gtest/gtest.h>
#include <gianduia/math/kdtree.h>
#include <gianduia/math/geometry.h>
#include <vector>
#include <random>
#include <algorithm>

using namespace gnd;

struct TestPhoton {
    Point3f p;
    int id;
    uint16_t kdFlags = 0;

    bool operator==(const TestPhoton& other) const {
        return id == other.id;
    }

    void setAxis(int axis) { kdFlags = (kdFlags & ~3) | (axis & 3); }
    int getAxis() const { return kdFlags & 3; }
};

TEST(KdTreeTest, EmptyTree) {
    KdTree<TestPhoton> tree;
    EXPECT_TRUE(tree.isEmpty());
    EXPECT_EQ(tree.getNodes().size(), 0);

    std::vector<TestPhoton> found;
    TestPhoton query{{0.0f, 0.0f, 0.0f}, -1};

    tree.searchRadius(query, 10.0f, [&](const TestPhoton& pt, float dist2) {
        found.push_back(pt);
    });

    EXPECT_TRUE(found.empty());
}

TEST(KdTreeTest, BasicRadiusSearch) {
    std::vector<TestPhoton> points = {
        {{0.0f, 0.0f, 0.0f}, 0},
        {{1.0f, 0.0f, 0.0f}, 1},
        {{0.0f, 1.0f, 0.0f}, 2},
        {{0.0f, 0.0f, 1.0f}, 3},
        {{10.0f, 10.0f, 10.0f}, 4}
    };

    KdTree<TestPhoton> tree;
    tree.build(points);
    EXPECT_FALSE(tree.isEmpty());
    EXPECT_EQ(tree.getNodes().size(), 5);

    std::vector<int> foundIds;
    TestPhoton query{{0.1f, 0.1f, 0.1f}, -1};

    tree.searchRadius(query, 1.5f, [&](const TestPhoton& pt, float dist2) {
        foundIds.push_back(pt.id);
    });

    EXPECT_EQ(foundIds.size(), 4);
    EXPECT_TRUE(std::find(foundIds.begin(), foundIds.end(), 0) != foundIds.end());
    EXPECT_TRUE(std::find(foundIds.begin(), foundIds.end(), 4) == foundIds.end());
}

TEST(KdTreeTest, BoundaryConditions) {
    std::vector<TestPhoton> points = {
        {{2.0f, 0.0f, 0.0f}, 1}
    };

    KdTree<TestPhoton> tree;
    tree.build(points);

    TestPhoton query{{0.0f, 0.0f, 0.0f}, 0};

    int countOutside = 0;
    tree.searchRadius(query, 1.99f, [&](const TestPhoton& pt, float dist2) { countOutside++; });
    EXPECT_EQ(countOutside, 0);

    int countExact = 0;
    tree.searchRadius(query, 2.0f, [&](const TestPhoton& pt, float dist2) { countExact++; });
    EXPECT_EQ(countExact, 1);

    int countInside = 0;
    tree.searchRadius(query, 2.01f, [&](const TestPhoton& pt, float dist2) { countInside++; });
    EXPECT_EQ(countInside, 1);
}

TEST(KdTreeTest, RebuildClearsPreviousData) {
    KdTree<TestPhoton> tree;
    
    std::vector<TestPhoton> set1 = { {{1.0f, 1.0f, 1.0f}, 1} };
    tree.build(set1);
    EXPECT_EQ(tree.getNodes().size(), 1);

    std::vector<TestPhoton> set2 = { {{2.0f, 2.0f, 2.0f}, 2}, {{3.0f, 3.0f, 3.0f}, 3} };
    tree.build(set2);
    EXPECT_EQ(tree.getNodes().size(), 2);

    TestPhoton query{{1.0f, 1.0f, 1.0f}, 0};
    int found = 0;
    tree.searchRadius(query, 0.5f, [&](const TestPhoton& pt, float dist2) { found++; });

    EXPECT_EQ(found, 0); 
}
