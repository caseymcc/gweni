/*
 *  Gweni - what the examples' command lines and their tests add to the app
 *  shell (gweni_app, gweni/app/device.h): --scale, --loop, --renderer and the
 *  on|off switches, the exit status of a degraded capture, the fixed profile
 *  clock, --depth-view, and the page's depth-view notices in the browser.
 *  OpenGLCore, ControlTest, HudExample, gweni_bench and the GL unit tests
 *  include it (target gweni_capture, over gweni_app).
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_example_capture_h_
#define _gweni_example_capture_h_

#include <gweni/app/device.h>
#include <cstdint>
#include <string>

namespace gweni
{
namespace controls
{
class Canvas;
}

namespace capture
{

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

//! A switch taking an optional on|off, at argv[i] (--stats-overlay, whether
//! gweni::tools::StatsOverlay is open at start-up, and --depth-view): on, unless
//! the word after it is "off"; an "on" or "off" there is taken (i moves past
//! it), so a later --stats-overlay off overrides an earlier one.
bool onOffArgument(int argc, char *argv[], int &i);

//! The examples' tool keys, for app::Window's keyFirst hook: F12 opens and
//! closes the inspector, F11 the theme editor (saving to and loading from
//! gweni-theme.json in the working directory), F10 the stats overlay, F9 the
//! depth view (the library binds no key). Coming before the canvas, they work
//! while the depth view takes its input. \return true for one of them, press
//! or release, which the canvas is then not given.
bool toolKey(controls::Canvas *canvas, int key, int action);

//! --depth-view: opens the canvas's depth view (gweni::tools::DepthView) the
//! first time it is called for that canvas once its z labels are numbered
//! (Canvas::highestZIndex(), after an updateCanvas()), so the view's default
//! spacing fits them; call it after each frame's updateCanvas(), before the
//! render. A canvas whose view it opened is not opened again, so Escape or F9
//! closes it for good.
void openDepthViewOnce(controls::Canvas &canvas);

#if GWENI_APP_WEBGL
//! Tells the page of \p canvas's depth view from now on, each call only if the
//! page set it: Module.gweniDepthViewChanged(open) as the view opens or closes
//! (F9, Escape, the shell's Depth button, --depth-view, an export), and
//! Module.gweniDepthViewSpacingChanged(factor) as its spacing factor
//! (DepthView::spacingFactor()) changes, R and the open included, so a page's
//! controls follow the view. Call once per canvas; never undone (the window
//! loop does not return in the browser).
void tellPageOfDepthView(controls::Canvas *canvas);
//! gweni_web_depth_view_spacing's work, in both apps: \p factor > 0 sets the
//! open view's spacing factor (clamped to x0.1..x20, the front layer kept),
//! anything else only reads; returns "view=open factor=<f>" or
//! "view=closed factor=0", after
std::string depthViewSpacing(controls::Canvas *canvas, double factor);
#endif

//-----------------------------------------------------------------------------
// Scale: the manual factor (--scale, GWENI_SCALE), which app::effectiveScale
// multiplies the window's content scale by
//-----------------------------------------------------------------------------

//! A scale given as text: a number in (0, 16]. \return false otherwise.
bool parseScale(const char *value, float &scale);

//! The manual factor: \a scale as --scale left it when \a given, otherwise
//! GWENI_SCALE when it is set, otherwise 1 (the flag beats the environment).
//! \return false, after one line naming the value, if GWENI_SCALE is not a
//! scale.
bool manualScale(bool given, float &scale);

//-----------------------------------------------------------------------------
// The window loop and the backend, as the command lines choose them
//-----------------------------------------------------------------------------

//! Reads --loop's value, argv[i+1], into \a options: idle or continuous.
//! \return false, after one line naming the tool, for another or none.
bool loopArgument(int argc, char **argv, int &i, platform::LoopOptions &options);
//! The --help text of --loop, for each tool's usage
extern const char *const LoopHelp;

//! The backend a run draws with: \a flag (--renderer's value) when it is not
//! empty, otherwise GWENI_RENDERER when it is set, otherwise gl. \return false,
//! after one line, for a name that is neither, or vulkan in a tree built without
//! it.
bool chooseBackend(const std::string &flag, app::Backend &backend);

//! The --help text of --renderer, for each tool's usage
extern const char *const RendererHelp;

}//namespace capture
}//namespace gweni

#endif//_gweni_example_capture_h_
