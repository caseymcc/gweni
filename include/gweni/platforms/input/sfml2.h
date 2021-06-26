/*
 *  Gweni
 *  Copyright (c) 2011 Facepunch Studios
 
 *  See license in Gweni.h
 */


#ifndef _gweni_input_sfml_h_
#define _gweni_input_sfml_h_

#include <gweni/Gweni.h>
#include <gweni/platforms/inputEventListener.h>
#include <SFML/Window/Event.hpp>

namespace gweni
{
namespace input
{

class SFML
{
public:

    SFML()
    :   m_eventListener(nullptr)
    ,   m_mouseX(0)
    ,   m_mouseY(0)
    {
    }

    void Initialize(IInputEventListener* c)
    {
        m_eventListener = c;
    }

    unsigned char TranslateKeyCode(int keyCode)
    {
        switch (keyCode)
        {
        case sf::Keyboard::BackSpace:
            return keys::Backspace;

        case sf::Keyboard::Return:
            return keys::Return;

        case sf::Keyboard::Escape:
            return keys::Escape;

        case sf::Keyboard::Tab:
            return keys::Tab;

        case sf::Keyboard::Space:
            return keys::Space;

        case sf::Keyboard::Up:
            return keys::Up;

        case sf::Keyboard::Down:
            return keys::Down;

        case sf::Keyboard::Left:
            return keys::Left;

        case sf::Keyboard::Right:
            return keys::Right;

        case sf::Keyboard::Home:
            return keys::Home;

        case sf::Keyboard::End:
            return keys::End;

        case sf::Keyboard::Delete:
            return keys::Delete;

        case sf::Keyboard::LControl:
            return keys::Control;

        case sf::Keyboard::LAlt:
            return keys::Alt;

        case sf::Keyboard::LShift:
            return keys::Shift;

        case sf::Keyboard::RControl:
            return keys::Control;

        case sf::Keyboard::RAlt:
            return keys::Alt;

        case sf::Keyboard::RShift:
            return keys::Shift;
        }

        return keys::Invalid;
    }

    bool ProcessMessage(const sf::Event& event)
    {
        if (!m_eventListener)
            return false;

        switch (event.type)
        {
        case sf::Event::MouseMoved:
            {
                const int dx = event.mouseMove.x-m_mouseX;
                const int dy = event.mouseMove.y-m_mouseY;
                m_mouseX = event.mouseMove.x;
                m_mouseY = event.mouseMove.y;
                return m_eventListener->inputMouseMoved(m_mouseX, m_mouseY, dx, dy);
            }

        case sf::Event::MouseButtonPressed:
        case sf::Event::MouseButtonReleased:
            return m_eventListener->inputMouseButton(event.mouseButton.button,
                                                event.type == sf::Event::MouseButtonPressed);

        case sf::Event::MouseWheelMoved:
            return m_eventListener->inputMouseWheel(event.mouseWheel.delta*60);

        case sf::Event::TextEntered:
            return m_eventListener->inputCharacter(event.text.unicode);

        case sf::Event::KeyPressed:
        case sf::Event::KeyReleased:
            {
                const bool bPressed = (event.type == sf::Event::KeyPressed);
                const char keyCode = event.key.code;
                const bool control = event.key.control;

                if (control && bPressed && keyCode >= 'a' && keyCode <= 'z')
                    return m_eventListener->inputCharacter(keyCode);

                const unsigned char key = TranslateKeyCode(keyCode);
                return m_eventListener->inputModifierKey(key, bPressed);
            }

        default:
            ;
        }

        return false;
    }

protected:

    IInputEventListener *m_eventListener;
    int m_mouseX;
    int m_mouseY;

};

}//namespace input
}//namespace gweni

#endif //_gweni_input_sfml_h_
