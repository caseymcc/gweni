# Freetype GL - A C OpenGL Freetype engine
#
# Distributed under the OSI-approved BSD 2-Clause License.  See accompanying
# file `LICENSE` for more details.

cmake_minimum_required(VERSION 3.10)

cmake_policy(SET CMP0074 NEW)

option(freetype-gl_USE_HUNTER OFF)
if(freetype-gl_USE_HUNTER)
    include(${CMAKE_CURRENT_LIST_DIR}/cmake/HunterGate.cmake)
    HunterGate(
        URL "https://github.com/cpp-pm/hunter/archive/v0.23.259.tar.gz"
        SHA1 "59541baf106b91ae4fec32e6c6b0990d04c7f6be"
    )
endif()

project(freetype-gl VERSION 20.21.6 LANGUAGES C CXX)

set(CMAKE_MODULE_PATH
    "${CMAKE_CURRENT_SOURCE_DIR}/cmake/Modules"
    "${CMAKE_MODULE_PATH}"
)

message(STATUS "Building for ${CMAKE_SYSTEM_NAME} target system")
message(STATUS "Building with ${CMAKE_C_COMPILER_ID} compiler")

# gweni patch: GL-free in every tree (opengl.h). The vertex buffers are CPU
# accumulators, so there is no GL, GLEW, GLAD or GLES to find, link or choose,
# and no VAO switch changing vertex_buffer_t's layout.
option(freetype-gl_BUILD_DEMOS "Build the freetype-gl example programs" ON)
option(freetype-gl_BUILD_APIDOC "Build the freetype-gl API documentation" ON)
option(freetype-gl_BUILD_HARFBUZZ "Build the freetype-gl harfbuzz support (experimental)" OFF)
option(freetype-gl_BUILD_MAKEFONT "Build the makefont tool" ON)
option(freetype-gl_BUILD_TESTS "Build the tests" ON)

include(RequireIncludeFile)
include(RequireFunctionExists)
include(CheckLibraryExists)
include(CheckSymbolExists)

require_include_file(stdbool.h HAVE_STDBOOL_H)
require_include_file(stdint.h HAVE_STDINT_H)
require_include_file(math.h HAVE_MATH_H)

check_library_exists(m cos "" HAVE_MATH_LIBRARY)

if(HAVE_MATH_LIBRARY)
    list(APPEND CMAKE_REQUIRED_LIBRARIES m)
    set(MATH_LIBRARY m)
endif()

require_function_exists(cos HAVE_COS)
require_function_exists(fabs HAVE_FABS)
require_function_exists(floor HAVE_FLOOR)
require_function_exists(fmod HAVE_FMOD)
require_function_exists(pow HAVE_POW)
require_function_exists(roundf HAVE_ROUNDF)
require_function_exists(round HAVE_ROUND)
require_function_exists(sin HAVE_SIN)
require_function_exists(sqrt HAVE_SQRT)
require_function_exists(tan HAVE_TAN)

check_symbol_exists(M_PI math.h HAVE_M_PI)

if(NOT HAVE_M_PI)
    list(APPEND CMAKE_REQUIRED_DEFINITIONS -D_USE_MATH_DEFINES)
    unset(HAVE_M_PI CACHE)

    check_symbol_exists(M_PI math.h HAVE_M_PI)

    if(NOT HAVE_M_PI)
        message(FATAL_ERROR "`M_PI` not defined in `math.h`.")
    else()
        add_definitions(-D_USE_MATH_DEFINES)
    endif()
endif()

if(NOT MINGW AND (WIN32 OR WIN64) AND NOT freetype-gl_USE_HUNTER)
    set(ENV{FREETYPE_DIR}
        ${CMAKE_CURRENT_SOURCE_DIR}/windows/freetype)
endif()

if(freetype-gl_USE_HUNTER)
    hunter_add_package(freetype)
    find_package(freetype CONFIG REQUIRED)
    set(FREETYPE_TARGET_LIBS freetype::freetype)

else()
    # gweni patch: vcpkg's freetype config and its imported target, never
    # FindFreetype's FREETYPE_LIBRARIES (absolute paths, which gweni's exported
    # targets would carry into an installed package)
    find_package(freetype CONFIG REQUIRED)
    set(FREETYPE_TARGET_LIBS freetype)
endif()

include_directories(
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/include/freetype-gl
)

if(MSVC)
    # _CRT_NONSTDC_NO_DEPRECATE -> remove warning C4996
    add_definitions(-D_CRT_SECURE_NO_WARNINGS -D_CRT_NONSTDC_NO_DEPRECATE)
endif(MSVC)

set(FREETYPE_GL_HDR
    include/freetype-gl/distance-field.h
    include/freetype-gl/edtaa3func.h
    include/freetype-gl/font-manager.h
    include/freetype-gl/freetype-gl.h
    include/freetype-gl/markup.h
    include/freetype-gl/opengl.h
    include/freetype-gl/platform.h
    include/freetype-gl/text-buffer.h
    include/freetype-gl/texture-atlas.h
    include/freetype-gl/texture-font.h
    include/freetype-gl/utf8-utils.h
    include/freetype-gl/vec234.h
    include/freetype-gl/vector.h
    include/freetype-gl/vertex-attribute.h
    include/freetype-gl/vertex-buffer.h
)

set(FREETYPE_GL_SRC
    distance-field.c
    edtaa3func.c
    font-manager.c
    platform.c
    text-buffer.c
    texture-atlas.c
    texture-font.c
    utf8-utils.c
    vector.c
    vertex-attribute.c
    vertex-buffer.c
)

# gweni patch: an OBJECT library, built into libgweni.a (deps/CMakeLists.txt)
add_library(freetype-gl OBJECT
    ${FREETYPE_GL_SRC}
    ${FREETYPE_GL_HDR}
)

target_include_directories(freetype-gl PUBLIC
    $<BUILD_INTERFACE:${CMAKE_CURRENT_LIST_DIR}>/include
)

# Link freetype-gl's dependencies regardless of how they were located (Hunter or
# the standard find modules used with vcpkg) so the static library and its
# consumers (e.g. makefont) resolve the freetype symbols.
target_link_libraries(freetype-gl
    ${FREETYPE_TARGET_LIBS}
)

if(freetype-gl_BUILD_MAKEFONT)
    add_executable(makefont makefont.c)

    target_link_libraries(makefont
        freetype-gl
        ${MATH_LIBRARY}
    )

    if(MSVC AND NOT (MSVC_VERSION LESS 1900))
        # prevent error LNK2019: unresolved external symbol _sprintf referenced in function __bdf_parse_properties
        # see http://stackoverflow.com/a/32418900/469659
        target_link_libraries(makefont "legacy_stdio_definitions.lib")
    endif()
endif()


# gweni patch: no install rules. freetype-gl is an OBJECT library whose objects
# go into gweni's own archive (libgweni.a), and gweni installs its headers under
# include/gweni/third_party/, so nothing here is installed: vcpkg's freetype-gl
# port owns include/freetype-gl/, lib/libfreetype-gl.a and lib/cmake/freetype-gl/.

message(STATUS "freetype-gl_BUILD_APIDOC ${freetype-gl_BUILD_APIDOC}")
#additional projects
if(freetype-gl_BUILD_APIDOC)
    add_subdirectory(doc)
endif()

if(freetype-gl_BUILD_HARFBUZZ)
    add_subdirectory(harfbuzz)
endif()

if(freetype-gl_BUILD_DEMOS)
    add_subdirectory(demos)
endif()

if(freetype-gl_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif(freetype-gl_BUILD_TESTS)
