/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_collapsiblelist_h_
#define _gweni_controls_collapsiblelist_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>
#include <gweni/controls/collapsibleCategory.h>


namespace gweni
{
namespace controls
{

class GWENI_EXPORT CollapsibleList: public ScrollControl
{
public:

    event::Caller onSelectionCaller;

    GWENI_CONTROL_INLINE(CollapsibleList, ScrollControl)
    {
        setScroll(false, true);
        setAutoHideBars(true);
    }

public:
    virtual void add(CollapsibleCategory *category)
    {
        category->setParent(this);
        category->setDock(DockPosition::Top);
        category->setMargin(Margin(1, 0, 1, 1));
        category->setList(this);
        category->onSelectionCaller.add(this, &ThisClass::onSelectionEvent);
    }

    virtual CollapsibleCategory *add(const String &name)
    {
        CollapsibleCategory *category=newChild<CollapsibleCategory>();

        category->setText(name);
        category->setName(name);
        add(category);
        return category;
    }

    virtual void unselectAll()
    {
        Base::List &children=getChildren();

        for(auto &&control : children)
        {
            CollapsibleCategory *child=
                gweni_cast<CollapsibleCategory>(control);

            if(!child)
                continue;

            child->unselectAll();
        }
    }

    virtual Button *getSelected()
    {
        Base::List &children=getChildren();

        for(auto &&control : children)
        {
            CollapsibleCategory *child=
                gweni_cast<CollapsibleCategory>(control);

            if(!child)
                continue;

            Button *selected=child->getSelected();

            if(selected)
                return selected;
        }

        return nullptr;
    }

protected:

    virtual void onSelection(CollapsibleCategory *control, Button *selected)
    {
        onSelectionCaller.call(this);
    }

    void onSelectionEvent(event::Info info)
    {
        auto child=gweni_cast<CollapsibleCategory>(info.controlCaller);

        if(child)
        {
            onSelection(child, child->getSelected());
        }
    }

};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_collapsiblelist_h_
