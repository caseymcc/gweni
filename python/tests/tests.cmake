#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The Python module's tests (included by python/CMakeLists.txt with
#  GWENI_BUILD_TESTS), every one labelled python:
#    python_pytest      the pytest suite (python/tests/test_*.py) less what
#                       makes a context (-m "not gpu"), against the staged
#                       package <tree>/python/gweni/
#    python_pytest_gpu  the suite's gpu-marked tests (a Headless or an App):
#                       label gpu, so under the render lock
#    python_exports     the module exports PyInit__gweni alone (exports.cmake)
#    python_stubs       python/gweni/_gweni.pyi is the stub nanobind's stubgen
#                       gives for the built module (python/stubs.cmake)
#    python_wheel       the wheel built, repaired to manylinux_2_35, installed
#                       and smoke-tested (wheel.sh;
#                       the release tree only)
#    python_wheel_build_dirs  tools/build_wheel.sh's modes never share a
#                       configured build directory (wheelBuildDirs.sh)
#    python_mypy        mypy --strict over python/examples/, with the staged
#                       package's stubs
#    python_parity      ButtonDemo from Python against ControlTest's (parity.py;
#                       label gpu)
#    python_window_idle App.run() in a window under Xvfb (window_idle.cmake;
#                       label gpu)
#

set(gweni_pytest_env
    "PYTHONPATH=${GWENI_PYTHON_STAGE}"
    "PYTHONDONTWRITEBYTECODE=1"
    "GWENI_PYTHON_STAGE=${GWENI_PYTHON_STAGE}")

foreach(variant "" _gpu)
    if(variant STREQUAL "_gpu")
        set(marks "gpu")
        set(labels "python;gpu")
    else()
        set(marks "not gpu")
        set(labels "python")
    endif()
    add_test(NAME python_pytest${variant}
        COMMAND ${Python_EXECUTABLE} -m pytest
            -p no:cacheprovider -q -m ${marks}
            --rootdir ${CMAKE_CURRENT_LIST_DIR}
            ${CMAKE_CURRENT_LIST_DIR}
        WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
    set_tests_properties(python_pytest${variant} PROPERTIES
        LABELS "${labels}"
        ENVIRONMENT "${gweni_pytest_env}"
        TIMEOUT 600)
endforeach()

add_test(NAME python_exports
    COMMAND ${CMAKE_COMMAND}
        -DMODULE=$<TARGET_FILE:_gweni>
        -P ${CMAKE_CURRENT_LIST_DIR}/exports.cmake)
set_tests_properties(python_exports PROPERTIES LABELS python)

# python_parity: ButtonDemo from Python (python/examples/button_demo.py)
# against ControlTest's, pixels and counters (parity.py's header)
if(TARGET ControlTest)
    add_test(NAME python_parity
        COMMAND ${Python_EXECUTABLE} ${CMAKE_CURRENT_LIST_DIR}/parity.py
            $<TARGET_FILE:ControlTest> ${CMAKE_CURRENT_BINARY_DIR}/parity)
    set_tests_properties(python_parity PROPERTIES
        LABELS "python;gpu"
        ENVIRONMENT "${gweni_pytest_env}"
        TIMEOUT 300)
endif()

# python_window_idle: App.run()'s idle loop, the post, Ctrl+C and the wakeup fd
# in a window under Xvfb (window_idle.cmake)
add_test(NAME python_window_idle
    COMMAND ${CMAKE_COMMAND}
        -DPYTHON=${Python_EXECUTABLE}
        -DSCRIPT=${CMAKE_CURRENT_LIST_DIR}/window_idle.py
        -P ${CMAKE_CURRENT_LIST_DIR}/window_idle.cmake)
set_tests_properties(python_window_idle PROPERTIES
    LABELS "python;gpu"
    ENVIRONMENT "${gweni_pytest_env}"
    TIMEOUT 180)

add_test(NAME python_stubs
    COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python_EXECUTABLE} -DSTAGE=${GWENI_PYTHON_STAGE}
        -DOUT=${CMAKE_CURRENT_BINARY_DIR}/_gweni.pyi
        -DCHECK=${GWENI_SOURCE_DIR}/python/gweni/_gweni.pyi
        -P ${GWENI_SOURCE_DIR}/python/stubs.cmake)
set_tests_properties(python_stubs PROPERTIES LABELS python)

add_test(NAME python_mypy
    COMMAND ${Python_EXECUTABLE} -m mypy --strict --no-incremental
        --cache-dir ${CMAKE_CURRENT_BINARY_DIR}/mypy-cache
        ${GWENI_SOURCE_DIR}/python/examples
    WORKING_DIRECTORY ${CMAKE_CURRENT_BINARY_DIR})
set_tests_properties(python_mypy PROPERTIES
    LABELS python
    ENVIRONMENT "${gweni_pytest_env}")

# python_wheel: the wheel built offline by tools/build_wheel.sh, repaired to
# manylinux_2_35 and checked by tools/repair_wheel.py, installed with pip and
# with uv into fresh venvs and smoke-tested from the installed package
# (wheel.sh's header). In the release tree only, as web_site_smoke is: it
# builds the whole library a second time. No gpu label: its one GL step takes
# the render lock itself.
if(CMAKE_BUILD_TYPE STREQUAL "Release")
    add_test(NAME python_wheel
        COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/wheel.sh ${CMAKE_BINARY_DIR})
    set_tests_properties(python_wheel PROPERTIES
        LABELS python
        ENVIRONMENT "GWENI_PYTHON=${Python_EXECUTABLE}"
        TIMEOUT 3600)
endif()

# python_wheel_build_dirs: tools/build_wheel.sh's plain and --vcpkg-installed
# modes each in a build directory of their own, cleared when its record says
# another mode (wheelBuildDirs.sh's header). It builds nothing, so every tree.
add_test(NAME python_wheel_build_dirs
    COMMAND bash ${CMAKE_CURRENT_LIST_DIR}/wheelBuildDirs.sh)
set_tests_properties(python_wheel_build_dirs PROPERTIES
    LABELS python
    ENVIRONMENT "GWENI_PYTHON=${Python_EXECUTABLE}"
    TIMEOUT 120)
