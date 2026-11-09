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

public:
    //void render(skin::Base *skin) override;

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
        // One compact clickable row (identifier + label).
        setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});

        m_radioButton=newChild<RadioButton>();
        m_radioButton->setDock(DockPosition::Left);
        m_radioButton->setMargin(Margin(0, 2, 2, 2));
        m_radioButton->setTabable(false);
        m_radioButton->setKeyboardInputEnabled(false);
        
        m_label=newChild<LabelClickable>();
        m_label->setText("Radio Button");
        m_label->setDock(DockPosition::Center);
        m_label->onPressCaller.add(m_radioButton, &CheckBox::onPress);
        m_label->onHoverEnter.add(m_radioButton, &CheckBox::onLabelHoverEnter);
        m_label->onHoverLeave.add(m_radioButton, &CheckBox::onLabelHoverLeave);
        m_label->setTabable(false);
        m_label->setKeyboardInputEnabled(false);
    }

public:
    //! A container of its RadioButton and its label.
    GWENI_DRAWS_NOTHING

    void renderFocus(gweni::skin::Base *skin) override
    {
        if(gweni::KeyboardFocus != this)
            return;

        if(!isTabable())
            return;

//        skin->drawKeyboardHighlight(this, skin::Generate, getRenderBounds(), 0);
        //skin->drawControl(this);
        if(getStateChange() != StateChange_Nothing)
            m_skinControl->update(skin->getRenderer(), this);
    }

    virtual RadioButton *getRadioButton()
    {
        return m_radioButton;
    }

    virtual LabelClickable *getLabel()
    {
        return m_label;
    }

    //! Put the text on the left of the radio identifier (default: right).
    virtual void setLabelOnLeft(bool left)
    {
        m_radioButton->setDock(left ? DockPosition::Right : DockPosition::Left);
        m_label->setTextAlignment(left ? (Alignment::Right|Alignment::CenterV)
                                       : (Alignment::Left|Alignment::CenterV));
    }

    virtual void setText(const String &str, bool doEvents=true)
    {
        m_label->setText(str, doEvents);
    }

    virtual const String &getText() const
    {
        return m_label->getText();
    }

    //! Disables the radio and the label with the row, so neither a click on
    //! them nor Space selects it, and both draw their disabled look.
    void setDisabled(bool active) override
    {
        ParentClass::setDisabled(active);
        m_radioButton->setDisabled(active);
        m_label->setDisabled(active);
    }

    bool onKeySpace(bool down) override
    {
        if(isDisabled())
            return true;

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
