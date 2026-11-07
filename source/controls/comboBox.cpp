/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/comboBox.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/canvas.h>

namespace gweni
{
namespace controls
{

//using namespace gweni::internal;

class GWENI_EXPORT DownArrow: public controls::Base
{
public:

    GWENI_CONTROL_INLINE(DownArrow, controls::Base)
    {
        setMouseInputEnabled(false);
        setSize(15, 15);
    }

public:
    void setComboBox(ComboBox *p)
    {
        m_comboBox=p;
    }

protected:

    ComboBox *m_comboBox;
};


GWENI_CONTROL_CONSTRUCTOR(ComboBox)
{
    setSize(100, 20);

//    m_menu=new Menu(this);
    m_menu=newChild<Menu>();

    m_menu->setHidden(true);
    m_menu->setDisableIconMargin(true);
    m_menu->setTabable(false);

//    DownArrow *arrow=new DownArrow(this);
    DownArrow *arrow=newChild<DownArrow>();

    arrow->setComboBox(this);
    // Set once here rather than in layout(), where they would change what the
    // pass had already measured.
    arrow->setAlignment(Alignment::Right | Alignment::CenterV);
    arrow->setPosition(Position::Absolute, 4, 0);
    arrow->setPos(4, 0);

    m_button=arrow;
    m_selectedItem=nullptr;

    // the selected item reads from the left, clear of the arrow
    setTextAlignment(Alignment::Left|Alignment::CenterV);
    // Inset from the left edge as the chevron is from the right.
    setTextPadding(Padding(ChevronInset, 0, 3, 0));
    setText("");
    setMargin(Margin(3, 0, 0, 0));
    setPadding(Padding(0, VerticalPadding, ChevronSize+ChevronInset, VerticalPadding));
    setTabable(true);
    setKeyboardInputEnabled(true);
}

MenuItem *ComboBox::addItem(const String &strLabel, const String &strName)
{
    MenuItem *item=m_menu->addItem(strLabel, "");
    item->setName(strName);
    item->onMenuItemSelected.add(this, &ComboBox::onItemSelected);

    // Default. Not through onItemSelected, which takes the item from the
    // event's caller, and there is none here.
    if(m_selectedItem == nullptr)
        selectItem(item, false);

    return item;
}

void ComboBox::render(skin::Base *skin)
{
    if(!shouldDrawBackground())
        return;

    if(getStateChange() == StateChange_Nothing || !m_skinControl)
        return;

    // Allocate primitives on first render, mirroring Base::render, otherwise the
    // skin renderer's update() draws with an empty primitive buffer.
    if(getStateChange() & StateChange_Created)
        m_skinControl->generate(skin->getRenderer(), this);

    m_skinControl->update(skin->getRenderer(), this);
}

void ComboBox::layout(skin::Base *skin)
{
    ParentClass::layout(skin);
}

// The text and the padding, the chevron's column included, from the Text
// child: the hidden menu is not counted (sizeOfChildren skips hidden
// children) and DownArrow has no preferred size. An empty string measures
// the font's size high and a set one a line high, so the height is at least
// a line: the box does not change height when an item is selected.
void ComboBox::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
    {
        const int line=std::max(m_text->getPreferredSize().height,
            getSkin()->getRenderer()->measureText(getFont(), " ").y);

        m_preferredSize.height=preferredFromContent(line+m_padding.top+m_padding.bottom, dim);
    }
}

void ComboBox::updateColors()
{
    if(!shouldDrawBackground())
        return setTextColor(getSkin()->Colors.Button.Normal);

    ParentClass::updateColors();
}

void ComboBox::onPress(event::Info)
{
    if(isMenuOpen())
        return getCanvas()->closeMenus();

    const bool wasMenuHidden=m_menu->hidden();
    getCanvas()->closeMenus();

    if(wasMenuHidden)
        openList();
}

void ComboBox::clearItems()
{
    if(m_menu)
        m_menu->clearItems();
}

void ComboBox::selectItem(MenuItem *item, bool fireChangeEvents)
{
    if(m_selectedItem == item)
        return;

    m_selectedItem=item;
    setText(m_selectedItem->getText());
    m_menu->setHidden(true);
    invalidate();

    if(fireChangeEvents)
    {
        onSelection.call(this);
        focus();
    }
}

void ComboBox::onItemSelected(event::Info info)
{
    // Convert selected to a menu item
    MenuItem *item=gweni_cast<MenuItem>(info.controlCaller);

    if(item)
    {
        selectItem(item);
    }
}

void ComboBox::selectItemByName(const gweni::String &name, bool fireChangeEvents)
{
    for(auto &&control : m_menu->getChildren())
    {
        MenuItem *child=gweni_cast<MenuItem>(control);

        if(child->getName() == name)
            return selectItem(child, fireChangeEvents);
    }
}

void ComboBox::onLostKeyboardFocus()
{
    setTextColor(Color(0, 0, 0, 255));
}

void ComboBox::onKeyboardFocus()
{
    // Until we add the blue highlighting again
    setTextColor(Color(0, 0, 0, 255));
    // m_selectedText->SetTextColor( Color( 255, 255, 255, 255 ) );
}

gweni::controls::Label *ComboBox::getSelectedItem()
{
    return m_selectedItem;
}

bool ComboBox::isMenuOpen()
{
    return m_menu && !m_menu->hidden();
}

void ComboBox::openList()
{
    if(!m_menu)
        return;

    m_menu->setParent(getCanvas());
    m_menu->setHidden(false);
    m_menu->bringToFront();
    gweni::Point p=localPosToCanvas(gweni::Point(0, 0));
    m_menu->setBounds(gweni::Rect(p.x, p.y+getHeight(), getWidth(), m_menu->getHeight()));
}

void ComboBox::closeList()
{
    if(!m_menu)
        return;

    m_menu->hide();
}

bool ComboBox::onKeyUp(bool down)
{
    if(down)
    {
        Base::List &children=m_menu->getChildren();
        Base::List::reverse_iterator it=std::find(children.rbegin(),
            children.rend(), m_selectedItem);

        if(it != children.rend() && (++it != children.rend()))
        {
            if(MenuItem *upElement=gweni_cast<MenuItem>(*it))
                selectItem(upElement);
        }
    }

    return true;
}

bool ComboBox::onKeyDown(bool down)
{
    if(down)
    {
        Base::List &children=m_menu->getChildren();
        Base::List::iterator it=std::find(children.begin(), children.end(), m_selectedItem);

        if(it != children.end() && (++it != children.end()))
        {
            if(MenuItem *downElement=gweni_cast<MenuItem>(*it))
                selectItem(downElement);
        }
    }

    return true;
}

void ComboBox::renderFocus(gweni::skin::Base * /*skin*/)
{}

}//namespace controls
}//namespace gweni
