#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The web tree's tests that hold the browser build to native, included by
#  tests/CMakeLists.txt in the web tree (EMSCRIPTEN) once web_runner is set and
#  ControlTest is a page:
#  - web_controltest_determinism: determinism.cmake in the browser, two page
#    loads of --shot-all --dump-bounds byte for byte the same;
#  - web_bounds_native[_scale150|_scale200]: every --dump-bounds listing
#    byte-identical to the native GLES tree's;
#  - web_stats_native: every --stats counter of every control, and OpenGLCore's,
#    identical to native;
#  - web_pixels_native[_scale200]: every --shot-all PNG and the hero screenshot
#    within the bound below of native.
#  The last three run the native reference themselves (web/native.mjs), from
#  GWENI_NATIVE_REFERENCE_DIR, the native GLES tree of the same build type
#  (./generate.sh web passes it), and fail when it is not built: they never skip.
#  Each native run takes the "render" lock through mm when mm is on PATH (one
#  run per lock); the browser runs take none.
#

set(GWENI_NATIVE_REFERENCE_DIR "${GWENI_NATIVE_REFERENCE_DIR}" CACHE PATH
    "the native GLES tree the web tree's web_*_native tests compare with (./generate.sh web sets it)")
if(NOT GWENI_NATIVE_REFERENCE_DIR)
    if(CMAKE_BUILD_TYPE STREQUAL "Release")
        set(web_native_type release)
    else()
        set(web_native_type debug)
    endif()
    set(GWENI_NATIVE_REFERENCE_DIR "${GWENI_SOURCE_DIR}/build_linux_x64_${web_native_type}_gles")
endif()
message(STATUS "tests: the web_*_native tests compare with ${GWENI_NATIVE_REFERENCE_DIR}")

add_test(NAME web_controltest_determinism
    COMMAND ${CMAKE_COMMAND}
        -DCONTROLTEST=$<TARGET_FILE:ControlTest>
        "-DRUNNER=${web_runner}"
        -DSOURCE_DIR=${GWENI_SOURCE_DIR}
        -DSCRATCH_DIR=${CMAKE_CURRENT_BINARY_DIR}/web_controltest_determinism
        -P ${CMAKE_CURRENT_SOURCE_DIR}/determinism.cmake)
set_tests_properties(web_controltest_determinism PROPERTIES
    LABELS web
    TIMEOUT 600)

if(NOT TARGET OpenGLCore)
    message(STATUS "tests: the web_*_native tests need OpenGLCore.html too")
    return()
endif()

set(web_native ${CMAKE_CROSSCOMPILING_EMULATOR} ${CMAKE_CURRENT_SOURCE_DIR}/web/native.mjs)
set(web_native_bins
    --native-bin ${GWENI_NATIVE_REFERENCE_DIR}/examples/openglcore
    --page-bin $<TARGET_FILE_DIR:ControlTest>)

# The pixel bound, per PNG (web/pngdiff.mjs): no channel of any pixel more than
# web_pixels_max_delta from native, and at most web_pixels_max_fraction percent
# of the pixels more than web_pixels_count_over from it. 1-level differences are
# not counted: text and gradient pixels whose blend or float -> 8-bit
# conversion the two rasterisers round differently, up to 10.5 % of a page
# (LabelMultilineDemo), which no shader change removes.
# Measured over the 39 --shot-all PNGs and the hero at scales 1, 1.5 and 2,
# SwiftShader (Chromium 158's headless shell) against llvmpipe (Mesa 23.2.1):
#   scale  max |delta|  pixels more than 1 off, worst
#   1      2            0.021 % (TreeControlDemo); ImagePanelDemo 4 and 0.046 %, left out below
#   1.5    2            0.005 % (ImagePanelDemo)
#   2      2            0.017 % (ImagePanelDemo)
#   the hero: 2 and 0.006 % at most; any difference at all: up to 10.5 % (LabelMultilineDemo)
# so 2 and 0.1 %, the latter the measured worst rounded up. The ceiling is
# max |delta| 2 and 0.5 %: a PNG above it is a bug to find (docs/architecture.md
# -> "The browser against native"), never a reason to widen the bound.
# Counting only pixels more than 1 off, and leaving ImagePanelDemo out at scale 1
# until #418, is the user's decision (#382): the first bar, every differing pixel
# counted against 0.5 %, cannot be met by any shader. A change to this rule is
# theirs too.
set(web_pixels_max_delta 2)
set(web_pixels_max_fraction 0.1)
set(web_pixels_count_over 1)
# PNGs left out of web_pixels_native at scale 1, each with its issue:
# - ImagePanelDemo: gwen.png, 896x896, minified to 100x100, sampled trilinearly from
#   glGenerateMipmap's levels, both implementation-defined (the mip filter, the
#   LOD and the trilinear weights): max |delta| 4 on one pixel, 3 on two.
#   Issue #418: build the mip chain on the CPU and filter in the shader.
set(web_pixels_excluded_scale1 ImagePanelDemo)

# each scale with its test-name suffix, as controltest_determinism_scale150's
foreach(scale_suffix "1:" "1.5:_scale150" "2:_scale200")
    string(REPLACE ":" ";" scale_suffix "${scale_suffix}")
    list(GET scale_suffix 0 scale)
    list(GET scale_suffix 1 suffix)
    set(scale_args)
    if(suffix)
        set(scale_args --scale ${scale})
    endif()

    add_test(NAME web_bounds_native${suffix}
        COMMAND ${web_native} bounds ${web_native_bins} ${scale_args}
            --scratch ${CMAKE_CURRENT_BINARY_DIR}/web_bounds_native${suffix})
    set_tests_properties(web_bounds_native${suffix} PROPERTIES
        LABELS web
        TIMEOUT 600)

    if(scale STREQUAL "1.5")
        continue()
    endif()
    set(exclude_args)
    if(scale STREQUAL "1")
        foreach(demo ${web_pixels_excluded_scale1})
            list(APPEND exclude_args --exclude ${demo})
        endforeach()
    endif()
    add_test(NAME web_pixels_native${suffix}
        COMMAND ${web_native} pixels ${web_native_bins} ${scale_args}
            --max-delta ${web_pixels_max_delta}
            --max-fraction ${web_pixels_max_fraction}
            --count-over ${web_pixels_count_over}
            ${exclude_args}
            --scratch ${CMAKE_CURRENT_BINARY_DIR}/web_pixels_native${suffix})
    set_tests_properties(web_pixels_native${suffix} PROPERTIES
        LABELS web
        TIMEOUT 600)
endforeach()

add_test(NAME web_stats_native
    COMMAND ${web_native} stats ${web_native_bins}
        --scratch ${CMAKE_CURRENT_BINARY_DIR}/web_stats_native)
set_tests_properties(web_stats_native PROPERTIES
    LABELS web
    TIMEOUT 900)
