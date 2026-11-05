/*
 *  Gweni
 *
 *  The demo application.
 *
 *  Usage: OpenGLCore [options]
 *    --demo <Name>        show that demo page at startup (e.g. ColorPickerDemo);
 *                         an unknown name lists the valid ones and exits non-zero
 *    --screenshot <path>  render a fixed number of frames, write one PNG and exit
 *    --frames <N>         render exactly N frames offscreen and exit
 *    --stats              with --frames, print per-frame timings and change counts
 *
 *  With no options it opens the interactive window as before. A capture needs no
 *  display: it hides the window, and with no display at all it falls back to a
 *  surfaceless EGL context. Run it from the repo root:
 *
 *    ./build_linux_x64_debug/examples/openglcore/OpenGLCore \
 *        --demo ColorPickerDemo --screenshot /tmp/hero.png
 *
 *  A capture installs a stepped clock (gweni::platform::setTimeSource), so the
 *  image depends on the frame number and not on how fast the process ran.
 *
 *  See license in Gweni.h
 */

#include <GL/glew.h>
#ifdef USE_DEBUG_FONT
#   include <gweni/platforms/renderers/opengl_debugFont.h>
#else
#   include <gweni/platforms/renderers/openglCore.h>
#endif
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/simple/simple.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <iostream>

#include <demo/demo.h>

#include <GLFW/glfw3.h>

#include "capture.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

//namespace gweni
//{
//namespace test
//{
//GWENI_DECLARE_TEST(Layout);
//}
//}

static gweni::input::GLFW3 g_glfwInput;
static gweni::renderer::Base *g_renderer=nullptr;
static gweni::controls::Canvas *g_canvas=nullptr;

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    const gweni::input::GLFW3::KeyEvent evt = { key, scancode, action, mods };
    g_glfwInput.processKeyEvent(evt);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    g_glfwInput.processMouseMove(xpos, ypos);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    g_glfwInput.processMouseButtons(button, action, mods);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_glfwInput.processScroll(xoffset, yoffset);
}

static void framebuffer_size_callback(GLFWwindow* /*window*/, int width, int height)
{
    if(width <= 0 || height <= 0)
        return;

    // Resize the GL viewport/projection and the root canvas so docked controls
    // re-layout to the new window size.
    if(g_renderer)
        g_renderer->setViewRect(gweni::Rect(gweni::Point(0, 0), gweni::Point(width, height)));

    if(g_canvas)
        g_canvas->setSize(width, height);
}

void glfwErrorCallback(int error_code, const char *description)
{
    std::cout<<"GLFW error: "<<error_code<<" "<<description<<std::endl;
}


// A capture's clock: one fixed step per rendered frame, so every run sees the
// same times. 1/60 s is the step, i.e. a nominal 60fps.
static const float g_frameStep=1.0f/60.0f;
static float g_clockSeconds=0.0f;

static float steppedClock()
{
    return g_clockSeconds;
}

// Frames rendered before a --screenshot readback: enough for the dock layout to
// settle, and fixed so the image does not depend on timing.
static const int g_captureFrames=3;

// How many controls the canvas tree holds, and how many of them still carry a
// state-change mask. This has to run between updateCanvas() and renderCanvas():
// render consumes the masks. getChildren() returns a control's inner panel's
// children when it has one, so a wrapper's inner panel is not itself counted --
// that is the public tree, and it is the same every run.
struct ChangeCounts
{
    size_t controls=0;
    size_t changed=0;
};

static void countChanges(gweni::controls::Base *control, ChangeCounts &counts)
{
    ++counts.controls;

    if(control->getStateChange() != gweni::controls::StateChange_Nothing)
        ++counts.changed;

    for(gweni::controls::Base *child : control->getChildren())
        countChanges(child, counts);
}

struct FrameSample
{
    ChangeCounts counts;
    long long updateUs=0;
    long long renderUs=0;
};

static long long medianOf(std::vector<long long> values)
{
    if(values.empty())
        return 0;

    std::sort(values.begin(), values.end());

    const size_t mid=values.size()/2;

    if(values.size()%2)
        return values[mid];

    return (values[mid-1]+values[mid])/2;
}

// min/median/max of both timing columns over the samples [first,end).
static void printTimingSummary(const char *label,
    const std::vector<FrameSample> &samples, size_t first)
{
    std::vector<long long> update, render;

    for(size_t i=first; i<samples.size(); ++i)
    {
        update.push_back(samples[i].updateUs);
        render.push_back(samples[i].renderUs);
    }

    if(update.empty())
    {
        std::printf("summary %s frames=0\n", label);
        return;
    }

    std::printf("summary %s frames=%zu"
        " update_us_min=%lld update_us_median=%lld update_us_max=%lld"
        " render_us_min=%lld render_us_median=%lld render_us_max=%lld\n",
        label, update.size(),
        *std::min_element(update.begin(), update.end()), medianOf(update),
        *std::max_element(update.begin(), update.end()),
        *std::min_element(render.begin(), render.end()), medianOf(render),
        *std::max_element(render.begin(), render.end()));
}

static void printUsage(std::ostream &out)
{
    out<<"Usage: OpenGLCore [--demo <Name>] [--screenshot <out.png>]"
         " [--frames <N>] [--stats]\n"
         "\n"
         "  --demo <Name>        show that demo page at startup\n"
         "  --screenshot <path>  render offscreen, write one PNG and exit\n"
         "  --frames <N>         render exactly N offscreen frames and exit;\n"
         "                       needs no display, and uses the same stepped\n"
         "                       1/60s clock as --screenshot\n"
         "  --stats              with --frames, print one line per frame plus a\n"
         "                       summary, to stdout\n"
         "\n"
         "Per-frame line fields:\n"
         "  frame      0-based frame index\n"
         "  controls   controls in the canvas tree this frame\n"
         "  changed    of those, how many had getStateChange() != Nothing,\n"
         "             counted between updateCanvas() and renderCanvas()\n"
         "  update_us  wall time of canvas->updateCanvas(), microseconds\n"
         "  render_us  wall time of canvas->renderCanvas(), microseconds\n"
         "  phase      warmup until the first frame with changed=0, then steady\n"
         "\n"
         "Summary lines give frames/controls, the warm-up frame count, and\n"
         "min/median/max of each timing over all frames and over the steady ones.\n";
}

int main(int argc, char *argv[])
{
    const gweni::Point screenSize(1024,768);

    gweni::capture::setToolName("OpenGLCore");

    std::string demoName;
    std::string shotPath;
    int frameCount=0;
    bool stats=false;

    for(int i=1; i<argc; ++i)
    {
        const std::string arg=argv[i];
        const bool hasValue=(i+1)<argc;

        if(arg == "--demo" && hasValue)
            demoName=argv[++i];
        else if(arg == "--screenshot" && hasValue)
            shotPath=argv[++i];
        else if(arg == "--frames" && hasValue)
        {
            frameCount=std::atoi(argv[++i]);

            if(frameCount<=0)
            {
                std::cerr<<"OpenGLCore: --frames needs a positive count\n";
                return EXIT_FAILURE;
            }
        }
        else if(arg == "--stats")
            stats=true;
        else if(arg == "--help" || arg == "-h")
        {
            printUsage(std::cout);
            return EXIT_SUCCESS;
        }
        else
        {
            std::cerr<<"OpenGLCore: unexpected argument "<<arg<<"\n";
            printUsage(std::cerr);
            return EXIT_FAILURE;
        }
    }

    const bool writingPng=!shotPath.empty();

    if(stats && frameCount == 0)
    {
        std::cerr<<"OpenGLCore: --stats needs --frames <N>\n";
        printUsage(std::cerr);
        return EXIT_FAILURE;
    }

    // Both a screenshot and a counted run render offscreen, so neither needs a
    // display; anything else is the interactive loop.
    const bool capturing=writingPng||(frameCount>0);

    if(writingPng && !gweni::capture::ensureDirectory(fs::path(shotPath).parent_path()))
        return EXIT_FAILURE;

    glfwSetErrorCallback(&glfwErrorCallback);

//    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
//    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
//    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
//    glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GL_TRUE);%

    // A capture does not need the window on screen, and on a machine with no
    // display at all it falls back to a surfaceless EGL context, so the demo can
    // be photographed in the build container as it stands.
    gweni::capture::Context ctx;

    if(!gweni::capture::initGlfwContext(ctx, screenSize,
        "Gweni OpenGL Core Profile Sample", !capturing))
    {
        if(!capturing)
        {
            std::cerr<<"OpenGLCore: GLFW could not open a window;"
                       " interactive mode needs a display\n";
            return EXIT_FAILURE;
        }
#if GWENI_EXAMPLE_EGL
        std::cerr<<"OpenGLCore: GLFW could not open a window"
                   " (no display?); falling back to surfaceless EGL\n";
        if(!gweni::capture::initEglContext(ctx))
            return EXIT_FAILURE;
#else
        return EXIT_FAILURE;
#endif
    }

//    glfwSwapInterval(0); //removes fps limiter

    if(!gweni::capture::initGlew())
    {
        gweni::capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

    gweni::capture::Framebuffer fb;

    if(capturing && !gweni::capture::createFramebuffer(fb, screenSize.x, screenSize.y))
    {
        gweni::capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }

//    gweni::platform::RelativeToExecutablePaths paths("../../resources/");
    gweni::platform::CurrentPath paths("resources");

    // Create a Gweni OpenGL Renderer
#ifdef USE_DEBUG_FONT
    gweni::renderer::OpenGL* renderer = new gweni::renderer::OpenGL_DebugFont();
#else
    gweni::renderer::OpenGLCore* renderer =
        new gweni::renderer::OpenGLCore(paths, gweni::Rect(gweni::Point(0,0), screenSize));
#endif
    renderer->init();

    // Create a Gweni skin
    gweni::skin::textured::TexturedSkin *skin = new gweni::skin::textured::TexturedSkin(renderer);

//    skin->init("DefaultSkin.png");
    skin->init("resources/DefaultSkin.json");
    skin->setDefaultFont("resources/OpenSans.ttf", 11);

    // Create a Canvas (it's root, on which all other Gweni panels are created)
    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the clear color is what actually shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;
    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, background.a/255.0f);

    gweni::controls::Canvas* canvas = new gweni::controls::Canvas(skin);
    canvas->setSize(screenSize.x, screenSize.y);
    canvas->setDrawBackground(true);
    canvas->setBackgroundColor(background);

    g_renderer=renderer;
    g_canvas=canvas;

//#define SINGLE_CONTROL

#ifndef SINGLE_CONTROL
    gweni::Demo *demo=canvas->newChild<gweni::Demo>();

    if(!demoName.empty() && !demo->showDemo(demoName))
    {
        std::cerr<<"OpenGLCore: no such demo \""<<demoName<<"\"\nAvailable demos:\n";
        for(const gweni::String &name : demo->getDemoNames())
            std::cerr<<"  "<<name<<"\n";

        delete demo;
        delete canvas;
        delete skin;
        delete renderer;
        if(capturing) gweni::capture::destroyFramebuffer(fb);
        gweni::capture::destroyContext(ctx);
        return EXIT_FAILURE;
    }
#else//SINGLE_CONTROL
    gweni::controls::Button *button=canvas->newChild<gweni::controls::Button>();

    button->setText("ButtonTest");
#endif//SINGLE_CONTROL

    int result=EXIT_SUCCESS;

    if(capturing)
    {
        // Everything time-dependent in the library reads this instead of the
        // platform clock, so the shot depends only on the frame number.
        gweni::platform::setTimeSource(&steppedClock);

        const int frames=(frameCount>0)?frameCount:g_captureFrames;

        std::vector<FrameSample> samples;
        bool steady=false;

        if(stats)
            samples.reserve(frames);

        for(int frame=0; frame<frames; ++frame)
        {
            typedef std::chrono::steady_clock Clock;

            const Clock::time_point updateBegin=Clock::now();
            canvas->updateCanvas();
            const Clock::time_point updateEnd=Clock::now();

            // Between the two phases: renderCanvas() clears the masks.
            FrameSample sample;

            if(stats)
                countChanges(canvas, sample.counts);

            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

            const Clock::time_point renderBegin=Clock::now();
            canvas->renderCanvas();
            const Clock::time_point renderEnd=Clock::now();

            if(stats)
            {
                using std::chrono::duration_cast;
                using std::chrono::microseconds;

                sample.updateUs=duration_cast<microseconds>(updateEnd-updateBegin).count();
                sample.renderUs=duration_cast<microseconds>(renderEnd-renderBegin).count();

                if(sample.counts.changed == 0)
                    steady=true;

                std::printf("frame=%d controls=%zu changed=%zu update_us=%lld"
                    " render_us=%lld phase=%s\n",
                    frame, sample.counts.controls, sample.counts.changed,
                    sample.updateUs, sample.renderUs,
                    steady?"steady":"warmup");
                std::fflush(stdout);

                samples.push_back(sample);
            }

            g_clockSeconds+=g_frameStep;
        }

        if(stats)
        {
            // The warm-up is the leading run of frames that still had something
            // to regenerate; the steady state after it is the measurement that
            // matters, so it is summarised on its own.
            size_t steadyFirst=0;

            while(steadyFirst<samples.size() && samples[steadyFirst].counts.changed != 0)
                ++steadyFirst;

            size_t steadyChangedMax=0;

            for(size_t i=steadyFirst; i<samples.size(); ++i)
                steadyChangedMax=std::max(steadyChangedMax, samples[i].counts.changed);

            std::printf("summary frames=%zu controls=%zu warmup_frames=%zu"
                " steady_frames=%zu steady_changed_max=%zu\n",
                samples.size(), samples.empty()?size_t(0):samples.back().counts.controls,
                steadyFirst, samples.size()-steadyFirst, steadyChangedMax);

            printTimingSummary("all", samples, 0);
            printTimingSummary("steady", samples, steadyFirst);
            std::fflush(stdout);
        }

        if(writingPng)
        {
            if(gweni::capture::writeFramebufferPng(screenSize.x, screenSize.y, shotPath))
                std::cout<<"wrote "<<shotPath<<"\n";
            else
                result=EXIT_FAILURE;
        }
    }
    else
    {
        g_glfwInput.initialize(canvas);

        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);

        // Apply the actual framebuffer size now (it can differ from the requested
        // window size, e.g. on high-DPI displays).
        {
            int fbWidth=0, fbHeight=0;
            glfwGetFramebufferSize(ctx.window, &fbWidth, &fbHeight);
            framebuffer_size_callback(ctx.window, fbWidth, fbHeight);
        }

        // Begin the main game loop
        while (!glfwWindowShouldClose(ctx.window))
        {
            canvas->updateCanvas();
            glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
            canvas->renderCanvas();
            glfwSwapBuffers(ctx.window);

            glfwPollEvents();

            gweni::platform::sleep(0);
        }
    }

#ifndef SINGLE_CONTROL
    delete demo;
#else//SINGLE_CONTROL
    delete button;
#endif//SINGLE_CONTROL

    delete canvas;
    delete skin;
    delete renderer;

    // Clean up OpenGL
    if(capturing)
        gweni::capture::destroyFramebuffer(fb);
    gweni::capture::destroyContext(ctx);

    return result;
}
