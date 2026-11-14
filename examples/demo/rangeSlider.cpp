/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */

#include "demo/rangeSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/rangeSlider.h"

namespace gweni
{

// Range sliders (controls::RangeSlider): a price band with each thumb's value
// as "%.0f" and a gap of 50, a stepped one on its notches, a joined "lo – hi"
// text, a disabled one, and a vertical one with its values in a column on its
// right. Each row has a Label naming it.
GWENI_CONTROL_CONSTRUCTOR(RangeSliderDemo)
{
    setDock(DockPosition::Center);

    auto row=[this](int y, int height, const char *caption) {
        controls::Label *label=newChild<controls::Label>();
        label->setText(caption);
        label->setPos(10, y+(height-14)/2);

        controls::RangeSlider *slider=newChild<controls::RangeSlider>();
        slider->setPos(80, y);
        slider->setSize(240, height);
        return slider;
    };

    m_price=row(10, 20, "Price");
    m_price->setRange(0, 1000);
    m_price->setMinimumGap(50);
    m_price->setValues(200, 800);
    m_price->setValueFormat("%.0f");

    // room for its ticks below the track
    m_stepped=row(40, 30, "Stepped");
    m_stepped->setRange(0, 100);
    m_stepped->setNotchCount(10);
    m_stepped->setClampToNotches(true);
    m_stepped->setValues(20, 60);

    m_joined=row(80, 20, "Hours");
    m_joined->setRange(0, 24);
    m_joined->setValues(9, 17);
    m_joined->setValueTextMode(controls::RangeSlider::ValueTextMode::Joined);
    m_joined->setValueFormat("%.0f");

    m_disabled=row(110, 20, "Disabled");
    m_disabled->setRange(0, 10);
    m_disabled->setValues(2.5, 7.5);
    m_disabled->setValueFormat("%.1f");
    m_disabled->setDisabled(true);

    m_vertical=newChild<controls::RangeSlider>();
    m_vertical->setHorizontal(false);
    m_vertical->setPos(350, 10);
    m_vertical->setSize(20, 180);
    m_vertical->setRange(-50, 50);
    m_vertical->setValues(-10, 30);
    m_vertical->setValueFormat("%+.0f");
}

}//namespace gweni
