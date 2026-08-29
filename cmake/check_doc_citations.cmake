# A citation from code to a document has to resolve.
#
# CONVENTIONS' Comments section splits a comment three ways and sends one of
# them - the rationale that outgrew its declaration - to a document beside the
# code, cited rather than restated. engine/render/SEAM.md is the first. That
# trade buys a shorter header and costs a second place for the same claim to
# live, so the citation between them is load-bearing in a way a paragraph in
# one file never was: renumber a section, rename a document, and the pointer
# still reads as authoritative while naming nothing.
#
# WHAT THIS PROVES AND WHAT IT DOES NOT, stated here because the distinction is
# the whole reason to be careful with it. It proves NAME CLOSURE: every
# document a comment names exists, every section it names is there, every
# trade-off number it cites is one PHILOSOPHY defines. It proves NOTHING about
# whether the prose is true. ApogeeVGC ran this experiment at scale - 105 docs
# and 2,479 keys relocated out of headers behind a linter that checks exactly
# this, green today - and its 2026-08-02 review then measured the prose: of 212
# findings, 79 false and 57 stale against 9 where the document was right and
# the code was wrong. That linter's own docstring calls itself "a total
# NAME-closure proof and a total TRUTH blind spot", and this file is the same
# instrument. A green build says the pointers resolve. Only the amendment rule
# at the top of SEAM.md says the prose is true, and only a reader keeps it.
#
# So this is cheap insurance on the mechanical half, not a reason to trust the
# document half more than a reader would.
#
# Run with: cmake -DREPO_DIR=<path> -P cmake/check_doc_citations.cmake

if(NOT DEFINED REPO_DIR)
    message(FATAL_ERROR "check_doc_citations.cmake: REPO_DIR is not set")
endif()

file(GLOB_RECURSE sources
    "${REPO_DIR}/engine/*.h" "${REPO_DIR}/engine/*.cpp"
    "${REPO_DIR}/samples/*.h" "${REPO_DIR}/samples/*.cpp"
    "${REPO_DIR}/tests/*.h" "${REPO_DIR}/tests/*.cpp"
    "${REPO_DIR}/bench/*.h" "${REPO_DIR}/bench/*.cpp")

# Every document in the tree, for resolving a citation that gives a bare
# filename. out/ is build output and external/ is vendored; neither is ours to
# cite and both would make a basename ambiguous for no reason.
file(GLOB_RECURSE all_docs "${REPO_DIR}/*.md")
set(docs "")
foreach(doc IN LISTS all_docs)
    if(NOT doc MATCHES "/(out|external)/")
        list(APPEND docs "${doc}")
    endif()
endforeach()

set(missing_docs "")
set(missing_sections "")
set(missing_tradeoffs "")
set(ambiguous_docs "")

foreach(source IN LISTS sources)
    file(READ "${source}" text)

    # A citation is prose and prose wraps. A path broken across two comment
    # lines ends the first in a hyphen - "docs/port/content-" then "probe.md" -
    # and is one token to a reader and two to a regex. Rejoin those before
    # matching anything, or the check reports a dangling document that is
    # merely hyphenated. There is exactly one such citation in the tree and it
    # is the reason this line exists.
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

        # A citation that spells a path is resolved as a path, from the
        # repository root exactly as an #include is (CONVENTIONS, Files). One
        # that gives a bare filename is resolved by name, and then what matters
        # is that the name is unique - the same rule a bare filename gets
        # anywhere else, because a reader who is given one has to find it too.
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
                # Reported, not failed. A citation that resolves to three
                # GAPS.md is READABLE - a reader lands on one of them and the
                # neighbouring sentence says which - where a citation that
                # resolves to none is not. Only the second is a defect, and
                # keeping them apart is what lets the first stay a hard check.
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
        # SEAM.md#6 names "## 6. Terms renderer.h defers here". The number is
        # what a renumbering moves, which is exactly why the citation carries
        # it and exactly why it is checked.
        if(section)
            file(STRINGS "${resolved}" headings REGEX "^## ${section}\\. ")
            if(NOT headings)
                list(APPEND missing_sections
                    "${source}\n      ${citation} - no \"## ${section}.\" heading in ${document}")
            endif()
        endif()
    endforeach()

    # --------------------------------------------------------- trade-offs
    # "T3: take the simpler model" is a complete comment (PHILOSOPHY.md), which
    # is only true while T3 is the one PHILOSOPHY defines. 158 sites cite one.
    #
    # Whole words, tested one at a time, because a substring match reads INT32
    # as a citation of T32. The line has to carry a comment marker: a citation
    # lives in prose, and this is what keeps a template parameter out.
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
