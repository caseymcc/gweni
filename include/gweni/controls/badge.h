/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_badge_h_
#define _gweni_controls_badge_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{

//! A count bubble on a corner of another control (its anchor): unanswered
//! items on a navigation entry, a toolbar button's pending count. It is an
//! attached child of the anchor (Base::attachTo), so the anchor needs no code
//! of its own: the badge is centred on its corner, overhangs the anchor,
//! moves, hides and scrolls with it, is cut where an ancestor cuts the anchor
//! (a ScrollControl's viewport: leave the anchor a margin there), is deleted
//! with it, takes no mouse input (a click on it is the anchor's, or on its
//! overhang the control's under it) and is drawn in front of the anchor's
//! later siblings, under Popups, Menus and the ToolTip box.
//!
//! It shows setCount()'s number, "99+" past getCap(), or with setDot() a
//! plain dot. A count of 0 hides it (its own hidden flag) unless
//! setShowZero(). Neither it nor its count is in the anchor's size, so 9 to
//! 10 does not move the anchor; a new count writes the badge's own slots and
//! lays out the badge (and the way down to it) only when its width changes.
//!
//! Its number is the badge's own text run, drawn at its getAttachedZIndex()
//! with its bubble, not an internal::Text child: a child is numbered at its
//! own preorder z, under the anchor's later siblings.
//!
//! Made with newChild() on its anchor, it is attached to its top-right
//! corner; attach() moves it. On a VirtualList's or DataTable's pooled row
//! it follows the row, not the item: the row binder sets the count.
class GWENI_EXPORT Badge: public Base
{
public:

    GWENI_CONTROL(Badge, Base);

public:

    //! The count shown in full; past it the badge reads "<cap>+".
    static constexpr int DefaultCap=99;
    //! A dot's side, and a number's least width and height are its line's.
    static constexpr int DotSize=8;
    //! Units each side of the number.
    static constexpr int PaddingX=4;

    //! Attaches it to \p anchor's \p corner, moved by \p offset.
    void attach(Base *anchor, Corner corner=Corner::TopRight, const Point &offset=Point(0, 0));

    void setCount(int count);
    int getCount() const { return m_count; }

    //! The largest count shown as it is (at least 1).
    void setCap(int cap);
    int getCap() const { return m_cap; }

    //! Whether a count of 0 is shown ("0") rather than hiding the badge.
    void setShowZero(bool show);
    bool getShowZero() const { return m_showZero; }

    //! A plain dot instead of the number, shown whatever the count.
    void setDot(bool dot);
    bool isDot() const { return m_dot; }

    //! What it shows: the count, "<cap>+", or "" for a dot or nothing.
    const String &getText() const { return m_text; }
    //! The font its number is drawn in: the skin's default.
    const gweni::Font &getFont();
    //! Where its number's box is drawn, in its parent's coordinates (its
    //! bounds' origin added, as a Text's textOrigin()).
    gweni::Point textOrigin() const;

    void calculateSize(skin::Base *skin, Dim dim) override;

protected:

    //! The text and hidden flag for the count, cap, dot and show-zero:
    //! marks the text redrawn, and lays the badge out again when its width
    //! changes.
    void refresh();
    //! The size the badge takes for the measured text.
    gweni::Point sizeFor(const gweni::Point &textSize) const;

    int m_count=0;
    int m_cap=DefaultCap;
    bool m_showZero=false;
    bool m_dot=false;
    String m_text;
    //! m_text's measure, as the last calculateSize() or refresh() took it
    gweni::Point m_textSize;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_badge_h_
