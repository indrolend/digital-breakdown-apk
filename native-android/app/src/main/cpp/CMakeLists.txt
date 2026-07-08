cmake_minimum_required(VERSION 3.22.1)

project(digitalbreakdown)

add_library(
    digitalbreakdown
    SHARED
    native_bridge.cpp
    game/Game.cpp
    render/Renderer.cpp
)

target_include_directories(
    digitalbreakdown
    PRIVATE
    game
    render
)

target_link_libraries(
    digitalbreakdown
    android
    log
    EGL
    GLESv2
)
