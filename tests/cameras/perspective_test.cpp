#include <gtest/gtest.h>
#include <gianduia/core/camera.h>
#include <gianduia/core/factory.h>
#include <gianduia/math/geometry.h>
#include <gianduia/math/ray.h>
#include <cmath>

TEST(PerspectiveCameraTest, FactoryInstantiation) {
    gnd::PropertyList props;
    props.setFloat("fov", 90.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("perspective", props);
    gnd::Camera* camera = dynamic_cast<gnd::Camera*>(obj.get());

    ASSERT_NE(camera, nullptr);
}

TEST(PerspectiveCameraTest, CenterRayDirection) {
    gnd::PropertyList props;
    props.setFloat("fov", 60.0f);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("perspective", props);
    gnd::Camera* camera = dynamic_cast<gnd::Camera*>(obj.get());
    ASSERT_NE(camera, nullptr);

    gnd::Ray ray;
    float weight = camera->shootRay(gnd::Point2f(0.5f, 0.5f), &ray);

    EXPECT_FLOAT_EQ(weight, 1.0f);

    EXPECT_FLOAT_EQ(ray.o.x(), 0.0f);
    EXPECT_FLOAT_EQ(ray.o.y(), 0.0f);
    EXPECT_FLOAT_EQ(ray.o.z(), 0.0f);

    EXPECT_FLOAT_EQ(ray.d.x(), 0.0f);
    EXPECT_FLOAT_EQ(ray.d.y(), 0.0f);
    EXPECT_FLOAT_EQ(ray.d.z(), -1.0f);
}

TEST(PerspectiveCameraTest, EdgeRayDirection) {
    gnd::PropertyList props;
    props.setFloat("fov", 90.0f);

    props.setInteger("width", 800);
    props.setInteger("height", 800);

    std::unique_ptr<gnd::GndObject> obj = gnd::GndFactory::getInstance()->createInstance("perspective", props);
    gnd::Camera* camera = dynamic_cast<gnd::Camera*>(obj.get());
    ASSERT_NE(camera, nullptr);

    gnd::Ray ray;
    camera->shootRay(gnd::Point2f(1.0f, 1.0f), &ray);
    float invSqrt3 = 1.0f / std::sqrt(3.0f);

    EXPECT_NEAR(ray.d.x(), invSqrt3, 1e-5f);
    EXPECT_NEAR(ray.d.y(), invSqrt3, 1e-5f);
    EXPECT_NEAR(ray.d.z(), -invSqrt3, 1e-5f);
}