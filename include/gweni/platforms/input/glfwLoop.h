/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#ifndef GWENI_INPUT_GLFW_LOOP_H
#define GWENI_INPUT_GLFW_LOOP_H

#include <gweni/controls/canvas.h>
#include <gweni/platforms/baseRender.h>
#include <gweni/platforms/idleLoop.h>
#include <gweni/platforms/platform.h>
#include <GLFW/glfw3.h>

#if defined(__EMSCRIPTEN__)
#include <emscripten.h>
#endif

#include <cmath>
#include <cstdio>
#include <functional>
#include <string>
#include <utility>

namespace gweni
{
namespace input
{

//! A GLFW window's loop over a canvas, an optional helper beside GLFW3 (the
//! core never links GLFW): platform::IdleLoop's scheduling with GLFW's events.
//! Natively, idle, it draws a frame only when the canvas has work
//! (Canvas::nextWorkTime()) and otherwise blocks in glfwWaitEventsTimeout()
//! until the next deadline, or glfwWaitEvents() with none; the canvas's wake
//! handler is glfwPostEmptyEvent(), so a post from another thread or an earlier
//! timer ends the wait. A frame not drawn is not swapped either, which is safe
//! with a SlotRenderer: every frame it draws draws the whole mirror. In the
//! browser it keeps emscripten_set_main_loop_arg at the browser's rate, each
//! callback polling and returning at once while there is no work (the wake
//! handler sets a frame-wanted flag); it never pauses the main loop, which a
//! missed wake would leave frozen. LoopMode::Continuous draws every iteration.
//! With LoopOptions::verify, a frame the loop would have skipped is drawn and
//! fails (stderr, IdleLoop::failures()) if the renderer's frameStats() show a
//! slot written or an upload.
class GlfwLoop
{
public:
    struct Callbacks
    {
        //! update, render and present one frame (on GL with the swap)
        std::function<void()> drawFrame;
        //! whether a frame can be drawn now (a Vulkan window's iconified one
        //! cannot: the loop waits for an event); empty is always
        std::function<bool()> canDraw;
        //! after every iteration (natively) or callback (in the browser)
        std::function<void(const platform::IdleLoop &)> afterIteration;
    };

    GlfwLoop(GLFWwindow *window, controls::Canvas *canvas, renderer::Base *renderer,
        platform::LoopOptions options, Callbacks callbacks)
        : m_window(window)
        , m_canvas(canvas)
        , m_renderer(renderer)
        , m_callbacks(std::move(callbacks))
        , m_loop(hooks(), options)
    {
#if defined(__EMSCRIPTEN__)
        // one thread: the flag the next callback reads
        m_canvas->setWakeHandler([this]() { m_loop.wantFrame(); });
#else
        // GLFW's one call that is safe from any thread
        m_canvas->setWakeHandler([]() { glfwPostEmptyEvent(); });
#endif
    }

    ~GlfwLoop()
    {
        m_canvas->setWakeHandler(nullptr);
    }

    GlfwLoop(const GlfwLoop &)=delete;
    GlfwLoop &operator=(const GlfwLoop &)=delete;

    //! Natively, iterates until the window should close. In the browser it
    //! starts the main loop and never returns (main's stack is unwound): make
    //! the loop with new and let it live as long as the page.
    void run()
    {
#if defined(__EMSCRIPTEN__)
        // the browser closes the page, not the window
        (void)m_window;
        emscripten_set_main_loop_arg(&GlfwLoop::webCallback, this, 0, 1);
#else
        while(!glfwWindowShouldClose(m_window))
        {
            m_loop.iterate();
            if(m_callbacks.afterIteration)
                m_callbacks.afterIteration(m_loop);
        }
#endif
    }

    platform::IdleLoop &loop() { return m_loop; }
    const platform::IdleLoop &loop() const { return m_loop; }

    //! What a verify frame failed for: the slots it wrote and what it
    //! uploaded, or empty when it wrote and uploaded nothing (or the renderer
    //! counts nothing).
    static std::string frameWrites(const renderer::FrameCounts &counts)
    {
        if(!counts.counted)
            return std::string();
        if(counts.primitiveSlotsWritten == 0 && counts.textSlotsWritten == 0 && counts.uploadCalls == 0)
            return std::string();
        return "wrote "+std::to_string(counts.primitiveSlotsWritten)+" primitive and "
            +std::to_string(counts.textSlotsWritten)+" glyph slots and uploaded "
            +std::to_string(counts.uploadBytes)+" bytes in "+std::to_string(counts.uploadCalls)+" calls";
    }

private:
    platform::IdleLoop::Hooks hooks()
    {
        platform::IdleLoop::Hooks hooks;

        hooks.now=[]() { return platform::getTimeInSeconds(); };
        hooks.nextWorkTime=[this]() { return m_canvas->nextWorkTime(); };
        hooks.poll=[]() { glfwPollEvents(); };
        hooks.wait=[](double seconds) {
#if defined(__EMSCRIPTEN__)
            (void)seconds;
#else
            if(std::isinf(seconds))
                glfwWaitEvents();
            else
                glfwWaitEventsTimeout(seconds);
#endif
        };
        hooks.drawFrame=[this]() { m_callbacks.drawFrame(); };
        hooks.canDraw=[this]() { return !m_callbacks.canDraw || m_callbacks.canDraw(); };
        hooks.frameWasIdle=[this](std::string &why) {
            why=frameWrites(m_renderer->frameStats());
            return why.empty();
        };
        hooks.verifyFailed=[](size_t frame, const std::string &why) {
            std::fprintf(stderr, "gweni: idle verify: frame %zu, drawn with no work reported, %s\n",
                frame, why.c_str());
        };
        return hooks;
    }

#if defined(__EMSCRIPTEN__)
    static void webCallback(void *arg)
    {
        GlfwLoop &self=*static_cast<GlfwLoop *>(arg);

        // poll first: a size the canvas's container asked for is applied there
        self.m_loop.tick();
        if(self.m_callbacks.afterIteration)
            self.m_callbacks.afterIteration(self.m_loop);
    }
#endif

    GLFWwindow *m_window;
    controls::Canvas *m_canvas;
    renderer::Base *m_renderer;
    Callbacks m_callbacks;
    platform::IdleLoop m_loop;
};

}//namespace input
}//namespace gweni

#endif//GWENI_INPUT_GLFW_LOOP_H
