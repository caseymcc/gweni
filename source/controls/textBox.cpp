/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/canvas.h>
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

// Positions in a TextBox are byte offsets into its UTF-8 getText(), and every
// one of them is kept on a codepoint boundary: the editing steps below move by
// whole codepoints, and a position set from outside is snapped back to one.
static int snapPos(const String &str, int pos)
{
    return static_cast<int>(utf8Snap(str, static_cast<size_t>(std::max(pos, 0))));
}

static int nextPos(const String &str, int pos)
{
    return static_cast<int>(utf8Next(str, static_cast<size_t>(pos)));
}

static int prevPos(const String &str, int pos)
{
    return static_cast<int>(utf8Prev(str, static_cast<size_t>(pos)));
}

// The caret blinks with this period, and stays lit this long after an edit, a
// cursor move or the focus arriving.
static constexpr float caretBlinkPeriod=0.5f;
static constexpr float caretSolidTime=1.5f;

#if GWENI_ANIMATE
class CaretBlink: public gweni::anim::Animation
{
public:

    void think() override
    {
        gweni_cast<TextBox>(m_control)->updateCaretBlink();
    }

};
#endif // if GWENI_ANIMATE


GWENI_CONTROL_CONSTRUCTOR(TextBox)
{
//    enlargePrimitiveIds(this, m_primitiveIds, 11);

    setSize(200, 20);
    // the box scrolls its text to the caret, so its size is the one asked for,
    // never its text's (a Label's Elastic)
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setPadding(Padding(4, 2, 4, 2));
    m_cursorPos=0;
    m_cursorEnd=0;
    m_cursorLine=0;
    m_editable=true;
    m_selectAll=false;
    m_maxTextLength=NO_MAX_LENGTH;
    setSkinTextColor(internal::SkinTextColor::LabelDefault);
    setTabable(true);
    addAccelerator("Ctrl + C", &TextBox::onCopy);
    addAccelerator("Ctrl + X", &TextBox::onCut);
    addAccelerator("Ctrl + V", &TextBox::onPaste);
    addAccelerator("Ctrl + A", &TextBox::onSelectAll);
}

bool TextBox::onChar(gweni::UnicodeChar c)
{
    if(c == '\t')
        return false;

    const String str=utf8Encode(c);

    if(str.empty())
        return false;

    insertText(str);
    return true;
}

void TextBox::insertText(const gweni::String &strInsert)
{
    if(!m_editable)
        return;

    if(hasSelection())
        eraseSelection();

    m_cursorPos=snapPos(getText(), std::min(m_cursorPos, textLength()));

    if(!isTextAllowed(strInsert, m_cursorPos))
        return;

    int insertSize=static_cast<int>(strInsert.size());
    if(m_maxTextLength > NO_MAX_LENGTH && textLength() + insertSize > m_maxTextLength)
    {
        // The limit is in bytes; a codepoint that does not fit whole is left out.
        insertSize=snapPos(strInsert, m_maxTextLength - textLength());

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

// Unfocused, the caret is not drawn and nothing here changes. Focused, each
// blink edge is one StateChange_Caret, which only shows or hides the caret.
void TextBox::updateCaretBlink()
{
    // the focus went without onLostKeyboardFocus (~Base clearing it, a
    // direct KeyboardFocus assignment): stop here
    if(!isFocussed())
    {
        stopCaretBlink();
        return;
    }

    const float now=gweni::platform::getTimeInSeconds();

    if(m_nextCaretBlink > now)
        return;

    m_nextCaretBlink=now+caretBlinkPeriod;
    m_caretVisible=!m_caretVisible;
    addStateChange(StateChange_Caret);

    if(Canvas *canvas=getCanvas())
        canvas->redraw();
}

void TextBox::setText(const String &str, bool doEvents)
{
    ParentClass::setText(str, doEvents);
    updatePlaceholder();
}

// The hint is a Text of its own rather than a text id of the skin's: it is
// laid out, clipped, scaled and shown or hidden like m_text, and it does not
// count in the box's size (doNotIncludeInSize).
void TextBox::setPlaceholder(const String &str)
{
    if(!m_placeholder)
    {
        if(str.empty())
            return;

        m_placeholder=newChild<internal::Text>();
        m_placeholder->doNotIncludeInSize();
        m_placeholder->setFont(m_text->getFont());
        m_placeholder->setPadding(m_text->getPadding());
        m_placeholder->setAlignment(getTextAlignment());
        m_placeholder->setSkinTextColor(internal::SkinTextColor::Placeholder);
    }

    m_placeholder->setText(str);
    updatePlaceholder();
}

const String &TextBox::getPlaceholder() const
{
    static const String none;
    return m_placeholder?m_placeholder->getText():none;
}

void TextBox::updatePlaceholder()
{
    if(!m_placeholder)
        return;

    const bool show=!m_placeholder->getText().empty() && !hidden() && getText().empty() && !isFocussed();

    if(m_placeholder->hidden() == !show)
        return;

    m_placeholder->setHidden(!show);
}

void TextBox::updateColors()
{
    ParentClass::updateColors();
    updatePlaceholder();
}

void TextBox::setFont(const gweni::Font &font)
{
    ParentClass::setFont(font);

    if(m_placeholder)
        m_placeholder->setFont(font);
}

void TextBox::setTextPadding(const Padding &padding)
{
    ParentClass::setTextPadding(padding);

    if(m_placeholder)
        m_placeholder->setPadding(padding);
}

void TextBox::setTextAlignment(Alignment alignment)
{
    ParentClass::setTextAlignment(alignment);

    if(m_placeholder)
        m_placeholder->setAlignment(getTextAlignment());
}

void TextBox::setWrap(bool b)
{
    ParentClass::setWrap(b);

    if(m_placeholder)
        m_placeholder->setAlignment(getTextAlignment());
}

// The focus change itself repaints this box (Base::focus's redraw()), which
// shows the caret and selection.
void TextBox::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();
    keepCaretLit();
    startCaretBlink();
}

void TextBox::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();
    stopCaretBlink();
    onLostFocusCaller.call(this);
}

// Only a focused box blinks, so only it has an animation to run: an unfocused
// one costs its canvas's update phase nothing.
void TextBox::startCaretBlink()
{
#if GWENI_ANIMATE
    if(m_caretBlink)
        return;

    m_caretBlink=new CaretBlink();
    gweni::anim::add(this, m_caretBlink);
#endif
}

void TextBox::stopCaretBlink()
{
#if GWENI_ANIMATE
    if(!m_caretBlink)
        return;

    gweni::anim::cancel(this, m_caretBlink);
    m_caretBlink=nullptr;
#endif
}

void TextBox::keepCaretLit()
{
    m_nextCaretBlink=gweni::platform::getTimeInSeconds()+caretSolidTime;

    if(m_caretVisible)
        return;

    m_caretVisible=true;

    if(isFocussed())
        addStateChange(StateChange_Caret);
}

void TextBox::refreshCursorBounds()
{
    keepCaretLit();
    makeCaretVisible();
    updateCursorRects();
}

// TextBox-local, from where m_text is now: the skin adds the box's position.
// A wrapped text's selection spans lines, which the one selection rect cannot
// show, so it is left empty there.
void TextBox::updateCursorRects()
{
    const gweni::Rect pA=getCharacterPosition(m_cursorPos);
    gweni::Rect selection;

    if(hasSelection() && !m_text->wrap())
    {
        const gweni::Rect pB=getCharacterPosition(m_cursorEnd);

        selection.x=std::min(pA.x, pB.x);
        selection.y=pA.y;
        selection.w=std::max(pA.x, pB.x)-selection.x;
        selection.h=pA.h;
    }

    // one unit wide and as high as the line box renderText draws
    const gweni::Rect caret(pA.x, pA.y, 1, pA.h);

    if(caret == m_rectCaretBounds && selection == m_rectSelectionBounds)
        return;

    m_rectCaretBounds=caret;
    m_rectSelectionBounds=selection;

    // unfocused neither is drawn; the focus arriving repaints them
    if(isFocussed())
        addStateChange(StateChange_Background);
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

    const int start=prevPos(getText(), m_cursorPos);
    deleteText(start, m_cursorPos-start);
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

    deleteText(m_cursorPos, nextPos(getText(), m_cursorPos)-m_cursorPos);
    return true;
}

bool TextBox::onKeyLeft(bool down)
{
    if(!down)
        return true;

    m_cursorPos=prevPos(getText(), m_cursorPos);

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
        m_cursorPos=nextPos(getText(), m_cursorPos);

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
    i=snapPos(getText(), i);

    if(m_cursorPos == i)
        return;

    m_cursorPos=i;
    m_cursorLine=0;
    refreshCursorBounds();
}

void TextBox::setCursorend(int i)
{
    i=snapPos(getText(), i);

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
    // Whole codepoints: both ends of the range are snapped back to a boundary.
    const int iEndPos=snapPos(str, iStartPos+iLength);
    iStartPos=snapPos(str, iStartPos);
    iLength=iEndPos-iStartPos;

    if(iLength <= 0)
        return;

    // Taken before setText, whose onTextChanged clamps it to the new length.
    const int cursorPos=m_cursorPos;

    str.erase(iStartPos, iLength);
    setText(str);

    if(cursorPos >= iEndPos)
        setCursorPos(cursorPos-iLength);
    else if(cursorPos > iStartPos)
        setCursorPos(iStartPos);

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

// Reads the measured string, not m_text's bounds: in a layout pass this runs
// before m_text is placed. A string that fits is placed by the text alignment
// (setTextAlignment); a scrolled one is unaligned, so its position is kept.
void TextBox::makeCaretVisible()
{
    const Size text=m_text->refreshSize(false);
    const Alignment textAlignment=getTextAlignment();

    if(text.width < getWidth())
    {
        alignText(textAlignment);
    }
    else
    {
        int iCaretPos=m_text->getCharacterPosition(m_cursorPos).x;
        int iRealCaretPos=iCaretPos+m_text->getX();
        int iSlidingZone=m_text->getFont().size + 1;   // getWidth()*0.1f

        // If the carat is already in a semi-good position, leave it, but
        // clamped below: the box may have grown since the text was scrolled.
        int x=m_text->getX();

        if(iRealCaretPos > getWidth()-iSlidingZone)
            x=getWidth()-iCaretPos-iSlidingZone;

        if(iRealCaretPos < iSlidingZone)
            x=-iCaretPos+iSlidingZone;

        // Don't show too much whitespace to the right
        if(x+text.width < getWidth()-getPadding().right)
            x=-text.width+(getWidth()-getPadding().right);

        // Or the left
        if(x > getPadding().left)
            x=getPadding().left;

        int y=0;

        if(textAlignment & Alignment::Top)
            y=getPadding().top;

        if(textAlignment & Alignment::Bottom)
            y=getHeight()-text.height-getPadding().bottom;

        if(textAlignment & Alignment::CenterV)
            y=(getHeight()-text.height) / 2;

        placeScrolledText(x, y, text);
    }
}

void TextBox::alignText(Alignment textAlignment)
{
    m_text->setPlacement(Placement::Parent);
    m_text->setAlignment(textAlignment);
}

void TextBox::placeScrolledText(int x, int y, const Size &text)
{
    const int width=m_text->wrap()?std::max(0, getWidth()-getPadding().left-getPadding().right):text.width;

    m_text->setAlignment(Alignment::Absolute);
    m_text->setPlacement(Placement::Owner);
    placeChild(m_text, x, y, width, text.height);
}

bool TextBox::sizedByRequest(Dim dim) const
{
    if(!hasRequestedSize(dim))
        return false;

    return dim == Dim::X?!m_sizedByText:m_text->wrap();
}

void TextBox::setSizedByText(bool sizedByText)
{
    if(m_sizedByText == sizedByText)
        return;

    m_sizedByText=sizedByText;
    invalidate();
}

// On an axis sized by its request (sizedByRequest: a size was asked for, as
// the constructor's 200x20 is) Base::calculateSize has made the request the
// preferred size, whatever the flags, and the string adds nothing: it
// scrolls inside the box. Otherwise the box asks for its string.
// A scrolled string is placed by this box (Placement::Owner), which leaves it
// out of sizeOfChildren; it counts as it does aligned, at its preferred size,
// so the box's size does not depend on whether it is scrolled. In X it counts
// beside the children docked Left or Right (a ColorSelector's swatch, a
// NumericUpDown's buttons), which sizeOfChildren takes the wider of with an
// aligned child: the string is aligned in the whole box, so it would run
// under them.
void TextBox::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(sizedByRequest(dim) || m_text->hidden())
        return;

    const Size &text=m_text->getPreferredSize();
    const Margin &margin=m_text->getMargin();

    if(dim == Dim::X)
    {
        int docked=0;

        for(auto &&child : getChildren())
        {
            if(child == m_text || child->hidden() || !child->shouldIncludeInSize())
                continue;

            if(child->getDock() & (DockPosition::Left|DockPosition::Right))
                docked+=child->getPreferredSize().width+child->getMargin().left+child->getMargin().right;
        }

        m_preferredSize.width=std::max(m_preferredSize.width, preferredFromContent(
            text.width+margin.left+margin.right+m_padding.left+m_padding.right+docked, Dim::X));
    }
    else if(m_text->getPlacement() == Placement::Owner)
        m_preferredSize.height=std::max(m_preferredSize.height, preferredFromContent(
            text.height+margin.top+margin.bottom+m_padding.top+m_padding.bottom, Dim::Y));
}

// Where m_text goes (aligned, or scrolled to the caret) is decided before the
// pass places it, from this control's size and the measured string: in X with
// the final width, and again in Y with the final height. Both are placement of
// a child laid out next. The caret and selection rectangles are taken once it
// is placed.
void TextBox::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    PlacementScope placement;
    makeCaretVisible();
}

void TextBox::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::Y)
    {
        PlacementScope placement;
        makeCaretVisible();
    }

    ParentClass::arrange(skin, dim);
}

void TextBox::postLayout(skin::Base *skin)
{
    updateCursorRects();
}

void TextBox::onTextChanged()
{
    m_cursorPos=snapPos(getText(), std::min(m_cursorPos, textLength()));
    m_cursorEnd=snapPos(getText(), std::min(m_cursorEnd, textLength()));

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
}

bool TextBoxMultiline::onKeyReturn(bool down)
{
    if(down)
        insertText("\n");

    return true;
}

void TextBoxMultiline::makeCaretVisible()
{
    const Size text=m_text->refreshSize(false);
    const Alignment textAlignment=getTextAlignment();

    if(text.height < getHeight())
    {
        alignText(textAlignment);
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

        // If the carat is already in a semi-good position, leave it, but
        // clamped below: the box may have grown since the text was scrolled.
        int y=m_text->getY();

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
        if(y+text.height < getHeight()-getPadding().bottom)
            y=-text.height+(getHeight()-getPadding().bottom);

        // Or the top
        if(y > getPadding().top)
            y=getPadding().top;

        int x=0;
        if(textAlignment & Alignment::Left)
            x=getPadding().left;
        if(textAlignment & Alignment::Right)
            x=getWidth()-text.width-getPadding().right;
        if(textAlignment & Alignment::CenterH)
            x=(getWidth()-text.width)*0.5f;

        placeScrolledText(x, y, text);
    }
}

int TextBoxMultiline::getCurrentLine()
{
    return m_text->getLineFromChar(m_cursorPos);
}

// Columns count codepoints, so Up and Down keep the caret over the same
// character count whatever the bytes of the lines are.
int TextBoxMultiline::columnOf(int pos)
{
    const String &str=getText();
    int column=0;

    for(int i=m_text->getStartCharFromLine(m_text->getLineFromChar(pos)); i < pos; i=nextPos(str, i))
        ++column;

    return column;
}

int TextBoxMultiline::columnOnLine(int line, int column)
{
    const String &str=getText();
    const int last=m_text->getLastCaretOnLine(line);
    int pos=m_text->getStartCharFromLine(line);

    for(; column > 0 && pos < last; --column)
        pos=nextPos(str, pos);

    return std::min(pos, last);
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

    m_cursorLine=0;
    m_cursorPos=m_text->getLastCaretOnLine(getCurrentLine());

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
    m_cursorLine=columnOf(m_cursorPos);

    int iLine=m_text->getLineFromChar(m_cursorPos);

    if(iLine == 0)
        return true;

    // The same column on the line above, or that line's last caret position.
    m_cursorPos=columnOnLine(iLine-1, m_cursorLine);

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
    m_cursorLine=columnOf(m_cursorPos);

    int iLine=m_text->getLineFromChar(m_cursorPos);
    int iLastLine=m_text->numLines()-1;
    if(iLine >= iLastLine || iLastLine < 1)
        return true;

    // The same column on the line below, or that line's last caret position.
    m_cursorPos=columnOnLine(iLine+1, m_cursorLine);

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
    updatePlaceholder();

    if(doEvents)
        onTextChanged();
}

void PasswordTextBox::setPasswordChar(const char c)
{
    m_passwordChar=c;
}

}//namespace controls
}//namespace gweni
