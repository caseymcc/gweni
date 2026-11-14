/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#ifndef _gweni_rangeslider_example_h_
#define _gweni_rangeslider_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class RangeSlider;
}

class RangeSliderDemo: public controls::Base
{
public:
    GWENI_CONTROL(RangeSliderDemo, controls::Base);

public:

    //! The rows, for the range-slider scenario.
    controls::RangeSlider *m_price=nullptr;
    controls::RangeSlider *m_stepped=nullptr;
    controls::RangeSlider *m_joined=nullptr;
    controls::RangeSlider *m_disabled=nullptr;
    controls::RangeSlider *m_vertical=nullptr;
};

}//namespace gweni

#endif//_gweni_rangeslider_example_h_
