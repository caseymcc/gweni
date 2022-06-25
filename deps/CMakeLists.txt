set(freetype-gl_USE_HUNTER ON CACHE BOOL "" FORCE)
set(freetype-gl_USE_VAO ON CACHE BOOL "Use a VAO to render a vertex_buffer instance (required for forward compatible OpenGL 3.0 contexts)" FORCE)
set(freetype-gl_BUILD_DEMOS OFF CACHE BOOL "Build the freetype-gl example programs" FORCE)
set(freetype-gl_BUILD_APIDOC OFF CACHE BOOL "Build the freetype-gl API documentation" FORCE)
set(freetype-gl_BUILD_TESTS OFF CACHE BOOL "Build the tests" FORCE)

add_subdirectory(freetype-gl)

set(IMGLIB_USE_COMPILETIME_TYPE OFF CACHE BOOL "Turn on/off support for compile time type informat" FORCE)
set(IMGLIB_USE_EIGEN OFF CACHE BOOL "Turn on/off support for eigen library" FORCE)
set(IMGLIB_USE_OPENCL OFF CACHE BOOL "Turn on/off support for OpenCl" FORCE)
set(IMGLIB_USE_OPENCL_CPP OFF CACHE BOOL "Turn on/off support for OpenCl CPP classes" FORCE)

add_subdirectory(imglib)
