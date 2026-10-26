# The engine's configure-time source identity, visible to a consumer after
# add_subdirectory() through an INTERNAL cache value. Recomputed on every
# configure: a cached answer from another pin or a previous dirty tree is not
# the identity of the source being configured now.
#
# A source archive can sit inside a consumer's Git checkout. Require this
# tree's own .git marker, including a submodule/worktree's .git file, before
# asking Git anything, and check the root it reports. Walking upward and
# stamping the consumer's commit would give an archive a plausible false name.
function(labrador_set_source_revision source_directory)
    set(LABRADOR_SOURCE_REVISION "unknown" CACHE INTERNAL
        "Labrador's configure-time commit, optionally suffixed -dirty, or unknown"
        FORCE)

    if(NOT EXISTS "${source_directory}/.git")
        return()
    endif()

    # Optional package discovery can be disabled by a consumer. Do not inherit
    # a previous caller's successful lookup when FindGit does not run.
    set(Git_FOUND FALSE)
    find_package(Git QUIET)
    if(NOT Git_FOUND)
        return()
    endif()

    file(REAL_PATH "${source_directory}" source_root)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --no-optional-locks -C "${source_root}"
            rev-parse --show-toplevel
        RESULT_VARIABLE root_result
        OUTPUT_VARIABLE git_root
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        ENCODING UTF-8)
    if(NOT root_result STREQUAL "0")
        return()
    endif()
    file(REAL_PATH "${git_root}" git_root)
    if(CMAKE_HOST_WIN32)
        string(TOLOWER "${source_root}" source_root_comparison)
        string(TOLOWER "${git_root}" git_root_comparison)
    else()
        set(source_root_comparison "${source_root}")
        set(git_root_comparison "${git_root}")
    endif()
    if(NOT git_root_comparison STREQUAL source_root_comparison)
        return()
    endif()

    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --no-optional-locks -C "${source_root}"
            rev-parse --verify HEAD
        RESULT_VARIABLE commit_result
        OUTPUT_VARIABLE commit
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        ENCODING UTF-8)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --no-optional-locks -C "${source_root}"
            status --porcelain --untracked-files=no
        RESULT_VARIABLE dirty_result
        OUTPUT_VARIABLE dirty
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ERROR_QUIET
        ENCODING UTF-8)
    if(NOT commit_result STREQUAL "0" OR NOT dirty_result STREQUAL "0"
            OR NOT commit MATCHES "^[0-9a-f]+$")
        return()
    endif()

    if(NOT dirty STREQUAL "")
        string(APPEND commit "-dirty")
    endif()
    set(LABRADOR_SOURCE_REVISION "${commit}" CACHE INTERNAL
        "Labrador's configure-time commit, optionally suffixed -dirty, or unknown"
        FORCE)
endfunction()
