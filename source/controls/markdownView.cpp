/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/markdownView.h>

namespace gweni
{
namespace controls
{

GWENI_CONTROL_CONSTRUCTOR(MarkdownView)
{
    setScroll(false, true);
    setAutoHideBars(false);
    m_document=newChild<MarkdownDocument>();
    m_document->onLinkClicked.add(this, &MarkdownView::onDocumentLink);
}

void MarkdownView::onDocumentLink(event::Info info)
{
    info.control=this;
    onLinkClicked.call(this, info);
}

}//namespace controls
}//namespace gweni
