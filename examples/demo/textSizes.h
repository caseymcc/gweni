/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#ifndef _gweni_textSizes_example_h_
#define _gweni_textSizes_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

//! The same string at 9 to 72 px in two faces, all in one frame, so every font's
//! runs have to be drawn from that font's own atlas: the sizes below the SDF
//! threshold each from a coverage atlas, the rest from the face's SDF atlas. Laid out to fit 1024x768
//! (ControlTest --size 1024x768).
class TextSizesDemo: public controls::Base
{
public:
    GWENI_CONTROL(TextSizesDemo, controls::Base);

    static constexpr int faceCount=2;
    static constexpr int sizeCount=9;

    //a Label keeps a pointer to its font, so the fonts live as long as the page
    Font m_fonts[faceCount][sizeCount];
};

}//namespace gweni

#endif//_gweni_textSizes_example_h_
