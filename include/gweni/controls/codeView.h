/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_codeview_h_
#define _gweni_controls_codeview_h_

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/codeEditor.h>

namespace gweni
{
namespace controls
{

class CodeView;

namespace internal
{

//! A CodeView's copy button (CodeView::setCopyButton): a Button the view
//! places over its viewport's top-right corner, shown while the view is
//! hovered or the button has the focus.
class GWENI_EXPORT CodeViewCopyButton: public Button
{
public:
    GWENI_CONTROL(CodeViewCopyButton, Button);
};

}//namespace internal

//! A read-only view of monospace text: a diff, a config file, a one-line
//! install command. It is a CodeEditor in its read-only mode
//! (CodeEditor::setReadOnly), so it has the editor's virtualized rows,
//! highlighting (setLanguage) and optional line numbers (setLineNumbers, off
//! by default), and none of its editing: it draws no caret and no current
//! line, and typing, the editing keys, the clipboard's cut and paste and undo
//! change nothing. The selection works as in the editor (a drag, Shift+click,
//! a double-click, Ctrl+A) and Ctrl+C copies it; Tab and Shift+Tab move the
//! focus.
//!
//! It sizes to its content: {Shrink, Shrink}, with no size asked (the list's
//! default size is not kept as a request), so placed by a Base parent (dock
//! None) it is as tall as its lines and as wide as its longest line
//! (CodeEditor::contentSize(): its cells, the one after them and TextInset on
//! both sides) inside its padding, with no bars; in a layout cell it is that
//! size cut to the cell, docked it is what the dock gives it, and a size the
//! app asks for (setSize) is its size. Wherever that is smaller than its
//! content, the content scrolls as in the editor. It is unwrapped.
//!
//! In the diff language (setLanguage("diff"), or setFileName() of a *.diff or
//! *.patch file) each row takes its line's kind from the line's tokens
//! (CodeLine::getDiffKind): a '+' line in a hunk Added, a '-' line Removed, an
//! "@@" header Hunk, which the skin draws as the row's line background (the
//! textured skin's DiffAdded, DiffRemoved and DiffHunk surfaces), the text in
//! the token colours as in any highlighted line. The kind is set as a row is
//! bound or its line's tokens change, so a scroll marks the rows it binds
//! alone.
//!
//! setCopyButton(true) gives it a copy button (internal::CodeViewCopyButton),
//! the view's own child placed CopyInset in from the viewport's top-right
//! corner, over the content: it does not scroll, and it is numbered after the
//! rows, so it draws in front of them. It is shown while the mouse is over the
//! view or anything in it (the button, a bar) or while it has the keyboard
//! focus, and hidden otherwise, so showing or hiding it writes its own slots
//! alone (the view's look does not follow the hover). A click (or Space with
//! the focus) is copy(): the selection, or the whole text when there is none,
//! to platform::setClipboardText() (which on the web needs the click's user
//! activation, as it has here), and the button shows CopiedLabel instead of
//! CopyLabel for CopiedSeconds on the canvas clock (Canvas::after()).
class GWENI_EXPORT CodeView: public CodeEditor
{
public:
    GWENI_CONTROL(CodeView, CodeEditor);

public:
    //! The units between the copy button and the viewport's top and right.
    static constexpr int CopyInset=4;
    //! How long the button says it copied, in seconds of the canvas clock.
    static constexpr double CopiedSeconds=1.5;
    static constexpr const char *CopyLabel="Copy";
    static constexpr const char *CopiedLabel="Copied";

    //! The copy button, shown or not. Default off; the button is made by the
    //! first setCopyButton(true).
    void setCopyButton(bool show);
    bool hasCopyButton() const { return m_copyButtonOn; }
    //! The button, null before the first setCopyButton(true).
    internal::CodeViewCopyButton *getCopyButton() const { return m_copyButton; }

    //! What copy() copies: the selection, or the whole text with none.
    String copyText() const;
    //! Puts copyText() on the clipboard and shows CopiedLabel on the button
    //! (if any) for CopiedSeconds, counted again from a copy meanwhile.
    void copy();
    //! Whether the button shows CopiedLabel.
    bool isCopied() const { return m_copied.valid(); }

    void onMouseEnter() override;
    void onMouseLeave() override;
    void onDescendantInteraction(Base *control) override;

    //! Highlights the text in the language of a file named \p fileName
    //! (text::languageForFileName: *.diff and *.patch are "diff"), none for a
    //! name of no known language; false then.
    bool setFileName(const String &fileName);

protected:
    //! With the diff language's tokenizer.
    bool showsDiffRows() const override;
    //! The rows, then the copy button over the viewport's top-right corner.
    void contentsPlaced(skin::Base *skin) override;

private:
    void onCopyPressed(event::Info info);
    //! Shows or hides the button for the hover and the focus.
    void updateCopyButton();
    //! The hovered control is this view or in it.
    bool hoverInside() const;
    void endCopied();

    internal::CodeViewCopyButton *m_copyButton=nullptr;
    bool m_copyButtonOn=false;
    //! The timer ending CopiedLabel, pending while it shows.
    TimerHandle m_copied;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_codeview_h_
