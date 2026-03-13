#include <gtest/gtest.h>
#include <gianduia/shapes/shape.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/ray.h>
#include <gianduia/math/geometry.h>
#include <gianduia/math/constants.h>

TEST(CylinderTest, FactoryAndBounds) {
    gnd::PropertyList props;
    props.setFloat("radius", 2.0f);
    props.setFloat("zMin", -3.0f);
    props.setFloat("zMax", 3.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cylinder", props);
    gnd::Shape* cylinder = dynamic_cast<gnd::Shape*>(obj.get());

    ASSERT_NE(cylinder, nullptr);

    gnd::Bounds3f bounds = cylinder->getBounds();

    EXPECT_FLOAT_EQ(bounds.pMin.x(), -2.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.y(), -2.0f);
    EXPECT_FLOAT_EQ(bounds.pMin.z(), -3.0f);

    EXPECT_FLOAT_EQ(bounds.pMax.x(), 2.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.y(), 2.0f);
    EXPECT_FLOAT_EQ(bounds.pMax.z(), 3.0f);
}

TEST(CylinderTest, RayIntersectionSide) {
    gnd::PropertyList props;
    props.setFloat("radius", 1.0f);
    props.setFloat("zMin", -2.0f);
    props.setFloat("zMax", 2.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cylinder", props);
    gnd::Shape* cylinder = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Ray ray(gnd::Point3f(5.0f, 0.0f, 0.0f), gnd::Vector3f(-1.0f, 0.0f, 0.0f));
    gnd::SurfaceInteraction isect;

    EXPECT_TRUE(cylinder->rayIntersect(ray, isect, false));
    cylinder->fillInteraction(ray, isect);

    EXPECT_FLOAT_EQ(isect.t, 4.0f);
    EXPECT_FLOAT_EQ(isect.p.x(), 1.0f);
    EXPECT_FLOAT_EQ(isect.n.x(), 1.0f);
    EXPECT_FLOAT_EQ(isect.n.z(), 0.0f);
}

TEST(CylinderTest, RayIntersectionCap) {
    gnd::PropertyList props;
    props.setFloat("radius", 1.0f);
    props.setFloat("zMin", -1.0f);
    props.setFloat("zMax", 1.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cylinder", props);
    gnd::Shape* cylinder = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Ray ray(gnd::Point3f(0.5f, 0.0f, 5.0f), gnd::Vector3f(0.0f, 0.0f, -1.0f));
    gnd::SurfaceInteraction isect;

    EXPECT_TRUE(cylinder->rayIntersect(ray, isect, false));
    cylinder->fillInteraction(ray, isect);

    EXPECT_FLOAT_EQ(isect.t, 4.0f);
    EXPECT_FLOAT_EQ(isect.p.z(), 1.0f);
    EXPECT_FLOAT_EQ(isect.n.z(), 1.0f);
}

TEST(CylinderTest, PdfSurface) {
    gnd::PropertyList props;
    props.setFloat("radius", 2.0f);
    props.setFloat("zMin", 0.0f);
    props.setFloat("zMax", 5.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("cylinder", props);
    gnd::Shape* cylinder = dynamic_cast<gnd::Shape*>(obj.get());

    gnd::Point3f ref(10.0f, 0.0f, 0.0f);
    gnd::SurfaceInteraction dummyIsect;

    float pdf = cylinder->pdfSurface(ref, dummyIsect);

    float expectedArea = 28.0f * gnd::Pi;
    EXPECT_FLOAT_EQ(pdf, 1.0f / expectedArea);
}