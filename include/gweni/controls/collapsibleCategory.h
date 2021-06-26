/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_collapsiblecategory_h_
#define _gweni_controls_collapsiblecategory_h_

#include <gweni/controls/base.h>
#include <gweni/controls/button.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>


namespace gweni
{
namespace controls
{

class CollapsibleList;

class GWENI_EXPORT CollapsibleCategory: public controls::Base
{
public:

    GWENI_CONTROL(CollapsibleCategory, controls::Base);

    void render(skin::Base *skin) override;

    virtual void setText(const String &text);

    virtual Button *add(const String &name);

    void postLayout(skin::Base * /*skin*/) override;

    virtual void setList(controls::CollapsibleList *p)
    {
        m_list=p;
    }

    virtual void unselectAll();
    virtual Button *getSelected();

    /// Set the expanded state of the category.
    /// @param expanded : New state.
    void setExpanded(bool expanded);
    /// Get the current expanded state of the category.
    /// @return Is expanded?
    bool isExpanded() const;

public:

    event::Caller onSelectionCaller;

protected:
    void calculateSize(skin::Base *skin, Dim dim) override;

    virtual void onSelection(event::Info info);

    controls::Button *m_button;
    controls::CollapsibleList *m_list;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_collapsiblecategory_h_
