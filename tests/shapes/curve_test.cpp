#include <gtest/gtest.h>
#include <gianduia/shapes/curve.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/geometry.h>

TEST(CurveTest, FactoryAndBounds) {
    gnd::PropertyList props;
    props.setPoint3("p0", gnd::Point3f(0.0f, 0.0f, 0.0f));
    props.setPoint3("p1", gnd::Point3f(0.0f, 1.0f, 0.0f));
    props.setPoint3("p2", gnd::Point3f(0.0f, 2.0f, 0.0f));
    props.setPoint3("p3", gnd::Point3f(0.0f, 3.0f, 0.0f));

    props.setFloat("w0", 2.0f);
    props.setFloat("w1", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("curve", props);
    gnd::Shape* curve = dynamic_cast<gnd::Shape*>(obj.get());

    ASSERT_NE(curve, nullptr);

    gnd::Bounds3f bounds = curve->getBounds();

    EXPECT_FLOAT_EQ(bounds.pMin.x(), -1.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.z(), -1.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.x(), 1.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.z(), 1.0f);

    EXPECT_FLOAT_EQ(bounds.pMin.y(), -1.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.y(), 4.0f);
}

TEST(CurveTest, RayIntersectionHit) {
    gnd::PropertyList props;
    props.setPoint3("p0", gnd::Point3f(0.0f, 0.0f, 0.0f));
    props.setPoint3("p1", gnd::Point3f(0.0f, 1.0f, 0.0f));
    props.setPoint3("p2", gnd::Point3f(0.0f, 2.0f, 0.0f));
    props.setPoint3("p3", gnd::Point3f(0.0f, 3.0f, 0.0f));
    props.setFloat("w0", 2.0f);
    props.setFloat("w1", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("curve", props);
    gnd::Shape* curve = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(curve, nullptr);

    gnd::Ray ray(gnd::Point3f(0.0f, 1.5f, 5.0f), gnd::Vector3f(0.0f, 0.0f, -1.0f));
    gnd::SurfaceInteraction isect;

    bool hit = curve->rayIntersect(ray, isect, false);
    EXPECT_TRUE(hit);

    curve->fillInteraction(ray, isect);

    EXPECT_FLOAT_EQ(isect.t, 4.0f);
    EXPECT_FLOAT_EQ(isect.p.x(), 0.0f);
    EXPECT_FLOAT_EQ(isect.p.y(), 1.5f);
    EXPECT_FLOAT_EQ(isect.p.z(), 1.0f);

    EXPECT_NEAR(isect.n.x(), 0.0f, 1e-5f);
    EXPECT_NEAR(isect.n.y(), 0.0f, 1e-5f);
    EXPECT_NEAR(isect.n.z(), 1.0f, 1e-5f);
}

TEST(CurveTest, RayIntersectionMiss) {
    gnd::PropertyList props;
    props.setPoint3("p0", gnd::Point3f(0.0f, 0.0f, 0.0f));
    props.setPoint3("p1", gnd::Point3f(0.0f, 1.0f, 0.0f));
    props.setPoint3("p2", gnd::Point3f(0.0f, 2.0f, 0.0f));
    props.setPoint3("p3", gnd::Point3f(0.0f, 3.0f, 0.0f));
    props.setFloat("w0", 2.0f);
    props.setFloat("w1", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("curve", props);
    gnd::Shape* curve = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(curve, nullptr);

    gnd::Ray ray(gnd::Point3f(5.0f, 1.5f, 5.0f), gnd::Vector3f(0.0f, 0.0f, -1.0f));
    gnd::SurfaceInteraction isect;

    bool hit = curve->rayIntersect(ray, isect, false);
    EXPECT_FALSE(hit);
}

TEST(CurveTest, PdfSurface) {
    gnd::PropertyList props;
    props.setPoint3("p0", gnd::Point3f(0.0f, 0.0f, 0.0f));
    props.setPoint3("p1", gnd::Point3f(0.0f, 1.0f, 0.0f));
    props.setPoint3("p2", gnd::Point3f(0.0f, 2.0f, 0.0f));
    props.setPoint3("p3", gnd::Point3f(0.0f, 3.0f, 0.0f));
    props.setFloat("w0", 2.0f);
    props.setFloat("w1", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("curve", props);
    gnd::Shape* curve = dynamic_cast<gnd::Shape*>(obj.get());
    ASSERT_NE(curve, nullptr);

    gnd::Point3f ref(10.0f, 0.0f, 0.0f);
    gnd::SurfaceInteraction dummyIsect;

    float pdf = curve->pdfSurface(ref, dummyIsect);
    EXPECT_FLOAT_EQ(pdf, 0.0f);
}