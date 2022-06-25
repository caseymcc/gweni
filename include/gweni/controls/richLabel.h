/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_richlabel_h_
#define _gweni_controls_richlabel_h_

#include <gweni/platforms/baseRender.h>
#include <gweni/controls/base.h>
#include <gweni/controls/text.h>


namespace gweni
{
namespace controls
{
class GWENI_EXPORT RichLabel: public controls::Base
{
public:

    GWENI_CONTROL(RichLabel, gweni::controls::Base);

public:
    void addLineBreak();
    void addText(const gweni::String &text, gweni::Color color, gweni::Font *font=nullptr);

    bool sizeToChildren(bool w=true, bool h=true) override;

protected:

    struct DividedText
    {
        typedef std::list<DividedText> List;
        DividedText()
        {
            type=0;
            font=nullptr;
        }

        unsigned char type;
        gweni::String text;
        gweni::Color color;
        gweni::Font *font;
    };


    void layout(gweni::skin::Base *skin) override;
    void splitLabel(const gweni::String &text, const gweni::Font &font,
        const DividedText &txt, int &x, int &y, int &lineheight);
    void createNewline(int &x, int &y, int &lineheight);
    void createLabel(const gweni::String &text, const DividedText &txt, int &x,
        int &y, int &lineheight, bool NoSplit);
    void rebuild();

    void onBoundsChanged(gweni::Rect oldBounds) override;

    DividedText::List m_textBlocks;
    bool m_needsRebuild;
};


}
}
#endif//_gweni_controls_richlabel_h_
