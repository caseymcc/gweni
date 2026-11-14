/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_controls_boardcolumn_h_
#define _gweni_controls_boardcolumn_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>

namespace gweni
{
namespace controls
{

//! A header above an independently scrolling body. Children made with
//! newChild() go into the body; dock cards Top, or put a layout::Flow there.
//! The body's vertical bar always shows, disabled when the cards fit, so
//! adding cards never changes their width. A vertical wheel over the body
//! is consumed even at an end or when it fits. Horizontal and Shift+wheel
//! bubble to the Board, as does a vertical wheel over the header.
class GWENI_EXPORT BoardColumn: public Base
{
public:
    GWENI_CONTROL(BoardColumn, Base);

public:
    //! The header, cards and body scroll bar draw.
    GWENI_DRAWS_NOTHING

    void setText(const String &text);
    const String &getText() const;
    Label *getHeader() const { return m_header; }
    ScrollControl *getBody() const { return m_body; }
    //! Fixed header height, at least 0. Default 36.
    void setHeaderHeight(int height);
    int getHeaderHeight() const { return m_header->getRequestedBounds().h; }

private:
    Label *m_header=nullptr;
    ScrollControl *m_body=nullptr;
};

}
}

#endif
