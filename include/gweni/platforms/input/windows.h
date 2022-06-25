/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
 *  See license in Gweni.h
 */


#ifndef _gweni_input_windows_h_
#define _gweni_input_windows_h_

#include <gweni/platforms/inputEventListener.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

namespace gweni
{
namespace input
{

//! Translate Windows system messages into Gweni events.
class Windows
{
public:

    Windows(IInputEventListener* listener)
        :   m_eventListener(listener)
        ,   m_mouseX(0)
        ,   m_mouseY(0)
    {
    }

    bool ProcessMessage(MSG msg)
    {
        if (!m_eventListener)
            return false;

        switch (msg.message)
        {
        // case WM_NCLBUTTONDOWN:
        case WM_SYSCOMMAND:
            if (msg.message == WM_SYSCOMMAND && msg.wParam != SC_CLOSE)
                return false;

            return m_eventListener->inputQuit();

        case WM_MOUSEMOVE:
            {
                const int x = (signed short)LOWORD(msg.lParam);
                const int y = (signed short)HIWORD(msg.lParam);
                const int dx = x - m_mouseX;
                const int dy = y - m_mouseY;
                m_mouseX = x;
                m_mouseY = y;
                return m_eventListener->inputMouseMoved(x, y, dx, dy);
            }

        case WM_CHAR:
            {
                const gweni::UnicodeChar chr = (gweni::UnicodeChar)msg.wParam;
                return m_eventListener->inputCharacter(chr);
            }

#ifdef WM_MOUSEWHEEL

        case WM_MOUSEWHEEL:
            return m_eventListener->inputMouseWheel((short)HIWORD(msg.wParam));

#endif

        case WM_LBUTTONDOWN:
            SetCapture(msg.hwnd);
            return m_eventListener->inputMouseButton(0, true);

        case WM_LBUTTONUP:
            ReleaseCapture();
            return m_eventListener->inputMouseButton(0, false);

        case WM_RBUTTONDOWN:
            SetCapture(msg.hwnd);
            return m_eventListener->inputMouseButton(1, true);

        case WM_RBUTTONUP:
            ReleaseCapture();
            return m_eventListener->inputMouseButton(1, false);

        case WM_MBUTTONDOWN:
            SetCapture(msg.hwnd);
            return m_eventListener->inputMouseButton(2, true);

        case WM_MBUTTONUP:
            ReleaseCapture();
            return m_eventListener->inputMouseButton(2, false);

        case WM_LBUTTONDBLCLK:
        case WM_RBUTTONDBLCLK:
        case WM_MBUTTONDBLCLK:
            // Filter out those events from the application
            return true;

        case WM_KEYDOWN:
        case WM_KEYUP:
            {
                const bool bDown = msg.message == WM_KEYDOWN;
                int key = -1;

                // These aren't sent by WM_CHAR when CTRL is down - but
                // we need them internally for copy and paste etc..
                if (bDown
                    && (GetKeyState(VK_CONTROL) & 0x80) != 0
                    && msg.wParam >= 'A'
                    && msg.wParam <= 'Z')
                {
                    const gweni::UnicodeChar chr = (gweni::UnicodeChar)msg.wParam;
                    return m_eventListener->inputCharacter(chr);
                }

                if (msg.wParam == VK_SHIFT)
                    key = keys::Shift;
                else if (msg.wParam == VK_RETURN)
                    key = keys::Return;
                else if (msg.wParam == VK_BACK)
                    key = keys::Backspace;
                else if (msg.wParam == VK_DELETE)
                    key = keys::Delete;
                else if (msg.wParam == VK_LEFT)
                    key = keys::Left;
                else if (msg.wParam == VK_RIGHT)
                    key = keys::Right;
                else if (msg.wParam == VK_TAB)
                    key = keys::Tab;
                else if (msg.wParam == VK_SPACE)
                    key = keys::Space;
                else if (msg.wParam == VK_HOME)
                    key = keys::Home;
                else if (msg.wParam == VK_END)
                    key = keys::End;
                else if (msg.wParam == VK_CONTROL)
                    key = keys::Control;
                else if (msg.wParam == VK_SPACE)
                    key = keys::Space;
                else if (msg.wParam == VK_UP)
                    key = keys::Up;
                else if (msg.wParam == VK_DOWN)
                    key = keys::Down;

                if (key != -1)
                    return m_eventListener->inputModifierKey(key, bDown);

                break;
            }

        default:
            break;
        }

        return false;
    }

protected:

    IInputEventListener* m_eventListener;
    int m_mouseX;
    int m_mouseY;

};

}//namespace input
}//namespace gweni

#endif //_gweni_input_windows_h_
