cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME aleph3)
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ALEPH3_BUILD_LEGACY "Build the legacy prototype engine and CLI." ON)
option(ALEPH3_BUILD_REWRITE "Build the rewrite-path SDK targets." ON)

set(ALEPH3_PUBLIC_HEADERS
    include/sdk/Engine.hpp
    include/sdk/Policy.hpp
    include/sdk/Schema.hpp
    include/sdk/Types.hpp
    include/ir/Node.hpp
    include/frontend/Token.hpp
    include/frontend/Lexer.hpp
    include/frontend/Parser.hpp
    include/semantics/Validator.hpp
    include/runtime/Evaluator.hpp)

if(ALEPH3_BUILD_REWRITE)
    add_library(aleph3_runtime INTERFACE)
    target_include_directories(aleph3_runtime INTERFACE include)

    add_library(aleph3_sdk
        src/sdk/Engine.cpp
        src/frontend/Lexer.cpp
        src/frontend/Parser.cpp
        src/semantics/Validator.cpp
        src/runtime/Evaluator.cpp
        ${ALEPH3_PUBLIC_HEADERS})
    target_include_directories(aleph3_sdk PUBLIC include)
    target_link_libraries(aleph3_sdk PUBLIC aleph3_runtime)

    add_executable(aleph3_rewrite_cli src/tooling/rewrite_cli.cpp)
    target_link_libraries(aleph3_rewrite_cli PRIVATE aleph3_sdk)
endif()

if(ALEPH3_BUILD_LEGACY)
    file(GLOB LEGACY_ROOT_SOURCES "src/*.cpp")
    list(REMOVE_ITEM LEGACY_ROOT_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp")
    file(GLOB LEGACY_COMPONENT_SOURCES
        "src/algebra/*.cpp"
        "src/evaluator/*.cpp"
        "src/expr/*.cpp"
        "src/transforms/*.cpp")
    file(GLOB LEGACY_HEADERS
        "include/*.hpp"
        "include/*.h"
        "include/algebra/*.hpp"
        "include/evaluator/*.hpp"
        "include/expr/*.hpp"
        "include/help/*.hpp"
        "include/normalizer/*.hpp"
        "include/parser/*.hpp"
        "include/transforms/*.hpp"
        "include/util/*.hpp")

    add_library(aleph3_legacy
        ${LEGACY_ROOT_SOURCES}
        ${LEGACY_COMPONENT_SOURCES}
        ${LEGACY_HEADERS})
    add_library(${PROJECT_NAME}_lib ALIAS aleph3_legacy)
    target_include_directories(aleph3_legacy PUBLIC include PRIVATE third_party/utf8cpp)

    add_executable(aleph3_cli_legacy src/main.cpp)
    set_target_properties(aleph3_cli_legacy PROPERTIES OUTPUT_NAME aleph3)
    target_link_libraries(aleph3_cli_legacy PRIVATE aleph3_legacy)
    target_include_directories(aleph3_cli_legacy PRIVATE third_party/utf8cpp)
endif()

include(CTest)

if(BUILD_TESTING)
    include(FetchContent)
    FetchContent_Declare(
        Catch2
        GIT_REPOSITORY https://github.com/catchorg/Catch2.git
        GIT_TAG v3.4.0
    )

    set(FETCHCONTENT_QUIET OFF)
    FetchContent_MakeAvailable(Catch2)

    if(ALEPH3_BUILD_LEGACY)
        file(GLOB LEGACY_TEST_ROOT "tests/*.cpp")
        file(GLOB LEGACY_TEST_SOURCES
            "tests/algebra/*.cpp"
            "tests/evaluator/*.cpp"
            "tests/parser/*.cpp")

        add_executable(aleph3_legacy_tests ${LEGACY_TEST_ROOT} ${LEGACY_TEST_SOURCES})
        target_link_libraries(aleph3_legacy_tests PRIVATE aleph3_legacy Catch2::Catch2WithMain)
        target_include_directories(aleph3_legacy_tests PRIVATE third_party/utf8cpp tests)
        add_test(NAME aleph3_legacy_tests COMMAND aleph3_legacy_tests)
    endif()

    if(ALEPH3_BUILD_REWRITE)
        file(GLOB REWRITE_TEST_SOURCES
            "tests/frontend/*.cpp"
            "tests/ir/*.cpp"
            "tests/runtime/*.cpp"
            "tests/semantics/*.cpp"
            "tests/sdk/*.cpp")

        add_executable(aleph3_sdk_tests ${REWRITE_TEST_SOURCES})
        target_link_libraries(aleph3_sdk_tests PRIVATE aleph3_sdk Catch2::Catch2WithMain)
        target_include_directories(aleph3_sdk_tests PRIVATE tests)
        add_test(NAME aleph3_sdk_tests COMMAND aleph3_sdk_tests)
    endif()
endif()

# Add compile definitions for versioning
add_compile_definitions(${PROJECT_NAME}_VERSION="0.1.0")
