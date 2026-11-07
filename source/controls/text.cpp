/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>
#include <gweni/utility.h>
#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(Text)
{
    m_font=&getSkin()->getDefaultFont();
    m_colorOverride=Color(255, 255, 255, 0);
    m_color=getSkin()->Colors.Label.Default;
    setMouseInputEnabled(false);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    setWrap(false);

//    m_textId=getTextId(this);
}

Text::~Text()
{
//    releaseTextId(m_textId);
    // NOTE: This font doesn't need to be released
    // Because it's a pointer to another font somewhere.
}

// A Text is the size of its string: calculateSize measures it, and the parent
// gives it that size when it places it (policyResize), so nothing here resizes
// it after the parent has. Wrapped lines are built in arrange().
void Text::layout(skin::Base * /*skin*/)
{}

const gweni::Font &Text::getFont() const
{
    return *m_font;
}

void Text::setFont(const gweni::Font &font)
{
    if(m_font == &font)
        return;

    addStateChange(controls::StateChange_Text);

    m_font=&font;
    m_wrapStale=true;
    // Change the font of multilines too!
    for(auto &&line : m_lines)
    {
        line->setFont(*m_font);
    }

    invalidate();
}

void Text::setText(const String &str)
{
    if(m_string == str)
        return;

    addStateChange(controls::StateChange_Text);

    m_string=str;
    m_wrapStale=true;
    // Its size, and wrapped its lines, come from the string: lay it out again.
    invalidate();
}

//void Text::render(skin::Base *skin)
//{
////    if(m_wrap)
////        return;
////
////    if(length() == 0)
////        return;
//    if(getStateChange() == StateChange_Nothing)
//        return;
//    
//    if(getStateChange() & StateChange_Created)
//    {
//
//    }
//
//    if(m_colorOverride.a == 0)
//        skin->getRenderer()->setDrawColor(m_color);
//    else
//        skin->getRenderer()->setDrawColor(m_colorOverride);
//
//    if(getStateChange() & controls::StateChange_Visibility)
//    {
//        if(hidden())
//            skin->getRenderer()->hideText(m_textId);
//        else
//            skin->getRenderer()->showText(m_textId);
//    }
//
//    skin->getRenderer()->renderText(m_textId, getFont(),
//        gweni::Point(getPadding().left, getPadding().top), getZIndex(),
//        m_string);
//}

// Wrapped, the rect on the row of the line holding the character, measured
// in the line's text as the line child would measure it, so it needs the wrap
// only, not the children (out of date between an edit and layout).
gweni::Rect Text::getCharacterPosition(unsigned int value)
{
    if(ensureWrapped())
    {
        const int index=static_cast<int>(value);
        const int lineIndex=getLineFromChar(index);
        const WrappedLine &line=m_wrapped[lineIndex];
        // On the '\n' that ends a line, the caret is at that line's end.
        const int pos=gweni::clamp(index-getStartCharFromLine(lineIndex), 0,
            static_cast<int>(line.text.length()));

        gweni::Rect rect=characterPositionIn(line.text, static_cast<unsigned int>(pos));
        rect.y+=lineIndex*lineHeight();
        return rect;
    }

    // Unwrapped, the string starts inside the padding (textOrigin()).
    gweni::Rect rect=characterPositionIn(m_string, value);

    rect.x+=getPadding().left;
    rect.y+=getPadding().top;
    return rect;
}

gweni::Rect Text::characterPositionIn(const gweni::String &str, unsigned int value)
{
    if(str.empty() || value == 0)
    {
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), " ");
        return gweni::Rect(0, 0, 0, p.y);
    }

    String sub=str.substr(0, value);
    gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), sub);
    return Rect(p.x, 0, 0, p.y);
}

gweni::Rect Text::getLineBox(int i)
{
    const bool wrapped=ensureWrapped() && i >= 0 && i < static_cast<int>(m_wrapped.size());
    const gweni::String &str=wrapped?m_wrapped[i].text:m_string;
    // wrapped, the rows of the line children; unwrapped, inside the padding
    const int x=wrapped?0:getPadding().left;
    const int y=wrapped?i*lineHeight():getPadding().top;

    gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), str);
    return gweni::Rect(x, y, clamp(p.x, 1, p.x), clamp(p.y, 1, p.y));
}

int Text::getClosestCharacter(gweni::Point p)
{
    if(ensureWrapped())
    {
        // The lines are stacked top to bottom: the last one starting at or
        // above p, or the first one when p is above them all.
        const int rowHeight=lineHeight();
        size_t lineIndex=0;

        for(size_t i=1; i < m_wrapped.size(); ++i)
        {
            if(p.y < static_cast<int>(i)*rowHeight)
                break;
            lineIndex=i;
        }

        const int iLine=static_cast<int>(lineIndex);
        const int start=getStartCharFromLine(iLine);
        const int iLinePos=closestCharacterIn(m_wrapped[lineIndex].text,
            gweni::Point(p.x, p.y-iLine*rowHeight));

        return gweni::clamp(start+iLinePos, start, getLastCaretOnLine(iLine));
    }

    // Unwrapped, the string starts inside the padding (textOrigin()).
    return closestCharacterIn(m_string,
        gweni::Point(p.x-getPadding().left, p.y-getPadding().top));
}

int Text::closestCharacterIn(const gweni::String &str, gweni::Point p)
{
    int iDistance=4096;
    size_t value=0;

    // Only codepoint boundaries: the caret never goes inside a UTF-8 sequence.
    for(size_t i=0; ; i=utf8Next(str, i))
    {
        gweni::Rect cp=characterPositionIn(str, static_cast<unsigned int>(i));
        const int iDist=std::abs(cp.x-p.x) + std::abs(cp.y-p.y);  // this isn't proper

        if(iDist <= iDistance)
        {
            iDistance=iDist;
            value=i;
        }

        if(i >= str.length())
            break;
    }

    return static_cast<int>(value);
}

// Unwrapped, the measured string. Wrapped, the text asks for the width the
// app asked its parent to be (less the parent's padding), or with no such
// request for its longest word; never for the parent's current width, which
// would hand a width an earlier pass gave back to the parent. Only the app's
// calls are requests, so neither depends on an earlier pass. It is then laid
// out at whatever width the parent got (policyResize), and its height is that
// of the lines at that width, final by the Y pass.
void Text::calculateSize(skin::Base *skin, Dim dim)
{
    // Every row is a line of the font high, an empty one included, whose own
    // measure would be the font's size.
    const int rowHeight=(dim == Dim::Y && !m_lines.empty())?lineHeight():0;

    for(auto &&line : m_lines)
    {
        line->calculateSize(skin, dim);

        if(dim == Dim::Y)
            line->m_preferredSize.height=rowHeight;
    }

    if(m_wrap && dim == Dim::X)
    {
        const Base *parent=getParent();

        if(parent->hasRequestedSize(Dim::X))
            m_preferredSize.width=std::max(0, parent->getRequestedBounds().w
                -parent->getPadding().left-parent->getPadding().right);
        else
            m_preferredSize.width=longestWordWidth()+getPadding().left+getPadding().right;
        return;
    }

    Size size=refreshSize(false);

    if(dim==Dim::X)
        m_preferredSize.width=size.width;
    else
        m_preferredSize.height=size.height;
}

// Wrapped text breaks its lines at its parent's inner width, which is final
// once the parent was arranged in X, so the lines are brought up to date at
// the start of this control's X arrange when the string, the font or that
// width changed. Lines added there are children this pass lays out next, X
// and Y, so building them is placement. Unwrapped, it has no lines.
void Text::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
    {
        if(m_wrap)
        {
            if(!linesCurrent(wrapWidth()))
            {
                PlacementScope placement;

                RefreshSizeWrap(true);
            }
        }
        else if(!m_lines.empty())
        {
            PlacementScope placement;

            removeLines(0);
            m_wrappedWidth=-1;
        }
    }

    Base::arrange(skin, dim);
}

// Whatever was asked, a Text placed by its parent takes its preferred size,
// the measured string or the height of the wrapped lines; wrapped, it is as
// wide as the parent's inner width, which its lines break at.
void Text::policyResize(const Size &preferred, Dim dim)
{
    const Rect &bounds=getBounds();

    if(dim == Dim::X)
        setArrangedBounds(bounds.x, bounds.y, m_wrap?std::max(0, wrapWidth()):preferred.width, bounds.h);
    else
        setArrangedBounds(bounds.x, bounds.y, bounds.w, preferred.height);
}

int Text::lineHeight()
{
    return getSkin()->getRenderer()->measureText(getFont(), " ").y;
}

int Text::wrapWidth()
{
    return getParent()->getWidth()-getParent()->getPadding().left-getParent()->getPadding().right;
}

Size Text::refreshSize(bool update)
{
    if(m_wrap)
        return RefreshSizeWrap(update);

    if(!m_font)
    {
        GWENI_ASSERT_MSG(false, "Text::refreshSize() - No Font!");
        return Size(0, 0);
    }

    // An empty string is one caret wide and one line box high, as high as
    // any typed string: the first character typed or the last one deleted
    // changes the width only, so the parent's layout does not move.
    gweni::Point p(1, lineHeight());

    if(length() > 0)
        p=getSkin()->getRenderer()->measureText(getFont(), m_string);

    p.x+=getPadding().left+getPadding().right;
    p.y+=getPadding().top+getPadding().bottom;

    if(p.x == getWidth() && p.y == getHeight())
        return Size(p.x, p.y);

    if(update)
        setSize(p.x, p.y);

    return Size(p.x, p.y);
}

int Text::longestWordWidth()
{
    if(!m_font)
        return 0;

    // Unbroken (no width): each word with the space after it, or a '\n'.
    std::vector<gweni::String> words;
    splitWords(getText(), 0, words);

    int width=0;

    for(auto &&word : words)
    {
        size_t end=word.size();

        while(end > 0 && (word[end-1] == ' ' || word[end-1] == '\n'))
            --end;

        if(end == 0)
            continue;

        width=std::max(width, getSkin()->getRenderer()->measureText(getFont(), word.substr(0, end)).x);
    }

    return width;
}

void Text::splitWords(const gweni::String &s, int w, std::vector<gweni::String> &elems)
{
    gweni::String str;

    // A step is one codepoint, so a word broken up for width is never broken
    // inside a UTF-8 sequence.
    for(size_t i=0, next=0; i < s.length(); i=next)
    {
        next=utf8Next(s, i);

        if(s[i] == '\n')
        {
            if(!str.empty())
                elems.push_back(str);

            elems.push_back("\n");
            str.clear();
            continue;
        }

        if(s[i] == ' ')
        {
            str+=s[i];
            elems.push_back(str);
            str.clear();
            continue;
        }

        const size_t charLength=next-i;
        str.append(s, i, charLength);

        // With no width to place it in, a word is not broken up.
        if(w <= 0)
            continue;

        // if adding character makes the word bigger than the textbox size
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), str);
        if(p.x > w)
        {
            if(str.size() == charLength)
            {
                // A single character already overflows the available width;
                // retrying it would never make progress.
                elems.push_back(str);
                str.clear();
                continue;
            }

            //split words
            str.resize(str.size()-charLength);
            elems.push_back(str);
            str.clear();
            next=i;
            continue;
        }
    }

    if(!str.empty())
        elems.push_back(str);
}

// A line ends at each '\n', which it holds but does not show (freetype-gl
// would break the run there, below the line's box), and before a word that
// would take it past w. A blank line is an empty line, and so is the one after
// a final '\n'. With w <= 0 the lines only break at '\n'.
void Text::wrapLines(int w, std::vector<WrappedLine> &lines)
{
    std::vector<gweni::String> words;
    splitWords(getText(), w, words);

    gweni::String strLine;
    int lineLength=0;

    for(auto &&word : words)
    {
        if(word == "\n")
        {
            lines.push_back({strLine, lineLength+1});
            strLine.clear();
            lineLength=0;
            continue;
        }

        const int wordLength=static_cast<int>(word.length());

        // Only the parent's width counts: this control's own is what the
        // last wrap made it, and comparing with it kept lines that no
        // longer fit once the parent got narrower.
        if(w > 0 && !strLine.empty()
            && getSkin()->getRenderer()->measureText(getFont(), strLine+word).x > w)
        {
            lines.push_back({strLine, lineLength});
            // the new line starts with the word that did not fit
            strLine=word;
            lineLength=wordLength;
            continue;
        }

        strLine+=word;
        lineLength+=wordLength;
    }

    lines.push_back({strLine, lineLength});
}

bool Text::wrapCurrent(int w) const
{
    return !m_wrapStale && w == m_wrapWidth && m_font == m_wrapFont && *m_font == m_wrapFontValue;
}

void Text::wrapAt(int w)
{
    if(wrapCurrent(w))
        return;

    m_wrapped.clear();
    wrapLines(w, m_wrapped);
    ++m_wrapCount;

    m_wrapStale=false;
    m_wrapWidth=w;
    m_wrapFont=m_font;
    m_wrapFontValue=*m_font;
    // a new wrap, which the line children do not show yet
    m_linesStale=true;
}

// Between an edit and the next layout the string is wrapped again at the
// width of the last wrap (the last layout's, or a newer parent width a
// measure has wrapped at since), the width that layout will wrap at unless
// the parent changes size, so the helpers see the edited string. Only the
// data: a child is made, moved or deleted by layout alone.
bool Text::ensureWrapped()
{
    if(!m_wrap || m_wrappedWidth < 0 || !m_font)
        return false;

    if(!wrapCurrent(m_wrapWidth))
        wrapAt(m_wrapWidth);

    return true;
}

bool Text::linesCurrent(int width) const
{
    return !m_linesStale && !m_lines.empty() && wrapCurrent(width);
}

void Text::removeLines(size_t count)
{
    while(m_lines.size() > count)
    {
        Text *line=m_lines.back();

        m_lines.pop_back();
        // ~Base takes it out of the children and gives its ids back.
        delete line;
    }
}

// The string is wrapped into m_wrapped unless it already holds this wrap (a
// helper wrapped the edit before this layout). The lines that are there are
// given the new strings and rows, which changes nothing (no new text slots)
// for a line that shows the same text on the same row; only the lines past
// them are added or deleted.
Size Text::RefreshSizeWrap(bool update)
{
    if(!m_font)
    {
        GWENI_ASSERT_MSG(false, "Text::refreshSize() - No Font!");
        return Size(0, 0);
    }

    const int w=wrapWidth();
    const int lineHeight=this->lineHeight();

    if(linesCurrent(w))
        return Size(w, static_cast<int>(m_lines.size())*lineHeight);

    wrapAt(w);

    const int height=static_cast<int>(m_wrapped.size())*lineHeight;

    if(!update)
        return Size(w, height);

    removeLines(m_wrapped.size());

    int y=0;

    for(size_t i=0; i < m_wrapped.size(); ++i)
    {
        if(i == m_lines.size())
        {
            // This control places its lines (Placement::Owner).
            Text *t=newChild<Text>();
            t->setPlacement(Placement::Owner);
            t->setFont(getFont());
            t->setTextColor(TextColor());
            t->setTextColorOverride(m_colorOverride);

            m_lines.push_back(t);
        }

        Text *t=m_lines[i];

        t->setText(m_wrapped[i].text);

        Size size=t->refreshSize(false);
        size.height=lineHeight;
        t->setPreferredSize(size);
        placeChild(t, 0, y, size.width, size.height);

        y+=lineHeight;
    }

    m_linesStale=false;
    m_wrappedWidth=w;

    // The parent's width and the height of the lines, the same as the
    // preferred size calculateSize gets from this function.
    setArrangedBounds(getX(), getY(), w, height);

    return Size(w, height);
}

unsigned int Text::numLines()
{
    if(!ensureWrapped())
        return 0;

    return static_cast<unsigned int>(m_wrapped.size());
}

Text *Text::getLine(int i)
{
    if(i < 0 || i >= static_cast<int>(m_lines.size()))
        return nullptr;

    return m_lines[i];
}

int Text::getLineLength(int i)
{
    if(!ensureWrapped() || i < 0 || i >= static_cast<int>(m_wrapped.size()))
        return 0;

    return m_wrapped[i].length;
}

// The line holding character i: the first one whose source range runs past
// it, so a '\n' belongs to the line it ends; past the end, the last line.
int Text::getLineFromChar(int i)
{
    if(!ensureWrapped())
        return 0;

    int iChars=0;
    const int count=static_cast<int>(m_wrapped.size());

    for(int iLine=0; iLine < count; ++iLine)
    {
        iChars+=m_wrapped[iLine].length;

        if(iChars > i)
            return iLine;
    }

    return std::max(0, count-1);
}

int Text::getStartCharFromLine(int i)
{
    if(!ensureWrapped())
        return 0;

    int iChars=0;
    const int count=std::min(i, static_cast<int>(m_wrapped.size()));

    for(int iLine=0; iLine < count; ++iLine)
        iChars+=m_wrapped[iLine].length;

    return gweni::clamp(iChars, 0, length());
}

int Text::getEndCharFromLine(int i)
{
    return gweni::clamp(getStartCharFromLine(i)+getLineLength(i), 0, length());
}

int Text::getLastCaretOnLine(int i)
{
    const int start=getStartCharFromLine(i);
    const int end=getEndCharFromLine(i);

    // The last line ends the text; any other ends with its '\n' or with the
    // character the next line wraps after.
    if(i >= static_cast<int>(numLines())-1 || end <= start)
        return end;

    return static_cast<int>(utf8Prev(m_string, static_cast<size_t>(end)));
}

int Text::getCharPosOnLine(int i)
{
    if(!ensureWrapped())
        return 0;

    int iLine=getLineFromChar(i);
    int iStart=getStartCharFromLine(iLine);
    return i-iStart;
}

}//namespace internal
}//namespace controls
}//namespace gweni
