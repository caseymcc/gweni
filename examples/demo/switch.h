/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_switch_example_h_
#define _gweni_switch_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Button;
class Switch;
}

//! Switches: off and on with no label, a label on the right and one on the
//! left, a disabled one off and one on, and one whose onToggled enables and
//! disables a Button beside it.
class SwitchDemo: public controls::Base
{
public:
    GWENI_CONTROL(SwitchDemo, controls::Base);

public:
    //! The parts a test drives.
    controls::Switch *offSwitch() const { return m_off; }
    controls::Switch *onSwitch() const { return m_on; }
    controls::Switch *labelRightSwitch() const { return m_labelRight; }
    controls::Switch *labelLeftSwitch() const { return m_labelLeft; }
    controls::Switch *disabledOffSwitch() const { return m_disabledOff; }
    controls::Switch *disabledOnSwitch() const { return m_disabledOn; }
    controls::Switch *enablingSwitch() const { return m_enabling; }
    controls::Button *enabledButton() const { return m_button; }

private:
    void onEnablingToggled(event::Info info);

    controls::Switch *m_off=nullptr;
    controls::Switch *m_on=nullptr;
    controls::Switch *m_labelRight=nullptr;
    controls::Switch *m_labelLeft=nullptr;
    controls::Switch *m_disabledOff=nullptr;
    controls::Switch *m_disabledOn=nullptr;
    controls::Switch *m_enabling=nullptr;
    controls::Button *m_button=nullptr;
};

}//namespace gweni

#endif//_gweni_switch_example_h_
