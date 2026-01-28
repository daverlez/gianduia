cmake_minimum_required(VERSION 3.20)
project(Gianduia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)

find_package(OpenGL REQUIRED)

find_path(STB_INCLUDE_DIR "stb_image.h")

add_executable(Gianduia
        src/app/main.cpp
        src/core/factory.cpp
        src/cameras/perspective.cpp
        src/core/bitmap.cpp
        src/shapes/sphere.cpp
)

target_include_directories(Gianduia PRIVATE
        src
        include
        ${STB_INCLUDE_DIR}
)

if(APPLE)
    target_compile_definitions(Gianduia PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_link_libraries(Gianduia PRIVATE
        glfw
        OpenGL::GL
        imgui::imgui
        glm::glm
        pugixml::pugixml
        TBB::tbb
        OpenEXR::OpenEXR
        OpenVDB::openvdb
)