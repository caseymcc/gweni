#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The resource mirror (cmake/mirrorDirectory.cmake), run as a cmake -P script
#  (tests/CMakeLists.txt registers it as mirror_directory):
#
#      cmake -DMIRROR=<cmake/mirrorDirectory.cmake> -DSCRATCH_DIR=<dir> \
#            -P mirrorDirectory.cmake
#
#  Mirrors a scratch source into a scratch destination that has an unchanged
#  file, a changed one, a stale one, one under the name it had before a rename,
#  a whole subtree and an empty directory the source does not have, and a file
#  where the source has a directory and the reverse. Passes only if afterwards
#  the destination lists exactly the source's entries with the source's
#  contents, the unchanged file was not rewritten (its mtime stays), a second
#  run removes and rewrites nothing, and a missing source fails.
#

foreach(var MIRROR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "mirrorDirectory.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
set(src "${SCRATCH_DIR}/src")
set(dst "${SCRATCH_DIR}/dst")

file(WRITE "${src}/keep.txt" "keep\n")
file(WRITE "${src}/changed.txt" "changed, new content\n")
file(WRITE "${src}/new.txt" "new\n")
file(WRITE "${src}/renamed_new.txt" "renamed\n")
file(WRITE "${src}/sub/nested.txt" "nested\n")
file(WRITE "${src}/.hidden" "dotfile\n")
file(WRITE "${src}/wasfile/inside.txt" "a directory where dst has a file\n")
file(WRITE "${src}/wasdir" "a file where dst has a directory\n")
file(MAKE_DIRECTORY "${src}/emptysrc")

file(WRITE "${dst}/keep.txt" "keep\n")
file(WRITE "${dst}/changed.txt" "changed, old content\n")
file(WRITE "${dst}/stale.txt" "stale\n")
file(WRITE "${dst}/renamed_old.txt" "renamed\n")
file(WRITE "${dst}/gone/deep/x.txt" "gone\n")
file(WRITE "${dst}/wasfile" "file\n")
file(WRITE "${dst}/wasdir/y.txt" "dir\n")
file(MAKE_DIRECTORY "${dst}/gone_empty")

set(errors "")

# runs the mirror into the variable named by out_var (its stdout and stderr)
function(run_mirror out_var)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -DSRC=${src} -DDST=${dst} -P "${MIRROR}"
        RESULT_VARIABLE result
        OUTPUT_VARIABLE out
        ERROR_VARIABLE out)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "the mirror exited with '${result}'\n${out}")
    endif()
    set(${out_var} "${out}" PARENT_SCOPE)
endfunction()

# the mtime of every file under dst, as "<path>=<seconds>" entries
function(dst_mtimes out_var)
    file(GLOB_RECURSE files RELATIVE "${dst}" LIST_DIRECTORIES false "${dst}/*")
    list(SORT files)
    set(stamps "")
    foreach(f IN LISTS files)
        file(TIMESTAMP "${dst}/${f}" stamp "%s" UTC)
        list(APPEND stamps "${f}=${stamp}")
    endforeach()
    set(${out_var} "${stamps}" PARENT_SCOPE)
endfunction()

file(TIMESTAMP "${dst}/keep.txt" keep_before "%s" UTC)
# file(TIMESTAMP) has one-second resolution, so a rewrite shows only after one
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.1)

run_mirror(first_out)

# every entry the source has, and only those
file(GLOB_RECURSE src_entries RELATIVE "${src}" LIST_DIRECTORIES true "${src}/*")
file(GLOB_RECURSE dst_entries RELATIVE "${dst}" LIST_DIRECTORIES true "${dst}/*")
list(SORT src_entries)
list(SORT dst_entries)
foreach(entry IN LISTS dst_entries)
    list(FIND src_entries "${entry}" found)
    if(found EQUAL -1)
        list(APPEND errors "not removed from dst: ${entry}")
    endif()
endforeach()
foreach(entry IN LISTS src_entries)
    list(FIND dst_entries "${entry}" found)
    if(found EQUAL -1)
        list(APPEND errors "missing from dst: ${entry}")
    elseif(IS_DIRECTORY "${src}/${entry}")
        if(NOT IS_DIRECTORY "${dst}/${entry}")
            list(APPEND errors "not a directory in dst: ${entry}")
        endif()
    elseif(IS_DIRECTORY "${dst}/${entry}")
        list(APPEND errors "a directory in dst: ${entry}")
    else()
        file(READ "${src}/${entry}" want)
        file(READ "${dst}/${entry}" got)
        if(NOT want STREQUAL got)
            list(APPEND errors "content differs: ${entry}")
        endif()
    endif()
endforeach()

file(TIMESTAMP "${dst}/keep.txt" keep_after "%s" UTC)
if(NOT keep_after STREQUAL keep_before)
    list(APPEND errors "unchanged file rewritten: keep.txt (mtime ${keep_before} -> ${keep_after})")
endif()

# one line per removed entry, the files of a removed subtree each named
foreach(entry stale.txt renamed_old.txt gone/deep/x.txt gone/deep gone gone_empty wasfile wasdir/y.txt wasdir)
    string(FIND "${first_out}" "Removed stale ${entry}\n" at)
    if(at EQUAL -1)
        list(APPEND errors "no 'Removed stale ${entry}' line")
    endif()
endforeach()

# a second run over a mirrored dst removes and rewrites nothing
dst_mtimes(stamps_before)
execute_process(COMMAND "${CMAKE_COMMAND}" -E sleep 1.1)
run_mirror(second_out)
dst_mtimes(stamps_after)
if(second_out MATCHES "Removed stale")
    list(APPEND errors "the second run removed something:\n${second_out}")
endif()
if(NOT stamps_after STREQUAL stamps_before)
    list(APPEND errors "the second run rewrote files: ${stamps_before} -> ${stamps_after}")
endif()

# a missing source fails, naming it, and leaves dst alone
execute_process(
    COMMAND "${CMAKE_COMMAND}" -DSRC=${SCRATCH_DIR}/no_such_src -DDST=${dst} -P "${MIRROR}"
    RESULT_VARIABLE result
    OUTPUT_VARIABLE out
    ERROR_VARIABLE out)
if(result EQUAL 0)
    list(APPEND errors "a missing source did not fail")
elseif(NOT out MATCHES "no_such_src")
    list(APPEND errors "a missing source failed without naming it:\n${out}")
endif()
if(NOT EXISTS "${dst}/keep.txt")
    list(APPEND errors "a missing source emptied dst")
endif()

if(errors)
    string(REPLACE ";" "\n  " report "${errors}")
    message(FATAL_ERROR "mirror_directory:\n  ${report}\nfirst run's output:\n${first_out}")
endif()
message(STATUS "mirror_directory: OK")
