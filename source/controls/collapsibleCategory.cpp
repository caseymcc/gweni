/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/controls/collapsibleCategory.h>
#include <gweni/controls/collapsibleList.h>


namespace gweni
{
namespace controls
{

class CategoryButton: public Button
{
    GWENI_CONTROL_INLINE(CategoryButton, Button)
    {
        enlargePrimitiveIds(this, m_primitiveIds, 1);

        setAlignment(Position::Left | Position::CenterV);
        m_alt=false;
    }

    void render(skin::Base *skin) override
    {
        if(m_alt)
        {
            if(isDepressed() || getToggleState())
                skin->getRender()->setDrawColor(skin->Colors.Category.LineAlt.Button_Selected);
            else if(isHovered())
                skin->getRender()->setDrawColor(skin->Colors.Category.LineAlt.Button_Hover);
            else
                skin->getRender()->setDrawColor(skin->Colors.Category.LineAlt.Button);
        }
        else
        {
            if(isDepressed() || getToggleState())
                skin->getRender()->setDrawColor(skin->Colors.Category.Line.Button_Selected);
            else if(isHovered())
                skin->getRender()->setDrawColor(skin->Colors.Category.Line.Button_Hover);
            else
                skin->getRender()->setDrawColor(skin->Colors.Category.Line.Button);
        }

        skin->getRender()->drawFilledRect(m_primitiveIds[0], this->getRenderBounds());
    }

    void updateColors() override
    {
        if(m_alt)
        {
            if(isDepressed() || getToggleState())
                return setTextColor(getSkin()->Colors.Category.LineAlt.Text_Selected);

            if(isHovered())
                return setTextColor(getSkin()->Colors.Category.LineAlt.Text_Hover);

            return setTextColor(getSkin()->Colors.Category.LineAlt.Text);
        }

        if(isDepressed() || getToggleState())
            return setTextColor(getSkin()->Colors.Category.Line.Text_Selected);

        if(isHovered())
            return setTextColor(getSkin()->Colors.Category.Line.Text_Hover);

        return setTextColor(getSkin()->Colors.Category.Line.Text);
    }

    bool m_alt;
};


class CategoryHeaderButton: public Button
{
    GWENI_CONTROL_INLINE(CategoryHeaderButton, Button)
    {
        setShouldDrawBackground(false);
        setIsToggle(true);
        setAlignment(Position::Center);
    }

    void updateColors() override
    {
        if(isDepressed() || getToggleState())
            return setTextColor(getSkin()->Colors.Category.Header_Closed);

        return setTextColor(getSkin()->Colors.Category.Header);
    }

};


GWENI_CONTROL_CONSTRUCTOR(CollapsibleCategory)
{
    m_list=nullptr;
    m_button=new CategoryHeaderButton(this);
    m_button->setText("Category Title");
    m_button->dock(Position::Top);
    m_button->setHeight(20);
    m_button->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    setPadding(Padding(1, 0, 1, 5));
    setSize(512, 512);
}

Button *CollapsibleCategory::add(const String &name)
{
    CategoryButton *button=new CategoryButton(this);
    button->setText(name);
    button->dock(Position::Top);
    button->sizeToContents();
    button->setSize(button->getWidth()+4, button->getHeight()+4);
    button->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    button->setPadding(Padding(5, 2, 2, 2));
    button->onPressCaller.add(this, &ThisClass::onSelection);
    return button;
}

void CollapsibleCategory::onSelection(event::Info info)
{
    CategoryButton *child=gweni_cast<CategoryButton>(info.controlCaller);

    if(!child)
        return;

    if(m_list)
        m_list->unselectAll();
    else
        unselectAll();

    child->setToggleState(true);
    onSelectionCaller.call(this);
}

void CollapsibleCategory::render(skin::Base *skin)
{
    //skin->drawCategoryInner(this, skin::Generate, m_button->getToggleState());
    skin->drawControl(this);
}

void CollapsibleCategory::setText(const String &text)
{
    m_button->setText(text);
}

void CollapsibleCategory::unselectAll()
{
    Base::List &children=getChildren();

    for(auto &&control : children)
    {
        CategoryButton *child=gweni_cast<CategoryButton>(control);

        if(!child)
            continue;

        child->setToggleState(false);
    }
}

void CollapsibleCategory::calculateSize(skin::Base *skin, Dim dim)
{
    if(processLayout(skin, dim))
        return;

    if(m_button->getToggleState())
    {
        m_button->calculateSize(skin, dim);
        m_preferredSize=m_button->getPreferredSize();
    }
    else
        m_preferredSize=sizeOfChildren(skin, dim);
}

void CollapsibleCategory::postLayout(skin::Base * /*skin*/)
{
    if(m_button->getToggleState())
        setHeight(m_button->getHeight());
    else
        sizeToChildren(false, true);

    Base::List &children=getChildren();
    bool b=true;

    for(auto &&control : children)
    {
        CategoryButton *child=gweni_cast<CategoryButton>(control);

        if(!child)
            continue;

        child->m_alt=b;
        child->updateColors();
        b=!b;
    }
}

Button *CollapsibleCategory::getSelected()
{
    Base::List &children=getChildren();

    for(auto &&control : children)
    {
        CategoryButton *child=gweni_cast<CategoryButton>(control);

        if(!child)
            continue;

        if(child->getToggleState())
            return child;
    }

    return nullptr;
}


void CollapsibleCategory::setExpanded(bool expanded)
{
    m_button->setToggleState(!expanded);
}

bool CollapsibleCategory::isExpanded() const
{
    return !m_button->getToggleState();
}

}//namespace controls
}//namespace gweni
