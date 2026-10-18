cmake_minimum_required(VERSION 3.20)

project(KorMotorforge
    VERSION 0.1.0
    DESCRIPTION "An LLM-first modular game engine"
    LANGUAGES CXX
)

include(CTest)
include(FetchContent)
include(GNUInstallDirs)

set(JSON_BuildTests OFF CACHE INTERNAL "")
set(JSON_Install OFF CACHE INTERNAL "")
FetchContent_Declare(
    nlohmann_json
    GIT_REPOSITORY https://github.com/nlohmann/json.git
    GIT_TAG v3.12.0
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(nlohmann_json)

add_library(motorforge_core
    src/core/engine_info.cpp
    src/core/project_validator.cpp
    src/core/project_creator.cpp
)
add_library(KorMotorforge::Core ALIAS motorforge_core)

target_compile_features(motorforge_core PUBLIC cxx_std_20)
target_include_directories(motorforge_core
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/src>
        $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>
)
target_link_libraries(motorforge_core PUBLIC nlohmann_json::nlohmann_json)
target_compile_definitions(motorforge_core
    PUBLIC MOTORFORGE_VERSION="${PROJECT_VERSION}"
)

if(MSVC)
    target_compile_options(motorforge_core PRIVATE /W4 /permissive-)
else()
    target_compile_options(motorforge_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

add_executable(motorforge
    src/cli/main.cpp
)
target_link_libraries(motorforge PRIVATE KorMotorforge::Core)

if(BUILD_TESTING)
    add_executable(motorforge_tests
        tests/test_engine_info.cpp
    )
    target_link_libraries(motorforge_tests PRIVATE KorMotorforge::Core)
    add_test(NAME motorforge_core_contract COMMAND motorforge_tests)

    add_executable(motorforge_project_validator_tests
        tests/test_project_validator.cpp
    )
    target_link_libraries(motorforge_project_validator_tests PRIVATE KorMotorforge::Core)
    add_test(NAME motorforge_project_validator COMMAND motorforge_project_validator_tests)

    add_executable(motorforge_project_creator_tests
        tests/test_project_creator.cpp
    )
    target_link_libraries(motorforge_project_creator_tests PRIVATE KorMotorforge::Core)
    add_test(NAME motorforge_project_creator COMMAND motorforge_project_creator_tests)

    add_test(NAME motorforge_cli_help COMMAND motorforge --help)
    add_test(NAME motorforge_cli_version COMMAND motorforge --version)
    add_test(NAME motorforge_cli_capabilities COMMAND motorforge capabilities --json)
    add_test(
        NAME motorforge_example_project
        COMMAND motorforge validate ${CMAKE_CURRENT_SOURCE_DIR}/examples/empty-project --json
    )
endif()

install(TARGETS motorforge
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)
