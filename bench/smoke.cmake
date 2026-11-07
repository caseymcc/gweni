#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  bench_smoke: runs every scene gweni_bench --list names at a small N for a
#  few frames, and fails, naming each scene, if one exits non-zero, crashes or
#  does not print its frame and summary lines. It checks that the bench runs,
#  not how fast.
#
#  cmake -DBENCH=<gweni_bench> -P smoke.cmake
#

if(NOT BENCH)
    message(FATAL_ERROR "bench_smoke: pass -DBENCH=<path to gweni_bench>")
endif()

set(SMOKE_N 50)
set(SMOKE_FRAMES 3)

execute_process(COMMAND ${BENCH} --list
    OUTPUT_VARIABLE list_output
    RESULT_VARIABLE list_result)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "bench_smoke: gweni_bench --list failed (${list_result})")
endif()

string(REGEX MATCHALL "(^|\n)[^ \n]+" scene_names "${list_output}")
set(scenes "")
foreach(name IN LISTS scene_names)
    string(STRIP "${name}" name)
    if(name)
        list(APPEND scenes "${name}")
    endif()
endforeach()

if(NOT scenes)
    message(FATAL_ERROR "bench_smoke: gweni_bench --list named no scenes")
endif()

set(failed "")
foreach(scene IN LISTS scenes)
    execute_process(COMMAND ${BENCH} --scene ${scene} --n ${SMOKE_N} --frames ${SMOKE_FRAMES}
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        RESULT_VARIABLE result)

    string(REGEX MATCHALL "(^|\n)scene=${scene} n=${SMOKE_N} frame=[0-9]+ " frame_lines "${output}")
    list(LENGTH frame_lines frame_count)

    if(NOT result EQUAL 0)
        message(STATUS "bench_smoke: ${scene}: exit ${result}\n${errors}")
        list(APPEND failed ${scene})
    elseif(NOT frame_count EQUAL SMOKE_FRAMES OR NOT output MATCHES "(^|\n)summary scene=${scene} ")
        message(STATUS "bench_smoke: ${scene}: ${frame_count} of ${SMOKE_FRAMES} frame lines,"
            " or no summary line\n${output}${errors}")
        list(APPEND failed ${scene})
    else()
        message(STATUS "bench_smoke: ${scene}: ok")
    endif()
endforeach()

if(failed)
    message(FATAL_ERROR "bench_smoke: failed: ${failed}")
endif()
