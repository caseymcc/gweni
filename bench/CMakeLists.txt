#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni_bench: fixed scenes run on the full path (gweni + OpenGLCore on
#  surfaceless EGL), printing per-frame phase times and work counters. Built
#  with the tests and the examples, for gweni_capture.
#

add_executable(gweni_bench
    main.cpp
    phases.cpp
    allocCounter.cpp
    fixedRandom.cpp
    scenes.cpp)
gweni_set_warnings(gweni_bench)

# Its getrandom() (fixedRandom.cpp) must interpose libc's for the libraries
# Mesa loads, so the exe exports that symbol, and only that one.
if(UNIX AND NOT APPLE)
    target_link_options(gweni_bench PRIVATE
        -Wl,--dynamic-list=${CMAKE_CURRENT_SOURCE_DIR}/exports.list)
endif()
target_link_libraries(gweni_bench
    gweni
    gweni_capture
    glfw
)

# The bench loads the resources/ next to it through
# RelativeToExecutablePaths("resources"), as the examples do, so it runs from
# any working directory.
get_property(bench_multi_config GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(bench_multi_config)
    set(bench_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/$<CONFIG>/resources)
else()
    set(bench_resource_dir ${CMAKE_CURRENT_BINARY_DIR}/resources)
endif()

add_custom_target(gweni_bench_resources
    COMMAND ${CMAKE_COMMAND} -E copy_directory_if_different
        ${GWENI_SOURCE_DIR}/resources ${bench_resource_dir}
    COMMENT "Copying resources/ next to gweni_bench"
    VERBATIM)
add_dependencies(gweni_bench gweni_bench_resources)

# Every scene in --list at a small N for a few frames: fails only if a scene
# does not run. Not a performance check.
add_test(NAME bench_smoke
    COMMAND ${CMAKE_COMMAND}
        -DBENCH=$<TARGET_FILE:gweni_bench>
        -P ${CMAKE_CURRENT_SOURCE_DIR}/smoke.cmake
    WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
set_tests_properties(bench_smoke PROPERTIES
    LABELS gpu
    TIMEOUT 300)

# ASAN tree: the same environment tests/CMakeLists.txt gives its tests, for the
# same reasons (Mesa/EGL leak at exit; a report must abort).
if(GWENI_SANITIZE MATCHES "address")
    set_tests_properties(bench_smoke PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
