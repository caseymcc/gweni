#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  gweni tests, run with: ctest --test-dir build_linux_x64_debug
#
#  Tests labelled "gpu" open a GL context (the examples' surfaceless EGL path,
#  --headless), so skip them with ctest -LE gpu. They run from the repo root,
#  because the binaries load resources/ relative to the working directory.
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

    # build, render and destroy every demo; exits non-zero unless the canvas
    # renders as the empty canvas it started as
    add_test(NAME controltest_teardown
        COMMAND $<TARGET_FILE:ControlTest> --headless --check-teardown
            --shot-dir ${CMAKE_CURRENT_BINARY_DIR}/teardown
        WORKING_DIRECTORY ${GWENI_SOURCE_DIR})
    set_tests_properties(controltest_teardown PROPERTIES
        LABELS gpu
        TIMEOUT 300)

    # Not registered: --scenario resize, which fails on main (issue #58, the
    # 640->1024->640 resize does not restore the layout).
else()
    message(STATUS "tests: ControlTest tests skipped, GWENI_BUILD_EXAMPLES is off")
endif()

# C++ unit tests (doctest), one executable. They need a GL context, which they
# get from the examples' surfaceless EGL code (gweni_capture), so they are only
# built along with the examples.
if(TARGET gweni_capture)
    find_package(doctest CONFIG REQUIRED)

    add_executable(gweni_unittests
        main.cpp
        textLength.cpp)
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
