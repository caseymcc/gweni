/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_folderpicker_h_
#define _gweni_controls_folderpicker_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/dialogs/folderOpen.h>
#include <gweni/controls/textBox.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT FolderPicker: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(FolderPicker, controls::Base)
    {
        m_button=new controls::Button(this);
        m_button->dock(Position::Right);
        m_button->setMargin(Margin(2, 0, 0, 0));
        m_button->setText("..");
        m_button->setSize(20, 20);
        m_button->onPressCaller.add(this, &FolderPicker::OnBrowse);
        m_textBox=new controls::TextBox(this);
        m_textBox->dock(Position::Fill);
        this->setSize(100, 20);
        m_browseName="Find Folder";
    }

public:
    void setFolder(const String &value)
    {
        m_textBox->setText(value);
        m_textBox->moveCaretToend();
        onFolderChanged.call(this);
    }

    const gweni::String &getFolder()
    {
        return m_textBox->getText();
    }

    void onBrowse(event::Info)
    {
        String folder;
        if(Dialogs::folderOpen(true, "Name", getFolder(), folder))
        {
            setFolder(folder);
        }
    }

    String getValue() override
    {
        return getFolder();
    }

    void setValue(const String &value) override
    {
        return setFolder(value);
    }

    event::Caller onFolderChanged;

private:
    controls::TextBox *m_textBox;
    controls::Button *m_button;
    gweni::String m_browseName;
};


}

}
#endif//_gweni_controls_folderpicker_h_
