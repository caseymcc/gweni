/*
 *  Gweni - the examples' command-line helpers over the app shell (capture.h).
 *
 *  See license in Gweni.h
 */

#include "capture.h"

#include <gweni/controls/canvas.h>
#include <gweni/platforms/platform.h>
#include <gweni/tools/depthView.h>
#include <gweni/tools/inspector.h>
#include <gweni/tools/statsOverlay.h>
#include <gweni/tools/themeEditor.h>

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <unordered_set>

namespace gweni
{
namespace capture
{

int exitStatus(int result, bool degraded)
{
    if(result != EXIT_SUCCESS || !degraded)
        return result;

    std::cerr<<app::toolName()<<": resources incomplete, exiting "<<ExitDegraded<<"\n";
    return ExitDegraded;
}

void installFixedProfileClock()
{
    static uint64_t reads=0;

    reads=0;
    platform::setProfileClock([]() { return (++reads)*FixedProfileStepMicros; });
}

bool onOffArgument(int argc, char *argv[], int &i)
{
    if(i+1 < argc)
    {
        const std::string value=argv[i+1];

        if(value == "on" || value == "off")
        {
            ++i;
            return value == "on";
        }
    }
    return true;
}

bool toolKey(controls::Canvas *canvas, int key, int action)
{
    if(((key != GLFW_KEY_F12) && (key != GLFW_KEY_F11) && (key != GLFW_KEY_F10)
        && (key != GLFW_KEY_F9)) || !canvas)
        return false;
    if(action != GLFW_PRESS)
        return true;
    if(key == GLFW_KEY_F9)
        tools::DepthView::toggle(canvas);
    else if(key == GLFW_KEY_F12)
        tools::Inspector::toggle(canvas);
    else if(key == GLFW_KEY_F10)
        tools::StatsOverlay::toggle(canvas);
    else if(tools::ThemeEditor::toggle(canvas))
        tools::ThemeEditor::find(canvas)->setThemePath("gweni-theme.json");
    return true;
}

void openDepthViewOnce(controls::Canvas &canvas)
{
    // by control id, never reused, so a canvas made at a deleted one's address
    // is opened too
    static std::unordered_set<size_t> opened;

    if(canvas.highestZIndex() <= 0 || opened.count(canvas.getControlId()))
        return;
    opened.insert(canvas.getControlId());
    if(tools::DepthView::open(&canvas))
        std::cout<<"the depth view is open; F9 or Escape closes it, +/- or Alt+wheel: z spacing, R reset\n";
}

#if GWENI_APP_WEBGL
namespace
{

struct PageDepthViewNotice: public event::Handler
{
    controls::Canvas *canvas=nullptr;

    void onOpenChanged(event::Info info)
    {
        if(info.control != canvas)
            return;
        EM_ASM({
            if(Module['gweniDepthViewChanged'])
                Module['gweniDepthViewChanged']($0 != 0);
        }, (int)info.integer);
    }

    void onSpacingChanged(event::Info info)
    {
        const tools::DepthView *view=(info.control == canvas)?tools::DepthView::find(canvas):nullptr;

        if(!view)
            return;
        EM_ASM({
            if(Module['gweniDepthViewSpacingChanged'])
                Module['gweniDepthViewSpacingChanged']($0);
        }, (double)view->spacingFactor());
    }
};

}//namespace

void tellPageOfDepthView(controls::Canvas *canvas)
{
    PageDepthViewNotice *notice=new PageDepthViewNotice;

    notice->canvas=canvas;
    tools::DepthView::onOpenChangedCaller.add(notice, &PageDepthViewNotice::onOpenChanged);
    tools::DepthView::onSpacingChangedCaller.add(notice, &PageDepthViewNotice::onSpacingChanged);
}

std::string depthViewSpacing(controls::Canvas *canvas, double factor)
{
    tools::DepthView *view=canvas?tools::DepthView::find(canvas):nullptr;

    if(!view)
        return "view=closed factor=0";
    if(factor > 0.0)
        view->setSpacingFactor((float)factor);

    char text[64];

    std::snprintf(text, sizeof(text), "view=open factor=%.6g", (double)view->spacingFactor());
    return text;
}
#endif

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
        std::cerr<<app::toolName()<<": GWENI_SCALE wants a number in (0, 16], e.g. 1.5, not '"
                 <<env<<"'\n";
        return false;
    }
    return true;
}

bool loopArgument(int argc, char **argv, int &i, platform::LoopOptions &options)
{
    if(i+1 >= argc || !platform::LoopOptions::parseMode(argv[i+1], options.mode))
    {
        std::cerr<<app::toolName()<<": --loop wants idle or continuous\n";
        return false;
    }
    ++i;
    return true;
}

const char *const LoopHelp=
    "  --loop <idle|continuous>  how the window draws: idle (the default) only\n"
    "                       when the canvas has work, sleeping in between;\n"
    "                       continuous every frame. GWENI_IDLE_VERIFY=1 draws\n"
    "                       the frames idle skips and fails one that writes\n";

bool chooseBackend(const std::string &flag, app::Backend &backend)
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
        backend=app::Backend::GL;
        return true;
    }
    if(name == "vulkan")
    {
#ifdef GWENI_HAS_VULKAN
        backend=app::Backend::Vulkan;
        return true;
#else
        std::cerr<<app::toolName()<<": "<<source<<(flag.empty() ? "=" : " ")<<"vulkan: this build has no Vulkan renderer"
                   " (configure with -DGWENI_RENDER_VULKAN=ON, as ./generate.sh does)\n";
        return false;
#endif
    }
    std::cerr<<app::toolName()<<": "<<source<<" wants gl or vulkan, not '"<<name<<"'\n";
    return false;
}

const char *const RendererHelp=
    "  --renderer <gl|vulkan>  the backend to draw with (default gl, or\n"
    "                       GWENI_RENDERER; the flag beats it): OpenGLCore on a\n"
    "                       GL context, or renderer::Vulkan on a Vulkan device\n"
    "                       (no GL or EGL context: a capture is always\n"
    "                       headless, a window has a Vulkan swapchain; with\n"
    "                       GWENI_VULKAN_VALIDATION=1 the validation layer is on\n"
    "                       and any message aborts)\n";

}//namespace capture
}//namespace gweni
