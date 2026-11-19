#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  A Python test in a tree that could not build the module
#  (python/CMakeLists.txt): one line saying why, and a failure.
#
if(CMAKE_VERSION VERSION_LESS 3.29)
    message(FATAL_ERROR "${MESSAGE}")
endif()
message("${MESSAGE}")
cmake_language(EXIT 1)
