# Offline configure/metadata fixtures; no compiler, network or original-checkout
# mutations. Run with cmake -DREPO_DIR=<absolute checkout> -P this-file.
# Fixtures remain under out/source-revision-check for inspection. The CI lint
# job runs this independently of the compiler matrix, without a ctest entry.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_source_revision.cmake: REPO_DIR is not set")
endif()
file(REAL_PATH "${REPO_DIR}" REPO_DIR)
include("${REPO_DIR}/cmake/source_revision.cmake")
find_package(Git REQUIRED)
find_program(revision_node NAMES node REQUIRED)
get_filename_component(cmake_bin "${CMAKE_COMMAND}" DIRECTORY)
find_program(revision_ninja NAMES ninja ninja-build
    HINTS "${cmake_bin}/../../Ninja" REQUIRED)

string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_suffix)
set(fixture "${REPO_DIR}/out/source-revision-check/${fixture_suffix}")
if(EXISTS "${fixture}")
    message(FATAL_ERROR "Refusing to overwrite an existing fixture: ${fixture}")
endif()
file(MAKE_DIRECTORY "${fixture}/hooks")

function(fixture_git repository)
    execute_process(
        COMMAND "${GIT_EXECUTABLE}" --no-optional-locks -C "${repository}"
            -c user.name=RevisionFixture -c user.email=revision@example.invalid
            -c commit.gpgsign=false "-c" "core.hooksPath=${fixture}/hooks"
            -c core.excludesFile= ${ARGN}
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        OUTPUT_STRIP_TRAILING_WHITESPACE
        ENCODING UTF-8)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Fixture git ${ARGN} failed: ${result}\n${error}")
    endif()
    set(fixture_git_output "${output}" PARENT_SCOPE)
endfunction()

function(expect_revision source expected)
    labrador_set_source_revision("${source}")
    if(NOT LABRADOR_SOURCE_REVISION STREQUAL expected)
        message(FATAL_ERROR "${source}: expected ${expected}, got ${LABRADOR_SOURCE_REVISION}")
    endif()
endfunction()

function(configure_consumer expected)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -S "${fixture}/consumer"
            -B "${fixture}/consumer-build" -G Ninja
            "-DCMAKE_MAKE_PROGRAM=${revision_ninja}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        ENCODING UTF-8)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Fixture consumer configure failed: ${result}\n${output}\n${error}")
    endif()
    file(READ "${fixture}/consumer-build/engine-revision.txt" actual)
    string(STRIP "${actual}" actual)
    if(NOT actual STREQUAL expected)
        message(FATAL_ERROR "Consumer expected ${expected}, got ${actual}")
    endif()
endfunction()

function(check_bundle source expected_commit expected_dirty name)
    execute_process(
        COMMAND "${revision_node}" "${REPO_DIR}/tools/web_bundle/bundle.mjs"
            --out "${fixture}/bundle-${name}" --repository "${source}"
            --emscripten "${fixture}/emscripten"
            --file "fixture.txt=${fixture}/notice.txt"
            --notice "${fixture}/notice.txt"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE error
        ENCODING UTF-8)
    if(NOT result STREQUAL "0")
        message(FATAL_ERROR "Fixture bundle failed: ${result}\n${output}\n${error}")
    endif()
    file(READ "${fixture}/bundle-${name}/BUILD.json" metadata)
    string(JSON actual_commit GET "${metadata}" commit)
    string(JSON actual_dirty GET "${metadata}" dirty)
    if(NOT actual_commit STREQUAL expected_commit OR NOT actual_dirty STREQUAL expected_dirty)
        message(FATAL_ERROR "Bundle expected ${expected_commit}/${expected_dirty}, got ${actual_commit}/${actual_dirty}")
    endif()
endfunction()

# The actual checkout is read-only throughout the suite. This comparison is
# the CI assertion that the exported configure value names the engine's HEAD.
if(EXISTS "${REPO_DIR}/.git")
    fixture_git("${REPO_DIR}" rev-parse --verify HEAD)
    set(expected_checkout "${fixture_git_output}")
    fixture_git("${REPO_DIR}" status --porcelain --untracked-files=no)
    if(NOT fixture_git_output STREQUAL "")
        string(APPEND expected_checkout "-dirty")
    endif()
    expect_revision("${REPO_DIR}" "${expected_checkout}")
endif()

file(MAKE_DIRECTORY "${fixture}/checkout" "${fixture}/consumer" "${fixture}/emscripten")
file(WRITE "${fixture}/emscripten/emscripten-version.txt" "\"fixture\"\n")
file(WRITE "${fixture}/notice.txt" "Fixture notice\n")
file(WRITE "${fixture}/checkout/tracked.txt" "original\n")
file(WRITE "${fixture}/checkout/CMakeLists.txt"
    "include(\"${REPO_DIR}/cmake/source_revision.cmake\")\n"
    "labrador_set_source_revision(\"\${CMAKE_CURRENT_SOURCE_DIR}\")\n")
file(WRITE "${fixture}/consumer/CMakeLists.txt"
    "cmake_minimum_required(VERSION 3.26)\nproject(RevisionConsumer NONE)\n"
    "add_subdirectory(\"${fixture}/checkout\" engine)\n"
    "file(WRITE \"\${CMAKE_CURRENT_BINARY_DIR}/engine-revision.txt\" \"\${LABRADOR_SOURCE_REVISION}\\n\")\n")
fixture_git("${fixture}/checkout" init -q)
fixture_git("${fixture}/checkout" add CMakeLists.txt tracked.txt)
fixture_git("${fixture}/checkout" commit -q -m initial)
fixture_git("${fixture}/checkout" rev-parse HEAD)
set(first_commit "${fixture_git_output}")
expect_revision("${fixture}/checkout" "${first_commit}")
configure_consumer("${first_commit}")
check_bundle("${fixture}/checkout" "${first_commit}" OFF clean)

# Untracked files do not define -dirty, matching the web bundle's contract.
file(WRITE "${fixture}/checkout/untracked.txt" "not part of the pin\n")
expect_revision("${fixture}/checkout" "${first_commit}")
file(WRITE "${fixture}/checkout/tracked.txt" "modified\n")
expect_revision("${fixture}/checkout" "${first_commit}-dirty")
configure_consumer("${first_commit}-dirty")
check_bundle("${fixture}/checkout" "${first_commit}" ON dirty)
fixture_git("${fixture}/checkout" add tracked.txt)
fixture_git("${fixture}/checkout" commit -q -m changed)
fixture_git("${fixture}/checkout" rev-parse HEAD)
set(second_commit "${fixture_git_output}")
if(first_commit STREQUAL second_commit)
    message(FATAL_ERROR "Fixture commit did not move")
endif()
expect_revision("${fixture}/checkout" "${second_commit}")
configure_consumer("${second_commit}")

# A real worktree has a .git file rather than a directory, as a submodule does.
fixture_git("${fixture}/checkout" worktree add -q --detach "${fixture}/worktree" HEAD)
if(IS_DIRECTORY "${fixture}/worktree/.git")
    message(FATAL_ERROR "The worktree fixture must exercise a .git file")
endif()
expect_revision("${fixture}/worktree" "${second_commit}")
check_bundle("${fixture}/worktree" "${second_commit}" OFF worktree)

file(MAKE_DIRECTORY "${fixture}/unborn")
fixture_git("${fixture}/unborn" init -q)
expect_revision("${fixture}/unborn" unknown)
check_bundle("${fixture}/unborn" unknown "" unborn)

# This archive has no own marker, even though Git could walk to checkout/.
file(MAKE_DIRECTORY "${fixture}/checkout/archive/labrador")
expect_revision("${fixture}/checkout/archive/labrador" unknown)
check_bundle("${fixture}/checkout/archive/labrador" unknown "" nested-archive)
file(MAKE_DIRECTORY "${fixture}/standalone-archive")
expect_revision("${fixture}/standalone-archive" unknown)

# Missing Git is expected absence, including when this build previously saw it.
set(CMAKE_DISABLE_FIND_PACKAGE_Git TRUE)
expect_revision("${fixture}/checkout" unknown)
unset(CMAKE_DISABLE_FIND_PACKAGE_Git)

message(STATUS "Source revision fixtures passed: checkout, consumer scope, dirty/clean reconfigure, worktree, unborn, archives and missing Git")
message(STATUS "Fixture evidence: ${fixture}")
