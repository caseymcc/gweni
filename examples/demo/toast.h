#ifndef _gweni_toast_example_h_
#define _gweni_toast_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
}

//! Toasts (Canvas::notify): a button per kind, one for a long text that wraps
//! onto several lines, and one for a sticky toast that stays until its close
//! button is pressed. Nothing is shown until a button is pressed.
class ToastDemo: public controls::Base
{
public:
    GWENI_CONTROL(ToastDemo, controls::Base);

public:
    void onInfo(event::Info);
    void onPositive(event::Info);
    void onWarning(event::Info);
    void onNegative(event::Info);
    void onLong(event::Info);
    void onSticky(event::Info);

    controls::Button *m_info=nullptr;
    controls::Button *m_positive=nullptr;
    controls::Button *m_warning=nullptr;
    controls::Button *m_negative=nullptr;
    controls::Button *m_long=nullptr;
    controls::Button *m_sticky=nullptr;
};

}//namespace gweni

#endif//_gweni_toast_example_h_
