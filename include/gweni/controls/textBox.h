/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_textbox_h_
#define _gweni_controls_textbox_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/caretBlink.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/text/undoStack.h>

namespace gweni
{
namespace anim
{
class Animation;
}

namespace controls
{

class GWENI_EXPORT TextBox:public Label
{
    GWENI_CONTROL(TextBox, Label);

public:
    void renderFocus(skin::Base * /*skin*/) override {}

    void calculateSize(skin::Base *skin, Dim dim) override;
    void layout(skin::Base *skin) override;
    void arrange(skin::Base *skin, Dim dim) override;
    void postLayout(skin::Base *skin) override;

    //! Flips the caret's blink phase when it is due, if focused. Its
    //! animation is registered only while the box has the focus.
    virtual void updateCaretBlink();

    void onKeyboardFocus() override;
    void onLostKeyboardFocus() override;

    bool onChar(gweni::UnicodeChar c) override;

    //! The app's text: it is no edit, so it clears the undo history.
    void setText(const String &str, bool doEvents=true) override;

    //! A hint shown in the skin's PlaceholderText colour while the box is
    //! empty and does not have the keyboard focus (imgui's InputTextWithHint).
    //! It is not content: getText(), the caret, the length limit and the box's
    //! size see only the typed text, and a hint wider than the box is clipped
    //! by it. An empty string means no hint.
    virtual void setPlaceholder(const String &str);
    const String &getPlaceholder() const;
    //! Whether the hint is drawn now.
    bool isPlaceholderShown() const { return m_placeholder && m_placeholder->visible(); }
    //! The hint's Text, nullptr until a hint is set: placed like the typed
    //! text, inside the same padding with the same alignment and font.
    const internal::Text *getPlaceholderText() const { return m_placeholder; }

    using Label::setFont;
    void setFont(const gweni::Font &font) override;
    void setTextPadding(const Padding &padding) override;
    void setTextAlignment(Alignment alignment) override;
    void setWrap(bool b) override;

    //! Edits at the caret, each recorded on the undo stack (getUndoStack()),
    //! as typing when it comes from the keyboard, as a paste from Ctrl+V,
    //! and as an edit of its own (EditKind::Other) when the app calls it.
    virtual void insertText(const gweni::String &str);
    virtual void deleteText(int iStartPos, int iLength);

    //! Takes back the last undo step (Ctrl+Z), or does it again (Ctrl+Y,
    //! Ctrl+Shift+Z): the text, the caret and the selection as they were
    //! before, or after, it, with one onTextChanged. False, changing
    //! nothing, with no step to take or in a box that is not editable.
    virtual bool undo();
    virtual bool redo();
    //! Every edit of the box's text from its keyboard, its clipboard or
    //! insertText/deleteText, in steps (text::UndoStack's coalescing rules:
    //! typing joins a step until the caret moves, a newline is typed, the
    //! kind of edit changes or a second passes on the platform clock).
    //! Positions in its records are (0, byte offset into getText()).
    text::UndoStack &getUndoStack() { return m_undo; }
    const text::UndoStack &getUndoStack() const { return m_undo; }

    virtual void refreshCursorBounds();
    //! The selection and caret rectangles, from where m_text is now.
    void updateCursorRects();

    //! Where the caret goes, one unit wide and line-high, in this control's
    //! own coordinates (add getRenderBounds()'s position for its parent's).
    const gweni::Rect &getCaretBounds() const { return m_rectCaretBounds; }
    //! The selection behind the selected glyphs, same coordinates; empty
    //! without a selection, and always in a wrapped (multiline) box.
    const gweni::Rect &getSelectionBounds() const { return m_rectSelectionBounds; }
    //! The caret's blink phase: whether it is lit, when the box has the focus.
    bool isCaretVisible() const { return m_caretBlink.visible(); }
    //! The z of the glyphs' run: a rect drawn at it is behind every glyph.
    int getTextZIndex() const { return m_text->getZIndex(); }

    bool onKeyReturn(bool down) override;
    bool onKeyBackspace(bool down) override;
    bool onKeyDelete(bool down) override;
    bool onKeyRight(bool down) override;
    bool onKeyLeft(bool down) override;
    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;

    bool AccelOnlyFocus() override { return true; }

    void onPaste(event::Info info) override;
    void onCopy(event::Info info) override;
    void onCut(event::Info info) override;
    void onSelectAll(event::Info info) override;
    void onUndo(event::Info info);
    void onRedo(event::Info info);

    void onMouseDoubleClickLeft(int x, int y) override;

    virtual void eraseSelection();
    virtual bool hasSelection();
    virtual String getSelection();

    virtual void setCursorPos(int i);
    virtual void setCursorend(int i);
    //! The caret and the other end of the selection, byte offsets into
    //! getText() (equal without a selection).
    int getCursorPos() const { return m_cursorPos; }
    int getCursorEnd() const { return m_cursorEnd; }

    void setMaxTextLength(int maxLength) { m_maxTextLength=maxLength; }
    int getMaxTextLength() const { return m_maxTextLength; }

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

    virtual void setEditable(bool b) { m_editable=b; }

    //! Makes this box ask for its string's width instead of its request,
    //! however it is flagged (see sizedByRequest): for a box whose parent
    //! fits its text, as a property row fits its value, so the string is no
    //! longer scrolled in a box of a fixed width. Off by default.
    void setSizedByText(bool sizedByText);
    bool sizedByText() const { return m_sizedByText; }

    virtual void setSelectAllOnFocus(bool b)
    {
        m_selectAll=b;
        if(b)
            onSelectAll(this);
    }

    virtual void makeCaretVisible();

    virtual void onEnter();

protected:
    //! A box's content is the size it was asked for, on every size flag, as
    //! a ScrollControl's is: its string scrolls inside it, so typing never
    //! changes its preferred size, and a box with a non-Fixed flag fills what
    //! its parent gives it on that axis (a Top dock's width, say) without
    //! growing with its text. In X unless setSizedByText(); in Y only when
    //! its text wraps (a TextBoxMultiline), since one line's height does not
    //! change with the string, so an Elastic height is still the text line's
    //! (a property row's box). With no request it asks for its string.
    bool sizedByRequest(Dim dim) const override;

    //! A string that fits is placed by the text alignment in this box's
    //! padded content box, like any Label's (Placement::Parent).
    void alignText(Alignment textAlignment);
    //! A string that does not fit is scrolled to where the caret needs it:
    //! this box places it (Placement::Owner) at (x, y), at the size \p text
    //! measured, as wide as the content box when it wraps.
    void placeScrolledText(int x, int y, const Size &text);

public:

    bool needsInputChars() override { return true; }

    virtual void moveCaretToend();
    virtual void moveCaretToStart();

    event::Caller onTextChangedCaller;
    event::Caller onReturnPressedCaller;
    //! Fired when the box loses the keyboard focus, after its caret stopped:
    //! Return (which moves the focus on), Tab, a click elsewhere, hiding it.
    event::Caller onLostFocusCaller;

    static constexpr int NO_MAX_LENGTH=-1;

protected:

    void onTextChanged() override;

    //! Every focus change repaints the box through redraw(), once the focus
    //! has moved (Base::focus and unfocus, which the canvas drops a hidden or
    //! input-disabled control's focus through): the hint's visibility follows
    //! it there.
    void updateColors() override;
    //! Shows the hint if there is one, the box is visible and empty and does
    //! not have the focus; hides it otherwise. Only ever toggles its
    //! visibility, so nothing is laid out again.
    void updatePlaceholder();

    virtual bool isTextAllowed(const gweni::String &/*str*/, int /*pos*/)
    {
        return true;
    }

    //! The text this box holds for \p str, applied by insertText (before the
    //! selection is erased, isTextAllowed and the length cap) and setText.
    //! One line: the trailing line breaks dropped, every other one ("\r\n",
    //! a lone "\r", "\n") and every "\t" one space. Returns false, leaving
    //! \p converted alone, when \p str needs no conversion (so the common
    //! case copies nothing); true with the result in \p converted otherwise.
    //! A result needs no conversion again.
    virtual bool normalizeText(const String &str, String &converted) const;

    //! Positions are byte offsets into getText(); m_text may show other text
    //! for it (a PasswordTextBox's mask). displayPos maps a getText() offset
    //! to m_text's, realPos maps one of m_text's back, and every caret,
    //! selection, click and scroll query of m_text goes through them. The
    //! identity here.
    virtual int displayPos(int realPos) const { return realPos; }
    virtual int realPos(int displayPos) const { return displayPos; }

    bool m_editable;
    bool m_selectAll;

    int m_cursorPos;
    int m_cursorEnd;
    int m_cursorLine;

    int m_maxTextLength;

    gweni::Rect m_rectSelectionBounds;
    gweni::Rect m_rectCaretBounds;

    //! Lights the caret and holds it lit for a while (an edit, a move).
    void keepCaretLit();

    bool m_sizedByText=false;
    //! The caret's blink, its animation registered from the focus arriving
    //! until it leaves (or the blink finds the box unfocused).
    internal::CaretBlink m_caretBlink;

    //! The hint, a second Text beside m_text, made by the first setPlaceholder.
    internal::Text *m_placeholder=nullptr;

    //! The selection as the undo stack records it: anchor m_cursorEnd, caret
    //! m_cursorPos.
    text::Selection currentSelection() const;
    //! Sets the text of an edit, an undo or a redo through setText without
    //! clearing the undo history. False, with the history cleared, when the
    //! text was set again inside it (an onTextChanged handler): the edit is
    //! then not recorded.
    bool setEditedText(const String &str);
    //! Called by setText() for a text that changes: an edit's own set keeps
    //! the history, any other clears it.
    void textSetting();
    //! Clears the history for a step that does not fit the text; false.
    bool historyLost();
    //! Records an applied edit at \p start that took out \p removed and put
    //! in \p inserted, with the selection \p before it and the current one
    //! after it, as m_editKind.
    void recordEdit(int start, const String &removed, const String &inserted, const text::Selection &before);
    //! Sets the text an undo or redo step leaves, then its selection.
    void applyUndoStep(const String &str, text::Selection selection);

    //! What the edits made now are, for the undo stack's coalescing: set by
    //! the key, char and clipboard handlers around their edits.
    struct EditKindScope
    {
        EditKindScope(TextBox &box, text::EditKind kind): m_box(box), m_previous(box.m_editKind) { box.m_editKind=kind; }
        ~EditKindScope() { m_box.m_editKind=m_previous; }

        TextBox &m_box;
        text::EditKind m_previous;
    };

    text::UndoStack m_undo;
    text::EditKind m_editKind=text::EditKind::Other;
    //! Set while an edit sets the text, so setText() keeps the history;
    //! taken by that setText before its onTextChanged.
    bool m_applyingEdit=false;
    //! The text changes setText() made, for setEditedText to see one nested
    //! in its own.
    unsigned int m_textSets=0;
};


class GWENI_EXPORT TextBoxNumeric: public TextBox
{
public:

    GWENI_CONTROL(TextBoxNumeric, TextBox);

public:
    virtual float GetFloatFromText();

private:

    bool isTextAllowed(const gweni::String &str, int pos) override;

};


class GWENI_EXPORT TextBoxMultiline: public TextBox
{
public:

    GWENI_CONTROL(TextBoxMultiline, TextBox);

public:
    bool onKeyReturn(bool down) override;
    void makeCaretVisible() override;

    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;

    virtual int getCurrentLine();

protected:

    //! Lines: "\r\n" and a lone "\r" become "\n"; "\n" and "\t" are kept.
    bool normalizeText(const String &str, String &converted) const override;

    //! How many codepoints pos is from the start of its line.
    int columnOf(int pos);
    //! The position that many codepoints into the line, or its last caret.
    int columnOnLine(int line, int column);

};


class GWENI_EXPORT PasswordTextBox: public TextBox
{
public:

    GWENI_CONTROL(PasswordTextBox, TextBox);

public:
    void setText(const String &str, bool doEvents=true) override;

    //! The character shown for each codepoint of the password, "*" by
    //! default: exactly one valid UTF-8 codepoint (a U+2022 bullet, say), which the mask
    //! is rebuilt with at once. Anything else (empty, two codepoints, invalid
    //! UTF-8) is refused, keeping the current one; returns whether it took it.
    virtual bool setPasswordChar(const String &c);
    //! The same for an ASCII character; any other byte is refused.
    bool setPasswordChar(const char c);
    const String &getPasswordChar() const { return m_passwordChar; }

    const String &getText() const override { return m_realText; }
    //! The password's length in bytes, as getText()'s, not the mask's.
    int textLength() override { return static_cast<int>(m_realText.length()); }

    //! The password never reaches the clipboard: Ctrl+C does nothing, and so
    //! does Ctrl+X, which neither copies nor deletes the selection (a delete
    //! alone would look like a cut that worked). Both keys are still taken
    //! by the box. Paste and select-all are TextBox's.
    void onCopy(event::Info) override {}
    void onCut(event::Info) override {}

protected:

    //! A mask character for each codepoint of the password: its offsets are
    //! the number of codepoints before a getText() offset times the mask
    //! character's bytes. A byte utf8Next steps over alone (an invalid one)
    //! is one codepoint here too.
    int displayPos(int realPos) const override;
    int realPos(int displayPos) const override;

    //! Shows the mask for m_realText, one m_passwordChar per codepoint.
    void updateMask();

    String m_passwordChar;
    String m_realText;
};

}
}
#endif//_gweni_controls_textbox_h_
