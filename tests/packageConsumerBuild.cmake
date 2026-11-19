#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Steps 2 and 3 of package_consumer (packageConsumer.cmake), shared with
#  vcpkg_port (vcpkgPort.cmake): tests/package/ configured and built against a
#  gweni package, then run (under the render lock when mm is on PATH, through
#  RUNNER on the web). Included by those scripts, with
#
#    package_test_name  the test's name, for the messages
#    check_run(<what> COMMAND ...)  the scripts' own, failing the test
#    prefix             the package's prefix (share/gweni/gweniConfig.cmake)
#    build              the consumer's build directory
#    CONSUMER_DIR GENERATOR BUILD_TYPE TOOLCHAIN TRIPLET INSTALLED_DIR SCRATCH_DIR
#    and optionally CHAINLOAD C_COMPILER CXX_COMPILER WEB_PAGE_DIR SANITIZE NODE RUNNER
#

# the consumer, configured and built against it
set(configure_args
    -S "${CONSUMER_DIR}" -B "${build}" -G "${GENERATOR}"
    "-DCMAKE_BUILD_TYPE=${BUILD_TYPE}"
    "-DCMAKE_TOOLCHAIN_FILE=${TOOLCHAIN}"
    "-DVCPKG_TARGET_TRIPLET=${TRIPLET}"
    "-DVCPKG_INSTALLED_DIR=${INSTALLED_DIR}"
    -DVCPKG_MANIFEST_MODE=OFF
    "-DCMAKE_PREFIX_PATH=${prefix}"
    # the config itself too: Emscripten's toolchain finds packages inside its
    # sysroot and vcpkg_installed alone (CMAKE_FIND_ROOT_PATH), not in a prefix
    "-Dgweni_DIR=${prefix}/share/gweni")
if(CHAINLOAD)
    list(APPEND configure_args "-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=${CHAINLOAD}")
endif()
if(C_COMPILER)
    list(APPEND configure_args "-DCMAKE_C_COMPILER=${C_COMPILER}")
endif()
if(CXX_COMPILER)
    list(APPEND configure_args "-DCMAKE_CXX_COMPILER=${CXX_COMPILER}")
endif()
if(WEB_PAGE_DIR)
    list(APPEND configure_args "-DPACKAGE_WEB_PAGE_DIR=${WEB_PAGE_DIR}")
endif()
if(SANITIZE)
    list(APPEND configure_args
        "-DCMAKE_CXX_FLAGS=-fsanitize=${SANITIZE} -fno-omit-frame-pointer"
        "-DCMAKE_EXE_LINKER_FLAGS=-fsanitize=${SANITIZE}")
endif()
check_run("configuring tests/package against the package"
    COMMAND "${CMAKE_COMMAND}" ${configure_args})
# A job count of its own: a bare --parallel is an unlimited make -j, which
# ignores CMAKE_BUILD_PARALLEL_LEVEL and, one translation unit per installed
# header, ran 200 compilers at once (the Pages runner has 4 cores, #579).
if(DEFINED ENV{CMAKE_BUILD_PARALLEL_LEVEL} AND NOT "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}" STREQUAL "")
    set(jobs "$ENV{CMAKE_BUILD_PARALLEL_LEVEL}")
else()
    cmake_host_system_information(RESULT jobs QUERY NUMBER_OF_LOGICAL_CORES)
endif()
check_run("building tests/package against the package"
    COMMAND "${CMAKE_COMMAND}" --build "${build}" --parallel ${jobs})

# the run
set(consumer "${build}/package_consumer")
if(RUNNER)
    string(APPEND consumer ".html")
endif()
if(NOT EXISTS "${consumer}")
    message(FATAL_ERROR "${package_test_name}: the build made no ${consumer}")
endif()
find_program(mm_program mm)
if(RUNNER)
    set(run_command "${NODE}" "${RUNNER}" "${consumer}")
elseif(mm_program)
    set(run_command "${mm_program}" run --lock render -- "${consumer}")
else()
    message(STATUS "${package_test_name}: mm is not on PATH, so the run takes no render lock")
    set(run_command "${consumer}")
endif()
check_run("${package_test_name}" COMMAND ${run_command} WORKING_DIRECTORY "${SCRATCH_DIR}")
