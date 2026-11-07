/*
 *  Gweni
 
 *  See license in Gweni.h
 */


#ifndef GWENI_INPUT_OPENGL_GLFW3_H
#define GWENI_INPUT_OPENGL_GLFW3_H

#include <gweni/platforms/inputEvent.h>
#include <GLFW/glfw3.h>

#include <cmath>

namespace gweni
{
namespace input
{

//! One cursor coordinate GLFW reports in window coordinates, in the canvas's
//! logical units: \a window * (\a framebufferSize / \a windowSize) / \a scale,
//! with the sizes along the same axis. The ratio makes it right on both kinds
//! of window system: on macOS and scaled Wayland window coordinates are points
//! (the framebuffer is larger), on X11 and Windows they are pixels (the two
//! sizes are equal). Not truncated, so a caller rounds once, at the end. A
//! zero-sized (minimised) window or a non-positive scale leaves it unmapped.
inline double windowToLogical(double window, int windowSize, int framebufferSize, float scale)
{
    if(windowSize <= 0 || framebufferSize <= 0 || !(scale > 0.0f))
        return window;

    return window*(static_cast<double>(framebufferSize)/windowSize)/scale;
}

class GLFW3
{
public:

    struct KeyEvent {
        int key, scancode, action, mods;
    };

    struct MouseEvent {
        double x, y;
    };

    GLFW3()
    :   m_eventCaller(nullptr)
    ,   m_mouse(-1, -1)
    ,   m_haveMouse(false)
    ,   m_windowSize(0, 0)
    ,   m_framebufferSize(0, 0)
    ,   m_scale(1.0f)
    {}

    void initialize(IInputEvent *c)
    {
        m_eventCaller = c;
    }

    unsigned char translateKeyCode(int keyCode)
    {
        switch (keyCode)
        {
        case GLFW_KEY_BACKSPACE:
            return keys::Backspace;

        case GLFW_KEY_ENTER:
        case GLFW_KEY_KP_ENTER:
            return keys::Return;

        case GLFW_KEY_ESCAPE:
            return keys::Escape;

        case GLFW_KEY_TAB:
            return keys::Tab;

        case GLFW_KEY_SPACE:
            return keys::Space;

        case GLFW_KEY_UP:
            return keys::Up;

        case GLFW_KEY_DOWN:
            return keys::Down;

        case GLFW_KEY_LEFT:
            return keys::Left;

        case GLFW_KEY_RIGHT:
            return keys::Right;

        case GLFW_KEY_HOME:
            return keys::Home;

        case GLFW_KEY_END:
            return keys::End;

        case GLFW_KEY_DELETE:
            return keys::Delete;

        case GLFW_KEY_LEFT_CONTROL:
            return keys::Control;

        case GLFW_KEY_LEFT_ALT:
            return keys::Alt;

        case GLFW_KEY_LEFT_SHIFT:
            return keys::Shift;

        case GLFW_KEY_RIGHT_CONTROL:
            return keys::Control;

        case GLFW_KEY_RIGHT_ALT:
            return keys::Alt;

        case GLFW_KEY_RIGHT_SHIFT:
            return keys::Shift;

        case GLFW_KEY_LEFT_SUPER:
        case GLFW_KEY_RIGHT_SUPER:
            return keys::Command;

        default: ;
        }

        return keys::Invalid;
    }

    // Shortcuts and text arrive on two separate paths, so nothing is handled
    // twice: GLFW's char callback drops everything below 32 and emits no char
    // for Ctrl/Alt+letter, so a Ctrl+letter comes only from here (as the
    // uppercase letter, which handleAccelerator turns into "CTRL+V"), and
    // typed text only from processCharEvent.
    bool processKeyEvent(const KeyEvent& event)
    {
        if (!m_eventCaller)
            return false;

        // gweni repeats held keys itself (onCanvasThink), so the OS's repeats
        // are dropped rather than treated as releases.
        if (event.action == GLFW_REPEAT)
            return false;

        const bool bPressed = (event.action == GLFW_PRESS);
        const int keyCode = event.key;
        const bool control = (event.mods & GLFW_MOD_CONTROL) != 0;

        if (bPressed)
            pressHeldModifiers(event.mods);

        bool handled = false;

        if (control && bPressed && keyCode >= GLFW_KEY_A && keyCode <= GLFW_KEY_Z)
            handled = m_eventCaller->inputCharacter(static_cast<UnicodeChar>('A'+(keyCode-GLFW_KEY_A)));

        const unsigned char key = translateKeyCode(keyCode);
        return m_eventCaller->inputModifierKey(key, bPressed) || handled;
    }

    // Typed text, from glfwSetCharCallback: one codepoint per call, forwarded
    // as it is. Canvas::inputCharacter drops the ones that are not text.
    bool processCharEvent(unsigned int codepoint)
    {
        if (!m_eventCaller)
            return false;

        return m_eventCaller->inputCharacter(static_cast<UnicodeChar>(codepoint));
    }

    // From glfwSetWindowFocusCallback. GLFW sends releases for the keys held
    // when the window loses focus, but a release that never reaches us would
    // leave a modifier down (and a held Ctrl drops every typed character), so
    // every key is let go here too. Releasing a key that is up does nothing.
    bool processFocus(bool focused)
    {
        if (!m_eventCaller || focused)
            return false;

        for (int key = keys::Invalid+1; key < keys::KeysCount; ++key)
            m_eventCaller->inputModifierKey(key, false);
        return true;
    }

    //! What processMouseMove maps the cursor through: the window's size in
    //! window coordinates (glfwGetWindowSize), its framebuffer's in device
    //! pixels (glfwGetFramebufferSize) and the canvas's scale. Keep it current
    //! from the framebuffer-size and content-scale callbacks. Until it is
    //! called the cursor is taken as it comes, as logical units.
    void setWindowMetrics(const Point &windowSize, const Point &framebufferSize, float scale)
    {
        m_windowSize = windowSize;
        m_framebufferSize = framebufferSize;
        m_scale = scale;
    }

    //! The cursor in window coordinates, from glfwSetCursorPosCallback. The
    //! canvas gets it, and the delta, in logical units.
    bool processMouseMove(double x, double y)
    {
        if (!m_eventCaller)
            return false;

        m_mouse.set(
            static_cast<int>(std::floor(windowToLogical(x, m_windowSize.x, m_framebufferSize.x, m_scale))),
            static_cast<int>(std::floor(windowToLogical(y, m_windowSize.y, m_framebufferSize.y, m_scale))));

        // The first event has no previous position: report a zero delta.
        if (!m_haveMouse)
        {
            m_lastMouse = m_mouse;
            m_haveMouse = true;
        }

        // Current minus last, the same sign as SDL2's xrel/yrel.
        const Point delta(m_mouse - m_lastMouse);
        m_lastMouse = m_mouse;

        return m_eventCaller->inputMouseMoved(m_mouse.x, m_mouse.y,
                                                delta.x, delta.y);
    }

    //! From glfwSetCursorEnterCallback. Leaving tells the canvas, which stops
    //! hovering (and hides a tooltip); the next move after coming back reports
    //! a zero delta, as the first one does.
    bool processCursorEnter(bool entered)
    {
        if (!m_eventCaller || entered)
            return false;

        m_haveMouse = false;
        return m_eventCaller->inputMouseLeave();
    }

    bool processMouseButtons(int button, int action, int mods)
    {
        return m_eventCaller->inputMouseButton(button, action == GLFW_PRESS);
    }

    bool processScroll(float sx, float sy)
    {
        // One wheel notch is 60 units, as documented on
        // IInputEvent::inputMouseWheel. GLFW reports +-1 per notch, and
        // fractional offsets on touchpads, so round.
        return m_eventCaller->inputMouseWheel(static_cast<int>(std::lround(sy*60.0f)));
    }

protected:

    // Presses the gweni modifier for each modifier bit GLFW says is held. One
    // that is already down is left alone; one the input handler forgot (it
    // clears held keys when the keyboard focus moves) is pressed again, so a
    // Ctrl held across a click into a TextBox still makes Ctrl+B a shortcut and
    // not a typed "B".
    void pressHeldModifiers(int mods)
    {
        if (mods & GLFW_MOD_CONTROL)
            m_eventCaller->inputModifierKey(keys::Control, true);
        if (mods & GLFW_MOD_SHIFT)
            m_eventCaller->inputModifierKey(keys::Shift, true);
        if (mods & GLFW_MOD_ALT)
            m_eventCaller->inputModifierKey(keys::Alt, true);
    }

    IInputEvent *m_eventCaller;
    Point m_mouse, m_lastMouse;
    bool m_haveMouse;
    Point m_windowSize, m_framebufferSize;
    float m_scale;

};

}//namespace input
}//namespace gweni

#endif //GWENI_INPUT_OPENGL_GLFW3_H
