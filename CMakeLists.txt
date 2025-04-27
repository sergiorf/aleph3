cmake_minimum_required(VERSION 3.20)
project(mathix VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED True)

set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)

include_directories(include)

file(GLOB_RECURSE LIB_SOURCES "src/*.cpp")
list(REMOVE_ITEM LIB_SOURCES "${CMAKE_SOURCE_DIR}/src/main.cpp") # Exclude main.cpp
file(GLOB_RECURSE HEADERS "include/*.hpp" "include/*.h")

# Create a library target for shared code
add_library(mathix_lib ${LIB_SOURCES} ${HEADERS})

add_executable(mathix src/main.cpp)
target_link_libraries(mathix PRIVATE mathix_lib)

# Fetch Catch2 for testing
include(FetchContent)
FetchContent_Declare(
    Catch2
    GIT_REPOSITORY https://github.com/catchorg/Catch2.git
    GIT_TAG v3.4.0 # Use the latest stable version
)

# Enable verbose output for FetchContent
set(FETCHCONTENT_QUIET OFF)

FetchContent_MakeAvailable(Catch2)

# Add test executable
file(GLOB_RECURSE TEST_SOURCES "tests/*.cpp")
add_executable(mathix_tests ${TEST_SOURCES})
target_link_libraries(mathix_tests PRIVATE mathix_lib Catch2::Catch2WithMain)

# Enable testing
include(CTest)
add_test(NAME MathixTests COMMAND mathix_tests)

# Add compile definitions for versioning
add_compile_definitions(MATHIX_VERSION="0.1.0")
