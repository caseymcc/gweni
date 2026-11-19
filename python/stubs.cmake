#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The module's stub, python/gweni/_gweni.pyi, generated from the built module
#  by the interpreter's own nanobind stubgen (the image's pinned nanobind), on
#  the staged package so it describes what is shipped:
#
#    cmake -DPYTHON=<interpreter> -DSTAGE=<tree>/python -DOUT=<file>
#          [-DCHECK=<checked-in stub>] -P stubs.cmake
#
#  Without CHECK it writes OUT (the gweni_python_stubs target writes the
#  checked-in one). With CHECK (python_stubs) it writes OUT and fails, with a
#  diff and the command that regenerates it, unless OUT is CHECK byte for byte.
#
cmake_minimum_required(VERSION 3.16)

execute_process(
    COMMAND ${CMAKE_COMMAND} -E env PYTHONDONTWRITEBYTECODE=1
        ${PYTHON} -m nanobind.stubgen -q -i ${STAGE} -m gweni._gweni -o ${OUT}
    RESULT_VARIABLE result)
if(NOT result EQUAL 0)
    message(FATAL_ERROR "stubs: nanobind.stubgen on ${STAGE}/gweni failed (${result})")
endif()

if(DEFINED CHECK)
    execute_process(COMMAND ${CMAKE_COMMAND} -E compare_files ${OUT} ${CHECK} RESULT_VARIABLE same)
    if(NOT same EQUAL 0)
        execute_process(COMMAND diff -u ${CHECK} ${OUT})
        message(FATAL_ERROR "python_stubs: ${CHECK} is not the stub of the built module (diff above); "
            "regenerate it with: cmake --build <tree> --target gweni_python_stubs")
    endif()
    message("python_stubs: ${CHECK} is the built module's stub")
endif()
