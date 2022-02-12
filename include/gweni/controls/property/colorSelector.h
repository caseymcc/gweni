/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_colorselector_h_
#define _gweni_controls_property_colorselector_h_

#include <gweni/controls/properties.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/hsvColorPicker.h>
#include <gweni/controls/canvas.h>
#include <stdio.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{
namespace internal
{
class ColorButton: public Button
{
    GWENI_CONTROL_INLINE(ColorButton, Button)
    {
        setPrimitiveIdsSize(1);

        m_color=colors::Black;
        setText("");
    }

public:
    void render(skin::Base *skin) override
    {
        skin->getRenderer()->setDrawColor(m_color);
        skin->getRenderer()->drawFilledRect(m_primitiveIds[0], getRenderBounds(), getZIndex());
    }

    void setColor(const gweni::Color &col)
    {
        m_color=col;
    }

    gweni::Color m_color;
};


}

namespace property
{
class ColorSelector: public property::Text
{
    GWENI_CONTROL_INLINE(ColorSelector, property::Text)
    {
        m_button=m_textBox->newChild<controls::internal::ColorButton>();
        m_button->dock(Position::Right);
        m_button->setWidth(20);
        m_button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});
        m_button->onPressCaller.add(this, &ColorSelector::onButtonPress);
        m_button->setMargin(Margin(1, 1, 1, 2));
    }

public:
    void onButtonPress(event::Info info)
    {
        gweni::controls::Menu *menu=getCanvas()->newChild<Menu>();
        menu->setSize(256, 180);
        menu->setDeleteOnClose(true);
        menu->setDisableIconMargin(true);

        gweni::controls::HSVColorPicker *picker=menu->newChild<gweni::controls::HSVColorPicker>();
        picker->dock(Position::Fill);
        picker->setSize(256, 128);
        float defaultColor[3];
        toFloats(m_textBox->getText(), defaultColor, 3);
        picker->setColor(gweni::Color(defaultColor[0], defaultColor[1], defaultColor[2], 255),
            false, true);
        picker->onColorChanged.add(this, &ColorSelector::colorChanged);

        menu->open(Position::Right | Position::Top);
    }

    void colorChanged(event::Info info)
    {
        gweni::controls::HSVColorPicker *picker=
            gweni_cast<gweni::controls::HSVColorPicker>(info.controlCaller);
        Color col=picker->getColor();
        m_textBox->setText(format("%u,%u,%u", col.r, col.g, col.b));
        doChanged();
    }

    String getPropertyValue() override
    {
        return m_textBox->getText();
    }

    void setPropertyValue(const String &v, bool fireChangeEvents) override
    {
        m_textBox->setText(v, fireChangeEvents);
    }

    bool isEditing() override
    {
        return m_textBox == gweni::KeyboardFocus;
    }

    void doChanged() override
    {
        ParentClass::doChanged();
        unsigned int r, g, b;
#if defined(_WIN32) && !defined(__MINGW32__)
        if(sscanf_s(m_textBox->getText().c_str(), "%u,%u,%u", &r, &g, &b) == 3)
#else
        if(sscanf(m_textBox->getText().c_str(), "%u,%u,%u", &r, &g, &b) == 3)
#endif
        {
            m_button->setColor(gweni::Color(r, g, b));
        }
    }

    controls::internal::ColorButton *m_button;
};


}
}
}
#endif//_gweni_controls_property_colorselector_h_
