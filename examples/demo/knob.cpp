/*
 * Gweni
 * Copyright (c) 2010 Facepunch Studios
 * See license in gweni.h
 */
#include "demo/knob.h"
#include <gweni/controls/knob.h>
#include <gweni/controls/label.h>

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(KnobDemo)
{
    setDock(DockPosition::Center);

    auto label=[this](const char *text, int x, int y)
    {
        auto *label=newChild<controls::Label>();
        label->setText(text);
        label->setPos(x, y);
    };
    auto knob=[this](const char *caption, int x, int y, int diameter)
    {
        auto *knob=newChild<controls::Knob>();
        knob->setCaption(caption);
        knob->setDiameter(diameter);
        knob->setPos(x, y);
        return knob;
    };

    label("Drag up to raise a value; Shift for fine steps.", 10, 10);

    m_vertical=knob("Vertical", 20, 40, 80);
    m_vertical->setRange(0, 10);
    m_vertical->setFormat("%.1f");
    m_vertical->setNumber(2.5);

    m_angular=knob("Angular", 155, 40, 80);
    m_angular->setDragMode(controls::Knob::DragMode::Angular);
    m_angular->setTickCount(11);
    m_angular->setNumber(0.65);

    m_bipolar=knob("Bipolar", 290, 40, 64);
    m_bipolar->setRange(-1, 1);
    m_bipolar->setBipolar(true);
    m_bipolar->setTickCount(9);
    m_bipolar->setNumber(-0.35);

    m_integer=knob("Integer", 20, 185, 48);
    m_integer->setRange(0, 10);
    m_integer->setInteger(true);
    m_integer->setNumber(4);

    m_disabled=knob("Disabled", 155, 185, 48);
    m_disabled->setNumber(0.75);
    m_disabled->setDisabled(true);

    label("Double-click, Ctrl+click or Return to type a value.", 10, 300);
    label("Arrows step; PageUp / PageDown step farther; Home / End.", 10, 322);
}

}
