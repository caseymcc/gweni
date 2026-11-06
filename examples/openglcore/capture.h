/*
 *  Gweni - shared offscreen capture plumbing for the examples' capture modes
 *  (ControlTest --shot, OpenGLCore --screenshot): a GL context with or without
 *  a display, an offscreen target, and framebuffer readback to PNG. Also where
 *  both examples choose their resource directory, load their fonts and work
 *  out the scale they draw at.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_example_capture_h_
#define _gweni_example_capture_h_

#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <gweni/platforms/input/glfw3.h>
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
namespace controls
{
class Canvas;
}
namespace renderer
{
class Base;
}

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

//! Load the skin's default font, the one Text and Label use, so a missing face
//! fails here, once, instead of at the first text drawn. \return false if it
//! did not load; the loader has already printed the path it tried.
bool loadDefaultFont(IResourceLoader &loader, const Font &skinFont);

//-----------------------------------------------------------------------------
// Scale: device pixels per logical unit. The canvas is laid out in logical
// units and the renderer draws it at framebuffer pixels; the effective scale is
// the window's OS content scale times a manual factor, which doubles as an
// app-level zoom.
//-----------------------------------------------------------------------------

//! A scale given as text: a number in (0, 16]. \return false otherwise.
bool parseScale(const char *value, float &scale);

//! The manual factor: \a scale as --scale left it when \a given, otherwise
//! GWENI_SCALE when it is set, otherwise 1 (the flag beats the environment).
//! \return false, after one line naming the value, if GWENI_SCALE is not a
//! scale.
bool manualScale(bool given, float &scale);

//! The OS content scale of \a window (glfwGetWindowContentScale, its x axis):
//! 2 on a Retina display, 1.5 on a desktop scaled to 150 %. 1 with no window
//! (the EGL context) and for every offscreen capture, so a capture is the same
//! on any display.
float contentScale(GLFWwindow *window);

//! The scale a window draws at: contentScale(window) * \a manual.
float effectiveScale(GLFWwindow *window, float manual);

//! A framebuffer size in device pixels as the logical size of a canvas drawn
//! at \a scale: round(size/scale), rounded half up as the renderer rounds.
Point logicalSize(const Point &framebuffer, float scale);

//! Fits the canvas to \a window's framebuffer: the renderer's view is the
//! framebuffer in device pixels, the canvas is that at effectiveScale(window,
//! \a manual) in logical units, and \a input maps the cursor through the
//! window and framebuffer sizes and that scale. What the framebuffer-size and
//! content-scale callbacks run; also call it once when the window is made. A
//! minimised (zero-sized) framebuffer changes nothing.
void fitCanvasToWindow(GLFWwindow *window, float manual, renderer::Base &renderer,
    controls::Canvas &canvas, input::GLFW3 &input);

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

//! A visible window is \a size in window coordinates scaled to its monitor's
//! content scale (GLFW_SCALE_TO_MONITOR), so on X11 and Windows a 150 % desktop
//! gets a window 1.5 times as many pixels across, as macOS and Wayland do on
//! their own. A hidden one (a capture) is \a size as it is.
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
