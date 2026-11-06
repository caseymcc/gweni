/*
	GWEN
	Copyright (c) 2011 Facepunch Studios
	See license in Gwen.h
*/
#ifndef _gweni_unicodeText_example_h_
#define _gweni_unicodeText_example_h_

#include <gweni/controls/base.h>

namespace gweni
{

//! Text outside the font's ASCII cache, so its glyphs are rasterised lazily and have
//! to reach the GPU atlas before they are drawn.
class UnicodeTextDemo: public controls::Base
{
public:
    GWENI_CONTROL(UnicodeTextDemo, controls::Base);
};

}//namespace gweni

#endif//_gweni_unicodeText_example_h_
