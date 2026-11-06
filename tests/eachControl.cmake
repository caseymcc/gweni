#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Runs one ControlTest check for every control in ControlTest --list, one
#  process per control, as a cmake -P script (tests/CMakeLists.txt registers it
#  as controltest_resize_each and controltest_one_pass_each):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -DMODE=resize|one-pass [-DSKIP=<names>] \
#            -P eachControl.cmake
#
#  MODE resize:   --scenario resize --require-one-pass, the 640->1024->640
#                 round trip, which has to come back byte for byte, every frame
#                 in one layout pass.
#  MODE one-pass: --stats --require-one-pass, the plain run (the default
#                 frames), every frame in one layout pass.
#
#  Every control is run, and the failing ones are named together at the end,
#  each with the lines ControlTest printed on stderr. SKIP leaves controls out
#  that another test already covers (Demo, for controltest_resize).
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR MODE)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "eachControl.cmake: pass -D${var}=...")
    endif()
endforeach()

if(MODE STREQUAL "resize")
    set(mode_args --scenario resize --require-one-pass)
elseif(MODE STREQUAL "one-pass")
    set(mode_args --stats --require-one-pass)
else()
    message(FATAL_ERROR "eachControl.cmake: MODE is resize or one-pass, not '${MODE}'")
endif()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

execute_process(
    COMMAND "${CONTROLTEST}" --list
    RESULT_VARIABLE result
    OUTPUT_VARIABLE listing
    ERROR_VARIABLE listing)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "ControlTest --list exited with '${result}'\n${listing}")
endif()

# "  ButtonDemo" lines, and "  Demo (the whole docked demo)"
string(REGEX MATCHALL "\n  [A-Za-z0-9_]+" names "${listing}")
string(REGEX REPLACE "\n  " "" names "${names}")
list(REMOVE_ITEM names ${SKIP})
if(NOT names)
    message(FATAL_ERROR "no control names in ControlTest --list:\n${listing}")
endif()

set(failed)
foreach(name IN LISTS names)
    set(log "${SCRATCH_DIR}/${name}.log")
    execute_process(
        COMMAND "${CONTROLTEST}" --headless ${mode_args} --control "${name}"
                --shot-dir "${SCRATCH_DIR}/${name}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${log}"
        ERROR_FILE "${log}")

    if(NOT result EQUAL 0)
        list(APPEND failed "${name}")
        # the --stats frame lines are noise here; the ControlTest: lines say
        # which frame or check failed
        file(STRINGS "${log}" reasons REGEX "^(ControlTest:|resize )")
        string(REPLACE ";" "\n    " reasons "${reasons}")
        message("FAILED: ${name} (exit '${result}', log ${log})\n    ${reasons}")
    endif()
endforeach()

list(LENGTH names total)
if(failed)
    list(LENGTH failed count)
    list(JOIN failed ", " failed)
    message(FATAL_ERROR "${MODE}: ${count} of ${total} controls failed: ${failed}")
endif()
list(JOIN names ", " names)
message("${MODE}: all ${total} controls passed: ${names}")
