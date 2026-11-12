#include "demo/textBox.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/textBox.h"
#include <gweni/controls/button.h>
#include <gweni/platforms/platform.h>

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(TextBoxDemo)
{
    setLayout(new controls::VerticalLayout());

    {
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("");
        label->setPlaceholder("Type here...");
        label->setPos(10, 10);
        label->onTextChangedCaller.add(this, &TextBoxDemo::onEdit);
        label->onReturnPressedCaller.add(this, &TextBoxDemo::onSubmit);
        m_first=label;
    }
    {
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("Normal Everyday Label");
        label->setPos(10, 10 + 25);
    }
    {
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("select All Text On Focus");
        label->setPos(10, 10 + 25 * 2);
        label->setSelectAllOnFocus(true);
    }
    {
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("Different Coloured Text, for some reason");
        label->setTextColor(Color(255, 0, 255, 255));
        label->setPos(10, 10 + 25 * 3);
    }
    {
        controls::TextBoxNumeric *label=newChild<controls::TextBoxNumeric>();
        label->setText("2004");
        label->setTextColor(Color(255, 0, 255, 255));
        label->setPos(10, 10 + 25 * 4);
    }
    {
        // the hint is not masked; what is typed is
        controls::PasswordTextBox *label=newChild<controls::PasswordTextBox>();
        label->setPlaceholder("Password");
    }
    {
        // the hint is not the value: GetFloatFromText() reads the typed text
        controls::TextBoxNumeric *label=newChild<controls::TextBoxNumeric>();
        label->setText("");
        label->setPlaceholder("Year");
    }
    {
//        m_font.facename="Impact";
//        m_font.size=50;
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("Different Font");
        label->setPos(10, 10 + 25 * 5);
//        label->setFont(m_font);
        label->setSize(200, 55);
    }
    {
        controls::TextBoxMultiline *label=newChild<controls::TextBoxMultiline>();
        label->setText("I think we're losing sight of the real issue here, which is: what are we gonna call ourselves?\n\nErm, and I think it comes down to a choice between \"The League Against Salivating Monsters\" or my own personal preference, which is \"The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society.\" Erm, one drawback with that: the abbreviation is \"CLITORIS.\"");
        label->setPos(220, 10);
        label->setSize(200, 180);
    }
    {
        // a copy button: platform::setClipboardText() from its press, inside the
        // click's user activation, which the browser's clipboard needs
        controls::Button *copy=newChild<controls::Button>();
        copy->setText("Copy");
        copy->setToolTipText("Copy the first box's text");
        copy->sizeToContents();
        copy->onPressCaller.add(this, &TextBoxDemo::onCopy);
    }
}

void TextBoxDemo::onEdit(event::Info info)
{

//    controls::TextBox *textbox=(controls::TextBox *)(control);
    //			UnitPrint( Format( "Textbox Edit: [%ls]\n", textbox->getText().getUnicode().c_str() ) );
}

void TextBoxDemo::onSubmit(event::Info info)
{
//    controls::TextBox *textbox=(controls::TextBox *)(control);
    //			UnitPrint( Format( "Textbox Submit: [%ls]\n", textbox->getText().getUnicode().c_str() ) );
}

void TextBoxDemo::onCopy(event::Info)
{
    platform::setClipboardText(m_first->getText());
}

}//namespace gweni
