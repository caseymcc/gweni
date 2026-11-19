/*
 *  Gweni - the app shell's device layer (gweni_app, gweni::app): the device a
 *  run draws with, of either backend (Device below): a GL context with or
 *  without a display (desktop GL 3.3 core, GLES 3.0 in the GLES tree,
 *  GWENI_GL_ES, or WebGL 2 in the browser) or a Vulkan device, headless or in a
 *  window with a swapchain, an offscreen target, readback to PNG, the window
 *  loop, the resource directory, the default font and the scale a window draws
 *  at. The examples, the bench and the GL unit tests are built on it; what
 *  parses their command lines is theirs (examples/openglcore/capture.h).
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_app_device_h_
#define _gweni_app_device_h_

#include <gweni/platforms/renderers/glApi.h>
#include <GLFW/glfw3.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/idleLoop.h>
#include <gweni/platforms/platformTypes.h>
#include <gweni/utils/filesystem.h>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <vector>

// GWENI_APP_EGL: the headless context path, the one --headless takes. On
// Linux it is a surfaceless EGL context; in the browser (Emscripten) it is a
// WebGL 2 context on the page's canvas (GWENI_APP_WEBGL), which keeps the
// EGL path's names (initEglContext) so the apps and the tests take it for
// --headless as they are. Either way a capture draws into the offscreen
// Framebuffer below, never into a window's.
#if defined(__EMSCRIPTEN__)
#define GWENI_APP_EGL 1
#define GWENI_APP_WEBGL 1
#include <emscripten/emscripten.h>
#include <emscripten/html5.h>
#elif defined(__linux__)
#define GWENI_APP_EGL 1
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
class SlotRenderer;
#ifdef GWENI_HAS_VULKAN
struct VulkanDevice;
class VulkanTarget;
#endif
}

namespace app
{

//! Prefix for this binary's diagnostics, e.g. "ControlTest".
void setToolName(const std::string &name);
//! The prefix setToolName set ("gweni" until it is called).
const std::string &toolName();

//! Report GLFW errors on stderr under the tool-name prefix. Call it before the
//! first initGlfwContext(): the "no display" error is raised inside glfwInit().
void installGlfwErrorHandler();

//-----------------------------------------------------------------------------
// Resources
//-----------------------------------------------------------------------------

//! The ResourcePaths an app loads everything through: \a dir (relative to
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

//-----------------------------------------------------------------------------
// Scale: device pixels per logical unit. The canvas is laid out in logical
// units and the renderer draws it at framebuffer pixels; the effective scale is
// the window's OS content scale times a manual factor, which doubles as an
// app-level zoom.
//-----------------------------------------------------------------------------

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
#if GWENI_APP_WEBGL
    EMSCRIPTEN_WEBGL_CONTEXT_HANDLE webgl=0;
#elif GWENI_APP_EGL
    EGLDisplay eglDisplay=EGL_NO_DISPLAY;
    EGLContext eglContext=EGL_NO_CONTEXT;
#endif
};

//! A visible window is \a size in window coordinates scaled to its monitor's
//! content scale (GLFW_SCALE_TO_MONITOR), so on X11 and Windows a 150 % desktop
//! gets a window 1.5 times as many pixels across, as macOS and Wayland do on
//! their own. A hidden one (a capture) is \a size as it is.
//!
//! In the browser the window is emscripten-glfw's (the contrib.glfw3 port) on
//! the page's #canvas, with a WebGL 2 context (depth, no antialiasing, no alpha,
//! so nothing behind the page shows through). Its framebuffer is the canvas's
//! CSS size times devicePixelRatio and its content scale devicePixelRatio
//! (GLFW_SCALE_FRAMEBUFFER), both followed by GLFW's callbacks as on a desktop.
//! A visible one fills the page's #canvas-container (web/shell.html) and
//! follows its size, so \a size is only where it starts.
bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible);

//! Runs \a ctx's window until it is closed on input::GlfwLoop
//! (gweni/platforms/input/glfwLoop.h) over \a canvas: \a frame and a buffer
//! swap each time the loop draws, which idle (\a options, loopOptions()) is
//! only when the canvas has work, waiting for events in between, and
//! continuous every iteration; in the browser one call per animation frame
//! (emscripten_set_main_loop), between which the page handles input, resizes
//! and draws, a frame only when there is work. \a renderer is read for the
//! verify mode's frameStats(). In the browser this never returns, and main's
//! stack is unwound before the first frame: \a frame must hold only what
//! outlives main's locals (heap objects, statics), never a reference to one.
void runWindowLoop(const Context &ctx, controls::Canvas *canvas, renderer::Base *renderer,
    const platform::LoopOptions &options, std::function<void()> frame,
    std::function<void(const platform::IdleLoop &)> afterIteration=nullptr);

//! A window's loop options: idle, with the verify mode when
//! GWENI_IDLE_VERIFY=1.
platform::LoopOptions loopOptions();

#if GWENI_APP_EGL
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

//! Read \a renderer's target (SlotRenderer::readPixels) and write it as a top-down
//! RGBA PNG: what a capture of either backend writes.
bool writeRendererPng(renderer::SlotRenderer &renderer, int width, int height,
    const std::string &path);

//-----------------------------------------------------------------------------
// The backend a run draws with, and the device it draws on
//-----------------------------------------------------------------------------

//! The renderer backends: OpenGLCore on a GL context, renderer::Vulkan on a
//! Vulkan device (in a tree built with GWENI_RENDER_VULKAN).
enum class Backend
{
    GL,
    Vulkan
};

//! "gl" or "vulkan", as --renderer takes it
const char *backendName(Backend backend);

//! Whether GWENI_VULKAN_VALIDATION=1 asks for the validation layer (the Vulkan
//! tests set it): a Vulkan device is then made with it, any message aborting.
bool vulkanValidation();

//! What a run draws with: a device of one backend and, for a capture, the
//! offscreen target its renderer draws into (for a GL window, the window's
//! framebuffer). Made by openDevice; delete every renderer on it first.
class Device
{
public:
    virtual ~Device()=default;

    virtual Backend backend() const=0;

    //! The context, with its window when there is one: on GL an interactive
    //! run's window or a capture's hidden one (none on EGL); on Vulkan the
    //! interactive window (a capture has none, and no GL context either).
    const Context &context() const { return m_context; }

    //! A new renderer drawing into the target at \a viewRect (device pixels),
    //! not init()'d: an OpenGLCore, or a renderer::Vulkan on the device. It
    //! keeps a reference to \a paths; the caller deletes it.
    virtual renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect)=0;

    //! The offscreen target made again at \a size device pixels, what a capture's
    //! resize does (its contents are undefined until the next frame). Between
    //! frames only. \return false, after one line, if it cannot be made.
    virtual bool resizeTarget(const Point &size)=0;

    //! Waits until the device has done every frame sent to it: glFinish, or
    //! vkQueueWaitIdle.
    virtual void finish()=0;

    //! The device's name: GL_RENDERER, or the Vulkan device's deviceName.
    virtual std::string deviceName() const=0;

    //-------------------------------------------------------------------------
    // The interactive window (context().window), what runWindowLoop runs

    //! Runs the window on input::GlfwLoop over \a canvas until it is closed,
    //! then waits for the device: \a frame each time the loop draws (idle:
    //! when the canvas has work, \a options). GL: app::runWindowLoop
    //! (\a frame and a buffer swap; in the browser one call per animation
    //! frame, never returning). Vulkan: beginWindowFrame, \a frame and
    //! presentWindowFrame, a frame not shown (out of date, minimised since)
    //! marking the canvas for another; while the window is minimised or a
    //! remake failed (beginWindowFrame false) the loop draws nothing and
    //! waits for the next event.
    //! \a afterIteration, if set, is called after every iteration (natively)
    //! or callback (in the browser), with the loop's counts.
    virtual void runWindowLoop(controls::Canvas *canvas, renderer::Base *renderer,
        const platform::LoopOptions &options, std::function<void()> frame,
        std::function<void(const platform::IdleLoop &)> afterIteration=nullptr);

    //! Readies the window for a frame: on Vulkan the target and the swapchain
    //! made again at the framebuffer's size if it changed or the last acquire or
    //! present said so (out of date, suboptimal). False, with nothing to draw
    //! or show, while the window is minimised (iconified, or a 0x0 framebuffer);
    //! always true on GL, which draws into the window's own framebuffer.
    virtual bool beginWindowFrame() { return true; }

    //! Shows what the renderer drew since beginWindowFrame: glfwSwapBuffers, or
    //! the target blitted into an acquired swapchain image (turned top-down, as
    //! the window shows it) and presented. With \a pixels it is also read
    //! back, top-down RGBA8 rows of \a size device pixels: on Vulkan the
    //! swapchain image presented, copied after the blit. \return whether
    //! anything was shown: false when an acquire found the swapchain out of
    //! date (beginWindowFrame makes it again) or the window has been minimised
    //! since beginWindowFrame (nothing is presented to a minimised window).
    virtual bool presentWindowFrame(std::vector<uint8_t> *pixels=nullptr, Point *size=nullptr)=0;

    //! What a Vulkan window's swapchain has done, for the scripted window test
    //! (ControlTest --window-test); every count since the window opened.
    struct WindowStats
    {
        long frames=0;          //!< beginWindowFrame calls that returned true
        long skipped=0;         //!< beginWindowFrame calls that returned false: minimised
        long presents=0;        //!< images presented
        //! frames drawn but not shown: the window was minimised between
        //! beginWindowFrame and presentWindowFrame, which then acquires nothing
        long dropped=0;
        long swapchains=0;      //!< swapchains made: the first, then one per recreate
        long resizeRecreates=0; //!< recreates for a new framebuffer size
        long outOfDate=0;       //!< VK_ERROR_OUT_OF_DATE_KHR from an acquire or a present
        long suboptimal=0;      //!< VK_SUBOPTIMAL_KHR from an acquire or a present
        Point extent;           //!< the swapchain's size, 0x0 before the first
    };

    //! The window's swapchain counts; null for GL, which has no swapchain of its own.
    virtual const WindowStats *windowStats() const { return nullptr; }

#ifdef GWENI_HAS_VULKAN
    //! The Vulkan device and target a renderer::Vulkan draws with, for a caller
    //! constructing its own (gweni_bench's timed one); null for GL.
    virtual renderer::VulkanDevice *vulkanDevice() { return nullptr; }
    virtual renderer::VulkanTarget *vulkanTarget() { return nullptr; }
#endif

protected:
    Context m_context;
};

struct DeviceOptions
{
    //! the window's size, or the offscreen target's, in device pixels
    Point size;
    //! the window's title
    std::string title;
    //! a capture: draws into an offscreen target, shows no window; otherwise
    //! the interactive window
    bool capturing=false;
    //! --headless: a GL capture goes straight to surfaceless EGL instead of
    //! trying a hidden GLFW window first (Vulkan is always headless)
    bool headless=false;
    //! the interactive window's clipboard is the system's
    //! (input::GLFW3::useSystemClipboard); a capture's never is, nor is the
    //! window's with this off (--window-test, which reads nothing from it)
    bool systemClipboard=true;
};

//! The device a run draws with, ready for newRenderer().
//!
//! GL: a GLFW window (visible unless \a options.capturing), or, with
//! \a options.headless, or for a capture when no window opens (no display),
//! the surfaceless EGL context; initGL()'d, and for a capture an offscreen
//! Framebuffer of \a options.size bound.
//!
//! Vulkan: a device made by renderer::createVulkanDevice and a VulkanTarget of
//! \a options.size, with no GL or EGL context. A capture has no window, so with
//! or without --headless it is the same. An interactive run (no capture) opens
//! a GLFW window with no client API (GLFW_NO_API), a Vulkan surface on it and
//! a FIFO swapchain (source/app/vulkanWindow.cpp), the target the framebuffer's size. With
//! GWENI_VULKAN_VALIDATION=1 (the Vulkan tests set it) the device has the
//! validation layer, and any warning or error aborts; it fails without the layer.
//!
//! \return null, after one line, if it cannot be made.
std::unique_ptr<Device> openDevice(Backend backend, const DeviceOptions &options);

//! Create \a dir (and parents) if it is not there yet.
bool ensureDirectory(const fs::path &dir);

}//namespace app
}//namespace gweni

#endif//_gweni_app_device_h_
