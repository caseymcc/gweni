/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/richText.h"
#include "gweni/controls/label.h"
#include "gweni/controls/richText.h"

#include <vector>

namespace gweni
{

using controls::RichSpan;

static std::vector<RichSpan> demoSpans()
{
    return {
        RichSpan::plain("A paragraph mixes "),
        RichSpan::plain("bold", RichSpan::Bold),
        RichSpan::plain(", "),
        RichSpan::plain("italic", RichSpan::Italic),
        RichSpan::plain(" and "),
        RichSpan::plain("bold italic", RichSpan::Bold|RichSpan::Italic),
        RichSpan::plain(" words with "),
        RichSpan::plain("inline_code()", RichSpan::Code),
        RichSpan::plain(", "),
        RichSpan::colored("green", Color(0x9e, 0xce, 0x6a, 255)),
        RichSpan::plain(" and "),
        RichSpan::colored("orange", Color(0xff, 0x9e, 0x64, 255)),
        RichSpan::plain(" text, "),
        RichSpan::plain("struck through", RichSpan::Strike),
        RichSpan::plain(" and "),
        RichSpan::plain("underlined", RichSpan::Underline),
        RichSpan::plain(" text and "),
        RichSpan::link("a link", "https://example.com/docs"),
        RichSpan::plain(", wrapping as one. A long address, "),
        RichSpan::link("https://github.com/caseymcc/gweni/blob/main/docs/architecture.md?plain=1&line=42",
            "https://github.com/caseymcc/gweni/blob/main/docs/architecture.md?plain=1&line=42"),
        RichSpan::plain(", breaks after its slashes."),
    };
}

// Each paragraph Fixed in width and as high as its lines there (Elastic in
// height), placed under its caption, room left under each for a few more
// lines; the status Label under both.
GWENI_CONTROL_CONSTRUCTOR(RichTextDemo)
{
    setDock(DockPosition::Center);

    controls::Label *label=newChild<controls::Label>();
    label->setText("At "+std::to_string(WideWidth)+" units");
    label->setPos(10, 10);

    m_wide=newChild<controls::RichText>();
    m_wide->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
    m_wide->setPos(10, 30);
    m_wide->setWidth(WideWidth);
    m_wide->setSpans(demoSpans());
    m_wide->onLinkClicked.add(this, &RichTextDemo::onLinkClicked);

    label=newChild<controls::Label>();
    label->setText("At "+std::to_string(NarrowWidth)+" units");
    label->setPos(10, 110);

    m_narrow=newChild<controls::RichText>();
    m_narrow->setSizeFlags({ SizeFlag::Fixed, SizeFlag::Elastic });
    m_narrow->setPos(10, 130);
    m_narrow->setWidth(NarrowWidth);
    m_narrow->setSpans(demoSpans());
    m_narrow->onLinkClicked.add(this, &RichTextDemo::onLinkClicked);

    m_status=newChild<controls::Label>();
    m_status->setText("Click a link: the demo shows its url here");
    m_status->setPos(10, 290);
}

void RichTextDemo::onLinkClicked(event::Info info)
{
    m_lastLink=info.string;
    ++m_linkClicks;
    m_status->setText("Clicked: "+m_lastLink);
}

}//namespace gweni
