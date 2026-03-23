add_executable(GianduiaTests
        cameras/perspective_test.cpp
        emitters/environment_test.cpp
        materials/fresnel_test.cpp
        materials/reflection_test.cpp
        materials/transmission_test.cpp
        math/color_test.cpp
        math/bounds_test.cpp
        math/frame_test.cpp
        math/microfacet_test.cpp
        math/warp_test.cpp
        shapes/sphere_test.cpp
        shapes/cylinder_test.cpp
)

target_link_libraries(GianduiaTests PRIVATE
        GianduiaEngine
        GTest::gtest_main
)

include(GoogleTest)
gtest_discover_tests(GianduiaTests)