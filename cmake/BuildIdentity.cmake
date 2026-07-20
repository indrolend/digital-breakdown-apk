function(db_configure_build_identity repository_root output_directory)
    file(READ "${repository_root}/version.json" DB_VERSION_JSON)

    string(JSON DB_GAME_VERSION GET "${DB_VERSION_JSON}" gameVersion)
    string(JSON DB_RELEASE_STAGE GET "${DB_VERSION_JSON}" releaseStage)
    string(JSON DB_PROTOCOL_VERSION GET "${DB_VERSION_JSON}" protocolVersion)

    set(DB_BUILD_NUMBER "1")
    if(DEFINED DIGITAL_BREAKDOWN_BUILD_NUMBER AND NOT DIGITAL_BREAKDOWN_BUILD_NUMBER STREQUAL "")
        set(DB_BUILD_NUMBER "${DIGITAL_BREAKDOWN_BUILD_NUMBER}")
    elseif(DEFINED ENV{GITHUB_RUN_NUMBER} AND NOT "$ENV{GITHUB_RUN_NUMBER}" STREQUAL "")
        set(DB_BUILD_NUMBER "$ENV{GITHUB_RUN_NUMBER}")
    endif()

    set(DB_SOURCE_COMMIT "unknown")
    set(DB_SOURCE_DIRTY 0)
    if(DEFINED DIGITAL_BREAKDOWN_SOURCE_COMMIT AND NOT DIGITAL_BREAKDOWN_SOURCE_COMMIT STREQUAL "")
        set(DB_SOURCE_COMMIT "${DIGITAL_BREAKDOWN_SOURCE_COMMIT}")
    elseif(DEFINED ENV{GITHUB_SHA} AND NOT "$ENV{GITHUB_SHA}" STREQUAL "")
        set(DB_SOURCE_COMMIT "$ENV{GITHUB_SHA}")
    else()
        find_package(Git QUIET)
        if(GIT_FOUND)
            execute_process(
                COMMAND "${GIT_EXECUTABLE}" -C "${repository_root}" rev-parse --short HEAD
                OUTPUT_VARIABLE DB_GIT_COMMIT
                OUTPUT_STRIP_TRAILING_WHITESPACE
                ERROR_QUIET
            )
            if(NOT DB_GIT_COMMIT STREQUAL "")
                set(DB_SOURCE_COMMIT "${DB_GIT_COMMIT}")
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" -C "${repository_root}" status --porcelain
                    OUTPUT_VARIABLE DB_GIT_DIRTY
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                )
                if(NOT DB_GIT_DIRTY STREQUAL "")
                    set(DB_SOURCE_DIRTY 1)
                endif()
            endif()
        endif()
    endif()

    file(MAKE_DIRECTORY "${output_directory}")
    configure_file(
        "${repository_root}/cmake/GeneratedBuildIdentity.hpp.in"
        "${output_directory}/GeneratedBuildIdentity.hpp"
        @ONLY
    )

    set(DB_GAME_VERSION "${DB_GAME_VERSION}" PARENT_SCOPE)
    set(DB_RELEASE_STAGE "${DB_RELEASE_STAGE}" PARENT_SCOPE)
    set(DB_BUILD_NUMBER "${DB_BUILD_NUMBER}" PARENT_SCOPE)
    set(DB_SOURCE_COMMIT "${DB_SOURCE_COMMIT}" PARENT_SCOPE)
    set(DB_GENERATED_INCLUDE_DIR "${output_directory}" PARENT_SCOPE)

    message(STATUS "Digital Breakdown ${DB_GAME_VERSION}-${DB_RELEASE_STAGE} build ${DB_BUILD_NUMBER} (${DB_SOURCE_COMMIT})")
endfunction()
