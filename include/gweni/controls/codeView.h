/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_codeview_h_
#define _gweni_controls_codeview_h_

#include <gweni/controls/codeEditor.h>

namespace gweni
{
namespace controls
{

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
class GWENI_EXPORT CodeView: public CodeEditor
{
public:
    GWENI_CONTROL(CodeView, CodeEditor);

public:
    //! Highlights the text in the language of a file named \p fileName
    //! (text::languageForFileName: *.diff and *.patch are "diff"), none for a
    //! name of no known language; false then.
    bool setFileName(const String &fileName);

protected:
    //! With the diff language's tokenizer.
    bool showsDiffRows() const override;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_codeview_h_
