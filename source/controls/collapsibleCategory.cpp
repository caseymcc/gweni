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

void CategoryButton::updateColors()
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



class CategoryHeaderButton: public Button
{
    GWENI_CONTROL_INLINE(CategoryHeaderButton, Button)
    {
        setShouldDrawBackground(false);
        setIsToggle(true);
        setAlignment(Alignment::Center);
    }

public:
    void updateColors() override
    {
        if(isDepressed() || getToggleState())
            return setTextColor(getSkin()->Colors.Category.Header_Closed);

        return setTextColor(getSkin()->Colors.Category.Header);
    }
};

GWENI_CONTROL_CONSTRUCTOR(CategoryInner)
{
    setPadding(Padding(1, 0, 1, 5));
}

CategoryButton *CategoryInner::add(const String &name)
{
//    CategoryButton *button=new CategoryButton(this);
    CategoryButton *button=newChild<CategoryButton>(name);

    button->setText(name);
    button->setDock(DockPosition::Top);
    button->sizeToContents();
    button->setSize(button->getWidth()+4, button->getHeight()+4);
    button->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    button->setPadding({5, 2, 2, 2});
//    button->onPressCaller.add(this, &ThisClass::onSelection);
    return button;
}

void CategoryInner::remove(const String &name)
{
    Base::List &children=getChildren();

    for(auto &&control:children)
    {
        CategoryButton *child=gweni_cast<CategoryButton>(control);

        if(!child)
            continue;

        if(child->getName() == name)
        {
            removeChild(child);
            break;
        }
    }
}

void CategoryInner::remove(CategoryButton *category)
{
    removeChild(category);
}

void CategoryInner::unselectAll()
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

CategoryButton *CategoryInner::getSelected()
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

void CategoryInner::postLayout(skin::Base *skin)
{
    Base::List &children=getChildren();
    bool b=true;

    for(auto &&control : children)
    {
        CategoryButton *child=gweni_cast<CategoryButton>(control);

        if(!child)
            continue;

        child->setAlt(b);
        child->updateColors();
        b=!b;
    }
}

GWENI_CONTROL_CONSTRUCTOR(CollapsibleCategory)
{
    m_list=nullptr;
//    m_button=new CategoryHeaderButton(this);
    m_button=newChild<CategoryHeaderButton>();

    m_button->setText("Category Title");
    m_button->setDock(DockPosition::Top);
    m_button->setHeight(20);
    m_button->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    m_button->setToggleState(true);
//    setPadding(Padding(1, 0, 1, 5));
    m_button->onToggleCaller.add(this, &ThisClass::onToggleHeaderButton);

    m_inner=newChild<CategoryInner>();
    m_inner->setDock(DockPosition::Bottom);

    setSize(512, 512);
}

CategoryButton *CollapsibleCategory::add(const String &name)
{
//    CategoryButton *button=new CategoryButton(this);
//    CategoryButton *button=newChild<CategoryButton>(name);
//
//    button->setText(name);
//    button->setDock(DockPosition::Top);
//    button->sizeToContents();
//    button->setSize(button->getWidth()+4, button->getHeight()+4);
//    button->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
//    button->setPadding({5, 2, 2, 2});
//    button->onPressCaller.add(this, &ThisClass::onSelection);
    CategoryButton *category=m_inner->add(name);

    category->onPressCaller.add(this, &ThisClass::onSelection);
    return category;
}

void CollapsibleCategory::remove(const String &name)
{
    m_inner->remove(name);
}

void CollapsibleCategory::remove(CategoryButton *category)
{
    m_inner->remove(category);
}

void CollapsibleCategory::onToggleHeaderButton(event::Info info)
{
    CategoryHeaderButton *child=gweni_cast<CategoryHeaderButton>(info.controlCaller);

    if(!child)
        return;

    bool hidden=!child->getToggleState();
    Base::List &children=getChildren();

    m_inner->setHidden(hidden);
//    for(auto &&control:children)
//    {
//        CategoryButton *child=gweni_cast<CategoryButton>(control);
//
//        if(!child)
//            continue;
//
//        child->setHidden(hidden);
//    }
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

void CollapsibleCategory::setText(const String &text)
{
    m_button->setText(text);
}

void CollapsibleCategory::unselectAll()
{
//    Base::List &children=getChildren();
//
//    for(auto &&control : children)
//    {
//        CategoryButton *child=gweni_cast<CategoryButton>(control);
//
//        if(!child)
//            continue;
//
//        child->setToggleState(false);
//    }
    m_inner->unselectAll();
}

void CollapsibleCategory::calculateSize(skin::Base *skin, Dim dim)
{
    if(processLayout(skin, dim))
        return;

    if(!m_button->getToggleState())
    {
        m_button->calculateSize(skin, dim);
        m_preferredSize=m_button->getPreferredSize();
    }
    else
        m_preferredSize=sizeOfChildren(skin, dim);
}

void CollapsibleCategory::postLayout(skin::Base * /*skin*/)
{
    if(!m_button->getToggleState())
        setHeight(m_button->getHeight());
    else
        sizeToChildren(false, true);

//    Base::List &children=getChildren();
//    bool b=true;
//
//    for(auto &&control : children)
//    {
//        CategoryButton *child=gweni_cast<CategoryButton>(control);
//
//        if(!child)
//            continue;
//
//        child->setAlt(b);
//        child->updateColors();
//        b=!b;
//    }
}

Button *CollapsibleCategory::getSelected()
{
//    Base::List &children=getChildren();
//
//    for(auto &&control : children)
//    {
//        CategoryButton *child=gweni_cast<CategoryButton>(control);
//
//        if(!child)
//            continue;
//
//        if(child->getToggleState())
//            return child;
//    }
//
//    return nullptr;
    return m_inner->getSelected();
}


void CollapsibleCategory::setExpanded(bool expanded)
{
    m_button->setToggleState(expanded);
}

bool CollapsibleCategory::isExpanded() const
{
    return m_button->getToggleState();
}

}//namespace controls
}//namespace gweni
