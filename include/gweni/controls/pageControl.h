/*
  * Gweni
  * Copyright (c) 2012 Facepunch Studios

  * See license in gweni.h
 */


#ifndef _gweni_controls_pagecontrol_h_
#define _gweni_controls_pagecontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>

namespace gweni
{
namespace controls
{
class GWENI_EXPORT PageControl: public Base
{
    static const unsigned int MaxPages=64;

    GWENI_CONTROL(PageControl, Base);

public:
    virtual void         setPageCount(unsigned int i);
    virtual unsigned int getPageCount()
    {
        return m_pageCount;
    }

    virtual void         showPage(unsigned int i);
    virtual unsigned int getPageNumber()
    {
        return m_currentPage;
    }

    virtual controls::Base *getPage(unsigned int i);
    virtual controls::Base *getCurrentPage();

    controls::Button *nextButton()
    {
        return m_next;
    }

    controls::Button *backButton()
    {
        return m_back;
    }

    controls::Button *finishButton()
    {
        return m_finish;
    }

    controls::Label *label()
    {
        return m_label;
    }

    void setUseFinishButton(bool b)
    {
        m_useFinish=b;
    }

    bool getUseFinishButton()
    {
        return m_useFinish;
    }

    event::Caller onPageChanged;
    event::Caller onFinish;

protected:

    virtual void nextPage(event::Info);
    virtual void previousPage(event::Info);
    virtual void finish(event::Info);

    virtual void hideAll();

    unsigned int m_currentPage;
    unsigned int m_pageCount;
    controls::Base *m_pages[MaxPages];

    controls::Button *m_next;
    controls::Button *m_back;
    controls::Button *m_finish;
    controls::Label *m_label;

    bool m_useFinish;

};


}
}
#endif//_gweni_controls_pagecontrol_h_
