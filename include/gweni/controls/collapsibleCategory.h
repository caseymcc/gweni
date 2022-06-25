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

class CategoryButton: public Button
{
    GWENI_CONTROL_INLINE(CategoryButton, Button)
    {
//        enlargePrimitiveIds(this, m_primitiveIds, 1);

        setAlignment(Alignment::Left | Alignment::CenterV);
        m_alt=false;
    }

public:
    void updateColors() override;

    bool getAlt() { return m_alt; }
    void setAlt(bool value) { m_alt=value; }

private:
    bool m_alt;
};

class CollapsibleList;

class CategoryInner: public Base
{
public:
    GWENI_CONTROL(CategoryInner, Base);

public:
    CategoryButton *add(const String &name);
    void remove(const String &name);
    void remove(CategoryButton *category);

    CategoryButton *getSelected();
    void unselectAll();

    void postLayout(skin::Base *skin) override;
    
private:

};

class GWENI_EXPORT CollapsibleCategory: public Base
{
public:

    GWENI_CONTROL(CollapsibleCategory, controls::Base);

public:
    //void render(skin::Base *skin) override;

    virtual void setText(const String &text);

    virtual CategoryButton *add(const String &name);
    virtual void remove(const String &name);
    virtual void remove(CategoryButton *category);

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

    void onToggleHeaderButton(event::Info info);
    virtual void onSelection(event::Info info);

    Button *m_button;
    CategoryInner *m_inner;
    CollapsibleList *m_list;
};

}//namespace controls
}//namespace gweni

#endif//_gweni_controls_collapsiblecategory_h_
