/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_stepper_example_h_
#define _gweni_stepper_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Stepper;
class TextBox;
}

//! Steppers: a horizontal linear one of four steps (the third optional)
//! whose second page's name field vetoes Next while it is empty and marks the
//! step Error, a vertical non-linear one whose steps are clicked and keyed to,
//! and a horizontal one whose third step is disabled, so next() and back()
//! pass over it.
class StepperDemo: public controls::Base
{
public:
    GWENI_CONTROL(StepperDemo, controls::Base);

public:
    //! The steppers a test drives.
    controls::Stepper *linearStepper() const { return m_linear; }
    controls::Stepper *verticalStepper() const { return m_vertical; }
    controls::Stepper *disabledStepStepper() const { return m_disabled; }
    controls::TextBox *nameField() const { return m_name; }

private:
    void onLinearChanging(event::Info info);
    void onLinearFinish(event::Info info);

    controls::Stepper *m_linear=nullptr;
    controls::Stepper *m_vertical=nullptr;
    controls::Stepper *m_disabled=nullptr;
    controls::TextBox *m_name=nullptr;
};

}//namespace gweni

#endif//_gweni_stepper_example_h_
