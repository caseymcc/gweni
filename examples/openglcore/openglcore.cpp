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
 *    --scale <s>          draw at s times the OS content scale (default 1, or
 *                         the GWENI_SCALE environment variable; the flag beats
 *                         it). The canvas is the framebuffer / scale in logical
 *                         units, so the UI is drawn s times as large
 *    --headless           with a capture, go straight to the surfaceless EGL
 *                         context instead of trying a GLFW window first
 *
 *  With no options it opens the interactive window as before; there F12 opens and
 *  closes the inspector (gweni::tools::Inspector) and F11 the theme editor
 *  (gweni::tools::ThemeEditor, saving to and loading from gweni-theme.json in
 *  the current directory). A capture needs no
 *  display: it hides the window, and with no display at all it falls back to a
 *  surfaceless EGL context (--headless skips the window, so the capture takes the
 *  same path whether or not a display exists). For example:
 *
 *    ./build_linux_x64_debug/examples/openglcore/OpenGLCore \
 *        --demo ColorPickerDemo --screenshot /tmp/hero.png
 *
 *  A capture installs a stepped clock (gweni::platform::setTimeSource), so the
 *  image depends on the frame number and not on how fast the process ran.
 *
 *  A missing resource directory, skin config (DefaultSkin.json) or default font
 *  is reported on stderr and drawn degraded: every surface magenta, the text in
 *  the library's built-in OpenSans. The window keeps running. A capture writes
 *  its image and exits 2 ("resources incomplete"); 1 is a failure, 0 a clean run.
 *
 *  See license in Gweni.h
 */

#include <gweni/platforms/renderers/glApi.h>
#include <gweni/platforms/renderers/slotRenderer.h>
#include <gweni/gweni.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/simple/simple.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/controls/canvas.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/themeEditor.h>
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
static float g_manualScale=1.0f;        // --scale or GWENI_SCALE

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // F12 opens and closes the inspector, F11 the theme editor; the library
    // binds no key
    if(key == GLFW_KEY_F12)
    {
        if((action == GLFW_PRESS) && g_canvas)
            gweni::tools::Inspector::toggle(g_canvas);
        return;
    }
    if(key == GLFW_KEY_F11)
    {
        if((action == GLFW_PRESS) && g_canvas && gweni::tools::ThemeEditor::toggle(g_canvas))
            gweni::tools::ThemeEditor::find(g_canvas)->setThemePath("gweni-theme.json");
        return;
    }

    const gweni::input::GLFW3::KeyEvent evt = { key, scancode, action, mods };
    g_glfwInput.processKeyEvent(evt);
}

static void char_callback(GLFWwindow* window, unsigned int codepoint)
{
    g_glfwInput.processCharEvent(codepoint);
}

static void window_focus_callback(GLFWwindow* window, int focused)
{
    g_glfwInput.processFocus(focused == GLFW_TRUE);
}

static void cursor_pos_callback(GLFWwindow* window, double xpos, double ypos)
{
    g_glfwInput.processMouseMove(xpos, ypos);
}

static void cursor_enter_callback(GLFWwindow* window, int entered)
{
    g_glfwInput.processCursorEnter(entered == GLFW_TRUE);
}

static void mouse_button_callback(GLFWwindow* window, int button, int action, int mods)
{
    g_glfwInput.processMouseButtons(button, action, mods);
}

static void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
    g_glfwInput.processScroll(xoffset, yoffset);
}

// The view is the framebuffer in device pixels and the canvas that in logical
// units, so docked controls re-layout to the new window size; the cursor is
// mapped through the same sizes and scale.
static void fitCanvas(GLFWwindow* window)
{
    if(g_renderer && g_canvas)
        gweni::capture::fitCanvasToWindow(window, g_manualScale, *g_renderer, *g_canvas, g_glfwInput);
}

static void framebuffer_size_callback(GLFWwindow* window, int /*width*/, int /*height*/)
{
    fitCanvas(window);
}

// The window moved to a monitor with another scale, or the desktop's changed.
static void content_scale_callback(GLFWwindow* window, float /*xscale*/, float /*yscale*/)
{
    fitCanvas(window);
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
         "                  [--scale <s>] [--headless] [--resources <dir>]\n"
         "                  [--renderer <gl|vulkan>]\n"
         "\n"
         "  --demo <Name>        show that demo page at startup\n"
         "  --screenshot <path>  render offscreen, write one PNG and exit\n"
         "  --frames <N>         render exactly N offscreen frames and exit;\n"
         "                       needs no display, and uses the same stepped\n"
         "                       1/60s clock as --screenshot\n"
         "  --stats              with --frames, print one line per frame plus a\n"
         "                       summary, to stdout\n"
         "  --scale <s>          draw at s device pixels per logical unit times\n"
         "                       the OS content scale (default 1, or\n"
         "                       GWENI_SCALE; the flag beats it). The canvas is\n"
         "                       the framebuffer / scale, so the UI is s times\n"
         "                       as large; an offscreen capture has a content\n"
         "                       scale of 1\n"
         "  --headless           force the surfaceless EGL context (no display\n"
         "                       needed); needs --screenshot or --frames\n"
         "  --resources <dir>    load the skin, fonts and textures from <dir>\n"
         "                       (relative to the working directory, or\n"
         "                       absolute) instead of the resources/ next to\n"
         "                       the executable\n"
       <<gweni::capture::RendererHelp<<
         "\n"
         "In the window, F12 opens and closes the inspector, and F11 the theme\n"
         "editor, which saves to and loads from gweni-theme.json in the current\n"
         "directory.\n"
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
         "min/median/max of each timing over all frames and over the steady ones.\n"
         "\n"
         "Exit status of --screenshot / --frames: 0 success, 1 failure, 2 written\n"
         "from incomplete resources (a missing resource dir, skin config or\n"
         "default font, each reported on stderr and drawn degraded: magenta\n"
         "surfaces, the built-in font).\n";
}

int main(int argc, char *argv[])
{
    const gweni::Point screenSize(1024,768);

    gweni::capture::setToolName("OpenGLCore");

    std::string demoName;
    std::string shotPath;
    std::string resourceDir;
    std::string rendererName;
    int frameCount=0;
    bool stats=false;
    bool scaleGiven=false;
    bool forceHeadless=false;

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
        else if(arg == "--scale" && hasValue)
        {
            if(!gweni::capture::parseScale(argv[++i], g_manualScale))
            {
                std::cerr<<"OpenGLCore: --scale wants a number in (0, 16], e.g. 1.5\n";
                return EXIT_FAILURE;
            }
            scaleGiven=true;
        }
        else if(arg == "--headless")
            forceHeadless=true;
        else if(arg == "--resources" && hasValue)
            resourceDir=argv[++i];
        else if(arg == "--renderer" && hasValue)
            rendererName=argv[++i];
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

    if(!gweni::capture::manualScale(scaleGiven, g_manualScale))
        return EXIT_FAILURE;

    gweni::capture::Backend backend;

    if(!gweni::capture::chooseBackend(rendererName, backend))
        return EXIT_FAILURE;

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

    if(forceHeadless && !capturing)
    {
        std::cerr<<"OpenGLCore: --headless has no window to show;"
                   " pass --screenshot or --frames\n";
        return EXIT_FAILURE;
    }

    if(writingPng && !gweni::capture::ensureDirectory(fs::path(shotPath).parent_path()))
        return EXIT_FAILURE;

    // Declared before the renderer, which keeps a reference to it. A missing
    // directory is reported here and the run carries on: what loads from it
    // degrades below.
    bool resourcesFound=true;
    const std::unique_ptr<gweni::ResourcePaths> paths=gweni::capture::resourcePaths(resourceDir, resourcesFound);

    gweni::capture::installGlfwErrorHandler();

    // A capture draws into an offscreen target and needs no display (GL falls
    // back to a surfaceless EGL context without one, Vulkan has no window at
    // all), so the demo can be photographed in the build container as it
    // stands; anything else is the window.
    gweni::capture::DeviceOptions deviceOptions;

    deviceOptions.size=screenSize;
    deviceOptions.title="Gweni OpenGL Core Profile Sample";
    deviceOptions.capturing=capturing;
    deviceOptions.headless=forceHeadless;

    std::unique_ptr<gweni::capture::Device> device=gweni::capture::openDevice(backend, deviceOptions);

    if(!device)
        return EXIT_FAILURE;

    const gweni::capture::Context &ctx=device->context();

    // Create a Gweni renderer: OpenGLCore, or renderer::Vulkan
    gweni::renderer::SlotRenderer *renderer=
        device->newRenderer(*paths, gweni::Rect(gweni::Point(0,0), screenSize));
    renderer->init();

    // Create a Gweni skin
    gweni::skin::textured::TexturedSkin *skin = new gweni::skin::textured::TexturedSkin(renderer);

    // A skin config or default font that does not load is reported by the
    // library and drawn degraded (every surface magenta; the built-in face, or no
    // text if that fails too). The window keeps running; a capture exits 2.
    bool degraded=!resourcesFound;

    if(!skin->init("DefaultSkin.json"))
        degraded=true;

    if(gweni::capture::loadDefaultFont(*renderer, skin->getDefaultFont()) != gweni::Font::Status::Loaded)
        degraded=true;

    // Create a Canvas (it's root, on which all other Gweni panels are created)
    // The canvas fill comes from the theme, so a "theme" block in
    // resources/DefaultSkin.json moves it too. Canvas::renderCanvas does not
    // currently paint its own background (the draw call there is commented
    // out), so the colour each frame clears the target to is what actually
    // shows.
    const gweni::Color background=gweni::skin::textured::tokyonight::Canvas.top;

    // The framebuffer is screenSize device pixels and the canvas that in logical
    // units. An offscreen capture has no OS content scale, so only the manual
    // factor counts; the window's own is applied once it is shown, below.
    gweni::controls::Canvas* canvas = new gweni::controls::Canvas(skin);
    const gweni::Point canvasSize=gweni::capture::logicalSize(screenSize, g_manualScale);
    canvas->setScale(g_manualScale);
    canvas->setSize(canvasSize.x, canvasSize.y);
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

            renderer->clearTarget(background);

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
            if(gweni::capture::writeRendererPng(*renderer, screenSize.x, screenSize.y, shotPath))
                std::cout<<"wrote "<<shotPath<<"\n";
            else
                result=EXIT_FAILURE;
        }
    }
    else
    {
        g_glfwInput.initialize(canvas);

        glfwSetKeyCallback(ctx.window, key_callback);
        glfwSetCharCallback(ctx.window, char_callback);
        glfwSetWindowFocusCallback(ctx.window, window_focus_callback);
        glfwSetCursorPosCallback(ctx.window, cursor_pos_callback);
        glfwSetCursorEnterCallback(ctx.window, cursor_enter_callback);
        glfwSetMouseButtonCallback(ctx.window, mouse_button_callback);
        glfwSetScrollCallback(ctx.window, scroll_callback);
        glfwSetFramebufferSizeCallback(ctx.window, framebuffer_size_callback);
        glfwSetWindowContentScaleCallback(ctx.window, content_scale_callback);

        // Apply the actual framebuffer size and content scale now (the
        // framebuffer differs from the window size on high-DPI displays).
        fitCanvas(ctx.window);

        // The main loop: one frame per displayed frame until the window closes
        // (in the browser, per animation frame, never returning). The frame
        // holds only the canvas and the renderer, heap objects, and the colour,
        // a copy, as the browser's loop needs.
        device->runWindowLoop([canvas, renderer, background]()
        {
            canvas->updateCanvas();
            renderer->clearTarget(background);
            canvas->renderCanvas();
        });
    }

#ifndef SINGLE_CONTROL
    delete demo;
#else//SINGLE_CONTROL
    delete button;
#endif//SINGLE_CONTROL

    delete canvas;
    delete skin;
    delete renderer;
    device.reset();

    return capturing?gweni::capture::exitStatus(result, degraded):result;
}
