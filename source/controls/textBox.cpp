/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/skin.h>
#include <gweni/anim.h>
#include <gweni/utility.h>
#include <gweni/platforms/platform.h>
#include <gweni/inputHandler.h>
#include <cmath>

namespace gweni
{
namespace controls
{

#if GWENI_ANIMATE
class ChangeCaretColor: public gweni::anim::Animation
{
public:

    void think() override
    {
        gweni_cast<TextBox>(m_control)->updateCaretColor();
    }

};
#endif // if GWENI_ANIMATE


GWENI_CONTROL_CONSTRUCTOR(TextBox)
{
//    enlargePrimitiveIds(this, m_primitiveIds, 11);

    setSize(200, 20);
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setAlignment(Alignment::Left|Alignment::CenterV);
    setPadding(Padding(4, 2, 4, 2));
    m_cursorPos=0;
    m_cursorEnd=0;
    m_cursorLine=0;
    m_editable=true;
    m_selectAll=false;
    m_maxTextLength=NO_MAX_LENGTH;
    setTextColor(gweni::Color(50, 50, 50, 255));         // TODO: From Skin
    setTabable(true);
    addAccelerator("Ctrl + C", &TextBox::onCopy);
    addAccelerator("Ctrl + X", &TextBox::onCut);
    addAccelerator("Ctrl + V", &TextBox::onPaste);
    addAccelerator("Ctrl + A", &TextBox::onSelectAll);
    gweni::anim::add(this, new ChangeCaretColor());
}

bool TextBox::onChar(gweni::UnicodeChar c)
{
    if(c == '\t')
        return false;

    insertText(String{c});
    return true;
}

void TextBox::insertText(const gweni::String &strInsert)
{
    if(!m_editable)
        return;

    if(hasSelection())
        eraseSelection();

    if(m_cursorPos > textLength())
        m_cursorPos=textLength();

    if(!isTextAllowed(strInsert, m_cursorPos))
        return;

    int insertSize=static_cast<int>(strInsert.size());
    if(m_maxTextLength > NO_MAX_LENGTH && textLength() + insertSize > m_maxTextLength)
    {
        insertSize=m_maxTextLength - textLength();

        if(insertSize <= 0)
            return;
    }

    String str=getText();
    str.insert(m_cursorPos, strInsert, 0, insertSize);
    setText(str);
    m_cursorPos+=insertSize;
    m_cursorEnd=m_cursorPos;
    m_cursorLine=0;
    refreshCursorBounds();
}

#if GWENI_ANIMATE
void TextBox::updateCaretColor()
{
    if(m_nextCaretColorChange > gweni::platform::getTimeInSeconds())
        return;

    if(!isFocussed())
    {
        m_nextCaretColorChange=gweni::platform::getTimeInSeconds()+0.5f; return;
    }

    gweni::Color targetcolor=gweni::Color(230, 230, 230, 255);

    if(m_caretColor == targetcolor)
        targetcolor=gweni::Color(20, 20, 20, 255);

    m_nextCaretColorChange=gweni::platform::getTimeInSeconds()+0.5f;
    m_caretColor=targetcolor;
    redraw();
}

#endif // if GWENI_ANIMATE

void TextBox::render(skin::Base *skin)
{
//    if(shouldDrawBackground())
////        skin->drawTextBox(this, skin::Generate);
//        skin->drawControl(this);
//
//    if(!isFocussed())
//        return;
//
//    // Draw selection.. if selected..
//    if(m_cursorPos != m_cursorEnd)
//    {
//        skin->getRenderer()->setDrawColor(gweni::Color(50, 170, 255, 200));
//        skin->getRenderer()->drawFilledRect(m_primitiveIds[9], m_rectSelectionBounds);
//    }
//
//    // Draw caret
//    skin->getRenderer()->setDrawColor(m_caretColor);
//    skin->getRenderer()->drawFilledRect(m_primitiveIds[10], m_rectCaretBounds);
    if(getStateChange() != StateChange_Nothing)
        m_skinControl->update(skin->getRenderer(), this);
}

void TextBox::refreshCursorBounds()
{
    m_nextCaretColorChange=gweni::platform::getTimeInSeconds()+1.5f;
    m_caretColor=getSkin()->Colors.Label.Bright;
    makeCaretVisible();
    gweni::Rect pA=getCharacterPosition(m_cursorPos);
    gweni::Rect pB=getCharacterPosition(m_cursorEnd);
    m_rectSelectionBounds.x=std::min(pA.x, pB.x);
    m_rectSelectionBounds.y=m_text->getY()-1;
    m_rectSelectionBounds.w=std::max(pA.x, pB.x)-m_rectSelectionBounds.x;
    m_rectSelectionBounds.h=m_text->getHeight()+2;
    m_rectCaretBounds.x=pA.x;
    m_rectCaretBounds.y=pA.y;
    m_rectCaretBounds.w=1;
    m_rectCaretBounds.h=pA.h;
    redraw();
}

void TextBox::onPaste(event::Info)
{
    insertText(platform::getClipboardText());
}

void TextBox::onCopy(event::Info)
{
    if(!hasSelection())
        return;

    platform::setClipboardText(getSelection());
}

void TextBox::onCut(event::Info)
{
    if(!hasSelection())
        return;

    platform::setClipboardText(getSelection());
    eraseSelection();
}

void TextBox::onSelectAll(event::Info)
{
    m_cursorEnd=0;
    m_cursorPos=textLength();
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::onMouseDoubleClickLeft(int /*x*/, int /*y*/)
{
    onSelectAll(this);
}

String TextBox::getSelection()
{
    if(!hasSelection())
        return "";

    int iStart=std::min(m_cursorPos, m_cursorEnd);
    int iEnd=std::max(m_cursorPos, m_cursorEnd);
    const String &str=getText();
    return str.substr(iStart, iEnd-iStart);
}

bool TextBox::onKeyReturn(bool down)
{
    if(down)
        return true;

    // Try to move to the next control, as if tab had been pressed
    onKeyTab(true);

    // If we still have focus, unfocus it.
    if(isFocussed())
        unfocus();

    // This is called AFTER the unfocusring so you can
    // refocus in your onReturnPressed hook.
    onEnter();
    return true;
}

bool TextBox::onKeyBackspace(bool down)
{
    if(!down)
        return true;

    if(hasSelection())
    {
        eraseSelection();
        return true;
    }

    if(m_cursorPos == 0)
        return true;

    deleteText(m_cursorPos-1, 1);
    return true;
}

bool TextBox::onKeyDelete(bool down)
{
    if(!down)
        return true;

    if(hasSelection())
    {
        eraseSelection();
        return true;
    }

    if(m_cursorPos >= textLength())
        return true;

    deleteText(m_cursorPos, 1);
    return true;
}

bool TextBox::onKeyLeft(bool down)
{
    if(!down)
        return true;

    if(m_cursorPos > 0)
        m_cursorPos--;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyRight(bool down)
{
    if(!down)
        return true;

    if(m_cursorPos < textLength())
        m_cursorPos++;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyHome(bool down)
{
    if(!down)
        return true;

    m_cursorPos=0;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBox::onKeyend(bool /*down*/)
{
    m_cursorPos=textLength();

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

void TextBox::setCursorPos(int i)
{
    if(m_cursorPos == i)
        return;

    m_cursorPos=i;
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::setCursorend(int i)
{
    if(m_cursorEnd == i)
        return;

    m_cursorEnd=i;
    refreshCursorBounds();
}

void TextBox::deleteText(int iStartPos, int iLength)
{
    if(!m_editable)
        return;

    String str=getText();
    str.erase(iStartPos, iLength);
    setText(str);

    if(m_cursorPos > iStartPos)
        setCursorPos(m_cursorPos-iLength);

    setCursorend(m_cursorPos);
}

bool TextBox::hasSelection()
{
    return m_cursorPos != m_cursorEnd;
}

void TextBox::eraseSelection()
{
    int iStart=std::min(m_cursorPos, m_cursorEnd);
    int iEnd=std::max(m_cursorPos, m_cursorEnd);
    deleteText(iStart, iEnd-iStart);
    // Move the cursor to the start of the selection,
    // since the end is probably outside of the string now.
    m_cursorPos=iStart;
    m_cursorEnd=iStart;
}

void TextBox::onMouseClickLeft(int x, int y, bool down)
{
    if(m_selectAll)
    {
        onSelectAll(this);
        m_selectAll=false;
        return;
    }

    int value=m_text->getClosestCharacter(m_text->canvasPosToLocal(gweni::Point(x, y)));

    if(down)
    {
        setCursorPos(value);

        if(!gweni::input::isShiftDown())
            setCursorend(value);

        gweni::MouseFocus=this;
    }
    else
    {
        if(gweni::MouseFocus == this)
        {
            setCursorPos(value);
            gweni::MouseFocus=nullptr;
        }
    }
}

void TextBox::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(gweni::MouseFocus != this)
        return;

    int value=m_text->getClosestCharacter(m_text->canvasPosToLocal(gweni::Point(x, y)));
    setCursorPos(value);
}

void TextBox::makeCaretVisible()
{
    if(m_text->getWidth() < getWidth())
    {
        m_text->setAlignment(getAlignment());
    }
    else
    {
        int iCaretPos=m_text->getCharacterPosition(m_cursorPos).x;
        int iRealCaretPos=iCaretPos+m_text->getX();
        int iSlidingZone=m_text->getFont().size + 1;   // getWidth()*0.1f

        // If the carat is already in a semi-good position, leave it.
        if(iRealCaretPos >= iSlidingZone && iRealCaretPos <= getWidth()-iSlidingZone)
            return;

        int x=0;

        if(iRealCaretPos > getWidth()-iSlidingZone)
            x=getWidth()-iCaretPos-iSlidingZone;

        if(iRealCaretPos < iSlidingZone)
            x=-iCaretPos+iSlidingZone;

        // Don't show too much whitespace to the right
        if(x+m_text->getWidth() < getWidth()-getPadding().right)
            x=-m_text->getWidth()+(getWidth()-getPadding().right);

        // Or the left
        if(x > getPadding().left)
            x=getPadding().left;

        int y=0;

        if(getAlignment() & Alignment::Top)
            y=getPadding().top;

        if(getAlignment() & Alignment::Bottom)
            y=getHeight()-m_text->getHeight()-getPadding().bottom;

        if(getAlignment() & Alignment::CenterV)
            y=(getHeight()-m_text->getHeight()) / 2;

        m_text->setPos(x, y);
    }
}

void TextBox::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
    refreshCursorBounds();
}

void TextBox::postLayout(skin::Base *skin)
{}

void TextBox::onTextChanged()
{
    if(m_cursorPos > textLength())
        m_cursorPos=textLength();

    if(m_cursorEnd > textLength())
        m_cursorEnd=textLength();

    onTextChangedCaller.call(this);
}

void TextBox::onEnter()
{
    onReturnPressedCaller.call(this);
}

void TextBox::moveCaretToend()
{
    m_cursorPos=textLength();
    m_cursorEnd=textLength();
    refreshCursorBounds();
}

void TextBox::moveCaretToStart()
{
    m_cursorPos=0;
    m_cursorEnd=0;
    refreshCursorBounds();
}

GWENI_CONTROL_CONSTRUCTOR(TextBoxMultiline)
{
    setWrap(true);
    setAlignment(Alignment::Left|Alignment::Top);
}

bool TextBoxMultiline::onKeyReturn(bool down)
{
    if(down)
        insertText("\n");

    return true;
}

void TextBoxMultiline::render(skin::Base *skin)
{
    if(shouldDrawBackground())
//        skin->drawTextBox(this, skin::Generate);
        skin->drawControl(this);

    if(!isFocussed())
        return;

    size_t index=0;

    if(m_cursorPos != m_cursorEnd)
    {
        unsigned int iCursorStartLine=m_text->getLineFromChar(m_cursorPos);
        unsigned int iCursorEndLine=m_text->getLineFromChar(m_cursorEnd);

        if(iCursorStartLine > m_text->numLines()-1)
            iCursorStartLine=m_text->numLines()-1;
        if(iCursorEndLine > m_text->numLines()-1)
            iCursorEndLine=m_text->numLines()-1;

        int iSelectionStartLine=(m_cursorPos < m_cursorEnd)?iCursorStartLine:iCursorEndLine;
        int iSelectionEndLine=(m_cursorPos < m_cursorEnd)?iCursorEndLine:iCursorStartLine;

        int iSelectionStartPos=(m_cursorPos < m_cursorEnd)?m_cursorPos:m_cursorEnd;
        int iSelectionEndPos=(m_cursorPos < m_cursorEnd)?m_cursorEnd:m_cursorPos;

        skin->getRenderer()->setDrawColor(gweni::Color(50, 170, 255, 200));
        m_rectSelectionBounds.h=m_text->getFont().size + 2;

        for(int iLine=iSelectionStartLine; iLine <= iSelectionEndLine; ++iLine)
        {
            gweni::Rect box=m_text->getLineBox(iLine);
            box.x+=m_text->getX();
            box.y+=m_text->getY();

            if(iLine == iSelectionStartLine)
            {
                gweni::Rect pos=getCharacterPosition(iSelectionStartPos);
                m_rectSelectionBounds.x=pos.x;
                m_rectSelectionBounds.y=pos.y-1;
            }
            else
            {
                m_rectSelectionBounds.x=box.x;
                m_rectSelectionBounds.y=box.y-1;
            }

            if(iLine == iSelectionEndLine)
            {
                gweni::Rect pos=getCharacterPosition(iSelectionEndPos);
                m_rectSelectionBounds.w=pos.x - m_rectSelectionBounds.x;
            }
            else
            {
                m_rectSelectionBounds.w=box.x + box.w - m_rectSelectionBounds.x;
            }
            if(m_rectSelectionBounds.w < 1)
                m_rectSelectionBounds.w=1;

//            if(m_primitiveIds.size() < index+1)
//                enlargePrimitiveIds(this, m_primitiveIds, index+1);

//            skin->getRenderer()->drawFilledRect(m_primitiveIds[index], m_rectSelectionBounds, getZIndex());
            index++;
        }
    }

    // Draw selection.. if selected..
    if(m_cursorPos != m_cursorEnd)
    {
        //skin->getRenderer()->setDrawColor( gweni::Color( 50, 170, 255, 200 ) );
        //skin->getRenderer()->drawFilledRect( m_rectSelectionBounds );
    }

    // Draw caret
    skin->getRenderer()->setDrawColor(m_caretColor);
    //skin->getRenderer()->drawFilledRect(m_primitiveIds[index], m_rectCaretBounds, getZIndex());
}

void TextBoxMultiline::makeCaretVisible()
{
    if(m_text->getHeight() < getHeight())
    {
        m_text->setAlignment(getAlignment());
    }
    else
    {
        //const Rect &bounds = getInnerBounds();

        //if ( pos  &Alignment::Top ) y = bounds.y + ypadding;
        //if ( pos  &Alignment::Bottom ) y = bounds.y + ( bounds.h - getHeight() - ypadding );
        //if ( pos  &Alignment::CenterV ) y = bounds.y + ( bounds.h - getHeight() )   *0.5;

        Rect pos=m_text->getCharacterPosition(m_cursorPos);
        int iCaretPos=pos.y; // + pos.h;
        int iRealCaretPos=iCaretPos+m_text->getY();
        //int iSlidingZone =  m_text->getFont()->size; //getWidth()*0.1f

        // If the carat is already in a semi-good position, leave it.
//        int mi = getPadding().top;
//        int ma = getHeight()-pos.h-getPadding().bottom;
        if(iRealCaretPos >= getPadding().top && iRealCaretPos <= getHeight()-pos.h-
            getPadding().bottom)
            return;

        int y=0;

        // bottom of carat too low
        if(iRealCaretPos > getHeight()-pos.h-getPadding().bottom)
        {
            //align bottom
            y=getHeight()-iCaretPos-pos.h-getPadding().bottom;
        }

        // top of carat too low
        if(iRealCaretPos < getPadding().top)
            y=-iCaretPos+getPadding().top;

        // Don't show too much whitespace to the bottom
        if(y+m_text->getHeight() < getHeight()-getPadding().bottom)
            y=-m_text->getHeight()+(getHeight()-getPadding().bottom);

        // Or the top
        if(y > getPadding().top)
            y=getPadding().top;

        int x=0;
        if(getAlignment() & Alignment::Left)
            x=getPadding().left;
        if(getAlignment() & Alignment::Right)
            x=getWidth()-m_text->getWidth()-getPadding().right;
        if(getAlignment() & Alignment::CenterH)
            x=(getWidth()-m_text->getWidth())*0.5f;

        m_text->setPos(x, y);
    }
}

int TextBoxMultiline::getCurrentLine()
{
    return m_text->getLineFromChar(m_cursorPos);
}

bool TextBoxMultiline::onKeyHome(bool down)
{
    if(!down)
        return true;

    int iCurrentLine=getCurrentLine();
    int value=m_text->getStartCharFromLine(iCurrentLine);
    m_cursorLine=0;
    m_cursorPos=value;

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyend(bool down)
{
    if(!down)
        return true;

    int iCurrentLine=getCurrentLine();
    int value=m_text->getEndCharFromLine(iCurrentLine);
    m_cursorLine=0;
    m_cursorPos=value;

    int iLastLine=m_text->numLines()-1;

    if(iCurrentLine < iLastLine && value > 0)
        m_cursorPos=value-1; // NAUGHTY
    else
        m_cursorPos=m_text->length();

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyUp(bool down)
{
    if(!down)
        return true;
    //if ( m_cursorLine == 0 )
    m_cursorLine=m_text->getCharPosOnLine(m_cursorPos);

    int iLine=m_text->getLineFromChar(m_cursorPos);

    if(iLine == 0)
        return true;

    m_cursorPos=m_text->getStartCharFromLine(iLine-1);
    m_cursorPos+=clamp(m_cursorLine, 0, m_text->getLine(iLine-1)->length()-1);
    m_cursorPos=clamp(m_cursorPos, 0, m_text->length());

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

bool TextBoxMultiline::onKeyDown(bool down)
{
    if(!down)
        return true;
    //if ( m_cursorLine == 0 )
    m_cursorLine=m_text->getCharPosOnLine(m_cursorPos);

    int iLine=m_text->getLineFromChar(m_cursorPos);
    int iLastLine=m_text->numLines()-1;
    if(iLine >= iLastLine || iLastLine < 1)
        return true;

    m_cursorPos=m_text->getStartCharFromLine(iLine+1);
    if(iLine+1 >= iLastLine)
        m_cursorPos+=clamp(m_cursorLine, 0, m_text->getLine(iLine+1)->length());
    else
        m_cursorPos+=clamp(m_cursorLine, 0, m_text->getLine(iLine+1)->length()-1);
    m_cursorPos=clamp(m_cursorPos, 0, m_text->length());

    if(!gweni::input::isShiftDown())
        m_cursorEnd=m_cursorPos;

    refreshCursorBounds();
    return true;
}

GWENI_CONTROL_CONSTRUCTOR(PasswordTextBox)
{
    m_realText="";
    m_passwordChar='*';
}

void PasswordTextBox::setText(const String &str, bool doEvents)
{
    if(m_realText == str)
        return;

    m_realText=str;
    String passwordChars;
    for(size_t i=0; i < m_realText.length(); i++)
    {
        passwordChars+=m_passwordChar;
    }

    m_text->setText(passwordChars);
    redraw();

    if(doEvents)
        onTextChanged();
}

void PasswordTextBox::setPasswordChar(const char c)
{
    m_passwordChar=c;
}

}//namespace controls
}//namespace gweni
