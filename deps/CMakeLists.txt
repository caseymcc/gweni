# Dependencies come from vcpkg, not Hunter. With USE_HUNTER off, freetype-gl
# locates freetype/glew via CMake's standard find modules (satisfied by the
# vcpkg toolchain).
set(freetype-gl_USE_HUNTER OFF CACHE BOOL "" FORCE)
set(freetype-gl_BUILD_DEMOS OFF CACHE BOOL "Build the freetype-gl example programs" FORCE)
set(freetype-gl_BUILD_APIDOC OFF CACHE BOOL "Build the freetype-gl API documentation" FORCE)
set(freetype-gl_BUILD_TESTS OFF CACHE BOOL "Build the tests" FORCE)
# Nothing uses makefont
set(freetype-gl_BUILD_MAKEFONT OFF CACHE BOOL "Build the makefont tool" FORCE)
# freetype-gl is GL-free in every tree (a gweni patch, its opengl.h): no GL,
# GLEW or GLES headers or libraries, one vertex_buffer_t layout everywhere.

# SYSTEM: freetype-gl keeps its own flags, and its headers must not be able to
# raise a diagnostic inside a gweni translation unit. Only the slot core
# (SlotRenderer, under either renderer) uses it, so a tree without one leaves it
# out.
if(GWENI_RENDER_OPENGL_CORE OR GWENI_RENDER_VULKAN)
    add_subdirectory(freetype-gl SYSTEM)
endif()

set(IMGLIB_USE_COMPILETIME_TYPE OFF CACHE BOOL "Turn on/off support for compile time type informat" FORCE)
set(IMGLIB_USE_EIGEN OFF CACHE BOOL "Turn on/off support for eigen library" FORCE)
set(IMGLIB_USE_OPENCL OFF CACHE BOOL "Turn on/off support for OpenCl" FORCE)
set(IMGLIB_USE_OPENCL_CPP OFF CACHE BOOL "Turn on/off support for OpenCl CPP classes" FORCE)

add_subdirectory(imglib)

# imglib is first-party in practice, so it gets gweni's warning flags.
gweni_set_warnings(imglib)
