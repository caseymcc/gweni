#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  app_window and app_window_vulkan: gweni_app's app::Window in a real window
#  (tests/app/window.cpp, whose header lists the checks), under an X server of
#  its own (xvfb-run -a, a free display, killed when the run ends). It fails
#  unless the exe exited 0 and printed "app_window: passed"; without xvfb-run
#  or Xvfb it fails, saying so, never skips.
#
#  -noreset: the test opens a second window after the first is gone, and an
#  Xvfb whose last client left resets, refusing a connection made meanwhile.
#
#  -DEXE=<gweni_app_window_test> -DRESOURCES=<resources dir>
#  [-DSERVERNUM=<n>]: the display xvfb-run -a searches up from, one of its
#  own for each variant, so a parallel ctest's other X servers (from 99) do
#  not race it for a number
#
cmake_minimum_required(VERSION 3.16)

function(fail message)
    message(FATAL_ERROR "app_window: FAIL: ${message}")
endfunction()

find_program(XVFB_RUN xvfb-run)
find_program(XVFB Xvfb)
if(NOT XVFB_RUN OR NOT XVFB)
    fail("no xvfb-run or Xvfb on PATH; the window test needs Xvfb (the image's xvfb package; ./run_docker.sh -r rebuilds it)")
endif()

if(NOT SERVERNUM)
    set(SERVERNUM 120)
endif()

execute_process(
    COMMAND ${XVFB_RUN} -a -n ${SERVERNUM} -s "-screen 0 1280x1024x24 -nolisten tcp -noreset" ${EXE} ${RESOURCES}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 120)
message("${output}${errors}")
if(NOT result EQUAL 0)
    fail("the window test exited ${result}")
endif()
if(NOT output MATCHES "app_window: passed")
    fail("the window test did not pass its own checks")
endif()
