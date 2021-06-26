#include <gweni/controls/layout/layout.h>

namespace gweni
{
namespace controls
{


LayoutItem::LayoutItem():
    m_control(nullptr),
    m_parent(nullptr),
    m_layoutParent(nullptr),
    m_sizeFlags(SizeFlag::Fixed, SizeFlag::Fixed),
    m_margin({0, 0, 0, 0}),
    m_padding({0, 0, 0, 0}),
    m_spacing(2)
{

}

LayoutItem::LayoutItem(LayoutType type):
    m_type(type),
    m_parent(nullptr),
    m_layoutParent(nullptr),
    m_control(nullptr),
    m_sizeFlags(SizeFlag::Fixed, SizeFlag::Fixed),
    m_margin({0, 0, 0, 0}),
    m_padding({0, 0, 0, 0}),
    m_spacing(2)
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

void LayoutItem::calculateSize(skin::Base *skin, Dim dim)
{
    if(m_control)
    {
        m_control->calculateSize(skin, dim);

        const Size &preferred=m_control->getPreferredSize();
        const Rect &bounds=m_control->getBounds();
        const Margin &margin=m_control->getMargin();
        const SizeFlags &sizeFlags=m_control->getSizeFlags();

        if(dim==Dim::X)
        {
            m_minimumSize.width=m_control->getMinimumSize().width;
            m_maximumSize.width=m_control->getMaximumSize().width;
            m_preferredSize.width=preferred.width+margin.left+margin.right;
        }
        else
        {
            m_minimumSize.height=m_control->getMinimumSize().height;
            m_maximumSize.height=m_control->getMaximumSize().height;
            m_preferredSize.height=preferred.height+margin.top+margin.bottom;
        }

        m_sizeFlags=m_control->getSizeFlags();
        return;
    }

    for(LayoutItem *child:m_children)
        child->calculateSize(skin, dim);

    switch(m_type)
    {
    case LayoutType::Horizontal:
        if(dim == Dim::X)
            calculateHorizontal();
        else
            calculateMaxHeight();
        break;
    case LayoutType::Vertical:
        if(dim == Dim::X)
            calculateMaxWidth();
        else
            calculateVertical();
        break;
    }

}

void LayoutItem::calculateHorizontal()
{
    //    m_bounds.w=0;
    m_preferredSize.width=0;
    m_minimumSize.width=0;
    m_maximumSize.width=std::numeric_limits<int>::max();

    bool addSpacing=false;

    for(LayoutItem *child:m_children)
    {
        const gweni::Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        m_preferredSize.width+=child->getPreferredSize().width;
        m_minimumSize.width=addWithOverflow(m_maximumSize.width, child->getMinimumSize().width);
        m_maximumSize.width=addWithOverflow(m_maximumSize.width, child->getMaximumSize().width);
        m_sizeFlags.horizontal=merge(m_sizeFlags.horizontal, sizeFlags.horizontal);

        if(addSpacing)
            m_preferredSize.width+=m_spacing;
        //            m_bounds.w+=m_spacing;
        else
            addSpacing=true;

        //        m_bounds.w+=bounds.w;
    }

    m_preferredSize.width+=m_padding.left+m_padding.right;
    //    m_bounds.w+=m_padding.left+m_padding.right;
}

void LayoutItem::calculateMaxHeight()
{
    //    m_bounds.h=0;
    m_preferredSize.height=0;
    m_minimumSize.height=0;
    m_maximumSize.height=std::numeric_limits<int>::max();

    for(LayoutItem *child:m_children)
    {
        const gweni::Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        m_preferredSize.height=std::max(m_preferredSize.height, child->getPreferredSize().height);
        m_minimumSize.height=std::max(m_minimumSize.height, child->getMinimumSize().height);
        m_maximumSize.height=std::min(m_maximumSize.height, child->getMaximumSize().height);
        m_sizeFlags.vertical=merge(m_sizeFlags.vertical, sizeFlags.vertical);

        //        if(bounds.h>m_bounds.h)
        //            m_bounds.h=bounds.h;
    }

    m_preferredSize.height+=m_padding.top+m_padding.bottom;
    //    m_bounds.h+=m_padding.top+m_padding.bottom;
}

void LayoutItem::calculateMaxWidth()
{
    //    m_bounds.w=0;
    m_minimumSize.width=0;
    m_maximumSize.width=std::numeric_limits<int>::max();

    for(LayoutItem *child:m_children)
    {
        const gweni::Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        m_preferredSize.width=std::max(m_preferredSize.width, child->getPreferredSize().width);
        m_minimumSize.width=std::max(m_minimumSize.width, child->getMinimumSize().width);
        m_maximumSize.width=std::min(m_maximumSize.width, child->getMaximumSize().width);

        m_sizeFlags.horizontal=merge(m_sizeFlags.horizontal, sizeFlags.horizontal);

        //        if(bounds.w>m_bounds.w)
        //            m_bounds.w=bounds.w;
    }
    m_preferredSize.width+=m_padding.left+m_padding.right;
    //    m_bounds.w+=m_padding.left+m_padding.right;

}

void LayoutItem::calculateVertical()
{
    //    m_bounds.h=0;
    m_preferredSize.height=0;
    m_minimumSize.height=0;
    m_maximumSize.height=std::numeric_limits<int>::max();

    bool addSpacing=false;
    for(LayoutItem *child:m_children)
    {
        const gweni::Rect &bounds=child->getBounds();
        SizeFlags sizeFlags=child->getSizeFlags();

        m_preferredSize.height+=child->getPreferredSize().height;
        m_minimumSize.height=addWithOverflow(m_minimumSize.height, child->getMinimumSize().height);
        m_maximumSize.height=addWithOverflow(m_maximumSize.height, child->getMaximumSize().height);
        m_sizeFlags.vertical=merge(m_sizeFlags.vertical, sizeFlags.vertical);

        if(addSpacing)
            m_preferredSize.height+=m_spacing;
        //            m_bounds.h+=m_spacing;
        else
            addSpacing=true;

        //        m_bounds.h+=bounds.h;
    }

    m_preferredSize.height+=m_padding.top+m_padding.bottom;
    //    m_bounds.h+=m_padding.top+m_padding.bottom;
}

void LayoutItem::updateControlSize(Dim dim)
{
    SizeFlags sizeFlags=m_control->getSizeFlags();
    Margin margin=m_control->getMargin();
    Rect bounds=m_control->getBounds();

    if(dim == Dim::X)
    {
        bounds.x=m_bounds.x+margin.left;
        if(bounds.w>m_bounds.w)
        {
            if((sizeFlags.horizontal == SizeFlag::Elastic)||
                (sizeFlags.horizontal == SizeFlag::Shrink)||
                (sizeFlags.horizontal==SizeFlag::Bloat))
                bounds.w=m_bounds.w;
        }
        else if(bounds.w<m_bounds.w)
        {
            if((sizeFlags.horizontal == SizeFlag::Elastic)||
                (sizeFlags.horizontal == SizeFlag::Expand)||
                (sizeFlags.horizontal == SizeFlag::Bloat))
                bounds.w=m_bounds.w;
        }
    }
    else
    {
        bounds.y=m_bounds.y+margin.top;
        if(bounds.h>m_bounds.h)
        {
            if((sizeFlags.vertical == SizeFlag::Elastic)||
                (sizeFlags.vertical == SizeFlag::Shrink) ||
                (sizeFlags.vertical==SizeFlag::Bloat))
                bounds.h=m_bounds.h;
        }
        else if(bounds.h<m_bounds.h)
        {
            if((sizeFlags.vertical == SizeFlag::Elastic)||
                (sizeFlags.vertical == SizeFlag::Expand)||
                (sizeFlags.vertical == SizeFlag::Bloat))
                bounds.h=m_bounds.h;
        }
    }

    m_control->setBounds(bounds);
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

        child->setBounds(bounds);
        x+=bounds.w+m_spacing;
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

        child->setBounds(bounds);
        y+=bounds.h+m_spacing;
    }
}

} //namespace controls
} //namespace gweni
