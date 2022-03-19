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
}

//void PageControl::setPageCount(unsigned int num)
//{
//    if(num >= MaxPages)
//        num=MaxPages;
//
//    for(unsigned int i=0; i < num; i++)
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
    Base *page=newChild<Base>(name);

    page->setDock(DockPosition::Center);

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

void PageControl::showPage(unsigned int index)
{
    if(m_currentPage == index)
        return;

    if(index >= m_pages.size())
        return;

    //hide current page
    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->hide();
        m_pages[m_currentPage]->setDock(DockPosition::None);
    }

    //show next page
    if(m_pages[index])
    {
        m_pages[index]->show();
        m_pages[index]->setDock(DockPosition::Center);
    }

    m_currentPage=index;
    m_back->setDisabled(m_currentPage == 0);
//    m_next->setDisabled(m_currentPage >= m_pageCount);
    m_next->setDisabled(m_currentPage >= m_pages.size());
    m_label->setText(format("Page %i of %i", m_currentPage+1, m_pages));

    if(getUseFinishButton())
    {
//        const bool bFinished=m_currentPage >= m_pageCount-1;
        const bool bFinished=(m_currentPage >= m_pages.size()-1);
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

Base *PageControl::getPage(unsigned int index)
{
    if(index >= m_pages.size())
        return nullptr;

    return m_pages[index];
}

void PageControl::removePage(unsigned int index)
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
    if(m_currentPage >= m_pages.size()-1)
        return;

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->setDock(DockPosition::None);

        if(m_animate)
        {
            anim::add(m_pages[m_currentPage],
                new anim::pos::X(m_pages[m_currentPage]->getX(), getWidth() *-1, 0.2f, true, 0.0f,
                    -1));
        }
        else
            m_pages[m_currentPage]->setHidden(true);
    }

    showPage(m_currentPage+1);

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->setDock(DockPosition::None);

        if(m_animate)
        {
            anim::add(m_pages[m_currentPage], new anim::pos::X(getWidth(), 0, 0.2f, false, 0.0f, -1));
        }
    }
}

void PageControl::previousPage(event::Info)
{
    if(m_currentPage == 0)
        return;

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->setDock(DockPosition::None);
        
        if(m_animate)
        {
            anim::add(m_pages[m_currentPage],
                new anim::pos::X(m_pages[m_currentPage]->getX(), getWidth(), 0.3f, true, 0.0f, -1));
        }
        else
            m_pages[m_currentPage]->setHidden(true);
    }

    showPage(m_currentPage-1);

    if(m_pages[m_currentPage])
    {
        if(m_animate)
        {
            m_pages[m_currentPage]->setDock(DockPosition::None);

            anim::add(m_pages[m_currentPage],
                new anim::pos::X(getWidth() *-1, 0, 0.3f, false, 0.0f, -1));
        }
    }
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
