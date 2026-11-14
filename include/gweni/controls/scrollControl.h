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

#include <memory>


namespace gweni
{
namespace controls
{

class Reorderable;

class GWENI_EXPORT ScrollControl: public Base
{
public:

    GWENI_CONTROL(ScrollControl, Base);

public:
    ~ScrollControl() override;

    //! Makes its items reorderable by drag and drop: the Reorderable (a
    //! CardReorder for the inner panel's cards, a list's own adapter) is this
    //! control's, which forwards its drag-and-drop hooks to it and has it
    //! place its looks as its layout ends. A press on anything inside the
    //! inner panel, in the viewport, starts the drag of the item under it.
    //! Null takes it away (a live drag of it is cancelled). Replaced or taken
    //! away while it is a live drag's target, the target is found again at
    //! the cursor in the next update phase, so the new one takes the drop.
    void setReorderable(std::unique_ptr<Reorderable> reorderable);
    Reorderable *getReorderable() const { return m_reorderable.get(); }

    bool dragAndDrop_draggable() override;
    bool dragAndDrop_takesDescendantDrag(Base *descendant) override;
    void dragAndDrop_pressed(int x, int y) override;
    draganddrop::Package *dragAndDrop_getPackage(int x, int y) override;
    void dragAndDrop_startDragging(draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_endDragging(bool success, int x, int y) override;
    bool dragAndDrop_canAcceptPackage(draganddrop::Package *package) override;
    void dragAndDrop_hoverEnter(draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_hover(draganddrop::Package *package, int x, int y) override;
    void dragAndDrop_hoverLeave(draganddrop::Package *package) override;
    bool dragAndDrop_handleDrop(draganddrop::Package *package, int x, int y) override;
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

    //! How far the content is scrolled on each axis, in whole units, from 0
    //! to getMax*ScrollOffset(): the scroll position is this offset, and the
    //! bars show it as a fraction of the content less the viewport. A bar the
    //! user moves (its thumb, its arrow buttons, the wheel) sets it, at the
    //! content size it was moved against, when the next layout reads it,
    //! rounded to a whole unit; a bar moved by under half a unit keeps its
    //! amount while it shows that offset, so small moves add up.
    //! Content that grows keeps the offset (the bar's amount and thumb follow
    //! the new size); each pass clamps it to the content and viewport it
    //! gives, and a bar no longer required takes it back to 0. A new offset
    //! is laid out by the next pass. With virtual content the inner panel is
    //! not scrolled vertically: the subclass places its children by
    //! getVerticalScrollOffset() in contentsPlaced(), and
    //! setVerticalScrollOffset() clamps to getMaxVerticalScrollOffset() at
    //! once.
    int getVerticalScrollOffset() const { return m_scrollOffset[1]; }
    //! Virtual: a VirtualList counts it among its scroll requests
    //! (VirtualList::scrollRequestSerial()).
    virtual void setVerticalScrollOffset(int offset);
    int getHorizontalScrollOffset() const { return m_scrollOffset[0]; }
    void setHorizontalScrollOffset(int offset);
    //! The content's size less the viewport's, never negative, as the last
    //! layout pass gave it (0 before one, or on an axis that cannot scroll);
    //! with virtual content, its height less the last pass's viewport height.
    int getMaxVerticalScrollOffset() const;
    int getMaxHorizontalScrollOffset() const { return m_scrollRange[0]; }
    //! The viewport's height (see getViewport()), as the last layout pass
    //! gave it; 0 before one.
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
    //! Uses the horizontal bar alone; bubbles when it cannot move.
    bool onMouseWheeledH(int iDelta) override;

    //! Scroll to an edge of the content: resolved by the next layout, to 0
    //! or getMax*ScrollOffset() of the content that layout gives, so a call
    //! right after adding content (ListBox::addItem) lands on its new end. A
    //! set*ScrollOffset() on that axis before then cancels it, even one that
    //! keeps the offset; a new virtual content height does not.
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

    //! m_scrollOffset's index for an axis.
    static int axis(Dim dim) { return dim == Dim::X?0:1; }
    BaseScrollBar *scrollBar(Dim dim) const
    {
        return dim == Dim::X?m_horizontalScrollBar:m_verticalScrollBar;
    }
    int getMaxScrollOffset(Dim dim) const
    {
        return dim == Dim::X?getMaxHorizontalScrollOffset():getMaxVerticalScrollOffset();
    }

    //! Sets dim's bar to show m_scrollOffset as a fraction of
    //! getMaxScrollOffset(dim), and records that amount in m_shownAmount.
    void showScrollOffset(Dim dim);

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
    int m_viewportHeight=0;
    Rect m_viewport;
    //! The scroll offset per axis (X, Y; see axis()), in units.
    int m_scrollOffset[2]={0, 0};
    //! The content less the viewport per axis, as the last pass placed it:
    //! a bar the user moved since is converted against it.
    int m_scrollRange[2]={0, 0};
    //! Each bar's scrolled amount when this control last set it from
    //! m_scrollOffset: a bar found elsewhere was moved by the user.
    float m_shownAmount[2]={0.f, 0.f};

    //! An edge a scrollTo*() asked for per axis, resolved by the next pass.
    enum class ScrollEdge
    {
        None,
        Start,
        End
    };

    ScrollEdge m_pendingEdge[2]={ScrollEdge::None, ScrollEdge::None};

    void scrollToEdge(Dim dim, ScrollEdge edge);

private:
    //! Which bars placeContents() requires and shows for this control's size
    //! and a content size.
    struct BarDecision
    {
        bool verticalRequired=false;
        bool horizontalRequired=false;
        bool verticalShown=false;
        bool horizontalShown=false;
    };

    BarDecision decideBars(const Size &content) const;

    std::unique_ptr<Reorderable> m_reorderable;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_scrollcontrol_h_
