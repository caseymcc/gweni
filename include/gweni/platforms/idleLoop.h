/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef _gweni_platforms_idleLoop_h_
#define _gweni_platforms_idleLoop_h_

#include <cstdlib>
#include <cstring>
#include <functional>
#include <limits>
#include <string>

namespace gweni
{
namespace platform
{

//! How a window's loop draws: only when the canvas has work
//! (controls::Canvas::nextWorkTime()), waiting for events or the next
//! deadline in between, or every iteration, as a game loop does.
enum class LoopMode
{
    Idle,
    Continuous
};

struct LoopOptions
{
    LoopMode mode=LoopMode::Idle;
    //! Whenever the idle loop would skip a frame it draws one anyway and fails
    //! it (IdleLoop::failures()) if it wrote a slot or uploaded anything: a
    //! change the canvas did not report as work.
    bool verify=false;

    //! "idle" or "continuous"; false for anything else, leaving \p mode as is.
    static bool parseMode(const char *text, LoopMode &mode)
    {
        if(!text)
            return false;
        if(std::strcmp(text, "idle") == 0)
            mode=LoopMode::Idle;
        else if(std::strcmp(text, "continuous") == 0)
            mode=LoopMode::Continuous;
        else
            return false;
        return true;
    }

    //! Whether GWENI_IDLE_VERIFY=1 is set.
    static bool verifyFromEnvironment()
    {
        const char *value=std::getenv("GWENI_IDLE_VERIFY");
        return value && (std::strcmp(value, "1") == 0);
    }
};

//! The scheduling of a window loop with no window system in it, so a test
//! drives it with a fake clock and a fake wait (input::GlfwLoop is it on
//! GLFW). Each iterate() delivers what is pending (poll), then draws a frame
//! if the canvas's work is due (nextWorkTime() at or before now()), and
//! otherwise blocks in wait() until that time (+infinity: until an event),
//! drawing nothing: an event that changed nothing makes the next iterate()
//! wait again. In LoopMode::Continuous every iterate() polls and draws. A
//! web page has no wait: tick() polls and draws only if work is due, and its
//! caller comes back on the next animation frame.
class IdleLoop
{
public:
    struct Hooks
    {
        //! platform::getTimeInSeconds()
        std::function<double()> now;
        //! controls::Canvas::nextWorkTime()
        std::function<double()> nextWorkTime;
        //! delivers pending events without blocking (glfwPollEvents())
        std::function<void()> poll;
        //! blocks until an event, the wake handler or \a seconds passing
        //! (+infinity: no timeout), delivering what comes
        std::function<void(double seconds)> wait;
        //! update, render and present one frame
        std::function<void()> drawFrame;
        //! whether a frame can be drawn now (false while a window is
        //! iconified: the loop waits for an event instead); empty is always
        std::function<bool()> canDraw;
        //! after a verify frame: whether it wrote and uploaded nothing,
        //! \a why otherwise
        std::function<bool(std::string &why)> frameWasIdle;
        //! a verify frame that was not idle, with its number and why;
        //! empty prints nothing
        std::function<void(size_t frame, const std::string &why)> verifyFailed;
    };

    IdleLoop(Hooks hooks, LoopOptions options): m_hooks(std::move(hooks)), m_options(options) {}

    //! One iteration of a native loop.
    void iterate()
    {
        if(m_hooks.poll)
            m_hooks.poll();

        if(m_options.mode == LoopMode::Continuous)
        {
            draw();
            return;
        }
        if(m_hooks.canDraw && !m_hooks.canDraw())
        {
            ++m_waits;
            m_hooks.wait(std::numeric_limits<double>::infinity());
            return;
        }

        const double now=m_hooks.now();
        const double due=m_hooks.nextWorkTime();

        if(due <= now)
        {
            draw();
            return;
        }

        if(m_options.verify)
            verifyFrame();
        ++m_waits;
        m_hooks.wait(due-now);
    }

    //! One animation-frame callback of a web loop: draws when work is due (or
    //! frameWanted() was set), else returns at once; a verify loop draws a
    //! verify frame instead.
    void tick()
    {
        ++m_ticks;
        if(m_hooks.poll)
            m_hooks.poll();

        if(m_options.mode == LoopMode::Continuous)
        {
            draw();
            return;
        }

        const bool wanted=m_frameWanted;

        m_frameWanted=false;
        if(wanted || (m_hooks.nextWorkTime() <= m_hooks.now()))
        {
            draw();
            return;
        }
        if(m_options.verify)
            verifyFrame();
    }

    //! The web loop's wake: the next tick() draws.
    void wantFrame() { m_frameWanted=true; }

    //! Frames drawn because there was work (or every one, continuous).
    size_t frames() const { return m_frames; }
    //! Frames drawn by the verify mode where the loop would have drawn none.
    size_t verifyFrames() const { return m_verifyFrames; }
    //! Verify frames that wrote or uploaded something.
    size_t failures() const { return m_failures; }
    //! Times iterate() waited.
    size_t waits() const { return m_waits; }
    //! tick() calls.
    size_t ticks() const { return m_ticks; }
    const LoopOptions &options() const { return m_options; }

private:
    void draw()
    {
        ++m_frames;
        m_hooks.drawFrame();
    }

    void verifyFrame()
    {
        ++m_verifyFrames;
        m_hooks.drawFrame();

        std::string why;

        if(!m_hooks.frameWasIdle || m_hooks.frameWasIdle(why))
            return;
        ++m_failures;
        if(m_hooks.verifyFailed)
            m_hooks.verifyFailed(m_frames+m_verifyFrames, why);
    }

    Hooks m_hooks;
    LoopOptions m_options;
    bool m_frameWanted=false;
    size_t m_frames=0;
    size_t m_verifyFrames=0;
    size_t m_failures=0;
    size_t m_waits=0;
    size_t m_ticks=0;
};

}//namespace platform
}//namespace gweni

#endif//_gweni_platforms_idleLoop_h_
