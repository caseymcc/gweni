/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dialogs_query_h_
#define _gweni_controls_dialogs_query_h_

#include <gweni/gweni.h>
#include <gweni/controls/dialogs/dialog.h>

namespace gweni
{
namespace controls
{
class Canvas;
class TextBox;
}

namespace dialogs
{

//! The buttons of a confirm(): Yes and No, or OK and Cancel.
enum class Buttons
{
    YesNo,
    OkCancel
};

//! The standard dialogs: a Dialog with a wrapped text, one or two footer
//! buttons and, for a prompt, a text field, opened at once on \p canvas.
//! Each returns the dialog, a handle to close(result) or close() it; it is
//! deleted once closed, so the handle is valid until the handler has run.
//!
//! \p onResult fires exactly once: with the button chosen (a click on it, or
//! Enter, which chooses the default button, the rightmost: OK or Yes), or with
//! the cancel result (No for YesNo, Cancel for OkCancel and a prompt, Ok for a
//! message) on Escape, the close button or close(). It runs after the dialog
//! has left the modal stack and given the keyboard focus back to the control
//! that held it before it opened (if that still exists and is visible), so it
//! may open another dialog or delete the control that opened this one. A
//! dialog opened over another (a confirm from a large Dialog) is modal to it,
//! and closing it hands the input back to it. Deleting the dialog or the
//! canvas calls no handler.
//!
//! The text wraps at TextWidth units; a dialog is never larger than the
//! canvas, and its text scrolls when the canvas is too small for it.
static constexpr int TextWidth=360;

//! The text and an OK button; the default and the cancel result are Ok.
GWENI_EXPORT Dialog *message(controls::Canvas *canvas, const String &title, const String &text,
    ResultHandler onResult=ResultHandler());

//! The text and No and Yes (Buttons::YesNo) or Cancel and OK (OkCancel), the
//! default rightmost (Yes, OK), the cancel result No or Cancel.
GWENI_EXPORT Dialog *confirm(controls::Canvas *canvas, const String &title, const String &text,
    Buttons buttons, ResultHandler onResult=ResultHandler());

//! prompt()'s dialog: the text, a single-line field and Cancel and OK. The
//! handler gets the field's text with Ok, and an empty string with Cancel.
class GWENI_EXPORT PromptDialog: public Dialog
{
public:

    GWENI_CONTROL(PromptDialog, Dialog);

public:
    controls::TextBox *getField() const { return m_field; }

    //! Gives the field the focus with its whole text selected.
    void open() override;

    String resultText(Result result) const override;

protected:

    controls::TextBox *m_field=nullptr;
};

//! The text and a field holding \p initial, focused with its text selected;
//! Enter in it chooses OK.
GWENI_EXPORT PromptDialog *prompt(controls::Canvas *canvas, const String &title, const String &text,
    const String &initial, ResultHandler onResult=ResultHandler());

}//namespace dialogs
}//namespace gweni

#endif//_gweni_controls_dialogs_query_h_
