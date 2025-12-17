cmake_minimum_required(VERSION 3.10)
project(rendering_project)

set(CMAKE_CXX_STANDARD 20)

include(FetchContent)
FetchContent_Declare(SFML
        GIT_REPOSITORY https://github.com/SFML/SFML.git
        GIT_TAG 3.0.2
        GIT_SHALLOW ON
        EXCLUDE_FROM_ALL
        SYSTEM)
FetchContent_MakeAvailable(SFML)

add_executable(rendering_project
        main.cpp
        Vector3.cpp
        Rotation.cpp Rotation.h
        Quaternion.cpp
        Ray.h Ray.cpp
        Objects/Object.h
        Objects/Sphere.h
        RayMarchingRender.h
        RayMarchingRender.cpp
        CameraBasis.cpp
        CameraBasis.h)

target_compile_features(rendering_project PRIVATE cxx_std_20)
target_link_libraries(rendering_project PRIVATE SFML::Graphics)
