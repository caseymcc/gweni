/*
 *  Gweni - a gweni canvas rendered headless (gweni_app, gweni::app): a
 *  Shell's parts on surfaceless EGL (WebGL 2 in a page) or a headless Vulkan
 *  device, drawn into the offscreen target, driven by script steps as
 *  ControlTest's flags drive it (the same synthesised events through
 *  input::GLFW3, the same key names, app/keys.h) on a stepped clock, and read
 *  back as pixels, a PNG and the frame's counters (app/frameStats.h).
 *
 *  The clock is the library's (platform::setTimeSource) while the Headless
 *  lives: steps*(1.0/60) in double, from 0 for each instance, one step after
 *  each frame and 60 before each press (a second, so two presses are never a
 *  double-click by accident). It opens and closes with the input handler's
 *  pointer state forgotten (input::resetPointerState()).
 *
 *  One app::Headless (or app::Window) is alive at a time in a process: a
 *  second is refused (ok() false, error() saying so); one after another works.
 *
 *  See license in Gweni.h
 */
#ifndef _gweni_app_headless_h_
#define _gweni_app_headless_h_

#include <gweni/app/device.h>
#include <gweni/app/frameStats.h>
#include <gweni/app/shell.h>
#include <gweni/platforms/input/glfw3.h>
#include <gweni/platforms/platformTypes.h>

#include <cstdint>
#include <string>
#include <vector>

namespace gweni
{
namespace app
{

struct HeadlessOptions
{
    //! the canvas, in logical units
    Point size=Point(640, 480);
    //! device px per logical unit: the target is size times it, snapped
    float scale=1.0f;
    //! the resource directory, as ShellOptions::resources
    std::string resources;
    Backend backend=Backend::GL;
    //! what stats() calls the control (its control= field)
    std::string label="Headless";
};

class Headless
{
public:
    explicit Headless(const HeadlessOptions &options=HeadlessOptions());
    Headless(const Point &size, float scale=1.0f, const std::string &resources=std::string());
    ~Headless();

    Headless(const Headless &)=delete;
    Headless &operator=(const Headless &)=delete;

    //! Whether it opened: false when another Window or Headless is alive or
    //! the device did not open (error() says which, and its line is on stderr).
    bool ok() const { return m_ok; }
    const std::string &error() const { return m_error; }
    //! a resource did not load (Shell::degraded)
    bool degraded() const { return m_shell.degraded; }

    Shell &shell() { return m_shell; }
    controls::Canvas *canvas() const;
    renderer::SlotRenderer *renderer() const;
    skin::textured::TexturedSkin *skin() const;
    input::GLFW3 &input() { return m_input; }
    //! the target in device px
    Point pixelSize() const { return m_pixels; }
    //! the clock's seconds now
    double seconds() const;
    //! frames drawn so far
    int frameCount() const { return m_frames; }

    //! \p count frames: the update, the clear to the theme's canvas fill, the
    //! render, the counters kept for stats(), the clock a step on.
    void frame(int count=1);

    //! The mouse to (x, y) in logical units.
    void move(int x, int y);
    //! A button (0 left, 1 right) pressed at (x, y): the clock 60 steps on,
    //! the mouse there, the press through input::GLFW3.
    void press(int x, int y, int button=0);
    //! A button released at (x, y).
    void release(int x, int y, int button=0);
    //! press() and release().
    void click(int x, int y, int button=0);
    //! A wheel of \p delta (60 a notch, down negative) on the vertical or the
    //! horizontal axis. \return whether the UI took it.
    bool wheel(int delta);
    bool wheelH(int delta);
    //! A key or chord as ControlTest's --key takes it ("return", "ctrl+a",
    //! "x"): a printable character typed, else key events through
    //! input::GLFW3. \return false, after one line, for a name it does not
    //! know.
    bool key(const std::string &chord);
    //! UTF-8 text typed, one codepoint at a time through the char path.
    void type(const std::string &text);

    //! The target as top-down RGBA8 rows (pixelSize() large).
    std::vector<uint8_t> pixels();
    //! The target as a PNG. \return false, after one line, if not written or
    //! before any frame.
    bool savePng(const std::string &path);

    //! The last frame's counters, as ControlTest --stats prints them (empty
    //! before the first frame).
    const StatFields &stats() const { return m_stats; }

private:
    void open(const HeadlessOptions &options);

    HeadlessOptions m_options;
    Shell m_shell;
    input::GLFW3 m_input;
    Point m_pixels;
    Point m_mouse=Point(-1, -1);
    Color m_background;
    int64_t m_clockSteps=0;
    int m_frames=0;
    size_t m_measuresAtFrame=0;
    size_t m_invalidationsAtFrame=0;
    StatFields m_stats;
    bool m_ok=false;
    bool m_claimed=false;
    std::string m_error;
};

}//namespace app
}//namespace gweni

#endif//_gweni_app_headless_h_
