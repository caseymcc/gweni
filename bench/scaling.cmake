#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  bench_scaling: the one CTest check on gweni_bench's numbers. Every scene
#  gweni_bench --list classifies scaling=local changes a fixed part of the
#  tree each frame, so the work it does per frame must not depend on how big
#  the tree is: the project's goal, that per-frame cost scales with what
#  changed, not UI size. This runs each local scene at SMALL_N and LARGE_N
#  (its default warm-up and frames) and fails if, on any measured frame, a
#  work counter is larger at LARGE_N than on the same frame at SMALL_N. It
#  reads only the exact counters, never a time, so it gives the same answer on
#  any host under any load.
#
#  cmake -DBENCH=<gweni_bench> [-DSMALL_N=1000] [-DLARGE_N=10000] -P scaling.cmake
#
#  Counters and what "grew" means:
#
#    counter                           checked  grew means
#    --------------------------------  -------  -------------------------------
#    visited, children_scanned,        yes      any increase
#      controls_changed, created,
#      visibility, geometry,
#      background, text
#    prim_slots_written,               yes      any increase
#      text_slots_written
#    upload_calls, upload_bytes        yes      any increase
#    buffer_regrows                    yes      any increase
#    atlas_upload_calls,               yes      any increase
#      atlas_upload_bytes
#    layout_passes, layout_capped,     yes      any increase
#      render_layout
#    layout_visited, measure_calls,    yes      any increase
#      measure_calls_<phase> (input,
#      update, render)
#    z_visited, z_changed              yes      any increase
#    anims_run                         yes      any increase
#    hit_tests                         yes      any increase
#    allocs_<phase> (input, update,    yes      any increase
#      render, flush, gpu)
#    allocs                            no       the sum of allocs_<phase>,
#                                               which are checked one by one
#    alloc_bytes                       no       a size: a control's strings
#                                               hold its index, which has more
#                                               digits at a larger N
#    draw_calls, text_draws            no       by design every frame draws
#                                               every slot, one draw per index
#                                               chunk of 16383 slots (per font
#                                               atlas for text), so they follow
#                                               the slot count, not the change
#    mirror_bytes, prim_ids, text_ids  no       sizes of the tree, not work
#    *_us                              no       times
#
#  Any other key a frame line carries is checked as "any increase", so a new
#  bench counter is covered until it is added to the table above.
#
#  A decrease is not a failure. A scene/counter pair known to grow is listed in
#  EXCLUDED below with the issue that tracks it; an entry whose counter no
#  longer grows fails the test too, so the fix removes it.
#

if(NOT BENCH)
    message(FATAL_ERROR "bench_scaling: pass -DBENCH=<path to gweni_bench>")
endif()
if(NOT SMALL_N)
    set(SMALL_N 1000)
endif()
if(NOT LARGE_N)
    set(LARGE_N 10000)
endif()

# Known growth, "<scene>:<counter>", each with the open issue it waits for, e.g.
#   set(EXCLUDED
#       some-scene:visited     # issue #NN: <what grows>
#   )
set(EXCLUDED
    # #407: the render walk passes over every child of each control it enters,
    # dirty or not (no allocation since #320, but O(siblings) in time)
    hover-sweep:children_scanned
    set-text:children_scanned
    type:children_scanned
    drag:children_scanned
    animate:children_scanned
    add-remove:children_scanned
)

# Keys that are not work counters (see the table above).
set(UNCHECKED scene n frame allocs alloc_bytes draw_calls text_draws mirror_bytes prim_ids text_ids)

execute_process(COMMAND ${BENCH} --list
    OUTPUT_VARIABLE list_output
    RESULT_VARIABLE list_result)
if(NOT list_result EQUAL 0)
    message(FATAL_ERROR "bench_scaling: gweni_bench --list failed (${list_result})")
endif()

string(REPLACE "\n" ";" list_lines "${list_output}")
set(scenes "")
foreach(line IN LISTS list_lines)
    if(line MATCHES "^([^ ]+) .* scaling=local( |$)")
        list(APPEND scenes "${CMAKE_MATCH_1}")
    endif()
endforeach()

if(NOT scenes)
    message(FATAL_ERROR "bench_scaling: gweni_bench --list named no scaling=local scene")
endif()

# Run one scene at one N; set <prefix>_frames to its frame indices and
# <prefix>_<frame>_<key> to each value of each frame line.
function(run_scene scene n prefix)
    execute_process(COMMAND ${BENCH} --scene ${scene} --n ${n}
        OUTPUT_VARIABLE output
        ERROR_VARIABLE errors
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "bench_scaling: ${scene} --n ${n}: exit ${result}\n${errors}")
    endif()
    if(NOT output MATCHES "(^|\n)summary scene=${scene} [^\n]* scaling=local( |\n|$)")
        message(FATAL_ERROR "bench_scaling: ${scene} --n ${n}: no summary line with"
            " scaling=local\n${output}${errors}")
    endif()

    string(REGEX MATCHALL "(^|\n)scene=${scene} n=${n} frame=[^\n]*" lines "${output}")
    set(frames "")
    foreach(line IN LISTS lines)
        string(REGEX MATCH "frame=([0-9]+)" unused "${line}")
        set(frame ${CMAKE_MATCH_1})
        list(APPEND frames ${frame})
        string(REGEX MATCHALL "[a-z0-9_]+=[^ \n]+" pairs "${line}")
        foreach(pair IN LISTS pairs)
            string(REGEX MATCH "^([a-z0-9_]+)=(.*)$" unused "${pair}")
            set(${prefix}_${frame}_${CMAKE_MATCH_1} "${CMAKE_MATCH_2}" PARENT_SCOPE)
            if(frame EQUAL 0)
                list(APPEND keys ${CMAKE_MATCH_1})
            endif()
        endforeach()
    endforeach()
    if(NOT frames)
        message(FATAL_ERROR "bench_scaling: ${scene} --n ${n}: no frame lines\n${output}")
    endif()
    set(${prefix}_frames ${frames} PARENT_SCOPE)
    set(${prefix}_keys ${keys} PARENT_SCOPE)
endfunction()

set(failures "")
set(stale "")
foreach(scene IN LISTS scenes)
    run_scene(${scene} ${SMALL_N} small)
    run_scene(${scene} ${LARGE_N} large)

    if(NOT small_frames STREQUAL large_frames)
        message(FATAL_ERROR "bench_scaling: ${scene}: frames ${small_frames} at N=${SMALL_N},"
            " ${large_frames} at N=${LARGE_N}")
    endif()

    set(scene_failures 0)
    set(grew_keys "")
    foreach(key IN LISTS small_keys)
        if(key IN_LIST UNCHECKED OR key MATCHES "_us$")
            continue()
        endif()
        foreach(frame IN LISTS small_frames)
            set(a "${small_${frame}_${key}}")
            set(b "${large_${frame}_${key}}")
            if(b STREQUAL "")
                message(FATAL_ERROR "bench_scaling: ${scene}: frame ${frame} has no ${key}= at N=${LARGE_N}")
            endif()
            if(b GREATER a)
                if(NOT key IN_LIST grew_keys)
                    list(APPEND grew_keys ${key})
                endif()
                if(NOT "${scene}:${key}" IN_LIST EXCLUDED)
                    message("GREW: ${scene} ${key} frame ${frame}: ${a} at N=${SMALL_N},"
                        " ${b} at N=${LARGE_N}")
                    math(EXPR scene_failures "${scene_failures} + 1")
                    if(NOT "${scene}:${key}" IN_LIST failures)
                        list(APPEND failures "${scene}:${key}")
                    endif()
                endif()
            endif()
        endforeach()
    endforeach()

    foreach(entry IN LISTS EXCLUDED)
        string(REGEX MATCH "^([^:]+):(.+)$" unused "${entry}")
        if(CMAKE_MATCH_1 STREQUAL scene AND NOT CMAKE_MATCH_2 IN_LIST grew_keys)
            list(APPEND stale "${entry}")
        endif()
    endforeach()

    list(LENGTH small_frames frame_count)
    if(scene_failures EQUAL 0)
        message("ok: ${scene} (${frame_count} frames)")
    else()
        message("FAILED: ${scene} (${scene_failures} frame counters grew)")
    endif()
endforeach()

# an entry for a scene that is no longer local, or no longer listed, is stale too
foreach(entry IN LISTS EXCLUDED)
    string(REGEX MATCH "^([^:]+):" unused "${entry}")
    if(NOT CMAKE_MATCH_1 IN_LIST scenes AND NOT entry IN_LIST stale)
        list(APPEND stale "${entry}")
    endif()
endforeach()

if(failures OR stale)
    set(why "")
    if(failures)
        string(REPLACE ";" ", " failure_list "${failures}")
        string(APPEND why "\n  grew from N=${SMALL_N} to N=${LARGE_N}: ${failure_list}")
    endif()
    if(stale)
        string(REPLACE ";" ", " stale_list "${stale}")
        string(APPEND why "\n  in EXCLUDED but no longer growing (remove them from"
            " bench/scaling.cmake): ${stale_list}")
    endif()
    message(FATAL_ERROR "bench_scaling: failed${why}")
endif()

string(REPLACE ";" ", " scene_list "${scenes}")
string(REPLACE ";" ", " excluded_list "${EXCLUDED}")
message("bench_scaling: ${scene_list}: no work counter grows from N=${SMALL_N} to N=${LARGE_N}"
    " (excluded: ${excluded_list})")
