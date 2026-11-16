if(NOT LABRADOR_ASAN)
    return()
endif()

find_package(Python3 REQUIRED COMPONENTS Interpreter)

add_executable(AsanDetectionProbe detection_probe.cpp)
target_include_directories(AsanDetectionProbe PRIVATE "${PROJECT_SOURCE_DIR}")
target_link_libraries(AsanDetectionProbe PRIVATE labrador_settings)
labrador_asan_test_allocations(AsanDetectionProbe)
add_test(NAME AsanDetectionTests
    COMMAND "${Python3_EXECUTABLE}" "${PROJECT_SOURCE_DIR}/tools/asan_probe.py"
        "$<TARGET_FILE:AsanDetectionProbe>")
set_tests_properties(AsanDetectionTests PROPERTIES TIMEOUT 75)
