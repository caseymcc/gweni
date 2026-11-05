/*
 *  Gweni - shared framebuffer readback + PNG encode for the examples' capture
 *  modes.
 *
 *  See license in Gweni.h
 */

#include "capture.h"

#include <imglib/png.h>
#include <imglib/simpleImage.h>
#include <cstdio>
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

bool initGlfwContext(Context &ctx, const Point &size, const std::string &title, bool visible)
{
    if(!glfwInit())
        return false;

    glfwWindowHint(GLFW_VISIBLE, visible?GLFW_TRUE:GLFW_FALSE);
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

bool writeFramebufferPng(int width, int height, const std::string &path)
{
    const size_t rowBytes=static_cast<size_t>(width)*4;
    std::vector<uint8_t> pixels(rowBytes*static_cast<size_t>(height));

    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());

    // glReadPixels hands back bottom-up rows; PNG wants top-down.
    std::vector<uint8_t> flipped(pixels.size());
    for(int y=0; y<height; ++y)
        memcpy(&flipped[rowBytes*static_cast<size_t>(y)],
               &pixels[rowBytes*static_cast<size_t>(height-1-y)], rowBytes);

    imglib::SimpleImage image(imglib::Format::RGBA, imglib::Depth::Bit8,
        static_cast<size_t>(width), static_cast<size_t>(height), flipped.data(), flipped.size());

    // Success is judged by the file below, so a stale PNG left at this path by an
    // earlier run would turn a failed write into a reported success.
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
