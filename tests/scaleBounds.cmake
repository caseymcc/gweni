#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Scaled-bounds check, run as a cmake -P script (tests/CMakeLists.txt registers
#  it as controltest_scale_bounds):
#
#      cmake -DCONTROLTEST=<ControlTest> -DSOURCE_DIR=<repo root> \
#            -DSCRATCH_DIR=<dir> -P scaleBounds.cmake
#
#  Runs ControlTest --shot-all --dump-bounds at --scale 1 and at --scale 2. The
#  layout is in logical units, so every .txt listing of the two runs has to be
#  byte-identical; the framebuffer is in device pixels, so every PNG of the
#  scale 2 run has to be exactly twice as wide and twice as high as its scale 1
#  twin.
#

foreach(var CONTROLTEST SOURCE_DIR SCRATCH_DIR)
    if(NOT DEFINED ${var})
        message(FATAL_ERROR "scaleBounds.cmake: pass -D${var}=...")
    endif()
endforeach()

# a stale run must not be able to pass
file(REMOVE_RECURSE "${SCRATCH_DIR}")
file(MAKE_DIRECTORY "${SCRATCH_DIR}")

foreach(scale 1 2)
    execute_process(
        COMMAND "${CONTROLTEST}" --headless --shot-all --dump-bounds --scale ${scale}
                --shot-dir "${SCRATCH_DIR}/scale${scale}"
        WORKING_DIRECTORY "${SOURCE_DIR}"
        RESULT_VARIABLE result
        OUTPUT_FILE "${SCRATCH_DIR}/scale${scale}.log"
        ERROR_FILE "${SCRATCH_DIR}/scale${scale}.log")

    if(NOT result EQUAL 0)
        file(READ "${SCRATCH_DIR}/scale${scale}.log" log)
        message(FATAL_ERROR "scale ${scale}: ControlTest exited with '${result}'\n${log}")
    endif()
endforeach()

file(GLOB files_1 RELATIVE "${SCRATCH_DIR}/scale1" "${SCRATCH_DIR}/scale1/*")
file(GLOB files_2 RELATIVE "${SCRATCH_DIR}/scale2" "${SCRATCH_DIR}/scale2/*")
list(SORT files_1)
list(SORT files_2)

if(NOT files_1 STREQUAL files_2)
    message(FATAL_ERROR "the two scales wrote different file sets:\n"
        "  scale 1: ${files_1}\n  scale 2: ${files_2}")
endif()

# A PNG's width and height, the first two fields of its IHDR chunk: big-endian
# 32-bit words at byte 16 and 20.
function(png_size path out_width out_height)
    file(READ "${path}" signature LIMIT 8 HEX)
    if(NOT signature STREQUAL "89504e470d0a1a0a")
        message(FATAL_ERROR "${path} is not a PNG")
    endif()

    file(READ "${path}" width_hex OFFSET 16 LIMIT 4 HEX)
    file(READ "${path}" height_hex OFFSET 20 LIMIT 4 HEX)
    math(EXPR width "0x${width_hex}")
    math(EXPR height "0x${height_hex}")
    set(${out_width} ${width} PARENT_SCOPE)
    set(${out_height} ${height} PARENT_SCOPE)
endfunction()

set(listings 0)
set(images 0)
set(failures)
foreach(file IN LISTS files_1)
    set(at1 "${SCRATCH_DIR}/scale1/${file}")
    set(at2 "${SCRATCH_DIR}/scale2/${file}")

    if(file MATCHES "\\.txt$")
        execute_process(
            COMMAND "${CMAKE_COMMAND}" -E compare_files "${at1}" "${at2}"
            RESULT_VARIABLE result
            OUTPUT_QUIET ERROR_QUIET)
        math(EXPR listings "${listings} + 1")

        if(NOT result EQUAL 0)
            list(APPEND failures "${file}")
            message("LISTING DIFFERS: ${file}")
        endif()
    elseif(file MATCHES "\\.png$")
        png_size("${at1}" w1 h1)
        png_size("${at2}" w2 h2)
        math(EXPR want_w "${w1} * 2")
        math(EXPR want_h "${h1} * 2")
        math(EXPR images "${images} + 1")

        if(NOT w2 EQUAL want_w OR NOT h2 EQUAL want_h)
            list(APPEND failures "${file}")
            message("SIZE WRONG: ${file} is ${w1}x${h1} at scale 1 and ${w2}x${h2} at scale 2,"
                " want ${want_w}x${want_h}")
        endif()
    endif()
endforeach()

if(listings EQUAL 0 OR images EQUAL 0)
    message(FATAL_ERROR "ControlTest wrote ${listings} listings and ${images} PNGs"
        " to ${SCRATCH_DIR}/scale1; want both")
endif()

if(failures)
    list(LENGTH failures count)
    message(FATAL_ERROR "${count} files fail (kept in ${SCRATCH_DIR}/scale1 and"
        " ${SCRATCH_DIR}/scale2): ${failures}")
endif()

message("${listings} listings identical and ${images} PNGs exactly 2x at scale 2")
