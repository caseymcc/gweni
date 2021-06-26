/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/utility.h>
#include <gweni/skin.h>
#include <gweni/controls/numericUpDown.h>
#include <cmath>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(NumericUpDown)
{
    setSize(100, 20);
    controls::Base *splitter=new controls::Base(this);
    splitter->dock(Position::Right);
    splitter->setWidth(13);
    NumericUpDownButton_Up *buttonUp=new NumericUpDownButton_Up(splitter);
    buttonUp->onPressCaller.add(this, &NumericUpDown::onButtonUp);
    buttonUp->setTabable(false);
    buttonUp->dock(Position::Top);
    buttonUp->setHeight(10);
    NumericUpDownButton_Down *buttonDown=new NumericUpDownButton_Down(splitter);
    buttonDown->onPressCaller.add(this, &NumericUpDown::onButtonDown);
    buttonDown->setTabable(false);
    buttonDown->dock(Position::Fill);
    buttonUp->setPadding(Padding(0, 1, 1, 0));
    m_max=100;
    m_min=0;
    m_lastNumber=0;
    setText("0");
}

void NumericUpDown::onButtonUp(event::Info)
{
    setIntValue(getIntValueUnclamped() + 1);
}

void NumericUpDown::onButtonDown(event::Info)
{
    setIntValue(getIntValueUnclamped() - 1);
}

int NumericUpDown::getIntValueUnclamped()
{
    return static_cast<int>(std::lround(GetFloatFromText()));
}

int NumericUpDown::getIntValue()
{
    return clamp(getIntValueUnclamped(), m_min, m_max);
}

void NumericUpDown::setMin(int i)
{
    m_min=i;
}

void NumericUpDown::setMax(int i)
{
    m_max=i;
}

void NumericUpDown::setIntValue(int i)
{
    i=clamp(i, m_min, m_max);

    setText(toString(i));

    if(m_lastNumber != i)
    {
        m_lastNumber=i;
        onChange();
    }
}

void NumericUpDown::onChange()
{
    onChanged.call(this);
}

void NumericUpDown::onEnter()
{
    setIntValue(getIntValueUnclamped());
    ParentClass::onEnter();
}

}//namespace controls
}//namespace gweni
