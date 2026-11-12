find_package(doctest CONFIG REQUIRED)

# One doctest executable, registered with ctest under its own name. Every
# suite shares test_main.cpp beside this file, so a suite's folder holds only
# its cases; LINKS names what the cases are tested against, which is not
# always the whole engine. Sources a configuration adds go on afterwards with
# target_sources, as they did before.
#
# TIMEOUT is ctest's limit in seconds, 60 unless given. A suite that creates a
# device can wait on it without bound (D3D12's fence wait is INFINITE, Vulkan's
# UINT64_MAX), and no preset sets a default, so without one a hang holds a CI
# job to its 90-minute limit without naming the entry. The longest suite in CI
# runs 43314200211 and 42556229672 took 1.08 s (AppTests, x64-debug-null);
# every other suite took under 0.8 s.
function(labrador_add_test_suite name)
    cmake_parse_arguments(PARSE_ARGV 1 suite "" "TIMEOUT" "SOURCES;LINKS")
    if(NOT suite_TIMEOUT)
        set(suite_TIMEOUT 60)
    endif()
    add_executable(${name}
        ${suite_SOURCES}
        "${CMAKE_CURRENT_FUNCTION_LIST_DIR}/test_main.cpp"
    )
    target_link_libraries(${name} PRIVATE
        ${suite_LINKS}
        doctest::doctest
        labrador_settings
    )
    add_test(NAME ${name} COMMAND ${name})
    set_tests_properties(${name} PROPERTIES TIMEOUT ${suite_TIMEOUT})
endfunction()

add_subdirectory(app)
add_subdirectory(assets)
add_subdirectory(audio)
add_subdirectory(collision)
add_subdirectory(core)
add_subdirectory(input)
add_subdirectory(linesweeper)
add_subdirectory(local_multiplayer)
add_subdirectory(math)
add_subdirectory(render)
add_subdirectory(scene)
add_subdirectory(ui)
