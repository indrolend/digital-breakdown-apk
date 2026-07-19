cmake_minimum_required(VERSION 3.22.1)

project(digitalbreakdown LANGUAGES CXX)

set(DB_SHARED_SOURCES
    game/Game.cpp
    render/Renderer.cpp
    ../../../../../native-network/MultiplayerProtocol.cpp
)

set(DB_SOURCE_COMMIT "unknown")
if(DEFINED DIGITAL_BREAKDOWN_SOURCE_COMMIT AND NOT DIGITAL_BREAKDOWN_SOURCE_COMMIT STREQUAL "")
    set(DB_SOURCE_COMMIT "${DIGITAL_BREAKDOWN_SOURCE_COMMIT}")
elseif(DEFINED ENV{GITHUB_SHA} AND NOT "$ENV{GITHUB_SHA}" STREQUAL "")
    set(DB_SOURCE_COMMIT "$ENV{GITHUB_SHA}")
else()
    find_package(Git QUIET)
    if(GIT_FOUND)
        get_filename_component(DB_REPOSITORY_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/../../../../.." ABSOLUTE)
        execute_process(
            COMMAND "${GIT_EXECUTABLE}" -C "${DB_REPOSITORY_ROOT}" rev-parse --short HEAD
            OUTPUT_VARIABLE DB_GIT_COMMIT
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(NOT DB_GIT_COMMIT STREQUAL "")
            set(DB_SOURCE_COMMIT "${DB_GIT_COMMIT}")
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${DB_REPOSITORY_ROOT}" status --porcelain
                OUTPUT_VARIABLE DB_GIT_DIRTY
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(NOT DB_GIT_DIRTY STREQUAL "")
                string(APPEND DB_SOURCE_COMMIT "-dirty")
            endif()
        endif()
    endif()
endif()

message(STATUS "Digital Breakdown native source: ${DB_SOURCE_COMMIT}")

if(EMSCRIPTEN)
    add_executable(
        digitalbreakdown
        web/web_bridge.cpp
        ${DB_SHARED_SOURCES}
    )

    target_include_directories(
        digitalbreakdown
        PRIVATE
        game
        render
        ../../../../../native-network
        web/include
    )

    target_compile_features(digitalbreakdown PRIVATE cxx_std_17)
    target_compile_definitions(
        digitalbreakdown
        PRIVATE
        DIGITAL_BREAKDOWN_SOURCE_COMMIT="${DB_SOURCE_COMMIT}"
    )

    set_target_properties(
        digitalbreakdown
        PROPERTIES
        OUTPUT_NAME "digital-breakdown"
        SUFFIX ".js"
    )

    target_link_options(
        digitalbreakdown
        PRIVATE
        "-sWASM=1"
        "-sMIN_WEBGL_VERSION=1"
        "-sMAX_WEBGL_VERSION=1"
        "-sALLOW_MEMORY_GROWTH=1"
        "-sENVIRONMENT=web"
        "-sMODULARIZE=1"
        "-sEXPORT_NAME=createDigitalBreakdown"
        "-sEXPORTED_FUNCTIONS=['_db_init','_db_resize','_db_frame','_db_key','_db_touch_controls','_db_source_commit']"
        "-sEXPORTED_RUNTIME_METHODS=['cwrap','UTF8ToString']"
        "-sNO_EXIT_RUNTIME=1"
        "--no-entry"
    )
else()
    add_library(
        digitalbreakdown
        SHARED
        native_bridge.cpp
        ${DB_SHARED_SOURCES}
    )

    target_include_directories(
        digitalbreakdown
        PRIVATE
        game
        render
        ../../../../../native-network
    )

    target_compile_features(digitalbreakdown PRIVATE cxx_std_17)
    target_compile_definitions(
        digitalbreakdown
        PRIVATE
        DIGITAL_BREAKDOWN_SOURCE_COMMIT="${DB_SOURCE_COMMIT}"
    )

    target_link_libraries(
        digitalbreakdown
        android
        log
        EGL
        GLESv2
    )
endif()
