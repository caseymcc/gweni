/*
 *  Gweni
 
 *  See license in Gweni.h
 */


#ifndef _gweni_input_null_h_
#define _gweni_input_null_h_

#include <gweni/platforms/inputEventListener.h>

namespace gweni
{
namespace input
{

class Null
{
public:

    Null()
    :   m_eventListener(nullptr)
    {}

    void Initialize(IInputEventListener *el)
    {
        m_eventListener = el;
    }

    unsigned char TranslateKeyCode(int keyCode)
    {
        return keys::Invalid;
    }

    // bool ProcessMessage()
    // {
    //     return false;
    // }

protected:

    IInputEventListener *m_eventListener;
};

}//namespace input
}//namespace gweni

#endif // _gweni_input_null_h_
