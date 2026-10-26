cmake_minimum_required(VERSION 3.26)

# Labrador — a 2D game engine, its samples, its tests and its benchmarks.
# docs/design/ARCHITECTURE.md describes the shape; this file is the whole of
# the top-level build.
project(Labrador LANGUAGES CXX)

include(cmake/source_revision.cmake)
labrador_set_source_revision("${CMAKE_CURRENT_SOURCE_DIR}")

include(cmake/settings.cmake)
include(cmake/compile_shaders.cmake)

# The module graph, enforced. Runs on every build.
#
# Standalone, the compiler now enforces this too: there is no game/ beside
# engine/, so an engine file including a game header cannot resolve it. The
# grep stays because that is only true standalone — a client building this as
# a subdirectory does so from a tree that may well have a game/ in it, and
# that client's include roots are not the engine's business. Belt, braces, and
# it costs one build step.
add_custom_target(check_engine_includes ALL
    COMMAND "${CMAKE_COMMAND}"
        "-DENGINE_DIR=${CMAKE_CURRENT_SOURCE_DIR}/engine"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_engine_includes.cmake"
    COMMENT "Checking that no engine file includes a game header"
    VERBATIM
)

# AND EVERY REAL TARGET DEPENDS ON IT, which ALL alone did not give. ALL puts
# a target in the default build and nowhere else: `ninja LabradorEngine`, an
# IDE building the one target it has open, a client building its game target
# - none of those is the default build, and every one of them skipped the
# check while this file said "runs on every build"
# (docs/review/gpt6/README.md, G6-10). labrador_settings is the interface
# target every real target links (cmake/settings.cmake), and a dependency
# on an interface library is followed to whatever links it, so this one line
# is the dependency for the engine, the samples, the tests, the benchmark
# and a consumer's game alike. It is deliberately here rather than on
# LabradorEngine: MattMath is under engine/ too and builds without it.
add_dependencies(labrador_settings check_engine_includes)

add_subdirectory(engine)

# A client consuming this as a submodule wants the library and nothing else:
# its ctest should not run the engine's tests, and it should certainly not
# build the engine's samples. Without this guard, adding Labrador as a
# subdirectory would otherwise drag in the sample and test targets and put
# the repository's tests in the client's ctest.
if(PROJECT_IS_TOP_LEVEL)
    enable_testing()

    # Citations from code to a document, enforced. Runs on every build.
    #
    # Top-level only, unlike the wall above it. That one guards a hazard a
    # consumer really has - their tree may well have a game/ beside this one -
    # where this guards the engine's own prose against the engine's own edits.
    # A client that renames nothing and cites nothing should not be handed a
    # build failure for a section number this repository moved, and the files
    # it reads are tests/, samples/, bench/ and tools/, none of which a
    # consumer builds.
    add_custom_target(check_doc_citations ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DREPO_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_doc_citations.cmake"
        COMMENT "Checking that every document a comment cites resolves"
        VERBATIM
    )

    # The same dependency the include check has, inside the same guard, so
    # a targeted build here runs it and a consumer's never sees it.
    add_dependencies(labrador_settings check_doc_citations)

    # The agent brief: AGENTS.md, which CLAUDE.md imports rather than copies.
    # Top-level only for the same reason: it guards this repository's own
    # records, not a consumer's build.
    add_custom_target(check_agent_brief ALL
        COMMAND "${CMAKE_COMMAND}"
            "-DREPO_DIR=${CMAKE_CURRENT_SOURCE_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/cmake/check_agent_brief.cmake"
        COMMENT "Checking that CLAUDE.md imports AGENTS.md"
        VERBATIM
    )
    add_dependencies(labrador_settings check_agent_brief)

    # Every sample, each with a browser entry point beside its wWinMain
    # (docs/port/web.md, section 9 says where each stands). These come before
    # the Node link options below, because each builds a page for a browser
    # rather than a test for Node.
    add_subdirectory(samples/minimal)
    add_subdirectory(samples/collision)
    add_subdirectory(samples/linesweeper)
    add_subdirectory(samples/local_multiplayer)
    add_subdirectory(samples/audio)
    add_subdirectory(samples/components)
    add_subdirectory(bench)

    # Windows only until it has a reason not to be: a capture tool that reads
    # back a device.
    if(WIN32)
        add_subdirectory(tools/linesweeper_capture)
    endif()

    # A browser build's tests run under Node, which ctest reaches through the
    # emulator Emscripten's toolchain sets. NODERAWFS hands them the real file
    # system, so a harness that loads "./content/" beside itself reads the
    # same files it reads on Windows, from the same build directory.
    if(EMSCRIPTEN)
        add_link_options(-sNODERAWFS=1 -sENVIRONMENT=node)

        # AND THE ONES NODE CANNOT RUN ARE PAGES, run in a headless Chromium
        # by tests/render/web/run_page.mjs: the render tests' two, which need
        # WebGL2 (the pixel page registered twice, once with S3TC hidden), and
        # the window's, which needs a DOM to deliver input to - and each
        # sample's development page, registered at the end of this file. One
        # lookup for all of them, here, because the window's page is in
        # tests/app, the samples' are in samples/, and the render pages are
        # not built at all against the null backend.
        #
        # ABSENT RATHER THAN SKIPPED WHEN THERE IS NO BROWSER, for the reason
        # tests/render/CMakeLists.txt gives for RenderPixelTests' absence
        # under the null backend. A machine without Chrome builds the pages
        # and registers no entry, and says so here.
        # LABRADOR_TEST_BROWSER names any Chromium that speaks the protocol,
        # Edge included.
        find_program(LABRADOR_TEST_BROWSER
            NAMES chrome google-chrome google-chrome-stable chromium
                chromium-browser
            PATHS
                "$ENV{ProgramFiles}/Google/Chrome/Application"
                "$ENV{LOCALAPPDATA}/Google/Chrome/Application"
            NO_CMAKE_FIND_ROOT_PATH
            DOC "A Chromium browser to run a browser build's pages in"
        )
        if(NOT LABRADOR_TEST_BROWSER)
            message(STATUS "No Chromium found, so RenderPixelTests, "
                "RenderPixelTestsWithoutS3tc, RenderContextLossTests, "
                "WebWindowTests and the samples' pages are built and not "
                "registered with ctest. Set LABRADOR_TEST_BROWSER to run them.")
        endif()
    endif()

    add_subdirectory(tests/app)
    add_subdirectory(tests/assets)
    add_subdirectory(tests/audio)
    add_subdirectory(tests/collision)
    add_subdirectory(tests/core)
    add_subdirectory(tests/input)
    add_subdirectory(tests/linesweeper)
    add_subdirectory(tests/local_multiplayer)
    add_subdirectory(tests/math)
    add_subdirectory(tests/render)
    add_subdirectory(tests/scene)
    add_subdirectory(tests/ui)

    # EACH SAMPLE'S PAGE, LOADED AND QUIT, because the page is what a browser
    # build ships and nothing else here loads one: a content path the module
    # cannot find, a page that hands it no canvas or a callback it never
    # gives would otherwise first show on a live site. Each entry serves the
    # built development page, waits for its frames, presses the keys that
    # are the sample's own way out, and passes when Module.onQuit is called;
    # a console error, a browser warning or Module.onError fails it
    # (run_page.mjs says exactly what). Registered here rather than in each
    # sample's folder because minimal's is the template a new project
    # copies, and the runner is this repository's, not the project's.
    if(EMSCRIPTEN AND LABRADOR_TEST_BROWSER)
        function(labrador_sample_page_test name target quit_keys)
            add_test(NAME ${name}
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "$<TARGET_FILE:${target}>"
                    --quit-keys "${quit_keys}"
                    --timeout 60
            )
        endfunction()
        # The pause menu: RESUME, RESTART, QUIT.
        labrador_sample_page_test(LineSweeperPageTests LineSweeperSample
            "Escape,ArrowDown,ArrowDown,Enter")
        labrador_sample_page_test(LocalMultiplayerPageTests
            LocalMultiplayerSample "Escape")
        # Escape asks, and Enter answers yes.
        labrador_sample_page_test(MinimalPageTests MinimalSample
            "Escape,Enter")
        labrador_sample_page_test(CollisionPageTests CollisionSample "Escape")
        # S stops a loop that is not playing, so it changes nothing on the
        # board, and it is a key that grants activation: the gesture that
        # lets Web Audio make its context. Escape grants none (the gesture note
        # in engine/audio/webaudio/audio_device.cpp) and is only the way out.
        labrador_sample_page_test(AudioPageTests AudioSample "KeyS,Escape")
        # A slider moved, an option row moved, and out. Escape quits from any
        # state and nothing here reads the status line, so this proves the
        # page loads, draws and quits; ComponentsTests holds what a key does.
        labrador_sample_page_test(ComponentsPageTests ComponentsGallery
            "ArrowDown,ArrowDown,ArrowLeft,ArrowUp,ArrowRight,Escape")
    endif()
endif()
