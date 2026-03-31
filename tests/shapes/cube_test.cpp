#include <gtest/gtest.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/geometry.h>

TEST(CubeTest, FactoryAndBounds) {
    gnd::PropertyList props;
    props.setVector3("extents", gnd::Vector3f(2.0f, 3.0f, 4.0f));

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cube", props);
    gnd::Shape* cube = dynamic_cast<gnd::Shape*>(obj.get());

    ASSERT_NE(cube, nullptr);

    gnd::Bounds3f bounds = cube->getBounds();

    EXPECT_FLOAT_EQ(bounds.pMin.x(), -2.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.y(), -3.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.z(), -4.0f);

    EXPECT_FLOAT_EQ(bounds.pMax.x(), 2.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.y(), 3.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.z(), 4.0f);
}

TEST(CubeTest, RayIntersection) {
    gnd::PropertyList props;
    props.setVector3("extents", gnd::Vector3f(1.0f, 1.0f, 1.0f));

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cube", props);
    gnd::Shape* cube = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Ray ray(gnd::Point3f(5.0f, 0.0f, 0.0f), gnd::Vector3f(-1.0f, 0.0f, 0.0f));
    gnd::SurfaceInteraction isect;

    EXPECT_TRUE(cube->rayIntersect(ray, isect, false));
    cube->fillInteraction(ray, isect);

    EXPECT_FLOAT_EQ(isect.t, 4.0f);
    EXPECT_FLOAT_EQ(isect.p.x(), 1.0f);
    EXPECT_FLOAT_EQ(isect.p.y(), 0.0f);
    EXPECT_FLOAT_EQ(isect.p.z(), 0.0f);
    EXPECT_FLOAT_EQ(isect.n.x(), 1.0f);
    EXPECT_FLOAT_EQ(isect.n.y(), 0.0f);
    EXPECT_FLOAT_EQ(isect.n.z(), 0.0f);
}

TEST(CubeTest, RayMiss) {
    gnd::PropertyList props;
    props.setVector3("extents", gnd::Vector3f(1.0f, 1.0f, 1.0f));

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cube", props);
    gnd::Shape* cube = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Ray ray(gnd::Point3f(5.0f, 5.0f, 0.0f), gnd::Vector3f(-1.0f, 0.0f, 0.0f));
    gnd::SurfaceInteraction isect;

    EXPECT_FALSE(cube->rayIntersect(ray, isect, false));
}

TEST(CubeTest, PdfSurface) {
    gnd::PropertyList props;
    props.setVector3("extents", gnd::Vector3f(1.0f, 2.0f, 3.0f));

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cube", props);
    gnd::Shape* cube = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Point3f ref(10.0f, 0.0f, 0.0f);
    gnd::SurfaceInteraction dummyIsect;

    float pdf = cube->pdfSurface(ref, dummyIsect);

    float expectedArea = 2.0f * (2.0f * 4.0f + 2.0f * 6.0f + 4.0f * 6.0f);
    EXPECT_FLOAT_EQ(pdf, 1.0f / expectedArea);
}

