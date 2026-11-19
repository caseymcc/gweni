#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  package_consumer: the tree, installed, is a package an app builds against.
#  Run as a cmake -P script (tests/CMakeLists.txt registers it in every native
#  tree with a renderer, label package):
#
#      cmake -DTREE=<build tree> -DSOURCE_DIR=<repo> -DCONSUMER_DIR=<tests/package>
#            -DSCRATCH_DIR=<dir> -DGENERATOR=<generator> -DBUILD_TYPE=<type>
#            -DTOOLCHAIN=<vcpkg.cmake> -DCHAINLOAD=<toolchain> -DTRIPLET=<triplet>
#            -DINSTALLED_DIR=<the tree's vcpkg_installed> -DC_COMPILER=... -DCXX_COMPILER=...
#            [-DSANITIZE=address]
#            [-DNODE=<emsdk's node> -DRUNNER=<tests/web/run.mjs> -DWEB_PAGE_DIR=<examples/openglcore/web>]
#            -P packageConsumer.cmake
#
#  1. cmake --install of the tree into <scratch>/prefix. It fails, naming the
#     file, if
#     - an installed file other than the archive names the build tree (the
#       prefix included: the config must be relocatable), the source tree or
#       vcpkg_installed (the archive's debug information names its sources, as
#       any library's does);
#     - anything named freetype-gl is installed outside
#       include/gweni/third_party/ (vcpkg's freetype-gl port owns those paths);
#     - share/gweni/resources/ is not exactly the textured skin's files.
#  2. configures and builds tests/package/ against the prefix with the tree's
#     vcpkg toolchain, triplet and compilers, reusing the tree's own
#     vcpkg_installed with no manifest install (vcpkg builds nothing), and with
#     SANITIZE the tree's sanitizer flags; that project's configure checks what
#     gweni::gweni exports and compiles every installed header alone.
#  3. runs package_consumer, which draws a frame on each renderer the package
#     has, under the render lock (mm run --lock render) when mm is on PATH. No
#     lock is held around the install or the build. The test's environment
#     (ASAN_OPTIONS, the Vulkan layer and driver pins) reaches it.
#
#  With RUNNER (package_consumer_web, the web trees) the toolchain is the web
#  tree's (vcpkg's, chainloading emsdk's Emscripten.cmake, wasm32-emscripten),
#  the consumer is built as a page on WEB_PAGE_DIR's shell.html and pre.js (test
#  plumbing; it links no Emscripten setting of gweni's own) and run.mjs runs it
#  in headless Chromium on SwiftShader, which takes no lock.
#

foreach(var TREE SOURCE_DIR CONSUMER_DIR SCRATCH_DIR GENERATOR BUILD_TYPE TOOLCHAIN TRIPLET INSTALLED_DIR)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "packageConsumer.cmake: pass -D${var}=...")
    endif()
endforeach()

function(check_run what)
    execute_process(${ARGN} RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "package_consumer: ${what} failed (${status})")
    endif()
endfunction()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
set(prefix "${SCRATCH_DIR}/prefix")
set(build "${SCRATCH_DIR}/build")

# 1. the install
check_run("cmake --install ${TREE}"
    COMMAND "${CMAKE_COMMAND}" --install "${TREE}" --prefix "${prefix}" --config "${BUILD_TYPE}"
    OUTPUT_QUIET)

file(GLOB_RECURSE installed RELATIVE "${prefix}" "${prefix}/*")
set(problems "")
foreach(file IN LISTS installed)
    if(file MATCHES "freetype-gl" AND NOT file MATCHES "^include/gweni/third_party/")
        string(APPEND problems "\n  ${file}: freetype-gl installed outside include/gweni/third_party/")
    endif()
    if(file MATCHES "\\.a$")
        continue()
    endif()
    file(READ "${prefix}/${file}" content)
    foreach(path "${TREE}" "${SOURCE_DIR}" "vcpkg_installed")
        string(FIND "${content}" "${path}" at)
        if(at GREATER -1)
            string(APPEND problems "\n  ${file}: names ${path}")
        endif()
    endforeach()
endforeach()

set(skin_files
    DefaultSkin.json DefaultSkin.png
    DejaVuSansMono-LICENSE.txt DejaVuSansMono.ttf
    OpenSans-Bold.ttf OpenSans-BoldItalic.ttf OpenSans-Italic.ttf
    OpenSans-LICENSE.txt OpenSans.ttf)
file(GLOB resources RELATIVE "${prefix}/share/gweni/resources" "${prefix}/share/gweni/resources/*")
list(SORT resources)
list(SORT skin_files)
if(NOT resources STREQUAL skin_files)
    string(APPEND problems "\n  share/gweni/resources holds [${resources}], not the skin's [${skin_files}]")
endif()
if(problems)
    message(FATAL_ERROR "package_consumer: the installed package is wrong:${problems}")
endif()
list(LENGTH installed installed_count)
message(STATUS "package_consumer: installed ${installed_count} files into ${prefix}")

# 2. the consumer, configured and built against it
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

# 3. the run
set(consumer "${build}/package_consumer")
if(RUNNER)
    string(APPEND consumer ".html")
endif()
if(NOT EXISTS "${consumer}")
    message(FATAL_ERROR "package_consumer: the build made no ${consumer}")
endif()
find_program(mm_program mm)
if(RUNNER)
    set(run_command "${NODE}" "${RUNNER}" "${consumer}")
elseif(mm_program)
    set(run_command "${mm_program}" run --lock render -- "${consumer}")
else()
    message(STATUS "package_consumer: mm is not on PATH, so the run takes no render lock")
    set(run_command "${consumer}")
endif()
check_run("package_consumer" COMMAND ${run_command} WORKING_DIRECTORY "${SCRATCH_DIR}")
