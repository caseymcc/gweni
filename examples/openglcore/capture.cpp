/*
 *  Gweni - shared framebuffer readback + PNG encode for the examples' capture
 *  modes.
 *
 *  See license in Gweni.h
 */

#include "capture.h"

#include <gweni/controls/canvas.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/platform.h>
#include <gweni/platforms/renderers/openglCore.h>
#ifdef GWENI_HAS_VULKAN
#include <gweni/platforms/renderers/vulkan.h>
#endif

#if GWENI_EXAMPLE_WEBGL
#include <GLFW/emscripten_glfw3.h>
#endif

#include <imglib/png.h>
#include <imglib/simpleImage.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <vector>

namespace gweni
{
namespace capture
{

static std::string g_tool="capture";

void setToolName(const std::string &name)
{
    g_tool=name;
}

static void glfwErrorCallback(int code, const char *description)
{
    std::cerr<<g_tool<<": GLFW error "<<code<<": "<<description<<"\n";
}

void installGlfwErrorHandler()
{
    glfwSetErrorCallback(&glfwErrorCallback);
}

std::unique_ptr<ResourcePaths> resourcePaths(const std::string &dir, bool &found)
{
    std::unique_ptr<ResourcePaths> paths;

    if(dir.empty())
        paths.reset(new platform::RelativeToExecutablePaths("resources"));
    else
        paths.reset(new platform::CurrentPath(dir));

    // An empty name resolves to the directory itself, with a trailing
    // separator that parent_path() drops.
    const fs::path resolved=fs::path(paths->getPath(ResourcePaths::Type::Other, "")).parent_path();

    found=fs::is_directory(resolved);

    if(!found)
        std::cerr<<g_tool<<": resource directory not found: "<<resolved.string()<<"\n";
    return paths;
}

Font::Status loadDefaultFont(IResourceLoader &loader, const Font &skinFont)
{
    return loader.loadFont(skinFont);
}

int exitStatus(int result, bool degraded)
{
    if(result != EXIT_SUCCESS || !degraded)
        return result;

    std::cerr<<g_tool<<": resources incomplete, exiting "<<ExitDegraded<<"\n";
    return ExitDegraded;
}

bool parseScale(const char *value, float &scale)
{
    char *end=nullptr;
    const float parsed=std::strtof(value, &end);
    if(end == value || *end != '\0' || !(parsed > 0.0f) || parsed > 16.0f)
        return false;

    scale=parsed;
    return true;
}

bool manualScale(bool given, float &scale)
{
    if(given)
        return true;

    scale=1.0f;

    const char *env=std::getenv("GWENI_SCALE");
    if(env && *env && !parseScale(env, scale))
    {
        std::cerr<<g_tool<<": GWENI_SCALE wants a number in (0, 16], e.g. 1.5, not '"
                 <<env<<"'\n";
        return false;
    }
    return true;
}

float contentScale(GLFWwindow *window)
{
    if(!window)
        return 1.0f;

    float x=1.0f, y=1.0f;
    glfwGetWindowContentScale(window, &x, &y);
    return (x > 0.0f) ? x : 1.0f;
}

float effectiveScale(GLFWwindow *window, float manual)
{
    return contentScale(window)*manual;
}

Point logicalSize(const Point &framebuffer, float scale)
{
    return Point(renderer::Base::snap(float(framebuffer.x), 1.0f/scale),
        renderer::Base::snap(float(framebuffer.y), 1.0f/scale));
}

void fitCanvasToWindow(GLFWwindow *window, float manual, renderer::Base &renderer,
    controls::Canvas &canvas, input::GLFW3 &input)
{
    Point framebuffer, windowSize;
    glfwGetFramebufferSize(window, &framebuffer.x, &framebuffer.y);
    glfwGetWindowSize(window, &windowSize.x, &windowSize.y);

    if(framebuffer.x <= 0 || framebuffer.y <= 0)
        return;

    const float scale=effectiveScale(window, manual);
    const Point size=logicalSize(framebuffer, scale);

    renderer.setViewRect(Rect(Point(0, 0), framebuffer));
    canvas.setScale(scale);
    canvas.setSize(size.x, size.y);
    input.setWindowMetrics(windowSize, framebuffer, scale);
}

#if GWENI_EXAMPLE_WEBGL
//! Tells the page (web/pre.js) what the current WebGL context is: the browser
//! runner fails a run whose context is not WebGL 2.
static void reportWebGLContext()
{
    EM_ASM({
        if(Module['gweniWebGLContext'])
            Module['gweniWebGLContext'](GLctx);
    });
}
#endif

bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible)
{
    if(!glfwInit())
        return false;

    glfwWindowHint(GLFW_VISIBLE, visible?GLFW_TRUE:GLFW_FALSE);
    // GLFW's default, asked for: OpenGLCore spreads its 65536 z values over the
    // depth buffer, 256 steps each in 24 bits, as in the EGL path's framebuffer
    // (initGL checks what the window got)
    glfwWindowHint(GLFW_DEPTH_BITS, 24);
#if GWENI_EXAMPLE_WEBGL
    // emscripten-glfw: the WebGL version is the context's major version, and the
    // framebuffer is the canvas's CSS size times devicePixelRatio, the content
    // scale (GLFW_SCALE_FRAMEBUFFER; GLFW_SCALE_TO_MONITOR is deprecated there).
    // No alpha, so the page never shows through what the blend leaves
    // translucent, no stencil and no multisampling, as in the EGL path's
    // framebuffer.
    emscripten_glfw_set_next_window_canvas_selector("#canvas");
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 2);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
    glfwWindowHint(GLFW_SCALE_FRAMEBUFFER, GLFW_TRUE);
    glfwWindowHint(GLFW_ALPHA_BITS, 0);
    glfwWindowHint(GLFW_STENCIL_BITS, 0);
    glfwWindowHint(GLFW_SAMPLES, 0);
#else
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, visible?GLFW_TRUE:GLFW_FALSE);
#ifdef GWENI_GL_ES
    // the GLES tree's context is GLES 3.0, as the EGL path's is
    glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_ES_API);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 0);
#endif
#endif
    ctx.window=glfwCreateWindow(size.x, size.y, title.c_str(), NULL, NULL);
    if(!ctx.window)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(ctx.window);
#if GWENI_EXAMPLE_WEBGL
    reportWebGLContext();
    // a window on the page fills its container and follows its size: each
    // change is a new framebuffer size through GLFW's callback
    if(visible && emscripten_glfw_make_canvas_resizable(ctx.window, "#canvas-container", nullptr)
        != EMSCRIPTEN_RESULT_SUCCESS)
    {
        std::cerr<<g_tool<<": the page has no #canvas-container to fit the canvas to;"
                   " it stays "<<size.x<<"x"<<size.y<<"\n";
    }
#endif
    return true;
}

#if GWENI_EXAMPLE_WEBGL
//! The frame run by emscripten_set_main_loop_arg, and how many have run.
struct WebLoop
{
    std::function<void()> frame;
    long frames=0;
};

static void webLoopFrame(void *arg)
{
    WebLoop &loop=*static_cast<WebLoop *>(arg);

    // the input arrived since the last frame has been delivered already, but a
    // size the canvas's container asked for is applied here
    glfwPollEvents();
    loop.frame();
    ++loop.frames;
    // how many frames the page has drawn, for whoever drives it (web/pre.js)
    EM_ASM({
        if(typeof gweniRun !== 'undefined')
            gweniRun.frames=$0;
    }, loop.frames);
}
#endif

void runWindowLoop(const Context &ctx, std::function<void()> frame)
{
#if GWENI_EXAMPLE_WEBGL
    // The browser swaps after each callback (emscripten-glfw's glfwSwapBuffers
    // only logs that it is not implemented) and delivers the input between
    // them; the poll that applies a resize opens each frame (webLoopFrame). Never deleted: the loop runs
    // as long as the page. The rate is the browser's (0: requestAnimationFrame);
    // simulating an infinite loop unwinds main, so nothing after this runs.
    (void)ctx;
    WebLoop *loop=new WebLoop;
    loop->frame=std::move(frame);
    emscripten_set_main_loop_arg(&webLoopFrame, loop, 0, 1);
#else
    while(!glfwWindowShouldClose(ctx.window))
    {
        frame();
        glfwSwapBuffers(ctx.window);
        glfwPollEvents();
        platform::sleep(0);
    }
#endif
}

#if GWENI_EXAMPLE_WEBGL
bool initEglContext(Context &ctx)
{
    EmscriptenWebGLContextAttributes attributes;

    emscripten_webgl_init_context_attributes(&attributes);
    attributes.majorVersion=2;
    attributes.minorVersion=0;
    // the default framebuffer is never drawn or read (a capture draws into its
    // own Framebuffer, with its own 24-bit depth buffer): no multisampling, and
    // the context must not be refused for running on SwiftShader, a CPU
    // rasteriser
    attributes.depth=EM_TRUE;
    attributes.stencil=EM_FALSE;
    attributes.antialias=EM_FALSE;
    attributes.premultipliedAlpha=EM_FALSE;
    attributes.preserveDrawingBuffer=EM_FALSE;
    attributes.failIfMajorPerformanceCaveat=EM_FALSE;
    // the extensions are asked for by name (glGetStringi), not enabled one by one
    attributes.enableExtensionsByDefault=EM_TRUE;

    ctx.webgl=emscripten_webgl_create_context("#canvas", &attributes);
    if(ctx.webgl <= 0)
    {
        std::cerr<<g_tool<<": no WebGL 2 context on #canvas (emscripten_webgl_create_context: "
                 <<ctx.webgl<<")\n";
        ctx.webgl=0;
        return false;
    }
    if(emscripten_webgl_make_context_current(ctx.webgl) != EMSCRIPTEN_RESULT_SUCCESS)
    {
        std::cerr<<g_tool<<": emscripten_webgl_make_context_current failed\n";
        emscripten_webgl_destroy_context(ctx.webgl);
        ctx.webgl=0;
        return false;
    }
    reportWebGLContext();
    return true;
}
#elif GWENI_EXAMPLE_EGL
bool initEglContext(Context &ctx)
{
    ctx.eglDisplay=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if(ctx.eglDisplay == EGL_NO_DISPLAY)
    {
        std::cerr<<g_tool<<": no surfaceless EGL display\n";
        return false;
    }

    EGLint major=0, minor=0;
    if(!eglInitialize(ctx.eglDisplay, &major, &minor))
    {
        std::cerr<<g_tool<<": eglInitialize failed\n";
        return false;
    }

#ifdef GWENI_GL_ES
    // the GLES tree: a GLES 3.0 context
    const EGLint renderableType=EGL_OPENGL_ES3_BIT;
    const EGLenum api=EGL_OPENGL_ES_API;
    const char *const apiName="EGL_OPENGL_ES_API";
#else
    // desktop GL 3.3 core
    const EGLint renderableType=EGL_OPENGL_BIT;
    const EGLenum api=EGL_OPENGL_API;
    const char *const apiName="EGL_OPENGL_API";
#endif

    const EGLint configAttribs[]=
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, renderableType,
        EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8,
        EGL_NONE
    };
    EGLConfig config;
    EGLint configCount=0;
    if(!eglChooseConfig(ctx.eglDisplay, configAttribs, &config, 1, &configCount) || configCount == 0)
    {
        std::cerr<<g_tool<<": no usable EGL config\n";
        return false;
    }

    if(!eglBindAPI(api))
    {
        std::cerr<<g_tool<<": eglBindAPI("<<apiName<<") failed\n";
        return false;
    }

    const EGLint contextAttribs[]=
    {
        EGL_CONTEXT_MAJOR_VERSION, 3,
#ifdef GWENI_GL_ES
        EGL_CONTEXT_MINOR_VERSION, 0,
#else
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
#endif
        EGL_NONE
    };
    ctx.eglContext=eglCreateContext(ctx.eglDisplay, config, EGL_NO_CONTEXT, contextAttribs);
    if(ctx.eglContext == EGL_NO_CONTEXT)
    {
        std::cerr<<g_tool<<": eglCreateContext failed\n";
        return false;
    }

    if(!eglMakeCurrent(ctx.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx.eglContext))
    {
        std::cerr<<g_tool<<": eglMakeCurrent failed\n";
        return false;
    }
    return true;
}
#endif//GWENI_EXAMPLE_EGL

void destroyContext(Context &ctx)
{
    if(ctx.window)
    {
        glfwTerminate();
        ctx.window=nullptr;
        return;
    }
#if GWENI_EXAMPLE_WEBGL
    if(ctx.webgl)
    {
        emscripten_webgl_make_context_current(0);
        emscripten_webgl_destroy_context(ctx.webgl);
        ctx.webgl=0;
    }
#elif GWENI_EXAMPLE_EGL
    if(ctx.eglDisplay != EGL_NO_DISPLAY)
    {
        eglMakeCurrent(ctx.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if(ctx.eglContext != EGL_NO_CONTEXT)
            eglDestroyContext(ctx.eglDisplay, ctx.eglContext);
        eglTerminate(ctx.eglDisplay);
        ctx.eglDisplay=EGL_NO_DISPLAY;
        ctx.eglContext=EGL_NO_CONTEXT;
    }
#endif
}

//! Whether the bound framebuffer's depth attachment (GL_DEPTH for a window's,
//! GL_DEPTH_ATTACHMENT for an FBO's) has at least the 24 bits OpenGLCore's z
//! layering needs; one line naming \a what if not.
static bool hasDepth24(GLenum attachment, const char *what)
{
    GLint bits=0;

    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, attachment,
        GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &bits);
    if(bits >= 24)
        return true;
    std::cerr<<g_tool<<": "<<what<<" has a "<<bits<<"-bit depth buffer, 24 bits are needed\n";
    return false;
}

bool initGL(const Context &ctx)
{
#ifndef GWENI_GL_ES
    glewExperimental=GL_TRUE;
    const GLenum status=glewInit();

    // vcpkg's GLEW is GLX-flavoured: on the EGL path it reports GLEW_ERROR_NO_GLX_DISPLAY
    // while every core entry point still resolves, so that one is not fatal.
    if(status != GLEW_OK && status != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr<<g_tool<<": glewInit failed: "<<glewGetErrorString(status)<<"\n";
        return false;
    }
    glGetError(); // GLEW's probing can leave an error flagged behind
#endif
    // a window draws into its own framebuffer; the EGL path has none, and its
    // offscreen one is checked by createFramebuffer
    if(ctx.window)
    {
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if(!hasDepth24(GL_DEPTH, "the window"))
            return false;
    }
    return true;
}

void destroyFramebuffer(Framebuffer &fb)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if(fb.fbo) glDeleteFramebuffers(1, &fb.fbo);
    if(fb.depth) glDeleteRenderbuffers(1, &fb.depth);
    if(fb.color) glDeleteTextures(1, &fb.color);
    fb={};
}

bool createFramebuffer(Framebuffer &fb, int width, int height)
{
    glGenTextures(1, &fb.color);
    glBindTexture(GL_TEXTURE_2D, fb.color);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);

    glGenRenderbuffers(1, &fb.depth);
    glBindRenderbuffer(GL_RENDERBUFFER, fb.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, width, height);

    glGenFramebuffers(1, &fb.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, fb.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, fb.color, 0);
    glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, fb.depth);

    if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
    {
        std::cerr<<g_tool<<": offscreen framebuffer incomplete\n";
        destroyFramebuffer(fb);
        return false;
    }
    if(!hasDepth24(GL_DEPTH_ATTACHMENT, "the offscreen framebuffer"))
    {
        destroyFramebuffer(fb);
        return false;
    }
    glViewport(0, 0, width, height);
    return true;
}

void readFramebufferRgba(int width, int height, std::vector<uint8_t> &pixels)
{
    const size_t rowBytes=static_cast<size_t>(width)*4;
    std::vector<uint8_t> raw(rowBytes*static_cast<size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, raw.data());

    // glReadPixels hands back bottom-up rows; PNG wants top-down.
    pixels.assign(raw.size(), 0);
    for(int y=0; y<height; ++y)
        memcpy(&pixels[rowBytes*static_cast<size_t>(y)],
               &raw[rowBytes*static_cast<size_t>(height-1-y)], rowBytes);
}

bool writeRgbaPng(const std::vector<uint8_t> &pixels, int width, int height,
    const std::string &path)
{
    imglib::SimpleImage image(imglib::Format::RGBA, imglib::Depth::Bit8,
        static_cast<size_t>(width), static_cast<size_t>(height),
        const_cast<uint8_t *>(pixels.data()), pixels.size());

    // A stale PNG left at this path by an earlier run must not be mistaken for
    // this capture's output.
    std::remove(path.c_str());
    if(fs::exists(path))
    {
        std::cerr<<g_tool<<": cannot replace existing "<<path<<"\n";
        return false;
    }

    const bool ok=imglib::savePng(image, path.c_str());

    if(!ok)
        std::cerr<<g_tool<<": failed to write "<<path<<"\n";
    return ok;
}

bool writeFramebufferPng(int width, int height, const std::string &path)
{
    std::vector<uint8_t> pixels;
    readFramebufferRgba(width, height, pixels);
    return writeRgbaPng(pixels, width, height, path);
}

bool writeRendererPng(renderer::SlotRenderer &renderer, int width, int height,
    const std::string &path)
{
    std::vector<uint8_t> pixels;
    renderer.readPixels(width, height, pixels);
    return writeRgbaPng(pixels, width, height, path);
}

const char *backendName(Backend backend)
{
    return (backend == Backend::Vulkan) ? "vulkan" : "gl";
}

bool chooseBackend(const std::string &flag, Backend &backend)
{
    std::string name=flag;
    const char *source="--renderer";

    if(name.empty())
    {
        const char *env=std::getenv("GWENI_RENDERER");

        name=(env && *env) ? env : "gl";
        source="GWENI_RENDERER";
    }

    if(name == "gl")
    {
        backend=Backend::GL;
        return true;
    }
    if(name == "vulkan")
    {
#ifdef GWENI_HAS_VULKAN
        backend=Backend::Vulkan;
        return true;
#else
        std::cerr<<g_tool<<": "<<source<<(flag.empty() ? "=" : " ")<<"vulkan: this build has no Vulkan renderer"
                   " (configure with -DGWENI_RENDER_VULKAN=ON, as ./generate.sh does)\n";
        return false;
#endif
    }
    std::cerr<<g_tool<<": "<<source<<" wants gl or vulkan, not '"<<name<<"'\n";
    return false;
}

const char *const RendererHelp=
    "  --renderer <gl|vulkan>  the backend to draw with (default gl, or\n"
    "                       GWENI_RENDERER; the flag beats it): OpenGLCore on a\n"
    "                       GL context, or renderer::Vulkan on a Vulkan device\n"
    "                       (no GL or EGL context; captures only, as if\n"
    "                       --headless, there is no Vulkan window yet; with\n"
    "                       GWENI_VULKAN_VALIDATION=1 the validation layer is on\n"
    "                       and any message aborts)\n";

namespace
{

//! A GL context (a window or surfaceless EGL) and, for a capture, the offscreen
//! framebuffer bound on it
class GlDevice: public Device
{
public:
    ~GlDevice() override
    {
        if(m_offscreen)
            destroyFramebuffer(m_framebuffer);
        destroyContext(m_context);
    }

    Backend backend() const override { return Backend::GL; }

    renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect) override
    {
        return new renderer::OpenGLCore(paths, viewRect);
    }

    bool resizeTarget(const Point &size) override
    {
        destroyFramebuffer(m_framebuffer);
        return createFramebuffer(m_framebuffer, size.x, size.y);
    }

    void finish() override
    {
        glFinish();
    }

    std::string deviceName() const override
    {
        const GLubyte *name=glGetString(GL_RENDERER);

        return name ? reinterpret_cast<const char *>(name) : "";
    }

    bool open(const DeviceOptions &options);

private:
    Framebuffer m_framebuffer;
    bool m_offscreen=false;
};

bool GlDevice::open(const DeviceOptions &options)
{
    // A capture does not need the window on screen, and on a machine with no
    // display at all it falls back to a surfaceless EGL context, so it can run in
    // the build container as it stands.
    if(options.headless)
    {
#if GWENI_EXAMPLE_EGL
        if(!initEglContext(m_context))
            return false;
#else
        std::cerr<<g_tool<<": --headless needs the EGL path, which this platform lacks\n";
        return false;
#endif
    }
    else if(!initGlfwContext(m_context, options.size, options.title, !options.capturing))
    {
        if(!options.capturing)
        {
            std::cerr<<g_tool<<": GLFW could not open a window;"
                       " interactive mode needs a display\n";
            return false;
        }
#if GWENI_EXAMPLE_EGL
        std::cerr<<g_tool<<": GLFW could not open a window"
                   " (no display?); falling back to surfaceless EGL\n";
        if(!initEglContext(m_context))
            return false;
#else
        std::cerr<<g_tool<<": GLFW could not open a window\n";
        return false;
#endif
    }

    if(!initGL(m_context))
        return false;

    if(options.capturing)
    {
        if(!createFramebuffer(m_framebuffer, options.size.x, options.size.y))
            return false;
        m_offscreen=true;
    }
    return true;
}

#ifdef GWENI_HAS_VULKAN
//! A Vulkan device with no window, and the target a capture draws into
class VulkanCaptureDevice: public Device
{
public:
    ~VulkanCaptureDevice() override
    {
        m_target.reset();
        renderer::destroyVulkanDevice(m_device);
    }

    Backend backend() const override { return Backend::Vulkan; }

    renderer::SlotRenderer *newRenderer(ResourcePaths &paths, const Rect &viewRect) override
    {
        return new renderer::Vulkan(paths, viewRect, m_device, *m_target);
    }

    bool resizeTarget(const Point &size) override
    {
        if(m_target->create(size.x, size.y))
            return true;
        std::cerr<<g_tool<<": could not make a "<<size.x<<"x"<<size.y<<" Vulkan target\n";
        return false;
    }

    void finish() override
    {
        vkQueueWaitIdle(m_device.queue);
    }

    std::string deviceName() const override
    {
        VkPhysicalDeviceProperties properties{};

        vkGetPhysicalDeviceProperties(m_device.physicalDevice, &properties);
        return properties.deviceName;
    }

    renderer::VulkanDevice *vulkanDevice() override { return &m_device; }
    renderer::VulkanTarget *vulkanTarget() override { return m_target.get(); }

    bool open(const DeviceOptions &options);

private:
    renderer::VulkanDevice m_device;
    std::unique_ptr<renderer::VulkanTarget> m_target;
};

bool VulkanCaptureDevice::open(const DeviceOptions &options)
{
    if(!options.capturing)
    {
        std::cerr<<g_tool<<": --renderer vulkan has no window yet;"
                   " it only captures (--shot, --screenshot, --frames, --stats, ...)\n";
        return false;
    }

    renderer::VulkanDeviceOptions deviceOptions;
    const char *validation=std::getenv("GWENI_VULKAN_VALIDATION");
    std::string error;

    deviceOptions.applicationName=g_tool;
    deviceOptions.validation=validation && std::strcmp(validation, "1") == 0;
    if(!renderer::createVulkanDevice(m_device, deviceOptions, error))
    {
        std::cerr<<g_tool<<": "<<error<<"\n";
        return false;
    }

    m_target.reset(new renderer::VulkanTarget(m_device));
    return resizeTarget(options.size);
}
#endif//GWENI_HAS_VULKAN

}//namespace

std::unique_ptr<Device> openDevice(Backend backend, const DeviceOptions &options)
{
#ifdef GWENI_HAS_VULKAN
    if(backend == Backend::Vulkan)
    {
        std::unique_ptr<VulkanCaptureDevice> device(new VulkanCaptureDevice);

        if(!device->open(options))
            return nullptr;
        return std::move(device);
    }
#else
    (void)backend;
#endif

    std::unique_ptr<GlDevice> device(new GlDevice);

    if(!device->open(options))
        return nullptr;
    return std::move(device);
}

bool ensureDirectory(const fs::path &dir)
{
    if(dir.empty() || fs::is_directory(dir))
        return true;

    try
    {
        if(fs::create_directories(dir))
            return true;
        std::cerr<<g_tool<<": cannot create directory "<<dir.string()<<"\n";
    }
    catch(const std::exception &error)
    {
        std::cerr<<g_tool<<": cannot create directory "<<dir.string()
                 <<": "<<error.what()<<"\n";
    }
    return false;
}

}//namespace capture
}//namespace gweni
