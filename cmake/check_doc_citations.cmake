# Check source citations against document names, numbered sections and
# PHILOSOPHY trade-off headings (docs/design/CONVENTIONS.md, Comments).
# Renaming a document or section must not leave an authoritative-looking
# pointer to nothing.
#
# This checks name closure for the recognized spellings, not whether a cited
# passage supports the source comment or describes current behavior. Bare
# filenames with multiple matches are reported without failing or checking
# their section. The amendment rule in engine/render/SEAM.md still needs review.
#
# Run with: cmake -DREPO_DIR=<path> -P cmake/check_doc_citations.cmake

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_doc_citations.cmake: REPO_DIR is not set")
endif()

file(GLOB_RECURSE sources
    "${REPO_DIR}/engine/*.h" "${REPO_DIR}/engine/*.cpp"
    "${REPO_DIR}/samples/*.h" "${REPO_DIR}/samples/*.cpp"
    "${REPO_DIR}/tests/*.h" "${REPO_DIR}/tests/*.cpp"
    "${REPO_DIR}/bench/*.h" "${REPO_DIR}/bench/*.cpp"
    "${REPO_DIR}/tools/*.h" "${REPO_DIR}/tools/*.cpp")

# Documents available for bare-filename lookup. Exclude build output, vendored
# files, the separate website tree and .claude/ worktrees, whose duplicate
# basenames are not repository citation targets.
file(GLOB_RECURSE all_docs "${REPO_DIR}/*.md")
set(docs "")
foreach(doc IN LISTS all_docs)
    # Matched below REPO_DIR, so that a checkout which is itself one of those
    # worktrees still sees its own documents.
    file(RELATIVE_PATH doc_in_repo "${REPO_DIR}" "${doc}")
    if(NOT doc_in_repo MATCHES "^(out|external|website|\\.claude)/")
        list(APPEND docs "${doc}")
    endif()
endforeach()

set(missing_docs "")
set(missing_sections "")
set(missing_tradeoffs "")
set(ambiguous_docs "")

foreach(source IN LISTS sources)
    file(READ "${source}" text)

    # Rejoin a hyphenated path wrapped across // comment lines, such as
    # "docs/port/content-" followed by "probe.md", before matching citations.
    string(REGEX REPLACE "-[ \t]*\r?\n[ \t]*//[ \t]*" "-" text "${text}")

    # ---------------------------------------------------------- documents
    # A trailing #N names a section of it.
    string(REGEX MATCHALL "[A-Za-z0-9_./-]*[A-Za-z0-9_-]\\.md(#[0-9]+)?"
        citations "${text}")
    list(REMOVE_DUPLICATES citations)

    foreach(citation IN LISTS citations)
        set(section "")
        if(citation MATCHES "^(.+\\.md)#([0-9]+)$")
            set(document "${CMAKE_MATCH_1}")
            set(section "${CMAKE_MATCH_2}")
        else()
            set(document "${citation}")
        endif()

        # Paths resolve from the repository root (CONVENTIONS, Files). Bare
        # filenames need a unique match before a numbered section can be checked.
        set(resolved "")
        set(candidates "")
        if(document MATCHES "/")
            if(EXISTS "${REPO_DIR}/${document}")
                set(resolved "${REPO_DIR}/${document}")
            endif()
        else()
            foreach(doc IN LISTS docs)
                if(doc MATCHES "/${document}$")
                    list(APPEND candidates "${doc}")
                endif()
            endforeach()
            list(LENGTH candidates candidate_count)
            if(candidate_count EQUAL 1)
                set(resolved "${candidates}")
            elseif(candidate_count GREATER 1)
                # Ambiguity is reported for a reader to resolve from context.
                # A missing document is fatal; ambiguity alone is not.
                list(JOIN candidates ", " candidate_list)
                list(APPEND ambiguous_docs
                    "${source}\n      ${citation} -> ${candidate_count}: ${candidate_list}")
            endif()
        endif()

        if(NOT resolved)
            if(NOT candidates)
                list(APPEND missing_docs "${source}\n      ${citation}")
            endif()
            continue()
        endif()

        # ------------------------------------------------------- sections
        # The recognized section form is SEAM.md#6, checked against a level-two
        # heading beginning "## 6. ". Textual anchors are not validated here.
        if(section)
            file(STRINGS "${resolved}" headings REGEX "^## ${section}\\. ")
            if(NOT headings)
                list(APPEND missing_sections
                    "${source}\n      ${citation} - no \"## ${section}.\" heading in ${document}")
            endif()
        endif()
    endforeach()

    # --------------------------------------------------------- trade-offs
    # Check standalone T-number tokens on lines containing // against the
    # PHILOSOPHY headings. Whole words avoid treating INT32 as a citation of
    # T32; the comment marker keeps an ordinary template parameter out.
    file(STRINGS "${source}" comment_lines REGEX "//")
    foreach(line IN LISTS comment_lines)
        string(REGEX REPLACE "[^A-Za-z0-9_]+" ";" words "${line}")
        foreach(word IN LISTS words)
            if(word MATCHES "^T([0-9]+)$")
                set(number "${CMAKE_MATCH_1}")
                file(STRINGS "${REPO_DIR}/docs/design/PHILOSOPHY.md" heading
                    REGEX "^### T${number}\\. ")
                if(NOT heading)
                    list(APPEND missing_tradeoffs "${source}\n      ${word}")
                endif()
            endif()
        endforeach()
    endforeach()
endforeach()

if(ambiguous_docs)
    list(REMOVE_DUPLICATES ambiguous_docs)
    list(JOIN ambiguous_docs "\n    " report)
    message(STATUS
        "check_doc_citations: bare filenames resolving to more than one document:\n"
        "    ${report}")
endif()

set(failures "")
if(missing_docs)
    list(REMOVE_DUPLICATES missing_docs)
    list(JOIN missing_docs "\n    " report)
    list(APPEND failures
        "These comments cite a document that does not exist:\n    ${report}")
endif()
if(missing_sections)
    list(REMOVE_DUPLICATES missing_sections)
    list(JOIN missing_sections "\n    " report)
    list(APPEND failures
        "These comments cite a section that does not exist:\n    ${report}")
endif()
if(missing_tradeoffs)
    list(REMOVE_DUPLICATES missing_tradeoffs)
    list(JOIN missing_tradeoffs "\n    " report)
    list(APPEND failures
        "These comments cite a trade-off PHILOSOPHY.md does not define:\n    ${report}")
endif()

if(failures)
    list(JOIN failures "\n  " report)
    message(FATAL_ERROR
        "A citation from code to a document has to resolve.\n"
        "  ${report}\n"
        "  Cite a document by its path from the repository root, a section as\n"
        "  <document>.md#<number>, and a trade-off by the number\n"
        "  docs/design/PHILOSOPHY.md gives it. See CONVENTIONS.md, Comments.")
endif()
