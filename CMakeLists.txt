cmake_minimum_required(VERSION 3.20)

set(PROJECT_NAME aleph3)
project(${PROJECT_NAME} VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

option(ALEPH3_BUILD_SYMBOLIC_ENGINE "Build the symbolic engine and its tests." ON)
option(ALEPH3_BUILD_SDK "Build the SDK and primary engine targets." ON)

set(ALEPH3_BUILD_KERNEL OFF)

if(DEFINED ALEPH3_BUILD_REWRITE AND NOT DEFINED ALEPH3_BUILD_SDK)
    set(ALEPH3_BUILD_SDK "${ALEPH3_BUILD_REWRITE}" CACHE BOOL "Build the SDK and primary engine targets." FORCE)
endif()

if(DEFINED ALEPH3_BUILD_LEGACY AND NOT DEFINED ALEPH3_BUILD_SYMBOLIC_ENGINE)
    set(ALEPH3_BUILD_SYMBOLIC_ENGINE "${ALEPH3_BUILD_LEGACY}" CACHE BOOL "Build the symbolic engine and its tests." FORCE)
endif()

if(ALEPH3_BUILD_SDK OR ALEPH3_BUILD_SYMBOLIC_ENGINE)
    set(ALEPH3_BUILD_KERNEL ON)
endif()

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
    include/tooling/RewriteCliSupport.hpp
    include/tooling/DemoHostFunctions.hpp)

if(ALEPH3_BUILD_SDK)
    add_library(aleph3_sdk
        src/sdk/Engine.cpp
        src/frontend/Lexer.cpp
        src/frontend/Parser.cpp
        src/semantics/Validator.cpp
        src/tooling/RewriteCliSupport.cpp
        src/tooling/DemoHostFunctions.cpp
        ${ALEPH3_PUBLIC_HEADERS})
    target_include_directories(aleph3_sdk PUBLIC include)

    add_executable(aleph3_cli src/tooling/aleph3_cli.cpp)
    target_link_libraries(aleph3_cli PRIVATE aleph3_sdk)

    add_executable(aleph3_sdk_example examples/sdk_host_example.cpp)
    target_link_libraries(aleph3_sdk_example PRIVATE aleph3_sdk)
endif()

if(ALEPH3_BUILD_KERNEL)
    file(GLOB SYMBOLIC_ROOT_SOURCES CONFIGURE_DEPENDS "src/*.cpp")
    file(GLOB SYMBOLIC_COMPONENT_SOURCES CONFIGURE_DEPENDS
        "src/algebra/*.cpp"
        "src/evaluator/*.cpp"
        "src/expr/*.cpp"
        "src/kernel/*.cpp"
        "src/packs/*.cpp"
        "src/symbols/*.cpp"
        "src/transforms/*.cpp"
        "src/tooling/SymbolicCliSupport.cpp")
    file(GLOB SYMBOLIC_HEADERS CONFIGURE_DEPENDS
        "include/*.hpp"
        "include/*.h"
        "include/algebra/*.hpp"
        "include/evaluator/*.hpp"
        "include/expr/*.hpp"
        "include/help/*.hpp"
        "include/kernel/*.hpp"
        "include/normalizer/*.hpp"
        "include/packs/*.hpp"
        "include/parser/*.hpp"
        "include/symbols/*.hpp"
        "include/tooling/*.hpp"
        "include/transforms/*.hpp"
        "include/util/*.hpp")

    add_library(aleph3_kernel
        ${SYMBOLIC_ROOT_SOURCES}
        ${SYMBOLIC_COMPONENT_SOURCES}
        ${SYMBOLIC_HEADERS})
    add_library(aleph3_symbolic ALIAS aleph3_kernel)
    add_library(${PROJECT_NAME}_lib ALIAS aleph3_kernel)
    target_include_directories(aleph3_kernel PUBLIC include PRIVATE third_party/utf8cpp)

    add_library(aleph3_pack_core_math INTERFACE)
    target_link_libraries(aleph3_pack_core_math INTERFACE aleph3_kernel)

    add_library(aleph3_pack_algebra INTERFACE)
    target_link_libraries(aleph3_pack_algebra INTERFACE aleph3_kernel)

    if(ALEPH3_BUILD_SDK)
        target_link_libraries(aleph3_sdk PRIVATE aleph3_kernel)
    endif()

    if(ALEPH3_BUILD_SDK AND ALEPH3_BUILD_SYMBOLIC_ENGINE)
        target_link_libraries(aleph3_cli PRIVATE aleph3_kernel)
        target_compile_definitions(aleph3_cli PRIVATE ALEPH3_HAS_SYMBOLIC_ENGINE=1)
    endif()
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

    if(ALEPH3_BUILD_SYMBOLIC_ENGINE)
        file(GLOB KERNEL_TEST_ROOT CONFIGURE_DEPENDS
            "tests/ExpandTests.cpp"
            "tests/ExprPrettyPrintTests.cpp"
            "tests/FullFormTests.cpp"
            "tests/GlobalTestSetup.cpp"
            "tests/NormalizerTests.cpp"
            "tests/SimplifyTests.cpp")
        file(GLOB KERNEL_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/algebra/*.cpp"
            "tests/evaluator/*.cpp"
            "tests/parser/*.cpp")
        file(GLOB SYMBOLIC_TOOLING_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/SymbolicCliSupportTests.cpp")
        file(GLOB PACK_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/packs/*.cpp")

        add_executable(
            aleph3_symbolic_tests
            ${KERNEL_TEST_ROOT}
            ${KERNEL_TEST_SOURCES}
            ${SYMBOLIC_TOOLING_TEST_SOURCES}
            ${PACK_TEST_SOURCES})
        target_link_libraries(aleph3_symbolic_tests PRIVATE aleph3_kernel Catch2::Catch2WithMain)
        target_include_directories(aleph3_symbolic_tests PRIVATE third_party/utf8cpp tests)
        add_test(NAME aleph3_symbolic_tests COMMAND aleph3_symbolic_tests)
    endif()

    if(ALEPH3_BUILD_SDK)
        file(GLOB SDK_FRONTEND_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/frontend/*.cpp"
            "tests/ir/*.cpp"
            "tests/semantics/*.cpp"
            "tests/sdk/*.cpp")
        file(GLOB SDK_TOOLING_TEST_SOURCES CONFIGURE_DEPENDS
            "tests/tooling/*.cpp")

        add_executable(
            aleph3_sdk_tests
            ${SDK_FRONTEND_TEST_SOURCES}
            ${SDK_TOOLING_TEST_SOURCES})
        target_link_libraries(aleph3_sdk_tests PRIVATE aleph3_sdk Catch2::Catch2WithMain)
        target_include_directories(aleph3_sdk_tests PRIVATE tests)
        target_compile_definitions(aleph3_sdk_tests PRIVATE ALEPH3_CLI_PATH="${CMAKE_RUNTIME_OUTPUT_DIRECTORY}/aleph3_cli")
        add_dependencies(aleph3_sdk_tests aleph3_cli)
        add_test(NAME aleph3_sdk_tests COMMAND aleph3_sdk_tests)
    endif()
endif()

# Add compile definitions for versioning
add_compile_definitions(${PROJECT_NAME}_VERSION="0.1.0")
