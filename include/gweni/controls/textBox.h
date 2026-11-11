/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_textbox_h_
#define _gweni_controls_textbox_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>

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

    virtual void insertText(const gweni::String &str);
    virtual void deleteText(int iStartPos, int iLength);

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
    bool isCaretVisible() const { return m_caretVisible; }
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

    void onMouseDoubleClickLeft(int x, int y) override;

    virtual void eraseSelection();
    virtual bool hasSelection();
    virtual String getSelection();

    virtual void setCursorPos(int i);
    virtual void setCursorend(int i);

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

    float m_nextCaretBlink=0.0f;
    bool m_caretVisible=true;
    bool m_sizedByText=false;
    //! The caret's blink animation, registered from the focus arriving until
    //! it leaves (or the blink finds the box unfocused); null meanwhile.
    anim::Animation *m_caretBlink=nullptr;
    void startCaretBlink();
    void stopCaretBlink();

    //! The hint, a second Text beside m_text, made by the first setPlaceholder.
    internal::Text *m_placeholder=nullptr;
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
    virtual void setPasswordChar(const char c);

    const String &getText() const override { return m_realText; }

protected:

    char m_passwordChar;
    String m_realText;
};

}
}
#endif//_gweni_controls_textbox_h_
