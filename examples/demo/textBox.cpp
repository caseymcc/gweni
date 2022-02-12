#include "demo/textBox.h"
#include "gweni/controls/textBox.h"

namespace gweni
{

TextBoxDemo::TextBoxDemo(const String &name):
    Base()
{
    {
        controls::TextBox *label=newChild<controls::TextBox>();
        label->setText("");
        label->setPos(10, 10);
        label->onTextChangedCaller.add(this, &TextBoxDemo::onEdit);
        label->onReturnPressedCaller.add(this, &TextBoxDemo::onSubmit);
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

}//namespace gweni
