/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/gweni.h>
#include <gweni/controls/dialogs/query.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>

namespace gweni
{
namespace dialogs
{

namespace
{

//! The units around the text across the dialog: its border and shadow, and
//! the message's margins.
constexpr int SideUnits=2*1+Dialog::ShadowSize+2*10;

//! A dialog of \p T on \p canvas with \p title and \p text, TextWidth wide,
//! as high as its content.
template<typename T>
T *makeDialog(controls::Canvas *canvas, const String &title, const String &text, ResultHandler onResult)
{
    T *dialog=canvas->newChild<T>();

    dialog->setTitle(title);
    dialog->setDialogSize(Extent::units(TextWidth+SideUnits), Extent::content());
    dialog->setResultHandler(std::move(onResult));

    dialog->setMessage(text);
    // nothing scrolls: the text and the field are the dialog's own
    dialog->getContent()->hide();
    return dialog;
}

}//namespace

Dialog *message(controls::Canvas *canvas, const String &title, const String &text, ResultHandler onResult)
{
    if(!canvas)
        return nullptr;

    Dialog *dialog=makeDialog<Dialog>(canvas, title, text, std::move(onResult));

    dialog->setCancelResult(Result::Ok);
    dialog->setDefaultButton(dialog->addButton("OK", Result::Ok));
    dialog->open();
    return dialog;
}

Dialog *confirm(controls::Canvas *canvas, const String &title, const String &text, Buttons buttons,
    ResultHandler onResult)
{
    if(!canvas)
        return nullptr;

    Dialog *dialog=makeDialog<Dialog>(canvas, title, text, std::move(onResult));

    if(buttons == Buttons::YesNo)
    {
        dialog->setCancelResult(Result::No);
        dialog->addButton("No", Result::No);
        dialog->setDefaultButton(dialog->addButton("Yes", Result::Yes));
    }
    else
    {
        dialog->setCancelResult(Result::Cancel);
        dialog->addButton("Cancel", Result::Cancel);
        dialog->setDefaultButton(dialog->addButton("OK", Result::Ok));
    }
    dialog->open();
    return dialog;
}

GWENI_CONTROL_CONSTRUCTOR(PromptDialog)
{
    // under the message, above the (hidden) content
    m_field=newChild<controls::TextBox>();
    m_field->setDock(DockPosition::Top);
    m_field->setMargin(Margin(10, 0, 10, 10));
    // the dialog's width, its own height
    m_field->setSizeFlags({SizeFlag::Expand, m_field->getSizeFlags().vertical});
    m_field->setName("field");
    setInitialFocus(m_field);
}

void PromptDialog::open()
{
    const bool wasOpen=isOpen();

    Dialog::open();

    if(!wasOpen && isOpen() && m_field->isFocussed())
        m_field->onSelectAll(m_field);
}

String PromptDialog::resultText(Result result) const
{
    return (result == Result::Ok) ? m_field->getText() : String();
}

PromptDialog *prompt(controls::Canvas *canvas, const String &title, const String &text, const String &initial,
    ResultHandler onResult)
{
    if(!canvas)
        return nullptr;

    PromptDialog *dialog=makeDialog<PromptDialog>(canvas, title, text, std::move(onResult));

    dialog->getField()->setText(initial);
    dialog->setCancelResult(Result::Cancel);
    dialog->addButton("Cancel", Result::Cancel);
    dialog->setDefaultButton(dialog->addButton("OK", Result::Ok));
    dialog->open();
    return dialog;
}

}//namespace dialogs
}//namespace gweni
