string(TIMESTAMP DB_BUILD_TIME "%Y-%m-%dT%H:%M:%SZ" UTC)

if(DB_COMMIT_OVERRIDE)
    set(DB_COMMIT_SHA "${DB_COMMIT_OVERRIDE}")
    set(DB_SOURCE_DIRTY false)
else()
    execute_process(
        COMMAND git -C "${DB_SOURCE_ROOT}" rev-parse HEAD
        OUTPUT_VARIABLE DB_COMMIT_SHA
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    execute_process(
        COMMAND git -C "${DB_SOURCE_ROOT}" status --porcelain --untracked-files=normal
        OUTPUT_VARIABLE DB_GIT_STATUS
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
    )
    if(DB_GIT_STATUS)
        set(DB_SOURCE_DIRTY true)
    else()
        set(DB_SOURCE_DIRTY false)
    endif()
endif()

if(NOT DB_COMMIT_SHA)
    set(DB_COMMIT_SHA "unknown")
endif()
string(LENGTH "${DB_COMMIT_SHA}" DB_COMMIT_LENGTH)
if(DB_COMMIT_LENGTH GREATER_EQUAL 7)
    string(SUBSTRING "${DB_COMMIT_SHA}" 0 7 DB_COMMIT_SHORT)
else()
    set(DB_COMMIT_SHORT "${DB_COMMIT_SHA}")
endif()

configure_file("${DB_TEMPLATE}" "${DB_OUTPUT}" @ONLY)
