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
    setTextPadding(Padding(2, SearchComboBox::RowPaddingY, SearchComboBox::RowPaddingRight,
        SearchComboBox::RowPaddingY));
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

void SearchComboBoxRow::setChecked(bool checked)
{
    if(m_checked == checked)
        return;

    m_checked=checked;
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

bool SearchComboBoxField::onKeyBackspace(bool down)
{
    if(m_box && m_box->isMultiSelect() && getText().empty())
    {
        if(down && !m_box->m_picks.empty())
            m_box->removePick(m_box->m_picks.size()-1, true);
        return true;
    }

    return ParentClass::onKeyBackspace(down);
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
    // the field and the chips go with the box (~Base), and must not reach it
    // meanwhile
    if(m_field)
        m_field->setComboBox(nullptr);

    for(Pick &pick : m_picks)
    {
        if(pick.chip)
            pick.chip->onRemove.removeHandler(this);
    }

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

    list->onVisibilityChanged.removeHandler(this);

    if(!list->hidden())
        list->hide();

    // delayed: this may run from the list's own row press (an onSelection
    // handler deleting the box, which it does with delayedDelete(): the box
    // goes on after its events, as every gweni control does)
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
// Popup), without a pick or after one: the query is over. The field shows
// the value again; with free text it keeps what was typed, which Return or
// the field losing the focus then takes or gives back (resolveText()).
void SearchComboBox::listVisibilityChanged(event::Info)
{
    redraw();

    if(isListOpen())
        return;

    m_query.clear();
    m_lowerQuery.clear();

    if(!m_allowCustom)
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

// What a shown row is when it is no option.
constexpr size_t AddRowKey=static_cast<size_t>(-2);
constexpr size_t NoMatchKey=static_cast<size_t>(-3);
}

void SearchComboBox::addOption(const String &label, const String &name)
{
    m_options.push_back({label, name});
    m_lowerLabels.push_back(lowerCased(label));
    m_picked.push_back(0);
    // a new label in this box's width
    invalidate();

    // the first option is the default, unless a free-text value is the value
    // or the box picks several; while the user types it stays in the field
    // and nothing is selected, the pick that ends the edit selecting
    // (setOptions())
    if((m_selected == NoOption) && !m_hasCustom && !m_multi && !fieldHoldsEdit())
        select(static_cast<int>(m_options.size())-1, false);

    if(isListOpen())
        optionsChanged();
}

bool SearchComboBox::fieldHoldsEdit() const
{
    return m_filterable && m_fieldEdited;
}

int SearchComboBox::findOption(const std::vector<Option> &options, const Option &option)
{
    for(size_t i=0; i < options.size(); ++i)
    {
        if((options[i].label == option.label) && (options[i].name == option.name))
            return static_cast<int>(i);
    }
    return NoOption;
}

// The selection and the picks stay on equal options; a free-text value stays
// the value; otherwise the first option is, or none while the user types. The
// field keeps what the user is typing, which the open list matches against
// the new options: an app refilling them from onTextChangedCaller (an async
// search) runs before the box filters for that edit.
void SearchComboBox::setOptions(std::vector<Option> options)
{
    const bool edited=fieldHoldsEdit();
    const std::vector<Option> previous=std::move(m_options);

    m_options=std::move(options);
    m_lowerLabels.clear();

    for(const Option &option : m_options)
        m_lowerLabels.push_back(lowerCased(option.label));

    m_picked.assign(m_options.size(), 0);

    // a pick of an option stays on an equal one (once); the rest go with
    // their options, free-text ones stay
    for(size_t i=0; i < m_picks.size();)
    {
        Pick &pick=m_picks[i];

        if(pick.option == NoOption)
        {
            ++i;
            continue;
        }

        const int now=findOption(m_options, previous[pick.option]);

        if((now == NoOption) || m_picked[now])
        {
            // removed as a free-text pick: its index is the old options'
            pick.option=NoOption;
            removePick(i, false);
            continue;
        }

        pick.option=now;
        m_picked[now]=1;
        ++i;
    }

    m_widthsValid=false;
    m_highlight=NoOption;
    m_highlightPos=VirtualList::NoItem;

    if(m_hasCustom || m_multi || m_options.empty())
        m_selected=NoOption;
    else
    {
        const int kept=(m_selected == NoOption)?NoOption:findOption(m_options, previous[m_selected]);

        // while the user types, a selection that is gone is none, not the
        // first result: the value is never one they did not pick, and the
        // pick that ends the edit fires onSelection
        if(kept != NoOption)
            m_selected=kept;
        else
            m_selected=edited?NoOption:0;
    }

    // shown again even when the index stays: the option at it can be another
    if(!edited)
        showSelection();
    invalidate();

    if(isListOpen())
        optionsChanged();
    else
    {
        // nothing shown: no index into the old options is left
        m_matches.clear();
        m_addRow=false;
        m_noMatch=false;
    }
}

void SearchComboBox::clearOptions()
{
    setOptions({});
}

void SearchComboBox::setSelectedIndex(int index)
{
    if(m_multi)
        return setSelectedIndices({index});

    select(index, false);
}

String SearchComboBox::getValue()
{
    if(m_hasCustom)
        return m_custom;

    if(m_selected == NoOption)
        return String();

    const Option &option=m_options[m_selected];

    return option.name.empty()?option.label:option.name;
}

void SearchComboBox::select(int index, bool doEvents)
{
    if((index < 0) || (static_cast<size_t>(index) >= m_options.size()))
        index=NoOption;

    if((index == m_selected) && !m_hasCustom)
        return;

    m_selected=index;
    m_hasCustom=false;
    m_custom.clear();
    setRefused(false);
    showSelection();

    if(doEvents)
        onSelection.call(this);
}

String SearchComboBox::shownValueText() const
{
    if(m_hasCustom)
        return m_custom;

    return (m_selected == NoOption)?String():m_options[m_selected].label;
}

// Multi-select, the chips are the value: the box shows no text, and the
// field is empty for the next query.
void SearchComboBox::showSelection()
{
    const String text=m_multi?String():shownValueText();

    // the edit, if any, is over
    m_fieldEdited=false;
    setRefused(false);

    if(m_filterable)
    {
        setText("");
        m_field->showText(text);
    }
    else
        setText(text);
}

void SearchComboBox::setRefused(bool refused)
{
    if(m_refused == refused)
        return;

    m_refused=refused;

    if(m_field)
        m_field->setSkinTextColor(refused?internal::SkinTextColor::SearchComboRefused:
            internal::SkinTextColor::LabelDefault);
}

// The value shown again even when the pick is the selected option: the
// field holds the query (or a refused text), which would otherwise stay and,
// with free text, be taken as a value when the field loses the focus.
void SearchComboBox::pick(int index, bool doEvents)
{
    select(index, doEvents);
    showSelection();
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
    // free text needs the field
    if(!filterable && m_allowCustom)
        return;

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

void SearchComboBox::setAllowCustomValue(bool allow)
{
    if(allow)
        setFilterable(true);

    m_allowCustom=allow;

    if(isListOpen())
        optionsChanged();
}

// The query matched over the cached labels: every option for an empty one.
// With free text, a query that is no option's label (case aside) adds the
// Add row after the matches; otherwise one that matches nothing shows the
// "No matches" row.
void SearchComboBox::matchOptions()
{
    bool exact=false;

    m_matches.clear();

    for(size_t i=0; i < m_options.size(); ++i)
    {
        if(m_lowerQuery.empty() || (m_lowerLabels[i].find(m_lowerQuery) != String::npos))
        {
            m_matches.push_back(i);
            exact=exact || (m_lowerLabels[i].size() == m_lowerQuery.size());
        }
    }

    m_addRow=m_allowCustom && !m_lowerQuery.empty() && !exact;
    m_noMatch=!m_allowCustom && !m_lowerQuery.empty() && m_matches.empty();
}

size_t SearchComboBox::shownRowCount() const
{
    return m_matches.size()+((m_addRow || m_noMatch)?1:0);
}

size_t SearchComboBox::highlightableCount() const
{
    return m_matches.size()+(m_addRow?1:0);
}

size_t SearchComboBox::rowKey(size_t position) const
{
    if(position < m_matches.size())
        return m_matches[position];

    return m_addRow?AddRowKey:NoMatchKey;
}

void SearchComboBox::rowKeys(std::vector<size_t> &keys) const
{
    keys.resize(shownRowCount());

    for(size_t i=0; i < keys.size(); ++i)
        keys[i]=rowKey(i);
}

// The first match, else the Add row (Return then adds what was typed).
size_t SearchComboBox::firstHighlight() const
{
    return (highlightableCount() > 0)?0:VirtualList::NoItem;
}

void SearchComboBox::optionsChanged()
{
    matchOptions();
    m_highlightPos=VirtualList::NoItem;
    m_highlight=NoOption;
    highlightPosition(firstHighlight(), false);

    if(m_rows)
    {
        m_rows->setItemCount(shownRowCount());
        // the options may be others than the rows were bound with
        m_rows->refresh();
    }
}

// What a pooled row shows: an option, the Add row or the "No matches" row
// (rowKey()). The rows still showing what they showed keep their binding;
// only their highlight can have moved. The Add row names the query, so it
// is bound again whenever it shows.
void SearchComboBox::rebindChanged(const std::vector<size_t> &previous)
{
    const size_t count=shownRowCount();

    m_rows->setItemCount(count);

    for(Base *control : m_rows->getRows())
    {
        const size_t item=m_rows->itemForRow(control);

        if((item == VirtualList::NoItem) || (item >= count))
            continue;

        const size_t before=(item < previous.size())?previous[item]:VirtualList::NoItem;
        const size_t now=rowKey(item);

        if((before != now) || (now == AddRowKey))
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

    // from the selection when opened, from the first row when typed into
    size_t position=VirtualList::NoItem;

    if(edited)
        position=firstHighlight();
    else if(m_selected != NoOption)
        position=static_cast<size_t>(m_selected);

    m_highlightPos=position;
    m_highlight=(position < m_matches.size())?static_cast<int>(m_matches[position]):NoOption;
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
    std::vector<size_t> previous;

    rowKeys(previous);
    matchOptions();
    m_highlightPos=firstHighlight();
    m_highlight=(m_highlightPos < m_matches.size())?static_cast<int>(m_matches[m_highlightPos]):NoOption;
    rebindChanged(previous);
    m_rows->scrollToItem(0);
}

void SearchComboBox::fieldTextChanged()
{
    m_query=m_field->getText();
    m_lowerQuery=lowerCased(m_query);
    m_fieldEdited=true;
    // an edit is no longer the refused value
    setRefused(false);

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

// Free text: what was typed is taken (resolveText()), else the value comes
// back.
void SearchComboBox::fieldLostFocus()
{
    closeList();

    if(m_allowCustom)
        resolveText();
    else
        showSelection();
}

int SearchComboBox::optionWithLabel(const String &text) const
{
    const String lower=lowerCased(text);

    for(size_t i=0; i < m_lowerLabels.size(); ++i)
    {
        if(m_lowerLabels[i] == lower)
            return static_cast<int>(i);
    }
    return NoOption;
}

// The field's text as the value: unchanged, or refused already, nothing; an
// option's label (case aside), that option; empty, the value back; anything
// else, a free-text value.
void SearchComboBox::resolveText()
{
    const String text=m_field->getText();

    // refused, the text stays the user's edit
    if(m_refused)
        return;

    // multi-select: an option's label picks it (if it is not), anything else
    // is a free-text pick
    if(m_multi)
    {
        if(text.empty())
        {
            m_fieldEdited=false;
            return;
        }

        const int option=optionWithLabel(text);

        if(option == NoOption)
            commitCustom(text);
        else
        {
            if(!isPicked(option))
                togglePick(option);
            m_field->showText(String());
            m_fieldEdited=false;
        }
        return;
    }

    // the value's own text: the edit is over
    if(text == shownValueText())
    {
        m_fieldEdited=false;
        return;
    }

    const int option=optionWithLabel(text);

    // an option's label in another case shows as the option's, the selected
    // one's included (select() leaves an unchanged index alone)
    if(option != NoOption)
    {
        select(option, true);
        showSelection();
    }
    else if(text.empty())
        showSelection();
    else
        commitCustom(text);
}

// The handler may refuse it (refuseCustomValue()); the field then keeps it in
// the refused colour, and the value stays what it was.
bool SearchComboBox::commitCustom(const String &text)
{
    event::Info info(this);

    info.string=text;
    m_refusing=false;
    onCustomValue.call(this, info);

    if(m_refusing)
    {
        m_refusing=false;
        setRefused(true);
        return false;
    }

    if(m_multi)
    {
        // a text picked already is not picked twice
        bool picked=false;

        for(const Pick &pick : m_picks)
            picked=picked || ((pick.option == NoOption) && (pick.text == text));

        if(!picked)
        {
            addPick(NoOption, text);
            fireSelectionChange(NoOption);
        }
        m_field->showText(String());
        m_fieldEdited=false;
        return true;
    }

    m_selected=NoOption;
    m_hasCustom=true;
    m_custom=text;
    showSelection();
    // its width is in the box's (calculateSize)
    invalidate();
    return true;
}

void SearchComboBox::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();

    if(m_filterable)
        m_field->focus();
}

void SearchComboBox::setDisabled(bool disabled)
{
    // a disabled box picks nothing: its open list goes
    if(disabled)
        closeList();

    ParentClass::setDisabled(disabled);

    if(m_field)
        m_field->setDisabled(disabled);

    for(Pick &pick : m_picks)
        pick.chip->setDisabled(disabled);
}

void SearchComboBox::setFont(const gweni::Font &font)
{
    ParentClass::setFont(font);

    if(m_field)
        m_field->setFont(font);
}

// The field covers the text area: from the text inset to the text padding
// before the chevron's column, the box's height, so its text lands where the
// box's own would.
void SearchComboBox::arrange(skin::Base *skin, Dim dim)
{
    if(m_multi)
    {
        std::vector<Rect> chips;
        Rect field;
        int height;

        flowRects(getWidth(), chips, field, height);

        for(size_t i=0; i < m_picks.size(); ++i)
        {
            const Rect &bounds=m_picks[i].chip->getBounds();

            if(dim == Dim::X)
                placeChild(m_picks[i].chip, Rect(chips[i].x, bounds.y, chips[i].w, bounds.h));
            else
                placeChild(m_picks[i].chip, Rect(bounds.x, chips[i].y, bounds.w, chips[i].h));
        }

        if(m_field && !m_field->hidden())
        {
            const Rect &bounds=m_field->getBounds();

            if(dim == Dim::X)
                placeChild(m_field, Rect(field.x, bounds.y, field.w, bounds.h));
            else
                placeChild(m_field, Rect(bounds.x, field.y, bounds.w, field.h));
        }
    }
    else if(m_field && !m_field->hidden())
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

String SearchComboBox::addRowText(const String &text)
{
    return "Add \xe2\x80\x9c"+text+"\xe2\x80\x9d";
}

void SearchComboBox::bindRow(Base *control, size_t item)
{
    internal::SearchComboBoxRow *row=gweni_cast<internal::SearchComboBoxRow>(control);

    if(!row || (item >= shownRowCount()))
        return;

    ++m_rowBinds;

    const size_t key=rowKey(item);

    if(key == NoMatchKey)
    {
        row->setText(noMatchesText());
        row->setDisabled(true);
        row->setHighlighted(false);
        row->setChecked(false);
        return;
    }

    row->setText((key == AddRowKey)?addRowText(m_query):m_options[key].label);
    row->setDisabled(false);
    row->setHighlighted(item == m_highlightPos);
    row->setChecked((key != AddRowKey) && isPicked(static_cast<int>(key)));
}

// An option row picks it; the Add row adds what was typed, closing the list
// unless the handler refused it.
void SearchComboBox::pickPosition(size_t position)
{
    const size_t key=rowKey(position);

    if(key == AddRowKey)
    {
        // a copy: a handler may set the field's text
        const String text=m_field->getText();

        if(!commitCustom(text))
            return;

        if(m_multi)
            afterMultiPick(NoOption);
        else
        {
            closeList();
            focusInput();
        }
        return;
    }

    if(key == NoMatchKey)
        return;

    if(m_multi)
    {
        togglePick(static_cast<int>(key));
        afterMultiPick(static_cast<int>(key));
    }
    else
        pick(static_cast<int>(key), true);
}

void SearchComboBox::rowPressed(event::Info info)
{
    if(info.integer < shownRowCount())
        pickPosition(info.integer);
}

void SearchComboBox::highlightPosition(size_t position, bool scroll)
{
    if(position >= highlightableCount())
        position=VirtualList::NoItem;

    if(position == m_highlightPos)
        return;

    if(m_rows && (m_highlightPos != VirtualList::NoItem))
    {
        if(auto *row=gweni_cast<internal::SearchComboBoxRow>(m_rows->rowForItem(m_highlightPos)))
            row->setHighlighted(false);
    }

    m_highlightPos=position;
    m_highlight=(position < m_matches.size())?static_cast<int>(m_matches[position]):NoOption;

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
    const size_t count=highlightableCount();

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
    if(!m_rows)
        return;

    const size_t item=m_rows->itemForRow(row);

    if(item < highlightableCount())
        highlightPosition(item, false);
}

bool SearchComboBox::onKeyUp(bool down)
{
    if(!down)
        return true;

    if(isListOpen())
        stepHighlight(-1);
    else if(m_multi)
        openList();
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
    else if(m_multi)
        openList();
    else if((m_selected == NoOption) && !m_options.empty())
        select(0, true);
    else if((m_selected != NoOption) && (static_cast<size_t>(m_selected)+1 < m_options.size()))
        select(m_selected+1, true);

    return true;
}

// Open, the highlighted row is picked (the Add row adds the text). Closed,
// a free-text box takes what was typed (resolveText()).
bool SearchComboBox::onKeyReturn(bool down)
{
    if(!isListOpen())
    {
        if(!m_allowCustom || (m_field->getText() == shownValueText()))
            return false;

        if(down)
            resolveText();
        return true;
    }

    if(down && (m_highlightPos != VirtualList::NoItem))
        pickPosition(m_highlightPos);
    return true;
}

bool SearchComboBox::onKeyBackspace(bool down)
{
    if(!m_multi)
        return false;

    if(down && !m_picks.empty())
        removePick(m_picks.size()-1, true);
    return true;
}

// Open, it closes the list and gives the field back the value; closed with
// the field edited (the list toggled shut, a refused value), it gives it
// back too.
bool SearchComboBox::onKeyEscape(bool down)
{
    const bool edited=m_filterable && (m_field->getText() != shownValueText());

    if(!isListOpen() && !edited)
        return false;

    if(down)
    {
        closeList();
        showSelection();
    }
    return true;
}


///////////////////////////////////////////////////////////////////////////
// Multi-select

void SearchComboBox::setMultiSelect(bool multi)
{
    if(m_multi == multi)
        return;

    closeList();
    clearPicks(false);
    m_multi=multi;
    m_selected=NoOption;
    m_hasCustom=false;
    m_custom.clear();
    setRefused(false);

    // the box grows with its rows of chips: a request is the least it is;
    // single again, the height flag it had before
    if(multi)
    {
        m_singleHeightFlag=getSizeFlags().vertical;
        setSizeFlags({getSizeFlags().horizontal, SizeFlag::Expand});
    }
    else
        setSizeFlags({getSizeFlags().horizontal, m_singleHeightFlag});

    showSelection();
    invalidate();
}

std::vector<int> SearchComboBox::getSelectedIndices() const
{
    std::vector<int> indices;

    for(const Pick &pick : m_picks)
    {
        if(pick.option != NoOption)
            indices.push_back(pick.option);
    }
    return indices;
}

std::vector<String> SearchComboBox::getSelectedValues() const
{
    std::vector<String> values;

    for(const Pick &pick : m_picks)
    {
        if(pick.option == NoOption)
            values.push_back(pick.text);
        else
        {
            const Option &option=m_options[pick.option];

            values.push_back(option.name.empty()?option.label:option.name);
        }
    }
    return values;
}

void SearchComboBox::setSelectedIndices(const std::vector<int> &indices)
{
    if(!m_multi)
        return;

    clearPicks(false);

    for(int index : indices)
    {
        if((index >= 0) && (static_cast<size_t>(index) < m_options.size()) && !isPicked(index))
        {
            addPick(index, String());
            refreshCheck(index);
        }
    }
}

bool SearchComboBox::isPicked(int index) const
{
    return (index >= 0) && (static_cast<size_t>(index) < m_picked.size()) && m_picked[index];
}

std::vector<Chip *> SearchComboBox::getChips() const
{
    std::vector<Chip *> chips;

    for(const Pick &pick : m_picks)
        chips.push_back(pick.chip);
    return chips;
}

// A removable chip, owner-placed by the box (arrange), taking no keyboard: a
// press on it leaves the focus with the field (pressKeepsKeyboardFocus) or
// gives it to the box.
void SearchComboBox::addPick(int index, const String &text)
{
    Chip *chip=newChild<Chip>();

    chip->setPlacement(Placement::Owner);
    chip->setText((index == NoOption)?text:m_options[index].label);
    chip->setRemovable(true);
    chip->setKeyboardInputEnabled(false);
    chip->setTabable(false);
    chip->setDisabled(isDisabled());
    chip->onRemove.add(this, &SearchComboBox::chipRemoved);

    m_picks.push_back({index, (index == NoOption)?text:String(), chip});

    if(index != NoOption)
        m_picked[index]=1;

    invalidate();
}

// Hidden at once and deleted after the frame: this can run from the chip's own
// remove button's click.
void SearchComboBox::removePick(size_t at, bool doEvents)
{
    if(at >= m_picks.size())
        return;

    const Pick pick=m_picks[at];

    m_picks.erase(m_picks.begin()+static_cast<std::ptrdiff_t>(at));

    if(pick.option != NoOption)
        m_picked[pick.option]=0;

    // hidden, it takes no click: its onRemove (which may be running now)
    // keeps its handler until the chip goes
    pick.chip->hide();
    pick.chip->delayedDelete();
    invalidate();
    refreshCheck(pick.option);

    if(doEvents)
        fireSelectionChange(pick.option);
}

void SearchComboBox::chipRemoved(event::Info info)
{
    for(size_t i=0; i < m_picks.size(); ++i)
    {
        if(m_picks[i].chip == info.controlCaller)
            return removePick(i, true);
    }
}

void SearchComboBox::clearPicks(bool keepCustom)
{
    for(size_t i=m_picks.size(); i-- > 0;)
    {
        if(!keepCustom || (m_picks[i].option != NoOption))
            removePick(i, false);
    }
}

void SearchComboBox::togglePick(int index)
{
    if(isPicked(index))
    {
        for(size_t i=0; i < m_picks.size(); ++i)
        {
            if(m_picks[i].option == index)
                return removePick(i, true);
        }
        return;
    }

    addPick(index, String());
    refreshCheck(index);
    fireSelectionChange(index);
}

void SearchComboBox::fireSelectionChange(int index)
{
    event::Info info(this);

    info.integer=static_cast<size_t>(index);
    onSelectionChange.call(this, info);
}

void SearchComboBox::refreshCheck(int index)
{
    if(!m_rows || (index == NoOption))
        return;

    for(Base *control : m_rows->getRows())
    {
        const size_t item=m_rows->itemForRow(control);

        if((item < m_matches.size()) && (static_cast<int>(m_matches[item]) == index))
        {
            if(auto *row=gweni_cast<internal::SearchComboBoxRow>(control))
                row->setChecked(isPicked(index));
        }
    }
}

// The list stays open. A query typed is over: the field is emptied and the
// list shows every option again (only the rows whose option changed bound
// again), the highlight on the option just picked.
void SearchComboBox::afterMultiPick(int option)
{
    // the query, not the field: a free-text pick has emptied the field already
    if(m_filterable && !m_query.empty())
    {
        m_field->showText(String());
        m_fieldEdited=false;
        m_query.clear();
        m_lowerQuery.clear();

        if(isListOpen())
            filter();
    }

    if(option != NoOption)
        setHighlightedIndex(option);

    focusInput();
}

// Each chip at its preferred size (cut to the row), left to right from the
// text inset, a new row when the next does not fit; the field after the last
// chip when MinFieldWidth fits beside it, on a row of its own otherwise, to
// the row's end. A row is as high as the highest of the chips' and the field's
// line; the box is its rows and its vertical padding, at least its one-line
// height.
void SearchComboBox::flowRects(int width, std::vector<Rect> &chips, Rect &field, int &height)
{
    const Padding &text=getTextPadding();
    const int left=m_padding.left+text.left;
    const int right=std::max(left, width-m_padding.right-text.right);
    const bool hasField=m_field && !m_field->hidden();
    int rowHeight=hasField?measureLineHeight(m_field->getFont()):0;

    for(const Pick &pick : m_picks)
        rowHeight=std::max(rowHeight, pick.chip->getPreferredSize().height);

    if(rowHeight <= 0)
        rowHeight=measureLineHeight(getFont());

    int x=left;
    int y=m_padding.top;

    chips.clear();

    for(const Pick &pick : m_picks)
    {
        const int w=std::min(pick.chip->getPreferredSize().width, right-left);

        if((x > left) && (x+w > right))
        {
            x=left;
            y+=rowHeight+RowGap;
        }

        chips.push_back(Rect(x, y, w, rowHeight));
        x+=w+ChipGap;
    }

    if(hasField)
    {
        if((x > left) && (x+MinFieldWidth > right))
        {
            x=left;
            y+=rowHeight+RowGap;
        }
        field=Rect(x, y, right-x, rowHeight);
    }
    else
        field=Rect();

    const bool empty=m_picks.empty() && !hasField;

    height=empty?0:y+rowHeight+m_padding.bottom;
}

void SearchComboBox::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    // the rows at the width the X arrange gave the box, final by now
    if(m_multi && (dim == Dim::Y))
    {
        std::vector<Rect> chips;
        Rect field;
        int height;

        flowRects(getWidth(), chips, field, height);
        m_preferredSize.height=std::max(m_preferredSize.height, preferredFromContent(height, Dim::Y));
        return;
    }

    if((dim != Dim::X) || (m_options.empty() && !m_hasCustom))
        return;

    // a free-text value too: only one is held, measured as the box lays out
    const int widest=std::max(optionsTextWidth(),
        m_hasCustom?measureText(getFont(), m_custom).x:0);
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
    {
        m_optionWidths.clear();
        m_widestWidth=0;
    }

    // fewer options than measured (a new set is measured from scratch)
    if(m_optionWidths.size() > m_options.size())
    {
        m_optionWidths.resize(m_options.size());
        m_widestWidth=0;

        for(int width : m_optionWidths)
            m_widestWidth=std::max(m_widestWidth, width);
    }

    while(m_optionWidths.size() < m_options.size())
    {
        m_optionWidths.push_back(measureText(font, m_options[m_optionWidths.size()].label).x);
        m_widestWidth=std::max(m_widestWidth, m_optionWidths.back());
    }

    m_widthsValid=true;
    m_widthsFont=font;
    m_widthsRenderer=renderer;
    m_widthsSerial=serial;
    return m_widestWidth;
}

void SearchComboBox::updateColors()
{
    if(!shouldDrawBackground())
        return setTextColor(getSkin()->Colors.Button.Normal);

    ParentClass::updateColors();
}

}//namespace controls
}//namespace gweni
