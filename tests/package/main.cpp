/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  package_consumer: an app built against an installed gweni (cmake --install
 *  of the tree into a scratch prefix, tests/packageConsumer.cmake), with
 *  nothing of gweni's own trees: find_package(gweni), gweni::gweni and
 *  gweni_add_resources (CMakeLists.txt beside it), its own GL context, and the
 *  library's createVulkanDevice. On each renderer the package was built with
 *  (PACKAGE_HAS_GL, GWENI_HAS_VULKAN) it puts a Label, a Button, a
 *  CheckBoxWithLabel and a TextBox on a canvas with the textured skin loaded
 *  from the resources/ gweni_add_resources put beside the executable
 *  (RelativeToExecutablePaths, as an app does), clears, updates and renders one
 *  frame and reads it back, and fails, with one line naming it, if
 *  - the skin config or the skin's default font does not load from there;
 *  - a control type has no skin renderer (the textured renderers' object is
 *    dropped from libgweni.a without its anchor, registerTexturedRenderers);
 *  - the frame is one flat colour;
 *  - the Vulkan device reports an error.
 *
 *  The GL context is surfaceless EGL with an offscreen framebuffer, desktop GL
 *  3.3 core through glew or, with GWENI_GL_ES (the GLES tree), GLES 3.0. The
 *  Vulkan device is validated when GWENI_VULKAN_VALIDATION=1, which the test
 *  sets.
 */
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/textBox.h>
#include <gweni/platforms/commonPlatform.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/skins/textured/texturedSkin.h>

#ifdef PACKAGE_HAS_GL
#include <gweni/platforms/renderers/glApi.h>
#include <gweni/platforms/renderers/openglCore.h>
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif
#ifdef GWENI_HAS_VULKAN
#include <gweni/platforms/renderers/vulkan.h>
#endif

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace
{

using namespace gweni;

constexpr int Width=320;
constexpr int Height=200;

bool fail(const char *backend, const std::string &message)
{
    std::fprintf(stderr, "package_consumer: %s: %s\n", backend, message.c_str());
    return false;
}

//! One frame of the four controls on \a renderer, read back and checked
bool drawFrame(renderer::SlotRenderer &renderer, const char *backend)
{
    skin::textured::TexturedSkin skin(&renderer);

    if(!skin.init("DefaultSkin.json"))
        return fail(backend, "the textured skin did not load DefaultSkin.json from the resources/ beside the executable");
    if(renderer.loadFont(skin.getDefaultFont()) != Font::Status::Loaded)
        return fail(backend, "the skin's default font did not load from the resources/ beside the executable");

    controls::Canvas *canvas=new controls::Canvas(&skin);
    canvas->setSize(Width, Height);

    controls::Label *label=canvas->newChild<controls::Label>();
    label->setText("gweni from a package");
    label->setBounds(10, 10, 200, 20);
    controls::Button *button=canvas->newChild<controls::Button>();
    button->setText("Button");
    button->setBounds(10, 40, 100, 24);
    controls::CheckBoxWithLabel *check=canvas->newChild<controls::CheckBoxWithLabel>();
    check->label()->setText("Check");
    check->checkBox()->setChecked(true);
    check->setBounds(10, 74, 150, 20);
    controls::TextBox *text=canvas->newChild<controls::TextBox>();
    text->setText("text");
    text->setBounds(10, 104, 150, 24);

    renderer.clearTarget(Color(0, 0, 0, 255));
    canvas->updateCanvas();
    canvas->renderCanvas();

    std::vector<uint8_t> pixels;
    renderer.readPixels(Width, Height, pixels);
    delete canvas;

    const std::vector<std::string> missing=controls::typesWithoutSkinRenderer();
    if(!missing.empty())
    {
        std::string names;
        for(const std::string &name:missing)
            names+=" "+name;
        return fail(backend, "control types without a skin renderer:"+names);
    }
    if(pixels.size() != size_t(Width)*Height*4)
        return fail(backend, "readPixels returned the wrong size");

    std::set<uint32_t> colors;
    for(size_t i=0; i < pixels.size(); i+=4)
    {
        uint32_t color;
        std::memcpy(&color, &pixels[i], 4);
        colors.insert(color);
    }
    if(colors.size() < 2)
        return fail(backend, "the frame is one flat colour: nothing was drawn");

    std::printf("package_consumer: %s: %dx%d frame, %zu colours\n", backend, Width, Height, colors.size());
    return true;
}

#ifdef PACKAGE_HAS_GL
//! A surfaceless EGL context with an offscreen framebuffer bound, as an app
//! drawing headless makes one (gweni has none to give: capture.cpp is its
//! examples')
class GlContext
{
public:
    ~GlContext()
    {
        if(m_display == EGL_NO_DISPLAY)
            return;
        if(m_fbo)
        {
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
            glDeleteFramebuffers(1, &m_fbo);
            glDeleteRenderbuffers(1, &m_depth);
            glDeleteTextures(1, &m_color);
        }
        eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        if(m_context != EGL_NO_CONTEXT)
            eglDestroyContext(m_display, m_context);
        eglTerminate(m_display);
    }

    bool create(std::string &error)
    {
        m_display=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
        if(m_display == EGL_NO_DISPLAY || !eglInitialize(m_display, nullptr, nullptr))
        {
            m_display=EGL_NO_DISPLAY;
            error="no surfaceless EGL display";
            return false;
        }
#ifdef GWENI_GL_ES
        const EGLint renderableType=EGL_OPENGL_ES3_BIT;
        const EGLenum api=EGL_OPENGL_ES_API;
        const EGLint contextAttribs[]={EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 0, EGL_NONE};
#else
        const EGLint renderableType=EGL_OPENGL_BIT;
        const EGLenum api=EGL_OPENGL_API;
        const EGLint contextAttribs[]={EGL_CONTEXT_MAJOR_VERSION, 3, EGL_CONTEXT_MINOR_VERSION, 3,
            EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT, EGL_NONE};
#endif
        const EGLint configAttribs[]={EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, renderableType,
            EGL_RED_SIZE, 8, EGL_GREEN_SIZE, 8, EGL_BLUE_SIZE, 8, EGL_ALPHA_SIZE, 8, EGL_NONE};
        EGLConfig config;
        EGLint configCount=0;

        if(!eglChooseConfig(m_display, configAttribs, &config, 1, &configCount) || configCount == 0)
        {
            error="no usable EGL config";
            return false;
        }
        if(!eglBindAPI(api))
        {
            error="eglBindAPI failed";
            return false;
        }
        m_context=eglCreateContext(m_display, config, EGL_NO_CONTEXT, contextAttribs);
        if(m_context == EGL_NO_CONTEXT || !eglMakeCurrent(m_display, EGL_NO_SURFACE, EGL_NO_SURFACE, m_context))
        {
            error="could not make an EGL context current";
            return false;
        }
#ifndef GWENI_GL_ES
        glewExperimental=GL_TRUE;
        const GLenum status=glewInit();
        // vcpkg's glew is GLX-flavoured: on EGL it reports NO_GLX_DISPLAY with
        // every core entry point resolved
        if(status != GLEW_OK && status != GLEW_ERROR_NO_GLX_DISPLAY)
        {
            error="glewInit failed";
            return false;
        }
        glGetError();
#endif
        glGenTextures(1, &m_color);
        glBindTexture(GL_TEXTURE_2D, m_color);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, Width, Height, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glGenRenderbuffers(1, &m_depth);
        glBindRenderbuffer(GL_RENDERBUFFER, m_depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, Width, Height);
        glGenFramebuffers(1, &m_fbo);
        glBindFramebuffer(GL_FRAMEBUFFER, m_fbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, m_color, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, m_depth);
        if(glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE)
        {
            error="the offscreen framebuffer is incomplete";
            return false;
        }
        glViewport(0, 0, Width, Height);
        return true;
    }

private:
    EGLDisplay m_display=EGL_NO_DISPLAY;
    EGLContext m_context=EGL_NO_CONTEXT;
    GLuint m_color=0;
    GLuint m_depth=0;
    GLuint m_fbo=0;
};

bool runGl()
{
    GlContext context;
    std::string error;

    if(!context.create(error))
        return fail("gl", error);

    platform::RelativeToExecutablePaths paths("resources");
    renderer::OpenGLCore renderer(paths, Rect(Point(0, 0), Point(Width, Height)));

    renderer.init();
    return drawFrame(renderer, "gl");
}
#endif//PACKAGE_HAS_GL

#ifdef GWENI_HAS_VULKAN
bool runVulkan()
{
    renderer::VulkanDevice device;
    renderer::VulkanDeviceOptions options;
    std::string error;
    const char *validation=std::getenv("GWENI_VULKAN_VALIDATION");

    options.applicationName="package_consumer";
    options.validation=(validation && std::strcmp(validation, "1") == 0);
    if(!renderer::createVulkanDevice(device, options, error))
        return fail("vulkan", error);

    bool result=false;
    {
        renderer::VulkanTarget target(device);

        if(target.create(Width, Height))
        {
            platform::RelativeToExecutablePaths paths("resources");
            renderer::Vulkan renderer(paths, Rect(Point(0, 0), Point(Width, Height)), device, target);

            renderer.init();
            result=drawFrame(renderer, "vulkan");
            if(result && renderer.deviceError())
                result=fail("vulkan", "the device reported an error");
        }
        else
            result=fail("vulkan", "could not make the Vulkan target");
    }
    renderer::destroyVulkanDevice(device);
    return result;
}
#endif//GWENI_HAS_VULKAN

}//namespace

int main()
{
    int renderers=0;
    bool ok=true;

#ifdef PACKAGE_HAS_GL
    ++renderers;
    ok=runGl() && ok;
#endif
#ifdef GWENI_HAS_VULKAN
    ++renderers;
    ok=runVulkan() && ok;
#endif
    if(renderers == 0)
    {
        std::fprintf(stderr, "package_consumer: the package has no renderer to draw with\n");
        return EXIT_FAILURE;
    }
    return ok ? EXIT_SUCCESS : EXIT_FAILURE;
}
