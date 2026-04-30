cmake_minimum_required(VERSION 3.20)
project(Gianduia VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

find_package(glad CONFIG REQUIRED)
find_package(glfw3 CONFIG REQUIRED)
find_package(imgui CONFIG REQUIRED)
find_package(glm CONFIG REQUIRED)
find_package(pugixml CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)
find_package(OpenEXR CONFIG REQUIRED)
find_package(OpenVDB CONFIG REQUIRED)
find_package(blosc CONFIG REQUIRED)
find_package(tinyobjloader CONFIG REQUIRED)
find_package(OpenGL REQUIRED)
find_package(hwy CONFIG REQUIRED)

find_path(STB_INCLUDE_DIR "stb_image.h")

if(POLICY CMP0135)
    cmake_policy(SET CMP0135 NEW)
endif()

include(FetchContent)

if(APPLE)
    if(CMAKE_SYSTEM_PROCESSOR MATCHES "arm64")
        set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.arm64.macos.tar.gz")
    else()
        set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.x86_64.macos.tar.gz")
    endif()
elseif(WIN32)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.x64.windows.zip")
elseif(UNIX AND NOT APPLE)
    set(OIDN_URL "https://github.com/RenderKit/oidn/releases/download/v2.4.1/oidn-2.4.1.x86_64.linux.tar.gz")
endif()

FetchContent_Declare(
        oidn_binaries
        URL ${OIDN_URL}
)
FetchContent_MakeAvailable(oidn_binaries)

file(GLOB_RECURSE OIDN_CONFIG_FILES "${oidn_binaries_SOURCE_DIR}/*OpenImageDenoiseConfig.cmake")

if (OIDN_CONFIG_FILES)
    list(GET OIDN_CONFIG_FILES 0 OIDN_CONFIG_FILE_PATH)

    get_filename_component(OpenImageDenoise_DIR "${OIDN_CONFIG_FILE_PATH}" DIRECTORY)
    message(STATUS "Found OpenImageDenoise in: ${OpenImageDenoise_DIR}")
else()
    message(FATAL_ERROR "Cannot find OpenImageDenoiseConfig.cmake in downloaded files.")
endif()

find_package(OpenImageDenoise REQUIRED)

if (MSVC)
    add_compile_definitions(_USE_MATH_DEFINES)
endif()

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
        OpenImageDenoise
        hwy::hwy
)

# Resolving blosc
if(TARGET blosc_static)
    target_link_libraries(GianduiaEngine PUBLIC blosc_static)
elseif(TARGET blosc_shared)
    target_link_libraries(GianduiaEngine PUBLIC blosc_shared)
elseif(TARGET blosc)
    target_link_libraries(GianduiaEngine PUBLIC blosc)
else()
    message(WARNING "No blosc target found. OpenVDB linking may fail.")
endif()

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
        src/app/debugRenderer.cpp
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

enable_testing()
find_package(GTest CONFIG REQUIRED)
add_subdirectory(tests)