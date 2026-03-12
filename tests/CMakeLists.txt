add_executable(GianduiaTests
        math/color_test.cpp
        math/bounds_test.cpp
)

target_link_libraries(GianduiaTests PRIVATE
        GianduiaEngine
        GTest::gtest_main
)

add_test(NAME CoreTests COMMAND GianduiaTests)