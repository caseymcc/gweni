/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_file_h_
#define _gweni_controls_property_file_h_

#include <gweni/controls/properties.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dialogs/fileOpen.h>

namespace gweni
{
namespace controls
{
namespace property
{
class File: public property::Text
{
    GWENI_CONTROL_INLINE(File, property::Text)
    {
        controls::Button *button=new controls::Button(this);
        button->setDock(DockPosition::Right);
        button->setText("...");
        button->setWidth(20);
        button->onPressCaller.add(this, &ThisClass::OnButtonPress);
        button->setMargin(Margin(1, 1, 1, 2));
        m_dialogName="Find File";
        m_fileExtension="*.*";
    }

public:
    File *setExtension(const gweni::String &string)
    {
        m_fileExtension=string;
        return this;
    }

    void onButtonPress(event::Info)
    {
        String fileChosen;
        if(Dialogs::fileOpen(true, m_dialogName,
            m_textBox->getText(), m_fileExtension, fileChosen))
        {
            m_textBox->setText(fileChosen);
        }
    }

    String m_dialogName;
    String m_fileExtension;
};


}
}
}

#endif//_gweni_controls_property_file_h_
