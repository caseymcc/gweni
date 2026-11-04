/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#include <gweni/gweni.h>
#include <gweni/utility.h>
#include <gweni/skin.h>
#include <gweni/controls/pageControl.h>
#include <gweni/anim.h>

namespace gweni
{
namespace controls
{


GWENI_CONTROL_CONSTRUCTOR(PageControl)
{
    m_animate=false;

    //m_pageCount=0;
    m_currentPage=0;
    setUseFinishButton(true);

//    for(size_t i=0; i < MaxPages; i++)
//    {
//        m_pages[i]=nullptr;
//    }

    m_controls=newChild<Base>();
    m_controls->setDock(DockPosition::Bottom);
    m_controls->setSize(24, 24);
    m_controls->setMargin(Margin(10, 10, 10, 10));
    m_finish=m_controls->newChild<Button>();
    m_finish->setText("finish");
    m_finish->setDock(DockPosition::Right);
    m_finish->onPressCaller.add(this, &ThisClass::finish);
    m_finish->setSize(70);
    m_finish->setMargin(Margin(4, 0, 0, 0));
    m_finish->hide();
    m_next=m_controls->newChild<Button>();
    m_next->setText("Next >");
    m_next->setDock(DockPosition::Right);
    m_next->onPressCaller.add(this, &ThisClass::nextPage);
    m_next->setSize(70);
    m_next->setMargin(Margin(4, 0, 0, 0));
    m_back=m_controls->newChild<Button>();
    m_back->setText("< Back");
    m_back->setDock(DockPosition::Right);
    m_back->onPressCaller.add(this, &ThisClass::previousPage);
    m_back->setSize(70);
    m_label=m_controls->newChild<Label>();
    m_label->setDock(DockPosition::Center);
    m_label->setAlignment(Alignment::Left|Alignment::CenterV);
    m_label->setText("Page 1 or 2");

    // The pages live in their own container that fills the area above the nav
    // bar. Each page docks Center and fills this container; only the current
    // page is shown, so there is exactly one visible page at the viewport.
    m_pageContainer=newChild<Base>();
    m_pageContainer->setDock(DockPosition::Center);
}

//void PageControl::setPageCount(size_t num)
//{
//    if(num >= MaxPages)
//        num=MaxPages;
//
//    for(size_t i=0; i < num; i++)
//    {
//        if(!m_pages[i])
//        {
//            m_pages[i]=newChild<Base>();
//            m_pages[i]->setDock(Position::Fill);
//        }
//    }
//
//    m_pageCount=num;
//    // Setting to -1 to force the page to change
//    m_currentPage=-1;
//    hideAll();
//    showPage(0);
//}

Base *PageControl::addPage(std::string name)
{
    Base *page=m_pageContainer->newChild<Base>(name);

    page->setDock(DockPosition::Center);

    // Only the current page is visible; hide the rest. Hiding before content is
    // added means that content inherits the hidden state (see Base::addChild).
    if(m_pages.size() != m_currentPage)
        page->hide();

    m_pages.push_back(page);

    return page;
}

std::vector<Base *> PageControl::addPages(size_t count)
{
    std::vector<Base *> pages;

    for(size_t i=0; i<count; ++i)
        pages.push_back(addPage());
    return pages;
}

void PageControl::hideAll()
{
//    for(size_t i=0; i < MaxPages; i++)
    for(size_t i=0; i<m_pages.size(); ++i)
    {
        if(!m_pages[i])
            continue;

        m_pages[i]->hide();
    }
}

void PageControl::showPage(size_t index)
{
    if(index >= m_pages.size())
        return;

    // Show only the requested page; hide the rest. Every page docks Center in
    // m_pageContainer, so the single visible page fills the viewport (hidden
    // pages are skipped by layout, so they don't stack or bleed).
    for(size_t i=0; i<m_pages.size(); ++i)
    {
        if(m_pages[i])
            m_pages[i]->setHidden(i != index);
    }

    m_currentPage=index;
    m_back->setDisabled(m_currentPage == 0);
    m_next->setDisabled(m_currentPage+1 >= m_pages.size());
    m_label->setText(format("Page %i of %i", (int)(m_currentPage+1), (int)m_pages.size()));

    if(getUseFinishButton())
    {
        const bool bFinished=(m_currentPage+1 >= m_pages.size());
        m_next->setHidden(bFinished);
        m_finish->setHidden(!bFinished);
    }

    {
        event::Info info(this);
        info.integer=index;
        info.control=m_pages[index];
        onPageChanged.call(this, info);
    }
}

Base *PageControl::getPage(size_t index)
{
    if(index >= m_pages.size())
        return nullptr;

    return m_pages[index];
}

void PageControl::removePage(size_t index)
{
    if(index >= m_pages.size())
        return;

    m_pages.erase(m_pages.begin()+index);
}

Base *PageControl::getCurrentPage()
{
    return getPage(getPageNumber());
}

void PageControl::nextPage(event::Info)
{
    if(m_currentPage+1 < m_pages.size())
        showPage(m_currentPage+1);
}

void PageControl::previousPage(event::Info)
{
    if(m_currentPage > 0)
        showPage(m_currentPage-1);
}

void PageControl::finish(event::Info)
{
    onFinish.call(this);
}

void PageControl::showControls(bool show)
{
    m_controls->setHidden(!show);
  
}

}//namespace controls
}//namespace gweni
