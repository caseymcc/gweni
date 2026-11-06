/*
 *  Gweni - shared offscreen capture plumbing for the examples' capture modes
 *  (ControlTest --shot, OpenGLCore --screenshot): a GL context with or without
 *  a display, an offscreen target, and framebuffer readback to PNG. Also where
 *  both examples choose their resource directory and load their fonts.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_example_capture_h_
#define _gweni_example_capture_h_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gweni/platforms/platformTypes.h>
#include <gweni/utils/filesystem.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

#if defined(__linux__)
#define GWENI_EXAMPLE_EGL 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

namespace gweni
{
namespace capture
{

//! Prefix for this binary's diagnostics, e.g. "ControlTest".
void setToolName(const std::string &name);

//! Report GLFW errors on stderr under the tool-name prefix. Call it before the
//! first initGlfwContext(): the "no display" error is raised inside glfwInit().
void installGlfwErrorHandler();

//-----------------------------------------------------------------------------
// Resources
//-----------------------------------------------------------------------------

//! The ResourcePaths an example loads everything through: \a dir (relative to
//! the working directory, or absolute) when --resources gave one, otherwise the
//! resources/ the build copies next to the executable. Prints one line naming
//! the directory and returns null if it does not exist. The renderer keeps a
//! reference, so the object must outlive it.
std::unique_ptr<ResourcePaths> resourcePaths(const std::string &dir);

//! Load the skin's default font and the global gweni::getDefaultFont() that
//! Text and Label use, so a missing face fails here, once, instead of at the
//! first text drawn. \return false if either did not load; the loader has
//! already printed the path it tried.
bool loadDefaultFonts(IResourceLoader &loader, const Font &skinFont);

//-----------------------------------------------------------------------------
// Context creation: a GLFW window (visible or hidden) or a surfaceless EGL
// context for machines with no display at all.
//-----------------------------------------------------------------------------
struct Context
{
    GLFWwindow *window=nullptr;     // null when running on EGL
#if GWENI_EXAMPLE_EGL
    EGLDisplay eglDisplay=EGL_NO_DISPLAY;
    EGLContext eglContext=EGL_NO_CONTEXT;
#endif
};

bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible);

#if GWENI_EXAMPLE_EGL
bool initEglContext(Context &ctx);
#endif

void destroyContext(Context &ctx);

//! glewInit, tolerating the EGL path's GLX complaint. \return false if fatal.
bool initGlew();

//-----------------------------------------------------------------------------
// Offscreen target + PNG readback
//-----------------------------------------------------------------------------
struct Framebuffer
{
    GLuint fbo=0;
    GLuint color=0;
    GLuint depth=0;
};

bool createFramebuffer(Framebuffer &fb, int width, int height);
void destroyFramebuffer(Framebuffer &fb);

//! Read the bound framebuffer into top-down RGBA8 rows, as a PNG wants them.
void readFramebufferRgba(int width, int height, std::vector<uint8_t> &pixels);

//! Write top-down RGBA8 rows as a PNG.
bool writeRgbaPng(const std::vector<uint8_t> &pixels, int width, int height,
    const std::string &path);

//! Read the bound framebuffer and write it as a top-down RGBA PNG.
bool writeFramebufferPng(int width, int height, const std::string &path);

//! Create \a dir (and parents) if it is not there yet.
bool ensureDirectory(const fs::path &dir);

}//namespace capture
}//namespace gweni

#endif//_gweni_example_capture_h_
