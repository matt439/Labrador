# vcpkg.json owns the numeric version. A clean exact tag or a release archive's
# exported tag metadata identifies a release; every other source is development.
include("${CMAKE_CURRENT_LIST_DIR}/source_revision.cmake")

function(labrador_set_version source_directory)
    file(READ "${source_directory}/vcpkg.json" manifest)
    string(JSON numeric_version ERROR_VARIABLE version_error GET "${manifest}" version-string)
    if(version_error OR NOT numeric_version MATCHES "^(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)\\.(0|[1-9][0-9]*)$")
        message(FATAL_ERROR "Labrador's vcpkg.json needs a version-string of the form X.Y.Z")
    endif()
    set(major "${CMAKE_MATCH_1}")
    set(minor "${CMAKE_MATCH_2}")
    set(patch "${CMAKE_MATCH_3}")
    foreach(component major minor patch)
        string(LENGTH "${${component}}" component_length)
        if(component_length GREATER 10 OR ${component} GREATER 4294967295)
            message(FATAL_ERROR "Labrador's version ${component} must fit an unsigned 32-bit integer")
        endif()
    endforeach()

    labrador_set_source_revision("${source_directory}")
    set(development ON)
    if(EXISTS "${source_directory}/.git")
        # source_revision has already proved this is the engine's own clean
        # checkout. A nearest tag, or a consumer's enclosing repository, cannot
        # establish release identity.
        if(LABRADOR_SOURCE_REVISION MATCHES "^[0-9a-f]+$")
            find_package(Git QUIET)
            if(Git_FOUND)
                execute_process(
                    COMMAND "${GIT_EXECUTABLE}" --no-optional-locks -C "${source_directory}"
                        rev-parse --verify --quiet "refs/tags/v${numeric_version}^{commit}"
                    RESULT_VARIABLE tag_result
                    OUTPUT_VARIABLE tagged_commit
                    OUTPUT_STRIP_TRAILING_WHITESPACE
                    ERROR_QUIET
                    ENCODING UTF-8)
                if(tag_result STREQUAL "0" AND tagged_commit STREQUAL LABRADOR_SOURCE_REVISION)
                    set(development OFF)
                endif()
            endif()
        endif()
    elseif(EXISTS "${source_directory}/cmake/release_identity.txt")
        # git archive expands this file; the verified release starter packer
        # writes the same format. Plain source copies retain the placeholders.
        file(STRINGS "${source_directory}/cmake/release_identity.txt" archive_identity)
        list(LENGTH archive_identity identity_lines)
        if(identity_lines EQUAL 2)
            list(GET archive_identity 0 commit_line)
            list(GET archive_identity 1 refs_line)
            string(LENGTH "${commit_line}" commit_length)
            string(REPLACE "." "\\." tag_pattern "v${numeric_version}")
            if(commit_line MATCHES "^commit=[0-9a-f]+$" AND
                    (commit_length EQUAL 47 OR commit_length EQUAL 71) AND
                    refs_line MATCHES "^refs=" AND
                    refs_line MATCHES "(^refs=|, )tag: ${tag_pattern}(, |$)")
                set(development OFF)
            endif()
        endif()
    endif()

    set(version_string "${numeric_version}")
    if(development)
        string(APPEND version_string "-dev")
    endif()
    set(LABRADOR_VERSION "${numeric_version}" CACHE INTERNAL "Labrador's canonical numeric version" FORCE)
    set(LABRADOR_VERSION_MAJOR "${major}" CACHE INTERNAL "Labrador's major version" FORCE)
    set(LABRADOR_VERSION_MINOR "${minor}" CACHE INTERNAL "Labrador's minor version" FORCE)
    set(LABRADOR_VERSION_PATCH "${patch}" CACHE INTERNAL "Labrador's patch version" FORCE)
    set(LABRADOR_VERSION_STRING "${version_string}" CACHE INTERNAL "Labrador's release or development version" FORCE)
    set(LABRADOR_VERSION_IS_DEVELOPMENT "${development}" CACHE INTERNAL "Whether release identity is unverified" FORCE)
endfunction()
