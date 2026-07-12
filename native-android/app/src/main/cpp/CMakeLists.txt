cmake_minimum_required(VERSION 3.22.1)

project(digitalbreakdown LANGUAGES CXX)

set(DB_SHARED_SOURCES
    game/Game.cpp
    render/Renderer.cpp
)

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
        web/include
    )

    target_compile_features(digitalbreakdown PRIVATE cxx_std_17)

    if(DEFINED DIGITAL_BREAKDOWN_SOURCE_COMMIT)
        target_compile_definitions(
            digitalbreakdown
            PRIVATE
            DIGITAL_BREAKDOWN_SOURCE_COMMIT="${DIGITAL_BREAKDOWN_SOURCE_COMMIT}"
        )
    endif()

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
    )

    target_compile_features(digitalbreakdown PRIVATE cxx_std_17)

    target_link_libraries(
        digitalbreakdown
        android
        log
        EGL
        GLESv2
    )
endif()
