#include <gtest/gtest.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/geometry.h>
#include <gianduia/math/constants.h>

TEST(SphereTest, FactoryAndBounds) {
    gnd::PropertyList props;
    props.setFloat("radius", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("sphere", props);
    gnd::Shape* sphere = dynamic_cast<gnd::Shape*>(obj.get());

    ASSERT_NE(sphere, nullptr);

    gnd::Bounds3f bounds = sphere->getBounds();

    EXPECT_FLOAT_EQ(bounds.pMin.x(), -2.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.y(), -2.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.z(), -2.0f);

    EXPECT_FLOAT_EQ(bounds.pMax.x(), 2.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.y(), 2.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.z(), 2.0f);
}

TEST(SphereTest, RayIntersectionHit) {
    gnd::PropertyList props;
    props.setFloat("radius", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("sphere", props);
    gnd::Shape* sphere = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(sphere, nullptr);

    gnd::Ray ray(gnd::Point3f(0.0f, 0.0f, 5.0f), gnd::Vector3f(0.0f, 0.0f, -1.0f));
    gnd::SurfaceInteraction isect;

    bool hit = sphere->rayIntersect(ray, isect, false);
    EXPECT_TRUE(hit);
    sphere->fillInteraction(ray, isect);

    EXPECT_FLOAT_EQ(isect.t, 3.0f);
    EXPECT_FLOAT_EQ(isect.p.x(), 0.0f);
    EXPECT_FLOAT_EQ(isect.p.y(), 0.0f);
    EXPECT_FLOAT_EQ(isect.p.z(), 2.0f);

    EXPECT_FLOAT_EQ(isect.n.x(), 0.0f);
    EXPECT_FLOAT_EQ(isect.n.y(), 0.0f);
    EXPECT_FLOAT_EQ(isect.n.z(), 1.0f);
}

TEST(SphereTest, RayIntersectionMiss) {
    gnd::PropertyList props;
    props.setFloat("radius", 1.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("sphere", props);
    gnd::Shape* sphere = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(sphere, nullptr);

    gnd::Ray ray(gnd::Point3f(5.0f, 0.0f, 0.0f), gnd::Vector3f(0.0f, 1.0f, 0.0f));
    gnd::SurfaceInteraction isect;

    bool hit = sphere->rayIntersect(ray, isect, false);

    EXPECT_FALSE(hit);
}

TEST(SphereTest, PdfSurface) {
    gnd::PropertyList props;
    props.setFloat("radius", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("sphere", props);
    gnd::Shape* sphere = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(sphere, nullptr);

    gnd::Point3f ref(10.0f, 0.0f, 0.0f);
    gnd::SurfaceInteraction dummyIsect;

    float pdf = sphere->pdfSurface(ref, dummyIsect);
    float expectedPdf = gnd::Inv4Pi / 4.0f;

    EXPECT_FLOAT_EQ(pdf, expectedPdf);
}