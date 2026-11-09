/*
 *  Gweni - shared offscreen capture plumbing for the examples' capture modes
 *  (ControlTest --shot, OpenGLCore --screenshot, gweni_bench): the device a run
 *  draws with, of either backend (--renderer gl|vulkan, Device below): a GL
 *  context with or without a display (desktop GL 3.3 core, GLES 3.0 in the GLES
 *  tree, GWENI_GL_ES, or WebGL 2 in the browser) or a headless Vulkan device,
 *  an offscreen target, and readback to PNG. Also where
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
#include <functional>
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
class SlotRenderer;
#ifdef GWENI_HAS_VULKAN
struct VulkanDevice;
class VulkanTarget;
#endif
}

namespace capture
{

//! Prefix for this binary's diagnostics, e.g. "ControlTest".
void setToolName(const std::string &name);
//! The prefix setToolName set ("capture" until it is called).
const std::string &toolName();

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

//! What a capture's profile clock (platform::profileTimeMicros()) advances per
//! read: installFixedProfileClock() makes it count reads, from 0, so the times
//! a frame records (CanvasFrameStats::times, the flush's) depend on how many
//! reads the frame makes and not on how fast it ran, and a shot showing them
//! (a tools::StatsOverlay's) is the same in every run. A window keeps the
//! default steady_clock.
constexpr uint64_t FixedProfileStepMicros=100;
void installFixedProfileClock();

//! --stats-overlay [on|off], the switch at argv[i]: whether it asks for the
//! overlay (gweni::tools::StatsOverlay) to be open at start-up. On, unless the
//! word after it is "off"; an "on" or "off" there is taken (i moves past it),
//! so a later --stats-overlay off overrides an earlier one.
bool statsOverlayArgument(int argc, char *argv[], int &i);

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
//!
//! In the browser the window is emscripten-glfw's (the contrib.glfw3 port) on
//! the page's #canvas, with a WebGL 2 context (depth, no antialiasing, no alpha,
//! so nothing behind the page shows through). Its framebuffer is the canvas's
//! CSS size times devicePixelRatio and its content scale devicePixelRatio
//! (GLFW_SCALE_FRAMEBUFFER), both followed by GLFW's callbacks as on a desktop.
//! A visible one fills the page's #canvas-container (web/shell.html) and
//! follows its size, so \a size is only where it starts.
bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible);

//! Runs \a frame once per displayed frame of \a ctx's window until it is
//! closed: natively a loop of \a frame, a buffer swap, an event poll and a
//! yield; in the browser one call per animation frame
//! (emscripten_set_main_loop), between which the page handles input, resizes
//! and draws. In the browser this never returns, and main's stack is unwound
//! before the first frame: \a frame must hold only what outlives main's locals
//! (heap objects, statics), never a reference to one.
void runWindowLoop(const Context &ctx, std::function<void()> frame);

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

//! The backend a run draws with: \a flag (--renderer's value) when it is not
//! empty, otherwise GWENI_RENDERER when it is set, otherwise gl. \return false,
//! after one line, for a name that is neither, or vulkan in a tree built without
//! it.
bool chooseBackend(const std::string &flag, Backend &backend);

//! Whether GWENI_VULKAN_VALIDATION=1 asks for the validation layer (the Vulkan
//! tests set it): a Vulkan device is then made with it, any message aborting.
bool vulkanValidation();

//! The --help text of --renderer, for each tool's usage
extern const char *const RendererHelp;

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

    //! Runs \a frame once per displayed frame of the window until it is closed,
    //! then waits for the device. GL: capture::runWindowLoop (a loop of \a frame,
    //! a buffer swap, an event poll and a yield; in the browser one call per
    //! animation frame, never returning). Vulkan: a loop of beginWindowFrame,
    //! \a frame and presentWindowFrame, and while the window is minimised
    //! (beginWindowFrame false) no frame, only glfwWaitEvents until it is
    //! restored.
    virtual void runWindowLoop(std::function<void()> frame);

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
//! a FIFO swapchain (vulkanWindow.cpp), the target the framebuffer's size. With
//! GWENI_VULKAN_VALIDATION=1 (the Vulkan tests set it) the device has the
//! validation layer, and any warning or error aborts; it fails without the layer.
//!
//! \return null, after one line, if it cannot be made.
std::unique_ptr<Device> openDevice(Backend backend, const DeviceOptions &options);

//! Create \a dir (and parents) if it is not there yet.
bool ensureDirectory(const fs::path &dir);

}//namespace capture
}//namespace gweni

#endif//_gweni_example_capture_h_
