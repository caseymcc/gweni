#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  vcpkg_port (and vcpkg_port_web): ports/gweni/ installs through vcpkg and is
#  a package an app builds and draws with. Run as a cmake -P script
#  (tests/CMakeLists.txt registers it in the native release tree, label
#  package, and in the release web tree as vcpkg_port_web, labels web and
#  package):
#
#      cmake -DTEST_NAME=<name> -DSOURCE_DIR=<repo> -DCONSUMER_DIR=<tests/package>
#            -DSCRATCH_DIR=<dir> -DVCPKG_ROOT=<vcpkg> -DGENERATOR=<generator>
#            -DBUILD_TYPE=<type> -DTOOLCHAIN=<vcpkg.cmake> -DTRIPLET=<triplet>
#            -DFEATURES=opengl,vulkan [-DEXTRA_PORTS=freetype-gl,...]
#            [-DCHAINLOAD=<toolchain>] [-DC_COMPILER=...] [-DCXX_COMPILER=...]
#            [-DNODE=<emsdk's node> -DRUNNER=<tests/web/run.mjs> -DWEB_PAGE_DIR=<examples/openglcore/web>]
#            -P vcpkgPort.cmake
#
#  1. fails unless ports/gweni/vcpkg.json's version-semver is project(gweni
#     VERSION ...)'s (CMakeLists.txt).
#  2. copies ports/gweni/ into <scratch>/overlay/gweni with its
#     vcpkg_from_github(...) replaced by this checkout as SOURCE_PATH, and the
#     portfile's first line a stamp of the working tree (a hash of every file
#     git tracks or would add, as it is on disk, uncommitted changes included):
#     vcpkg's package ABI hashes the portfile, not the source, so without it a
#     binary cache would hand back the gweni of an earlier tree.
#  3. installs, in manifest mode through that overlay, a manifest of gweni with
#     FEATURES and EXTRA_PORTS beside it (natively vcpkg's own freetype-gl, so a
#     file both ports install fails the install), for TRIPLET, with its own
#     install, buildtrees and packages roots under the scratch dir (nothing a
#     concurrent configure uses is touched). The binary cache is read only
#     (--binarysource=clear;default,read): the dependencies come from what the
#     trees' configures stored, and no per-tree gweni is stored. It fails if
#     vcpkg fails or prints a warning (a post-build check), naming the log.
#  4. builds and runs tests/package/ against <scratch>/installed/<TRIPLET>
#     (packageConsumerBuild.cmake, package_consumer's own steps: its configure
#     checks what gweni::gweni exports and compiles every installed header
#     alone; its run draws a frame on each renderer, under the render lock when
#     mm is on PATH, or in headless Chromium through RUNNER).
#  No lock is held around the install or the builds.
#

foreach(var TEST_NAME SOURCE_DIR CONSUMER_DIR SCRATCH_DIR VCPKG_ROOT GENERATOR BUILD_TYPE TOOLCHAIN TRIPLET FEATURES)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "vcpkgPort.cmake: pass -D${var}=...")
    endif()
endforeach()
set(package_test_name ${TEST_NAME})
# comma-separated, so the test's command line needs no escaped ;
string(REPLACE "," ";" FEATURES "${FEATURES}")
string(REPLACE "," ";" EXTRA_PORTS "${EXTRA_PORTS}")

function(check_run what)
    execute_process(${ARGN} RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        message(FATAL_ERROR "${package_test_name}: ${what} failed (${status})")
    endif()
endfunction()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# 1. the port's version is the project's
file(READ "${SOURCE_DIR}/ports/gweni/vcpkg.json" port_manifest)
string(JSON port_version ERROR_VARIABLE json_error GET "${port_manifest}" version-semver)
if(json_error)
    message(FATAL_ERROR "${package_test_name}: ports/gweni/vcpkg.json has no version-semver (${json_error})")
endif()
file(READ "${SOURCE_DIR}/CMakeLists.txt" project_lists)
if(NOT project_lists MATCHES "project\\(gweni VERSION ([0-9.]+)")
    message(FATAL_ERROR "${package_test_name}: CMakeLists.txt has no project(gweni VERSION ...)")
endif()
set(project_version "${CMAKE_MATCH_1}")
if(NOT port_version STREQUAL project_version)
    message(FATAL_ERROR "${package_test_name}: ports/gweni/vcpkg.json is version ${port_version}, "
        "project(gweni VERSION ${project_version}) in CMakeLists.txt: they must agree")
endif()

# 2. the overlay, this checkout as its source, stamped
execute_process(COMMAND git -C "${SOURCE_DIR}" ls-files --cached --others --exclude-standard
    OUTPUT_VARIABLE tree_files RESULT_VARIABLE status)
if(NOT status EQUAL 0)
    message(FATAL_ERROR "${package_test_name}: git ls-files in ${SOURCE_DIR} failed (${status})")
endif()
string(REPLACE "\n" ";" tree_files "${tree_files}")
set(tree_listing "")
foreach(file IN LISTS tree_files)
    # a tracked file deleted in the working tree is not there to hash
    if(file STREQUAL "" OR IS_DIRECTORY "${SOURCE_DIR}/${file}" OR NOT EXISTS "${SOURCE_DIR}/${file}")
        continue()
    endif()
    file(SHA256 "${SOURCE_DIR}/${file}" file_hash)
    string(APPEND tree_listing "${file} ${file_hash}\n")
endforeach()
string(SHA256 tree_stamp "${tree_listing}")

set(overlay "${SCRATCH_DIR}/overlay")
file(COPY "${SOURCE_DIR}/ports/gweni" DESTINATION "${overlay}")
file(READ "${overlay}/gweni/portfile.cmake" portfile)
string(REGEX REPLACE "vcpkg_from_github\\([^)]*\\)" "set(SOURCE_PATH \"${SOURCE_DIR}\")" local_portfile "${portfile}")
if(local_portfile STREQUAL portfile)
    message(FATAL_ERROR "${package_test_name}: ports/gweni/portfile.cmake has no vcpkg_from_github(...) to replace")
endif()
file(WRITE "${overlay}/gweni/portfile.cmake"
    "# ${package_test_name}: the source is ${SOURCE_DIR}, its tree stamped ${tree_stamp}\n${local_portfile}")
message(STATUS "${package_test_name}: gweni ${port_version} from ${SOURCE_DIR}, tree ${tree_stamp}")

# 3. the install, in manifest mode
set(manifest_dir "${SCRATCH_DIR}/manifest")
set(dependencies "{\"name\": \"gweni\", \"default-features\": false, \"features\": [")
set(separator "")
foreach(feature IN LISTS FEATURES)
    string(APPEND dependencies "${separator}\"${feature}\"")
    set(separator ", ")
endforeach()
string(APPEND dependencies "]}")
foreach(port IN LISTS EXTRA_PORTS)
    string(APPEND dependencies ", \"${port}\"")
endforeach()
file(WRITE "${manifest_dir}/vcpkg.json"
    "{\n  \"name\": \"gweni-port-consumer\",\n  \"version\": \"0\",\n  \"dependencies\": [${dependencies}]\n}\n")

set(installed "${SCRATCH_DIR}/installed")
set(vcpkg_log "${SCRATCH_DIR}/vcpkg-install.log")
if(EXISTS "${VCPKG_ROOT}/vcpkg")
    set(vcpkg_program "${VCPKG_ROOT}/vcpkg")
else()
    find_program(vcpkg_program vcpkg REQUIRED)
endif()
execute_process(
    COMMAND "${CMAKE_COMMAND}" -E env "VCPKG_ROOT=${VCPKG_ROOT}"
        "${vcpkg_program}" install
            "--x-manifest-root=${manifest_dir}"
            "--overlay-ports=${overlay}"
            "--triplet=${TRIPLET}"
            "--x-install-root=${installed}"
            "--x-buildtrees-root=${SCRATCH_DIR}/buildtrees"
            "--x-packages-root=${SCRATCH_DIR}/packages"
            "--binarysource=clear;default,read"
            --clean-after-build
    WORKING_DIRECTORY "${SCRATCH_DIR}"
    OUTPUT_VARIABLE vcpkg_output ERROR_VARIABLE vcpkg_output
    RESULT_VARIABLE status)
file(WRITE "${vcpkg_log}" "${vcpkg_output}")
if(NOT status EQUAL 0)
    message("${vcpkg_output}")
    message(FATAL_ERROR "${package_test_name}: vcpkg install failed (${status}), log ${vcpkg_log}")
endif()
string(REGEX MATCHALL "[^\n]*[Ww]arning[^\n]*" vcpkg_warnings "${vcpkg_output}")
if(vcpkg_warnings)
    string(REPLACE ";" "\n  " vcpkg_warnings "${vcpkg_warnings}")
    message(FATAL_ERROR "${package_test_name}: vcpkg warned installing gweni (log ${vcpkg_log}):\n  ${vcpkg_warnings}")
endif()
set(prefix "${installed}/${TRIPLET}")
if(NOT EXISTS "${prefix}/share/gweni/gweniConfig.cmake")
    message(FATAL_ERROR "${package_test_name}: vcpkg installed no ${prefix}/share/gweni/gweniConfig.cmake")
endif()

# 4. the consumer, built against the installed port, and its run
set(build "${SCRATCH_DIR}/build")
set(INSTALLED_DIR "${installed}")
include("${CMAKE_CURRENT_LIST_DIR}/packageConsumerBuild.cmake")
