/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_scrollcontrol_h_
#define _gweni_controls_scrollcontrol_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/scrollBar.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/horizontalScrollBar.h>


namespace gweni
{
namespace controls
{

class GWENI_EXPORT ScrollControl: public Base
{
public:

    GWENI_CONTROL(ScrollControl, Base);

    void layout(skin::Base *skin) override;
    void render(skin::Base *skin) override;

    virtual void setScroll(bool h, bool v);
    virtual void setAutoHideBars(bool should)
    {
        m_autoHideBars=should;
    }

    virtual bool canScrollHorizontal() const
    {
        return m_canScrollHorizontal;
    }

    virtual bool canScrollVertical() const
    {
        return m_canScrollVertical;
    }

    void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child) override;
    virtual void updateScrollBars();

    virtual void setVerticalScrollRequired(bool req);
    virtual void setHorizontalScrollRequired(bool req);

    virtual void setInnerSize(int w, int h);

    virtual void onVeritcalBarMoved(event::Info);
    virtual void onHorizontalBarMoved(event::Info);

    bool onMouseWheeled(int iDelta) override;

    virtual void scrollToBottom();
    virtual void scrollToTop();
    virtual void scrollToLeft();
    virtual void scrollToRight();

    virtual void clear();

protected:

    virtual bool contentsAreDocked();

    bool m_canScrollHorizontal;
    bool m_canScrollVertical;

    bool m_autoHideBars;

    controls::BaseScrollBar *m_verticalScrollBar;
    controls::BaseScrollBar *m_horizontalScrollBar;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollcontrol_h_
