/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */
#ifndef _gweni_knob_example_h_
#define _gweni_knob_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls { class Knob; }

class KnobDemo: public controls::Base
{
public:
    GWENI_CONTROL(KnobDemo, controls::Base);

public:
    controls::Knob *vertical() const { return m_vertical; }
    controls::Knob *angular() const { return m_angular; }
    controls::Knob *bipolar() const { return m_bipolar; }
    controls::Knob *integer() const { return m_integer; }
    controls::Knob *disabled() const { return m_disabled; }

private:
    controls::Knob *m_vertical=nullptr;
    controls::Knob *m_angular=nullptr;
    controls::Knob *m_bipolar=nullptr;
    controls::Knob *m_integer=nullptr;
    controls::Knob *m_disabled=nullptr;
};
}
#endif
