#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Index-chunk check, run as a cmake -P script (tests/CMakeLists.txt registers
#  it as controltest_index_chunks):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> [-DOPENGLCORE=<OpenGLCore>] [-DCHUNK_SLOTS=<n>] \
#            -P indexChunks.cmake
#
#  OpenGLCore draws its slots in chunks of at most kSlotsPerChunk, each over the
#  one static 16-bit quad index buffer with its vertex attributes pointed at the
#  chunk's first slot. GWENI_INDEX_CHUNK_SLOTS=<n> (CHUNK_SLOTS, default 64) makes
#  the chunks small enough that every demo spans many of them. This runs
#  ControlTest --shot-all (and, given OPENGLCORE, the hero demo's --screenshot)
#  with the default chunk size and with the small one, and requires every PNG to
#  be byte-identical: a slot drawn from the wrong vertexes, or clipped by another
#  slot's clip box, changes pixels. It also requires the small chunks to have
#  taken effect, as more draw calls in Demo's ControlTest --stats line.
#
#  StatsOverlayDemo.png is left out of the comparison: its stats overlay shows
#  the frame's draw calls, which the small chunks raise by design.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "indexChunks.cmake: pass -D${var}=...")
    endif()
endforeach()

if(NOT DEFINED CHUNK_SLOTS OR CHUNK_SLOTS STREQUAL "")
    set(CHUNK_SLOTS 64)
endif()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# run <name> <chunk slots, empty for the default> <command...>: the output goes to
# <name>.log, and a non-zero exit fails the test
function(run_capture name slots)
    if(slots STREQUAL "")
        set(env -E env --unset=GWENI_INDEX_CHUNK_SLOTS)
    else()
        set(env -E env GWENI_INDEX_CHUNK_SLOTS=${slots})
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" ${env} ${ARGN}
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${SCRATCH_DIR}/${name}.log"
        ERROR_FILE "${SCRATCH_DIR}/${name}.log")

    if(NOT result EQUAL 0)
        file(READ "${SCRATCH_DIR}/${name}.log" log)
        message(FATAL_ERROR "${name}: exited with '${result}'\n${log}")
    endif()
endfunction()

foreach(run default chunked)
    if(run STREQUAL "default")
        set(slots "")
    else()
        set(slots ${CHUNK_SLOTS})
    endif()

    run_capture(${run} "${slots}" "${CONTROLTEST}" --headless --shot-all
        --shot-dir "${SCRATCH_DIR}/${run}")

    if(DEFINED OPENGLCORE AND NOT OPENGLCORE STREQUAL "")
        run_capture(${run}_hero "${slots}" "${OPENGLCORE}" --headless
            --screenshot "${SCRATCH_DIR}/${run}/OpenGLCore.png")
    endif()

    run_capture(${run}_stats "${slots}" "${CONTROLTEST}" --headless --control Demo
        --stats --frames 1)
    file(READ "${SCRATCH_DIR}/${run}_stats.log" log)
    if(NOT log MATCHES "draw_calls=([0-9]+)")
        message(FATAL_ERROR "${run}: no draw_calls= in ControlTest --stats\n${log}")
    endif()
    set(draw_calls_${run} ${CMAKE_MATCH_1})
endforeach()

if(NOT draw_calls_chunked GREATER draw_calls_default)
    message(FATAL_ERROR "GWENI_INDEX_CHUNK_SLOTS=${CHUNK_SLOTS} did not split the draws: "
        "Demo made ${draw_calls_chunked} draw calls, ${draw_calls_default} by default")
endif()

file(GLOB files_a RELATIVE "${SCRATCH_DIR}/default" "${SCRATCH_DIR}/default/*")
file(GLOB files_b RELATIVE "${SCRATCH_DIR}/chunked" "${SCRATCH_DIR}/chunked/*")
list(SORT files_a)
list(SORT files_b)

if(NOT files_a STREQUAL files_b)
    message(FATAL_ERROR "the two runs wrote different file sets:\n"
        "  default: ${files_a}\n  chunked: ${files_b}")
endif()

list(LENGTH files_a total)
if(total EQUAL 0)
    message(FATAL_ERROR "nothing was written to ${SCRATCH_DIR}/default")
endif()

# the PNGs that show the draw count (see the header)
set(EXCLUDED StatsOverlayDemo.png)

set(differing)
foreach(file IN LISTS files_a)
    if(file IN_LIST EXCLUDED)
        continue()
    endif()

    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${SCRATCH_DIR}/default/${file}" "${SCRATCH_DIR}/chunked/${file}"
        RESULT_VARIABLE result
        OUTPUT_QUIET ERROR_QUIET)

    if(NOT result EQUAL 0)
        list(APPEND differing "${file}")
        message("DIFFERS: ${file}")
    endif()
endforeach()

if(differing)
    list(LENGTH differing count)
    message(FATAL_ERROR "${count} of ${total} PNGs differ between the default chunk size "
        "and GWENI_INDEX_CHUNK_SLOTS=${CHUNK_SLOTS} (kept in ${SCRATCH_DIR}/default and "
        "${SCRATCH_DIR}/chunked): ${differing}")
endif()

message("${total} PNGs identical at the default chunk size and at ${CHUNK_SLOTS} slots "
    "(Demo: ${draw_calls_default} draw calls by default, ${draw_calls_chunked} chunked)")
