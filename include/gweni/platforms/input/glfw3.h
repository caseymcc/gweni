/*
 *  Gweni
 
 *  See license in Gweni.h
 */


#ifndef GWENI_INPUT_OPENGL_GLFW3_H
#define GWENI_INPUT_OPENGL_GLFW3_H

#include <gweni/platforms/inputEvent.h>
#include <GLFW/glfw3.h>

namespace gweni
{
namespace input
{

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

        default: ;
        }

        return keys::Invalid;
    }

    bool processKeyEvent(const KeyEvent& event)
    {
        if (!m_eventCaller)
            return false;

        const bool bPressed = (event.action == GLFW_PRESS);
        const char keyCode = event.key;
        const bool control = (event.mods & GLFW_MOD_CONTROL) != 0;

        if (control && bPressed && keyCode >= 'a' && keyCode <= 'z')
            return m_eventCaller->inputCharacter(keyCode);

        const unsigned char key = translateKeyCode(keyCode);
        return m_eventCaller->inputModifierKey(key, bPressed);
    }

    bool processMouseMove(int x, int y)
    {
        if (m_mouse.x < 0)
            m_lastMouse = m_mouse;

        m_mouse.set(x, y);
        const Point delta(m_lastMouse - m_mouse);
        m_lastMouse = m_mouse;

        return m_eventCaller->inputMouseMoved(m_mouse.x, m_mouse.y,
                                                delta.x, delta.y);
    }

    bool processMouseButtons(int button, int action, int mods)
    {
        return m_eventCaller->inputMouseButton(button, action == GLFW_PRESS);
    }

    bool processScroll(float sx, float sy)
    {
        return m_eventCaller->inputMouseWheel(sy);
    }

protected:

    IInputEvent *m_eventCaller;
    Point m_mouse, m_lastMouse;

};

}//namespace input
}//namespace gweni

#endif //GWENI_INPUT_OPENGL_GLFW3_H
