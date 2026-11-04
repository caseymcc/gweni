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
namespace controls
{

class GWENI_EXPORT TextBox:public Label
{
    GWENI_CONTROL(TextBox, Label);

public:
    void render(skin::Base *skin) override;
    void renderFocus(skin::Base * /*skin*/) override {}

    void layout(skin::Base *skin) override;
    void postLayout(skin::Base *skin) override;

#if GWENI_ANIMATE
    virtual void updateCaretColor();
#endif

    bool onChar(gweni::UnicodeChar c) override;

    virtual void insertText(const gweni::String &str);
    virtual void deleteText(int iStartPos, int iLength);

    virtual void refreshCursorBounds();

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

    virtual void setSelectAllOnFocus(bool b)
    {
        m_selectAll=b;
        if(b)
            onSelectAll(this);
    }

    virtual void makeCaretVisible();

    virtual void onEnter();

    bool needsInputChars() override { return true; }

    virtual void moveCaretToend();
    virtual void moveCaretToStart();

    event::Caller onTextChangedCaller;
    event::Caller onReturnPressedCaller;

    static constexpr int NO_MAX_LENGTH=-1;

protected:

    void onTextChanged() override;
    virtual bool isTextAllowed(const gweni::String &/*str*/, int /*pos*/)
    {
        return true;
    }

    bool m_editable;
    bool m_selectAll;

    int m_cursorPos;
    int m_cursorEnd;
    int m_cursorLine;

    int m_maxTextLength;

    gweni::Rect m_rectSelectionBounds;
    gweni::Rect m_rectCaretBounds;

    float m_nextCaretColorChange;
    gweni::Color m_caretColor;
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
    void render(skin::Base *skin) override;
    void makeCaretVisible() override;

    bool onKeyHome(bool down) override;
    bool onKeyend(bool down) override;
    bool onKeyUp(bool down) override;
    bool onKeyDown(bool down) override;

    virtual int getCurrentLine();

protected:

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
