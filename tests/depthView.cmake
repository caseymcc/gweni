#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The depth view's check, run as a cmake -P script (tests/CMakeLists.txt
#  registers it as controltest_depth_view and, with Vulkan, _vulkan):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -P depthView.cmake
#
#  Runs ControlTest --scenario depth-view --require-one-pass twice, in two
#  separate processes, and fails unless both pass and every PNG the first wrote
#  is byte-identical to the second's: the shots through the turned perspective
#  and orthographic cameras are as deterministic as the front view. The
#  scenario itself (examples/openglcore/controltest.cpp, "--scenario
#  depth-view") fails unless F9 opening the view, each camera-only frame, a
#  click while it is open and F9 closing it enter no control, write and upload
#  nothing, lay out nothing and renumber no z, the click presses nothing, the
#  hover, focus, tree and live ids are kept, and the frame after it closes is
#  the frame before it opened, byte for byte.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "depthView.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

foreach(run a b)
    execute_process(
        COMMAND "${CONTROLTEST}" --headless --scenario depth-view --require-one-pass
            --shot-dir "${SCRATCH_DIR}/${run}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors)
    message("run ${run}:\n${output}${errors}")
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "run ${run}: ControlTest exited with '${result}'")
    endif()
endforeach()

file(GLOB shots RELATIVE "${SCRATCH_DIR}/a" "${SCRATCH_DIR}/a/*.png")
list(SORT shots)
list(LENGTH shots count)
if(count LESS 4)
    message(FATAL_ERROR "the scenario wrote ${count} PNGs to ${SCRATCH_DIR}/a, not 4")
endif()

set(differing)
foreach(shot IN LISTS shots)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
            "${SCRATCH_DIR}/a/${shot}" "${SCRATCH_DIR}/b/${shot}"
        RESULT_VARIABLE result
        OUTPUT_QUIET ERROR_QUIET)
    if(NOT result EQUAL 0)
        list(APPEND differing "${shot}")
    endif()
endforeach()

if(differing)
    message(FATAL_ERROR "differ between two runs (kept in ${SCRATCH_DIR}/a and b): ${differing}")
endif()
message("${count} PNGs identical across two runs: ${shots}")
