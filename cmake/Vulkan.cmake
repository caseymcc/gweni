#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The Vulkan build plumbing, included by CMakeLists.txt when
#  GWENI_RENDER_VULKAN is on. Its packages come from the vcpkg manifest feature
#  "vulkan" (vcpkg.json), which ./generate.sh turns on with the option.
#
#  It defines:
#
#    gweni_vulkan                  an INTERFACE target: the Vulkan headers and
#                                  volk, which loads libvulkan.so.1 at run time
#                                  (nothing links libvulkan), with
#                                  VK_NO_PROTOTYPES; CMakeLists.txt also gives
#                                  everything linking gweni GWENI_HAS_VULKAN
#    gweni_add_spirv(<target> <shader>...)
#                                  compiles each GLSL shader (.vert, .frag, ...)
#                                  to SPIR-V with glslangValidator at build time
#                                  and makes it a header <target>'s sources
#                                  include by the shader's file name plus ".h":
#                                  smoke.vert -> #include "smoke.vert.h", which
#                                  defines const uint32_t smoke_vert[] (the
#                                  name with every non-alphanumeric as '_')
#    gweni_vulkan_test_environment(<test>...)
#                                  the environment every Vulkan test runs in:
#                                  the vcpkg validation layer alone on the
#                                  loader's layer path, and the device pinned to Mesa's
#                                  lavapipe (GWENI_VULKAN_TEST_ICD); also the
#                                  labels gpu and vulkan
#

find_package(VulkanHeaders CONFIG QUIET)
find_package(volk CONFIG QUIET)

set(gweni_vulkan_installed "${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}")

# Plain paths in vcpkg's fixed layout, not cached find_*() results: a cached
# path would outlive the feature being dropped from an existing tree, and the
# check below would pass with the files gone.
set(GWENI_GLSLANG_VALIDATOR "${gweni_vulkan_installed}/tools/glslang/glslangValidator")
set(GWENI_VULKAN_LAYER_MANIFEST
    "${gweni_vulkan_installed}/share/vulkan/explicit_layer.d/VkLayer_khronos_validation.json")
set(GWENI_VULKAN_LAYER_LIBRARY "${gweni_vulkan_installed}/lib/libVkLayer_khronos_validation.so")
foreach(file GWENI_GLSLANG_VALIDATOR GWENI_VULKAN_LAYER_MANIFEST GWENI_VULKAN_LAYER_LIBRARY)
    if(NOT EXISTS "${${file}}")
        set(${file} "${${file}} (missing)")
        set(gweni_vulkan_missing TRUE)
    endif()
endforeach()

if(NOT VulkanHeaders_FOUND OR NOT volk_FOUND OR gweni_vulkan_missing)
    message(FATAL_ERROR
        "GWENI_RENDER_VULKAN is ON but the vcpkg feature \"vulkan\" is not installed "
        "(VulkanHeaders found: ${VulkanHeaders_FOUND}, volk found: ${volk_FOUND}, "
        "glslangValidator: ${GWENI_GLSLANG_VALIDATOR}, "
        "validation layer: ${GWENI_VULKAN_LAYER_MANIFEST}, ${GWENI_VULKAN_LAYER_LIBRARY}). "
        "Configure through ./generate.sh, which adds it, or pass "
        "-DVCPKG_MANIFEST_FEATURES=vulkan; -DGWENI_RENDER_VULKAN=OFF builds without Vulkan.")
endif()

# The installed manifest names its library bare ("library_path":
# "libVkLayer_khronos_validation.so"), which the loader dlopen()s through the
# system search path, where it is not. The tests' layer path is a copy of it in
# the build tree naming the library by its absolute path instead (the release
# build, in every tree; it is not instrumented under ASAN, as no vcpkg
# dependency is).
set(GWENI_VULKAN_LAYER_DIR "${CMAKE_BINARY_DIR}/vulkan/explicit_layer.d")
file(READ "${GWENI_VULKAN_LAYER_MANIFEST}" gweni_layer_manifest)
string(REGEX REPLACE "\"library_path\": *\"[^\"]*\""
    "\"library_path\": \"${GWENI_VULKAN_LAYER_LIBRARY}\""
    gweni_layer_manifest "${gweni_layer_manifest}")
set(gweni_layer_copy "${GWENI_VULKAN_LAYER_DIR}/VkLayer_khronos_validation.json")
set(gweni_layer_old "")
if(EXISTS "${gweni_layer_copy}")
    file(READ "${gweni_layer_copy}" gweni_layer_old)
endif()
if(NOT gweni_layer_old STREQUAL gweni_layer_manifest)
    file(WRITE "${gweni_layer_copy}" "${gweni_layer_manifest}")
endif()

message(STATUS "Vulkan: glslangValidator ${GWENI_GLSLANG_VALIDATOR}")
message(STATUS "Vulkan: validation layer ${GWENI_VULKAN_LAYER_LIBRARY}")

add_library(gweni_vulkan INTERFACE)
target_link_libraries(gweni_vulkan INTERFACE Vulkan::Headers volk::volk)
target_compile_definitions(gweni_vulkan INTERFACE VK_NO_PROTOTYPES)

function(gweni_add_spirv target)
    set(outputDir "${CMAKE_CURRENT_BINARY_DIR}/${target}_spirv")
    file(MAKE_DIRECTORY "${outputDir}")

    set(headers "")
    foreach(shader ${ARGN})
        get_filename_component(shaderPath "${shader}" ABSOLUTE)
        get_filename_component(shaderName "${shader}" NAME)
        get_filename_component(shaderDir "${shaderPath}" DIRECTORY)
        string(MAKE_C_IDENTIFIER "${shaderName}" symbol)
        set(header "${outputDir}/${shaderName}.h")

        # -I the shader's own dir so a shared body can be #included
        # (GL_GOOGLE_include_directive); the depfile rebuilds on its change
        add_custom_command(
            OUTPUT "${header}"
            COMMAND "${GWENI_GLSLANG_VALIDATOR}" -V --target-env vulkan1.3
                --vn "${symbol}" "-I${shaderDir}"
                --depfile "${header}.d"
                -o "${header}" "${shaderPath}"
            DEPENDS "${shaderPath}"
            DEPFILE "${header}.d"
            COMMENT "Compiling ${shaderName} to SPIR-V"
            VERBATIM)
        list(APPEND headers "${header}")
    endforeach()

    target_sources(${target} PRIVATE ${headers})
    target_include_directories(${target} PRIVATE "${outputDir}")
endfunction()

# lavapipe's ICD manifest, which the tests pin the loader to: they are written
# against Mesa's CPU rasteriser (as the GL tests are against llvmpipe), and a
# host's own GPU driver can fail to enumerate (radv of the container's Mesa does
# on a Radeon 8060S, failing vkEnumeratePhysicalDevices for every device).
# Empty leaves the loader's own choice.
find_file(GWENI_VULKAN_TEST_ICD lvp_icd.x86_64.json
    PATHS /usr/share/vulkan/icd.d
    NO_DEFAULT_PATH)
if(NOT GWENI_VULKAN_TEST_ICD)
    message(STATUS "Vulkan: lavapipe (mesa-vulkan-drivers) not found; the Vulkan tests "
        "will fail until the image has it")
    set(GWENI_VULKAN_TEST_ICD "")
endif()

function(gweni_vulkan_test_environment)
    # VK_LAYER_PATH replaces the explicit layer search path, so only this
    # layer, the one built with the headers, can be the one enabled.
    # VK_ICD_FILENAMES is what the container's loader (1.3.204) reads; newer
    # loaders read VK_DRIVER_FILES and still take the old name.
    set(environment "VK_LAYER_PATH=${GWENI_VULKAN_LAYER_DIR}")
    if(GWENI_VULKAN_TEST_ICD)
        list(APPEND environment
            "VK_ICD_FILENAMES=${GWENI_VULKAN_TEST_ICD}"
            "VK_DRIVER_FILES=${GWENI_VULKAN_TEST_ICD}")
    endif()
    set_property(TEST ${ARGN} APPEND PROPERTY ENVIRONMENT ${environment})
    set_property(TEST ${ARGN} APPEND PROPERTY LABELS gpu vulkan)
endfunction()
