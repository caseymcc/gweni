#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  python_exports: the module's dynamic symbol table defines PyInit__gweni and
#  nothing else (python/exports.map, -Wl,--exclude-libs,ALL), so its static
#  freetype, libpng, libjpeg-turbo, GLFW and GLEW never meet another
#  extension's copies in one process. Fails naming every other defined symbol.
#
#  -DMODULE=<_gweni.abi3.so>
#
cmake_minimum_required(VERSION 3.16)

execute_process(COMMAND nm -D --defined-only "${MODULE}"
    OUTPUT_VARIABLE listing
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "python_exports: nm -D --defined-only ${MODULE} failed")
endif()

string(REPLACE "\n" ";" lines "${listing}")
set(others "")
set(found OFF)
foreach(line IN LISTS lines)
    if(line STREQUAL "")
        continue()
    endif()
    string(REGEX REPLACE "^[0-9a-fA-F]* *[A-Za-z] " "" symbol "${line}")
    if(symbol STREQUAL "PyInit__gweni")
        set(found ON)
    else()
        list(APPEND others "${symbol}")
    endif()
endforeach()

if(NOT found)
    message(FATAL_ERROR "python_exports: ${MODULE} does not export PyInit__gweni")
endif()
if(others)
    list(LENGTH others count)
    string(REPLACE ";" "\n  " named "${others}")
    message(FATAL_ERROR "python_exports: ${MODULE} exports ${count} symbols besides PyInit__gweni:\n  ${named}")
endif()
message("python_exports: PyInit__gweni alone")
