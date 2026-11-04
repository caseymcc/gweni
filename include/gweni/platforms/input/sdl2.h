/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
 *  See license in Gweni.h
 *  SDL2 Added by BQ.
 */
#ifndef _gweni_input_sdl2_h_
#define _gweni_input_sdl2_h_

#include <gweni/InputHandler.h>
#include <gweni/Gweni.h>
#include <gweni/platforms/inputEventListener.h>
#include <SDL.h>


namespace gweni
{
namespace input
{
class SDL2
{
public:

    SDL2()
    {
        m_listener = nullptr;
    }

    void Initialize(gweni::IInputEventListener* c)
    {
        m_listener = c;
    }

    bool ProcessEvent(const SDL_Event* event)
    {
        if (!m_listener)
            return false;

        switch (event->type)
        {
        case SDL_KEYUP:
        case SDL_KEYDOWN:
            {
                const SDL_KeyboardEvent *ke = &event->key;
                int key = -1;
                SDL_Scancode scancode = ke->keysym.scancode;

                switch (scancode)
                {
                case SDL_SCANCODE_RETURN:       key = keys::Return; break;
                case SDL_SCANCODE_BACKSPACE:    key = keys::Backspace; break;
                case SDL_SCANCODE_DELETE:       key = keys::Delete; break;
                case SDL_SCANCODE_LEFT:         key = keys::Left; break;
                case SDL_SCANCODE_RIGHT:        key = keys::Right; break;
                case SDL_SCANCODE_LSHIFT:       key = keys::Shift; break;
                case SDL_SCANCODE_RSHIFT:       key = keys::Shift; break;
                case SDL_SCANCODE_TAB:          key = keys::Tab; break;
                case SDL_SCANCODE_SPACE:        key = keys::Space; break;
                case SDL_SCANCODE_HOME:         key = keys::Home; break;
                case SDL_SCANCODE_END:          key = keys::End; break;
                case SDL_SCANCODE_LCTRL:        key = keys::Control; break;
                case SDL_SCANCODE_RCTRL:        key = keys::Control; break;
                case SDL_SCANCODE_UP:           key = keys::Up; break;
                case SDL_SCANCODE_DOWN:         key = keys::Down; break;
                case SDL_SCANCODE_ESCAPE:       key = keys::Escape; break;
                case SDL_SCANCODE_LALT:         key = keys::Alt; break;
                case SDL_SCANCODE_RALT:         key = keys::Alt; break;
                default:                        return false;
                }

                return m_listener->inputModifierKey(key, ke->state != 0);
            }

        case SDL_TEXTINPUT:
            {
                // TODO: This will probably need fixing for UTF-8.
                return m_listener->inputCharacter(event->text.text[0]);
            }

        case SDL_MOUSEMOTION:
            {
                const SDL_MouseMotionEvent* E = &event->motion;
                return m_listener->inputMouseMoved(E->x, E->y, E->xrel, E->yrel);
            }

        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            {
                const SDL_MouseButtonEvent* E = &event->button;
                int Button = -1;

                switch (E->button)
                {
                case SDL_BUTTON_LEFT:
                    Button = 0;
                    break;

                case SDL_BUTTON_MIDDLE:
                    Button = 2;
                    break;

                case SDL_BUTTON_RIGHT:
                    Button = 1;
                    break;

                default:
                    return false;
                }

                return m_listener->inputMouseButton(Button, E->state != 0);
            }

        case SDL_MOUSEWHEEL:
            {
                const SDL_MouseWheelEvent* E = &event->wheel;
                return m_listener->inputMouseWheel(E->y);
            }

        default:
            return false;
        }
    }

protected:

    gweni::IInputEventListener* m_listener;

};

}//namespace input
}//namespace gweni

#endif // _gweni_input_sdl2_h_
