# Dependencies come from vcpkg, not Hunter. With USE_HUNTER off, freetype-gl
# locates freetype/glew via CMake's standard find modules (satisfied by the
# vcpkg toolchain).
set(freetype-gl_USE_HUNTER OFF CACHE BOOL "" FORCE)
set(freetype-gl_USE_VAO ON CACHE BOOL "Use a VAO to render a vertex_buffer instance (required for forward compatible OpenGL 3.0 contexts)" FORCE)
set(freetype-gl_BUILD_DEMOS OFF CACHE BOOL "Build the freetype-gl example programs" FORCE)
set(freetype-gl_BUILD_APIDOC OFF CACHE BOOL "Build the freetype-gl API documentation" FORCE)
set(freetype-gl_BUILD_TESTS OFF CACHE BOOL "Build the tests" FORCE)
# The GLES tree (GWENI_GL_ES): freetype-gl against <GLES3/gl3.h> and libGLESv2,
# without glew (freetype-gl_WITH_GLES3 is a gweni patch)
if(GWENI_GL_ES)
    set(freetype-gl_WITH_GLEW OFF CACHE BOOL "Use the GLEW library to fetch OpenGL function pointers" FORCE)
    set(freetype-gl_WITH_GLES3 ON CACHE BOOL "Build against GLES 3.0 (<GLES3/gl3.h>, libGLESv2)" FORCE)
else()
    set(freetype-gl_WITH_GLEW ON CACHE BOOL "Use the GLEW library to fetch OpenGL function pointers" FORCE)
    set(freetype-gl_WITH_GLES3 OFF CACHE BOOL "Build against GLES 3.0 (<GLES3/gl3.h>, libGLESv2)" FORCE)
endif()

# SYSTEM: freetype-gl keeps its own flags, and its headers must not be able to
# raise a diagnostic inside a gweni translation unit. Only OpenGLCore uses it,
# and it needs GL and glew to configure, so a tree without the renderer (the
# web tree, for now) leaves it out.
if(GWENI_RENDER_OPENGL_CORE)
    add_subdirectory(freetype-gl SYSTEM)
endif()

set(IMGLIB_USE_COMPILETIME_TYPE OFF CACHE BOOL "Turn on/off support for compile time type informat" FORCE)
set(IMGLIB_USE_EIGEN OFF CACHE BOOL "Turn on/off support for eigen library" FORCE)
set(IMGLIB_USE_OPENCL OFF CACHE BOOL "Turn on/off support for OpenCl" FORCE)
set(IMGLIB_USE_OPENCL_CPP OFF CACHE BOOL "Turn on/off support for OpenCl CPP classes" FORCE)

add_subdirectory(imglib)

# imglib is first-party in practice, so it gets gweni's warning flags.
gweni_set_warnings(imglib)
