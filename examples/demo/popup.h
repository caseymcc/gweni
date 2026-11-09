#ifndef _gweni_popup_example_h_
#define _gweni_popup_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class Popup;
}

class PopupDemo: public controls::Base
{
public:
    GWENI_CONTROL(PopupDemo, controls::Base);

public:
    //! A button was pressed: opens its popup, or closes it when it is open.
    void onToggle(event::Info info);
    //! The Options popup's Done button: closes it.
    void onDone(event::Info info);

    controls::Button *m_options=nullptr;
    controls::Button *m_corner=nullptr;
    controls::Popup *m_optionsPopup=nullptr;
    controls::Popup *m_cornerPopup=nullptr;
};

}//namespace gweni

#endif//_gweni_popup_example_h_
