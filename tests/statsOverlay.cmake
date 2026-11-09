#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The stats overlay's check, run as a cmake -P script (tests/CMakeLists.txt
#  registers it as controltest_stats_overlay, and _vulkan):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -P statsOverlay.cmake
#
#  1. ControlTest --scenario stats-overlay --require-one-pass must pass: over
#     the settled Demo, the overlay writes nothing between its refreshes and
#     only its own slots on one, shows what the frames of each window counted,
#     a hover on the next refresh and 0 on the one after, lets the mouse through
#     to a Button under it and gives its ids back (its header in
#     examples/openglcore/controltest.cpp).
#  2. The app's counters do not see it: Demo's --stats lines, the clock
#     stepping so the overlay refreshes, are the same with --stats-overlay as
#     without, but for the renderer's whole-frame counts, which hold its slots,
#     uploads and draws (prim_slots_written, text_slots_written, upload_*,
#     mirror_bytes, buffer_regrows, atlas_upload_*, draw_calls, prim_ids), and
#     excluded_changed, which counts its changed controls by design.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "statsOverlay.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# run(<name> <args>...): ControlTest, its stdout in <name>.out and its stderr
# in <name>.err; a non-zero exit fails the test
function(run name)
    execute_process(
        COMMAND "${CONTROLTEST}" --headless ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${SCRATCH_DIR}/${name}.out"
        ERROR_FILE "${SCRATCH_DIR}/${name}.err")
    if(NOT result EQUAL 0)
        file(READ "${SCRATCH_DIR}/${name}.out" out)
        file(READ "${SCRATCH_DIR}/${name}.err" err)
        message(FATAL_ERROR "${name}: ControlTest exited with '${result}'\n${out}${err}")
    endif()
endfunction()

# 1
run(scenario --scenario stats-overlay --require-one-pass --shot-dir "${SCRATCH_DIR}/shots")
file(READ "${SCRATCH_DIR}/scenario.out" out)
string(REGEX MATCH "stats-overlay [^\n]*" line "${out}")
message("${line}")

# 2
set(stats --control Demo --stats --stats-clock step --frames 40)
run(without ${stats})
run(with ${stats} --stats-overlay)

set(renderer_keys "prim_slots_written|text_slots_written|upload_calls|upload_bytes|mirror_bytes"
    "|buffer_regrows|atlas_upload_calls|atlas_upload_bytes|draw_calls|prim_ids|excluded_changed")
string(JOIN "" renderer_keys ${renderer_keys})

foreach(name without with)
    file(STRINGS "${SCRATCH_DIR}/${name}.out" lines REGEX "^frame=")
    set(kept)
    foreach(frame_line IN LISTS lines)
        string(REGEX REPLACE " (${renderer_keys})=[^ ]*" "" frame_line "${frame_line}")
        list(APPEND kept "${frame_line}")
    endforeach()
    set(lines_${name} "${kept}")
endforeach()

list(LENGTH lines_without count)
if(count EQUAL 0)
    message(FATAL_ERROR "no --stats frame lines from Demo (${SCRATCH_DIR}/without.out)")
endif()

foreach(i RANGE 1 ${count})
    math(EXPR index "${i}-1")
    list(GET lines_without ${index} a)
    list(GET lines_with ${index} b)
    if(NOT a STREQUAL b)
        message(FATAL_ERROR "the app's --stats counters differ with --stats-overlay (frame ${index}):\n"
            "  without: ${a}\n  with:    ${b}")
    endif()
endforeach()

list(LENGTH lines_with count_with)
if(NOT count_with EQUAL count)
    message(FATAL_ERROR "${count} --stats frame lines without the overlay, ${count_with} with it")
endif()

message("${count} --stats frame lines of Demo: the app's counters the same with --stats-overlay")
