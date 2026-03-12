add_executable(GianduiaTests
        cameras/perspective_test.cpp
        math/color_test.cpp
        math/bounds_test.cpp
        math/frame_test.cpp
        math/warp_test.cpp
)

target_link_libraries(GianduiaTests PRIVATE
        GianduiaEngine
        GTest::gtest_main
)

add_test(NAME CoreTests COMMAND GianduiaTests)