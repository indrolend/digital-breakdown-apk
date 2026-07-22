cmake_minimum_required(VERSION 3.22)

# Set the runtime before dependencies are configured so DigitalBreakdown and
# statically linked libraries such as GLFW agree on /MT versus /MD.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

project(DigitalBreakdownDesktop LANGUAGES C CXX)

enable_testing()

include(FetchContent)

set(GLFW_BUILD_DOCS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_TESTS OFF CACHE BOOL "" FORCE)
set(GLFW_BUILD_EXAMPLES OFF CACHE BOOL "" FORCE)
set(GLFW_INSTALL OFF CACHE BOOL "" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

FetchContent_Declare(
    glfw
    GIT_REPOSITORY https://github.com/glfw/glfw.git
    GIT_TAG 3.4
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(glfw)
FetchContent_Declare(
    miniaudio
    GIT_REPOSITORY https://github.com/mackron/miniaudio.git
    GIT_TAG 0.11.25
    GIT_SHALLOW TRUE
)
FetchContent_MakeAvailable(miniaudio)
if(APPLE)
    set(USE_TLS ON CACHE BOOL "" FORCE)
    set(USE_OPEN_SSL OFF CACHE BOOL "" FORCE)
    set(IXWEBSOCKET_INSTALL OFF CACHE BOOL "" FORCE)
    FetchContent_Declare(
        ixwebsocket
        GIT_REPOSITORY https://github.com/machinezone/IXWebSocket.git
        GIT_TAG v11.4.6
        GIT_SHALLOW TRUE
    )
    FetchContent_MakeAvailable(ixwebsocket)
endif()
find_package(OpenGL REQUIRED)

set(DB_SHARED_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../native-android/app/src/main/cpp")

add_executable(
    DigitalBreakdown
    main.cpp
    DesktopRenderer.cpp
    DesktopAudio.cpp
    DesktopMultiplayer.cpp
    "${miniaudio_SOURCE_DIR}/miniaudio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network/MultiplayerProtocol.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)

target_include_directories(
    DigitalBreakdown
    PRIVATE
    "${DB_SHARED_ROOT}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network"
    "${miniaudio_SOURCE_DIR}"
)

target_compile_features(DigitalBreakdown PRIVATE cxx_std_17)
target_compile_definitions(DigitalBreakdown PRIVATE GLFW_INCLUDE_NONE)

target_link_libraries(
    DigitalBreakdown
    PRIVATE
    glfw
    OpenGL::GL
)
if(APPLE)
    target_link_libraries(DigitalBreakdown PRIVATE ixwebsocket)
    set_target_properties(DigitalBreakdown PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Digital Breakdown"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.indrolend.digitalbreakdown.native"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    )
endif()

add_executable(
    Pass7ParityTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/game_pass7_parity_smoke.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(Pass7ParityTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(Pass7ParityTest PRIVATE cxx_std_17)

add_executable(
    MultiplayerProtocolTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/multiplayer_protocol_test.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network/MultiplayerProtocol.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(MultiplayerProtocolTest PRIVATE "${DB_SHARED_ROOT}/game" "${CMAKE_CURRENT_SOURCE_DIR}/../native-network")
target_compile_features(MultiplayerProtocolTest PRIVATE cxx_std_17)

add_executable(
    GameplayRoleAndSoulMotionTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/gameplay_role_and_soul_motion_test.cpp"
)
target_include_directories(GameplayRoleAndSoulMotionTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(GameplayRoleAndSoulMotionTest PRIVATE cxx_std_17)

add_test(NAME GameplayRoleAndSoulMotionTest COMMAND GameplayRoleAndSoulMotionTest)

if(WIN32)
    target_link_libraries(DigitalBreakdown PRIVATE winmm winhttp)
endif()

add_custom_command(TARGET DigitalBreakdown POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/audio"
        "$<TARGET_FILE_DIR:DigitalBreakdown>/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-models"
        "$<TARGET_FILE_DIR:DigitalBreakdown>/models"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/../native-tv-gifs"
        "$<TARGET_FILE_DIR:DigitalBreakdown>/tv-gifs")

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
