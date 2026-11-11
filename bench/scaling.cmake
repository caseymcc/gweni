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
#  A scene listed in LARGE_N_FOR below runs at its own, smaller large N, with
#  the reason beside it.
#
#  N need not be a control count. In vlist-variable-scroll it is the items of
#  a variable-height VirtualList, of which only the rows on screen are
#  controls; the items live in its height index, whose O(log N) walks show in
#  no counter, so the check holds the scroll's measures, slots and layout to
#  the rows it brings into view (the thumb, which moves more at the small N,
#  is the only counter that differs). In code-edit and code-scroll it is the
#  lines of a CodeEditor's document, of which only the lines on screen are
#  controls: code-edit edits at line 500 (or N/2 below N=1000), so the strings
#  and line numbers on screen are the same at both sizes while every line past
#  it is in the document, and an edit that cost the lines below it would show;
#  code-scroll moves the view a line a frame.
#
#  Counters and what "grew" means:
#
#    counter                           checked  grew means
#    --------------------------------  -------  -------------------------------
#    visited, render_carried,          yes      any increase
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
#    layout_children_scanned           yes      any increase
#    invalidations,                    yes      any increase
#      invalidations_<phase> (input,
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
#    primitive_slots_drawn,            only in  the slots the primitive draws
#      primitive_ranges                PINNED   cover, and their ranges, follow
#                                      scenes   the shown slots, not the change,
#                                               so they grow with N wherever
#                                               the shown part does; a PINNED
#                                               scene's shown part is fixed,
#                                               and there "grew" means any
#                                               change (hidden-pages: N hidden
#                                               controls, 40 shown)
#    mirror_bytes, prim_ids, text_ids  no       sizes of the tree, not work
#    primitive_slots_culled            no       a size: the shown slots culled
#                                               now, which follow N where the
#                                               clipped part does
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
    # issue #659: an append grows the log's HeightIndex, whose leaf splits (a
    # few operator news every ~128 appends) land on frames set by N mod 128,
    # not by N's size: the per-frame counts differ on 3 or 4 frames in 20
    log-append:allocs_input
    log-append-detached:allocs_input
    log-append-capped:allocs_input
)

# Counters checked in the named scenes only, "<scene>:<counter>", each failing on
# any change from N=SMALL_N to N=LARGE_N (see the table above).
set(PINNED
    hidden-pages:primitive_slots_drawn
    hidden-pages:primitive_ranges
)

# A smaller LARGE_N for the named scenes, "<scene>:<n>", each with its reason; a
# value at or above LARGE_N changes nothing (an ASAN tree's 3000).
#   graph-drag, graph-marquee: a NodeGraph node of theirs (a title, two pins,
#   a Label body, its link) is 17 controls, about 18 z labels, so past about
#   3600 nodes the canvas's 0..65535 z range is full, the labels clamp and
#   tie, and the graph no longer draws in order (docs/architecture.md, "The
#   node graph"): 3000 is the largest round N it draws right at
set(LARGE_N_FOR
    graph-drag:3000
    graph-marquee:3000
)

# Keys that are not work counters (see the table above).
set(UNCHECKED scene n frame allocs alloc_bytes draw_calls primitive_slots_drawn primitive_ranges primitive_slots_culled text_draws mirror_bytes prim_ids text_ids)

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
    set(large_n ${LARGE_N})
    foreach(entry IN LISTS LARGE_N_FOR)
        string(REGEX MATCH "^([^:]+):([0-9]+)$" unused "${entry}")
        if(CMAKE_MATCH_1 STREQUAL scene AND CMAKE_MATCH_2 LESS large_n)
            set(large_n ${CMAKE_MATCH_2})
        endif()
    endforeach()

    run_scene(${scene} ${SMALL_N} small)
    run_scene(${scene} ${large_n} large)

    if(NOT small_frames STREQUAL large_frames)
        message(FATAL_ERROR "bench_scaling: ${scene}: frames ${small_frames} at N=${SMALL_N},"
            " ${large_frames} at N=${large_n}")
    endif()

    foreach(entry IN LISTS PINNED)
        string(REGEX MATCH "^([^:]+):(.+)$" unused "${entry}")
        if(CMAKE_MATCH_1 STREQUAL scene AND NOT CMAKE_MATCH_2 IN_LIST small_keys)
            message(FATAL_ERROR "bench_scaling: PINNED ${entry}: its frame lines have no ${CMAKE_MATCH_2}=")
        endif()
    endforeach()

    set(scene_failures 0)
    set(grew_keys "")
    foreach(key IN LISTS small_keys)
        set(pinned OFF)
        if("${scene}:${key}" IN_LIST PINNED)
            set(pinned ON)
        elseif(key IN_LIST UNCHECKED OR key MATCHES "_us$")
            continue()
        endif()
        foreach(frame IN LISTS small_frames)
            set(a "${small_${frame}_${key}}")
            set(b "${large_${frame}_${key}}")
            if(b STREQUAL "")
                message(FATAL_ERROR "bench_scaling: ${scene}: frame ${frame} has no ${key}= at N=${large_n}")
            endif()
            if(b GREATER a OR (pinned AND NOT b EQUAL a))
                if(NOT key IN_LIST grew_keys)
                    list(APPEND grew_keys ${key})
                endif()
                if(NOT "${scene}:${key}" IN_LIST EXCLUDED)
                    if(pinned)
                        set(verb CHANGED)
                    else()
                        set(verb GREW)
                    endif()
                    message("${verb}: ${scene} ${key} frame ${frame}: ${a} at N=${SMALL_N},"
                        " ${b} at N=${large_n}")
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

# a pinned counter of a scene that is not local or not listed
foreach(entry IN LISTS PINNED)
    string(REGEX MATCH "^([^:]+):(.+)$" unused "${entry}")
    if(NOT CMAKE_MATCH_1 IN_LIST scenes)
        message(FATAL_ERROR "bench_scaling: PINNED ${entry}: ${CMAKE_MATCH_1} is not a scaling=local scene")
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
        string(APPEND why "\n  grew (or, pinned, changed) from N=${SMALL_N} to N=${LARGE_N}"
            " (or the scene's LARGE_N_FOR): ${failure_list}")
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
string(REPLACE ";" ", " large_n_list "${LARGE_N_FOR}")
message("bench_scaling: ${scene_list}: no work counter grows from N=${SMALL_N} to N=${LARGE_N}"
    " (excluded: ${excluded_list}; their own large N: ${large_n_list})")
