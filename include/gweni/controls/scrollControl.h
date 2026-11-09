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
    //! A container: its inner panel's children and its scroll bars draw.
    GWENI_DRAWS_NOTHING

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

    //! Virtual content: a vertical content height the inner panel does not
    //! hold, for a subclass that places only what is on screen itself
    //! (VirtualList). With one set (>= 0), the vertical bar is decided and
    //! ranged from it instead of the inner panel's preferred height, and the
    //! inner panel is not scrolled: it is placed at the top, the viewport's
    //! height, and the subclass places its children by
    //! getVerticalScrollOffset() in contentsPlaced(). So no child is ever
    //! placed far outside the viewport, however tall the content is. -1 (the
    //! default) turns it off.
    void setVirtualContentHeight(int height);
    int getVirtualContentHeight() const { return m_virtualHeight; }
    bool hasVirtualContent() const { return m_virtualHeight >= 0; }

    //! With virtual content, how far the content is scrolled, in units: from
    //! 0 to getMaxVerticalScrollOffset(). It is kept as a whole number, and the
    //! bar shows it as a fraction; a bar the user moves sets it (read by the
    //! next layout). A new offset
    //! is laid out by the next pass, which clamps it to the viewport it gives.
    int getVerticalScrollOffset() const { return m_virtualOffset; }
    void setVerticalScrollOffset(int offset);
    //! The virtual content height less the viewport's, never negative.
    int getMaxVerticalScrollOffset() const;
    //! With virtual content, the viewport's height (see getViewport()), as
    //! the last layout pass gave it; 0 before one.
    int getViewportHeight() const { return m_viewportHeight; }

    //! The area the content is seen through, in this control's coordinates,
    //! as the last layout pass placed it: this control less its padding, the
    //! bars that show and the inner panel's margin. Unscrolled, the inner
    //! panel's top-left is the viewport's; it is at least the viewport's size.
    const Rect &getViewport() const { return m_viewport; }

    //! The viewport, for the inner panel: the content is drawn and hit-tested
    //! only inside it, never in the inset band around it. The bars have none.
    const Rect *getChildClipRect(const Base *child) const override
    {
        return child == m_innerPanel?&m_viewport:nullptr;
    }
    bool hasChildClipRects() const override { return true; }

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

    //! Whether a size asked for on this axis is the size this control asks
    //! for, the bars inside it and the content scrolled (see calculateSize).
    //! With no request it asks for its content. A subclass that never scrolls
    //! its content on an axis returns false to ask for the content there, a
    //! request then only being the least it is given.
    bool sizedByRequest(Dim dim) const override { return hasRequestedSize(dim); }

    //! Decides which bars show and sizes the inner panel and the bars for this
    //! control's current size, before any of them is laid out. See arrange().
    void placeContents();

    //! Called by the Y arrange right after placeContents(), before any child is
    //! laid out, under a PlacementScope: a subclass with virtual content
    //! places its children in the inner panel here, from
    //! getVerticalScrollOffset() and getViewportHeight(). A child it creates,
    //! or whose content it changes, here is measured in X by it (calculateSize)
    //! before it returns, since the X measuring is over.
    virtual void contentsPlaced(skin::Base * /*skin*/) {}

    //! Sets the vertical bar to show m_virtualOffset.
    void showVirtualOffset();

    //! The content's size, which the size asked for with no request, the bars
    //! and the inner panel's size are worked out from: the inner panel's
    //! preferred size (measured by calculateSize), with the virtual content
    //! height when there is one. A subclass that places owner-placed children
    //! in the inner panel itself, which do not count in its preferred size,
    //! gives their extent here (a virtualised DataTable body its columns'
    //! width, so it scrolls sideways).
    virtual Size contentSize() const;

    bool m_canScrollHorizontal;
    bool m_canScrollVertical;

    bool m_autoHideBars;

    controls::BaseScrollBar *m_verticalScrollBar;
    controls::BaseScrollBar *m_horizontalScrollBar;

    //! The vertical bar's width and the horizontal bar's height, as last
    //! measured while shown. A hidden bar measures as nothing (its children are
    //! hidden with it), and deciding whether to show it needs its size.
    Size m_barSize=Size(15, 15);

    int m_virtualHeight=-1;
    int m_virtualOffset=0;
    int m_viewportHeight=0;
    Rect m_viewport;
    //! The vertical bar's scrolled amount when this control last set it from
    //! m_virtualOffset: a bar found elsewhere was moved by the user.
    float m_shownVirtualAmount=0.f;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollcontrol_h_
