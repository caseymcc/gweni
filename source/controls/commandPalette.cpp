/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  See license in gweni.h
 */


#include <gweni/controls/commandPalette.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/text/fuzzyMatch.h>
#include <gweni/skin.h>
#include <gweni/gweni.h>

#include <algorithm>

namespace gweni
{
namespace controls
{

namespace
{
// What the "No matching commands" row is: no command's index.
constexpr size_t NoMatchKey=static_cast<size_t>(-3);

bool within(const Base *control, const Base *root)
{
    for(; control; control=control->getParent())
    {
        if(control == root)
            return true;
    }
    return false;
}
}

namespace internal
{

///////////////////////////////////////////////////////////////////////////
//CommandPalettePopup

GWENI_CONTROL_CONSTRUCTOR(CommandPalettePopup)
{
}

CommandPalettePopup::~CommandPalettePopup()
{
    if(m_palette)
        m_palette->popupDeleted();
}

// The width follows the canvas's, which invalidates its children when it is
// resized. The list under the field is MaxShownRows rows high; the popup is
// cut to the shown ones (its children are clipped to it).
void CommandPalettePopup::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(!m_palette)
        return;

    if(dim == Dim::X)
    {
        if(Canvas *canvas=getCanvas())
            m_preferredSize.width=CommandPalette::widthFor(canvas->getWidth());
        return;
    }

    CommandPaletteList *list=m_palette->getList();
    CommandPaletteField *field=m_palette->getField();

    if(!list || !field)
        return;

    const size_t rows=std::min(m_palette->shownRowCount(), CommandPalette::MaxShownRows);
    const Margin &listMargin=list->getMargin();
    const Margin &fieldMargin=field->getMargin();
    const int height=m_padding.top+fieldMargin.top+field->getPreferredSize().height+fieldMargin.bottom
        +listMargin.top+static_cast<int>(rows)*list->getRowHeight()+m_padding.bottom;

    m_preferredSize.height=std::min(m_preferredSize.height, height);
}

///////////////////////////////////////////////////////////////////////////
//CommandPaletteList

GWENI_CONTROL_CONSTRUCTOR(CommandPaletteList)
{
    m_innerPanel->setMargin(Margin(0, 0, 0, 0));
    setDock(DockPosition::Top);
    setMargin(Margin(0, CommandPalette::ListGap, 0, 0));
}

void CommandPaletteList::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::Y)
        m_preferredSize.height=static_cast<int>(CommandPalette::MaxShownRows)*getRowHeight();
}

///////////////////////////////////////////////////////////////////////////
//CommandPaletteRow

// The labels take no mouse input, so the row is what a move or a press is on.
GWENI_CONTROL_CONSTRUCTOR(CommandPaletteRow)
{
    setPadding(Padding(6, 0, 6, 0));
    setTextPadding(Padding(0, 0, 0, 0));

    m_category=newChild<Label>();
    m_category->setDock(DockPosition::Left);
    m_category->setMargin(Margin(0, 0, CommandPalette::CategoryGap, 0));
    m_category->setSkinTextColor(SkinTextColor::CommandPaletteCategory);
    m_category->setMouseInputEnabled(false);

    m_shortcut=newChild<Label>();
    m_shortcut->setDock(DockPosition::Right);
    m_shortcut->setMargin(Margin(CommandPalette::CategoryGap, 0, 0, 0));
    m_shortcut->setSkinTextColor(SkinTextColor::CommandPaletteShortcut);
    m_shortcut->setMouseInputEnabled(false);

    m_label=newChild<Label>();
    m_label->setDock(DockPosition::Center);
    m_label->setMouseInputEnabled(false);
}

void CommandPaletteRow::show(const String &category, const String &label, const String &shortcut)
{
    // Label::setText changes nothing for the same string
    m_category->setText(category);
    m_category->setHidden(category.empty());
    m_label->setText(label);
    m_shortcut->setText(shortcut);
    m_shortcut->setHidden(shortcut.empty());
}

const String &CommandPaletteRow::getCategoryText() const
{
    return m_category->getText();
}

const String &CommandPaletteRow::getLabelText() const
{
    return m_label->getText();
}

const String &CommandPaletteRow::getShortcutText() const
{
    return m_shortcut->getText();
}

void CommandPaletteRow::onMouseMoved(int x, int y, int deltaX, int deltaY)
{
    // SearchComboBoxRow's would tell its box, which it has none of
    VirtualListRow::onMouseMoved(x, y, deltaX, deltaY);

    if(m_palette)
        m_palette->rowHovered(this);
}

void CommandPaletteRow::updateColors()
{
    ParentClass::updateColors();

    if(m_label)
        m_label->setTextColor(getTextColor());
}

///////////////////////////////////////////////////////////////////////////
//CommandPaletteField

GWENI_CONTROL_CONSTRUCTOR(CommandPaletteField)
{
    setDock(DockPosition::Top);
    // as wide as the popup (a TextBox is a fixed 200 by default)
    setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    setPlaceholder(CommandPalette::placeholderText());
}

bool CommandPaletteField::onKeyUp(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyUp(down);

    if(down)
        m_palette->stepHighlight(-1);
    return true;
}

bool CommandPaletteField::onKeyDown(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyDown(down);

    if(down)
        m_palette->stepHighlight(1);
    return true;
}

bool CommandPaletteField::onKeyPageUp(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyPageUp(down);

    if(down)
        m_palette->pageHighlight(-1);
    return true;
}

bool CommandPaletteField::onKeyPageDown(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyPageDown(down);

    if(down)
        m_palette->pageHighlight(1);
    return true;
}

// The press runs the highlighted command, if any; press and release are the
// field's either way, so it moves no focus on as a TextBox's would.
bool CommandPaletteField::onKeyReturn(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyReturn(down);

    if(down)
        m_palette->runHighlighted();
    return true;
}

// Taken, both ways: the palette has one focusable control, and a Tab moving
// the focus behind the open popup would leave it open and deaf to keys.
bool CommandPaletteField::onKeyTab(bool down)
{
    if(!m_palette)
        return ParentClass::onKeyTab(down);

    return true;
}

void CommandPaletteField::clearText()
{
    if(getText().empty())
        return;

    m_clearing=true;
    setText(String(), false);
    m_clearing=false;
}

void CommandPaletteField::onTextChanged()
{
    ParentClass::onTextChanged();

    if(!m_clearing && m_palette)
        m_palette->fieldTextChanged();
}

}//namespace internal

///////////////////////////////////////////////////////////////////////////
//CommandPalette

// Hidden and drawing nothing: it is the popup that shows.
GWENI_CONTROL_CONSTRUCTOR(CommandPalette)
{
    setHidden(true);
    setBounds(0, 0, 0, 0);
    setMouseInputEnabled(false);
    setKeyboardInputEnabled(false);
    setTabable(false);
    m_filter.setMatcher([this](const FilteredList &, std::vector<size_t> &matches)
        {
            matchCommands(matches);
        });
}

CommandPalette::~CommandPalette()
{
    // closed while it is whole: popupClosed gives the focus back (not in a
    // canvas's teardown, which deletes the popup with its other children and
    // where what popupClosed reads may be gone, as Base::dropOwnedPopups)
    Canvas *canvas=m_popup?m_popup->getCanvas():nullptr;

    if(!canvas || !canvas->releasingChildren())
        close();
    detach();

    // the popup goes with its owner (Base's ~Base) and must not call back
    if(m_popup)
    {
        m_popup->onClose.removeHandler(this);
        m_popup->setPalette(nullptr);
    }

    if(m_list)
    {
        m_list->onItemPressed.removeHandler(this);

        for(Base *control : m_list->getRows())
        {
            if(auto *row=gweni_cast<internal::CommandPaletteRow>(control))
                row->setPalette(nullptr);
        }
    }

    if(m_field)
        m_field->setPalette(nullptr);
}

String CommandPalette::matchText(const Command &command)
{
    if(command.category.empty())
        return command.label;

    return command.category+": "+command.label;
}

int CommandPalette::widthFor(int canvasWidth)
{
    return std::max(0, std::min(Width, canvasWidth-2*CanvasInset));
}

void CommandPalette::addCommand(const Command &command)
{
    auto found=m_byId.find(command.id);

    if(found != m_byId.end())
        m_commands[found->second]=command;
    else
    {
        m_byId[command.id]=m_commands.size();
        m_commands.push_back(command);
    }
    commandsChanged();
}

bool CommandPalette::removeCommand(const String &id)
{
    auto found=m_byId.find(id);

    if(found == m_byId.end())
        return false;

    m_commands.erase(m_commands.begin()+static_cast<std::ptrdiff_t>(found->second));
    m_byId.clear();

    for(size_t i=0; i < m_commands.size(); ++i)
        m_byId[m_commands[i].id]=i;

    commandsChanged();
    return true;
}

void CommandPalette::clearCommands()
{
    m_commands.clear();
    m_byId.clear();
    commandsChanged();
}

int CommandPalette::findCommand(const String &id) const
{
    auto found=m_byId.find(id);

    return (found == m_byId.end())?NoCommand:static_cast<int>(found->second);
}

bool CommandPalette::isCommandEnabled(size_t index) const
{
    const Command &command=m_commands[index];

    return command.enabled && (!command.isEnabled || command.isEnabled());
}

void CommandPalette::commandsChanged()
{
    m_stale=true;

    if(!isOpen())
    {
        // nothing shown: no index into the old commands is left
        m_filter.clearMatches();
        m_filter.setHighlightState(VirtualList::NoItem);
        return;
    }

    // the commands may be others than the rows were bound with
    m_filter.match();

    if(m_filter.matches().empty())
        m_filter.setTrailingRow(NoMatchKey, false, false);
    else
        m_filter.clearTrailingRow();

    m_filter.setHighlightState(m_filter.matches().empty()?VirtualList::NoItem:0);
    m_list->setItemCount(m_filter.shownRowCount());
    m_list->refresh();
    m_list->scrollToItem(0);
    m_popup->invalidate();
}

// The empty query's order sorts strings, so it is kept until the commands
// change.
void CommandPalette::prepare() const
{
    if(!m_stale)
        return;

    m_stale=false;
    m_texts.clear();
    m_texts.reserve(m_commands.size());

    for(const Command &command : m_commands)
        m_texts.push_back(matchText(command));

    m_sortedOrder.resize(m_commands.size());

    for(size_t i=0; i < m_sortedOrder.size(); ++i)
        m_sortedOrder[i]=i;

    std::stable_sort(m_sortedOrder.begin(), m_sortedOrder.end(), [this](size_t a, size_t b)
        {
            const Command &left=m_commands[a];
            const Command &right=m_commands[b];

            if(left.category != right.category)
                return left.category < right.category;
            return left.label < right.label;
        });
}

void CommandPalette::recentRanks(std::vector<int> &ranks) const
{
    const int none=static_cast<int>(m_recent.size());

    ranks.assign(m_commands.size(), none);

    if(!m_recentFirst)
        return;

    for(size_t i=0; i < m_recent.size(); ++i)
    {
        auto found=m_byId.find(m_recent[i]);

        if(found != m_byId.end())
            ranks[found->second]=static_cast<int>(i);
    }
}

// Disabled commands are left out. An empty query: the recent ones, latest
// first, then the rest in m_sortedOrder. Otherwise text::fuzzyRank's order,
// equal scores by recency (its tie rank) and then index.
void CommandPalette::matchCommands(std::vector<size_t> &matches) const
{
    prepare();

    std::vector<int> ranks;

    recentRanks(ranks);

    const String &query=m_filter.query();

    if(query.empty())
    {
        if(m_recentFirst)
        {
            for(const String &id : m_recent)
            {
                auto found=m_byId.find(id);

                if((found != m_byId.end()) && isCommandEnabled(found->second))
                    matches.push_back(found->second);
            }
        }

        const int none=static_cast<int>(m_recent.size());

        for(size_t index : m_sortedOrder)
        {
            if((ranks[index] == none) && isCommandEnabled(index))
                matches.push_back(index);
        }
        return;
    }

    text::fuzzyRank(query, m_texts, matches, &ranks);
    matches.erase(std::remove_if(matches.begin(), matches.end(),
        [this](size_t index) { return !isCommandEnabled(index); }), matches.end());
}

void CommandPalette::attach(Canvas *canvas, const String &accelerator)
{
    detach();

    if(!canvas)
        return;

    if(!getParent())
        setParent(canvas);

    m_attachedCanvas=canvas;
    m_attachedCanvasId=canvas->getControlId();
    m_accelerator=Base::acceleratorKey(accelerator);
    canvas->addAccelerator(accelerator, &CommandPalette::acceleratorPressed, this);
    m_acceleratorCaller=canvas->m_accelerators[m_accelerator];
}

void CommandPalette::detach()
{
    Canvas *canvas=m_attachedCanvas;
    event::Caller *caller=m_acceleratorCaller;

    m_attachedCanvas=nullptr;
    m_acceleratorCaller=nullptr;

    if(!canvas || (Base::getControlById(m_attachedCanvasId) != canvas))
        return;

    auto found=canvas->m_accelerators.find(m_accelerator);

    // another palette may have taken the key since: its caller stays
    if((found != canvas->m_accelerators.end()) && (found->second == caller))
    {
        delete found->second;
        canvas->m_accelerators.erase(found);
    }
}

void CommandPalette::acceleratorPressed(event::Info)
{
    toggle();
}

void CommandPalette::makePopup()
{
    Canvas *canvas=getCanvas();

    if(!canvas)
        return;

    // Base::createPopup with the palette's own type: owned by the palette
    m_popup=canvas->newChild<internal::CommandPalettePopup>();
    m_popup->setOwner(this);
    m_popup->setPalette(this);

    m_field=m_popup->newChild<internal::CommandPaletteField>();
    m_field->setPalette(this);

    m_list=m_popup->newChild<internal::CommandPaletteList>();
    m_list->setRowFactory([this](Base *parent) -> Base *
        {
            internal::CommandPaletteRow *row=parent->newChild<internal::CommandPaletteRow>();

            row->setPalette(this);
            return row;
        });
    m_list->setRowBinder([this](Base *row, size_t item) { bindRow(row, item); });
    m_list->onItemPressed.add(this, &CommandPalette::rowPressed);
    m_filter.setRows(m_list, [](Base *control, bool highlighted)
        {
            if(auto *row=gweni_cast<internal::CommandPaletteRow>(control))
                row->setHighlighted(highlighted);
        });
    m_popup->onClose.add(this, &CommandPalette::popupClosed);
}

void CommandPalette::popupDeleted()
{
    m_popup=nullptr;
    m_field=nullptr;
    m_list=nullptr;
    m_filter.setRows(nullptr, nullptr);
}

int CommandPalette::rowHeight()
{
    return measureLineHeight(getSkin()->getDefaultFont())+2*SearchComboBox::RowPaddingY;
}

bool CommandPalette::isOpen() const
{
    return m_popup && m_popup->isOpen();
}

void CommandPalette::open()
{
    if(isOpen())
        return;

    if(!m_popup)
        makePopup();

    if(!m_popup)
        return;

    m_hasReturnFocus=KeyboardFocus && !within(KeyboardFocus, m_popup);
    m_returnFocus=m_hasReturnFocus?KeyboardFocus->getControlId():0;

    m_field->clearText();
    m_filter.clearQuery();
    m_filter.match();

    if(m_filter.matches().empty())
        m_filter.setTrailingRow(NoMatchKey, false, false);
    else
        m_filter.clearTrailingRow();

    m_filter.setHighlightState(m_filter.matches().empty()?VirtualList::NoItem:0);
    m_list->setRowHeight(rowHeight());
    m_list->setItemCount(m_filter.shownRowCount());
    m_list->refresh();
    m_list->scrollToItem(0);
    m_popup->open(this, PopupPlacement::CanvasTop);
    m_field->focus();
}

void CommandPalette::close()
{
    if(isOpen())
        m_popup->close();
}

void CommandPalette::toggle()
{
    if(isOpen())
        close();
    else
        open();
}

// However it closed (Escape, a press outside, a run, the accelerator): the
// focus back to the control that had it, unless the close gave it to another
// (a press outside on a control focuses that control after this).
void CommandPalette::popupClosed(event::Info)
{
    const bool had=m_hasReturnFocus;

    m_hasReturnFocus=false;

    if(KeyboardFocus && !within(KeyboardFocus, m_popup))
        return;

    if(KeyboardFocus)
        KeyboardFocus->unfocus();

    if(!had)
        return;

    Base *control=Base::getControlById(m_returnFocus);

    if(control && (control->getCanvas() == getCanvas()) && control->visible())
        control->focus();
}

void CommandPalette::filter()
{
    std::vector<size_t> previous;

    m_filter.rowKeys(previous);
    m_filter.match();

    if(m_filter.matches().empty())
        m_filter.setTrailingRow(NoMatchKey, false, false);
    else
        m_filter.clearTrailingRow();

    const size_t before=previous.size();

    m_filter.setHighlightState(m_filter.matches().empty()?VirtualList::NoItem:0);
    m_filter.rebindChanged(previous);
    m_list->scrollToItem(0);

    // the popup is as high as the shown rows
    if(std::min(before, MaxShownRows) != std::min(m_filter.shownRowCount(), MaxShownRows))
        m_popup->invalidate();
}

void CommandPalette::fieldTextChanged()
{
    if(!isOpen())
        return;

    m_filter.setQuery(m_field->getText());
    filter();
}

void CommandPalette::bindRow(Base *control, size_t item)
{
    internal::CommandPaletteRow *row=gweni_cast<internal::CommandPaletteRow>(control);

    if(!row || (item >= m_filter.shownRowCount()))
        return;

    m_filter.countBind();

    const size_t key=m_filter.rowKey(item);

    if(key == NoMatchKey)
    {
        row->show(String(), noMatchesText(), String());
        row->setDisabled(true);
        row->setHighlighted(false);
        return;
    }

    const Command &command=m_commands[key];

    row->show(command.category.empty()?String():command.category+":", command.label, command.shortcut);
    row->setDisabled(false);
    row->setHighlighted(item == m_filter.highlightPosition());
}

void CommandPalette::rowPressed(event::Info info)
{
    const size_t position=info.integer;

    if((position >= m_filter.matches().size()))
        return;

    run(m_filter.matches()[position]);
}

void CommandPalette::rowHovered(internal::CommandPaletteRow *row)
{
    m_filter.rowHovered(row);
}

void CommandPalette::stepHighlight(int step)
{
    m_filter.step(step);
}

void CommandPalette::pageHighlight(int pages)
{
    m_filter.page(pages, MaxShownRows);
}

bool CommandPalette::runHighlighted()
{
    const int index=m_filter.highlightedOption();

    if(index == FilteredList::NoOption)
        return false;

    run(static_cast<size_t>(index));
    return true;
}

bool CommandPalette::runCommand(const String &id)
{
    const int index=findCommand(id);

    if((index == NoCommand) || !isCommandEnabled(static_cast<size_t>(index)))
        return false;

    run(static_cast<size_t>(index));
    return true;
}

// Closed first, so the focus is back where it was when onCommand runs (a
// handler opening a dialog gives it back there).
void CommandPalette::run(size_t index)
{
    // a copy: a handler may change the commands
    const String id=m_commands[index].id;

    m_recent.erase(std::remove(m_recent.begin(), m_recent.end(), id), m_recent.end());
    m_recent.insert(m_recent.begin(), id);

    if(m_recent.size() > m_recentLimit)
        m_recent.resize(m_recentLimit);

    close();

    event::Info info(this);

    info.string=id;
    onCommand.call(this, info);
}

void CommandPalette::setRecentFirst(bool recentFirst)
{
    if(m_recentFirst == recentFirst)
        return;

    m_recentFirst=recentFirst;

    if(isOpen())
        filter();
}

void CommandPalette::setRecentLimit(size_t limit)
{
    m_recentLimit=limit;

    if(m_recent.size() > limit)
        m_recent.resize(limit);
}

void CommandPalette::clearRecent()
{
    m_recent.clear();
}

}//namespace controls
}//namespace gweni
