cmake_minimum_required(VERSION 3.22)

project(DigitalBreakdownDesktop LANGUAGES C CXX)

# Set the runtime after compiler detection but before dependencies are
# configured so DigitalBreakdown and statically linked libraries such as GLFW
# agree on /MT versus /MD.
if(MSVC)
    set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")
endif()

enable_testing()

include(FetchContent)

set(DB_CHANNEL "experimental" CACHE STRING "Digital Breakdown release channel")
set(DB_SAVE_FORMAT_VERSION "4" CACHE STRING "Digital Breakdown persistent save format version")
string(TIMESTAMP DB_BUILD_DATE "%Y.%m.%d" UTC)
string(TIMESTAMP DB_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)
if(NOT DEFINED DB_HUMAN_VERSION)
    set(DB_HUMAN_VERSION "experimental-${DB_BUILD_DATE}")
endif()
if(NOT DEFINED DB_COMMIT_SHA)
    execute_process(
        COMMAND git -C "${CMAKE_CURRENT_SOURCE_DIR}/.." rev-parse HEAD
        OUTPUT_VARIABLE DB_COMMIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
endif()
if(NOT DB_COMMIT_SHA)
    set(DB_COMMIT_SHA "unknown")
endif()
string(SUBSTRING "${DB_COMMIT_SHA}" 0 7 DB_COMMIT_SHORT)
if(WIN32)
    set(DB_PLATFORM "windows")
elseif(APPLE)
    set(DB_PLATFORM "macos")
else()
    set(DB_PLATFORM "desktop")
endif()
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" DB_ARCHITECTURE)
if(DB_ARCHITECTURE MATCHES "^(amd64|x86_64)$")
    set(DB_ARCHITECTURE "x64")
elseif(DB_ARCHITECTURE MATCHES "^(aarch64|arm64)$")
    set(DB_ARCHITECTURE "arm64")
endif()

configure_file(
    "${CMAKE_CURRENT_SOURCE_DIR}/BuildIdentity.cpp.in"
    "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildIdentity.cpp"
    @ONLY
)

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
    DesktopUpdateService.cpp
    "${CMAKE_CURRENT_BINARY_DIR}/generated/BuildIdentity.cpp"
    "${miniaudio_SOURCE_DIR}/miniaudio.c"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network/MultiplayerProtocol.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
if(WIN32)
    target_sources(DigitalBreakdown PRIVATE ControllerRumbleWin.cpp)
elseif(APPLE)
    target_sources(DigitalBreakdown PRIVATE ControllerRumbleMac.m)
    set_source_files_properties(ControllerRumbleMac.m PROPERTIES COMPILE_FLAGS "-fobjc-arc")
endif()

target_include_directories(
    DigitalBreakdown
    PRIVATE
    "${DB_SHARED_ROOT}/game"
    "${CMAKE_CURRENT_SOURCE_DIR}"
    "${CMAKE_CURRENT_SOURCE_DIR}/third_party/stb"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network"
    "${CMAKE_CURRENT_BINARY_DIR}/generated"
    "${miniaudio_SOURCE_DIR}"
)

target_compile_features(DigitalBreakdown PRIVATE cxx_std_17)
target_compile_definitions(DigitalBreakdown PRIVATE
    GLFW_INCLUDE_NONE
    DB_BUILD_CONFIGURATION="$<CONFIG>"
)

target_link_libraries(
    DigitalBreakdown
    PRIVATE
    glfw
    OpenGL::GL
)
if(APPLE)
    target_link_libraries(DigitalBreakdown PRIVATE ixwebsocket "-framework AppKit" "-framework GameController" "-framework CoreHaptics")
    set_target_properties(DigitalBreakdown PROPERTIES
        MACOSX_BUNDLE TRUE
        MACOSX_BUNDLE_BUNDLE_NAME "Data"
        MACOSX_BUNDLE_GUI_IDENTIFIER "com.indrolend.digitalbreakdown.native"
        MACOSX_BUNDLE_BUNDLE_VERSION "1"
        MACOSX_BUNDLE_SHORT_VERSION_STRING "0.1.0"
    )
endif()
if(WIN32)
    target_link_libraries(DigitalBreakdown PRIVATE xinput9_1_0)
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
    MultiplayerDeterminismTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/multiplayer_determinism_test.cpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/../native-network/MultiplayerProtocol.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(MultiplayerDeterminismTest PRIVATE "${DB_SHARED_ROOT}/game" "${CMAKE_CURRENT_SOURCE_DIR}/../native-network" "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(MultiplayerDeterminismTest PRIVATE cxx_std_17)

add_executable(
    HostRemotePeerSimulationIsolationTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/host_remote_peer_simulation_isolation_test.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(HostRemotePeerSimulationIsolationTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(HostRemotePeerSimulationIsolationTest PRIVATE cxx_std_17)

add_executable(
    MultiplayerConnectionStateTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/multiplayer_connection_state_test.cpp"
)
target_include_directories(MultiplayerConnectionStateTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(MultiplayerConnectionStateTest PRIVATE cxx_std_17)

add_executable(
    GameplayRoleAndSoulMotionTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/gameplay_role_and_soul_motion_test.cpp"
)
target_include_directories(GameplayRoleAndSoulMotionTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(GameplayRoleAndSoulMotionTest PRIVATE cxx_std_17)

add_executable(
    GameplayGeometryAndConfigTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/gameplay_geometry_and_config_test.cpp"
)
target_include_directories(GameplayGeometryAndConfigTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(GameplayGeometryAndConfigTest PRIVATE cxx_std_17)

add_executable(
    TargetLifecycleTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/target_lifecycle_test.cpp"
)
target_include_directories(TargetLifecycleTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(TargetLifecycleTest PRIVATE cxx_std_17)

add_executable(
    GameplayStateContractsTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/gameplay_state_contracts_test.cpp"
)
target_include_directories(GameplayStateContractsTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(GameplayStateContractsTest PRIVATE cxx_std_17)

add_executable(
    PhoneBodyContractTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/phone_body_contract_test.cpp"
)
target_include_directories(PhoneBodyContractTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(PhoneBodyContractTest PRIVATE cxx_std_17)

add_executable(
    PhoneMenuLayoutTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/phone_menu_layout_test.cpp"
)
target_include_directories(PhoneMenuLayoutTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(PhoneMenuLayoutTest PRIVATE cxx_std_17)

add_executable(
    MenuNavigationTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/menu_navigation_test.cpp"
)
target_include_directories(MenuNavigationTest PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(MenuNavigationTest PRIVATE cxx_std_17)

add_executable(
    PhoneDisplayStateTest
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/phone_display_state_test.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(PhoneDisplayStateTest PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(PhoneDisplayStateTest PRIVATE cxx_std_17)

add_executable(
    GameplayFlowProbe
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/gameplay_flow_probe.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(GameplayFlowProbe PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(GameplayFlowProbe PRIVATE cxx_std_17)

add_executable(
    RoomProgressionProbe
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/room_progression_probe.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(RoomProgressionProbe PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(RoomProgressionProbe PRIVATE cxx_std_17)

add_executable(
    DeterministicInputSoak
    "${CMAKE_CURRENT_SOURCE_DIR}/../native/tests/deterministic_input_soak.cpp"
    "${DB_SHARED_ROOT}/game/Game.cpp"
)
target_include_directories(DeterministicInputSoak PRIVATE "${DB_SHARED_ROOT}/game")
target_compile_features(DeterministicInputSoak PRIVATE cxx_std_17)

add_test(NAME GameplayRoleAndSoulMotionTest COMMAND GameplayRoleAndSoulMotionTest)
add_test(NAME MultiplayerProtocolTest COMMAND MultiplayerProtocolTest)
add_test(NAME MultiplayerDeterminismTest COMMAND MultiplayerDeterminismTest)
add_test(NAME HostRemotePeerSimulationIsolationTest COMMAND HostRemotePeerSimulationIsolationTest)
add_test(NAME MultiplayerConnectionStateTest COMMAND MultiplayerConnectionStateTest)
add_test(NAME GameplayGeometryAndConfigTest COMMAND GameplayGeometryAndConfigTest)
add_test(NAME TargetLifecycleTest COMMAND TargetLifecycleTest)
add_test(NAME GameplayStateContractsTest COMMAND GameplayStateContractsTest)
add_test(NAME PhoneBodyContractTest COMMAND PhoneBodyContractTest)
add_test(NAME PhoneMenuLayoutTest COMMAND PhoneMenuLayoutTest)
add_test(NAME MenuNavigationTest COMMAND MenuNavigationTest)
add_test(NAME PhoneDisplayStateTest COMMAND PhoneDisplayStateTest)
add_test(NAME DesktopSaveRecoveryTest COMMAND DigitalBreakdown --save-roundtrip-test)
add_test(NAME GameplayFlowProbe COMMAND GameplayFlowProbe)
add_test(NAME RoomProgressionProbe COMMAND RoomProgressionProbe)
add_test(NAME DeterministicInputSoak COMMAND DeterministicInputSoak)

if(WIN32)
    target_link_libraries(DigitalBreakdown PRIVATE winmm winhttp)
endif()

add_custom_command(TARGET DigitalBreakdown POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/audio"
        "$<TARGET_FILE_DIR:DigitalBreakdown>/audio"
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/fonts"
        "$<TARGET_FILE_DIR:DigitalBreakdown>/fonts"
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
