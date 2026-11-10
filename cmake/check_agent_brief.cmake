# AGENTS.md is the agent brief, and CLAUDE.md is not a second copy of it: its
# first line is "@AGENTS.md", which Claude Code reads as an import of the whole
# brief. Anything below that line is for Claude Code alone. A brief kept as two
# copies drifts the first time an amendment is made to one and not the other,
# and nothing would notice: each agent would go on reading its own copy as the
# truth (T4). So the build checks that CLAUDE.md still imports AGENTS.md
# rather than carrying a copy, and names the file when it does not (T5, T6).
#
# The brief's preset table is a second copy of another kind: it states how many
# ctest entries each preset pair registers, and tests/ctest_inventory.json is
# the list CI holds each of its configurations to. A count written by hand
# beside a machine source drifts the first time an entry is added, so the
# counts are derived from the inventory here, and a difference names the row,
# the stated count and the derived one. The table speaks of preset pairs and
# the inventory of CI configurations, and the two overlap without matching:
#
#   - A bare count is checked for each preset of its row that the inventory
#     lists. A preset the inventory does not list is reported as not checked.
#   - "N Debug" and "N Release", with or without "with Chromium", are the
#     entries of the row's debug or release preset whose names do not begin
#     with "Firefox". "N more with Firefox" is the ones that do.
#   - Any other clause, such as a count without a browser, has no source in
#     the inventory and is reported as not checked rather than guessed at.
#
# A row with a preset in the inventory and no count this can check fails, so
# rewording a row cannot quietly switch its check off. The prose sentence
# "<number> `Firefox*` entries" is checked where it appears.
#
# This is CMake rather than Python because the brief check is a build step and
# the build needs no Python. The inventory is three flat collections, which
# string(JSON) reads directly. check_agent_brief_fixtures.cmake proves the
# failures.
#
# Top-level builds only, like the citation check: a consumer of the engine
# does not carry this repository's agent brief.
#
# Run with: cmake -DREPO_DIR=<path> -P cmake/check_agent_brief.cmake
# It prints what it did not check, unless -DLIST_NOT_CHECKED=OFF asks for the
# number alone.
cmake_minimum_required(VERSION 3.26)

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_agent_brief.cmake: REPO_DIR is not set")
endif()

set(claude_brief "${REPO_DIR}/CLAUDE.md")
set(agents_brief "${REPO_DIR}/AGENTS.md")

foreach(path IN ITEMS "${agents_brief}" "${claude_brief}")
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "The agent brief ${path} is missing.\n"
            "  See CONTRIBUTING.md, The agent brief.")
    endif()
endforeach()

# The first line of CLAUDE.md, with a trailing carriage return dropped so that
# a checkout's autocrlf setting cannot make a correct file fail.
file(READ "${claude_brief}" text)
string(FIND "${text}" "\n" first_newline)
if(first_newline EQUAL -1)
    set(first_line "${text}")
else()
    string(SUBSTRING "${text}" 0 ${first_newline} first_line)
endif()
string(REGEX REPLACE "\r$" "" first_line "${first_line}")

if(NOT first_line STREQUAL "@AGENTS.md")
    message(FATAL_ERROR
        "The agent brief has drifted. The first line of this file must be "
        "\"@AGENTS.md\":\n"
        "    ${claude_brief}\n"
        "  Make the amendment in AGENTS.md only; CLAUDE.md imports it. "
        "See CONTRIBUTING.md, The agent brief.")
endif()

# The inventory's counts.
set(inventory_path "${REPO_DIR}/tests/ctest_inventory.json")
if(NOT EXISTS "${inventory_path}")
    message(FATAL_ERROR "The ctest inventory is missing:\n"
        "    ${inventory_path}\n"
        "  AGENTS.md's preset table is checked against it.")
endif()
file(READ "${inventory_path}" inventory)

# string(JSON <mode> <inventory> <path...>), failing with the file's name.
function(inventory_query variable mode)
    string(JSON value ERROR_VARIABLE json_error ${mode} "${inventory}" ${ARGN})
    if(json_error)
        message(FATAL_ERROR "The ctest inventory cannot be read:\n"
            "    ${inventory_path}\n"
            "  ${json_error}")
    endif()
    set("${variable}" "${value}" PARENT_SCOPE)
endfunction()

# total_<configuration> is every entry the configuration registers, and
# firefox_<configuration> those among them named Firefox*. firefox_names
# counts each such entry once, whichever configurations register it.
set(configurations "")
inventory_query(configuration_count LENGTH configurations)
inventory_query(shared_count LENGTH every_configuration)
if(configuration_count GREATER 0)
    math(EXPR last_configuration "${configuration_count} - 1")
    foreach(index RANGE ${last_configuration})
        inventory_query(configuration GET configurations ${index})
        list(APPEND configurations "${configuration}")
        set("total_${configuration}" ${shared_count})
        set("firefox_${configuration}" 0)
    endforeach()
endif()
set(firefox_names 0)
inventory_query(entry_count LENGTH some_configurations)
if(entry_count GREATER 0)
    math(EXPR last_entry "${entry_count} - 1")
    foreach(entry_index RANGE ${last_entry})
        inventory_query(entry MEMBER some_configurations ${entry_index})
        if("${entry}" MATCHES "^Firefox")
            math(EXPR firefox_names "${firefox_names} + 1")
        endif()
        inventory_query(user_count LENGTH some_configurations "${entry}")
        if(user_count GREATER 0)
            math(EXPR last_user "${user_count} - 1")
            foreach(user_index RANGE ${last_user})
                inventory_query(configuration GET some_configurations "${entry}" ${user_index})
                if(NOT "${configuration}" IN_LIST configurations)
                    message(FATAL_ERROR "The ctest inventory's ${entry} names "
                        "${configuration}, which is not in its configurations:\n"
                        "    ${inventory_path}")
                endif()
                math(EXPR "total_${configuration}" "${total_${configuration}} + 1")
                if("${entry}" MATCHES "^Firefox")
                    math(EXPR "firefox_${configuration}" "${firefox_${configuration}} + 1")
                endif()
            endforeach()
        endif()
    endforeach()
endif()

# The brief as one string with no carriage returns, and with the three
# characters CMake's lists give meaning to replaced, so that a table row can be
# a list element whatever its prose contains. No message below may contain a
# semicolon, for the same reason.
file(READ "${agents_brief}" brief)
string(ASCII 29 open_bracket)
string(ASCII 30 close_bracket)
string(ASCII 31 semicolon)
string(REPLACE "\r" "" brief "${brief}")
string(REPLACE "[" "${open_bracket}" brief "${brief}")
string(REPLACE "]" "${close_bracket}" brief "${brief}")
string(REPLACE ";" "${semicolon}" brief "${brief}")

# The table runs from its header row to the first blank line.
string(FIND "${brief}" "\n| Preset pair |" table_start)
if(table_start EQUAL -1)
    message(FATAL_ERROR
        "AGENTS.md's preset table was not found. Its header row must begin "
        "\"| Preset pair |\":\n"
        "    ${agents_brief}\n"
        "  The table's ctest counts are checked against tests/ctest_inventory.json.")
endif()
math(EXPR table_start "${table_start} + 1")
string(SUBSTRING "${brief}" ${table_start} -1 table)
string(FIND "${table}" "\n\n" table_end)
if(NOT table_end EQUAL -1)
    string(SUBSTRING "${table}" 0 ${table_end} table)
endif()
string(REGEX MATCHALL "\n\\|[^\n]*" rows "${table}")

set(problems "")
set(not_checked "")
set(checked_count 0)
set(configurations_in_rows "")

# Compares one stated count with the inventory's and records a difference.
function(compare_count row preset what stated derived)
    if(NOT "${stated}" EQUAL "${derived}")
        list(APPEND problems
            "row ${row}: states ${stated} ${what} for `${preset}`, and tests/ctest_inventory.json derives ${derived}")
        set(problems "${problems}" PARENT_SCOPE)
    endif()
    math(EXPR checked_count "${checked_count} + 1")
    set(checked_count "${checked_count}" PARENT_SCOPE)
endfunction()

foreach(row IN LISTS rows)
    if("${row}" MATCHES "^\n\\|[-|: ]+$")
        continue()
    endif()
    if(NOT "${row}" MATCHES "^\n\\|([^|]*)\\|([^|]*)\\|([^|]*)\\|")
        string(STRIP "${row}" row)
        list(APPEND problems "a row does not have three cells: ${row}")
        continue()
    endif()
    set(preset_cell "${CMAKE_MATCH_1}")
    set(count_cell "${CMAKE_MATCH_3}")
    string(STRIP "${preset_cell}" row_name)
    string(REGEX MATCHALL "`[^`]+`" presets "${preset_cell}")
    string(REPLACE "`" "" presets "${presets}")
    if("${presets}" STREQUAL "")
        list(APPEND problems "row ${row_name}: names no preset in backticks")
        continue()
    endif()

    # The counts are the cell's text before its first semicolon, separated by
    # commas and slashes.
    string(REGEX REPLACE "${semicolon}.*$" "" clause "${count_cell}")
    string(REGEX REPLACE "[,/]" ";" parts "${clause}")
    set(total_checked "")
    set(firefox_checked "")
    foreach(part IN LISTS parts)
        string(STRIP "${part}" part)
        if("${part}" MATCHES "^([0-9]+)$")
            set(stated "${CMAKE_MATCH_1}")
            foreach(preset IN LISTS presets)
                if("${preset}" IN_LIST configurations)
                    compare_count("${row_name}" "${preset}" "ctest entries"
                        "${stated}" "${total_${preset}}")
                    list(APPEND total_checked "${preset}")
                    list(APPEND firefox_checked "${preset}")
                else()
                    list(APPEND not_checked
                        "${stated} for `${preset}`, which the inventory does not list")
                endif()
            endforeach()
        elseif("${part}" MATCHES "^([0-9]+) (Debug|Release)( with Chromium)?$")
            set(stated "${CMAKE_MATCH_1}")
            set(build_type "${CMAKE_MATCH_2}")
            string(TOLOWER "${build_type}" build_suffix)
            set(matched FALSE)
            foreach(preset IN LISTS presets)
                if("${preset}" MATCHES "-${build_suffix}(-|$)")
                    set(matched TRUE)
                    if("${preset}" IN_LIST configurations)
                        math(EXPR derived "${total_${preset}} - ${firefox_${preset}}")
                        compare_count("${row_name}" "${preset}"
                            "ctest entries not named Firefox*" "${stated}" "${derived}")
                        list(APPEND total_checked "${preset}")
                    else()
                        list(APPEND not_checked
                            "${stated} for `${preset}`, which the inventory does not list")
                    endif()
                endif()
            endforeach()
            if(NOT matched)
                list(APPEND problems
                    "row ${row_name}: \"${part}\" matches no ${build_type} preset in the row")
            endif()
        elseif("${part}" MATCHES "^([0-9]+) more with Firefox$")
            set(stated "${CMAKE_MATCH_1}")
            foreach(preset IN LISTS presets)
                if("${preset}" IN_LIST configurations)
                    compare_count("${row_name}" "${preset}" "Firefox* ctest entries"
                        "${stated}" "${firefox_${preset}}")
                    list(APPEND firefox_checked "${preset}")
                endif()
            endforeach()
        elseif(NOT "${part}" STREQUAL "")
            list(APPEND not_checked
                "\"${part}\" in row ${row_name}, which the inventory has no source for")
        endif()
    endforeach()

    foreach(preset IN LISTS presets)
        if(NOT "${preset}" IN_LIST configurations)
            continue()
        endif()
        list(APPEND configurations_in_rows "${preset}")
        if(NOT "${preset}" IN_LIST total_checked)
            list(APPEND problems
                "row ${row_name}: states no count for `${preset}` that can be checked, and tests/ctest_inventory.json derives ${total_${preset}}")
        elseif("${firefox_${preset}}" GREATER 0 AND NOT "${preset}" IN_LIST firefox_checked)
            list(APPEND problems
                "row ${row_name}: states no \"N more with Firefox\" for `${preset}`, and tests/ctest_inventory.json derives ${firefox_${preset}}")
        endif()
    endforeach()
endforeach()

if("${configurations_in_rows}" STREQUAL "")
    list(APPEND problems "no row names a configuration of tests/ctest_inventory.json")
endif()
foreach(configuration IN LISTS configurations)
    if(NOT "${configuration}" IN_LIST configurations_in_rows)
        list(APPEND not_checked
            "the ${total_${configuration}} entries of `${configuration}`, which has no row in the table")
    endif()
endforeach()

# The prose beside the table: "eight `Firefox*` entries", as a word or digits.
if("${brief}" MATCHES "([A-Za-z0-9]+)[ \n]+`Firefox\\*`[ \n]+entries")
    set(word "${CMAKE_MATCH_1}")
    string(TOLOWER "${word}" lower_word)
    set(number_words zero one two three four five six seven eight nine ten
        eleven twelve thirteen fourteen fifteen sixteen seventeen eighteen
        nineteen twenty)
    list(FIND number_words "${lower_word}" stated)
    if("${word}" MATCHES "^[0-9]+$")
        set(stated "${word}")
    endif()
    if("${stated}" EQUAL -1)
        list(APPEND problems
            "prose \"${word} `Firefox*` entries\": the count is not a number this check reads, and tests/ctest_inventory.json derives ${firefox_names}")
    elseif(NOT "${stated}" EQUAL "${firefox_names}")
        list(APPEND problems
            "prose \"${word} `Firefox*` entries\": states ${stated}, and tests/ctest_inventory.json derives ${firefox_names}")
    else()
        math(EXPR checked_count "${checked_count} + 1")
    endif()
else()
    list(APPEND not_checked "the prose count of `Firefox*` entries, which was not found")
endif()

if(problems)
    list(JOIN problems "\n    " report)
    message(FATAL_ERROR
        "AGENTS.md's ctest counts differ from tests/ctest_inventory.json:\n"
        "    ${report}\n"
        "  Correct the count in this file, or the inventory if an entry changed:\n"
        "    ${agents_brief}")
endif()

set(summary "check_agent_brief: ${checked_count} ctest counts in AGENTS.md match tests/ctest_inventory.json.")
if(not_checked AND DEFINED LIST_NOT_CHECKED AND NOT LIST_NOT_CHECKED)
    # The build passes -DLIST_NOT_CHECKED=OFF: one line on every build, with
    # the list a standalone run away.
    list(LENGTH not_checked not_checked_count)
    string(APPEND summary " ${not_checked_count} are not checked, and a standalone run lists them.")
elseif(not_checked)
    list(JOIN not_checked "\n     " report)
    string(APPEND summary "\n   Not checked:\n     ${report}")
endif()
message(STATUS "${summary}")
