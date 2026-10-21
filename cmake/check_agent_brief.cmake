# AGENTS.md is the agent brief, and CLAUDE.md is not a second copy of it: its
# first line is "@AGENTS.md", which Claude Code reads as an import of the whole
# brief. Anything below that line is for Claude Code alone. A brief kept as two
# copies drifts the first time an amendment is made to one and not the other,
# and nothing would notice: each agent would go on reading its own copy as the
# truth (T4). So the build checks that CLAUDE.md still imports AGENTS.md
# rather than carrying a copy, and names the file when it does not (T5, T6).
#
# Top-level builds only, like the citation check: a consumer of the engine
# does not carry this repository's agent brief.
#
# Run with: cmake -DREPO_DIR=<path> -P cmake/check_agent_brief.cmake

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
