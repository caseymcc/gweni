#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The Vulkan-only tree (./generate.sh linux vulkan) holds nothing GL, two ways:
#
#   cmake -DMODE=elf -DEXE=<exe> -DREADELF=<readelf> -DNM=<nm> -P noGl.cmake
#     fails, naming each one, if the exe (vulkan_frame, which links gweni and
#     nothing else) has a GL-family library in its NEEDED entries (readelf -d:
#     libGL*, libOpenGL, libEGL, libGLEW, libGLX, libGLES*) or an undefined
#     gl*, egl*, glew* or __glew* symbol (nm -u), so nothing in gweni calls GL
#     even through a library that is not GL itself.
#
#   cmake -DMODE=cache -DCACHE=<tree>/CMakeCache.txt -P noGl.cmake
#     fails, naming each one, if the tree's CMakeCache.txt has an entry a GL,
#     GLEW or EGL find leaves (OPENGL_*, OpenGL_*, GLEW_*, *EGL*), so no
#     find_package of them ran in the configure.

function(fail message)
    message(FATAL_ERROR "noGl.cmake (${MODE}): ${message}")
endfunction()

if(MODE STREQUAL "elf")
    foreach(var EXE READELF NM)
        if(NOT ${var})
            fail("${var} not given")
        endif()
    endforeach()
    if(NOT EXISTS "${EXE}")
        fail("no ${EXE}")
    endif()

    execute_process(COMMAND ${READELF} -d ${EXE}
        OUTPUT_VARIABLE dynamic RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        fail("${READELF} -d ${EXE} failed (${status})")
    endif()
    set(found "")
    string(REGEX MATCHALL "\\(NEEDED\\)[^\n]*\\[[^]\n]*\\]" needed "${dynamic}")
    if(NOT needed)
        fail("${READELF} -d ${EXE} lists no NEEDED entry: not a dynamic ELF?")
    endif()
    foreach(entry IN LISTS needed)
        string(REGEX REPLACE ".*\\[([^]]*)\\]" "\\1" library "${entry}")
        if(library MATCHES "^lib(GL|OpenGL|EGL|GLEW|GLX|GLES)")
            list(APPEND found "NEEDED ${library}")
        endif()
    endforeach()

    execute_process(COMMAND ${NM} -u ${EXE}
        OUTPUT_VARIABLE undefined RESULT_VARIABLE status)
    if(NOT status EQUAL 0)
        fail("${NM} -u ${EXE} failed (${status})")
    endif()
    string(REPLACE "\n" ";" lines "${undefined}")
    foreach(line IN LISTS lines)
        # U: undefined, w/v: weak undefined
        string(REGEX REPLACE "^ *[Uwv] +" "" symbol "${line}")
        string(REGEX REPLACE "@.*" "" symbol "${symbol}")
        if(symbol MATCHES "^(gl[A-Z]|egl[A-Z]|glew[A-Z]|__glew)")
            list(APPEND found "undefined ${symbol}")
        endif()
    endforeach()

    if(found)
        list(JOIN found "\n  " list)
        fail("${EXE} uses GL:\n  ${list}")
    endif()
    list(LENGTH needed count)
    message(STATUS "noGl.cmake (elf): ${EXE}: ${count} NEEDED entries, none GL; no undefined GL symbol")
elseif(MODE STREQUAL "cache")
    if(NOT EXISTS "${CACHE}")
        fail("no ${CACHE}")
    endif()
    file(STRINGS "${CACHE}" entries REGEX "^[A-Za-z0-9_.+-]+:")
    set(found "")
    foreach(entry IN LISTS entries)
        string(REGEX REPLACE ":.*" "" name "${entry}")
        if(name MATCHES "^(OPENGL_|OpenGL_|GLEW_)" OR name MATCHES "EGL")
            list(APPEND found "${name}")
        endif()
    endforeach()
    if(found)
        list(JOIN found "\n  " list)
        fail("${CACHE} has GL entries, so a GL, GLEW or EGL find ran:\n  ${list}")
    endif()
    list(LENGTH entries count)
    message(STATUS "noGl.cmake (cache): ${CACHE}: ${count} entries, none GL")
else()
    fail("MODE must be elf or cache")
endif()
