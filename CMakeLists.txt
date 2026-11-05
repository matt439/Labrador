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
# LabradorEngine: LabradorMath is under engine/ too and builds without it.
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

        # Protocol teardown has no browser or GPU dependency. In particular,
        # a page crash must preserve its failure instead of crashing the
        # runner on a command reply already in transit.
        add_test(NAME BrowserProtocolTests
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} --test
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/devtools.test.mjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/firefox_protocol.test.mjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/sample_key.test.mjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/sample_input_bridge.test.mjs"
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/bundle_host.test.mjs"
        )

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

        # A FIREFOX, FOR THE SAME PAGES OVER WEBDRIVER BIDI, by
        # tests/render/web/run_firefox.mjs: each sample's page, the window's
        # and LineSweeper's bundle page, registered at the end of this file.
        # Absent rather than skipped where configure finds none, as above.
        find_program(LABRADOR_TEST_FIREFOX
            NAMES firefox
            PATHS
                "$ENV{ProgramFiles}/Mozilla Firefox"
            NO_CMAKE_FIND_ROOT_PATH
            DOC "A Firefox to run a browser build's pages in over WebDriver BiDi"
        )
        if(NOT LABRADOR_TEST_FIREFOX)
            message(STATUS "No Firefox found, so the Firefox* page entries "
                "are not registered with ctest. Set LABRADOR_TEST_FIREFOX "
                "to run them.")
        endif()
    endif()

    add_subdirectory(tests)

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
        # Arguments after quit_keys are passed to the runner as they are.
        function(labrador_sample_page_test name target quit_keys)
            add_test(NAME ${name}
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "$<TARGET_FILE:${target}>"
                    --quit-keys "${quit_keys}"
                    --timeout 60
                    ${ARGN}
            )
        endfunction()
        # The pause menu: RESUME, RESTART, COLOURS, QUIT. The page reports
        # which particle field it made from prefers-reduced-motion, once for
        # each value emulated, since a CI runner's own setting may be either.
        labrador_sample_page_test(LineSweeperPageTests LineSweeperSample
            "Escape,ArrowDown,ArrowDown,ArrowDown,Enter"
            --sample-input fixed-step
            --emulate-media "prefers-reduced-motion=no-preference"
            --expect-console "LineSweeper: motion full")
        labrador_sample_page_test(LineSweeperReducedMotionPageTests
            LineSweeperSample "Escape,ArrowDown,ArrowDown,ArrowDown,Enter"
            --sample-input fixed-step
            --emulate-media "prefers-reduced-motion=reduce"
            --expect-console "LineSweeper: motion reduced")
        # On COLOURS, left steps back (wrapping from STANDARD to tritan),
        # right steps on, and Enter steps on (wrapping back to STANDARD) and
        # keeps the menu open: standard, tritan, deutan, tritan, standard,
        # protan. Had left or right moved the cursor instead, Enter would
        # land on another row. QUIT then closes the menu, and the match logs
        # the palette it was left in before it quits.
        labrador_sample_page_test(LineSweeperPalettePageTests
            LineSweeperSample
            "Escape,ArrowDown,ArrowDown,ArrowLeft,ArrowLeft,ArrowRight,Enter,Enter,ArrowDown,Enter"
            --sample-input fixed-step
            --expect-console "LineSweeper: palette Protanomaly / Protanopia")
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
        # Navigate settings, request an animation restart, then quit. A browser can
        # consume Escape to leave full screen, so send a second press for the gallery.
        # This checks page loading/drawing/exit, without reading the rows or frames.
        # ComponentsTests covers widget/playback behavior, not these bindings.
        labrador_sample_page_test(ComponentsPageTests ComponentsGallery
            "ArrowDown,ArrowDown,ArrowLeft,ArrowUp,ArrowRight,KeyR,Escape,Escape")
        # The same gallery by mouse and emulated touch: a slider dragged and
        # clamped, a release off the pressed row, a tap on a value, and Quit
        # tapped (samples/components/web/pointer_input.mjs).
        add_test(NAME ComponentsPointerPageTests
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                --browser "${LABRADOR_TEST_BROWSER}"
                --page "$<TARGET_FILE:ComponentsGallery>"
                --input "${CMAKE_CURRENT_SOURCE_DIR}/samples/components/web/pointer_input.mjs"
                --timeout 60
        )

        # The packaged host page mounts an ES-module factory on a click,
        # rather than exposing the development shell's Module. Its driver
        # exercises real instances, quit/replay, the touch gate and recovery
        # UI. Context-loss coverage needs the rasterizing browser build.
        if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
            add_test(NAME LineSweeperBundlePageTests
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "${CMAKE_BINARY_DIR}/web-dist/linesweeper/index.html"
                    --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/bundle_input.mjs"
                    --timeout 120
            )
            # The longest page test, so ctest starts it first rather than in
            # registration order; and never beside the pacing page, which
            # times frames on the same software rasteriser. Run together on a
            # loaded runner the two ran two to three times their usual time
            # and timed out (jobs 125764194088, 126339685291).
            set_tests_properties(LineSweeperBundlePageTests PROPERTIES
                COST 1000 RESOURCE_LOCK linesweeper_frame_timing)
            # The site's own page for the same bundle, from a checkout of the
            # host repository after its sync-demo: the page is the host's, so
            # it is registered only when configure is given that checkout's
            # public/ folder, which the run serves whole for the page's fonts.
            set(LABRADOR_HOST_SITE "" CACHE PATH
                "A host site checkout's public/ folder, holding demo/linesweeper/index.html; registers LineSweeperHostPageTests")
            if(LABRADOR_HOST_SITE)
                add_test(NAME LineSweeperHostPageTests
                    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                        --browser "${LABRADOR_TEST_BROWSER}"
                        --root "${LABRADOR_HOST_SITE}"
                        --page "${LABRADOR_HOST_SITE}/demo/linesweeper/index.html"
                        --input "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/host_input.mjs"
                        --timeout 120
                )
            endif()
            # Release-only instrument acceptance: completed CPU-frame rows
            # and exact fixture counts, without a physical pacing verdict.
            if(CMAKE_BUILD_TYPE STREQUAL "Release")
                add_test(NAME LineSweeperPacingPageTests
                    COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                        "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_page.mjs"
                        --browser "${LABRADOR_TEST_BROWSER}"
                        --page "$<TARGET_FILE:LineSweeperSample>"
                        --input "${CMAKE_CURRENT_SOURCE_DIR}/tools/web_pacing/page_input.mjs"
                        --timeout 60
                )
                set_tests_properties(LineSweeperPacingPageTests PROPERTIES
                    RESOURCE_LOCK linesweeper_frame_timing)
            endif()
        endif()
    endif()

    # THE SAME PAGES IN FIREFOX, over WebDriver BiDi rather than DevTools
    # (tests/render/web/run_firefox.mjs says what each run requires and
    # what it does not claim). Each sample's page is loaded, drawn and quit
    # by its own keys; the window's page runs window_input.mjs's sequence;
    # the bundle page is played, its context lost and restored, quit and
    # played again with bundle_page.mjs's assertions. Only with a rasterizing
    # backend, because a sample's run counts real WebGL draws. Named Firefox*
    # so the CI steps that run Chromium's pages can leave them out.
    if(EMSCRIPTEN AND LABRADOR_TEST_FIREFOX
        AND NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
        set(labrador_firefox_runner
            "${CMAKE_CURRENT_SOURCE_DIR}/tests/render/web/run_firefox.mjs")
        set(labrador_firefox_samples
            linesweeper minimal collision audio local_multiplayer components)
        set(labrador_firefox_names
            LineSweeper Minimal Collision Audio LocalMultiplayer Components)
        foreach(sample name IN ZIP_LISTS
                labrador_firefox_samples labrador_firefox_names)
            add_test(NAME Firefox${name}PageTests
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${labrador_firefox_runner}"
                    --browser "${LABRADOR_TEST_FIREFOX}"
                    --build-dir "${CMAKE_BINARY_DIR}"
                    --sample ${sample}
                    --timeout 60
            )
            set_tests_properties(Firefox${name}PageTests PROPERTIES TIMEOUT 90)
        endforeach()
        add_test(NAME FirefoxWebWindowTests
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${labrador_firefox_runner}"
                --browser "${LABRADOR_TEST_FIREFOX}"
                --build-dir "${CMAKE_BINARY_DIR}"
                --test WebWindowTests
                --timeout 60
        )
        set_tests_properties(FirefoxWebWindowTests PROPERTIES TIMEOUT 90)
        add_test(NAME FirefoxLineSweeperBundlePageTests
            COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR} "${labrador_firefox_runner}"
                --browser "${LABRADOR_TEST_FIREFOX}"
                --bundle "${CMAKE_BINARY_DIR}/web-dist/linesweeper"
                --timeout 120
        )
        # Never beside the other LineSweeper pages that time frames on the
        # same software rasteriser, for the reason Chromium's bundle entry gives.
        set_tests_properties(FirefoxLineSweeperBundlePageTests PROPERTIES
            TIMEOUT 180 COST 900 RESOURCE_LOCK linesweeper_frame_timing)
    endif()
endif()
