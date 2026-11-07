/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_separator_h_
#define _gweni_controls_separator_h_

#include <gweni/controls/base.h>
#include <gweni/controls/label.h>

namespace gweni
{
namespace controls
{

//! A rule one logical unit thick, horizontal (the default) or vertical, that
//! takes the length its dock or layout gives it (its preferred length is 0).
//! A horizontal one may carry a caption (setText): the caption sits
//! captionInset() in from the left, the rule runs on both sides of it,
//! captionGap() clear of it, through the middle of the caption's line, and the
//! separator is then the caption's height. The textured skin draws the rule
//! max(1, snap(1)) device px thick wherever it lands on the pixel grid.
class GWENI_EXPORT Separator: public Base
{
public:

    GWENI_CONTROL(Separator, Base);

public:

    //! The orientation; each sets the size flags to grow along the rule only
    //! (Expand along, Shrink across).
    virtual void setVertical();
    virtual void setHorizontal();
    bool isHorizontal() const { return m_horizontal; }

    //! The caption; an empty string removes it. A vertical separator keeps
    //! the string but shows no caption.
    virtual void setText(const String &text);
    const String &getText() const;

    //! The caption's Label (for its font or colour), or null before the first
    //! setText().
    Label *getCaption() const { return m_caption; }

    //! Whether a caption is shown: a non-empty one on a horizontal separator.
    bool captionShown() const;

    //! Logical units from the separator's left edge to the caption.
    void setCaptionInset(int inset);
    int captionInset() const { return m_captionInset; }

    //! Logical units left clear between the caption and the rule on each side.
    void setCaptionGap(int gap);
    int captionGap() const { return m_captionGap; }

    void calculateSize(skin::Base *skin, Dim dim) override;
    void onChildBoundsChanged(gweni::Rect oldChildBounds, Base *child) override;

protected:

    bool m_horizontal=true;
    Label *m_caption=nullptr;
    int m_captionInset=16;
    int m_captionGap=6;
};

}
}
#endif//_gweni_controls_separator_h_
