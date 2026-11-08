#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  window_vulkan: renderer::Vulkan in a real window, the Vulkan backend's
#  sign-off. Under an X server of its own (xvfb-run -a, a free display, killed
#  when the run ends) and the test window manager (gweni_test_wm, which makes an
#  iconify real: Xvfb has no window manager), ControlTest --window-test opens a
#  GLFW_NO_API window with a FIFO swapchain on lavapipe's X11 WSI and:
#    1. presents at SIZE (640x480) and reads the presented image back;
#    2. resizes itself from code (glfwSetWindowSize) to 5/4 and 4/5 of it and
#       back, reading back the image presented at each size;
#    3. iconifies (the window unmapped, WM_STATE iconic) and restores, reading
#       back the image presented after the restore.
#  It fails unless every image read back is byte-identical to a --headless
#  --renderer vulkan capture of the same control at that size, every resize
#  recreated the swapchain (resize_recreates >= resizes), nothing was drawn or
#  presented while the window was iconified (and the loop waited instead), and
#  ControlTest exited 0; the validation layer (gweni_vulkan_test_environment)
#  aborts on any message, and a hang is the test's TIMEOUT.
#
#  -DCONTROLTEST=<exe> -DTEST_WM=<exe> -DSCRATCH_DIR=<dir>
#  [-DCONTROL=ButtonDemo] [-DSIZE=640x480]
#
cmake_minimum_required(VERSION 3.16)

if(NOT CONTROL)
    set(CONTROL ButtonDemo)
endif()
if(NOT SIZE)
    set(SIZE 640x480)
endif()

function(fail message)
    message(FATAL_ERROR "window_vulkan: FAIL: ${message}")
endfunction()

# the X server comes from the image's xvfb package: no skipping without it
find_program(XVFB_RUN xvfb-run)
find_program(XVFB Xvfb)
if(NOT XVFB_RUN OR NOT XVFB)
    fail("no xvfb-run or Xvfb on PATH; the window test needs Xvfb (the image's xvfb package; ./run_docker.sh -r rebuilds it)")
endif()
if(NOT TEST_WM OR NOT EXISTS "${TEST_WM}")
    fail("no gweni_test_wm (\"${TEST_WM}\"): it needs the X11 headers (xorg-dev in the image)")
endif()

file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}/window" "${SCRATCH_DIR}/headless")

# a 24-bit screen larger than every size the window takes
execute_process(
    COMMAND ${XVFB_RUN} -a -s "-screen 0 1280x1024x24 -nolisten tcp"
        ${TEST_WM} ${CONTROLTEST} --renderer vulkan --control ${CONTROL} --size ${SIZE}
            --window-test ${SCRATCH_DIR}/window
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 240)
message("${output}${errors}")
if(NOT result EQUAL 0)
    fail("ControlTest --window-test exited ${result}")
endif()

string(REGEX MATCH "window-test: resizes=([0-9]+) swapchains=([0-9]+) recreates=([0-9]+) resize_recreates=([0-9]+)"
    summary "${output}")
if(NOT summary)
    fail("ControlTest printed no window-test summary line")
endif()
set(resizes ${CMAKE_MATCH_1})
set(resize_recreates ${CMAKE_MATCH_4})
if(resizes LESS 3)
    fail("${resizes} resizes; the test makes 3")
endif()
if(resize_recreates LESS resizes)
    fail("${resizes} resizes but ${resize_recreates} swapchain recreates for a new size")
endif()
string(REGEX MATCH "window-test: iconified frames=([0-9]+) presents=([0-9]+) skipped=([0-9]+)"
    iconified "${output}")
if(NOT iconified OR NOT CMAKE_MATCH_1 EQUAL 0 OR NOT CMAKE_MATCH_2 EQUAL 0 OR CMAKE_MATCH_3 EQUAL 0)
    fail("while iconified the loop must draw and present nothing and wait: ${iconified}")
endif()

# every image the window presented against a headless capture at its size
file(GLOB presented RELATIVE "${SCRATCH_DIR}/window" "${SCRATCH_DIR}/window/*.png")
list(SORT presented)
list(LENGTH presented count)
if(count LESS 5)
    fail("${count} presented images read back, not 5: ${presented}")
endif()

set(captured)
set(mismatches 0)
foreach(name IN LISTS presented)
    if(NOT name MATCHES "_([0-9]+x[0-9]+)\\.png$")
        fail("unexpected file ${name}")
    endif()
    set(size ${CMAKE_MATCH_1})
    set(reference "${SCRATCH_DIR}/headless/${CONTROL}_${size}.png")

    if(NOT size IN_LIST captured)
        execute_process(
            COMMAND ${CONTROLTEST} --headless --renderer vulkan --control ${CONTROL} --size ${size}
                --shot ${reference}
            RESULT_VARIABLE capture_result
            OUTPUT_VARIABLE capture_output
            ERROR_VARIABLE capture_errors
            TIMEOUT 120)
        if(NOT capture_result EQUAL 0)
            message("${capture_output}${capture_errors}")
            fail("the headless capture at ${size} exited ${capture_result}")
        endif()
        list(APPEND captured ${size})
    endif()

    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files
        "${SCRATCH_DIR}/window/${name}" "${reference}"
        RESULT_VARIABLE differs)
    if(differs)
        message("differs: ${name} from the headless capture ${CONTROL}_${size}.png")
        math(EXPR mismatches "${mismatches}+1")
    else()
        message("matches: ${name}")
    endif()
endforeach()

if(mismatches GREATER 0)
    fail("${mismatches} presented image(s) differ from the headless captures; both are in ${SCRATCH_DIR}")
endif()
message("window_vulkan: ${count} presented images byte-identical to headless captures,"
    " ${resize_recreates} recreates for ${resizes} resizes, nothing presented while iconified")
