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
    //! The ComboBox renderer paints the chevron itself.
    GWENI_DRAWS_NOTHING

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

    makeList();

//    DownArrow *arrow=new DownArrow(this);
    DownArrow *arrow=newChild<DownArrow>();

    arrow->setComboBox(this);
    // Set once here rather than in layout(), where it would change what the
    // pass had already measured.
    arrow->setAlignment(Alignment::Right | Alignment::CenterV);

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

void ComboBox::makeList()
{
    m_menu=newChild<internal::ComboBoxList>();
    m_menu->setComboBox(this);

    m_menu->setHidden(true);
    m_menu->setDisableIconMargin(true);
    m_menu->setTabable(false);
}

MenuItem *ComboBox::addItem(const String &strLabel, const String &strName)
{
    // the list was deleted from outside: a new one, as the box had at first
    if(!m_menu)
        makeList();

    MenuItem *item=m_menu->addItem(strLabel, "");
    item->setName(strName);
    item->onMenuItemSelected.add(this, &ComboBox::onItemSelected);
    // a new text to count in this box's width
    invalidate();

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

void ComboBox::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::X)
        return;

    const int items=itemsTextWidth();

    if(m_itemTexts.empty())
        return;

    const Padding &textPadding=getTextPadding();

    m_preferredSize.width=std::max(m_preferredSize.width, preferredFromContent(
        items+textPadding.left+textPadding.right+m_padding.left+m_padding.right, Dim::X));
}

int ComboBox::itemsTextWidth()
{
    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;
    const unsigned serial=renderer?renderer->textLayoutSerial():0;
    const gweni::Font &font=getFont();
    const bool sameLayout=font == m_itemsFont && renderer == m_itemsRenderer && serial == m_itemsSerial;

    if(!m_menu)
    {
        m_itemTexts.clear();
        m_itemWidths.clear();
        return 0;
    }

    Base::List &children=m_menu->getChildren();
    size_t count=0;
    int widest=0;

    for(auto &&child : children)
    {
        MenuItem *item=gweni_cast<MenuItem>(child);

        if(!item)
            continue;

        const String &text=item->getText();

        if(count == m_itemTexts.size())
        {
            m_itemTexts.push_back(text);
            m_itemWidths.push_back(measureText(font, text).x);
        }
        else if(!sameLayout || m_itemTexts[count] != text)
        {
            m_itemTexts[count]=text;
            m_itemWidths[count]=measureText(font, text).x;
        }

        widest=std::max(widest, m_itemWidths[count]);
        ++count;
    }

    m_itemTexts.resize(count);
    m_itemWidths.resize(count);
    m_itemsFont=font;
    m_itemsRenderer=renderer;
    m_itemsSerial=serial;
    return widest;
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

    // no list (deleted from outside): the press still closes the open menus
    const bool wasMenuHidden=m_menu && m_menu->hidden();
    getCanvas()->closeMenus();

    if(wasMenuHidden)
        openList();
}

ComboBox::~ComboBox()
{
    internal::ComboBoxList *list=m_menu;

    if(!list)
        return;

    // A list waiting for its delete must not reach this box.
    m_menu=nullptr;
    list->setComboBox(nullptr);

    // Never opened, it is still this box's child: ~Base deletes it.
    if(list->getParent() == this)
        return;

    // Opened, it is the canvas's child.
    Canvas *canvas=list->getCanvas();

    // A canvas tearing down deletes it with its other children, and deleting
    // it here would take a sibling out of the list that canvas is walking.
    if(canvas && canvas->releasingChildren())
        return;

    if(!list->hidden())
        list->hide();

    // Delayed, as a context menu or a popup is: this may run from one of its
    // own items' events (an onSelection handler deleting the box).
    if(canvas)
        list->delayedDelete();
    else
        delete list;
}

void ComboBox::itemRemoved(Base *child)
{
    // a pointer compare: the item may be inside its ~Base
    if(child == m_selectedItem)
    {
        m_selectedItem=nullptr;
        setText("");
    }

    // the item leaves this box's width, whether or not the list is its child
    invalidate();
}

void ComboBox::listDeleted()
{
    // Its items go with it, past onChildRemoved (the list is in its ~Base
    // then).
    m_menu=nullptr;
    m_selectedItem=nullptr;

    // A canvas tearing down deletes this box next: nothing to show.
    Canvas *canvas=getCanvas();

    if(canvas && canvas->releasingChildren())
        return;

    setText("");
    invalidate();
}

void ComboBox::clearItems()
{
    if(m_menu)
        m_menu->clearItems();

    // Nothing is selected: the items are deleted before the next frame, and
    // the next one added is the default again.
    m_selectedItem=nullptr;
    setText("");

    // the items leave this box's width once deleted, before the next layout
    invalidate();
}

void ComboBox::selectItem(MenuItem *item, bool fireChangeEvents)
{
    // Only an item of this box's own list: one that left it (given another
    // parent, still connected to this box) or another box's would never be
    // dropped by itemRemoved() and would dangle once deleted.
    if(!item || !m_menu || (item->getParent() != m_menu))
        return;

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
    if(!m_menu)
        return;

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
    // Its position and width, never a height: a height asked for would cap
    // the list (ScrollControl), which is as high as its items.
    m_menu->setPos(p.x, p.y+getHeight());
    m_menu->setWidth(getWidth());
}

void ComboBox::closeList()
{
    if(!m_menu)
        return;

    m_menu->hide();
}

bool ComboBox::onKeyUp(bool down)
{
    if(down && m_menu)
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
    if(down && m_menu)
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

namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(ComboBoxList)
{
}

ComboBoxList::~ComboBoxList()
{
    if(m_comboBox)
        m_comboBox->listDeleted();
}

void ComboBoxList::onChildRemoved(controls::Base *child)
{
    ParentClass::onChildRemoved(child);

    if(m_comboBox)
        m_comboBox->itemRemoved(child);
}

}//namespace internal

}//namespace controls
}//namespace gweni
