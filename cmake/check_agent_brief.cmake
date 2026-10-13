# CLAUDE.md and AGENTS.md are one brief under two names, one for each agent
# that reads them, and they differ only in their title line. Two copies of a
# six-hundred-line file drift the first time an amendment is made to one and
# not the other, and nothing would notice: each agent would go on reading its
# own copy as the truth (T4). So the build compares them, below line 1, and
# names both files when they differ (T5, T6).
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

# The body of a brief: everything after its first line, with line endings
# normalised so that a checkout's autocrlf setting cannot make two equal
# files differ.
function(brief_body path out_var)
    if(NOT EXISTS "${path}")
        message(FATAL_ERROR "The agent brief ${path} is missing.\n"
            "  See CONTRIBUTING.md, The agent brief.")
    endif()
    file(READ "${path}" text)
    string(REPLACE "\r\n" "\n" text "${text}")
    string(FIND "${text}" "\n" first_newline)
    if(first_newline EQUAL -1)
        set(text "")
    else()
        math(EXPR body_start "${first_newline} + 1")
        string(SUBSTRING "${text}" ${body_start} -1 text)
    endif()
    set(${out_var} "${text}" PARENT_SCOPE)
endfunction()

brief_body("${claude_brief}" claude_body)
brief_body("${agents_brief}" agents_body)

if(NOT claude_body STREQUAL agents_body)
    message(FATAL_ERROR
        "The agent brief has drifted. These two files must be identical below "
        "their title line:\n"
        "    ${claude_brief}\n"
        "    ${agents_brief}\n"
        "  Make the amendment in one, copy it to the other, and keep each "
        "file's own first line. See CONTRIBUTING.md, The agent brief.")
endif()
