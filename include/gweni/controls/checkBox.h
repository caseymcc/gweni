/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_checkbox_h_
#define _gweni_controls_checkbox_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/labelClickable.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT CheckBox: public Button
{
public:

    GWENI_CONTROL(CheckBox, Button);

public:
    //void render(skin::Base *skin) override;
    void onPress(event::Info) override;

    virtual void setChecked(bool checked);
    void toggle() override
    {
        setChecked(!isChecked());
    }

    virtual bool isChecked()
    {
        return m_checked;
    }

    //! Whether the label of the row this box belongs to (CheckBoxWithLabel,
    //! LabeledRadioButton) is hovered. A click on that label toggles the box,
    //! so the skin lights the box for it as for the box itself. Repaints the
    //! box when it changes.
    virtual void setLabelHovered(bool hovered);

    virtual bool isLabelHovered() const
    {
        return m_labelHovered;
    }

    //! Handlers for the label's onHoverEnter/onHoverLeave.
    void onLabelHoverEnter(event::Info);
    void onLabelHoverLeave(event::Info);

    gweni::event::Caller onChecked;
    gweni::event::Caller onUnChecked;
    gweni::event::Caller onCheckChanged;

private:

    // For derived controls
    virtual bool allowUncheck()
    {
        return true;
    }

    void onCheckStatusChanged();

    bool m_checked;
    bool m_labelHovered=false;
};


class GWENI_EXPORT CheckBoxWithLabel: public Base
{
public:
    GWENI_CONTROL_INLINE(CheckBoxWithLabel, Base)
    {
        setSize(200, 19);
        // One compact clickable row (identifier + label); don't let the layout
        // stretch it (which would float the label to the vertical centre).
        setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});

        m_checkbox=newChild<CheckBox>();
        m_checkbox->setDock(DockPosition::Left);
        m_checkbox->setMargin(Margin(0, 2, 2, 2));
        m_checkbox->setTabable(false);
        
        m_label=newChild<LabelClickable>();
        m_label->setDock(DockPosition::Center);
        m_label->onPressCaller.add(m_checkbox, &CheckBox::onPress);
        m_label->onHoverEnter.add(m_checkbox, &CheckBox::onLabelHoverEnter);
        m_label->onHoverLeave.add(m_checkbox, &CheckBox::onLabelHoverLeave);
        m_label->setTabable(false);
        setTabable(false);
    }

public:
    //! A container of its CheckBox and its label.
    GWENI_DRAWS_NOTHING

    virtual CheckBox *checkBox()
    {
        return m_checkbox;
    }

    virtual LabelClickable *label()
    {
        return m_label;
    }

    //! Put the text on the left of the checkbox (default: right).
    virtual void setLabelOnLeft(bool left)
    {
        m_checkbox->setDock(left ? DockPosition::Right : DockPosition::Left);
        m_label->setTextAlignment(left ? (Alignment::Right|Alignment::CenterV)
                                       : (Alignment::Left|Alignment::CenterV));
    }

    bool onKeySpace(bool down) override
    {
        if(!down)
            m_checkbox->setChecked(!m_checkbox->isChecked());

        return true;
    }

private:

    CheckBox *m_checkbox;
    LabelClickable *m_label;
};


}
}
#endif//_gweni_controls_checkbox_h_
