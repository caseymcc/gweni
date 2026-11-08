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
#include <gweni/controls/colorButton.h>
#include <stdio.h>
#include <algorithm>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{
namespace property
{
//! A property whose value is "r,g,b" (or "r g b"), with a ColorButton swatch
//! of that colour at the right of its text box: a click on it opens an
//! HSVColorPicker in a Menu, which writes the value back as "r,g,b".
class ColorSelector: public property::Text
{
    GWENI_CONTROL_INLINE(ColorSelector, property::Text)
    {
        m_button=m_textBox->newChild<controls::ColorButton>();
        m_button->setDock(DockPosition::Right);
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
        picker->setDock(DockPosition::Center);
        picker->setSize(256, 128);
        // the swatch holds the colour the text last stood for (updateSwatch)
        picker->setColor(m_button->getColor(), false, true);
        picker->onColorChanged.add(this, &ColorSelector::colorChanged);

        menu->open(Alignment::Right |Alignment::Top);
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
        updateSwatch();
    }

    bool isEditing() override
    {
        return m_textBox == gweni::KeyboardFocus;
    }

    void doChanged() override
    {
        ParentClass::doChanged();
        updateSwatch();
    }

    //! The swatch takes the colour the text holds, three numbers 0-255
    //! separated by commas or spaces; other text leaves it as it was.
    void updateSwatch()
    {
        unsigned int r, g, b;
        String text=m_textBox->getText();

        std::replace(text.begin(), text.end(), ',', ' ');
#if defined(_WIN32) && !defined(__MINGW32__)
        if(sscanf_s(text.c_str(), "%u %u %u", &r, &g, &b) == 3 && r < 256 && g < 256 && b < 256)
#else
        if(sscanf(text.c_str(), "%u %u %u", &r, &g, &b) == 3 && r < 256 && g < 256 && b < 256)
#endif
        {
            m_button->setColor(gweni::Color(r, g, b));
        }
    }

    controls::ColorButton *m_button;
};


}
}
}
#endif//_gweni_controls_property_colorselector_h_
