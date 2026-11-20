#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The Python module gweni (GWENI_BUILD_PYTHON): python/src/*.cpp, nanobind,
#  built as one stable-ABI extension, _gweni.abi3.so (cp312-abi3: CPython 3.12
#  and newer), over gweni_app; python/gweni/, the package around it.
#
#  The interpreter is Python_EXECUTABLE (./generate.sh passes the image's
#  CPython 3.12, GWENI_PYTHON), and nanobind is the one installed for it (the
#  image's pinned PyPI package, found through `python -m nanobind
#  --cmake_dir`), not vcpkg's port, which builds its own CPython.
#
#  The package is laid out by install(... COMPONENT python EXCLUDE_FROM_ALL)
#  rules: the module, the .py files, the stubs, py.typed and resources/
#  (mirrored, cmake/mirrorDirectory.cmake). The wheel installs that component
#  (pyproject.toml), a plain cmake --install of the tree leaves it out, and
#  every build stages it in <tree>/python/gweni/ (gweni_python_stage), which is
#  what the tests import.
#
#  Without the interpreter (an image from before it, a tree configured without
#  ./generate.sh) or nanobind the configure goes on with no module, and the
#  Python tests are registered to fail, saying what is missing.
#

set(GWENI_PYTHON_STAGE ${CMAKE_CURRENT_BINARY_DIR})
set(gweni_python_missing "")

find_package(Python 3.12 COMPONENTS Interpreter Development.Module Development.SABIModule)
if(NOT Python_FOUND)
    set(gweni_python_missing "no CPython 3.12+ with its headers (Python_EXECUTABLE=${Python_EXECUTABLE})")
else()
    execute_process(
        COMMAND ${Python_EXECUTABLE} -m nanobind --cmake_dir
        OUTPUT_VARIABLE gweni_nanobind_dir
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE gweni_nanobind_result
        ERROR_QUIET)
    if(NOT gweni_nanobind_result EQUAL 0)
        set(gweni_python_missing "no nanobind for ${Python_EXECUTABLE}")
    else()
        find_package(nanobind CONFIG PATHS ${gweni_nanobind_dir} NO_DEFAULT_PATH)
        if(NOT nanobind_FOUND)
            set(gweni_python_missing "no nanobind CMake package in ${gweni_nanobind_dir}")
        endif()
    endif()
endif()
if(NOT TARGET gweni_app)
    set(gweni_python_missing "no gweni_app (GWENI_BUILD_APP and the GL renderer)")
endif()

if(gweni_python_missing)
    message(STATUS "gweni: no Python module: ${gweni_python_missing}")
    if(GWENI_BUILD_TESTS)
        # registered to fail, never skipped: a tree that should test the
        # module says why it cannot
        set(gweni_python_tests python_pytest python_pytest_gpu python_exports python_parity python_window_idle
            python_stubs python_mypy)
        # python_wheel is the release tree's alone (tests.cmake)
        if(CMAKE_BUILD_TYPE STREQUAL "Release")
            list(APPEND gweni_python_tests python_wheel)
        endif()
        foreach(test ${gweni_python_tests})
            add_test(NAME ${test}
                COMMAND ${CMAKE_COMMAND}
                    "-DMESSAGE=${test}: the Python module is not built: ${gweni_python_missing}; from a host, ./run_docker.sh -r rebuilds the image with CPython 3.12 (Dockerfile)"
                    -P ${CMAKE_CURRENT_SOURCE_DIR}/missing.cmake)
            set_tests_properties(${test} PROPERTIES LABELS python)
        endforeach()
    endif()
    return()
endif()

nanobind_add_module(_gweni STABLE_ABI NB_STATIC
    src/module.cpp
    src/session.h
    src/session.cpp
    src/control.h
    src/control.cpp
    src/headless.cpp
    src/app.cpp
    src/controls.cpp
    src/events.h
    src/events.cpp
    src/handles.h
    src/simple.cpp
    src/containers.cpp
)
target_link_libraries(_gweni PRIVATE gweni_app)
gweni_set_warnings(_gweni)
set_target_properties(_gweni PROPERTIES DEBUG_POSTFIX "")
# One exported symbol, PyInit__gweni (exports.map), and none of the static
# libraries' (freetype, libpng, libjpeg-turbo, GLFW, GLEW), so they never meet
# Pillow's, matplotlib's or pygame's copies in the same process
target_link_options(_gweni PRIVATE
    -Wl,--version-script=${CMAKE_CURRENT_SOURCE_DIR}/exports.map
    -Wl,--exclude-libs,ALL)
set_property(TARGET _gweni APPEND PROPERTY LINK_DEPENDS ${CMAKE_CURRENT_SOURCE_DIR}/exports.map)

file(GLOB gweni_python_files CONFIGURE_DEPENDS
    ${CMAKE_CURRENT_SOURCE_DIR}/gweni/*.py
    ${CMAKE_CURRENT_SOURCE_DIR}/gweni/*.pyi
    ${CMAKE_CURRENT_SOURCE_DIR}/gweni/py.typed)

install(TARGETS _gweni LIBRARY DESTINATION gweni COMPONENT python EXCLUDE_FROM_ALL)
install(FILES ${gweni_python_files} DESTINATION gweni COMPONENT python EXCLUDE_FROM_ALL)
# resources/ as a mirror, so a staged package an earlier build left holds what
# a fresh one does (install(DIRECTORY) never deletes)
install(CODE "
    execute_process(
        COMMAND \"${CMAKE_COMMAND}\"
            \"-DSRC=${GWENI_SOURCE_DIR}/resources\"
            \"-DDST=\$ENV{DESTDIR}\${CMAKE_INSTALL_PREFIX}/gweni/resources\"
            -P \"${GWENI_SOURCE_DIR}/cmake/mirrorDirectory.cmake\"
        RESULT_VARIABLE gweni_mirror_result)
    if(NOT gweni_mirror_result EQUAL 0)
        message(FATAL_ERROR \"gweni: mirroring resources/ into the Python package failed\")
    endif()"
    COMPONENT python EXCLUDE_FROM_ALL)

# The licenses of what the module links statically, in gweni/licenses/ (named
# in python/THIRD_PARTY_NOTICES.md, which the wheel's metadata carries): the
# vcpkg ports' copyright files, the vendored freetype-gl's and imglib's, and
# nanobind's from the package it was built with. gweni's own LICENSE and the
# fonts' are the wheel's license files (pyproject.toml); the fonts' are in
# resources/ too.
set(gweni_python_ports freetype libpng zlib bzip2 brotli libjpeg-turbo glfw3 glew md4c rapidjson glm)
set(gweni_python_port_share "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/share")
foreach(port IN LISTS gweni_python_ports)
    if(EXISTS "${gweni_python_port_share}/${port}/copyright")
        install(FILES "${gweni_python_port_share}/${port}/copyright" RENAME "${port}.txt"
            DESTINATION gweni/licenses COMPONENT python EXCLUDE_FROM_ALL)
    else()
        message(WARNING "gweni: no ${gweni_python_port_share}/${port}/copyright for the Python package's licenses")
    endif()
endforeach()
install(FILES ${GWENI_SOURCE_DIR}/deps/freetype-gl/LICENSE RENAME freetype-gl.txt
    DESTINATION gweni/licenses COMPONENT python EXCLUDE_FROM_ALL)
install(FILES ${GWENI_SOURCE_DIR}/deps/imglib/LICENSE RENAME imglib.txt
    DESTINATION gweni/licenses COMPONENT python EXCLUDE_FROM_ALL)
install(FILES ${GWENI_SOURCE_DIR}/LICENSE RENAME gweni.txt
    DESTINATION gweni/licenses COMPONENT python EXCLUDE_FROM_ALL)
file(GLOB gweni_nanobind_license "${gweni_nanobind_dir}/../../nanobind-*.dist-info/licenses/LICENSE"
    "${gweni_nanobind_dir}/../LICENSE")
if(gweni_nanobind_license)
    list(GET gweni_nanobind_license 0 gweni_nanobind_license)
    install(FILES "${gweni_nanobind_license}" RENAME nanobind.txt
        DESTINATION gweni/licenses COMPONENT python EXCLUDE_FROM_ALL)
else()
    message(WARNING "gweni: no nanobind LICENSE beside ${gweni_nanobind_dir}")
endif()

# Stage the package on every build, as the wheel lays it out:
# <tree>/python/gweni/
add_custom_target(gweni_python_stage ALL
    COMMAND ${CMAKE_COMMAND} --install ${CMAKE_BINARY_DIR} --component python
        --prefix ${GWENI_PYTHON_STAGE}
    COMMENT "Staging the Python package in ${GWENI_PYTHON_STAGE}/gweni"
    VERBATIM)
add_dependencies(gweni_python_stage _gweni)

# Writes the checked-in stub, python/gweni/_gweni.pyi, from the module just
# built (stubs.cmake); python_stubs fails while the two differ. Not part of
# all: it writes the source tree.
add_custom_target(gweni_python_stubs
    COMMAND ${CMAKE_COMMAND} -DPYTHON=${Python_EXECUTABLE} -DSTAGE=${GWENI_PYTHON_STAGE}
        -DOUT=${CMAKE_CURRENT_SOURCE_DIR}/gweni/_gweni.pyi -P ${CMAKE_CURRENT_SOURCE_DIR}/stubs.cmake
    COMMENT "Writing python/gweni/_gweni.pyi from the built module"
    VERBATIM)
add_dependencies(gweni_python_stubs gweni_python_stage)

if(GWENI_BUILD_TESTS)
    include(${CMAKE_CURRENT_SOURCE_DIR}/tests/tests.cmake)
endif()
