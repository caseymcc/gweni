#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Clip-cull check, run as a cmake -P script (tests/CMakeLists.txt registers
#  it as controltest_clip_cull):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> [-DOPENGLCORE=<OpenGLCore>] -P clipCulling.cmake
#
#  SlotRenderer leaves out of the draws a primitive quad or a text run drawn
#  wholly outside its clip box (SlotRenderer::setClipCulling), which the
#  fragment shaders' clip test would have discarded pixel by pixel. This runs
#  ControlTest --shot-all (and, given OPENGLCORE, the hero demo's --screenshot)
#  with the cull on (the default) and off (GWENI_CLIP_CULL=0) and requires every
#  PNG to be byte-identical: a cull that dropped a pixel the shader keeps
#  changes pixels. It also requires the cull to have taken effect, as
#  primitive_slots_culled and text_runs_culled above 0 in Demo's ControlTest
#  --stats line with it on and 0 with it off.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "clipCulling.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

# run <name> <on|off> <command...>: the output goes to <name>.log, and a non-zero
# exit fails the test
function(run_capture name cull)
    if(cull STREQUAL "on")
        set(env -E env --unset=GWENI_CLIP_CULL)
    else()
        set(env -E env GWENI_CLIP_CULL=0)
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

foreach(run on off)
    run_capture(${run} ${run} "${CONTROLTEST}" --headless --shot-all
        --shot-dir "${SCRATCH_DIR}/${run}")

    if(DEFINED OPENGLCORE AND NOT OPENGLCORE STREQUAL "")
        run_capture(${run}_hero ${run} "${OPENGLCORE}" --headless
            --screenshot "${SCRATCH_DIR}/${run}/OpenGLCore.png")
    endif()

    run_capture(${run}_stats ${run} "${CONTROLTEST}" --headless --control Demo
        --stats --frames 1)
    file(READ "${SCRATCH_DIR}/${run}_stats.log" log)
    foreach(counter primitive_slots_culled text_runs_culled)
        if(NOT log MATCHES "${counter}=([0-9]+)")
            message(FATAL_ERROR "${run}: no ${counter}= in ControlTest --stats\n${log}")
        endif()
        set(${counter}_${run} ${CMAKE_MATCH_1})
    endforeach()
endforeach()

foreach(counter primitive_slots_culled text_runs_culled)
    if(NOT ${counter}_on GREATER 0)
        message(FATAL_ERROR "the cull culled nothing: Demo's ${counter} is "
            "${${counter}_on} with it on")
    endif()
    if(NOT ${counter}_off EQUAL 0)
        message(FATAL_ERROR "GWENI_CLIP_CULL=0 did not turn the cull off: Demo's "
            "${counter} is ${${counter}_off}")
    endif()
endforeach()

file(GLOB files_a RELATIVE "${SCRATCH_DIR}/on" "${SCRATCH_DIR}/on/*")
file(GLOB files_b RELATIVE "${SCRATCH_DIR}/off" "${SCRATCH_DIR}/off/*")
list(SORT files_a)
list(SORT files_b)

if(NOT files_a STREQUAL files_b)
    message(FATAL_ERROR "the two runs wrote different file sets:\n"
        "  on: ${files_a}\n  off: ${files_b}")
endif()

list(LENGTH files_a total)
if(total EQUAL 0)
    message(FATAL_ERROR "nothing was written to ${SCRATCH_DIR}/on")
endif()

set(differing)
foreach(file IN LISTS files_a)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E compare_files
                "${SCRATCH_DIR}/on/${file}" "${SCRATCH_DIR}/off/${file}"
        RESULT_VARIABLE result
        OUTPUT_QUIET ERROR_QUIET)

    if(NOT result EQUAL 0)
        list(APPEND differing "${file}")
        message("DIFFERS: ${file}")
    endif()
endforeach()

if(differing)
    list(LENGTH differing count)
    message(FATAL_ERROR "${count} of ${total} PNGs differ between the clip cull on and "
        "GWENI_CLIP_CULL=0 (kept in ${SCRATCH_DIR}/on and ${SCRATCH_DIR}/off): ${differing}")
endif()

message("${total} PNGs identical with the clip cull on and off (Demo: "
    "${primitive_slots_culled_on} primitive slots and ${text_runs_culled_on} text runs culled)")
