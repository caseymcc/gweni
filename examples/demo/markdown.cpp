/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/markdown.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/markdownView.h"
#include "gweni/platforms/baseRender.h"
#include "gweni/skin.h"

#include <fstream>
#include <iostream>
#include <sstream>

namespace gweni
{

// The file as bytes, or a line naming it (and one on stderr) when it cannot
// be read
static String readReference(controls::Base *demo)
{
    skin::Base *skin=demo->getSkin();
    const String path=(skin && skin->getRenderer())?
        skin->getRenderer()->getResourcePaths().getPath(ResourcePaths::Type::Other, MarkdownDemo::FileName)
        :String(MarkdownDemo::FileName);
    std::ifstream file(path, std::ios::binary);

    if(!file)
    {
        std::cerr<<"MarkdownDemo: cannot read "<<path<<"\n";
        return "*"+path+" could not be read.*\n";
    }

    std::ostringstream text;

    text<<file.rdbuf();
    return text.str();
}

GWENI_CONTROL_CONSTRUCTOR(MarkdownDemo)
{
    setDock(DockPosition::Center);

    m_wrapToggle=newChild<controls::CheckBoxWithLabel>("wrap");
    m_wrapToggle->label()->setText("Wrap code");
    m_wrapToggle->setPos(10, 8);
    m_wrapToggle->checkBox()->onCheckChanged.add(this, &MarkdownDemo::onWrapChanged);

    m_status=newChild<controls::Label>();
    m_status->setText("Click a link: the demo shows its url here");
    m_status->setPos(110, 10);

    m_view=newChild<controls::MarkdownView>();
    m_view->setDock(DockPosition::Center);
    m_view->setMargin(Margin(10, 34, 10, 10));
    m_view->onLinkClicked.add(this, &MarkdownDemo::onLinkClicked);
    m_reference=readReference(this);
    m_view->setMarkdown(m_reference);
}

void MarkdownDemo::onLinkClicked(event::Info info)
{
    m_lastLink=info.string;
    ++m_linkClicks;
    m_status->setText("Link: "+m_lastLink);
}

void MarkdownDemo::onWrapChanged(event::Info)
{
    m_view->setCodeWrap(m_wrapToggle->checkBox()->isChecked());
}

}//namespace gweni
