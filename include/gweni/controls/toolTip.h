/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_tooltip_h_
#define _gweni_controls_tooltip_h_

#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{

//! The box a canvas shows a hovered control's tooltip in (Base::setToolTipText,
//! Base::setToolTip). A canvas owns at most one, as its last child, and reuses
//! it for every control: Canvas decides in its update phase when it shows, for
//! which control and where, so render only paints it. It takes no mouse input.
class GWENI_EXPORT ToolTip: public Label
{
public:

    GWENI_CONTROL(ToolTip, Label);

public:

    //! Gives a setToolTip() control it holds back to its owner.
    ~ToolTip() override;

    //! The skin's TooltipText, read again so a theme changed at run time
    //! reaches the box.
    void updateColors() override;

    //! The logical units between a control and its tooltip.
    static constexpr int Gap=2;

    //! Where a tooltip \p size big goes for a control at \p anchor (both in
    //! canvas coordinates), inside \p area: below the control with their left
    //! edges aligned, above it if there is no room below, and pulled left to
    //! stay inside the area's right edge (but never past its left one). A
    //! tooltip with room neither below nor above sits at the area's bottom.
    static Point placement(const Rect &anchor, const Point &size, const Rect &area);

    //! Fills in \p owner's tooltip, its text or its own control, sizes itself
    //! to it and moves to placement() for the owner's bounds in \p area. It is
    //! not shown: the canvas does that.
    void setContentFor(Base *owner, const Rect &area);

    //! Moves to placement() for \p owner's bounds in \p area, if it is not
    //! there already.
    void placeFor(Base *owner, const Rect &area);

    //! Gives the owner its setToolTip() control back, hidden, as its child
    //! again. \p ownerDeleted: the owner is going away, so it is deleted
    //! instead.
    void releaseContent(bool ownerDeleted);

    //! The setToolTip() control it holds while shown, else null.
    Base *content() const { return m_content; }

protected:

    //! The owner's setToolTip() control, re-parented here while shown.
    Base *m_content=nullptr;
    Base *m_contentOwner=nullptr;
};

}
}
#endif//_gweni_controls_tooltip_h_
