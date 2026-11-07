/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#include "demo/textSizes.h"
#include "gweni/controls/label.h"

#include <algorithm>
#include <string>

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(TextSizesDemo)
{
    const char *const faces[faceCount]={"OpenSans", "LiberationSans-Regular"};
    //18 is OpenGLCore's default SDF threshold: the smaller sizes before it are coverage text
    const int sizes[sizeCount]={9, 11, 14, 16, 18, 24, 36, 48, 72};
    //caps, ascenders, descenders, digits and punctuation, short enough for 72 px in 1024
    const char *const sample="Sphinx gyp 0123456789 &?!";

    const int left=10;
    const int textLeft=46; //past the widest size label, "72 px"
    int y=4;

    //a Label's default alignment (Left|CenterV) places it in its parent, which
    //would override setPos; these rows are placed by hand
    auto newLabel=[this]()
    {
        controls::Label *label=newChild<controls::Label>();
        label->setAlignment(Alignment::Absolute);
        return label;
    };

    for(int face=0; face<faceCount; ++face)
    {
        controls::Label *heading=newLabel();
        heading->setText(faces[face]);
        heading->sizeToContents();
        heading->setPos(left, y);
        y+=heading->getHeight()+2;

        for(int i=0; i<sizeCount; ++i)
        {
            Font &font=m_fonts[face][i];
            font.facename=faces[face];
            font.size=static_cast<float>(sizes[i]);

            controls::Label *text=newLabel();
            text->setFont(font);
            text->setText(sample);
            text->sizeToContents();
            text->setPos(textLeft, y);

            //the size, in the skin's default font, centred on its row
            controls::Label *size=newLabel();
            size->setText(std::to_string(sizes[i])+" px");
            size->sizeToContents();
            size->setPos(left, y+std::max(0, (text->getHeight()-size->getHeight())/2));

            y+=text->getHeight();
        }
        y+=6;
    }
}

}//namespace gweni
