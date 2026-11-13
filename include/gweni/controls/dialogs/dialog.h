/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_dialogs_dialog_h_
#define _gweni_controls_dialogs_dialog_h_

#include <gweni/controls/base.h>
#include <gweni/controls/dragger.h>
#include <gweni/gweni.h>

#include <functional>

namespace gweni
{
namespace controls
{
class Button;
class Label;
class ScrollControl;
class WindowCloseButton;
}

namespace dialogs
{

//! What closed a dialog: the button chosen, or its cancel result (Escape, the
//! close button, close() with none).
enum class Result
{
    Ok,
    Yes,
    No,
    Cancel
};

//! A dialog's result handler: the result and, for a prompt, its text (empty
//! otherwise). Called once, after the dialog has left the canvas's modal stack
//! and given the keyboard focus back, so it may open another dialog or delete
//! the control that opened this one.
using ResultHandler=std::function<void(Result, const String &)>;

//! A dialog's width or height: its content's (the default), a number of
//! units, or a fraction of the canvas's. Whichever it is, the dialog is
//! never larger than the canvas.
struct Extent
{
    enum class Kind
    {
        Content,
        Units,
        Fraction
    };

    Kind kind=Kind::Content;
    float value=0.0f;

    static Extent content() { return Extent(); }
    static Extent units(int units) { return Extent{Kind::Units, float(units)}; }
    static Extent fraction(float fraction) { return Extent{Kind::Fraction, fraction}; }

    //! The extent along an axis of \p canvas units, the content's being
    //! \p content, clamped to [0, canvas].
    int resolve(int canvas, int content) const;
};

namespace internal
{

//! A Dialog's title bar: its title and its close button, a drag handle that
//! moves the dialog (Dialog::dragTo). Drawn as a strip inside the dialog's
//! border (the DialogTitle surface).
class GWENI_EXPORT DialogTitleBar: public controls::internal::Dragger
{
public:

    GWENI_CONTROL(DialogTitleBar, controls::internal::Dragger);

public:
    static constexpr int Height=26;
};

//! A Dialog's footer row: its buttons, right-aligned in the order they were
//! added, on a strip inside the dialog's border (the DialogFooter surface).
//! Hidden until the first button is added.
class GWENI_EXPORT DialogFooter: public controls::Base
{
public:

    GWENI_CONTROL(DialogFooter, controls::Base);

public:
    //! The row the buttons are in, docked right.
    controls::Base *getRow() const { return m_row; }

protected:

    controls::Base *m_row=nullptr;
};

}//namespace internal

//! A modal dialog on a canvas (the base of message(), confirm() and prompt(),
//! query.h): a title bar with a close button, a content area that scrolls
//! (getContent(), a ScrollControl whose inner panel the app fills) and an
//! optional footer row of buttons (addButton()).
//!
//! It is a child of the canvas, made with canvas->newChild<Dialog>() (open()
//! moves one made elsewhere there), hidden until open() shows it and puts it
//! on the canvas's modal stack (Canvas::pushModal), so nothing under it takes
//! the input while it is open. It is owner-placed: its own arrange sizes and
//! places it, so a resize or a Canvas::setScale centres and clamps it again
//! without the app doing anything. Its width and height are each an Extent:
//! its content's, units or a fraction of the canvas, never more than the
//! canvas (setDialogSize(Extent::units(1200), Extent::fraction(0.92f))). It is
//! centred on the canvas until the user drags it by its title bar, then kept
//! where it was dragged to, inside the canvas, across a resize.
//!
//! close(result) hides it, takes it off the modal stack (which gives the focus
//! back to the control that had it before it opened), then calls the result
//! handler (setResultHandler) once and onClose, then deletes it (delayed)
//! unless setDeleteOnClose(false). Escape (with no Popup open over it and
//! nothing inside it taking the key), its close button, close() with no
//! result and setHidden(true) while open close it with its cancel result
//! (setCancelResult, Cancel by default). Deleting it or its canvas calls no
//! handler.
//!
//! The textured skin draws it as the Dialog surface, a window-like panel with
//! a border, on an opaque DialogShadow ShadowSize units down and right of it,
//! inside its bounds (so the shadow covers what is under it as the panel does:
//! a translucent one would hide the app's text there instead of darkening it,
//! as every glyph is drawn after every slot). No backdrop is drawn behind it.
class GWENI_EXPORT Dialog: public controls::Base
{
public:

    GWENI_CONTROL(Dialog, controls::Base);

public:
    //! Calls no handler.
    ~Dialog() override;

    //! The shadow's width below and right of the panel, in units, inside the
    //! dialog's bounds.
    static constexpr int ShadowSize=4;

    void setTitle(const String &title);
    String getTitle() const;
    controls::Label *getTitleLabel() const { return m_titleLabel; }

    //! A wrapped text under the title bar, above the content, as wide as the
    //! dialog (outside the content's ScrollControl, so it does not scroll):
    //! the standard dialogs' text. Hidden while empty, as it is at first.
    controls::Label *setMessage(const String &text);
    controls::Label *getMessage() const { return m_message; }

    //! The size the next layout gives it (see Extent): Extent::content() for
    //! both by default.
    void setDialogSize(Extent width, Extent height);
    const Extent &getDialogWidth() const { return m_width; }
    const Extent &getDialogHeight() const { return m_height; }

    //! The content area: a ScrollControl that scrolls vertically, filling the
    //! dialog between the title bar and the footer. Add the content to it
    //! (getContent()->newChild<...>()).
    controls::ScrollControl *getContent() const { return m_content; }

    //! Adds a button to the footer, right of those added before it, showing
    //! the footer. Its press does nothing on its own: connect onPressCaller.
    //! It takes the keyboard focus (Tab reaches it, Space presses it).
    controls::Button *addButton(const String &label);
    //! A footer button whose press closes the dialog with \p result.
    controls::Button *addButton(const String &label, Result result);
    internal::DialogFooter *getFooter() const { return m_footer; }

    //! The button Enter presses (see onKeyReturn()), and the one open() gives
    //! the keyboard focus to when no setInitialFocus() is set; null for none.
    void setDefaultButton(controls::Button *button) { m_defaultButton=button; }
    controls::Button *getDefaultButton() const { return m_defaultButton; }
    //! The control open() gives the keyboard focus to (a prompt's field); by
    //! default the default button.
    void setInitialFocus(controls::Base *control) { m_initialFocus=control; }

    //! Whether the close button shows and Escape closes it (true by default).
    void setClosable(bool closable);
    bool isClosable() const;
    controls::WindowCloseButton *getCloseButton() const { return m_closeButton; }

    //! Shows it and puts it on its canvas's modal stack (made the canvas's
    //! child first), then gives the keyboard focus to the initial focus or
    //! else the default button. Nothing while it is open, or with no canvas.
    virtual void open();
    //! Closes it with \p result (see the class comment). Nothing unless open.
    virtual void close(Result result);
    //! close(getCancelResult()).
    void close() { close(m_cancelResult); }
    bool isOpen() const { return m_open; }

    void setResultHandler(ResultHandler handler) { m_onResult=std::move(handler); }
    void setCancelResult(Result result) { m_cancelResult=result; }
    Result getCancelResult() const { return m_cancelResult; }
    //! The text the handler gets with \p result: empty here, a prompt's
    //! field's text for Ok there.
    virtual String resultText(Result /*result*/) const { return String(); }

    //! Deleted (delayedDelete()) once closed; true by default. Off, a closed
    //! dialog stays hidden on the canvas and may be opened again.
    void setDeleteOnClose(bool deleteOnClose) { m_deleteOnClose=deleteOnClose; }

    //! Hiding an open dialog closes it with its cancel result.
    void setHidden(bool hidden) override;

    //! Escape closes it with its cancel result while it is open and closable.
    bool onKeyEscape(bool down) override;
    //! Enter, from a focused control inside it that does not take it itself
    //! (anything but a multiline TextBox, a CodeEditor, a DragNumber's edit
    //! box, a Selectable: a single-line TextBox passes the press on), or with
    //! nothing focused: presses the focused footer button, else the default
    //! button, if it is shown and enabled.
    bool onKeyReturn(bool down) override;

    //! Moves it so its top-left is at \p pos (canvas units), kept inside the
    //! canvas, and keeps it there across resizes: what its title bar's drag
    //! does. False when it did not move.
    bool dragTo(const Point &pos);
    //! Whether the user put it somewhere (dragTo()); else it is centred.
    bool isUserPlaced() const { return m_userPlaced; }

    //! Its size from its Extents and its place, centred or where it was
    //! dragged to, inside the canvas, one axis per arrange (Placement::Owner:
    //! the canvas leaves both to it).
    void arrange(skin::Base *skin, Dim dim) override;

    //! Fired once it has closed, after the result handler, however it closed
    //! (not when it is deleted).
    event::Caller onClose;

protected:

    void onCloseButton(event::Info);
    void onTitleDragStart(event::Info);
    void onTitleDragged(event::Info);

    internal::DialogTitleBar *m_titleBar=nullptr;
    controls::Label *m_titleLabel=nullptr;
    controls::WindowCloseButton *m_closeButton=nullptr;
    controls::Label *m_message=nullptr;
    controls::ScrollControl *m_content=nullptr;
    internal::DialogFooter *m_footer=nullptr;
    controls::Button *m_defaultButton=nullptr;
    controls::Base *m_initialFocus=nullptr;

    Extent m_width;
    Extent m_height;
    bool m_open=false;
    bool m_closing=false;
    bool m_deleteOnClose=true;
    Result m_cancelResult=Result::Cancel;
    ResultHandler m_onResult;

    //! Where the user dragged it (its top-left before clamping), once
    //! m_userPlaced; the mouse less the dialog's top-left as the drag began.
    bool m_userPlaced=false;
    Point m_userPos;
    Point m_dragHold;
};

}//namespace dialogs
}//namespace gweni

#endif//_gweni_controls_dialogs_dialog_h_
