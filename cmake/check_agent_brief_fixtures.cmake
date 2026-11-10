# Compiler-free fixtures for the agent brief check, retained under out/ for
# review. Each case is a small AGENTS.md, CLAUDE.md and ctest inventory of its
# own, so the cases do not move when the real brief or inventory does.
# CASE optionally selects one exact named case for a focused regression run.
# Run: cmake -DREPO_DIR=<checkout> [-DCASE=<name>] -P this-file.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_agent_brief_fixtures.cmake: REPO_DIR is not set")
endif()
file(REAL_PATH "${REPO_DIR}" REPO_DIR)
set(checker "${REPO_DIR}/cmake/check_agent_brief.cmake")
if(NOT EXISTS "${checker}")
    message(FATAL_ERROR "Missing agent brief checker: ${checker}")
endif()
string(RANDOM LENGTH 12 ALPHABET 0123456789abcdef fixture_suffix)
set(fixture "${REPO_DIR}/out/agent-brief-check/${fixture_suffix}")
if(EXISTS "${fixture}")
    message(FATAL_ERROR "Refusing to overwrite an existing fixture: ${fixture}")
endif()
set(selected_count 0)

# The inventory every case starts from. two-debug and two-release register 3
# entries, three-debug 4, and web-release 6: 4 not named Firefox* and 2 that
# are. spare-release registers 2 and has no row in the brief.
set(base_inventory [=[
{
  "configurations": ["two-debug", "two-release", "three-debug", "web-release", "spare-release"],
  "every_configuration": ["CoreTests", "MathTests"],
  "some_configurations": {
    "SmokeTests": ["two-debug", "two-release", "three-debug"],
    "ExtraTests": ["three-debug"],
    "PageTests": ["web-release"],
    "PixelTests": ["web-release"],
    "FirefoxPageTests": ["web-release"],
    "FirefoxPixelTests": ["web-release"]
  },
  "chromium_registered": ["PageTests", "PixelTests"]
}
]=])

# The brief every case starts from, which states those counts. three-release
# and web-debug are presets the inventory does not list.
set(base_brief [=[
# A brief

Prose before the table; with [a link](README.md) and a semicolon.

| Preset pair | Render / audio | ctest entries and device requirements |
|---|---|---|
| `two-debug`, `two-release` | D3D11 / XAudio2 | 3; debug permits WARP fallback |
| `three-debug`, `three-release` | D3D12 / XAudio2 | 4; debug permits WARP fallback |
| `web-debug`, `web-release` | WebGL2 / Web Audio | 3 Debug / 4 Release with Chromium, 2 more with Firefox, 2 without; Node plus browser pages |

Where configure finds a Firefox, two `Firefox*` entries run the pages.

| Changed area | Planned selections |
|---|---|
| `engine/render/` | 5 presets; not a row of the table above |
]=])

# Writes one case and runs the checker on it. `expected` is "pass" or "fail".
# Every further argument is text the checker's output must contain: its error
# for a failing case, and its summary for a passing one. `brief`, `inventory`
# and `claude` are the three files' contents; an empty inventory writes none.
function(check_fixture name expected brief inventory claude)
    if(DEFINED CASE AND NOT CASE STREQUAL name)
        return()
    endif()
    set(root "${fixture}/${name}")
    if(EXISTS "${root}")
        message(FATAL_ERROR "Refusing to overwrite an existing case: ${root}")
    endif()
    file(MAKE_DIRECTORY "${root}/tests")
    file(WRITE "${root}/AGENTS.md" "${brief}")
    file(WRITE "${root}/CLAUDE.md" "${claude}")
    if(NOT inventory STREQUAL "")
        file(WRITE "${root}/tests/ctest_inventory.json" "${inventory}")
    endif()
    execute_process(COMMAND "${CMAKE_COMMAND}" "-DREPO_DIR=${root}" -P "${checker}"
        RESULT_VARIABLE result OUTPUT_VARIABLE output ERROR_VARIABLE error)
    if(expected STREQUAL "pass")
        if(NOT result STREQUAL "0")
            message(FATAL_ERROR "Allowed fixture ${name} failed:\n${output}\n${error}")
        endif()
        set(searched "${output}")
    else()
        if(result STREQUAL "0")
            message(FATAL_ERROR "Forbidden fixture ${name} passed:\n${output}")
        endif()
        set(searched "${error}")
    endif()
    foreach(needle IN LISTS ARGN)
        string(FIND "${searched}" "${needle}" position)
        if(position EQUAL -1)
            message(FATAL_ERROR
                "Fixture ${name} did not say \"${needle}\":\n${output}\n${error}")
        endif()
    endforeach()
    math(EXPR next_count "${selected_count} + 1")
    set(selected_count "${next_count}" PARENT_SCOPE)
endfunction()

# A case whose brief is the base brief with one piece of text replaced. The
# replaced text must be there, so a case cannot pass by changing nothing.
function(check_brief_change name expected old new)
    string(FIND "${base_brief}" "${old}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Fixture ${name} replaces text the base brief does not have: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" brief "${base_brief}")
    check_fixture(${name} ${expected} "${brief}" "${base_inventory}" "@AGENTS.md\n" ${ARGN})
    set(selected_count "${selected_count}" PARENT_SCOPE)
endfunction()

# The same for the inventory.
function(check_inventory_change name expected old new)
    string(FIND "${base_inventory}" "${old}" position)
    if(position EQUAL -1)
        message(FATAL_ERROR "Fixture ${name} replaces text the base inventory does not have: ${old}")
    endif()
    string(REPLACE "${old}" "${new}" inventory "${base_inventory}")
    check_fixture(${name} ${expected} "${base_brief}" "${inventory}" "@AGENTS.md\n" ${ARGN})
    set(selected_count "${selected_count}" PARENT_SCOPE)
endfunction()

# What passes, and what a passing run says it left alone.
check_fixture(base pass "${base_brief}" "${base_inventory}" "@AGENTS.md\n"
    "6 ctest counts in AGENTS.md match"
    "4 for `three-release`, which the inventory does not list"
    "3 for `web-debug`, which the inventory does not list"
    "\"2 without\" in row `web-debug`, `web-release`, which the inventory has no source for"
    "the 2 entries of `spare-release`, which has no row in the table")
string(REPLACE "\n" "\r\n" crlf_brief "${base_brief}")
check_fixture(carriage_returns pass "${crlf_brief}" "${base_inventory}" "@AGENTS.md\r\n"
    "6 ctest counts in AGENTS.md match")
check_brief_change(firefox_count_in_digits pass
    "two `Firefox*` entries" "2 `Firefox*` entries"
    "6 ctest counts in AGENTS.md match")
check_brief_change(firefox_prose_wrapped pass
    "two `Firefox*` entries" "two\n`Firefox*` entries"
    "6 ctest counts in AGENTS.md match")
check_brief_change(firefox_prose_absent pass
    "two `Firefox*` entries" "its Firefox entries"
    "5 ctest counts in AGENTS.md match"
    "the prose count of `Firefox*` entries, which was not found")
check_brief_change(row_outside_inventory pass
    "| `web-debug`" "| `other-debug`, `other-release` | GL / XAudio2 | 9; a driver |\n| `web-debug`"
    "9 for `other-debug`, which the inventory does not list"
    "9 for `other-release`, which the inventory does not list")

# The import line, which the count check must not have displaced.
check_fixture(copied_brief fail "${base_brief}" "${base_inventory}" "${base_brief}"
    "The agent brief has drifted" "CLAUDE.md")

# A stated count that differs: the row, the stated count and the derived one.
check_brief_change(native_count_high fail
    "| 4; debug" "| 5; debug"
    "row `three-debug`, `three-release`" "states 5 ctest entries for `three-debug`" "derives 4")
check_brief_change(native_count_low fail
    "| 3; debug" "| 2; debug"
    "row `two-debug`, `two-release`" "states 2 ctest entries for `two-debug`"
    "states 2 ctest entries for `two-release`" "derives 3")
check_brief_change(release_count fail
    "4 Release with Chromium" "5 Release with Chromium"
    "row `web-debug`, `web-release`"
    "states 5 ctest entries not named Firefox* for `web-release`" "derives 4")
check_brief_change(firefox_count fail
    "2 more with Firefox" "3 more with Firefox"
    "row `web-debug`, `web-release`" "states 3 Firefox* ctest entries for `web-release`" "derives 2")
check_brief_change(firefox_prose_count fail
    "two `Firefox*` entries" "three `Firefox*` entries"
    "prose \"three `Firefox*` entries\"" "states 3" "derives 2")
check_brief_change(firefox_prose_not_a_number fail
    "two `Firefox*` entries" "several `Firefox*` entries"
    "prose \"several `Firefox*` entries\"" "not a number this check reads" "derives 2")
check_brief_change(two_counts_wrong fail
    "| 3; debug permits WARP fallback |\n| `three-debug`, `three-release` | D3D12 / XAudio2 | 4;"
    "| 7; debug permits WARP fallback |\n| `three-debug`, `three-release` | D3D12 / XAudio2 | 8;"
    "states 7 ctest entries for `two-debug`" "states 8 ctest entries for `three-debug`")

# The inventory moving under an unchanged brief, which is how the counts drift.
check_inventory_change(entry_added fail
    "\"ExtraTests\": [\"three-debug\"]" "\"ExtraTests\": [\"three-debug\", \"two-release\"]"
    "states 3 ctest entries for `two-release`" "derives 4")
check_inventory_change(shared_entry_added fail
    "[\"CoreTests\", \"MathTests\"]" "[\"CoreTests\", \"MathTests\", \"SceneTests\"]"
    "states 3 ctest entries for `two-debug`" "derives 4"
    "states 4 ctest entries not named Firefox* for `web-release`" "derives 5")
check_inventory_change(firefox_entry_added fail
    "\"FirefoxPixelTests\": [\"web-release\"]"
    "\"FirefoxPixelTests\": [\"web-release\"],\n    \"FirefoxWindowTests\": [\"web-release\"]"
    "states 2 Firefox* ctest entries for `web-release`" "derives 3"
    "prose \"two `Firefox*` entries\"")

# A row reworded so that nothing in it can be checked fails; it does not pass
# by no longer being read.
check_brief_change(count_reworded fail
    "| 4; debug" "| about 4; debug"
    "row `three-debug`, `three-release`"
    "states no count for `three-debug` that can be checked" "derives 4")
check_brief_change(count_removed fail
    "| 3; debug permits" "| debug permits"
    "states no count for `two-debug` that can be checked" "derives 3")
check_brief_change(release_clause_reworded fail
    "4 Release with Chromium" "4 in Release"
    "states no count for `web-release` that can be checked" "derives 6")
check_brief_change(firefox_clause_removed fail
    ", 2 more with Firefox" ""
    "states no \"N more with Firefox\" for `web-release`" "derives 2")
check_brief_change(build_type_without_preset fail
    "| `three-debug`, `three-release` | D3D12 / XAudio2 | 4; debug"
    "| `three-debug` | D3D12 / XAudio2 | 4 Release; debug"
    "row `three-debug`" "\"4 Release\" matches no Release preset in the row")
check_brief_change(table_header_renamed fail
    "| Preset pair |" "| Presets |"
    "AGENTS.md's preset table was not found")
check_brief_change(row_without_backticks fail
    "| `three-debug`, `three-release` |" "| three-debug, three-release |"
    "names no preset in backticks")
check_brief_change(row_with_two_cells fail
    "| `three-debug`, `three-release` | D3D12 / XAudio2 | 4; debug permits WARP fallback |"
    "| `three-debug`, `three-release` | 4"
    "a row does not have three cells")

# An inventory this cannot read fails and names the file.
check_fixture(inventory_missing fail "${base_brief}" "" "@AGENTS.md\n"
    "The ctest inventory is missing" "ctest_inventory.json")
check_fixture(inventory_not_json fail "${base_brief}" "not json" "@AGENTS.md\n"
    "The ctest inventory cannot be read" "ctest_inventory.json")
check_inventory_change(inventory_without_shared_list fail
    "\"every_configuration\"" "\"each_configuration\""
    "The ctest inventory cannot be read")
check_inventory_change(unknown_configuration fail
    "\"ExtraTests\": [\"three-debug\"]" "\"ExtraTests\": [\"four-debug\"]"
    "ExtraTests names four-debug" "ctest_inventory.json")

# CMake reflows a message's unindented lines, so the texts above are either
# short openings or the indented lines that carry the counts.

# A table none of whose presets the inventory lists has checked nothing.
check_fixture(no_configuration_in_table fail "${base_brief}"
    "{\"configurations\": [\"spare-release\"], \"every_configuration\": [\"CoreTests\"], \"some_configurations\": {}}"
    "@AGENTS.md\n"
    "no row names a configuration of tests/ctest_inventory.json")

if(selected_count EQUAL 0)
    message(FATAL_ERROR "Unknown agent brief fixture CASE: ${CASE}")
endif()
message(STATUS "Agent brief fixtures passed: ${selected_count} cases at ${fixture}")
