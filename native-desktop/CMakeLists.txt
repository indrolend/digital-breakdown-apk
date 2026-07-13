cmake_minimum_required(VERSION 3.22)

project(DigitalBreakdownDesktop LANGUAGES CXX)

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)

set(DB_SHARED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../native-android/app/src/main/cpp")

add_executable(
    DigitalBreakdown
    main.cpp
    DesktopRenderer.cpp
    "${DB_SHARED_ROOT}/game/Game.cpp"
)

target_include_directories(
    DigitalBreakdown
    PRIVATE
    "${DB_SHARED_ROOT}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}"
)

target_compile_features(DigitalBreakdown PRIVATE cxx_std_17)
target_compile_definitions(DigitalBreakdown PRIVATE GLFW_INCLUDE_NONE)

target_link_libraries(
    DigitalBreakdown
    PRIVATE
    glfw
    opengl32
)

if(MSVC)
    target_compile_options(DigitalBreakdown PRIVATE /W4 /permissive-)
else()
    target_compile_options(DigitalBreakdown PRIVATE -Wall -Wextra -Wpedantic)
endif()

set_target_properties(
    DigitalBreakdown
    PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin"
    VS_DEBUGGER_WORKING_DIRECTORY "${CMAKE_CURRENT_SOURCE_DIR}/.."
)
