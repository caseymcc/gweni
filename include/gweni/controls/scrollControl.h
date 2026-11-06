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

public:
    void render(skin::Base *skin) override;

    void calculateSize(skin::Base *skin, Dim dim) override;
    void arrange(skin::Base *skin, Dim dim) override;

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
    //! Asks for the scrollbars to be brought up to date with the content. The
    //! work is done by the next layout pass (arrange()); during a pass this
    //! does nothing, as the pass already does it.
    virtual void updateScrollBars();

    //! Shows (and enables) the bar when the content overflows that way, or
    //! scrolls it back to the start, disables it and, with auto-hide, hides it.
    virtual void setVerticalScrollRequired(bool required);
    virtual void setHorizontalScrollRequired(bool required);

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

    //! Decides which bars show and sizes the inner panel and the bars for this
    //! control's current size, before any of them is laid out. See arrange().
    void placeContents();

    bool m_canScrollHorizontal;
    bool m_canScrollVertical;

    bool m_autoHideBars;

    controls::BaseScrollBar *m_verticalScrollBar;
    controls::BaseScrollBar *m_horizontalScrollBar;

    //! The vertical bar's width and the horizontal bar's height, as last
    //! measured while shown. A hidden bar measures as nothing (its children are
    //! hidden with it), and deciding whether to show it needs its size.
    Size m_barSize=Size(15, 15);
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollcontrol_h_
