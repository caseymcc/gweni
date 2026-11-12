#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  window_idle and window_idle_vulkan: the example window's idle loop
#  (input::GlfwLoop over platform::IdleLoop) in a real window. Under an X server
#  of its own (xvfb-run -a, a free display, killed when the run ends),
#  ControlTest --idle-test opens CONTROL's window on RENDERER (gl, or vulkan
#  with the validation layer from gweni_vulkan_test_environment), takes no
#  input, and once the canvas has settled:
#    1. lets it idle 2 s, which must draw no frame, swap or present nothing and
#       take under 0.1 s of the process's CPU time (getrusage, every thread);
#    2. posts to the canvas from another thread (Canvas::post), which must wake
#       the loop (its wake handler, glfwPostEmptyEvent) and draw exactly one
#       frame, writing glyph slots, with one swap or present;
#    3. closes the window from a canvas timer.
#  It fails unless ControlTest exited 0 and printed both measures and
#  "idle-test: passed"; a loop the post does not wake fails within 10 s,
#  and a hang is the test's TIMEOUT.
#
#  -DCONTROLTEST=<exe> [-DRENDERER=gl|vulkan] [-DCONTROL=ButtonDemo]
#
cmake_minimum_required(VERSION 3.16)

if(NOT CONTROL)
    set(CONTROL ButtonDemo)
endif()
if(NOT RENDERER)
    set(RENDERER gl)
endif()

function(fail message)
    message(FATAL_ERROR "window_idle (${RENDERER}): FAIL: ${message}")
endfunction()

# the X server comes from the image's xvfb package: no skipping without it
find_program(XVFB_RUN xvfb-run)
find_program(XVFB Xvfb)
if(NOT XVFB_RUN OR NOT XVFB)
    fail("no xvfb-run or Xvfb on PATH; the idle window test needs Xvfb (the image's xvfb package; ./run_docker.sh -r rebuilds it)")
endif()

execute_process(
    COMMAND ${XVFB_RUN} -a -s "-screen 0 1280x1024x24 -nolisten tcp"
        ${CONTROLTEST} --renderer ${RENDERER} --control ${CONTROL} --idle-test
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 120)
message("${output}${errors}")
if(NOT result EQUAL 0)
    fail("ControlTest --idle-test exited ${result}")
endif()

string(REGEX MATCH "idle-test: idle frames=([0-9]+) presents=([0-9]+) cpu_s=([0-9.e-]+)" idle "${output}")
if(NOT idle)
    fail("ControlTest printed no idle measure")
endif()
string(REGEX MATCH "idle-test: after post frames=([0-9]+) presents=([0-9]+)" posted "${output}")
if(NOT posted)
    fail("ControlTest printed no measure after the post")
endif()
if(NOT output MATCHES "idle-test: passed")
    fail("ControlTest did not pass its own checks")
endif()
