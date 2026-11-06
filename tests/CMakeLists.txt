#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni tests, run with: ctest --test-dir build_linux_x64_debug
#
#  Tests labelled "gpu" open a GL context (the examples' surfaceless EGL path,
#  --headless), so skip them with ctest -LE gpu. The examples load the
#  resources/ the build copies next to them, so they run from any working
#  directory (controltest_any_cwd checks it); their tests still run from the
#  repo root, where relative --shot paths are meant. gweni_unittests loads
#  resources/ relative to the working directory, so it must run there.
#

if(TARGET ControlTest)
    # every --shot-all PNG and --dump-bounds listing, byte-identical across two
    # separate ControlTest processes
    add_test(NAME controltest_determinism
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_determinism PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # the same at --scale 1.5, where every edge is rounded to the device-pixel
    # grid, so a rounding that depends on anything but the input shows here
    add_test(NAME controltest_determinism_scale150
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/determinism_scale150
            -DSCALE=1.5
            -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_determinism_scale150 PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # --shot-all --dump-bounds at --scale 1 and 2: the listings (logical units)
    # are identical and every PNG (device pixels) is exactly twice the size
    add_test(NAME controltest_scale_bounds
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/scale_bounds
            -P ${CMAKE_CURRENT_SOURCE_DIR}/scaleBounds.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_scale_bounds PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # build, render and destroy every demo; exits non-zero unless the canvas
    # renders as the empty canvas it started as, and every frame of it takes
    # one layout pass
    add_test(NAME controltest_teardown
        COMMAND $<TARGET_FILE:ControlTest> --headless --check-teardown
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/teardown
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_teardown PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # the whole Demo at 640x480, 1024x768 and 640x480 again: the listing, the
    # renderer mirror (per id) and the pixels of the last shot match the first
    # and a canvas built directly at 640x480, and every frame takes one layout
    # pass (issue #58)
    add_test(NAME controltest_resize
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario resize
            --require-one-pass
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/resize
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_resize PROPERTIES
        LABELS gpu
        TIMEOUT 120)

    # the same round trip for every other control in --list, one process each
    add_test(NAME controltest_resize_each
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/resize_each
            -DMODE=resize
            -DSKIP=Demo
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_resize_each PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # the plain run of every control in --list (Demo included): every frame
    # takes one layout pass and the render phase lays out nothing
    add_test(NAME controltest_one_pass_each
        COMMAND ${CMAKE_COMMAND}
            -DCONTROLTEST=$<TARGET_FILE:ControlTest>
            -DSOURCE_DIR=${GWENI_SOURCE_DIR}
            -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/one_pass_each
            -DMODE=one-pass
            -P ${CMAKE_CURRENT_SOURCE_DIR}/eachControl.cmake
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_one_pass_each PROPERTIES
        LABELS gpu
        TIMEOUT 600)

    # hover, press, disabled and focus repaint the control (and only it), a
    # settled frame does nothing, and collapsing a tree node still moves the
    # nodes below; exits non-zero on any failed check
    add_test(NAME controltest_interaction
        COMMAND $<TARGET_FILE:ControlTest> --headless --scenario interaction
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/interaction
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_interaction PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # the resources/ next to the exe are found from a directory that has none
    add_test(NAME controltest_any_cwd
        COMMAND $<TARGET_FILE:ControlTest> --headless --control ButtonDemo
            --shot ${CMAKE_CURRENT_BINARY_DIR}/any_cwd/ButtonDemo.png
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(controltest_any_cwd PROPERTIES
        LABELS gpu
        TIMEOUT 60)

    # a --resources directory that does not exist is one line naming it, before
    # any GL context is made, so this needs no GPU
    add_test(NAME controltest_missing_resources
        COMMAND $<TARGET_FILE:ControlTest> --headless --control ButtonDemo
            --shot ${CMAKE_CURRENT_BINARY_DIR}/missing_resources/ButtonDemo.png
            --resources ${CMAKE_CURRENT_BINARY_DIR}/no_such_resources)
    set_tests_properties(controltest_missing_resources PROPERTIES
        PASS_REGULAR_EXPRESSION "^ControlTest: resource directory not found: [^\n]*/no_such_resources\n*$"
        TIMEOUT 30)
else()
    message(STATUS "tests: ControlTest tests skipped, GWENI_BUILD_EXAMPLES is off")
endif()

find_package(doctest CONFIG REQUIRED)
include(doctest)

# Headless C++ unit tests (doctest) under unit/: no GL context and no resources,
# so they carry no gpu label and ctest -LE gpu runs them without the GPU lock.
# Each test case is registered as its own CTest test, so a failure names it.
add_executable(gweni_unit_tests
    unit/main.cpp
    unit/skinSwap.cpp
    unit/platformSleep.cpp
    unit/layoutPasses.cpp
    unit/sliderNotches.cpp
    unit/colorControls.cpp
    unit/scrollControl.cpp
    unit/resourcePaths.cpp
    unit/requestedSize.cpp)
gweni_set_warnings(gweni_unit_tests)
target_link_libraries(gweni_unit_tests
    gweni
    doctest::doctest)

# discovered at build time, so the ASAN environment below does not reach them;
# they get it here instead
set(gweni_unit_tests_properties TIMEOUT 60)
if(GWENI_SANITIZE MATCHES "address")
    list(APPEND gweni_unit_tests_properties
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
doctest_discover_tests(gweni_unit_tests
    TEST_PREFIX "unit:"
    PROPERTIES ${gweni_unit_tests_properties})

# C++ unit tests (doctest) that need a GL context, one executable. They get it
# from the examples' surfaceless EGL code (gweni_capture), so they are only
# built along with the examples.
if(TARGET gweni_capture)
    add_executable(gweni_unittests
        main.cpp
        textLength.cpp
        textRunCache.cpp
        textWrap.cpp
        textAtlases.cpp
        textGrayscale.cpp
        fontLoad.cpp
        fontRelease.cpp
        color.cpp
        fillStyles.cpp
        clipBoxes.cpp
        scaledEdges.cpp
        textureReload.cpp)
    gweni_set_warnings(gweni_unittests)
    target_link_libraries(gweni_unittests
        gweni
        gweni_capture
        doctest::doctest)

    add_test(NAME unittests
        COMMAND $<TARGET_FILE:gweni_unittests>
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(unittests PROPERTIES
        LABELS gpu
        TIMEOUT 60)
else()
    message(STATUS "tests: unit tests skipped, they need gweni_capture (GWENI_BUILD_EXAMPLES)")
endif()

# ASAN tree (./generate.sh linux asan): overflows fail the test, leaks do not.
# Mesa/EGL leak at exit and none of that is ours, so leak checking is off; with
# abort_on_error a report aborts the process instead of exiting 1, which ctest
# and determinism.cmake (whose ControlTest children inherit this environment
# through execute_process) both count as a failure.
if(GWENI_SANITIZE MATCHES "address")
    get_property(gweni_tests DIRECTORY PROPERTY TESTS)
    set_tests_properties(${gweni_tests} PROPERTIES
        ENVIRONMENT "ASAN_OPTIONS=detect_leaks=0:abort_on_error=1")
endif()
