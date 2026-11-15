/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#ifndef _gweni_markdown_example_h_
#define _gweni_markdown_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class CheckBoxWithLabel;
class Label;
class MarkdownView;
}

//! The reference Markdown document, resources/MarkdownDemo.md (every element
//! a MarkdownView shows), in a MarkdownView filling the demo under a "Wrap
//! code" toggle and a Label showing the url of the last link clicked (the
//! demo routes it there and opens nothing).
class MarkdownDemo: public controls::Base
{
public:
    GWENI_CONTROL(MarkdownDemo, controls::Base);

public:
    //! The file the demo shows, found as the skin's resources are
    static constexpr const char *FileName="MarkdownDemo.md";

    controls::MarkdownView *view() const { return m_view; }
    controls::CheckBoxWithLabel *wrapToggle() const { return m_wrapToggle; }
    //! The document's text as read from the file (a line naming it when it
    //! could not be read)
    const gweni::String &referenceText() const { return m_reference; }
    //! The url of the last link clicked, empty before the first
    const gweni::String &lastLink() const { return m_lastLink; }
    //! How many link clicks the demo was told of
    int linkClicks() const { return m_linkClicks; }

private:
    void onLinkClicked(event::Info info);
    void onWrapChanged(event::Info info);

    controls::MarkdownView *m_view=nullptr;
    controls::CheckBoxWithLabel *m_wrapToggle=nullptr;
    controls::Label *m_status=nullptr;
    gweni::String m_reference;
    gweni::String m_lastLink;
    int m_linkClicks=0;
};

}//namespace gweni

#endif//_gweni_markdown_example_h_
