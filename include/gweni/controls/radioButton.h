/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_radiobutton_h_
#define _gweni_controls_radiobutton_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/labelClickable.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT RadioButton: public CheckBox
{
    GWENI_CONTROL(RadioButton, CheckBox);
    void render(skin::Base *skin) override;

private:

    // From CheckBox
    bool allowUncheck() override
    {
        return false;
    }

};


class GWENI_EXPORT LabeledRadioButton: public Base
{
public:

    GWENI_CONTROL_INLINE(LabeledRadioButton, Base)
    {
        //                setSize(200, 19);
        m_radioButton=new RadioButton(this);
        m_radioButton->dock(Position::Left);
        m_radioButton->setMargin(Margin(0, 2, 2, 2));
        m_radioButton->setTabable(false);
        m_radioButton->setKeyboardInputEnabled(false);
        m_label=new LabelClickable(this);
        m_label->setAlignment(Position::CenterV | Position::Left);
        m_label->setText("Radio Button");
        m_label->dock(Position::Fill);
        m_label->onPressCaller.add(m_radioButton, &CheckBox::onPress);
        m_label->setTabable(false);
        m_label->setKeyboardInputEnabled(false);
    }

    void renderFocus(gweni::skin::Base *skin) override
    {
        if(gweni::KeyboardFocus != this)
            return;

        if(!isTabable())
            return;

//        skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 0);
        skin->drawControl(this);
    }

    virtual RadioButton *getRadioButton()
    {
        return m_radioButton;
    }

    virtual LabelClickable *getLabel()
    {
        return m_label;
    }

    virtual void setText(const String &str, bool doEvents=true)
    {
        m_label->setText(str, doEvents);
    }

    virtual const String &getText() const
    {
        return m_label->getText();
    }

    bool onKeySpace(bool down) override
    {
        if(down)
            m_radioButton->setChecked(!m_radioButton->isChecked());

        return true;
    }

    virtual void select()
    {
        m_radioButton->setChecked(true);
    }

private:

    RadioButton *m_radioButton;
    LabelClickable *m_label;
};


}
}
#endif//_gweni_controls_radiobutton_h_
