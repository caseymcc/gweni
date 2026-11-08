/*
 *  Gweni - shared offscreen capture plumbing for the examples' capture modes
 *  (ControlTest --shot, OpenGLCore --screenshot): a GL context with or without
 *  a display (desktop GL 3.3 core, GLES 3.0 in the GLES tree, GWENI_GL_ES, or
 *  WebGL 2 in the browser),
 *  an offscreen target, and framebuffer readback to PNG. Also where
 *  both examples choose their resource directory, load their fonts and work
 *  out the scale they draw at.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_example_capture_h_
#define _gweni_example_capture_h_

#include <gweni/platforms/renderers/glApi.h>
#include <GLFW/glfw3.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platformTypes.h>
#include <gweni/utils/filesystem.h>
#include <cstdint>
#include <memory>
#include <string>
#include <vector>

// GWENI_EXAMPLE_EGL: the headless context path, the one --headless takes. On
// Linux it is a surfaceless EGL context; in the browser (Emscripten) it is a
// WebGL 2 context on the page's canvas (GWENI_EXAMPLE_WEBGL), which keeps the
// EGL path's names (initEglContext) so the examples and the tests take it for
// --headless as they are. Either way a capture draws into the offscreen
// Framebuffer below, never into a window's.
#if defined(__EMSCRIPTEN__)
#define GWENI_EXAMPLE_EGL 1
#define GWENI_EXAMPLE_WEBGL 1
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#elif defined(__linux__)
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
//! resources/ the build copies next to the executable. If the directory does not
//! exist it prints one line naming it and sets \a found to false, but still
//! returns the paths: the skin, fonts and textures then fail to load one by one
//! and degrade (magenta surfaces, the built-in font) instead of the run stopping
//! here. Never null. The renderer keeps a reference, so the object must outlive
//! it.
std::unique_ptr<ResourcePaths> resourcePaths(const std::string &dir, bool &found);

//! Load the skin's default font, the one Text and Label use, so a missing face
//! is reported here, once, instead of at the first text drawn. \return the
//! loader's status: Loaded, LoadedFallback (the face is missing or unreadable
//! and text is drawn in the built-in one) or an error (no text is drawn). The
//! loader has already printed the path it tried.
Font::Status loadDefaultFont(IResourceLoader &loader, const Font &skinFont);

//! The exit status of a capture run that found a resource missing: it wrote
//! what it rendered, but not from the resources it was given.
constexpr int ExitDegraded=2;

//! The exit status of a non-interactive run: \a result, or ExitDegraded, after
//! one line saying so, when \a result is EXIT_SUCCESS and \a degraded (the
//! resource dir, the skin config or the default font was missing; each of those
//! has printed its own line already). A failure (EXIT_FAILURE) wins over it.
int exitStatus(int result, bool degraded);

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
#if GWENI_EXAMPLE_WEBGL
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl=0;
#elif GWENI_EXAMPLE_EGL
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
//! The headless context, current: surfaceless EGL (GL 3.3 core, or GLES 3.0 in
//! the GLES tree), or in the browser WebGL 2 on the page's #canvas with a depth
//! buffer and no antialiasing (the capture reads its own Framebuffer, never the
//! canvas). \return false, after one line, if there is none.
bool initEglContext(Context &ctx);
#endif

void destroyContext(Context &ctx);

//! Readies \a ctx, current: glewInit on desktop GL (tolerating the EGL path's
//! GLX complaint), nothing to load on GLES, whose entry points are linked; then,
//! for a window, checks that its framebuffer has a 24-bit depth buffer (one line
//! if not). \return false if fatal.
bool initGL(const Context &ctx);

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
