#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  python_window_idle: python/tests/window_idle.py (its header says what it
#  checks) under an X server of its own (xvfb-run -a, a free display, killed
#  when the run ends), as window_idle runs ControlTest --idle-test
#  (tests/windowIdle.cmake). Without xvfb-run or Xvfb it fails, saying so; it
#  never skips.
#
#  -DPYTHON=<interpreter> -DSCRIPT=<window_idle.py>
#
cmake_minimum_required(VERSION 3.16)

find_program(XVFB_RUN xvfb-run)
find_program(XVFB Xvfb)
if(NOT XVFB_RUN OR NOT XVFB)
    message(FATAL_ERROR "python_window_idle: FAIL: no xvfb-run or Xvfb on PATH; the window test needs Xvfb (the image's xvfb package; ./run_docker.sh -r rebuilds it)")
endif()

execute_process(
    COMMAND ${XVFB_RUN} -a -s "-screen 0 1280x1024x24 -nolisten tcp" ${PYTHON} ${SCRIPT}
    RESULT_VARIABLE result
    OUTPUT_VARIABLE output
    ERROR_VARIABLE errors
    TIMEOUT 60)
message("${output}${errors}")
if(NOT result EQUAL 0)
    message(FATAL_ERROR "python_window_idle: FAIL: window_idle.py exited ${result}")
endif()
if(NOT output MATCHES "python_window_idle: passed")
    message(FATAL_ERROR "python_window_idle: FAIL: window_idle.py did not say it passed")
endif()
