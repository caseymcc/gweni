/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/searchComboBox.h>
#include <gweni/controls/canvas.h>
#include <gweni/skin.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace internal
{

///////////////////////////////////////////////////////////////////////////
//SearchComboBoxList

// No scrolling of its own: the options list in it scrolls, and the menu is as
// high as that list.
GWENI_CONTROL_CONSTRUCTOR(SearchComboBoxList)
{
    setScroll(false, false);
    setDisableIconMargin(true);
    setTabable(false);
}

SearchComboBoxList::~SearchComboBoxList()
{
    if(m_box)
        m_box->listDeleted();
}

int SearchComboBoxList::boxWidth()
{
    if(!m_box || (getAnchor() != m_box) || hidden() || !getCanvas() || (getParent() != getCanvas()))
        return -1;

    return m_box->getCanvasBounds().w;
}

// The width it is given is the box's, which Menu::arrange takes through
// policyResize as the preferred one: a width the content asks for would make
// it as wide as its widest row, which the box's own width already fits.
void SearchComboBoxList::arrange(skin::Base *skin, Dim dim)
{
    const int width=boxWidth();

    if(width >= 0)
    {
        m_preferredSize.width=width;

        // Menu::arrange sets only the height in Y; the box's width may have
        // become final since the X arrange
        if((dim == Dim::Y) && (getWidth() != width))
            setArrangedBounds(getX(), getY(), width, getHeight());
    }

    ParentClass::arrange(skin, dim);
}

///////////////////////////////////////////////////////////////////////////
//SearchComboBoxOptions

// The rows meet the menu's padding: no inset of its own.
GWENI_CONTROL_CONSTRUCTOR(SearchComboBoxOptions)
{
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    setDock(DockPosition::Top);
}

void SearchComboBoxOptions::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::Y)
        return;

    const size_t rows=std::min(getItemCount(), SearchComboBox::MaxShownRows);

    m_preferredSize.height=static_cast<int>(rows)*getRowHeight();
}

///////////////////////////////////////////////////////////////////////////
//SearchComboBoxRow

// A MenuItem's text inset (the menu's padding and the item's), and no focus:
// a press on it leaves the keyboard focus with the box.
GWENI_CONTROL_CONSTRUCTOR(SearchComboBoxRow)
{
    setTextPadding(Padding(2, SearchComboBox::RowPaddingY, 16, SearchComboBox::RowPaddingY));
    setKeyboardInputEnabled(false);
    setTabable(false);
}

void SearchComboBoxRow::setHighlighted(bool highlighted)
{
    if(m_highlighted == highlighted)
        return;

    m_highlighted=highlighted;
    redraw();
}

void SearchComboBoxRow::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    ParentClass::onMouseMoved(x, y, deltaX, deltaY);

    if(m_box)
        m_box->rowHovered(this);
}

void SearchComboBoxRow::updateColors()
{
    const auto &colors=getSkin()->Colors.Button;

    if(isDisabled())
        setTextColor(colors.Disabled);
    else if(m_highlighted)
        setTextColor(colors.Hover);
    else
        setTextColor(colors.Normal);
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//SearchComboBox

// A ComboBox's size, paddings and text placement, with no arrow child: the
// renderer paints the chevron in the right padding.
GWENI_CONTROL_CONSTRUCTOR(SearchComboBox)
{
    setSize(100, 20);
    setTextAlignment(Alignment::Left|Alignment::CenterV);
    setTextPadding(Padding(ChevronInset, 0, 3, 0));
    setText("");
    setMargin(Margin(3, 0, 0, 0));
    setPadding(Padding(0, VerticalPadding, ChevronSize+ChevronInset, VerticalPadding));
    setTabable(true);
    setKeyboardInputEnabled(true);
}

SearchComboBox::~SearchComboBox()
{
    internal::SearchComboBoxList *list=m_list;

    if(!list)
        return;

    m_list=nullptr;
    m_rows=nullptr;
    list->setComboBox(nullptr);

    Canvas *canvas=list->getCanvas();

    // a canvas tearing down deletes it with its other children
    if(canvas && canvas->releasingChildren())
        return;

    if(!list->hidden())
        list->hide();

    // delayed: this may run from the list's own row press (an onSelection
    // handler deleting the box)
    if(canvas)
        list->delayedDelete();
    else
        delete list;
}

// Made on the first open, on the canvas: a box never opened has no list.
void SearchComboBox::makeList()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    m_list=canvas->newChild<internal::SearchComboBoxList>();
    m_list->setComboBox(this);
    m_list->setHidden(true);

    m_rows=m_list->newChild<internal::SearchComboBoxOptions>();
    m_rows->setRowFactory([this](Base *parent) -> Base *
        {
            internal::SearchComboBoxRow *row=parent->newChild<internal::SearchComboBoxRow>();

            row->setComboBox(this);
            return row;
        });
    m_rows->setRowBinder([this](Base *row, size_t item) { bindRow(row, item); });
    m_rows->onItemPressed.add(this, &SearchComboBox::rowPressed);
    m_list->onVisibilityChanged.add(this, &SearchComboBox::listVisibilityChanged);
}

void SearchComboBox::listVisibilityChanged(event::Info)
{
    redraw();
}

void SearchComboBox::listDeleted()
{
    m_list=nullptr;
    m_rows=nullptr;
}

void SearchComboBox::addOption(const String &label, const String &name)
{
    m_options.push_back({label, name});
    // a new label in this box's width
    invalidate();

    if(m_selected == NoOption)
        select(static_cast<int>(m_options.size())-1, false);

    if(isListOpen())
        rebuildMatches();
}

void SearchComboBox::setOptions(std::vector<Option> options)
{
    m_options=std::move(options);
    m_widthsValid=false;
    m_highlight=NoOption;
    // shown again even when the index stays: the option at it is another
    m_selected=m_options.empty()?NoOption:0;
    setText((m_selected == NoOption)?String():m_options[0].label);
    invalidate();

    if(isListOpen())
        rebuildMatches();
}

void SearchComboBox::clearOptions()
{
    setOptions({});
}

void SearchComboBox::setSelectedIndex(int index)
{
    select(index, false);
}

String SearchComboBox::getValue()
{
    if(m_selected == NoOption)
        return String();

    const Option &option=m_options[m_selected];

    return option.name.empty()?option.label:option.name;
}

void SearchComboBox::select(int index, bool doEvents)
{
    if((index < 0) || (static_cast<size_t>(index) >= m_options.size()))
        index=NoOption;

    if(index == m_selected)
        return;

    m_selected=index;
    setText((index == NoOption)?String():m_options[index].label);

    if(doEvents)
        onSelection.call(this);
}

void SearchComboBox::pick(int index, bool doEvents)
{
    select(index, doEvents);
    closeList();
    focus();
}

bool SearchComboBox::isListOpen() const
{
    return m_list && !m_list->hidden();
}

int SearchComboBox::rowHeight()
{
    return measureLineHeight(getSkin()->getDefaultFont())+2*RowPaddingY;
}

void SearchComboBox::rebuildMatches()
{
    m_matches.resize(m_options.size());

    for(size_t i=0; i < m_matches.size(); ++i)
        m_matches[i]=i;

    if((m_highlight != NoOption) && (static_cast<size_t>(m_highlight) >= m_options.size()))
        m_highlight=NoOption;

    if(m_rows)
    {
        m_rows->setItemCount(m_matches.size());
        // the options may be others than the rows were bound with
        m_rows->refresh();
    }
}

void SearchComboBox::openList()
{
    if(!m_list)
        makeList();

    if(!m_list)
        return;

    m_highlight=m_selected;
    m_rows->setRowHeight(rowHeight());
    rebuildMatches();

    if(m_highlight != NoOption)
        m_rows->scrollToItem(static_cast<size_t>(m_highlight));

    m_list->openNextTo(this, PopupPlacement::Below, 0);
}

void SearchComboBox::closeList()
{
    if(!isListOpen())
        return;

    m_list->close();
}

void SearchComboBox::onPress(event::Info)
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    if(isListOpen())
    {
        canvas->closeMenus();
        return;
    }

    canvas->closeMenus();
    openList();
}

void SearchComboBox::bindRow(Base *control, size_t item)
{
    internal::SearchComboBoxRow *row=gweni_cast<internal::SearchComboBoxRow>(control);

    if(!row || (item >= m_matches.size()))
        return;

    const size_t option=m_matches[item];

    row->setText(m_options[option].label);
    row->setHighlighted(static_cast<int>(option) == m_highlight);
}

void SearchComboBox::rowPressed(event::Info info)
{
    const size_t item=info.integer;

    if(item < m_matches.size())
        pick(static_cast<int>(m_matches[item]), true);
}

// In plain mode the shown options are every option, in order, so an option's
// place in the list is its index.
void SearchComboBox::setHighlightedIndex(int index)
{
    if((index < 0) || (static_cast<size_t>(index) >= m_options.size()))
        index=NoOption;

    if(index == m_highlight)
        return;

    if(m_rows && (m_highlight != NoOption))
    {
        if(auto *row=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(m_highlight)))
            row->setHighlighted(false);
    }

    m_highlight=index;

    if(!m_rows || (m_highlight == NoOption))
        return;

    if(auto *row=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(m_highlight)))
        row->setHighlighted(true);

    m_rows->scrollToItem(static_cast<size_t>(m_highlight));
}

void SearchComboBox::stepHighlight(int step)
{
    const int count=static_cast<int>(m_matches.size());

    if(count == 0)
        return;

    int position;

    if(m_highlight == NoOption)
        position=(step > 0)?0:count-1;
    else
        position=((m_highlight+step)%count+count)%count;

    setHighlightedIndex(static_cast<int>(m_matches[position]));
}

void SearchComboBox::rowHovered(internal::SearchComboBoxRow *row)
{
    if(!m_rows)
        return;

    const size_t item=m_rows->itemForRow(row);

    if((item >= m_matches.size()) || (static_cast<int>(m_matches[item]) == m_highlight))
        return;

    if(m_highlight != NoOption)
    {
        if(auto *previous=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(m_highlight)))
            previous->setHighlighted(false);
    }

    m_highlight=static_cast<int>(m_matches[item]);
    row->setHighlighted(true);
}

bool SearchComboBox::onKeyUp(bool down)
{
    if(!down)
        return true;

    if(isListOpen())
        stepHighlight(-1);
    else if(m_selected > 0)
        select(m_selected-1, true);

    return true;
}

bool SearchComboBox::onKeyDown(bool down)
{
    if(!down)
        return true;

    if(isListOpen())
        stepHighlight(1);
    else if((m_selected == NoOption) && !m_options.empty())
        select(0, true);
    else if((m_selected != NoOption) && (static_cast<size_t>(m_selected)+1 < m_options.size()))
        select(m_selected+1, true);

    return true;
}

bool SearchComboBox::onKeyReturn(bool down)
{
    if(!isListOpen())
        return false;

    if(down && (m_highlight != NoOption))
        pick(m_highlight, true);
    return true;
}

bool SearchComboBox::onKeyEscape(bool down)
{
    if(!isListOpen())
        return false;

    if(down)
        closeList();
    return true;
}

void SearchComboBox::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if((dim != Dim::X) || m_options.empty())
        return;

    const int widest=optionsTextWidth();
    const Padding &textPadding=getTextPadding();

    m_preferredSize.width=std::max(m_preferredSize.width, preferredFromContent(
        widest+textPadding.left+textPadding.right+m_padding.left+m_padding.right, Dim::X));
}

// Appended options are measured alone; a new set, font or text layout
// measures every label again.
int SearchComboBox::optionsTextWidth()
{
    const renderer::Base *renderer=getSkin()?getSkin()->getRenderer():nullptr;
    const unsigned serial=renderer?renderer->textLayoutSerial():0;
    const gweni::Font &font=getFont();

    if(!m_widthsValid || !(font == m_widthsFont) || (renderer != m_widthsRenderer) || (serial != m_widthsSerial))
        m_optionWidths.clear();

    m_optionWidths.resize(std::min(m_optionWidths.size(), m_options.size()));

    while(m_optionWidths.size() < m_options.size())
        m_optionWidths.push_back(measureText(font, m_options[m_optionWidths.size()].label).x);

    m_widthsValid=true;
    m_widthsFont=font;
    m_widthsRenderer=renderer;
    m_widthsSerial=serial;

    int widest=0;

    for(int width : m_optionWidths)
        widest=std::max(widest, width);
    return widest;
}

void SearchComboBox::updateColors()
{
    if(!shouldDrawBackground())
        return setTextColor(getSkin()->Colors.Button.Normal);

    ParentClass::updateColors();
}

}//namespace controls
}//namespace gweni
