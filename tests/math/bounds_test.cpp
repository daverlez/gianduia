#include <gtest/gtest.h>
#include <gianduia/math/bounds.h>
#include <gianduia/math/ray.h>

// Test construction and ensuring pMin/pMax are correctly ordered
TEST(Bounds3fTest, ConstructionAndOrdering) {
    gnd::Bounds3f emptyBounds;
    // Default bounds should have inverted min/max
    EXPECT_GT(emptyBounds.pMin.x(), emptyBounds.pMax.x());

    gnd::Point3f p1(2.0f, -1.0f, 5.0f);
    gnd::Point3f p2(-3.0f, 4.0f, 1.0f);
    gnd::Bounds3f b(p1, p2);

    // The constructor must automatically sort the coordinates
    EXPECT_FLOAT_EQ(b.pMin.x(), -3.0f);
    EXPECT_FLOAT_EQ(b.pMax.x(), 2.0f);
    EXPECT_FLOAT_EQ(b.pMin.y(), -1.0f);
    EXPECT_FLOAT_EQ(b.pMax.y(), 4.0f);
    EXPECT_FLOAT_EQ(b.pMin.z(), 1.0f);
    EXPECT_FLOAT_EQ(b.pMax.z(), 5.0f);
}

// Test volume, surface area, and maximum extent
TEST(Bounds3fTest, GeometricProperties) {
    gnd::Bounds3f b(gnd::Point3f(0.0f, 0.0f, 0.0f), gnd::Point3f(2.0f, 3.0f, 4.0f));

    EXPECT_FLOAT_EQ(b.volume(), 24.0f); // 2 * 3 * 4
    EXPECT_FLOAT_EQ(b.surfaceArea(), 52.0f); // 2 * (6 + 8 + 12)
    EXPECT_EQ(b.maximumExtent(), 2); // Z axis is the longest (length 4.0)

    gnd::Point3f centroid = b.centroid();
    EXPECT_FLOAT_EQ(centroid.x(), 1.0f);
    EXPECT_FLOAT_EQ(centroid.y(), 1.5f);
    EXPECT_FLOAT_EQ(centroid.z(), 2.0f);
}

// Test the union operations (merging boxes and points)
TEST(Bounds3fTest, UnionOperations) {
    gnd::Bounds3f b1(gnd::Point3f(0.0f, 0.0f, 0.0f), gnd::Point3f(1.0f, 1.0f, 1.0f));
    gnd::Point3f p(2.0f, -1.0f, 0.5f);

    gnd::Bounds3f bUnion = gnd::Union(b1, p);
    EXPECT_FLOAT_EQ(bUnion.pMin.y(), -1.0f);
    EXPECT_FLOAT_EQ(bUnion.pMax.x(), 2.0f);

    gnd::Bounds3f b2(gnd::Point3f(-2.0f, 0.0f, 0.0f), gnd::Point3f(0.5f, 2.0f, 2.0f));
    gnd::Bounds3f bUnionBoxes = gnd::Union(b1, b2);

    EXPECT_FLOAT_EQ(bUnionBoxes.pMin.x(), -2.0f);
    EXPECT_FLOAT_EQ(bUnionBoxes.pMax.y(), 2.0f);
    EXPECT_FLOAT_EQ(bUnionBoxes.pMax.z(), 2.0f);
}

// Test ray-box intersection with standard and edge cases
TEST(Bounds3fTest, RayIntersection) {
    gnd::Bounds3f b(gnd::Point3f(-1.0f, -1.0f, -1.0f), gnd::Point3f(1.0f, 1.0f, 1.0f));

    // Case 1: Standard hit from outside
    gnd::Ray rHit(gnd::Point3f(-3.0f, 0.0f, 0.0f), gnd::Vector3f(1.0f, 0.0f, 0.0f));
    float t0 = 0.0f, t1 = 0.0f;
    EXPECT_TRUE(b.rayIntersect(rHit, &t0, &t1));
    EXPECT_FLOAT_EQ(t0, 2.0f); // Enters at x = -1
    EXPECT_FLOAT_EQ(t1, 4.0f); // Exits at x = 1

    // Case 2: Complete miss (parallel but outside)
    gnd::Ray rMiss(gnd::Point3f(-3.0f, 2.0f, 0.0f), gnd::Vector3f(1.0f, 0.0f, 0.0f));
    EXPECT_FALSE(b.rayIntersect(rMiss));

    // Case 3: Ray originates *inside* the box
    gnd::Ray rInside(gnd::Point3f(0.0f, 0.0f, 0.0f), gnd::Vector3f(0.0f, 1.0f, 0.0f));
    EXPECT_TRUE(b.rayIntersect(rInside, &t0, &t1));
    EXPECT_FLOAT_EQ(t0, 0.0f);
    EXPECT_FLOAT_EQ(t1, 1.0f); // Exits at y = 1

    // Case 4: Ray hits but exceeds tMax
    gnd::Ray rShort(gnd::Point3f(-3.0f, 0.0f, 0.0f), gnd::Vector3f(1.0f, 0.0f, 0.0f), 1.0f);
    EXPECT_FALSE(b.rayIntersect(rShort));
}