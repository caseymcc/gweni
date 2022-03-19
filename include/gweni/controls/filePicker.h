/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_filepicker_h_
#define _gweni_controls_filepicker_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/dialogs/fileOpen.h>
#include <gweni/controls/textBox.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT FilePicker: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(FilePicker, controls::Base)
    {
        m_button=new controls::Button(this);
        m_button->setDock(DockPosition::Right);
        m_button->setMargin(Margin(2, 0, 0, 0));
        m_button->setText("..");
        m_button->setSize(20, 20);
        m_button->onPressCaller.add(this, &FilePicker::OnBrowse);
        m_textBox=new controls::TextBox(this);
        m_textBox->setDock(DockPosition::Center);
        this->setSize(100, 20);
        setFileType("Any Type | *.*");
    }

public:
    void setFileType(const gweni::String &string)
    {
        m_fileType=string;
    }

    gweni::String getFileType()
    {
        return m_fileType;
    }

    void setFileName(const String &value)
    {
        m_textBox->setText(value);
        m_textBox->moveCaretToend();
        onFileChanged.call(this);
    }

    const gweni::String &getFileName()
    {
        return m_textBox->getText();
    }

    void onBrowse(event::Info)
    {
        String fileChosen;
        if(Dialogs::fileOpen(true, "Name", "Start Path", m_fileType, fileChosen))
        {
            setFileName(fileChosen);
        }
    }

    String getValue() override
    {
        return getFileName();
    }

    void setValue(const String &value) override
    {
        return setFileName(value);
    }

    event::Caller onFileChanged;

private:

    controls::TextBox *m_textBox;
    controls::Button *m_button;

    String m_fileType;
};


}

}
#endif//_gweni_controls_filepicker_h_
