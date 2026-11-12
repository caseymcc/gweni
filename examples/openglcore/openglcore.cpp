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
 *    --stats-overlay [on|off]  open the stats overlay (gweni::tools::StatsOverlay)
 *                         at start-up, in the window or a capture; a later
 *                         --stats-overlay off overrides an earlier one
 *    --depth-view [on|off]  open the depth view (gweni::tools::DepthView) once
 *                         the first frame has numbered the z labels, in the
 *                         window or a capture; a later --depth-view off
 *                         overrides an earlier one
 *
 *  With no options it opens the interactive window as before. There the shown
 *  page is in the URL (platform::pushUrl): picking one in the demo list pushes
 *  ?demo=<Name>, keeping the query's other keys, and back and forward (the
 *  browser's on the web) show the page the URL names; the page last shown is
 *  stored (platform::storageSet: localStorage on the web, elsewhere
 *  $XDG_STATE_HOME/gweni/OpenGLCore.json, or ~/.local/state/...) and shown
 *  again at start-up when --demo names none; the window title is
 *  "gweni — <Name>". A capture does none of it, so what it draws never depends
 *  on what an earlier run stored. F12 opens and
 *  closes the inspector (gweni::tools::Inspector), F11 the theme editor
 *  (gweni::tools::ThemeEditor, saving to and loading from gweni-theme.json in
 *  the current directory), F10 the stats overlay and F9 the depth view, which
 *  takes the mouse and the keys for its camera while open (left-drag orbits,
 *  right-drag pans, the wheel dollies, P switches the projection, Escape
 *  closes it; see gweni/tools/depthView.h). A capture needs no display: it
 *  hides the window, and with no display at all it falls back to a
 *  surfaceless EGL context (--headless skips the window, so the capture takes the
 *  same path whether or not a display exists). For example:
 *
 *    ./build_linux_x64_debug/examples/openglcore/OpenGLCore \
 *        --demo ColorPickerDemo --screenshot /tmp/hero.png
 *
 *  A capture installs a stepped clock (gweni::platform::setTimeSource), so the
 *  image depends on the frame number and not on how fast the process ran, and a
 *  fixed profile clock (gweni::capture::installFixedProfileClock), so the frame
 *  times the library records are the same in every run too.
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
#include <gweni/tools/depthView.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>
#include <gweni/tools/themeEditor.h>
#include <iostream>

#include <demo/demo.h>

#include <GLFW/glfw3.h>

#include "capture.h"
#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <optional>
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

static gweni::Demo *g_demo=nullptr;

//! The window's shell: its page in the URL and the title, the page last shown
//! in the storage. Not for a capture, which reads and writes none of it.
namespace navigation
{

static const char *const StoredPageKey="last-demo";
static GLFWwindow *g_window=nullptr;
static bool g_storage=false;
//! What a URL naming no page shows: the page shown at start-up, else the first.
static std::string g_startDemo;
//! The page pageShown() was last told of.
static std::string g_shownDemo;

//! The current URL with demo=<name> in its query (the first demo key, the
//! others dropped), the query's other keys and the hash kept as they are.
static std::string urlForDemo(const std::string &name)
{
    namespace platform=gweni::platform;

    const platform::Url url=platform::parseUrl(platform::currentUrl());
    platform::QueryParams params;
    bool found=false;

    for(const auto &param:platform::parseQuery(url.query))
    {
        if(param.first != "demo")
            params.push_back(param);
        else if(!found)
        {
            params.emplace_back("demo", name);
            found=true;
        }
    }
    if(!found)
        params.emplace_back("demo", name);
    return "?"+platform::buildQuery(params)+(url.hash.empty()?"":"#"+url.hash);
}

//! The page \p name is shown: the window's title and the stored page.
static void announce(const std::string &name)
{
    g_shownDemo=name;
    if(g_window)
        glfwSetWindowTitle(g_window, ("gweni \xe2\x80\x94 "+name).c_str());
    if(g_storage)
        gweni::platform::storageSet(StoredPageKey, name);
}

//! Demo::setPageShownHandler: the user's pick is a new history entry, a page
//! the app showed (--demo, a stored page, back or forward) replaces the
//! current one, so restoring a page never grows the history. Picking the page
//! already shown replaces too, or Back would first go to that page again.
static void pageShown(const std::string &name, bool picked)
{
    const std::string url=urlForDemo(name);
    const bool again=name == g_shownDemo;

    if(picked && !again)
        gweni::platform::pushUrl(url);
    else
        gweni::platform::replaceUrl(url);
    announce(name);
}

//! platform::setUrlChangedHandler: back or forward, outside any frame. The
//! page the URL names, or the start page when it names none.
static void urlChanged(const std::string &url)
{
    std::string name=g_startDemo;

    for(const auto &param:gweni::platform::parseQuery(gweni::platform::parseUrl(url).query))
    {
        if(param.first == "demo")
        {
            name=param.second;
            break;
        }
    }
    if(g_demo && !name.empty() && name != g_demo->shownDemo())
        g_demo->showDemo(name);
}

//! Where the window keeps its storage: localStorage on the web (the prefix
//! keeps it apart from other apps on the origin), else a file in the user's
//! state directory. \return False if there is none to keep it in.
static bool openStorage()
{
#if GWENI_EXAMPLE_WEBGL
    gweni::platform::setStoragePrefix("gweni-demo/");
    return true;
#else
    std::string dir;
#if defined(_WIN32)
    if(const char *local=std::getenv("LOCALAPPDATA"))
        dir=std::string(local)+"\\gweni";
#else
    const char *state=std::getenv("XDG_STATE_HOME");
    const char *home=std::getenv("HOME");

    if(state && state[0] == '/')
        dir=std::string(state)+"/gweni";
    else if(home && home[0])
        dir=std::string(home)+"/.local/state/gweni";
#endif
    if(dir.empty() || !gweni::capture::ensureDirectory(fs::path(dir)))
        return false;
    gweni::platform::setStorageFile((fs::path(dir)/"OpenGLCore.json").string());
    return true;
#endif
}

//! Set up the window's navigation before its first page is shown. \return
//! The page to show at start-up: --demo's, else the stored one (if it is still
//! a page), else none.
static std::string open(GLFWwindow *window, gweni::Demo &demo, const std::string &demoName)
{
    g_window=window;
    g_storage=openStorage();

    std::string start=demoName;

    if(start.empty() && g_storage)
    {
        const std::optional<std::string> stored=gweni::platform::storageGet(StoredPageKey);
        const std::vector<std::string> names=demo.getDemoNames();

        if(stored && std::find(names.begin(), names.end(), *stored) != names.end())
            start=*stored;
    }
    g_startDemo=start.empty()?demo.shownDemo():start;

    demo.setPageShownHandler(&pageShown);
    gweni::platform::setUrlChangedHandler(&urlChanged);
    // With no page to show, the one the shell is built showing is the page
    // shown (a pick of it pushes nothing), titled and stored. Its URL stays as
    // it is: one naming no page already means the start page (urlChanged).
    if(start.empty() && !demo.shownDemo().empty())
        announce(demo.shownDemo());
    return start;
}

}//namespace navigation

#if GWENI_EXAMPLE_WEBGL

//! For a script driving the page (tests/web/interactive.mjs, through
//! Module.ccall(name, 'string')): whether the canvas's stats overlay is open,
//! whether the shell's Stats button is on, and the button's rect on the canvas
//! in logical units, as "overlay=open button=on rect=x,y,w,h". Valid until the
//! next call.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_stats_overlay()
{
    static std::string state;

    state.clear();
    if(g_canvas && g_demo && g_demo->statsButton())
    {
        gweni::controls::Button *button=g_demo->statsButton();
        const gweni::Point at=button->localPosToCanvas();

        state=std::string("overlay=")+(gweni::tools::StatsOverlay::find(g_canvas)?"open":"closed")
            +" button="+(button->getToggleState()?"on":"off")
            +" rect="+std::to_string(at.x)+","+std::to_string(at.y)+","
            +std::to_string(button->getWidth())+","+std::to_string(button->getHeight());
    }
    return state.c_str();
}

//! The same for the depth view and the shell's Depth button, after opening the
//! view (\p on 1) or closing it (0); any other value leaves it as it is:
//! "view=open button=on rect=x,y,w,h".
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_depth_view(int on)
{
    using gweni::tools::DepthView;

    static std::string state;

    state.clear();
    if(g_canvas && g_demo && g_demo->depthButton())
    {
        if(on == 1 && !DepthView::find(g_canvas))
            DepthView::open(g_canvas);
        else if(on == 0 && DepthView::find(g_canvas))
            DepthView::close(g_canvas);

        gweni::controls::Button *button=g_demo->depthButton();
        const gweni::Point at=button->localPosToCanvas();

        state=std::string("view=")+(DepthView::find(g_canvas)?"open":"closed")
            +" button="+(button->getToggleState()?"on":"off")
            +" rect="+std::to_string(at.x)+","+std::to_string(at.y)+","
            +std::to_string(button->getWidth())+","+std::to_string(button->getHeight());
    }
    return state.c_str();
}

//! The shell's navigation, for tests/web/navigation.mjs: the page shown, the
//! status bar link's rect on the canvas (logical units) and currentUrl(), as
//! "demo=<Name> link=x,y,w,h url=<url>". Valid until the next call.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_navigation()
{
    static std::string state;

    state.clear();
    if(g_demo && g_demo->link())
    {
        gweni::controls::Button *link=g_demo->link();
        const gweni::Point at=link->localPosToCanvas();

        state="demo="+g_demo->shownDemo()
            +" link="+std::to_string(at.x)+","+std::to_string(at.y)+","
            +std::to_string(link->getWidth())+","+std::to_string(link->getHeight())
            +" url="+gweni::platform::currentUrl();
    }
    return state.c_str();
}

//! The demo list entry of the page \p name: its rect on the canvas as
//! "x,y,w,h", empty if there is none or it is not shown.
extern "C" EMSCRIPTEN_KEEPALIVE const char *gweni_web_demo_entry(const char *name)
{
    static std::string state;

    state.clear();
    if(g_demo && name)
    {
        gweni::controls::Button *entry=g_demo->demoEntry(name);

        if(entry && entry->visible())
        {
            const gweni::Point at=entry->localPosToCanvas();

            state=std::to_string(at.x)+","+std::to_string(at.y)+","
                +std::to_string(entry->getWidth())+","+std::to_string(entry->getHeight());
        }
    }
    return state.c_str();
}
#endif//GWENI_EXAMPLE_WEBGL

static void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    // F12 opens and closes the inspector, F11 the theme editor, F10 the stats
    // overlay, F9 the depth view; the library binds no key. They come before
    // the canvas, so they work while the depth view takes its input.
    if(key == GLFW_KEY_F9)
    {
        if((action == GLFW_PRESS) && g_canvas)
            gweni::tools::DepthView::toggle(g_canvas);
        return;
    }
    if(key == GLFW_KEY_F10)
    {
        if((action == GLFW_PRESS) && g_canvas)
            gweni::tools::StatsOverlay::toggle(g_canvas);
        return;
    }
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
// same times. 1/60 s is the step, i.e. a nominal 60fps. It counts whole steps
// and reads as steps*(1.0/60) in double.
static const double g_frameStep=1.0/60.0;
static int64_t g_clockSteps=0;

static double steppedClock()
{
    return double(g_clockSteps)*g_frameStep;
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
         "                  [--renderer <gl|vulkan>] [--stats-overlay [on|off]]\n"
         "                  [--depth-view [on|off]]\n"
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
         "  --stats-overlay [on|off]  open the stats overlay at start-up, in the\n"
         "                       window or a capture (on if neither follows);\n"
         "                       a later --stats-overlay off overrides it\n"
         "  --depth-view [on|off]  open the depth view once the first frame has\n"
         "                       numbered the z labels, in the window or a\n"
         "                       capture (on if neither follows); a later\n"
         "                       --depth-view off overrides it\n"
       <<gweni::capture::RendererHelp<<
         "\n"
         "In the window, F12 opens and closes the inspector, F11 the theme\n"
         "editor, which saves to and loads from gweni-theme.json in the current\n"
         "directory, F10 the stats overlay and F9 the depth view: the UI seen\n"
         "in 3D along its z, which takes the mouse and keys while open\n"
         "(left-drag orbits, right- or middle-drag pans, the wheel and W/S\n"
         "dolly, the arrows and A/D move, P switches perspective and\n"
         "orthographic, + and - space the z labels, R resets, Escape or F9\n"
         "closes it).\n"
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
    bool statsOverlay=false;
    bool depthView=false;

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
        else if(arg == "--stats-overlay")
            statsOverlay=gweni::capture::onOffArgument(argc, argv, i);
        else if(arg == "--depth-view")
            depthView=gweni::capture::onOffArgument(argc, argv, i);
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
    g_demo=demo;

    // the window's page comes from --demo, else from the storage
    const std::string startDemo=capturing?demoName:navigation::open(ctx.window, *demo, demoName);

    if(!startDemo.empty() && !demo->showDemo(startDemo))
    {
        std::cerr<<"OpenGLCore: no such demo \""<<startDemo<<"\"\nAvailable demos:\n";
        for(const gweni::String &name : demo->getDemoNames())
            std::cerr<<"  "<<name<<"\n";

        gweni::platform::setUrlChangedHandler(nullptr);
        g_demo=nullptr;
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

    if(statsOverlay)
    {
        gweni::tools::StatsOverlay::open(canvas);
        // one line, so a page (and tests/web/site.mjs) can tell it was asked for
        std::cout<<"OpenGLCore: the stats overlay is open; F10 toggles it\n";
    }

    int result=EXIT_SUCCESS;

    if(capturing)
    {
        // Everything time-dependent in the library reads this instead of the
        // platform clock, so the shot depends only on the frame number.
        gweni::platform::setTimeSource(&steppedClock);
        // and the library's frame times count clock reads instead
        gweni::capture::installFixedProfileClock();

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

            if(depthView)
                gweni::capture::openDepthViewOnce(*canvas);

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

            ++g_clockSteps;
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
        device->runWindowLoop([canvas, renderer, background, depthView]()
        {
            canvas->updateCanvas();
            if(depthView)
                gweni::capture::openDepthViewOnce(*canvas);
            renderer->clearTarget(background);
            canvas->renderCanvas();
        });
    }

#ifndef SINGLE_CONTROL
    gweni::platform::setUrlChangedHandler(nullptr);
    g_demo=nullptr;
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
