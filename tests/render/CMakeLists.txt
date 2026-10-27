find_package(doctest CONFIG REQUIRED)

add_executable(RenderTests
    block_decode_tests.cpp
    camera_tests.cpp
    colour_tests.cpp
    dds_file_tests.cpp
    draw_object_tests.cpp
    font_tests.cpp
    label_tests.cpp
    render_resources_tests.cpp
    renderer_seam_tests.cpp
    resolution_manager_tests.cpp
    sprite_font_file_tests.cpp
    sprite_geometry_tests.cpp
    sprite_sheet_object_tests.cpp
    text_encoding_tests.cpp
    text_tests.cpp
    viewport_manager_tests.cpp
    viewport_tests.cpp
    visual_tests.cpp
    test_main.cpp
)
target_link_libraries(RenderTests PRIVATE
    LabradorEngine
    doctest::doctest
    labrador_settings
)

# The test content, beside the executables. ctest runs from the build
# directory and the harnesses load "./content/" relative to it - a four-colour
# texture and a font, both owned by these tests rather than borrowed from a
# sample, so that restyling a sample cannot move a pin in here. RenderTests
# wants it because a font is not a device resource, so reading one and
# measuring with it is arithmetic this executable can do:
# sprite_font_file_tests.cpp reads the real file rather than a fixture, because
# a fixture could only agree with the parser that wrote it. RenderPixelTests
# wants it to draw with.
#
# ONE TARGET FOR BOTH EXECUTABLES, AND THAT IS NOT TIDINESS. Both link into
# this directory, so a POST_BUILD copy on each is two processes writing the
# same two files, and Ninja links them concurrently - the loser fails with
# "Error copying directory ...: Permission denied" and takes the build down
# before a single test has run. It is a race, so it fires at whatever rate two
# links happen to collide: twice in ninety-five CI runs, once under
# x64-debug-gl and once under x64-debug-vulkan, which reads as a backend fault
# and is nothing of the kind. A single node in the build graph cannot race
# itself. The samples copy their content this way for a different reason and
# the shape is the same one.
add_custom_target(render_test_content ALL)
add_custom_command(TARGET render_test_content POST_BUILD
    COMMAND "${CMAKE_COMMAND}" -E copy_directory_if_different
        "${CMAKE_CURRENT_SOURCE_DIR}/content"
        "$<TARGET_FILE_DIR:RenderTests>/content"
    VERBATIM
)
add_dependencies(RenderTests render_test_content)

# The null backend's own tests, in the one configuration that has it.
#
# A backend is chosen at build time, so a test that names one only compiles
# when that one is built - which is the same rule a client lives under and is
# why this is a source list and not an #ifdef. RenderTests is otherwise
# identical in every configuration.
if(LABRADOR_RENDER_BACKEND STREQUAL "null")
    target_sources(RenderTests PRIVATE null_tests.cpp)
endif()

# The two Direct3D backends' HRESULT translation, which is Windows code by
# construction and so is tested only where it compiles.
if(WIN32)
    target_sources(RenderTests PRIVATE throw_if_failed_tests.cpp)
endif()

add_test(NAME RenderTests COMMAND RenderTests)

# RenderPixelTests — the drawing half, on a real device.
#
# A separate executable from RenderTests, which is pure arithmetic and wants no
# device. This one creates a device and a hidden window, so it is the one entry
# in ctest that can fail for a reason outside the repository: a Direct3D debug
# build falls back to WARP and needs no GPU, but a release build has no such
# fallback (device_resources.cpp), and neither the OpenGL backend nor the
# Vulkan one has any fallback at all - Vulkan's software implementations are
# installed rather than shipped - so all three need an adapter. THE RELEASE
# HALF OF THAT IS ALSO A MEASUREMENT: CI runs this executable under x64-release
# on a machine with no GPU and it passes, which is how a build machine is known
# to offer an adapter rather than to fall back to one
# (.github/workflows/ci.yml). Keeping it
# apart means that failure names itself
# instead of taking the arithmetic down with it.
#
# NOT BUILT AT ALL AGAINST THE NULL BACKEND, rather than built and excluded. It
# records what it was asked to draw and never draws it, so read_back_buffer
# throws saying so - and a test that cannot pass is better absent than skipped,
# because a skipped test is a line in a log nobody reads and a missing one is a
# question somebody asks.
#
# AND IN A BROWSER BUILD IT IS A PAGE, run in headless Chrome rather than under
# Node, because Node has no WebGL. The cases, the harness and the golden set
# are the same files; what differs is the window (test_window.h), where the
# set is read from, and who runs it - see the browser half below.
if(NOT LABRADOR_RENDER_BACKEND STREQUAL "null")
    # The golden set's codec, on every platform (golden_image.cpp says why it
    # is one codec rather than each platform's own). A test dependency only.
    find_package(lodepng CONFIG REQUIRED)

    add_executable(RenderPixelTests
        golden_image.cpp
        pixel_tests.cpp
        test_main.cpp
    )
    target_link_libraries(RenderPixelTests PRIVATE
        LabradorEngine
        doctest::doctest
        labrador_settings
        lodepng
    )

    # The window the device is made on, one file per platform as the engine's
    # own platform folders are.
    if(EMSCRIPTEN)
        target_sources(RenderPixelTests PRIVATE web/test_window.cpp)
    else()
        target_sources(RenderPixelTests PRIVATE
            win32/test_window.cpp
            presentation_lifecycle_tests.cpp
        )
    endif()

    if(LABRADOR_RENDER_BACKEND STREQUAL "gl")
        target_sources(RenderPixelTests PRIVATE gl/texture_tests.cpp)
    elseif(LABRADOR_RENDER_BACKEND STREQUAL "d3d12")
        target_sources(RenderPixelTests PRIVATE
            d3d12/submit_allocation_tests.cpp
            d3d12/texture_upload_tests.cpp
            "$<$<CONFIG:Debug>:${CMAKE_CURRENT_SOURCE_DIR}/d3d12/presentation_diagnostics_tests.cpp>"
        )

        # One isolated executable supplies the actual DeviceResources object
        # with only its Win32 wait call redirected to bounded observation.
        # Normal static linking leaves the original archive member unextracted;
        # do not use whole-archive or duplicate-definition suppression here.
        add_executable(D3D12SynchronizationTests
            d3d12/pending_submission_tests.cpp
            d3d12/instrumented_device_resources.cpp
            d3d12/wait_observer.h
            win32/test_window.cpp
            test_main.cpp
        )
        target_link_libraries(D3D12SynchronizationTests PRIVATE
            LabradorEngine
            doctest::doctest
            labrador_settings
        )
        add_test(NAME D3D12SynchronizationTests COMMAND D3D12SynchronizationTests)
        set_tests_properties(D3D12SynchronizationTests PROPERTIES TIMEOUT 60)

        # Opt-in runtime capability probe; InfoQueue1 is newer than the normal
        # Debug diagnostic interface. It neither builds by default nor joins CTest.
        add_executable(D3D12DestructionProbe EXCLUDE_FROM_ALL
            d3d12/destruction_probe.cpp
            win32/test_window.cpp
        )
        target_link_libraries(D3D12DestructionProbe PRIVATE
            LabradorEngine
            labrador_settings
        )

        # Hidden Present may block before two slots can remain queued. Keep
        # this bounded capability probe opt-in, outside the required tests.
        add_executable(D3D12PresentedSlotsProbe EXCLUDE_FROM_ALL
            d3d12/presented_slots_probe.cpp
            d3d12/instrumented_device_resources.cpp
            win32/test_window.cpp
        )
        target_link_libraries(D3D12PresentedSlotsProbe PRIVATE
            LabradorEngine
            labrador_settings
        )
    elseif(LABRADOR_RENDER_BACKEND STREQUAL "vulkan")
        find_package(Vulkan REQUIRED)
        target_sources(RenderPixelTests PRIVATE
            vulkan/texture_tests.cpp
            vulkan/submit_failure_tests.cpp
            vulkan/submit_allocation_tests.cpp
            vulkan/initialization_tests.cpp
            vulkan/pending_submission_tests.cpp
        )
        target_link_libraries(RenderPixelTests PRIVATE Vulkan::Vulkan)

        # Opt-in validation snapshot through actual device destruction. Its
        # executable owns only an extra instance messenger, not the device.
        add_executable(VulkanDestructionProbe EXCLUDE_FROM_ALL
            vulkan/destruction_probe.cpp
            win32/test_window.cpp
        )
        target_link_libraries(VulkanDestructionProbe PRIVATE
            LabradorEngine
            labrador_settings
            Vulkan::Vulkan
        )

        # Ordinary presentation plus an exact timeline oracle. Already-retired
        # work is unverified, so this observation is outside default CTest.
        add_executable(VulkanPresentedSlotsProbe EXCLUDE_FROM_ALL
            vulkan/presented_slots_probe.cpp
            win32/test_window.cpp
        )
        target_link_libraries(VulkanPresentedSlotsProbe PRIVATE
            LabradorEngine
            labrador_settings
            Vulkan::Vulkan
        )
    endif()

    if(NOT EMSCRIPTEN)
        # The golden set, addressed where it lives rather than copied beside
        # the executable like content/ is, and golden_image.cpp says why: a
        # regeneration has to write into the source tree, because that is the
        # only place the new images can be reviewed before they are committed.
        target_compile_definitions(RenderPixelTests PRIVATE
            LABRADOR_GOLDEN_DIR="${CMAKE_CURRENT_SOURCE_DIR}/golden"
        )

        # The same content, copied by the same single target rather than by a
        # second POST_BUILD of its own - see render_test_content above for
        # what the second copy cost.
        add_dependencies(RenderPixelTests render_test_content)

        add_test(NAME RenderPixelTests COMMAND RenderPixelTests)
    else()
        # A PAGE, ITS LOADER AND THE MODULE: RenderPixelTests.html, .js and
        # .wasm, from web/shell.html. The content and the golden set are
        # embedded rather than read from the disk, because a page has no file
        # system but the one it carries - content at /content, where the
        # harness's "./content/" resolves, and the set at /golden, which is the
        # source folder's contents and not a copy of a build directory's.
        #
        # THE DIRECTORY'S NODE OPTIONS ARE OVERRIDDEN, not inherited: the root
        # CMakeLists links every test with NODERAWFS for Node, and NODERAWFS
        # refuses an embedded file. A later -s setting wins in emcc, and these
        # come after the directory's.
        #
        # EXIT_RUNTIME so that main's return value is the page's exit code, and
        # FS so that the runner can copy rejected frames out of the page's
        # memory when it ends.
        set_target_properties(RenderPixelTests PROPERTIES SUFFIX ".html")
        target_compile_definitions(RenderPixelTests PRIVATE
            LABRADOR_GOLDEN_DIR="/golden"
        )
        target_link_options(RenderPixelTests PRIVATE
            -sENVIRONMENT=web
            -sNODERAWFS=0
            -sEXIT_RUNTIME=1
            -sEXPORTED_RUNTIME_METHODS=FS
            "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/content"
            "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/golden@/golden"
            "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        )

        # Nothing embedded is a source, so a change to it relinks only because
        # it is named here. The set is globbed, which no source list in this
        # tree is: an image is added by a test case rather than by a person
        # editing a list, and a stale page is caught at the next configure.
        file(GLOB render_golden_images CONFIGURE_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/golden/*.png")
        set_property(TARGET RenderPixelTests APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/content/quad.dds"
            "${CMAKE_CURRENT_SOURCE_DIR}/content/jetbrains_mono_bold_16.spritefont"
            ${render_golden_images}
        )

        # WHO RUNS IT: web/run_page.mjs, under the Node the other entries run
        # under, serving the page and driving a headless Chromium over the
        # DevTools protocol. ctest prepends no emulator to a command that is
        # not a target, so the emulator is named here.
        #
        # ABSENT RATHER THAN SKIPPED WHEN THERE IS NO BROWSER, for the reason
        # the null backend's absence is given above. The root CMakeLists looks
        # for one, for every page in the build, and says so when it finds
        # none; this page is then built and not registered.
        if(LABRADOR_TEST_BROWSER)
            add_test(NAME RenderPixelTests
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "$<TARGET_FILE:RenderPixelTests>"
                    --collect /golden-actual
            )

            # AND ONCE MORE AS A DEVICE WITHOUT S3TC, which is most phones'
            # browsers: the same page and the same golden set, with
            # WEBGL_compressed_texture_s3tc hidden from every context it
            # makes, so gl/ takes the path that decodes block-compressed
            # textures on the CPU (engine/render/block_decode.h) and uploads
            # pixels. Every frame is held to the images the native upload
            # made; the BC2 case is where the two paths could differ, and its
            # block decodes exactly on both. Its own entry rather than a second
            # run inside the first, so a failure names which path it was.
            add_test(NAME RenderPixelTestsWithoutS3tc
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "$<TARGET_FILE:RenderPixelTests>"
                    --collect /golden-actual
                    --hide-extension WEBGL_compressed_texture_s3tc
            )
        endif()

        # RenderContextLossTests - what a lost WebGL2 context does to the
        # renderer and its shell, which only a browser can take away. Its own
        # page, because it needs frames to pass between the loss and the
        # restore, which a synchronous doctest case never lets happen
        # (web/context_loss_tests.cpp says what it pins). The same shell, the
        # same window and the same runner as RenderPixelTests, and the same
        # rule when there is no browser to run it in. A WebGL error the browser
        # logs fails it, because a stale name deleted on a restored context is
        # an error the engine drains before it could ever be asserted.
        add_executable(RenderContextLossTests
            web/context_loss_tests.cpp
            web/test_window.cpp
        )
        target_link_libraries(RenderContextLossTests PRIVATE
            LabradorEngine
            labrador_settings
        )
        set_target_properties(RenderContextLossTests PROPERTIES SUFFIX ".html")
        target_link_options(RenderContextLossTests PRIVATE
            -sENVIRONMENT=web
            -sNODERAWFS=0
            -sEXIT_RUNTIME=1
            "SHELL:--embed-file ${CMAKE_CURRENT_SOURCE_DIR}/content@/content"
            "SHELL:--shell-file ${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
        )
        set_property(TARGET RenderContextLossTests APPEND PROPERTY LINK_DEPENDS
            "${CMAKE_CURRENT_SOURCE_DIR}/web/shell.html"
            "${CMAKE_CURRENT_SOURCE_DIR}/content/quad.dds"
        )
        if(LABRADOR_TEST_BROWSER)
            add_test(NAME RenderContextLossTests
                COMMAND ${CMAKE_CROSSCOMPILING_EMULATOR}
                    "${CMAKE_CURRENT_SOURCE_DIR}/web/run_page.mjs"
                    --browser "${LABRADOR_TEST_BROWSER}"
                    --page "$<TARGET_FILE:RenderContextLossTests>"
                    --timeout 60
                    --browser-warnings fail
            )
        endif()
    endif()
endif()
