/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/skin.h>
#include <gweni/platforms/commonPlatform.h>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(Text)
{
//    m_font=&getSkin()->getDefaultFont();
    m_font=&getDefaultFont();
    m_colorOverride=Color(255, 255, 255, 0);
//    m_color=getSkin()->Colors.Label.Default;
    // Tokyo Night foreground: default text is light so it reads on the dark theme
    // even for controls that never explicitly set a text color.
    m_color=Color(0xc0, 0xca, 0xf5, 255);
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
    m_textChanged=true;
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
    m_textChanged=true;
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

gweni::Rect Text::getCharacterPosition(unsigned int value)
{
    if(!m_lines.empty())
    {
        const int index=static_cast<int>(value);
        const int lineIndex=getLineFromChar(index);
        Text *line=m_lines[lineIndex];
        // On the '\n' that ends a line, the caret is at that line's end.
        const int pos=gweni::clamp(index-getStartCharFromLine(lineIndex), 0, line->length());

        gweni::Rect rect=line->getCharacterPosition(pos);
        rect.x+=line->getX();
        rect.y+=line->getY();
        return rect;
    }

    if(length() == 0 || value == 0)
    {
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), " ");
        return gweni::Rect(0, 0, 0, p.y);
    }

    String sub=m_string.substr(0, value);
    gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), sub);
    return Rect(p.x, 0, 0, p.y);
}

gweni::Rect Text::getLineBox(int i)
{
    Text *line=getLine(i);
    if(line != nullptr)
    {
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), line->m_string);
        return gweni::Rect(line->getX(), line->getY(), clamp(p.x, 1, p.x), clamp(p.y, 1, p.y));
    }
    else
    {
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), m_string);
        return gweni::Rect(0, 0, clamp(p.x, 1, p.x), clamp(p.y, 1, p.y));
    }
}

int Text::getClosestCharacter(gweni::Point p)
{
    if(!m_lines.empty())
    {
        // The lines are stacked top to bottom: the last one starting at or
        // above p, or the first one when p is above them all.
        size_t lineIndex=0;

        for(size_t i=1; i < m_lines.size(); ++i)
        {
            if(p.y < m_lines[i]->getY())
                break;
            lineIndex=i;
        }

        const int iLine=static_cast<int>(lineIndex);
        Text *line=m_lines[lineIndex];
        const int start=getStartCharFromLine(iLine);
        const int iLinePos=line->getClosestCharacter(gweni::Point(p.x-line->getX(), p.y-line->getY()));

        return gweni::clamp(start+iLinePos, start, getLastCaretOnLine(iLine));
    }

    int iDistance=4096;
    int value=0;

    for(unsigned i=0; i < m_string.length()+1; i++)
    {
        gweni::Rect cp=getCharacterPosition(i);
        const int iDist=std::abs(cp.x-p.x) + std::abs(cp.y-p.y);  // this isn't proper

        if(iDist > iDistance)
            continue;

        iDistance=iDist;
        value=i;
    }

    return value;
}

void Text::onScaleChanged()
{
    invalidate();
}

// Unwrapped, the measured string. Wrapped, the text asks for the width its
// parent asked for (less the parent's padding), not for the parent's current
// width, which would hand a width an earlier pass gave back to the parent; it
// is then laid out at whatever width the parent got (policyResize), and its
// height is that of the lines at that width, final by the Y pass.
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

        m_preferredSize.width=std::max(0, parent->getRequestedBounds().w
            -parent->getPadding().left-parent->getPadding().right);
        return;
    }

    Size size=refreshSize(false);

    if(dim==Dim::X)
        m_preferredSize.width=size.width;
    else
        m_preferredSize.height=size.height;
}

// Wrapped text breaks its lines at its parent's inner width, which is final
// once the parent was arranged in X, so the lines are rebuilt at the start of
// this control's X arrange when the string, the font or that width changed.
// They are new children this pass lays out next, X and Y, so building them is
// placement.
void Text::arrange(skin::Base *skin, Dim dim)
{
    if(dim == Dim::X)
    {
        if(m_wrap)
        {
            const int width=wrapWidth();

            if(m_textChanged || width != m_wrappedWidth)
            {
                PlacementScope placement;

                RefreshSizeWrap(true);
                m_wrappedWidth=width;
            }
        }
        m_textChanged=false;
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

    gweni::Point p(1, getFont().size);

    if(length() > 0)
        p=getSkin()->getRenderer()->measureText(getFont(), m_string);

    p.x+=getPadding().left+getPadding().right;
    p.y+=getPadding().top+getPadding().bottom;

    if(p.x == getWidth() && p.y == getHeight())
        return Size(p.x, p.y);

    if(p.y < getFont().size)
        p.y=getFont().size;

    if(update)
        setSize(p.x, p.y);

    return Size(p.x, p.y);
}

void Text::splitWords(const gweni::String &s, int w, std::vector<gweni::String> &elems)
{
    gweni::String str;

    for(size_t i=0; i < s.length(); i++)
    {
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

        str+=s[i];

        // With no width to place it in, a word is not broken up.
        if(w <= 0)
            continue;

        // if adding character makes the word bigger than the textbox size
        gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), str);
        if(p.x > w)
        {
            if(str.size() == 1)
            {
                // A single character already overflows the available width;
                // retrying it would never make progress.
                elems.push_back(str);
                str.clear();
                continue;
            }

            //split words
            str.pop_back();
            elems.push_back(str);
            str.clear();
            --i;
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

Size Text::RefreshSizeWrap(bool update)
{
    if(!m_font)
    {
        GWENI_ASSERT_MSG(false, "Text::refreshSize() - No Font!");
        return Size(0, 0);
    }

    const int w=wrapWidth();
    std::vector<WrappedLine> lines;

    wrapLines(w, lines);

    const int lineHeight=this->lineHeight();
    const int height=static_cast<int>(lines.size())*lineHeight;

    if(!update)
        return Size(w, height);

    removeAllChildren();

    for(auto &&line:m_lines)
    {
        delete line;
    }
    m_lines.clear();
    m_lineLengths.clear();

    int y=0;

    for(auto &&line : lines)
    {
        Text *t=newChild<Text>();
        t->setFont(getFont());
        t->setTextColor(TextColor());
        t->setText(line.text);
        // Each line stays where it is put; measured here, as it did not
        // exist when this pass measured the tree.
        t->setAlignment(Alignment::Absolute);

        Size size=t->refreshSize();
        size.height=lineHeight;
        t->setPreferredSize(size);
        t->setPos(0, y);

        m_lines.push_back(t);
        m_lineLengths.push_back(line.length);
        y+=lineHeight;
    }

    // The parent's width and the height of the lines, the same as the
    // preferred size calculateSize gets from this function.
    setSize(w, height);

    return Size(w, height);
}

unsigned int Text::numLines()
{
    return static_cast<unsigned int>(m_lines.size());
}

Text *Text::getLine(int i)
{
    if(i < 0 || i >= static_cast<int>(m_lines.size()))
        return nullptr;

    return m_lines[i];
}

int Text::getLineLength(int i)
{
    if(i < 0 || i >= static_cast<int>(m_lineLengths.size()))
        return 0;

    return m_lineLengths[i];
}

// The line holding character i: the first one whose source range runs past
// it, so a '\n' belongs to the line it ends; past the end, the last line.
int Text::getLineFromChar(int i)
{
    int iChars=0;
    const int count=static_cast<int>(m_lineLengths.size());

    for(int iLine=0; iLine < count; ++iLine)
    {
        iChars+=m_lineLengths[iLine];

        if(iChars > i)
            return iLine;
    }

    return std::max(0, count-1);
}

int Text::getStartCharFromLine(int i)
{
    int iChars=0;
    const int count=std::min(i, static_cast<int>(m_lineLengths.size()));

    for(int iLine=0; iLine < count; ++iLine)
        iChars+=m_lineLengths[iLine];

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
    if(i >= static_cast<int>(m_lines.size())-1 || end <= start)
        return end;

    return end-1;
}

int Text::getCharPosOnLine(int i)
{
    int iLine=getLineFromChar(i);

    if(!getLine(iLine))
        return 0;

    int iStart=getStartCharFromLine(iLine);
    return i-iStart;
}

}//namespace internal
}//namespace controls
}//namespace gweni
