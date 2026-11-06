#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Two-process determinism check, run as a cmake -P script (tests/CMakeLists.txt
#  registers it as controltest_determinism):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -P determinism.cmake
#
#  Runs ControlTest --shot-all --dump-bounds twice, in two separate processes,
#  and requires every PNG and .txt listing of the first run to be byte-identical
#  to the second's. They must be separate processes: within one, the heap layout
#  is the same, so output that depends on pointer values (ASLR) would agree with
#  itself. That is how ListBoxDemo's over-read in text_buffer_add_text went
#  unseen (fixed in cca344a).
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "determinism.cmake: pass -D${var}=...")
    endif()
endforeach()

# Demos whose output is known to differ run to run, left out of the comparison
# until fixed. Name the issue next to each entry, e.g.
#   set(EXCLUDED_DEMOS
#       SomeDemo    # issue #NN: <what differs>
#   )
set(EXCLUDED_DEMOS)

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

foreach(run a b)
    execute_process(
        COMMAND "${CONTROLTEST}" --headless --shot-all --dump-bounds
                --shot-dir "${SCRATCH_DIR}/${run}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${SCRATCH_DIR}/${run}.log"
        ERROR_FILE "${SCRATCH_DIR}/${run}.log")

    if(NOT result EQUAL 0)
        file(READ "${SCRATCH_DIR}/${run}.log" log)
        message(FATAL_ERROR "run ${run}: ControlTest exited with '${result}'\n${log}")
    endif()
endforeach()

file(GLOB files_a RELATIVE "${SCRATCH_DIR}/a" "${SCRATCH_DIR}/a/*")
file(GLOB files_b RELATIVE "${SCRATCH_DIR}/b" "${SCRATCH_DIR}/b/*")
list(SORT files_a)
list(SORT files_b)

if(NOT files_a STREQUAL files_b)
    message(FATAL_ERROR "the two runs wrote different file sets:\n"
        "  a: ${files_a}\n  b: ${files_b}")
endif()

list(LENGTH files_a total)
if(total EQUAL 0)
    message(FATAL_ERROR "ControlTest wrote nothing to ${SCRATCH_DIR}/a")
endif()

set(compared 0)
set(differing)
foreach(file IN LISTS files_a)
    get_filename_component(demo "${file}" NAME_WE)
    if(demo IN_LIST EXCLUDED_DEMOS)
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${SCRATCH_DIR}/a/${file}" "${SCRATCH_DIR}/b/${file}"
        RESULT_VARIABLE result
        OUTPUT_QUIET ERROR_QUIET)
    math(EXPR compared "${compared} + 1")

    if(NOT result EQUAL 0)
        list(APPEND differing "${file}")
        message("DIFFERS: ${file}")
    endif()
endforeach()

if(differing)
    list(LENGTH differing count)
    message(FATAL_ERROR "${count} of ${compared} files differ between two runs "
        "(kept in ${SCRATCH_DIR}/a and ${SCRATCH_DIR}/b): ${differing}")
endif()

if(EXCLUDED_DEMOS)
    set(excluded ", excluded demos: ${EXCLUDED_DEMOS}")
endif()
message("${compared} files identical across two runs (${total} written per run${excluded})")
