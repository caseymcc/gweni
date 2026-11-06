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
    invalidate(controls::StateChange_Text);
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
        TextLines::iterator it=m_lines.begin();
        TextLines::iterator itEnd=m_lines.end();
        unsigned int iChars=0;

        Text *line=nullptr;
        while(it != itEnd)
        {
            line=*it;
            ++it;
            iChars+=line->length();

            if(iChars <= value)
                continue;

            iChars-=line->length();
            gweni::Rect rect=line->getCharacterPosition(value-iChars);
            rect.x+=line->getX();
            rect.y+=line->getY();
            return rect;
        }

        //manage special case of the last character
        gweni::Rect rect=line->getCharacterPosition(line->length());
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
        TextLines::iterator it=m_lines.begin();
        TextLines::iterator itEnd=m_lines.end();
        int iChars=0;

        Text *line=nullptr;
        while(it != itEnd)
        {
            line=*it;
            ++it;
            iChars+=line->length();

            if(p.y < line->getY())
                continue;
            if(p.y > line->bottom())
                continue;
            if(p.y < line->bottom())
                break;
        }

        iChars-=line->length();
        int iLinePos=line->getClosestCharacter(gweni::Point(p.x-line->getX(), p.y-line->getY()));
        return iChars+iLinePos;
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
    for(auto &&line : m_lines)
        line->calculateSize(skin, dim);

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

void Text::splitWords(const gweni::String &s, std::vector<gweni::String> &elems)
{
    gweni::String str;

    int w=wrapWidth();

    if(w<=0) //unable to place string in nothingness
        return;

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

Size Text::RefreshSizeWrap(bool update)
{
    if(update)
    {
        removeAllChildren();

        for(auto &&line:m_lines)
        {
            delete line;
        }
        m_lines.clear();
    }

    std::vector<gweni::String> words;
    splitWords(getText(), words);

    // Adding a word to the end simplifies the code below
    // which is anything but simple.
    words.push_back("");

    if(!m_font)
    {
        GWENI_ASSERT_MSG(false, "Text::refreshSize() - No Font!");
        return Size(0, 0);
    }

    Point fontSize=getSkin()->getRenderer()->measureText(getFont(), " ");
    int w=wrapWidth();
    int x=0, y=0;
    gweni::String strLine;

    for(auto &&it=words.begin(); it != words.end(); ++it)
    {
        bool bFinishLine=false;
        bool bWrapped=false;

        // If this word is a newline - make a newline (we still add it to the
        // text)
        if((*it).c_str()[0] == '\n')
            bFinishLine=true;

        // Does adding this word drive us over the width?
        {
            strLine+=*it;
            gweni::Point p=getSkin()->getRenderer()->measureText(getFont(), strLine);

            // Only the parent's width counts: this control's own is what the
            // last wrap made it, and comparing with it kept lines that no
            // longer fit once the parent got narrower.
            if(p.x > w)
            {
                bFinishLine=true;
                bWrapped=true;
            }
        }

        // If this is the last word then finish the line
        if(--words.end() == it)
            bFinishLine=true;

        if(bFinishLine)
        {
            if(update)
            {
                Text *t=newChild<Text>();
                t->setFont(getFont());
                t->setTextColor(TextColor());
                if(bWrapped)
                {
                    t->setText(strLine.substr(0, strLine.length()-(*it).length()));
                    // newline should start with the word that was too big
                    strLine=*it;
                }
                else
                {
                    t->setText(strLine.substr(0, strLine.length()));
                    //new line is empty
                    strLine.clear();
                }
                // Each line stays where it is put; measured here, as it
                // did not exist when this pass measured the tree.
                t->setAlignment(Alignment::Absolute);
                t->setPreferredSize(t->refreshSize());
                t->setPos(x, y);
                m_lines.push_back(t);
            }
            else
            {
                if(bWrapped)
                    strLine=*it;
                else
                    strLine.clear();
            }
            // newline should start with the word that was too big
            // strLine = *it;

            // Position the newline
            y+=fontSize.y;
            x=0;
            // if ( strLine[0] == ' ' ) x -= fontSize.x;
        }
    }

    // The parent's width and the height of the lines, the same as the
    // preferred size calculateSize gets from this function.
    if(update)
        setSize(w, y);

    return Size(w, y);
}

unsigned int Text::numLines()
{
    return static_cast<unsigned int>(m_lines.size());
}

Text *Text::getLine(int i)
{
    TextLines::iterator it=m_lines.begin();
    TextLines::iterator itEnd=m_lines.end();

    while(it != itEnd)
    {
        if(i == 0)
            return *it;

        ++it;
        i--;
    }

    return nullptr;
}

int Text::getLineFromChar(int i)
{
    int iChars=0;
    int iLine=0;

    for(auto &&line : m_lines)
    {
        iChars+=line->length();

        if(iChars > i)
            return iLine;

        iLine++;
    }

    if(iLine > 0)
        return iLine-1;

    return iLine;
}

int Text::getStartCharFromLine(int i)
{
    int iChars=0;

    for(auto &&line : m_lines)
    {
        if(i == 0)
            return gweni::clamp(iChars, 0, length());

        iChars+=line->length();
        i--;
    }

    return gweni::clamp(iChars, 0, length());
}

int Text::getEndCharFromLine(int i)
{
    int iStart=getStartCharFromLine(i);
    Text *iLine=getLine(i);

    if(iLine)
        iStart+=iLine->length();

    return gweni::clamp(iStart, 0, length());
}

int Text::getCharPosOnLine(int i)
{
    int iLine=getLineFromChar(i);
    Text *line=getLine(iLine);

    if(!line)
        return 0;

    int iStart=getStartCharFromLine(iLine);
    return i-iStart;
}

}//namespace internal
}//namespace controls
}//namespace gweni
