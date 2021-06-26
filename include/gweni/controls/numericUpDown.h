/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_numericupdown_h_
#define _gweni_controls_numericupdown_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/textBox.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT NumericUpDownButton_Up: public Button
{
    GWENI_CONTROL_INLINE(NumericUpDownButton_Up, Button)
    {
        setSize(7, 7);
    }

    void render(skin::Base *skin) override
    {
//        skin->drawNumericUpDownButton(this, skin::Generate, isDepressed(), true);
        skin->drawControl(this);
    }

};


class GWENI_EXPORT NumericUpDownButton_Down: public Button
{
    GWENI_CONTROL_INLINE(NumericUpDownButton_Down, Button)
    {
        setSize(7, 7);
    }

    void render(skin::Base *skin) override
    {
//        skin->drawNumericUpDownButton(this, skin::Generate, isDepressed(), false);
        skin->drawControl(this);
    }

};


class GWENI_EXPORT NumericUpDown: public TextBoxNumeric
{
public:

    GWENI_CONTROL(NumericUpDown, TextBoxNumeric);

    virtual void setMin(int i);
    virtual void setMax(int i);
    virtual void setIntValue(int i);
    virtual int getIntValue();

    event::Caller onChanged;

private:

    void onEnter() override;
    virtual void onChange();

    virtual void onButtonUp(event::Info);
    virtual void onButtonDown(event::Info);

    bool onKeyUp(bool down) override
    {
        if(down)
            onButtonUp(nullptr);

        return true;
    }

    bool onKeyDown(bool down) override
    {
        if(down)
            onButtonDown(nullptr);

        return true;
    }

    virtual int getIntValueUnclamped();

    int m_max;
    int m_min;
    int m_lastNumber;
};


}
}
#endif//_gweni_controls_numericupdown_h_
