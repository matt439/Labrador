find_package(doctest CONFIG REQUIRED)

# One doctest executable, registered with ctest under its own name. Every
# suite shares test_main.cpp beside this file, so a suite's folder holds only
# its cases; LINKS names what the cases are tested against, which is not
# always the whole engine. Sources a configuration adds go on afterwards with
# target_sources, as they did before.
function(labrador_add_test_suite name)
    cmake_parse_arguments(PARSE_ARGV 1 suite "" "" "SOURCES;LINKS")
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
