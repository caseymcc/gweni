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

std::unique_ptr<ResourcePaths> resourcePaths(const std::string &dir)
{
    std::unique_ptr<ResourcePaths> paths;

    if(dir.empty())
        paths.reset(new platform::RelativeToExecutablePaths("resources"));
    else
        paths.reset(new platform::CurrentPath(dir));

    // An empty name resolves to the directory itself, with a trailing
    // separator that parent_path() drops.
    const fs::path resolved=fs::path(paths->getPath(ResourcePaths::Type::Other, "")).parent_path();

    if(!fs::is_directory(resolved))
    {
        std::cerr<<g_tool<<": resource directory not found: "<<resolved.string()<<"\n";
        return nullptr;
    }
    return paths;
}

bool loadDefaultFont(IResourceLoader &loader, const Font &skinFont)
{
    return loader.loadFont(skinFont) == Font::Status::Loaded;
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

bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible)
{
    if(!glfwInit())
        return false;

    glfwWindowHint(GLFW_VISIBLE, visible?GLFW_TRUE:GLFW_FALSE);
    glfwWindowHint(GLFW_SCALE_TO_MONITOR, visible?GLFW_TRUE:GLFW_FALSE);
    ctx.window=glfwCreateWindow(size.x, size.y, title.c_str(), NULL, NULL);
    if(!ctx.window)
    {
        glfwTerminate();
        return false;
    }
    glfwMakeContextCurrent(ctx.window);
    return true;
}

#if GWENI_EXAMPLE_EGL
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

    const EGLint configAttribs[]=
    {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
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

    if(!eglBindAPI(EGL_OPENGL_API))
    {
        std::cerr<<g_tool<<": eglBindAPI(EGL_OPENGL_API) failed\n";
        return false;
    }

    const EGLint contextAttribs[]=
    {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
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
#if GWENI_EXAMPLE_EGL
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

bool initGlew()
{
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
