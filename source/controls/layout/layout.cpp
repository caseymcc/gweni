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

// One axis of a size, rect, margin or size flags, so the size and arrange
// passes are written once for both axes.
int &length(Size &size, Dim dim)
{
    return (dim == Dim::X)?size.width:size.height;
}

int length(const Size &size, Dim dim)
{
    return (dim == Dim::X)?size.width:size.height;
}

int &pos(Rect &rect, Dim dim)
{
    return (dim == Dim::X)?rect.x:rect.y;
}

int &length(Rect &rect, Dim dim)
{
    return (dim == Dim::X)?rect.w:rect.h;
}

//! the side of a margin or padding a layout starts from along dim
int lead(const Margin &margin, Dim dim)
{
    return (dim == Dim::X)?margin.left:margin.top;
}

//! both sides of a margin or padding along dim
int sides(const Margin &margin, Dim dim)
{
    return (dim == Dim::X)?margin.left+margin.right:margin.top+margin.bottom;
}

//! Bloat takes all the space there is: it grows and shrinks like Elastic,
//! and along the stacking axis before the others
bool canGrow(SizeFlag sizeFlag)
{
    return (sizeFlag == SizeFlag::Expand) || (sizeFlag == SizeFlag::Elastic) || (sizeFlag == SizeFlag::Bloat);
}

bool canShrink(SizeFlag sizeFlag)
{
    return (sizeFlag == SizeFlag::Shrink) || (sizeFlag == SizeFlag::Elastic) || (sizeFlag == SizeFlag::Bloat);
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

    // A hidden control takes no space and no spacing, as in a dock layout
    // (Base::sizeOfChildren); a nested layout always counts.
    for(LayoutItem *child:m_children)
    {
        if(!child->skipped())
            child->calculateSize(skin, dim);
    }

    if(dim == alongDim())
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
        if(child->skipped())
            continue;

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
        if(child->skipped())
            continue;

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

    if(dim == alongDim())
        arrangeAlong(dim);
    else
        arrangeAcross(dim);

    // a hidden control keeps the bounds it had, as in a dock layout
    for(LayoutItem *child:m_children)
    {
        if(!child->skipped())
            child->arrange(skin, dim);
    }
}

// Along the stacking axis the children follow each other from the lead
// padding, m_spacing apart, each at its preferred length plus its share of
// the space the layout has over its own preferred length (which holds the
// padding and the spacing). Space over goes to the Bloat children if there
// are any, otherwise to the Expand and Elastic ones; space short comes out of
// the Bloat children if there are any, otherwise out of the Shrink and Elastic
// ones. Fixed keeps its preferred length.
void LayoutItem::arrangeAlong(Dim dim)
{
    int shrink=0;
    int expand=0;
    int bloat=0;

    for(LayoutItem *child:m_children)
    {
        if(child->skipped())
            continue;

        const SizeFlag sizeFlag=flag(child->getSizeFlags(), dim);

        if(sizeFlag == SizeFlag::Bloat)
            bloat++;
        else
        {
            if(canGrow(sizeFlag))
                expand++;
            if(canShrink(sizeFlag))
                shrink++;
        }
    }

    const int extraSpace=length(m_bounds, dim)-length(m_preferredSize, dim);
    int bloatSpace=0;
    int expandSpace=0;
    int shrinkSpace=0;

    if(bloat > 0)
        bloatSpace=extraSpace/bloat;
    else if((extraSpace > 0) && (expand > 0))
        expandSpace=extraSpace/expand;
    else if((extraSpace < 0) && (shrink > 0))
        shrinkSpace=extraSpace/shrink;

    int position=pos(m_bounds, dim)+lead(m_padding, dim);

    for(LayoutItem *child:m_children)
    {
        if(child->skipped())
            continue;

        Rect bounds=child->getBounds();
        const SizeFlag sizeFlag=flag(child->getSizeFlags(), dim);
        int childLength=length(child->getPreferredSize(), dim);

        if(sizeFlag == SizeFlag::Bloat)
            childLength+=bloatSpace;
        else
        {
            if(canGrow(sizeFlag))
                childLength+=expandSpace;
            if(canShrink(sizeFlag))
                childLength+=shrinkSpace;
        }

        pos(bounds, dim)=position;
        length(bounds, dim)=childLength;
        child->setBounds(bounds);
        position+=childLength+m_spacing;

#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeAlong %s - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), (dim == Dim::X)?"X":"Y", control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
    }
}

// Across the stacking axis every child starts at the lead padding and gets
// its preferred length, or the whole length inside the padding when it is
// shorter and the child can grow, or longer and the child can shrink.
void LayoutItem::arrangeAcross(Dim dim)
{
    const int position=pos(m_bounds, dim)+lead(m_padding, dim);
    const int inner=length(m_bounds, dim)-sides(m_padding, dim);

    for(LayoutItem *child:m_children)
    {
        if(child->skipped())
            continue;

        Rect bounds=child->getBounds();
        const SizeFlag sizeFlag=flag(child->getSizeFlags(), dim);
        const int preferred=length(child->getPreferredSize(), dim);
        int childLength=preferred;

        if(((preferred < inner) && canGrow(sizeFlag)) || ((preferred > inner) && canShrink(sizeFlag)))
            childLength=inner;

        pos(bounds, dim)=position;
        length(bounds, dim)=childLength;
        child->setBounds(bounds);

#ifdef DEBUG_LAYOUT
        Base *control=child->getControl();

        if(control)
            logUpdates("%s%s - layout arrangeAcross %s - (%d, %d, %d, %d)\n", spaceByParent(control).c_str(), control->getTypeName(), (dim == Dim::X)?"X":"Y", control->getX(), control->getY(), control->getWidth(), control->getHeight());
#endif//DEBUG_LAYOUT
    }
}

} //namespace controls
} //namespace gweni
