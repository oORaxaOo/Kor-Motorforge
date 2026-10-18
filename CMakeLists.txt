cmake_minimum_required(VERSION 3.20)
project(KorMotorforge VERSION 0.1.0 DESCRIPTION "An LLM-first modular game engine" LANGUAGES CXX)

include(CTest)
include(FetchContent)

find_package(ZLIB REQUIRED)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(nlohmann_json GIT_REPOSITORY https://github.com/nlohmann/json.git GIT_TAG v3.12.0 GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(nlohmann_json)

add_library(motorforge_core
    src/core/engine_info.cpp
    src/core/project_validator.cpp
    src/core/project_creator.cpp
    src/core/procedural_sprite.cpp
    src/core/scene_store.cpp
    src/core/runtime.cpp
    src/core/scene_preview.cpp
    src/core/asset.cpp
    src/core/sprite.cpp
    src/core/atlas.cpp
    src/core/animation.cpp
)
add_library(KorMotorforge::Core ALIAS motorforge_core)

target_compile_features(motorforge_core PUBLIC cxx_std_20)

target_include_directories(motorforge_core PUBLIC $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src> $<INSTALL_INTERFACE:include>)

target_link_libraries(motorforge_core PUBLIC nlohmann_json::nlohmann_json ZLIB::ZLIB)

target_compile_definitions(motorforge_core PUBLIC MOTORFORGE_VERSION="${PROJECT_VERSION}")

if(MSVC)
    target_compile_options(motorforge_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(motorforge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(motorforge src/cli/main.cpp)
target_link_libraries(motorforge PRIVATE KorMotorforge::Core)

if(BUILD_TESTING)
    add_executable(motorforge_asset_pipeline_tests tests/test_asset_pipeline.cpp)
    target_link_libraries(motorforge_asset_pipeline_tests PRIVATE KorMotorforge::Core)
    add_test(NAME motorforge_asset_pipeline COMMAND motorforge_asset_pipeline_tests)

    add_executable(motorforge_sprite_animation_tests tests/test_sprite_animation.cpp)
    target_link_libraries(motorforge_sprite_animation_tests PRIVATE KorMotorforge::Core)
    add_test(NAME motorforge_sprite_animation COMMAND motorforge_sprite_animation_tests)
endif()
