/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/segmentedButtons.h>
#include <gweni/controls/imagePanel.h>
#include <gweni/gweni.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cstdlib>

namespace gweni
{
namespace controls
{

///////////////////////////////////////////////////////////////////////////
//SegmentedButton

// Owner-placed by its row, as wide and high as its content: its Text has no
// padding of its own, the segment keeps the room around it.
GWENI_CONTROL_CONSTRUCTOR(SegmentedButton)
{
    setDock(DockPosition::None);
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    setTextPadding(Padding(0, 0, 0, 0));
    setDefaultTextAlignment(Alignment::Left|Alignment::CenterV);
    m_text->setPlacement(Placement::Owner);
}

// Its row reads its selection to move the exclusive one on, which it can
// only do before the Selectable part is destroyed (Base's destructor, which
// tells the row through onChildRemoved, runs after it).
SegmentedButton::~SegmentedButton()
{
    if(m_row)
        m_row->segmentDestroyed(this);
}

void SegmentedButton::setText(const String &str, bool doEvents)
{
    ParentClass::setText(str, doEvents);
    if(m_imageOnly)
        setToolTipText(str);
}

void SegmentedButton::setSelected(bool selected, bool doEvents)
{
    if(!selected && m_row && (m_row->getMode() == SegmentedButtons::Mode::Exclusive) && !m_row->allowsNone()
        && (m_row->m_group.getSelected() == this))
        return;

    ParentClass::setSelected(selected, doEvents);
}

bool SegmentedButton::onKeyReturn(bool down)
{
    if(m_row && (m_row->getMode() == SegmentedButtons::Mode::Exclusive))
        return false;
    return ParentClass::onKeyReturn(down);
}

void SegmentedButton::setImage(const String &name, bool center)
{
    const bool had=(m_image != nullptr);

    ParentClass::setImage(name, center);
    // Button's padding for its image is the segment's to keep
    setTextPadding(Padding(0, 0, 0, 0));
    if(m_image)
    {
        m_image->setPlacement(Placement::Owner);
        m_image->setMargin(Margin(0, 0, 0, 0));
        m_image->setMouseInputEnabled(false);
    }
    if(had != (m_image != nullptr))
        invalidate();
}

void SegmentedButton::setImageOnly(bool imageOnly)
{
    if(imageOnly == m_imageOnly)
        return;

    m_imageOnly=imageOnly;
    m_text->setHidden(imageOnly);
    setToolTipText(imageOnly?getText():String());
    invalidate();
}

void SegmentedButton::setCheckRoom(bool room)
{
    if(room == m_checkRoom)
        return;

    m_checkRoom=room;
    invalidate();
    redraw();
}

bool SegmentedButton::isDisabled()
{
    return ParentClass::isDisabled() || (m_row && m_row->isDisabled());
}

void SegmentedButton::updateColors()
{
    const auto &colors=getSkin()->Colors.SegmentedButton;

    if(isDisabled())
        setTextColor(colors.Disabled);
    else if(isSelected())
        setTextColor(colors.Selected);
    else if(isHovered())
        setTextColor(colors.Hover);
    else
        setTextColor(colors.Normal);
}

bool SegmentedButton::isTabable()
{
    if(!m_row)
        return ParentClass::isTabable();
    return m_row->getTabStop() == this;
}

bool SegmentedButton::textShows() const
{
    return !m_imageOnly && !m_text->getText().empty();
}

int SegmentedButton::imageWidth() const
{
    return m_image?m_image->textureWidth():0;
}

int SegmentedButton::imageHeight() const
{
    return m_image?m_image->textureHeight():0;
}

int SegmentedButton::contentWidth() const
{
    int width=m_checkRoom?CheckSize+CheckGap:0;

    if(m_image)
        width+=imageWidth()+(textShows()?ImageGap:0);
    if(textShows())
        width+=m_text->getPreferredSize().width;
    return width;
}

void SegmentedButton::calculateSize(skin::Base *skin, Dim dim)
{
    // the Text and the image are measured for themselves; neither counts
    ParentClass::calculateSize(skin, dim);

    if(dim == Dim::X)
    {
        m_preferredSize.width=preferredFromContent(SidePadding*2+contentWidth(), Dim::X);
        return;
    }

    int height=std::max(m_checkRoom?CheckSize:0, imageHeight());

    if(textShows())
        height=std::max(height, m_text->getPreferredSize().height);
    m_preferredSize.height=preferredFromContent(PaddingY*2+height, Dim::Y);
}

// The check's room, the image and the text, as one group centred in the
// segment (it is wider than its content in a row of equal widths).
void SegmentedButton::arrangeHorizontal(skin::Base *skin)
{
    ParentClass::arrangeHorizontal(skin);

    int x=std::max(SidePadding, (getWidth()-contentWidth())/2);
    const Rect check=m_checkRoom?Rect(x, m_checkRect.y, CheckSize, CheckSize):Rect();

    // the check is the skin's to draw where this says: a move within the
    // same bounds (a new text in a row of equal widths) is a new look
    if(!(check == m_checkRect))
    {
        m_checkRect=check;
        addStateChange(StateChange_Background);
    }
    if(m_checkRoom)
        x+=CheckSize+CheckGap;
    if(m_image)
    {
        placeChild(m_image, x, m_image->getY(), imageWidth(), imageHeight());
        x+=imageWidth()+(textShows()?ImageGap:0);
    }
    placeChild(m_text, x, m_text->getY(), textShows()?m_text->getPreferredSize().width:0, m_text->getHeight());
}

// Each part centred on the segment's height.
void SegmentedButton::arrangeVertical(skin::Base *skin)
{
    ParentClass::arrangeVertical(skin);

    const int height=getHeight();

    if(m_checkRoom && (m_checkRect.y != (height-CheckSize)/2))
    {
        m_checkRect.y=(height-CheckSize)/2;
        addStateChange(StateChange_Background);
    }
    if(m_image)
        placeChild(m_image, m_image->getX(), (height-imageHeight())/2, m_image->getWidth(), imageHeight());

    const int textHeight=m_text->getPreferredSize().height;

    placeChild(m_text, m_text->getX(), (height-textHeight)/2, m_text->getWidth(), textHeight);
}

///////////////////////////////////////////////////////////////////////////
//SegmentedButtons

// As large as its segments, wherever it is put.
GWENI_CONTROL_CONSTRUCTOR(SegmentedButtons)
{
    setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
}

// The segments are deleted by Base's destructor, after the row's own part is
// gone: none may ask it anything by then.
SegmentedButtons::~SegmentedButtons()
{
    for(SegmentedButton *segment : m_segments)
        segment->m_row=nullptr;
}

SegmentedButton *SegmentedButtons::addSegment(const String &text, const String &name)
{
    SegmentedButton *segment=newChild<SegmentedButton>();

    segment->setPlacement(Placement::Owner);
    segment->setName(name);
    segment->setText(text);
    segment->m_row=this;
    segment->setCheckRoom(m_showCheck);
    segment->onSelect.add(this, &SegmentedButtons::onSegmentSelect);
    segment->onDeselect.add(this, &SegmentedButtons::onSegmentDeselect);
    m_segments.push_back(segment);

    if(m_mode == Mode::Exclusive)
    {
        m_group.add(segment);
        segment->setToggleable(m_allowNone);
        ensureSelection(segmentCount()-1);
    }
    segment->updateColors();
    restyleSegments();
    invalidate();
    return segment;
}

SegmentedButton *SegmentedButtons::addSegment(const String &text, const String &image, bool imageOnly,
    const String &name)
{
    SegmentedButton *segment=addSegment(text, name);

    segment->setImage(image);
    segment->setImageOnly(imageOnly);
    return segment;
}

void SegmentedButtons::removeSegment(int index)
{
    SegmentedButton *segment=getSegment(index);

    if(!segment)
        return;

    const bool wasSelected=segment->isSelected();

    // Out of the row at once, deleted with the canvas's delayed deletes: this
    // may be called from the segment's own click (an onSelectionChange
    // handler), which goes on in Button::onPress once the handler returns.
    // Hidden meanwhile, so it takes no input and no room, and out of the
    // group, so the selection can move on now. Its handlers stay (a Caller
    // must not lose one while it calls it); with m_row null they ignore it.
    m_segments.erase(m_segments.begin()+index);
    segment->m_row=nullptr;
    m_group.remove(segment);
    segment->setHidden(true);
    segment->delayedDelete();

    if(wasSelected && (m_mode == Mode::Exclusive))
        ensureSelection(index);
    restyleSegments();
}

void SegmentedButtons::removeSegment(SegmentedButton *segment)
{
    removeSegment(indexOf(segment));
}

SegmentedButton *SegmentedButtons::getSegment(int index) const
{
    if(index < 0 || index >= segmentCount())
        return nullptr;
    return m_segments[static_cast<size_t>(index)];
}

int SegmentedButtons::indexOf(const Base *segment) const
{
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i] == segment)
            return static_cast<int>(i);
    }
    return -1;
}

void SegmentedButtons::setSegmentImage(int index, const String &image, bool imageOnly)
{
    if(SegmentedButton *segment=getSegment(index))
    {
        segment->setImage(image);
        segment->setImageOnly(imageOnly);
    }
}

void SegmentedButtons::setMode(Mode mode)
{
    if(mode == m_mode)
        return;

    m_mode=mode;
    if(mode == Mode::Multi)
    {
        for(SegmentedButton *segment : m_segments)
            m_group.remove(segment);
        return;
    }

    // the first selected one stays, before the group sees any of them
    bool kept=false;

    for(SegmentedButton *segment : m_segments)
    {
        if(!segment->isSelected())
            continue;
        if(kept)
            segment->setSelected(false);
        kept=true;
    }
    for(SegmentedButton *segment : m_segments)
    {
        m_group.add(segment);
        segment->setToggleable(m_allowNone);
    }
    ensureSelection();
}

void SegmentedButtons::setAllowNone(bool allow)
{
    if(allow == m_allowNone)
        return;

    m_allowNone=allow;
    if(m_mode != Mode::Exclusive)
        return;

    for(SegmentedButton *segment : m_segments)
        segment->setToggleable(allow);
    ensureSelection();
}

void SegmentedButtons::setSelected(int index, bool selected)
{
    SegmentedButton *segment=getSegment(index);

    // the segment refuses the exclusive deselect itself
    if(segment)
        segment->setSelected(selected);
}

bool SegmentedButtons::isSelected(int index) const
{
    const SegmentedButton *segment=getSegment(index);

    return segment && segment->isSelected();
}

int SegmentedButtons::getSelectedIndex() const
{
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i]->isSelected())
            return static_cast<int>(i);
    }
    return -1;
}

std::vector<int> SegmentedButtons::getSelectedIndices() const
{
    std::vector<int> indices;

    for(size_t i=0; i < m_segments.size(); ++i)
    {
        if(m_segments[i]->isSelected())
            indices.push_back(static_cast<int>(i));
    }
    return indices;
}

void SegmentedButtons::setEqualWidths(bool equal)
{
    if(equal == m_equalWidths)
        return;

    m_equalWidths=equal;
    invalidate();
}

void SegmentedButtons::setShowCheck(bool show)
{
    if(show == m_showCheck)
        return;

    m_showCheck=show;
    for(SegmentedButton *segment : m_segments)
        segment->setCheckRoom(show);
}

void SegmentedButtons::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    ParentClass::setDisabled(active);
    // each segment's colours follow the row's state
    for(SegmentedButton *segment : m_segments)
        segment->redraw();
}

// Exclusive: the segment the selection moved to fires, the one it left does
// not (the group's selection is already the new one when it is deselected).
void SegmentedButtons::onSegmentSelect(event::Info info)
{
    if(!isOwnSegment(info.controlCaller))
        return;
    onSelectionChange.call(this, event::Info(info.controlCaller));
}

void SegmentedButtons::onSegmentDeselect(event::Info info)
{
    if(!isOwnSegment(info.controlCaller))
        return;
    if((m_mode == Mode::Exclusive) && m_group.getSelected())
        return;

    onSelectionChange.call(this, event::Info(info.controlCaller));
}

void SegmentedButtons::ensureSelection(int from)
{
    if((m_mode != Mode::Exclusive) || m_allowNone || m_segments.empty() || m_group.getSelected())
        return;

    const int count=segmentCount();

    from=std::min(std::max(from, 0), count-1);
    for(int i=from; i < count; ++i)
    {
        if(!m_segments[static_cast<size_t>(i)]->isDisabled())
        {
            m_segments[static_cast<size_t>(i)]->setSelected(true);
            return;
        }
    }
    for(int i=from-1; i >= 0; --i)
    {
        if(!m_segments[static_cast<size_t>(i)]->isDisabled())
        {
            m_segments[static_cast<size_t>(i)]->setSelected(true);
            return;
        }
    }
    // every segment disabled: one is still selected
    m_segments[static_cast<size_t>(from)]->setSelected(true);
}

void SegmentedButtons::segmentDestroyed(SegmentedButton *segment)
{
    const int index=indexOf(segment);

    if(index < 0)
        return;

    const bool wasSelected=segment->isSelected();

    m_segments.erase(m_segments.begin()+index);
    segment->m_row=nullptr;
    m_group.remove(segment);
    if(wasSelected && (m_mode == Mode::Exclusive))
        ensureSelection(index);
    restyleSegments();
}

// A segment moved to another parent leaves the row: its handlers, its
// group, its owner placement and the check's room go with it (a deleted one
// has left from its destructor already).
void SegmentedButtons::onChildRemoved(Base *child)
{
    ParentClass::onChildRemoved(child);

    const int index=indexOf(child);

    if(index < 0)
        return;

    SegmentedButton *segment=m_segments[static_cast<size_t>(index)];

    segmentDestroyed(segment);
    segment->onSelect.removeHandler(this);
    segment->onDeselect.removeHandler(this);
    segment->setPlacement(Placement::Parent);
    segment->setCheckRoom(false);
}

bool SegmentedButtons::isOwnSegment(const Base *control) const
{
    // only segments call the handlers that ask
    const SegmentedButton *segment=static_cast<const SegmentedButton *>(control);

    return segment && segment->m_row == this;
}

void SegmentedButtons::restyleSegments()
{
    for(SegmentedButton *segment : m_segments)
        segment->redraw();
}

bool SegmentedButtons::canFocus(SegmentedButton *segment)
{
    return segment && !segment->isDisabled() && !segment->hidden();
}

SegmentedButton *SegmentedButtons::getTabStop() const
{
    const int focused=indexOf(KeyboardFocus);

    if((focused >= 0) && canFocus(m_segments[static_cast<size_t>(focused)]))
        return m_segments[static_cast<size_t>(focused)];

    if(m_mode == Mode::Exclusive)
    {
        const int selected=getSelectedIndex();

        if((selected >= 0) && canFocus(m_segments[static_cast<size_t>(selected)]))
            return m_segments[static_cast<size_t>(selected)];
    }
    for(SegmentedButton *segment : m_segments)
    {
        if(canFocus(segment))
            return segment;
    }
    return nullptr;
}

bool SegmentedButtons::focusSegment(SegmentedButton *segment)
{
    if(!segment)
        return false;

    segment->focus();
    if(m_mode == Mode::Exclusive)
        segment->setSelected(true);
    return true;
}

bool SegmentedButtons::moveFocus(int steps)
{
    SegmentedButton *from=getTabStop();

    if(isDisabled() || !from)
        return false;

    const int direction=(steps < 0)?-1:1;
    int index=indexOf(from);
    SegmentedButton *target=from;

    for(int left=std::abs(steps); left > 0; --left)
    {
        int next=index+direction;

        while((next >= 0) && (next < segmentCount()) && !canFocus(m_segments[static_cast<size_t>(next)]))
            next+=direction;
        if((next < 0) || (next >= segmentCount()))
            break;
        index=next;
        target=m_segments[static_cast<size_t>(index)];
    }
    return focusSegment(target);
}

bool SegmentedButtons::moveFocusToEnd(bool last)
{
    if(isDisabled())
        return false;

    for(int i=0; i < segmentCount(); ++i)
    {
        SegmentedButton *segment=m_segments[static_cast<size_t>(last?segmentCount()-1-i:i)];

        if(canFocus(segment))
            return focusSegment(segment);
    }
    return false;
}

// Each key on its press; a key the row does not take goes on up.
bool SegmentedButtons::onKeyLeft(bool down)
{
    return down?moveFocus(-1):!isDisabled();
}

bool SegmentedButtons::onKeyRight(bool down)
{
    return down?moveFocus(1):!isDisabled();
}

bool SegmentedButtons::onKeyUp(bool down)
{
    return onKeyLeft(down);
}

bool SegmentedButtons::onKeyDown(bool down)
{
    return onKeyRight(down);
}

bool SegmentedButtons::onKeyHome(bool down)
{
    return down?moveFocusToEnd(false):!isDisabled();
}

bool SegmentedButtons::onKeyend(bool down)
{
    return down?moveFocusToEnd(true):!isDisabled();
}

int SegmentedButtons::segmentPositions(const std::vector<int> &widths, std::vector<int> &x)
{
    int right=0;

    x.resize(widths.size());
    for(size_t i=0; i < widths.size(); ++i)
    {
        x[i]=(i == 0)?0:right-Joint;
        right=x[i]+widths[i];
    }
    return right;
}

void SegmentedButtons::segmentWidths(std::vector<int> &widths) const
{
    int widest=0;

    widths.resize(m_segments.size());
    for(size_t i=0; i < m_segments.size(); ++i)
    {
        widths[i]=m_segments[i]->getPreferredSize().width;
        widest=std::max(widest, widths[i]);
    }
    if(m_equalWidths)
        std::fill(widths.begin(), widths.end(), widest);
}

void SegmentedButtons::calculateSize(skin::Base *skin, Dim dim)
{
    // the segments are measured for their own subtrees; none counts
    ParentClass::calculateSize(skin, dim);

    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        std::vector<int> widths, x;

        segmentWidths(widths);
        m_preferredSize.width=preferredFromContent(segmentPositions(widths, x)+padding.left+padding.right,
            Dim::X);
        return;
    }

    int height=0;

    for(const SegmentedButton *segment : m_segments)
        height=std::max(height, segment->getPreferredSize().height);
    m_preferredSize.height=preferredFromContent(height+padding.top+padding.bottom, Dim::Y);
}

// The segments are placed before Base's arrange enters them, not from
// arrangeHorizontal/arrangeVertical: from MinChangedWalk children on, Base's
// arrange walks only the changed ones and calls neither (Flow's pattern). A
// segment given new bounds here is marked, so that walk enters it.
void SegmentedButtons::arrange(skin::Base *skin, Dim dim)
{
    const Padding &padding=getPadding();

    if(dim == Dim::X)
    {
        std::vector<int> widths, x;

        segmentWidths(widths);
        segmentPositions(widths, x);
        for(size_t i=0; i < m_segments.size(); ++i)
        {
            SegmentedButton *segment=m_segments[i];

            placeChild(segment, padding.left+x[i], segment->getY(), widths[i], segment->getHeight());
        }
    }
    else
    {
        // every segment the tallest one's height, at the top padding, however
        // tall the row's dock or layout makes it
        int height=0;

        for(const SegmentedButton *segment : m_segments)
            height=std::max(height, segment->getPreferredSize().height);

        for(SegmentedButton *segment : m_segments)
            placeChild(segment, segment->getX(), padding.top, segment->getWidth(), height);
    }

    ParentClass::arrange(skin, dim);
}

}//namespace controls
}//namespace gweni
