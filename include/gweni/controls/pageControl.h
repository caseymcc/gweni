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
    Base *addPage(std::string name="");
    std::vector<Base *> addPages(size_t count);

//    virtual void setPageCount(unsigned int i);
    virtual unsigned int getPageCount()
    {
//        return m_pageCount;
        return m_pages.size();
    }

    virtual void showPage(unsigned int index);
    virtual unsigned int getPageNumber()
    {
        return m_currentPage;
    }

    virtual Base *getPage(unsigned int index);
    virtual void removePage(unsigned int index);
    virtual Base *getCurrentPage();

    virtual void setAnimated(bool animate) { m_animate=animate; }
    virtual bool getAnimated() { return m_animate; }

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

    void showControls(bool show=true);

    event::Caller onPageChanged;
    event::Caller onFinish;

protected:

    virtual void nextPage(event::Info);
    virtual void previousPage(event::Info);
    virtual void finish(event::Info);

    virtual void hideAll();

    unsigned int m_currentPage;
//    unsigned int m_pageCount;
//    Base *m_pages[MaxPages];
    std::vector<Base *> m_pages;

    bool m_animate;
    Base *m_controls;
    Button *m_next;
    Button *m_back;
    Button *m_finish;
    Label *m_label;

    bool m_useFinish;

};


}
}
#endif//_gweni_controls_pagecontrol_h_
