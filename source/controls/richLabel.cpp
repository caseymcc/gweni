/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/controls/richLabel.h>
#include <gweni/controls/label.h>
#include <gweni/utility.h>

namespace gweni
{
namespace controls
{

const unsigned char Type_Text=0;
const unsigned char Type_Newline=1;

GWENI_CONTROL_CONSTRUCTOR(RichLabel)
{
    m_needsRebuild=false;
}

void RichLabel::addLineBreak()
{
    DividedText t;
    t.type=Type_Newline;
    m_textBlocks.push_back(t);
}

void RichLabel::addText(const String &text, Color color, Font *font)
{
    if(text.length() == 0)
        return;

    StringList lst;
    split(text, "\n", lst, false);

    for(size_t i=0; i < lst.size(); i++)
    {
        if(i > 0)
            addLineBreak();

        DividedText t;
        t.type=Type_Text;
        t.text=lst[i];
        t.color=color;
        t.font=font;
        m_textBlocks.push_back(t);
        m_needsRebuild=true;
        invalidate();
    }
}

bool RichLabel::sizeToChildren(bool w, bool h)
{
    rebuild();
    return ParentClass::sizeToChildren(w, h);
}

void RichLabel::splitLabel(const String &text, const Font &font,
    const DividedText &txt, int &x, int &y, int &lineheight)
{
    StringList lst;
    split(text, " ", lst, true);

    if(lst.size() == 0)
        return;

    int iSpaceLeft=getWidth()-x;
    // Does the whole word fit in?
    {
        Point StringSize=getSkin()->getRenderer()->measureText(font, text);

        if(iSpaceLeft > StringSize.x)
            return createLabel(text, txt, x, y, lineheight, true);
    }
    // If the first word is bigger than the line, just give up.
    {
        Point WordSize=getSkin()->getRenderer()->measureText(font, lst[0]);

        if(WordSize.x >= iSpaceLeft)
        {
            createLabel(lst[0], txt, x, y, lineheight, true);

            if(lst[0].size() >= text.size())
                return;

            String LeftOver=text.substr(lst[0].size()+1);
            return splitLabel(LeftOver, font, txt, x, y, lineheight);
        }
    }
    String strNewString="";

    for(size_t i=0; i < lst.size(); i++)
    {
        Point WordSize=getSkin()->getRenderer()->measureText(font, strNewString+lst[i]);

        if(WordSize.x > iSpaceLeft)
        {
            createLabel(strNewString, txt, x, y, lineheight, true);
            x=0;
            y+=lineheight;
            break;
        }

        strNewString+=lst[i];
    }

    String LeftOver=text.substr(strNewString.size()+1);
    return splitLabel(LeftOver, font, txt, x, y, lineheight);
}

void RichLabel::createLabel(const String &text, const DividedText &txt, int &x, int &y,
    int &lineheight, bool NoSplit)
{
    //
    // Use default font or is one set?
    //
    const Font *font=&getSkin()->getDefaultFont();

    if(txt.font)
        font=txt.font;

    //
    // This string is too long for us, split it up.
    //
    Point p=getSkin()->getRenderer()->measureText(*font, text);

    if(lineheight == -1)
        lineheight=p.y;

    if(!NoSplit)
    {
        if(x+p.x > getWidth())
            return splitLabel(text, *font, txt, x, y, lineheight);
    }

    //
    // Wrap
    //
    if(x+p.x >= getWidth())
        createNewline(x, y, lineheight);

    Label *label=newChild<Label>();
    label->setText(x == 0?trimLeft<String>(text,
        " "):text);
    label->setTextColor(txt.color);
    label->setFont(*font);
    label->sizeToContents();
    label->setPos(x, y);
    // lineheight = (lineheight + label->getHeight()) / 2;
    x+=label->getWidth();

    if(x >= getWidth())
        createNewline(x, y, lineheight);
}

void RichLabel::createNewline(int &x, int &y, int &lineheight)
{
    x=0;
    y+=lineheight;
}

void RichLabel::rebuild()
{
    removeAllChildren();
    int x=0;
    int y=0;
    int lineheight=-1;

    for(auto &&text : m_textBlocks)
    {
        if(text.type == Type_Newline)
        {
            createNewline(x, y, lineheight);
            continue;
        }

        if(text.type == Type_Text)
        {
            createLabel(text.text, text, x, y, lineheight, false);
            continue;
        }
    }

    m_needsRebuild=false;
}

void RichLabel::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);
    rebuild();
}

void RichLabel::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    if(m_needsRebuild)
        rebuild();
}

}//namespace controls
}//namespace gweni
