/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_richText_example_h_
#define _gweni_richText_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class Label;
class RichText;
}

//! The same paragraph of every RichText span kind (bold, italic, bold italic,
//! inline code, coloured, struck through, underlined, a link and a long URL
//! link) at two widths, 420 and 260 units, each under a Label naming its
//! width, and a Label under them showing the url of the last link clicked
//! (the demo routes it there and opens nothing).
class RichTextDemo: public controls::Base
{
public:
    GWENI_CONTROL(RichTextDemo, controls::Base);

public:
    //! The widths the two paragraphs are laid out at
    static constexpr int WideWidth=420;
    static constexpr int NarrowWidth=260;

    controls::RichText *wide() const { return m_wide; }
    controls::RichText *narrow() const { return m_narrow; }
    //! The url of the last link clicked, empty before the first
    const gweni::String &lastLink() const { return m_lastLink; }
    //! How many link clicks the demo was told of
    int linkClicks() const { return m_linkClicks; }

private:
    void onLinkClicked(event::Info info);

    controls::RichText *m_wide=nullptr;
    controls::RichText *m_narrow=nullptr;
    controls::Label *m_status=nullptr;
    gweni::String m_lastLink;
    int m_linkClicks=0;
};

}//namespace gweni

#endif//_gweni_richText_example_h_
