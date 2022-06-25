/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
 *  See license in Gweni.h
 */


#ifndef _gweni_input_allegro_h_
#define _gweni_input_allegro_h_

#include <gweni/platforms/inputEventListener.h>
#include <allegro5/allegro.h>

namespace gweni
{
    namespace input
    {
        class Allegro
        {
        public:

            Allegro()
            :   m_eventListener(nullptr)
            ,   m_mouseX(0)
            ,   m_mouseY(0)
            {
            }

            void Initialize(IInputEventListener* el)
            {
                m_eventListener = el;
            }

            unsigned char TranslateKeyCode(int keyCode)
            {
                switch (keyCode)
                {
                case ALLEGRO_KEY_BACKSPACE:
                    return keys::Backspace;

                case ALLEGRO_KEY_ENTER:
                case ALLEGRO_KEY_PAD_ENTER:
                    return keys::Return;

                case ALLEGRO_KEY_ESCAPE:
                    return keys::Escape;

                case ALLEGRO_KEY_TAB:
                    return keys::Tab;

                case ALLEGRO_KEY_SPACE:
                    return keys::Space;

                case ALLEGRO_KEY_UP:
                    return keys::Up;

                case ALLEGRO_KEY_DOWN:
                    return keys::Down;

                case ALLEGRO_KEY_LEFT:
                    return keys::Left;

                case ALLEGRO_KEY_RIGHT:
                    return keys::Right;

                case ALLEGRO_KEY_HOME:
                    return keys::Home;

                case ALLEGRO_KEY_END:
                    return keys::End;

                case ALLEGRO_KEY_DELETE:
                    return keys::Delete;

                case ALLEGRO_KEY_LSHIFT:
                    return keys::Shift;

                case ALLEGRO_KEY_LCTRL:
                    return keys::Control;

                case ALLEGRO_KEY_ALT:
                    return keys::Alt;

                case ALLEGRO_KEY_COMMAND:
                    return keys::Command;

                case ALLEGRO_KEY_RSHIFT:
                    return keys::Shift;

                case ALLEGRO_KEY_RCTRL:
                    return keys::Control;

                case ALLEGRO_KEY_ALTGR:
                    return keys::Alt;

                default: ;
                }

                return keys::Invalid;
            }

            bool ProcessMessage(const ALLEGRO_EVENT& event)
            {
                if (!m_eventListener)
                    return false;

                switch (event.type)
                {
                case ALLEGRO_EVENT_MOUSE_AXES:
                    {
                        const int dx = event.mouse.dx;
                        const int dy = event.mouse.dy;

                        if (event.mouse.dz != 0)
                            return m_eventListener->inputMouseWheel(event.mouse.dz*60);

                        m_mouseX = event.mouse.x;
                        m_mouseY = event.mouse.y;
                        return m_eventListener->inputMouseMoved(m_mouseX, m_mouseY, dx, dy);
                    }

                case ALLEGRO_EVENT_MOUSE_BUTTON_DOWN:
                case ALLEGRO_EVENT_MOUSE_BUTTON_UP:
                    return m_eventListener->inputMouseButton(
                                                    event.mouse.button-1,
                                                    event.type == ALLEGRO_EVENT_MOUSE_BUTTON_DOWN);

                case ALLEGRO_EVENT_KEY_CHAR:
                    return m_eventListener->inputCharacter(event.keyboard.unichar);

                case ALLEGRO_EVENT_KEY_DOWN:
                case ALLEGRO_EVENT_KEY_UP:
                    {
                        const bool bPressed = (event.type == ALLEGRO_EVENT_KEY_DOWN);

                        if (event.keyboard.keycode != 0
                            && bPressed
                            && event.keyboard.keycode >= ALLEGRO_KEY_A
                            && event.keyboard.keycode <= ALLEGRO_KEY_Z)
                        {
                            return m_eventListener->inputCharacter(event.keyboard.keycode);
                        }

                        const unsigned char key = TranslateKeyCode(event.keyboard.keycode);
                        return key == keys::Invalid
                               ? false
                               : m_eventListener->inputModifierKey(key, bPressed);
                    }
                }

                return false;
            }

        protected:

            IInputEventListener *m_eventListener;
            int m_mouseX;
            int m_mouseY;

        };
    }
}

#endif //_gweni_input_allegro_h_
