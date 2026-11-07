#include <gweni/controls/layout/layout.h>

namespace gweni
{
namespace controls
{


LayoutItem::LayoutItem():
    m_parent(nullptr),
    m_layoutParent(nullptr),
    m_control(nullptr),
    m_margin(0, 0, 0, 0),
    m_padding(0, 0, 0, 0),
    m_spacing(2),
    m_sizeFlags(SizeFlag::Fixed, SizeFlag::Fixed)
{

}

LayoutItem::LayoutItem(LayoutType type):
    m_parent(nullptr),
    m_layoutParent(nullptr),
    m_control(nullptr),
    m_type(type),
    m_margin(0, 0, 0, 0),
    m_padding(0, 0, 0, 0),
    m_spacing(2),
    m_sizeFlags(SizeFlag::Fixed, SizeFlag::Fixed)
{

}

LayoutItem::~LayoutItem()
{
    for(LayoutItem *child:m_children)
        delete child;

    m_children.clear();
}

void LayoutItem::setParent(Base *parent)
{
    m_parent=parent;
}

void LayoutItem::setLayoutParent(LayoutItem *layoutParent)
{
    m_layoutParent=layoutParent;
}

void LayoutItem::addControl(Base *control)
{
    LayoutItem *layoutItem=new LayoutItem();
    LayoutItem *parentLayout=m_parent->getLayout();
    LayoutItem *controlLayout=nullptr;

    if(parentLayout)
        controlLayout=parentLayout->findControl(control);

    //if it already belongs to a layout need to remove
    if(controlLayout)
    {
        LayoutItem *controlParentLayout=controlLayout->getLayoutParent();

        if(controlParentLayout)
            controlParentLayout->removeControl(control);
    }

    layoutItem->setParent(m_parent);
    layoutItem->setLayoutParent(this);
    layoutItem->setControl(control);
    m_children.push_back(layoutItem);
}

void LayoutItem::removeControl(Base *control)
{
    for(auto iter=m_children.begin(); iter!=m_children.end(); ++iter)
    {
        if((*iter)->getControl() == control)
        {
            LayoutItem *item=*iter;

            m_children.erase(iter);
            delete item;
            break;
        }
    }
}

LayoutItem *LayoutItem::findControl(Base *control)
{
    for(auto iter=m_children.begin(); iter!=m_children.end(); ++iter)
    {
        if((*iter)->getControl()==control)
            return (*iter);
        else
        {
            LayoutItem *item=(*iter)->findControl(control);

            if(item)
                return item;
        }
    }

    return nullptr;
}

void LayoutItem::addLayout(LayoutItem *item)
{
    item->setParent(m_parent);
    item->setLayoutParent(this);
    m_children.push_back(item);
}

void LayoutItem::removeLayout(LayoutItem *item)
{
    for(auto iter=m_children.begin(); iter!=m_children.end(); ++iter)
    {
        if(*iter == item)
        {
            LayoutItem *localItem=*iter;

            m_children.erase(iter);
            delete localItem;
            break;
        }
    }
}

Base *LayoutItem::getControl()
{
    return m_control;
}

void LayoutItem::setControl(Base *control)
{
    m_control=control;
}

void LayoutItem::updateControl()
{
    if(!m_control)
        return;

    //    Rect bounds=m_control->GetNeeded();
    //    Margin margin=m_control->getMargin();
    //
    //    bounds.x=m_bounds.x+margin.left;
    //    bounds.y=m_bounds.y+margin.top;
    //    m_control->setBounds(bounds);
}

void LayoutItem::setPadding(const Padding &padding)
{
    m_padding=padding;

    if(m_parent)
        m_parent->invalidate();
}

void LayoutItem::setSpacing(int spacing)
{
    m_spacing=spacing;

    if(m_parent)
        m_parent->invalidate();
}

namespace
{

// One axis of a size, margin or size flags, so the size pass is written once
// for both axes.
int &length(Size &size, Dim dim)
{
    return (dim == Dim::X)?size.width:size.height;
}

int length(const Size &size, Dim dim)
{
    return (dim == Dim::X)?size.width:size.height;
}

//! both sides of a margin or padding along dim
int sides(const Margin &margin, Dim dim)
{
    return (dim == Dim::X)?margin.left+margin.right:margin.top+margin.bottom;
}

SizeFlag &flag(SizeFlags &sizeFlags, Dim dim)
{
    return (dim == Dim::X)?sizeFlags.horizontal:sizeFlags.vertical;
}

SizeFlag flag(const SizeFlags &sizeFlags, Dim dim)
{
    return (dim == Dim::X)?sizeFlags.horizontal:sizeFlags.vertical;
}

}//namespace

void LayoutItem::calculateSize(skin::Base *skin, Dim dim)
{
    if(m_control)
    {
        m_control->calculateSize(skin, dim);

        // The cell holds the control and its margins, so its minimum and
        // maximum do too, and minimum <= preferred <= maximum holds from here up.
        const int margins=sides(m_control->getMargin(), dim);

        length(m_minimumSize, dim)=addWithOverflow(length(m_control->getMinimumSize(), dim), margins);
        length(m_maximumSize, dim)=addWithOverflow(length(m_control->getMaximumSize(), dim), margins);
        length(m_preferredSize, dim)=length(m_control->getPreferredSize(), dim)+margins;

        m_sizeFlags=m_control->getSizeFlags();
        return;
    }

    for(LayoutItem *child:m_children)
        child->calculateSize(skin, dim);

    const Dim along=(m_type == LayoutType::Horizontal)?Dim::X:Dim::Y;

    if(dim == along)
        calculateAlong(dim);
    else
        calculateAcross(dim);
}

// Along the stacking axis the children follow each other: the preferred,
// minimum and maximum lengths are each the children's sum, the spacing between
// them and the padding. A sum saturates at INT_MAX, so the maximum is
// unbounded only when a child's is.
void LayoutItem::calculateAlong(Dim dim)
{
    int preferred=0;
    int minimum=0;
    int maximum=0;
    SizeFlag sizeFlag=SizeFlag::Fixed;
    bool addSpacing=false;

    for(LayoutItem *child:m_children)
    {
        const int spacing=addSpacing?m_spacing:0;

        addSpacing=true;
        preferred=addWithOverflow(addWithOverflow(preferred, spacing), length(child->getPreferredSize(), dim));
        minimum=addWithOverflow(addWithOverflow(minimum, spacing), length(child->getMinimumSize(), dim));
        maximum=addWithOverflow(addWithOverflow(maximum, spacing), length(child->getMaximumSize(), dim));
        sizeFlag=merge(sizeFlag, flag(child->getSizeFlags(), dim));
    }

    const int padding=sides(m_padding, dim);

    length(m_preferredSize, dim)=addWithOverflow(preferred, padding);
    length(m_minimumSize, dim)=addWithOverflow(minimum, padding);
    length(m_maximumSize, dim)=addWithOverflow(maximum, padding);
    flag(m_sizeFlags, dim)=sizeFlag;
}

// Across the stacking axis the children share one length: the preferred and
// minimum lengths are the largest child's, the maximum the smallest child's
// maximum (INT_MAX with no children), each with the padding. Children whose
// ranges do not overlap give a minimum above the maximum: no length fits them
// all.
void LayoutItem::calculateAcross(Dim dim)
{
    int preferred=0;
    int minimum=0;
    int maximum=std::numeric_limits<int>::max();
    SizeFlag sizeFlag=SizeFlag::Fixed;

    for(LayoutItem *child:m_children)
    {
        preferred=std::max(preferred, length(child->getPreferredSize(), dim));
        minimum=std::max(minimum, length(child->getMinimumSize(), dim));
        maximum=std::min(maximum, length(child->getMaximumSize(), dim));
        sizeFlag=merge(sizeFlag, flag(child->getSizeFlags(), dim));
    }

    const int padding=sides(m_padding, dim);

    length(m_preferredSize, dim)=addWithOverflow(preferred, padding);
    length(m_minimumSize, dim)=addWithOverflow(minimum, padding);
    length(m_maximumSize, dim)=addWithOverflow(maximum, padding);
    flag(m_sizeFlags, dim)=sizeFlag;
}

void LayoutItem::updateControlSize(Dim dim)
{
    // Sized from the control's request and preferred size against this cell
    // (Base::layoutCellSize), never from its current bounds, which an earlier
    // pass may have grown or shrunk. The cell holds the margins
    // (calculateSize adds them), so the control gets what is inside them.
    Margin margin=m_control->getMargin();
    Rect bounds=m_control->getBounds();

    if(dim == Dim::X)
    {
        bounds.x=m_bounds.x+margin.left;
        bounds.w=m_control->layoutCellSize(dim, std::max(0, m_bounds.w-margin.left-margin.right));
    }
    else
    {
        bounds.y=m_bounds.y+margin.top;
        bounds.h=m_control->layoutCellSize(dim, std::max(0, m_bounds.h-margin.top-margin.bottom));
    }

    m_control->setArrangedBounds(bounds.x, bounds.y, bounds.w, bounds.h);
}

void LayoutItem::arrange(skin::Base *skin, Dim dim)
{
    if(m_control)
    {
        updateControlSize(dim);
        m_control->arrange(skin, dim);
        return;
    }

    switch(m_type)
    {
    case LayoutType::Horizontal:
        if(dim == Dim::X)
            arrangeHorizontal();
        else
            arrangeVerticalPos();
        break;
    case LayoutType::Vertical:
        if(dim == Dim::X)
            arrangeHorizontalPos();
        else
            arrangeVertical();
        break;
    }

    for(LayoutItem *child:m_children)
        child->arrange(skin, dim);
}

void LayoutItem::arrangeHorizontal()
{
    int x=m_bounds.x+m_padding.left;

    int shrink=0;
    int expand=0;
    int bloat=0;

    for(LayoutItem *child:m_children)
    {
        const SizeFlags &sizeFlags=child->getSizeFlags();

        if(sizeFlags.horizontal==SizeFlag::Shrink)
            shrink++;
        else if(sizeFlags.horizontal==SizeFlag::Expand)
            expand++;
        else if(sizeFlags.horizontal==SizeFlag::Elastic)
        {
            shrink++;
            expand++;
        }
        else if(sizeFlags.horizontal==SizeFlag::Bloat)
            bloat++;
    }

    int extraSpace=m_bounds.w-m_preferredSize.width-m_padding.left-m_padding.right;
    int bloatSpace=0;
    int expandSpace=0;
    int shrinkSpace=0;

    if(extraSpace > 0)
    {
        if(bloat > 0)
            bloatSpace=extraSpace/bloat;
        else if(expand > 0)
            expandSpace=extraSpace/expand;
    }
    else if(extraSpace < 0)
    {
        if(shrink > 0)
            shrinkSpace=extraSpace/shrink;
    }

    for(LayoutItem *child:m_children)
    {
        Rect bounds=child->getBounds();
        Size preferred=child->getPreferredSize();
        const SizeFlags &sizeFlags=child->getSizeFlags();

        bounds.x=x;

        if(sizeFlags.horizontal==SizeFlag::Bloat)
            bounds.w=preferred.width+bloatSpace;
        else if((sizeFlags.horizontal==SizeFlag::Elastic)||(sizeFlags.horizontal==SizeFlag::Expand))
            bounds.w=preferred.width+expandSpace;
        else if((sizeFlags.horizontal==SizeFlag::Elastic)||(sizeFlags.horizontal==SizeFlag::Shrink))
            bounds.w=preferred.width+shrinkSpace;
        else if(sizeFlags.horizontal==SizeFlag::Fixed)
            bounds.w=preferred.width;

        child->setBounds(bounds);
        x+=bounds.w+m_spacing;

#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeHorizontal - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
    }
}

void LayoutItem::arrangeVerticalPos()
{
    int y=m_bounds.y+m_padding.top;

    for(LayoutItem *child:m_children)
    {
        Rect bounds=child->getBounds();
        const Size preferred=child->getPreferredSize();

        bounds.y=y;
        bounds.h=preferred.height+m_padding.top+m_padding.bottom;

        SizeFlags sizeFlags=child->getSizeFlags();

        if(preferred.height < m_bounds.h)
        {
            if((sizeFlags.vertical==SizeFlag::Elastic)||(sizeFlags.vertical==SizeFlag::Expand)||(sizeFlags.vertical==SizeFlag::Bloat))
                bounds.h=m_bounds.h-m_padding.top-m_padding.bottom;

        }
        else if(preferred.height > m_bounds.h)
        {
            if((sizeFlags.vertical==SizeFlag::Elastic)||(sizeFlags.vertical==SizeFlag::Shrink))
                bounds.h=m_bounds.h-m_padding.top-m_padding.bottom;
        }

        child->setBounds(bounds);
#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeVerticalPos - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
    }
}

void LayoutItem::arrangeHorizontalPos()
{
    int x=m_bounds.x+m_padding.left;

    for(LayoutItem *child:m_children)
    {
        Rect bounds=child->getBounds();
        const Size preferred=child->getPreferredSize();

        bounds.x=x;
        bounds.w=preferred.width+m_padding.left+m_padding.right;

        //        child->setPosition(x, bounds.y);

        SizeFlags sizeFlags=child->getSizeFlags();

        if(preferred.width < m_bounds.w)
        {
            if((sizeFlags.horizontal == SizeFlag::Elastic) || (sizeFlags.horizontal == SizeFlag::Expand) || (sizeFlags.horizontal == SizeFlag::Bloat))
                bounds.w=m_bounds.w-m_padding.left-m_padding.right;

        }
        else if(preferred.width > m_bounds.w)
        {
            if((sizeFlags.horizontal == SizeFlag::Elastic) || (sizeFlags.horizontal == SizeFlag::Shrink))
                bounds.w=m_bounds.w-m_padding.left-m_padding.right;
        }

        child->setBounds(bounds);
        //        child->updateControl();
#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeHorizontalPos - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
    }
}

void LayoutItem::arrangeVertical()
{
    int y=m_bounds.y+m_padding.top;

    int shrink=0;
    int expand=0;
    int bloat=0;

    for(LayoutItem *child:m_children)
    {
        const SizeFlags &sizeFlags=child->getSizeFlags();

        if(sizeFlags.vertical == SizeFlag::Shrink)
            shrink++;
        else if(sizeFlags.vertical == SizeFlag::Expand)
            expand++;
        else if(sizeFlags.vertical == SizeFlag::Elastic)
        {
            shrink++;
            expand++;
        }
        else if(sizeFlags.vertical == SizeFlag::Bloat)
        {
            shrink++;
            bloat++;
        }
    }

    int extraSpace=m_bounds.h-m_preferredSize.height-m_padding.top-m_padding.bottom;
    int bloatSpace=0;
    int expandSpace=0;
    int shrinkSpace=0;

    if(extraSpace > 0)
    {
        if(bloat > 0)
            bloatSpace=extraSpace/bloat;
        else if(expand > 0)
            expandSpace=extraSpace/expand;
    }
    else if(extraSpace < 0)
    {
        if(bloat > 0)
            bloatSpace=extraSpace/bloat;
        else if(shrink > 0)
            shrinkSpace=extraSpace/shrink;
    }

    for(LayoutItem *child:m_children)
    {
        Rect bounds=child->getBounds();
        Size preferred=child->getPreferredSize();
        const SizeFlags &sizeFlags=child->getSizeFlags();

        bounds.y=y;

        if(sizeFlags.vertical==SizeFlag::Bloat)
            bounds.h=preferred.height+bloatSpace;
        else if(sizeFlags.vertical == SizeFlag::Elastic)
            bounds.h=preferred.height+expandSpace+shrinkSpace;
        else if(sizeFlags.vertical == SizeFlag::Expand)
            bounds.h=preferred.height+expandSpace;
        else if(sizeFlags.vertical == SizeFlag::Shrink)
            bounds.h=preferred.height+shrinkSpace;
        else if(sizeFlags.vertical == SizeFlag::Fixed)
            bounds.h=preferred.height;

        child->setBounds(bounds);
#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeVertical - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
        y+=bounds.h+m_spacing;
    }
}

} //namespace controls
} //namespace gweni
