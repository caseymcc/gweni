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

// Menu's height (its content's, up to the canvas's) cut to the shown rows:
// the options list in it is MaxShownRows high, and its rows past the item
// count are hidden.
void SearchComboBoxList::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if((dim != Dim::Y) || !m_options)
        return;

    const Padding &inner=m_innerPanel->getPadding();
    const size_t rows=std::min(m_options->getItemCount(), SearchComboBox::MaxShownRows);
    const int height=static_cast<int>(rows)*m_options->getRowHeight()+inner.top+inner.bottom
        +m_padding.top+m_padding.bottom;

    m_preferredSize.height=std::min(m_preferredSize.height, height);
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

// A fixed viewport, so the pool (VirtualList: the viewport's rows plus one)
// does not follow the matches.
void SearchComboBoxOptions::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
        m_preferredSize.height=static_cast<int>(SearchComboBox::MaxShownRows)*getRowHeight();
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

///////////////////////////////////////////////////////////////////////////
//SearchComboBoxField

// Its box sets its placement, paddings, alignment and font (setFilterable).
GWENI_CONTROL_CONSTRUCTOR(SearchComboBoxField)
{
}

// The keys the box acts on while the field has the focus. Up and Down are
// not a single-line TextBox's own; Return is (its release moves the focus
// on), and Escape is the box's.
bool SearchComboBoxField::onKeyUp(bool down)
{
    return m_box?m_box->onKeyUp(down):ParentClass::onKeyUp(down);
}

bool SearchComboBoxField::onKeyDown(bool down)
{
    return m_box?m_box->onKeyDown(down):ParentClass::onKeyDown(down);
}

bool SearchComboBoxField::onKeyReturn(bool down)
{
    if(down)
    {
        m_returnTaken=m_box && m_box->onKeyReturn(true);
        return m_returnTaken || ParentClass::onKeyReturn(true);
    }

    if(m_returnTaken)
    {
        m_returnTaken=false;
        return true;
    }

    return ParentClass::onKeyReturn(false);
}

bool SearchComboBoxField::onKeyEscape(bool down)
{
    if(m_box && m_box->onKeyEscape(down))
        return true;

    return ParentClass::onKeyEscape(down);
}

void SearchComboBoxField::onMouseClickLeft(int x, int y, bool down)
{
    ParentClass::onMouseClickLeft(x, y, down);

    if(down && m_box)
        m_box->fieldClicked();
}

void SearchComboBoxField::onLostKeyboardFocus()
{
    ParentClass::onLostKeyboardFocus();

    if(m_box)
        m_box->fieldLostFocus();
}

void SearchComboBoxField::insertText(const String &str)
{
    m_inserting=true;
    m_changedInInsert=false;
    ParentClass::insertText(str);
    m_inserting=false;

    if(m_changedInInsert && m_box)
        m_box->fieldTextChanged();
}

void SearchComboBoxField::onTextChanged()
{
    ParentClass::onTextChanged();

    if(m_inserting)
        m_changedInInsert=true;
    else if(m_box)
        m_box->fieldTextChanged();
}

void SearchComboBoxField::showText(const String &text)
{
    if(text == getText())
        return;

    setText(text, false);
    moveCaretToend();
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
    // the field goes with the box (~Base), and must not reach it meanwhile
    if(m_field)
        m_field->setComboBox(nullptr);

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
    m_list->setOptions(m_rows);
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

// Closed, by the box or from outside (a press outside it, Escape over a
// Popup), without a pick or after one: the query is over, and the field
// shows the selection again.
void SearchComboBox::listVisibilityChanged(event::Info)
{
    redraw();

    if(isListOpen())
        return;

    m_query.clear();
    m_lowerQuery.clear();
    showSelection();
}

void SearchComboBox::listDeleted()
{
    m_list=nullptr;
    m_rows=nullptr;
}

namespace
{
// ASCII letters lower-cased, every other byte (a UTF-8 sequence's included)
// kept: a substring of the lower-cased label is a substring of the label.
String lowerCased(const String &text)
{
    String lower=text;

    for(char &c : lower)
    {
        if((c >= 'A') && (c <= 'Z'))
            c=static_cast<char>(c-'A'+'a');
    }
    return lower;
}
}

void SearchComboBox::addOption(const String &label, const String &name)
{
    m_options.push_back({label, name});
    m_lowerLabels.push_back(lowerCased(label));
    // a new label in this box's width
    invalidate();

    if(m_selected == NoOption)
        select(static_cast<int>(m_options.size())-1, false);

    if(isListOpen())
        optionsChanged();
}

void SearchComboBox::setOptions(std::vector<Option> options)
{
    m_options=std::move(options);
    m_lowerLabels.clear();

    for(const Option &option : m_options)
        m_lowerLabels.push_back(lowerCased(option.label));

    m_widthsValid=false;
    m_highlight=NoOption;
    m_highlightPos=VirtualList::NoItem;
    // shown again even when the index stays: the option at it is another
    m_selected=m_options.empty()?NoOption:0;
    showSelection();
    invalidate();

    if(isListOpen())
        optionsChanged();
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
    showSelection();

    if(doEvents)
        onSelection.call(this);
}

void SearchComboBox::showSelection()
{
    const String label=(m_selected == NoOption)?String():m_options[m_selected].label;

    if(m_filterable)
    {
        setText("");
        m_field->showText(label);
    }
    else
        setText(label);
}

void SearchComboBox::pick(int index, bool doEvents)
{
    select(index, doEvents);
    closeList();
    focusInput();
}

void SearchComboBox::focusInput()
{
    if(m_filterable)
        m_field->focus();
    else
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

void SearchComboBox::setFilterable(bool filterable)
{
    if(m_filterable == filterable)
        return;

    if(filterable && !m_field)
    {
        m_field=newChild<internal::SearchComboBoxField>();
        m_field->setComboBox(this);
        m_field->setPlacement(Placement::Owner);
        m_field->setPadding(Padding(0, 0, 0, 0));
        m_field->setTextPadding(Padding(0, 0, 0, 0));
        m_field->setTextAlignment(Alignment::Left|Alignment::CenterV);
        m_field->setFont(getFont());
        m_field->setDisabled(isDisabled());
    }

    const bool hadFocus=isFocussed() || (m_field && m_field->isFocussed());

    m_filterable=filterable;
    closeList();
    m_field->setHidden(!filterable);
    // The field takes the Tab stop and the keyboard: a press on the box
    // keeps the field's focus (pressKeepsKeyboardFocus), and a focus() of
    // the box passes it on.
    setTabable(!filterable);
    setKeyboardInputEnabled(!filterable);
    showSelection();

    if(hadFocus)
        focusInput();
}

// The query matched over the cached labels: every option for an empty one.
void SearchComboBox::matchOptions()
{
    m_matches.clear();

    for(size_t i=0; i < m_options.size(); ++i)
    {
        if(m_lowerQuery.empty() || (m_lowerLabels[i].find(m_lowerQuery) != String::npos))
            m_matches.push_back(i);
    }

    m_noMatch=!m_lowerQuery.empty() && m_matches.empty();
}

size_t SearchComboBox::shownRowCount() const
{
    return m_noMatch?1:m_matches.size();
}

void SearchComboBox::optionsChanged()
{
    matchOptions();
    highlightPosition(m_matches.empty()?VirtualList::NoItem:0, false);

    if(m_rows)
    {
        m_rows->setItemCount(shownRowCount());
        // the options may be others than the rows were bound with
        m_rows->refresh();
    }
}

// What a pooled row shows: the option at its place, or the "No matches" row.
// The rows still showing what they showed keep their binding; only their
// highlight can have moved.
void SearchComboBox::rebindChanged(const std::vector<size_t> &previous, bool previousNoMatch)
{
    constexpr size_t NoMatchRow=static_cast<size_t>(-2);
    const size_t count=shownRowCount();

    m_rows->setItemCount(count);

    for(Base *control : m_rows->getRows())
    {
        const size_t item=m_rows->itemForRow(control);

        if((item == VirtualList::NoItem) || (item >= count))
            continue;

        const size_t before=previousNoMatch?NoMatchRow:(item < previous.size()?previous[item]:VirtualList::NoItem);
        const size_t now=m_noMatch?NoMatchRow:m_matches[item];

        if(before != now)
            m_rows->refreshItem(item);
        else if(auto *row=gweni_cast<internal::SearchComboBoxRow>(control))
            row->setHighlighted(item == m_highlightPos);
    }
}

void SearchComboBox::showList(bool edited)
{
    if(!m_list)
        makeList();

    if(!m_list)
        return;

    if(!edited)
    {
        m_query.clear();
        m_lowerQuery.clear();
    }

    matchOptions();

    // from the selection when opened, from the first match when typed into
    size_t position=VirtualList::NoItem;

    if(edited)
        position=m_matches.empty()?VirtualList::NoItem:0;
    else if(m_selected != NoOption)
        position=static_cast<size_t>(m_selected);

    m_highlight=(position == VirtualList::NoItem)?NoOption:static_cast<int>(m_matches[position]);
    m_highlightPos=position;
    m_rows->setRowHeight(rowHeight());
    m_rows->setItemCount(shownRowCount());
    m_rows->refresh();
    m_rows->scrollToItem((position == VirtualList::NoItem)?0:position);
    m_list->openNextTo(this, PopupPlacement::Below, 0);
}

void SearchComboBox::openList()
{
    showList(false);
}

void SearchComboBox::closeList()
{
    if(!isListOpen())
        return;

    m_list->close();
}

void SearchComboBox::filter()
{
    const std::vector<size_t> previous=m_matches;
    const bool previousNoMatch=m_noMatch;

    matchOptions();
    m_highlight=m_matches.empty()?NoOption:static_cast<int>(m_matches[0]);
    m_highlightPos=m_matches.empty()?VirtualList::NoItem:0;
    rebindChanged(previous, previousNoMatch);
    m_rows->scrollToItem(0);
}

void SearchComboBox::fieldTextChanged()
{
    m_query=m_field->getText();
    m_lowerQuery=lowerCased(m_query);

    event::Info info(this);

    info.string=m_query;
    onTextChangedCaller.call(this, info);

    if(isListOpen())
        filter();
    else
        showList(true);
}

// A click on the field with the list closed opens it with the field's text
// selected, so what is typed replaces it.
void SearchComboBox::fieldClicked()
{
    if(isListOpen())
        return;

    if(Canvas *canvas=getCanvas())
        canvas->closeMenus();

    showList(false);
    m_field->onSelectAll(m_field);
}

void SearchComboBox::fieldLostFocus()
{
    closeList();
    showSelection();
}

void SearchComboBox::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();

    if(m_filterable)
        m_field->focus();
}

void SearchComboBox::setDisabled(bool disabled)
{
    ParentClass::setDisabled(disabled);

    if(m_field)
        m_field->setDisabled(disabled);
}

// The field covers the text area: from the text inset to the text padding
// before the chevron's column, the box's height, so its text lands where the
// box's own would.
void SearchComboBox::arrange(skin::Base *skin, Dim dim)
{
    if(m_field && !m_field->hidden())
    {
        const Rect &bounds=m_field->getBounds();
        const Padding &text=getTextPadding();

        if(dim == Dim::X)
            placeChild(m_field, Rect(m_padding.left+text.left, bounds.y,
                std::max(0, getWidth()-m_padding.left-text.left-m_padding.right-text.right), bounds.h));
        else
            placeChild(m_field, Rect(bounds.x, 0, bounds.w, getHeight()));
    }

    ParentClass::arrange(skin, dim);
}

void SearchComboBox::onPress(event::Info)
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    if(m_filterable)
        m_field->focus();

    if(isListOpen())
    {
        canvas->closeMenus();
        return;
    }

    canvas->closeMenus();
    openList();

    if(m_filterable)
        m_field->onSelectAll(m_field);
}

void SearchComboBox::bindRow(Base *control, size_t item)
{
    internal::SearchComboBoxRow *row=gweni_cast<internal::SearchComboBoxRow>(control);

    if(!row)
        return;

    ++m_rowBinds;

    if(m_noMatch)
    {
        row->setText(noMatchesText());
        row->setDisabled(true);
        row->setHighlighted(false);
        return;
    }

    if(item >= m_matches.size())
        return;

    row->setText(m_options[m_matches[item]].label);
    row->setDisabled(false);
    row->setHighlighted(item == m_highlightPos);
}

void SearchComboBox::rowPressed(event::Info info)
{
    const size_t item=info.integer;

    if(!m_noMatch && (item < m_matches.size()))
        pick(static_cast<int>(m_matches[item]), true);
}

void SearchComboBox::highlightPosition(size_t position, bool scroll)
{
    if(position >= m_matches.size())
        position=VirtualList::NoItem;

    if(position == m_highlightPos)
        return;

    if(m_rows && (m_highlightPos != VirtualList::NoItem))
    {
        if(auto *row=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(m_highlightPos)))
            row->setHighlighted(false);
    }

    m_highlightPos=position;
    m_highlight=(position == VirtualList::NoItem)?NoOption:static_cast<int>(m_matches[position]);

    if(!m_rows || (position == VirtualList::NoItem))
        return;

    if(auto *row=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(position)))
        row->setHighlighted(true);

    if(scroll)
        m_rows->scrollToItem(position);
}

// The option's place among the shown ones, O(matches) on data.
void SearchComboBox::setHighlightedIndex(int index)
{
    size_t position=VirtualList::NoItem;

    for(size_t i=0; i < m_matches.size(); ++i)
    {
        if(static_cast<int>(m_matches[i]) == index)
        {
            position=i;
            break;
        }
    }

    highlightPosition(position, true);
}

void SearchComboBox::stepHighlight(int step)
{
    const size_t count=m_matches.size();

    if(count == 0)
        return;

    size_t position;

    if(m_highlightPos == VirtualList::NoItem)
        position=(step > 0)?0:count-1;
    else
        position=static_cast<size_t>(((static_cast<int64_t>(m_highlightPos)+step)%static_cast<int64_t>(count)
            +static_cast<int64_t>(count))%static_cast<int64_t>(count));

    highlightPosition(position, true);
}

void SearchComboBox::rowHovered(internal::SearchComboBoxRow *row)
{
    if(!m_rows || m_noMatch)
        return;

    const size_t item=m_rows->itemForRow(row);

    if(item < m_matches.size())
        highlightPosition(item, false);
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

// Open, it closes the list, which gives the field back the selection's label;
// closed with the field edited (the list toggled shut), it gives it back too.
bool SearchComboBox::onKeyEscape(bool down)
{
    const bool edited=m_filterable && (m_field->getText() != ((m_selected == NoOption)?String():
        m_options[m_selected].label));

    if(!isListOpen() && !edited)
        return false;

    if(down)
    {
        closeList();
        showSelection();
    }
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
