#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Mirrors one directory into another, run as a cmake -P script:
#
#      cmake -DSRC=<source dir> -DDST=<destination dir> -P mirrorDirectory.cmake
#
#  Afterwards DST holds exactly what SRC holds: every file and directory of SRC,
#  and nothing else. A DST entry SRC does not have (or has as the other kind, a
#  file for a directory or the reverse) is removed, with one "Removed stale
#  <path>" line each; a file is copied only if its content differs, so an
#  unchanged one keeps its mtime. The examples' build runs it on every build to
#  keep the resources/ next to them in step with the repo's
#  (examples/openglcore/CMakeLists.txt), which copy_directory_if_different
#  alone does not: it never deletes. tests/mirrorDirectory.cmake checks it.
#
#  Plain file() and cmake -E copy_if_different, so it runs on the project's
#  minimum CMake (3.12).
#

foreach(var SRC DST)
    if(NOT DEFINED ${var} OR "${${var}}" STREQUAL "")
        message(FATAL_ERROR "mirrorDirectory.cmake: pass -D${var}=<dir>")
    endif()
endforeach()
if(NOT IS_DIRECTORY "${SRC}")
    message(FATAL_ERROR "mirrorDirectory.cmake: source is not a directory: ${SRC}")
endif()
if(EXISTS "${DST}" AND NOT IS_DIRECTORY "${DST}")
    message(FATAL_ERROR "mirrorDirectory.cmake: destination is not a directory: ${DST}")
endif()
file(MAKE_DIRECTORY "${DST}")

# dotfiles included: CMake's * matches a leading '.'
file(GLOB_RECURSE src_entries RELATIVE "${SRC}" LIST_DIRECTORIES true FOLLOW_SYMLINKS "${SRC}/*")
file(GLOB_RECURSE dst_entries RELATIVE "${DST}" LIST_DIRECTORIES true "${DST}/*")

set(src_dirs "")
set(src_files "")
foreach(entry IN LISTS src_entries)
    if(IS_DIRECTORY "${SRC}/${entry}")
        list(APPEND src_dirs "${entry}")
    else()
        list(APPEND src_files "${entry}")
    endif()
endforeach()

# Reverse order visits every entry before its parent directory, so a directory
# SRC lacks is empty by the time it is reached and each file in it is reported.
list(SORT dst_entries)
list(REVERSE dst_entries)
foreach(entry IN LISTS dst_entries)
    set(path "${DST}/${entry}")
    # a link is never what the copy below makes, so it never matches
    if(IS_SYMLINK "${path}")
        set(keep FALSE)
    elseif(IS_DIRECTORY "${path}")
        list(FIND src_dirs "${entry}" found)
        if(found EQUAL -1)
            set(keep FALSE)
        else()
            set(keep TRUE)
        endif()
    else()
        list(FIND src_files "${entry}" found)
        if(found EQUAL -1)
            set(keep FALSE)
        else()
            set(keep TRUE)
        endif()
    endif()
    if(NOT keep)
        if(IS_DIRECTORY "${path}" AND NOT IS_SYMLINK "${path}")
            file(REMOVE_RECURSE "${path}")
        else()
            file(REMOVE "${path}")
        endif()
        message(STATUS "Removed stale ${entry}")
    endif()
endforeach()

foreach(dir IN LISTS src_dirs)
    file(MAKE_DIRECTORY "${DST}/${dir}")
endforeach()
foreach(entry IN LISTS src_files)
    execute_process(
        COMMAND "${CMAKE_COMMAND}" -E copy_if_different "${SRC}/${entry}" "${DST}/${entry}"
        RESULT_VARIABLE result)
    if(NOT result EQUAL 0)
        message(FATAL_ERROR "mirrorDirectory.cmake: could not copy ${SRC}/${entry} to ${DST}/${entry}")
    endif()
endforeach()
