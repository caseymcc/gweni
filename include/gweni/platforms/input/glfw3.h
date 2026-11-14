/*
 *  Gweni
 
 *  See license in Gweni.h
 */


#ifndef GWENI_INPUT_OPENGL_GLFW3_H
#define GWENI_INPUT_OPENGL_GLFW3_H

#include <gweni/platforms/inputEvent.h>
#include <gweni/platforms/platform.h>
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
    ,   m_controlChord(false)
    {}

    void initialize(IInputEvent *c)
    {
        m_eventCaller = c;
    }

    //! Make the platform clipboard (platform::getClipboardText(),
    //! setClipboardText(), requestClipboardText()) the system's, through
    //! GLFW's on \a window: on X11 the CLIPBOARD selection, which the window
    //! owns after a copy, serves to other clients from glfwPollEvents() and
    //! loses when it closes. Call it once with the
    //! window an app shows, and with null before the window is destroyed,
    //! which gives the in-process clipboard back. A capture or a test never
    //! calls it, so what it pastes never depends on the host. In a browser
    //! page it is the browser's clipboard (platform::useBrowserClipboard),
    //! not emscripten-glfw's, whose paste is the one before the keystroke.
    static void useSystemClipboard(GLFWwindow *window)
    {
#if defined(__EMSCRIPTEN__)
        platform::useBrowserClipboard(window != nullptr);
#else
        if(!window)
        {
            platform::setClipboardBackend(nullptr, nullptr, nullptr);
            return;
        }

        platform::setClipboardBackend(
            [window]()
            {
                // an empty clipboard, or one holding no text, is nothing to
                // paste, not an error for the app's GLFW error callback
                GLFWerrorfun previous=glfwSetErrorCallback(nullptr);
                const char *text=glfwGetClipboardString(window);

                glfwSetErrorCallback(previous);
                return text ? String(text) : String();
            },
            [window](const String &text)
            {
                glfwSetClipboardString(window, text.c_str());
                return true;
            });
#endif
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

        case GLFW_KEY_PAGE_UP:
            return keys::PageUp;

        case GLFW_KEY_PAGE_DOWN:
            return keys::PageDown;

        case GLFW_KEY_F4:
            return keys::F4;

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
    // typed text only from processCharEvent. A letter with Ctrl and Alt both
    // held is AltGr on Windows (see syncModifiers): its character comes from
    // the char callback, so it is no shortcut, and a real Ctrl+Alt+letter does
    // nothing (gweni has no Alt accelerators).
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
        const unsigned char key = translateKeyCode(keyCode);
        const bool control = (event.mods & GLFW_MOD_CONTROL) != 0;
        const bool alt = (event.mods & GLFW_MOD_ALT) != 0;

        syncModifiers(event.mods, key);

        bool handled = false;

        m_controlChord = control && !alt && bPressed && keyCode >= GLFW_KEY_A && keyCode <= GLFW_KEY_Z;
        if (m_controlChord)
            handled = m_eventCaller->inputCharacter(static_cast<UnicodeChar>('A'+(keyCode-GLFW_KEY_A)));

        // The key's own press or release comes after the sync, so it wins: on
        // X11 the mods are the state before the event, without Ctrl on Ctrl's
        // own press and with it on its release.
        return m_eventCaller->inputModifierKey(key, bPressed) || handled;
    }

    // Typed text, from glfwSetCharCallback: one codepoint per call, forwarded
    // as it is. Canvas::inputCharacter drops the ones that are not text.
    bool processCharEvent(unsigned int codepoint)
    {
        if (!m_eventCaller)
            return false;

        // emscripten-glfw sends Ctrl+letter's letter as a char too, right
        // after its key event (native GLFW sends none): the key event ran the
        // accelerator already, and a second run would paste or undo twice
        if (m_controlChord)
        {
            m_controlChord = false;
            return false;
        }

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
        if(!m_eventCaller)
            return false;

        // One wheel notch is 60 units, as documented on
        // IInputEvent::inputMouseWheel. GLFW reports +-1 per notch, and
        // fractional offsets on touchpads, so round.
        const int horizontal=static_cast<int>(std::lround(sx*60.0f));
        const int vertical=static_cast<int>(std::lround(sy*60.0f));
        bool used=false;

        if(vertical)
            used=m_eventCaller->inputMouseWheel(vertical);
        if(horizontal)
            used=m_eventCaller->inputMouseWheelH(horizontal) || used;

        return used;
    }

protected:

    // Makes the gweni modifiers match the bits GLFW says are held, pressing
    // and releasing them, before each key event. One already in that state is
    // left alone (onKeyEvent ignores a press of a key that is down and a
    // release of one that is up). So a modifier the input handler forgot (it
    // clears held keys when the keyboard focus moves) is pressed again, and a
    // Ctrl held across a click into a TextBox still makes Ctrl+B a shortcut
    // and not a typed "B"; and one whose release never arrived is let go by
    // the next key event at the latest, which GLFW delivers before that key's
    // char. \a ownKey, the event's own key, is left to that event.
    //
    // Ctrl and Alt held together count as no Ctrl, because that is AltGr on
    // Windows, and Windows itself makes characters from Ctrl+Alt as AltGr. It
    // sends AltGr as a synthetic Left Ctrl followed by Right Alt; GLFW (3.5.1,
    // win32_window.c) discards the synthetic Ctrl's press and release, so no
    // LEFT_CONTROL event ever comes, but it reads the mods from
    // GetKeyState(VK_CONTROL), which that Ctrl has set. Every key under AltGr
    // therefore carries GLFW_MOD_CONTROL|GLFW_MOD_ALT. Counted as Ctrl, it
    // would drop the AltGr character (Canvas::inputCharacter refuses
    // characters while Ctrl is down), and since the Ctrl release never comes
    // only this sync lets it go. Linux and macOS never set the Ctrl bit for
    // AltGr/Option. The mods on the Right Alt events themselves are not
    // certain, so a Ctrl one of them presses is released by the next key.
    void syncModifiers(int mods, unsigned char ownKey)
    {
        const bool alt = (mods & GLFW_MOD_ALT) != 0;
        const bool control = (mods & GLFW_MOD_CONTROL) != 0 && !alt;
        const bool shift = (mods & GLFW_MOD_SHIFT) != 0;

        if (ownKey != keys::Control)
            m_eventCaller->inputModifierKey(keys::Control, control);
        if (ownKey != keys::Shift)
            m_eventCaller->inputModifierKey(keys::Shift, shift);
        if (ownKey != keys::Alt)
            m_eventCaller->inputModifierKey(keys::Alt, alt);
    }

    IInputEvent *m_eventCaller;
    Point m_mouse, m_lastMouse;
    bool m_haveMouse;
    Point m_windowSize, m_framebufferSize;
    float m_scale;
    // the last key event was a Ctrl+letter press (processCharEvent)
    bool m_controlChord;

};

}//namespace input
}//namespace gweni

#endif //GWENI_INPUT_OPENGL_GLFW3_H
