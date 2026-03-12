cmake_minimum_required(VERSION 3.20)
project(Gianduia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(OpenGL REQUIRED)

find_path(STB_INCLUDE_DIR "stb_image.h")

add_library(GianduiaEngine OBJECT)

target_include_directories(GianduiaEngine PUBLIC
        src
        include
        ${STB_INCLUDE_DIR}
)

target_link_libraries(GianduiaEngine PUBLIC
        glm::glm
        TBB::tbb
        OpenEXR::OpenEXR
        OpenVDB::openvdb
        tinyobjloader::tinyobjloader
        pugixml::pugixml
)

add_subdirectory(src/core)
add_subdirectory(src/cameras)
add_subdirectory(src/shapes)
add_subdirectory(src/scene)
add_subdirectory(src/samplers)
add_subdirectory(src/integrators)
add_subdirectory(src/math)
add_subdirectory(src/textures)
add_subdirectory(src/materials)
add_subdirectory(src/emitters)

add_executable(Gianduia
        src/app/main.cpp
        src/app/application.cpp
        src/app/postProcessor.cpp
)

if(APPLE)
    target_compile_definitions(Gianduia PRIVATE GL_SILENCE_DEPRECATION)
endif()

target_link_libraries(Gianduia PRIVATE
        GianduiaEngine
        glad::glad
        glfw
        OpenGL::GL
        imgui::imgui
)