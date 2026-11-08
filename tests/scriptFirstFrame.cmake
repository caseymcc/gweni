#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  ControlTest script mode's own frames, run as a cmake -P script
#  (tests/CMakeLists.txt registers it as controltest_script_first_frame):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -P scriptFirstFrame.cmake
#
#  Criterion 1: a script that starts with input is settled first and a --shot
#  straight after input renders one frame, so ButtonDemo's
#      --move 30,5 --shot a.png
#  writes the same PNG, byte for byte, as
#      --frames 3 --move 30,5 --frames 1 --shot b.png
#  and that PNG shows "Event Tester" hovered (it differs from the idle shot).
#  Without them the first one read back a framebuffer nothing had drawn into.
#
#  Criterion 4: a shot before any frame was rendered (--frames 0 --shot) exits
#  non-zero and writes no PNG.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "scriptFirstFrame.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# run(<name> <args>...): ControlTest on ButtonDemo, its output in <name>.log
# and its exit status in result_<name>
function(run name)
    execute_process(
        COMMAND "${CONTROLTEST}" --headless --control ButtonDemo ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${SCRATCH_DIR}/${name}.log"
        ERROR_FILE "${SCRATCH_DIR}/${name}.log")
    set(result_${name} "${result}" PARENT_SCOPE)
endfunction()

function(require_success name)
    if(NOT result_${name} EQUAL 0)
        file(READ "${SCRATCH_DIR}/${name}.log" log)
        message(FATAL_ERROR "${name}: ControlTest exited with '${result_${name}}'\n${log}")
    endif()
endfunction()

set(failures)

# criterion 1
run(input_first --move 30,5 --shot "${SCRATCH_DIR}/input-first.png")
run(frames_first --frames 3 --move 30,5 --frames 1 --shot "${SCRATCH_DIR}/frames-first.png")
run(idle --frames 4 --shot "${SCRATCH_DIR}/idle.png")
foreach(name input_first frames_first idle)
    require_success(${name})
endforeach()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
            "${SCRATCH_DIR}/input-first.png" "${SCRATCH_DIR}/frames-first.png"
    RESULT_VARIABLE differs)
if(NOT differs EQUAL 0)
    list(APPEND failures "criterion 1: input-first shot differs from frames-first")
endif()

execute_process(
    COMMAND ${CMAKE_COMMAND} -E compare_files
            "${SCRATCH_DIR}/frames-first.png" "${SCRATCH_DIR}/idle.png"
    RESULT_VARIABLE differs)
if(differs EQUAL 0)
    list(APPEND failures
        "criterion 1: the --move 30,5 shot is the idle shot: \"Event Tester\" is not hovered")
endif()

# criterion 4
run(zero_frames --frames 0 --shot "${SCRATCH_DIR}/zero-frames.png")
if(result_zero_frames EQUAL 0)
    list(APPEND failures "criterion 4: zero-frame shot exited 0")
endif()
if(EXISTS "${SCRATCH_DIR}/zero-frames.png")
    list(APPEND failures "criterion 4: zero-frame shot wrote a file")
endif()
file(READ "${SCRATCH_DIR}/zero_frames.log" log)
if(NOT log MATCHES "before any frame was rendered")
    list(APPEND failures "criterion 4: zero-frame shot did not say why on stderr:\n${log}")
endif()

if(failures)
    string(REPLACE ";" "\n  " failures "${failures}")
    message(FATAL_ERROR "controltest_script_first_frame failed:\n  ${failures}")
endif()

message(STATUS "script first frame: input-first matches frames-first, a zero-frame shot is refused")
