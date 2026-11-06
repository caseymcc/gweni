/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#ifndef _gweni_textDemo_example_h_
#define _gweni_textDemo_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

//! A flat light rectangle (drawn by demoControlRenderers.cpp), so text can be
//! shown dark-on-light as well as in the theme's light-on-dark.
class TextDemoLightPanel: public controls::Base
{
public:
    GWENI_CONTROL(TextDemoLightPanel, controls::Base);
};

//! Small UI text at 8 to 16 px in OpenSans, once light-on-dark (the theme) and
//! once dark-on-light, to show how glyphs sit on the pixel grid: letter gaps,
//! stem weight and colour fringes. Fits 640x480.
class TextDemo: public controls::Base
{
public:
    GWENI_CONTROL(TextDemo, controls::Base);

    static constexpr int sizeCount=7;

    //a Label keeps a pointer to its font, so the fonts live as long as the page
    Font m_fonts[sizeCount];
};

}//namespace gweni

#endif//_gweni_textDemo_example_h_
