/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#include "demo/textDemo.h"
#include "gweni/controls/label.h"

#include <algorithm>
#include <string>

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(TextDemoLightPanel)
{
}

GWENI_CONTROL_CONSTRUCTOR(TextDemo)
{
    const int sizes[sizeCount]={8, 9, 10, 11, 12, 14, 16};
    const char *const sample="Illumination Tester 0123";
    //vertical stems side by side, where uneven gaps and colour fringes show most
    const char *const stems="minimum lllIII 1111";

    //the night theme's background as the ink, on TextDemoLightPanel's light fill
    const Color darkText(0x1a, 0x1b, 0x26);

    const int left=10;
    const int sampleLeft=50; //past the widest size label, "16 px"
    const int stemsLeft=280; //past the widest sample, at 16 px

    for(int i=0; i<sizeCount; ++i)
    {
        m_fonts[i].facename="OpenSans";
        m_fonts[i].size=static_cast<float>(sizes[i]);
    }

    //a Label's default alignment (Left|CenterV) places it in its parent, which
    //would override setPos; these rows are placed by hand
    auto newLabel=[](controls::Base *parent, const String &text, const Font *font,
        const Color *color, int x, int y)
    {
        controls::Label *label=parent->newChild<controls::Label>();
        label->setAlignment(Alignment::Absolute);
        if(font != nullptr)
            label->setFont(*font);
        if(color != nullptr)
            label->setTextColor(*color);
        label->setText(text);
        label->sizeToContents();
        label->setPos(x, y);
        return label;
    };

    //one block of rows, each size's sample and stems with the size beside them;
    //returns the height it took
    auto addRows=[&](controls::Base *parent, const Color *color, int y)
    {
        const int top=y;

        for(int i=0; i<sizeCount; ++i)
        {
            controls::Label *text=newLabel(parent, sample, &m_fonts[i], color, sampleLeft, y);
            newLabel(parent, stems, &m_fonts[i], color, stemsLeft, y);

            //the size, in the skin's default font, centred on its row
            controls::Label *size=newLabel(parent, std::to_string(sizes[i])+" px", nullptr, color, left, y);
            size->setPos(left, y+std::max(0, (text->getHeight()-size->getHeight())/2));

            y+=text->getHeight()+2;
        }
        return y-top;
    };

    int y=6;

    newLabel(this, "Light on dark (the theme)", nullptr, nullptr, left, y);
    y+=18;
    y+=addRows(this, nullptr, y)+10;

    newLabel(this, "Dark on light", nullptr, nullptr, left, y);
    y+=18;

    TextDemoLightPanel *panel=newChild<TextDemoLightPanel>();

    panel->setAlignment(Alignment::Absolute);
    panel->setPos(0, y);
    const int panelHeight=addRows(panel, &darkText, 6)+6;
    panel->setSize(620, panelHeight);
}

}//namespace gweni
