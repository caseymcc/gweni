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
    m_pageCount=0;
    m_currentPage=0;
    setUseFinishButton(true);

    for(size_t i=0; i < MaxPages; i++)
    {
        m_pages[i]=nullptr;
    }

    Base *controls=newChild<Base>();
    controls->dock(Position::Bottom);
    controls->setSize(24, 24);
    controls->setMargin(Margin(10, 10, 10, 10));
    m_finish=controls->newChild<Button>();
    m_finish->setText("finish");
    m_finish->dock(Position::Right);
    m_finish->onPressCaller.add(this, &ThisClass::finish);
    m_finish->setSize(70);
    m_finish->setMargin(Margin(4, 0, 0, 0));
    m_finish->hide();
    m_next=controls->newChild<Button>();
    m_next->setText("Next >");
    m_next->dock(Position::Right);
    m_next->onPressCaller.add(this, &ThisClass::nextPage);
    m_next->setSize(70);
    m_next->setMargin(Margin(4, 0, 0, 0));
    m_back=controls->newChild<Button>();
    m_back->setText("< Back");
    m_back->dock(Position::Right);
    m_back->onPressCaller.add(this, &ThisClass::previousPage);
    m_back->setSize(70);
    m_label=controls->newChild<Label>();
    m_label->dock(Position::Fill);
    m_label->setAlignment(Position::Left|Position::CenterV);
    m_label->setText("Page 1 or 2");
}

void PageControl::setPageCount(unsigned int num)
{
    if(num >= MaxPages)
        num=MaxPages;

    for(unsigned int i=0; i < num; i++)
    {
        if(!m_pages[i])
        {
            m_pages[i]=newChild<Base>();
            m_pages[i]->dock(Position::Fill);
        }
    }

    m_pageCount=num;
    // Setting to -1 to force the page to change
    m_currentPage=-1;
    hideAll();
    showPage(0);
}

void PageControl::hideAll()
{
    for(size_t i=0; i < MaxPages; i++)
    {
        if(!m_pages[i])
            continue;

        m_pages[i]->hide();
    }
}

void PageControl::showPage(unsigned int i)
{
    if(m_currentPage == i)
        return;

    if(m_pages[i])
    {
        m_pages[i]->show();
        m_pages[i]->dock(Position::Fill);
    }

    m_currentPage=i;
    m_back->setDisabled(m_currentPage == 0);
    m_next->setDisabled(m_currentPage >= m_pageCount);
    m_label->setText(format("Page %i of %i", m_currentPage+1, m_pages));

    if(getUseFinishButton())
    {
        const bool bFinished=m_currentPage >= m_pageCount-1;
        m_next->setHidden(bFinished);
        m_finish->setHidden(!bFinished);
    }

    {
        event::Info info(this);
        info.integer=i;
        info.control=m_pages[i];
        onPageChanged.call(this, info);
    }
}

Base *PageControl::getPage(unsigned int i)
{
    return m_pages[i];
}

Base *PageControl::getCurrentPage()
{
    return getPage(getPageNumber());
}

void PageControl::nextPage(event::Info)
{
    if(m_currentPage >= m_pageCount-1)
        return;

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->dock(Position::None);
        anim::add(m_pages[m_currentPage],
            new anim::pos::X(m_pages[m_currentPage]->getX(), getWidth() *-1, 0.2f, true, 0.0f,
                -1));
    }

    showPage(m_currentPage+1);

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->dock(Position::None);
        anim::add(m_pages[m_currentPage], new anim::pos::X(getWidth(), 0, 0.2f, false, 0.0f, -1));
    }
}

void PageControl::previousPage(event::Info)
{
    if(m_currentPage == 0)
        return;

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->dock(Position::None);
        anim::add(m_pages[m_currentPage],
            new anim::pos::X(m_pages[m_currentPage]->getX(), getWidth(), 0.3f, true, 0.0f, -1));
    }

    showPage(m_currentPage-1);

    if(m_pages[m_currentPage])
    {
        m_pages[m_currentPage]->dock(Position::None);
        anim::add(m_pages[m_currentPage],
            new anim::pos::X(getWidth() *-1, 0, 0.3f, false, 0.0f, -1));
    }
}

void PageControl::finish(event::Info)
{
    onFinish.call(this);
}

}//namespace controls
}//namespace gweni
