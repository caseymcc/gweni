/*
 *  Gweni - single-control test harness
 *
 *  Drops ONE control (via its demo builder) directly on the canvas, so each
 *  control can be verified in isolation without the demo's dock/page layout.
 *
 *  Usage: ControlTest [options]
 *    --control <Name>   control demo to show (default ButtonDemo)
 *    --size WxH         canvas/framebuffer size (default 640x480)
 *    --frames N         frames to render before capturing (default 3)
 *    --shot <path>      render offscreen and write one PNG, then exit
 *    --shot-all         write one PNG per control demo, in this one run
 *    --shot-dir <dir>   directory for --shot-all, created if missing (default /tmp)
 *    --headless         force the surfaceless EGL context (no display needed)
 *    --list             print the control demo names and exit
 *
 *  With no --shot/--shot-all it opens the interactive window, as before.
 */

#include <GL/glew.h>
#include <gweni/platforms/renderers/openglCore.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <GLFW/glfw3.h>
#include <imglib/png.h>
#include <imglib/simpleImage.h>
#include <gweni/utils/filesystem.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <string>
#include <vector>

#if defined(__linux__)
#define GWENI_CONTROLTEST_EGL 1
#include <EGL/egl.h>
#include <EGL/eglext.h>
#endif

#include <demo/button.h>
#include <demo/checkBox.h>
#include <demo/collapsibleList.h>
#include <demo/colorPicker.h>
#include <demo/comboBox.h>
#include <demo/crossSplitter.h>
#include <demo/groupBox.h>
#include <demo/imagePanel.h>
#include <demo/label.h>
#include <demo/labelMultiline.h>
#include <demo/listBox.h>
#include <demo/menuStrip.h>
#include <demo/numeric.h>
#include <demo/pageControl.h>
#include <demo/progressBar.h>
#include <demo/properties.h>
#include <demo/radioButton.h>
#include <demo/scrollControl.h>
#include <demo/slider.h>
#include <demo/statusBar.h>
#include <demo/tabControl.h>
#include <demo/treeControl.h>
#include <demo/textBox.h>
#include <demo/window.h>

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;

static void key_callback(GLFWwindow*, int key, int scancode, int action, int mods)
{   const gweni::input::GLFW3::KeyEvent e={key,scancode,action,mods}; g_glfwInput.processKeyEvent(e); }
static void cursor_pos_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processMouseMove(x,y); }
static void mouse_button_callback(GLFWwindow*, int b, int a, int m)
{   g_glfwInput.processMouseButtons(b,a,m); }
static void scroll_callback(GLFWwindow*, double x, double y)
{   g_glfwInput.processScroll(x, y); }
static void framebuffer_size_callback(GLFWwindow*, int w, int h)
{
    if(w<=0||h<=0) return;
    if(g_renderer) g_renderer->setViewRect(gweni::Rect(gweni::Point(0,0), gweni::Point(w,h)));
    if(g_canvas) g_canvas->setSize(w,h);
}

#define GWENI_CONTROL_DEMOS(X) \
    X(ButtonDemo) X(LabelDemo) X(LabelMultilineDemo) X(ProgressBarDemo) \
    X(GroupBoxDemo) X(ImagePanelDemo) X(StatusBarDemo) X(ComboBoxDemo) \
    X(TextBoxDemo) X(ListBoxDemo) X(CrossSplitterDemo) X(RadioButtonDemo) \
    X(CheckBoxDemo) X(NumericDemo) X(SliderDemo) X(MenuStripDemo) \
    X(WindowDemo) X(TreeControlDemo) X(PropertiesDemo) X(TabControlDemo) \
    X(ScrollControlDemo) X(PageControlDemo) X(CollapsibleListDemo) X(ColorPickerDemo)

static const char *const g_controlNames[]=
{
#define GWENI_NAME(N) #N,
    GWENI_CONTROL_DEMOS(GWENI_NAME)
#undef GWENI_NAME
};

static gweni::controls::Base *createControl(gweni::controls::Canvas *canvas, const std::string &which)
{
#define TRY(N) if(which==#N) return canvas->newChild<gweni::N>();
    GWENI_CONTROL_DEMOS(TRY)
#undef TRY
    return nullptr;
}

static bool isKnownControl(const std::string &which)
{
    for(const char *name : g_controlNames)
        if(which == name) return true;
    return false;
}

static void printControlNames(std::ostream &out)
{
    out<<"Available controls:\n";
    for(const char *name : g_controlNames)
        out<<"  "<<name<<"\n";
}

//-----------------------------------------------------------------------------
// Context creation: a GLFW window (visible or hidden) or a surfaceless EGL
// context for machines with no display at all.
//-----------------------------------------------------------------------------
struct Context
{
    GLFWwindow *window=nullptr;     // null when running on EGL
#if GWENI_CONTROLTEST_EGL
    EGLDisplay eglDisplay=EGL_NO_DISPLAY;
    EGLContext eglContext=EGL_NO_CONTEXT;
#endif
};

static bool initGlfwContext(Context &ctx, const gweni::Point &size, const std::string &title, bool visible)
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

#if GWENI_CONTROLTEST_EGL
static bool initEglContext(Context &ctx)
{
    ctx.eglDisplay=eglGetPlatformDisplay(EGL_PLATFORM_SURFACELESS_MESA, EGL_DEFAULT_DISPLAY, nullptr);
    if(ctx.eglDisplay == EGL_NO_DISPLAY)
    {
        std::cerr<<"ControlTest: no surfaceless EGL display\n";
        return false;
    }

    EGLint major=0, minor=0;
    if(!eglInitialize(ctx.eglDisplay, &major, &minor))
    {
        std::cerr<<"ControlTest: eglInitialize failed\n";
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
        std::cerr<<"ControlTest: no usable EGL config\n";
        return false;
    }

    if(!eglBindAPI(EGL_OPENGL_API))
    {
        std::cerr<<"ControlTest: eglBindAPI(EGL_OPENGL_API) failed\n";
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
        std::cerr<<"ControlTest: eglCreateContext failed\n";
        return false;
    }

    if(!eglMakeCurrent(ctx.eglDisplay, EGL_NO_SURFACE, EGL_NO_SURFACE, ctx.eglContext))
    {
        std::cerr<<"ControlTest: eglMakeCurrent failed\n";
        return false;
    }
    return true;
}
#endif//GWENI_CONTROLTEST_EGL

static void destroyContext(Context &ctx)
{
    if(ctx.window)
    {
        glfwTerminate();
        ctx.window=nullptr;
        return;
    }
#if GWENI_CONTROLTEST_EGL
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

//-----------------------------------------------------------------------------
// Offscreen target + PNG readback
//-----------------------------------------------------------------------------
struct Framebuffer
{
    GLuint fbo=0;
    GLuint color=0;
    GLuint depth=0;
};

static void destroyFramebuffer(Framebuffer &fb)
{
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    if(fb.fbo) glDeleteFramebuffers(1, &fb.fbo);
    if(fb.depth) glDeleteRenderbuffers(1, &fb.depth);
    if(fb.color) glDeleteTextures(1, &fb.color);
    fb={};
}

static bool createFramebuffer(Framebuffer &fb, int width, int height)
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
        std::cerr<<"ControlTest: offscreen framebuffer incomplete\n";
        destroyFramebuffer(fb);
        return false;
    }
    glViewport(0, 0, width, height);
    return true;
}

static bool writeFramebufferPng(int width, int height, const std::string &path)
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
        std::cerr<<"ControlTest: cannot replace existing "<<path<<"\n";
        return false;
    }

    imglib::savePng(image, path.c_str());

    // imglib::savePng returns false even when it succeeded (deps/imglib png.cpp
    // falls off the end of the function), so judge it by the file it produced.
    FILE *written=fopen(path.c_str(), "rb");
    const bool ok=written && (fseek(written, 0, SEEK_END) == 0) && (ftell(written) > 0);
    if(written) fclose(written);

    if(!ok)
        std::cerr<<"ControlTest: failed to write "<<path<<"\n";
    return ok;
}

//-----------------------------------------------------------------------------

// A fresh container has no /tmp/shots, so make the destination rather than
// failing once per capture.
static bool ensureDirectory(const fs::path &dir)
{
    if(dir.empty() || fs::is_directory(dir))
        return true;

    try
    {
        if(fs::create_directories(dir))
            return true;
        std::cerr<<"ControlTest: cannot create directory "<<dir.string()<<"\n";
    }
    catch(const std::exception &error)
    {
        std::cerr<<"ControlTest: cannot create directory "<<dir.string()
                 <<": "<<error.what()<<"\n";
    }
    return false;
}

static bool parseSize(const std::string &value, gweni::Point &size)
{
    const size_t sep=value.find_first_of("xX");
    if(sep == std::string::npos) return false;

    const int w=std::atoi(value.substr(0, sep).c_str());
    const int h=std::atoi(value.substr(sep+1).c_str());
    if(w<=0 || h<=0) return false;

    size=gweni::Point(w, h);
    return true;
}

int main(int argc, char *argv[])
{
    std::string which="ButtonDemo";
    std::string shotPath;
    std::string shotDir="/tmp";
    gweni::Point screenSize(640, 480);
    int frames=3;
    bool shotAll=false;
    bool forceHeadless=false;

    for(int i=1; i<argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1)<argc;

        if(arg == "--control" && hasValue) which=argv[++i];
        else if(arg == "--shot" && hasValue) shotPath=argv[++i];
        else if(arg == "--shot-dir" && hasValue) shotDir=argv[++i];
        else if(arg == "--frames" && hasValue) frames=std::atoi(argv[++i]);
        else if(arg == "--shot-all") shotAll=true;
        else if(arg == "--headless") forceHeadless=true;
        else if(arg == "--list"){ printControlNames(std::cout); return EXIT_SUCCESS; }
        else if(arg == "--size" && hasValue)
        {
            if(!parseSize(argv[++i], screenSize))
            {
                std::cerr<<"ControlTest: --size wants WxH, e.g. 640x480\n";
                return EXIT_FAILURE;
            }
        }
        else
        {
            std::cerr<<"ControlTest: unknown argument: "<<arg<<"\n";
            return EXIT_FAILURE;
        }
    }

    if(frames<1) frames=1;

    if(!isKnownControl(which))
    {
        std::cout<<"Unknown control: "<<which<<"\n";
        printControlNames(std::cout);
        return EXIT_FAILURE;
    }

    if(shotAll && !shotPath.empty())
    {
        std::cerr<<"ControlTest: --shot and --shot-all are exclusive;"
                   " --shot-all names its files after the controls\n";
        return EXIT_FAILURE;
    }

    const bool capturing=shotAll || !shotPath.empty();

    std::vector<std::string> captures;
    if(shotAll)
    {
        if(!ensureDirectory(shotDir))
            return EXIT_FAILURE;
        for(const char *name : g_controlNames)
            captures.push_back(name);
    }
    else if(capturing)
    {
        if(!ensureDirectory(fs::path(shotPath).parent_path()))
            return EXIT_FAILURE;
        captures.push_back(which);
    }

    if(forceHeadless && !capturing)
    {
        std::cerr<<"ControlTest: --headless has no window to show; pass --shot or --shot-all\n";
        return EXIT_FAILURE;
    }

    Context ctx;
    if(forceHeadless)
    {
#if GWENI_CONTROLTEST_EGL
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: --headless needs the EGL path, which this platform lacks\n";
        return EXIT_FAILURE;
#endif
    }
    else if(!initGlfwContext(ctx, screenSize, "Gweni ControlTest - "+which, !capturing))
    {
        if(!capturing)
        {
            std::cerr<<"ControlTest: GLFW could not open a window;"
                       " interactive mode needs a display\n";
            return EXIT_FAILURE;
        }
#if GWENI_CONTROLTEST_EGL
        std::cerr<<"ControlTest: GLFW could not open a window"
                   " (no display?); falling back to surfaceless EGL\n";
        if(!initEglContext(ctx)) return EXIT_FAILURE;
#else
        std::cerr<<"ControlTest: GLFW could not open a window\n";
        return EXIT_FAILURE;
#endif
    }

    glewExperimental=GL_TRUE;
    const GLenum glewStatus=glewInit();
    // vcpkg's GLEW is GLX-flavoured: on the EGL path it reports GLEW_ERROR_NO_GLX_DISPLAY
    // while every core entry point still resolves, so that one is not fatal.
    if(glewStatus != GLEW_OK && glewStatus != GLEW_ERROR_NO_GLX_DISPLAY)
    {
        std::cerr<<"ControlTest: glewInit failed: "<<glewGetErrorString(glewStatus)<<"\n";
        destroyContext(ctx);
        return EXIT_FAILURE;
    }
    glGetError(); // GLEW's probing can leave an error flagged behind

    Framebuffer fb;
    if(capturing && !createFramebuffer(fb, screenSize.x, screenSize.y))
    {
        destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::platform::CurrentPath paths("resources");
    gweni::renderer::OpenGLCore *renderer=
        new gweni::renderer::OpenGLCore(paths, gweni::Rect(gweni::Point(0,0), screenSize));
    renderer->init();

    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(renderer);
    skin->init("resources/DefaultSkin.json");
    skin->setDefaultFont("resources/OpenSans.ttf", 11);

    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);
    canvas->setSize(screenSize.x, screenSize.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(background);

    g_renderer=renderer;
    g_canvas=canvas;

    int result=EXIT_SUCCESS;

    if(capturing)
    {
        for(const std::string &name : captures)
        {
            gweni::controls::Base *control=createControl(canvas, name);
            control->setDock(gweni::DockPosition::Center);

            for(int frame=0; frame<frames; ++frame)
            {
                canvas->updateCanvas();
                glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
                canvas->renderCanvas();
            }

            const std::string path=shotAll ? shotDir+"/"+name+".png" : shotPath;
            if(!writeFramebufferPng(screenSize.x, screenSize.y, path))
                result=EXIT_FAILURE;
            else
                std::cout<<"wrote "<<path<<"\n";

            // Goes through the canvas so preDelete() releases the control's
            // primitives; a bare delete leaves the renderer holding them.
            control->delayedDelete();
            canvas->updateCanvas();
        }
    }
    else
    {
        gweni::controls::Base *control=createControl(canvas, which);
        control->setDock(gweni::DockPosition::Center);

        g_glfwInput.initialize(canvas);
        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);

        while(!glfwWindowShouldClose(ctx.window))
        {
            canvas->updateCanvas();
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            canvas->renderCanvas();
            glfwSwapBuffers(ctx.window);
            glfwPollEvents();
            gweni::platform::sleep(0);
        }

        delete control;
    }

    delete canvas;
    delete skin;
    delete renderer;
    if(capturing) destroyFramebuffer(fb);
    destroyContext(ctx);
    return result;
}
