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

#include <algorithm>
#include <iterator>

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

void Text::applyTextColor(const gweni::Color &col)
{
    if(m_color == col)
        return;

    m_color=col;
    addStateChange(controls::StateChange_Text);

    for(auto &&line : m_lines)
        line->setTextColor(col);
}

void Text::setSkinTextColor(SkinTextColor which)
{
    m_skinColor=which;
    refreshSkinTextColor();
}

void Text::refreshSkinTextColor()
{
    const skin::Base *skin=getSkin();

    if(!skin)
        return;

    switch(m_skinColor)
    {
    case SkinTextColor::None:           return;
    case SkinTextColor::LabelDefault:   return applyTextColor(skin->Colors.Label.Default);
    case SkinTextColor::LabelBright:    return applyTextColor(skin->Colors.Label.Bright);
    case SkinTextColor::LabelDark:      return applyTextColor(skin->Colors.Label.Dark);
    case SkinTextColor::LabelHighlight: return applyTextColor(skin->Colors.Label.Highlight);
    case SkinTextColor::Placeholder:    return applyTextColor(skin->Colors.PlaceholderText);
    }
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
    dropMeasures();
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

    noteEdit(m_string, str);
    m_string=str;
    // the line height is the font's alone
    m_stringMeasured=false;
    m_longestWordMeasured=false;
    // Its size, and wrapped its lines, come from the string: lay it out again.
    invalidate();
}

// The bytes that differ, found without measuring or allocating: the string is
// compared from both ends. Before the first wrap, or when something else
// already wants the whole string wrapped again, there is nothing to record.
void Text::noteEdit(const gweni::String &oldString, const gweni::String &newString)
{
    if(m_wrapStale)
        return;

    const size_t oldLength=oldString.size();
    const size_t newLength=newString.size();
    const size_t common=std::min(oldLength, newLength);

    const size_t prefix=static_cast<size_t>(std::mismatch(oldString.begin(), oldString.begin()+common,
        newString.begin()).first-oldString.begin());
    size_t suffix=0;

    while(suffix < common-prefix && oldString[oldLength-1-suffix] == newString[newLength-1-suffix])
        ++suffix;

    // oldString[start, oldEnd) became newString[start, newEnd)
    const size_t start=prefix;
    const size_t oldEnd=oldLength-suffix;
    const size_t newEnd=newLength-suffix;

    if(!m_editPending)
    {
        m_editPending=true;
        m_editStart=start;
        m_editOldEnd=oldEnd;
        m_editNewEnd=newEnd;
        return;
    }

    // An edit of a string that was itself an edit of the wrapped one: the
    // union of the two, in the wrapped string's bytes and the new string's.
    // The bytes of oldString from m_editNewEnd on are the wrapped string's
    // from m_editOldEnd on, and those from oldEnd on are newString's from
    // newEnd on, so the end of the union in oldString maps to both.
    const size_t end=std::max(m_editNewEnd, oldEnd);

    m_editOldEnd=m_editOldEnd+(end-m_editNewEnd);
    m_editNewEnd=newEnd+(end-oldEnd);
    m_editStart=std::min(m_editStart, start);
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
        return gweni::Rect(0, 0, 0, lineHeight());

    String sub=str.substr(0, value);
    gweni::Point p=measureText(getFont(), sub);
    return Rect(p.x, 0, 0, p.y);
}

gweni::Rect Text::getLineBox(int i)
{
    const bool wrapped=ensureWrapped() && i >= 0 && i < static_cast<int>(m_wrapped.size());
    const gweni::String &str=wrapped?m_wrapped[i].text:m_string;
    // wrapped, the rows of the line children; unwrapped, inside the padding
    const int x=wrapped?0:getPadding().left;
    const int y=wrapped?i*lineHeight():getPadding().top;

    // unwrapped, the whole string, which is kept
    gweni::Point p=wrapped?measureText(getFont(), str):measuredString();
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
    // characterPositionIn's x of every caret, from one layout of the string
    std::vector<int> xs;

    measureCarets(getFont(), str, xs);

    // Only codepoint boundaries: the caret never goes inside a UTF-8 sequence.
    // Each caret is at y 0, as characterPositionIn puts it.
    size_t k=0;

    for(size_t i=0; ; i=utf8Next(str, i), ++k)
    {
        const int iDist=std::abs(xs[k]-p.x) + std::abs(p.y);  // this isn't proper

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

    // Every child is a line, and one that did not change keeps its preferred
    // size and the row height it was given: with the rows as high as they
    // were made, only the changed lines are measured and given it.
    if(walkChangedChildren() && m_children.size() == m_lines.size()
        && (dim == Dim::X || rowHeight == m_linesRowHeight))
    {
        countLayoutChildrenScanned(m_changedChildren);
        for(size_t i=0; i < m_changedChildren.size(); ++i)
        {
            Text *line=static_cast<Text *>(m_changedChildren[i]);

            if(!line)
                continue;

            line->doCalculateSize(skin, dim);

            if(dim == Dim::Y)
                line->m_preferredSize.height=rowHeight;
        }
    }
    else
    {
        countLayoutChildrenScanned(m_lines.size());
        for(auto &&line : m_lines)
        {
            line->doCalculateSize(skin, dim);

            if(dim == Dim::Y)
                line->m_preferredSize.height=rowHeight;
        }
    }

    if(dim == Dim::Y)
        m_linesRowHeight=rowHeight;

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
    renewMeasureKey();

    if(!m_lineHeightMeasured)
    {
        m_lineHeight=measureLineHeight(getFont());
        m_lineHeightMeasured=true;
    }

    return m_lineHeight;
}

gweni::Point Text::measuredString()
{
    renewMeasureKey();

    if(!m_stringMeasured)
    {
        m_stringSize=measureText(getFont(), m_string);
        m_stringMeasured=true;
    }

    return m_stringSize;
}

bool Text::measureKeyCurrent()
{
    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;

    return m_font == m_measureFont && *m_font == m_measureFontValue && renderer == m_measureRenderer
        && renderer && renderer->textLayoutSerial() == m_measureSerial;
}

void Text::renewMeasureKey()
{
    if(measureKeyCurrent())
        return;

    dropMeasures();

    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;

    m_measureFont=m_font;
    m_measureFontValue=*m_font;
    m_measureRenderer=renderer;
    m_measureSerial=renderer?renderer->textLayoutSerial():0;
}

void Text::dropMeasures()
{
    m_stringMeasured=false;
    m_lineHeightMeasured=false;
    m_longestWordMeasured=false;
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
    // changes the width only, so the parent's layout does not move. A string
    // is measured alone: its height is that line box already. Both are kept
    // (measuredString(), lineHeight()), so the passes that ask again, in each
    // axis and from each ScrollControl above, measure nothing.
    gweni::Point p=(length() > 0)?measuredString():gweni::Point(1, lineHeight());

    p.x+=getPadding().left+getPadding().right;
    p.y+=getPadding().top+getPadding().bottom;

    if(p.x == getWidth() && p.y == getHeight())
        return Size(p.x, p.y);

    // The size of the string, computed: whoever placed this Text sizes it
    // from its preferred size anyway (policyResize).
    if(update)
        setArrangedBounds(getX(), getY(), p.x, p.y);

    return Size(p.x, p.y);
}

int Text::longestWordWidth()
{
    if(!m_font)
        return 0;

    // asked on every X pass of a wrapped Text whose parent has no width request
    renewMeasureKey();

    if(m_longestWordMeasured)
        return m_longestWord;

    // Unbroken (no width): each word with the space after it, or a '\n'.
    std::vector<gweni::String> words;
    text::splitWords(getText(), 0, nullptr, words);

    int width=0;

    for(auto &&word : words)
    {
        size_t end=word.size();

        while(end > 0 && (word[end-1] == ' ' || word[end-1] == '\n'))
            --end;

        if(end == 0)
            continue;

        width=std::max(width, measureText(getFont(), word.substr(0, end)).x);
    }

    m_longestWord=width;
    m_longestWordMeasured=true;
    return width;
}

void Text::wrapLines(const gweni::String &s, int w, std::vector<WrappedLine> &lines)
{
    text::wrapLines(s, w, [this](const gweni::String &str) { return measureText(getFont(), str).x; }, lines);
}

bool Text::wrapCurrent(int w) const
{
    return !m_wrapStale && !m_editPending && w == m_wrapWidth && m_font == m_wrapFont
        && *m_font == m_wrapFontValue;
}

void Text::wrapAt(int w)
{
    if(wrapCurrent(w))
        return;

    if(!m_wrapStale && w == m_wrapWidth && m_font == m_wrapFont && *m_font == m_wrapFontValue)
    {
        // only the string changed: its edited paragraphs
        wrapEdit(w);
    }
    else
    {
        m_wrapped.clear();
        wrapLines(m_string, w, m_wrapped);
        // a new wrap, which the line children do not show yet: all of them
        m_linesStale=true;
        m_linesPartial=false;
    }
    ++m_wrapCount;

    m_wrapStale=false;
    m_editPending=false;
    m_wrapWidth=w;
    m_wrapFont=m_font;
    m_wrapFontValue=*m_font;
}

// A line ends at a '\n' when it holds one more character than it shows.
static bool endsParagraph(int length, const gweni::String &text)
{
    return length > static_cast<int>(text.length());
}

// A paragraph's lines depend on nothing outside it: the lines restart at each
// '\n', and splitWords never carries a word across one. So the edit is
// widened to whole paragraphs, from the start of the one holding its first
// byte to the first '\n' at or after its end (the end of the string if there
// is none), and that part of the string is wrapped again alone, by the same
// code with the same measures as a wrap of the whole string, which therefore
// gives the same lines. Its lines replace those of the old paragraphs; the
// lines before and after them hold the same characters as before, and a line
// records only how many it holds, so they are kept as they are. Finding the
// paragraph walks the lines' lengths and measures nothing. One paragraph with
// no '\n' in it is the whole string, and is wrapped again whole.
void Text::wrapEdit(int w)
{
    const size_t start=m_editStart;
    const size_t oldEditEnd=m_editOldEnd;
    const size_t newEditEnd=m_editNewEnd;
    const size_t count=m_wrapped.size();

    // the paragraph holding byte `start` of the wrapped string: its first
    // line and where it starts (the line holding a '\n' is the one it ends,
    // and the end of the string is on the last line)
    size_t first=0;
    size_t paragraphStart=0;
    size_t offset=0;

    for(size_t i=0; i < count; ++i)
    {
        const WrappedLine &line=m_wrapped[i];

        if(offset+static_cast<size_t>(line.length) > start)
            break;

        offset+=static_cast<size_t>(line.length);

        if(endsParagraph(line.length, line.text) && i+1 < count)
        {
            first=i+1;
            paragraphStart=offset;
        }
    }

    // to the first '\n' at or after the edit, found in the new string: the
    // bytes after the edit are the wrapped string's after its edit
    const size_t newLength=m_string.size();
    const size_t oldLength=newLength-newEditEnd+oldEditEnd;
    const size_t newline=m_string.find('\n', newEditEnd);
    const bool toEnd=(newline == gweni::String::npos);
    const size_t newEnd=toEnd?newLength:newline+1;
    const size_t oldEnd=toEnd?oldLength:oldEditEnd+(newline-newEditEnd)+1;

    // the old paragraphs' lines: from `first` until they reach oldEnd
    size_t last=first;

    for(size_t held=paragraphStart; last < count && (held < oldEnd || toEnd); ++last)
        held+=static_cast<size_t>(m_wrapped[last].length);

    std::vector<WrappedLine> lines;

    wrapLines(m_string.substr(paragraphStart, newEnd-paragraphStart), w, lines);
    // wrapped alone, a part ending in a '\n' has an empty line after it,
    // which is the next paragraph's first, kept
    if(!toEnd)
        lines.pop_back();

    const size_t oldCount=last-first;
    const size_t newCount=lines.size();

    if(oldCount == newCount)
    {
        std::move(lines.begin(), lines.end(), m_wrapped.begin()+first);
    }
    else
    {
        m_wrapped.erase(m_wrapped.begin()+first, m_wrapped.begin()+last);
        m_wrapped.insert(m_wrapped.begin()+first, std::make_move_iterator(lines.begin()),
            std::make_move_iterator(lines.end()));
    }

    // The children showed the lines before this edit: only these differ
    // now. Already behind by an earlier edit, they differ by the union of the
    // two, in their lines and in the new ones, mapped as in noteEdit. Behind
    // a whole wrap, they are synced whole anyway.
    if(!m_linesStale)
    {
        m_linesStale=true;
        m_linesPartial=true;
        m_syncFirst=first;
        m_syncOldEnd=last;
        m_syncNewEnd=first+newCount;
    }
    else if(m_linesPartial)
    {
        const size_t end=std::max(m_syncNewEnd, last);

        m_syncOldEnd=m_syncOldEnd+(end-m_syncNewEnd);
        m_syncNewEnd=first+newCount+(end-last);
        m_syncFirst=std::min(m_syncFirst, first);
    }
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
// helper wrapped the edit before this layout), and the line children are made
// to show it (syncLines).
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

    syncLines(lineHeight);

    m_linesStale=false;
    m_linesPartial=false;
    m_wrappedWidth=w;

    // The parent's width and the height of the lines, the same as the
    // preferred size calculateSize gets from this function.
    setArrangedBounds(getX(), getY(), w, height);

    return Size(w, height);
}

// After edits alone, the children of the lines that did not change keep
// their strings and rows and are not touched; the edited lines' children are
// reused in order, the surplus deleted or the shortfall made there, so a line
// below an edit keeps its own child, and is moved only if the number of lines
// above it changed. Otherwise (a first wrap, a new width or font) the lines
// that are there are given the new strings and rows by index, which changes
// nothing (no new text slots) for a line that shows the same text on the same
// row; only the lines past them are added or deleted.
void Text::syncLines(int lineHeight)
{
    const size_t count=m_wrapped.size();

    if(m_linesPartial && m_syncOldEnd <= m_lines.size()
        && m_lines.size()-(m_syncOldEnd-m_syncFirst)+(m_syncNewEnd-m_syncFirst) == count)
    {
        const size_t first=m_syncFirst;
        const size_t oldCount=m_syncOldEnd-first;
        const size_t newCount=m_syncNewEnd-first;

        if(oldCount > newCount)
        {
            for(size_t i=first+newCount; i < first+oldCount; ++i)
                // ~Base takes it out of the children and gives its ids back.
                delete m_lines[i];

            m_lines.erase(m_lines.begin()+first+newCount, m_lines.begin()+first+oldCount);
        }
        else
        {
            for(size_t i=first+oldCount; i < first+newCount; ++i)
                insertLine(i);
        }

        // a new child is synced whatever it shows
        for(size_t i=first; i < first+newCount; ++i)
            syncLine(i, lineHeight, i >= first+oldCount);

        // the rows below, when there are more or fewer lines above them
        if(oldCount != newCount)
        {
            for(size_t i=first+newCount; i < count; ++i)
                placeChildPos(m_lines[i], 0, static_cast<int>(i)*lineHeight);
        }
        return;
    }

    removeLines(count);

    while(m_lines.size() < count)
        insertLine(m_lines.size());

    for(size_t i=0; i < count; ++i)
        syncLine(i, lineHeight, true);
}

void Text::syncLine(size_t i, int lineHeight, bool always)
{
    Text *t=m_lines[i];
    const int y=static_cast<int>(i)*lineHeight;

    // After edits alone, a child showing this string on this row was given
    // them by an earlier sync, in this font, so its size is the same too.
    if(!always && t->getText() == m_wrapped[i].text && t->getY() == y)
        return;

    t->setText(m_wrapped[i].text);

    Size size=t->refreshSize(false);
    size.height=lineHeight;
    t->setPreferredSize(size);
    placeChild(t, 0, y, size.width, size.height);
}

Text *Text::insertLine(size_t index)
{
    // This control places its lines (Placement::Owner).
    Text *t=newChild<Text>();
    t->setPlacement(Placement::Owner);
    t->setFont(getFont());
    t->setTextColor(TextColor());
    t->setTextColorOverride(m_colorOverride);

    // among the children in the lines' order, so they are numbered top to
    // bottom however they were made: made last, it is after the last already
    if(index < m_lines.size())
        t->bringNextToControl(m_lines[index], false);

    m_lines.insert(m_lines.begin()+static_cast<std::ptrdiff_t>(index), t);
    return t;
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
