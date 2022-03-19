/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_property_folder_h_
#define _gweni_controls_property_folder_h_

#include <gweni/controls/properties.h>
#include <gweni/controls/button.h>
#include <gweni/controls/dialogs/folderOpen.h>

namespace gweni
{
namespace controls
{
namespace property
{
class Folder: public property::Text
{
    GWENI_CONTROL_INLINE(Folder, property::Text)
    {
        controls::Button *button=new controls::Button(this);
        button->setDock(DockPosition::Right);
        button->setText("...");
        button->setWidth(20);
        button->onPressCaller.add(this, &ThisClass::OnButtonPress);
        button->setMargin(Margin(1, 1, 1, 2));
        m_dialogName="Find Folder";
    }

public:
    void onButtonPress(event::Info)
    {
        String folder;
        if(Dialogs::folderOpen(true, m_dialogName,
            m_textBox->getText(), folder))
        {
            m_textBox->setText(folder);
        }
    }

    String m_dialogName;
};


}
}
}
#endif//_gweni_controls_property_folder_h_
