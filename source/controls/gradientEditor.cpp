/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/gradientEditor.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/colorEdit.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/inputHandler.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{
namespace internal
{

namespace
{

GradientEditor *editorOf(const Base *control)
{
    for(Base *parent=control->getParent(); parent; parent=parent->getParent())
    {
        if(auto *editor=gweni_cast<GradientEditor>(parent))
            return editor;
    }
    return nullptr;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(GradientBar)
{
    setMouseInputEnabled(false);
}

GradientEditor *GradientBar::getEditor() const
{
    return editorOf(this);
}


GWENI_CONTROL_CONSTRUCTOR(GradientStopStrip)
{
    setMouseInputEnabled(true);
}

GradientEditor *GradientStopStrip::getEditor() const
{
    return editorOf(this);
}

void GradientStopStrip::onMouseClickLeft(int x, int y, bool down)
{
    GradientEditor *editor=getEditor();

    if(!down || !editor || editor->isDisabled())
        return;

    const Point at=canvasPosToLocal(Point(x, y));
    const size_t index=editor->addStopAt(editor->stripPosition(at.x));

    // the new stop's handle has the press: it drags from its centre
    if(GradientStopHandle *handle=editor->getHandle(index))
    {
        handle->focus();
        editor->beginDrag(handle, GradientEditor::HandleWidth/2);
    }
}

void GradientStopStrip::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    GradientEditor *editor=getEditor();

    if(!editor)
        return;

    const Gradient *gradient=editor->getGradient();

    for(size_t i=0; i < editor->getHandleCount(); ++i)
    {
        placeChild(editor->getHandle(i), editor->handleLeft(gradient->stop(i).position), 0,
            GradientEditor::HandleWidth, getHeight());
    }
}


GWENI_CONTROL_CONSTRUCTOR(GradientStopHandle)
{
    setPlacement(Placement::Owner);
    setMouseInputEnabled(true);
    setKeyboardInputEnabled(true);
    setTabable(true);
    setCursor(cursor::SizeWE);
}

GradientEditor *GradientStopHandle::getEditor() const
{
    return editorOf(this);
}

void GradientStopHandle::setStopColor(const gweni::Color &color)
{
    if(color == m_color)
        return;

    m_color=color;
    addStateChange(StateChange_Background);
}

bool GradientStopHandle::isSelected() const
{
    GradientEditor *editor=getEditor();

    return editor && (editor->indexOf(this) == editor->getSelectedStop());
}

bool GradientStopHandle::isDepressed() const
{
    GradientEditor *editor=getEditor();

    return editor && (editor->getDraggedHandle() == this);
}

void GradientStopHandle::onMouseClickLeft(int x, int /*y*/, bool down)
{
    GradientEditor *editor=getEditor();

    if(!editor)
        return;

    if(!down)
    {
        if(editor->getDraggedHandle() == this)
            editor->endDrag();
        return;
    }

    if(editor->isDisabled())
        return;

    editor->beginDrag(this, canvasPosToLocal(Point(x, 0)).x);
}

void GradientStopHandle::onMouseDoubleClickLeft(int x, int y)
{
    onMouseClickLeft(x, y, true);

    GradientEditor *editor=getEditor();

    if(editor && !editor->isDisabled() && editor->getColorEdit() && !editor->getColorEdit()->isPickerOpen())
        editor->getColorEdit()->togglePicker();
}

void GradientStopHandle::onMouseMoved(int x, int /*y*/, int /*deltaX*/, int /*deltaY*/)
{
    GradientEditor *editor=getEditor();

    if(editor && (editor->getDraggedHandle() == this))
        editor->dragTo(x);
}

void GradientStopHandle::onKeyboardFocus()
{
    ParentClass::onKeyboardFocus();

    GradientEditor *editor=getEditor();

    if(editor && !editor->isDisabled())
    {
        const size_t index=editor->indexOf(this);

        if(index < editor->getHandleCount())
            editor->setSelectedStop(index, true);
    }
}

bool GradientStopHandle::onKeyLeft(bool down)
{
    GradientEditor *editor=getEditor();

    if(down && editor && !editor->isDisabled())
        editor->nudgeStop(editor->indexOf(this),
            -(input::isShiftDown()?GradientEditor::NudgeShiftStep:GradientEditor::NudgeStep));
    return true;
}

bool GradientStopHandle::onKeyRight(bool down)
{
    GradientEditor *editor=getEditor();

    if(down && editor && !editor->isDisabled())
        editor->nudgeStop(editor->indexOf(this),
            input::isShiftDown()?GradientEditor::NudgeShiftStep:GradientEditor::NudgeStep);
    return true;
}

bool GradientStopHandle::onKeyDelete(bool down)
{
    GradientEditor *editor=getEditor();

    if(down && editor && !editor->isDisabled())
        editor->deleteStop(editor->indexOf(this));
    return true;
}

bool GradientStopHandle::onKeyEscape(bool down)
{
    GradientEditor *editor=getEditor();

    // only a drag takes it: otherwise it goes on up (a dialog's cancel)
    if(!editor || !editor->isDragging())
        return false;
    if(down)
        editor->cancelDrag();
    return true;
}

}//namespace internal


GWENI_CONTROL_CONSTRUCTOR(GradientEditor)
{
    setSize(320, 0);
    setSizeFlags({SizeFlag::Fixed, SizeFlag::Elastic});

    // the bar first, then the strip and its handles, then the row: the focus
    // order of the handles and the fields
    m_bar=newChild<internal::GradientBar>();
    m_bar->setPlacement(Placement::Owner);

    m_strip=newChild<internal::GradientStopStrip>();
    m_strip->setPlacement(Placement::Owner);

    m_colorEdit=newChild<ColorEdit>();
    m_colorEdit->setPlacement(Placement::Owner);
    m_colorEdit->onColorChanged.add(this, &GradientEditor::onColorEditChanged);

    m_position=newChild<DragNumber>();
    m_position->setPlacement(Placement::Owner);
    m_position->setRange(0.0, 1.0);
    m_position->setSpeed(0.005);
    m_position->setFormat("%.3f");
    m_position->setWidth(PositionFieldWidth);
    m_position->onValueChanged.add(this, &GradientEditor::onPositionChanged);

    syncHandles();
    syncRow();
}

void GradientEditor::setGradient(Gradient *gradient)
{
    // a drag of the gradient it had ends with it
    if(isDragging())
        endDrag();

    m_gradient=gradient?gradient:&m_ownGradient;
    m_selected=0;
    gradientChanged();
}

void GradientEditor::gradientChanged()
{
    // the stops the press found are not the gradient's now
    if(isDragging())
        endDrag();
    m_selected=std::min(m_selected, m_gradient->stopCount()-1);
    syncHandles();
    syncRow();
    stopsMoved();
}

void GradientEditor::syncHandles()
{
    while(m_handles.size() < m_gradient->stopCount())
    {
        internal::GradientStopHandle *handle=m_strip->newChild<internal::GradientStopHandle>();

        applyHandleInput(handle);
        m_handles.push_back(handle);
    }
    while(m_handles.size() > m_gradient->stopCount())
    {
        internal::GradientStopHandle *handle=m_handles.back();

        m_handles.pop_back();
        if(m_drag.handle == handle)
            endDrag();
        handle->setHidden(true);
        handle->delayedDelete();
    }
    for(size_t i=0; i < m_handles.size(); ++i)
    {
        m_handles[i]->setStopColor(m_gradient->stop(i).color);
        // a selection that moved repaints the handles it left and took
        m_handles[i]->redraw();
    }
}

void GradientEditor::syncRow()
{
    const GradientStop &stop=m_gradient->stop(m_selected);

    m_syncing=true;
    m_colorEdit->setColor(stop.color, false);
    m_position->setNumber(stop.position, false);
    m_syncing=false;
}

void GradientEditor::stopsMoved()
{
    m_bar->addStateChange(StateChange_Background);
    m_strip->invalidate();
    // a handle placed under the resting mouse (an add, a nudge, a delete) is
    // the hovered one once laid out, so a second press there is the handle's
    if(Canvas *canvas=getCanvas())
        canvas->hoverMayHaveMoved();
}

void GradientEditor::applyHandleInput(internal::GradientStopHandle *handle)
{
    handle->setKeyboardInputEnabled(!isDisabled());
    handle->setTabable(!isDisabled());
}

void GradientEditor::setSelectedStop(size_t index, bool doEvents)
{
    select(index, doEvents);
}

void GradientEditor::select(size_t index, bool doEvents)
{
    index=std::min(index, m_gradient->stopCount()-1);
    if(index == m_selected)
        return;

    const size_t old=m_selected;

    m_selected=index;
    if(old < m_handles.size())
        m_handles[old]->redraw();
    m_handles[index]->redraw();
    syncRow();
    if(doEvents)
    {
        event::Info info(this);

        info.integer=m_selected;
        onSelectionChange.call(this, info);
    }
}

internal::GradientStopHandle *GradientEditor::getHandle(size_t index) const
{
    return (index < m_handles.size())?m_handles[index]:nullptr;
}

size_t GradientEditor::indexOf(const internal::GradientStopHandle *handle) const
{
    for(size_t i=0; i < m_handles.size(); ++i)
    {
        if(m_handles[i] == handle)
            return i;
    }
    return m_handles.size();
}

float GradientEditor::stripPosition(int stripX) const
{
    const int span=m_strip->getWidth()-HandleWidth;

    if(span <= 0)
        return 0.0f;
    return Gradient::clampPosition((float)(stripX-HandleWidth/2)/(float)span);
}

int GradientEditor::handleLeft(float t) const
{
    const int span=std::max(0, m_strip->getWidth()-HandleWidth);

    return (int)std::lround(t*(float)span);
}

void GradientEditor::fireChanged(size_t index)
{
    event::Info info(this);

    info.integer=index;
    onChanged.call(this, info);
}

bool GradientEditor::moveStop(size_t &index, float t)
{
    t=Gradient::clampPosition(t);
    if(index >= m_gradient->stopCount() || m_gradient->stop(index).position == t)
        return false;

    const size_t to=m_gradient->moveStop(index, t);

    if(to != index)
    {
        internal::GradientStopHandle *handle=m_handles[index];

        m_handles.erase(m_handles.begin()+(std::ptrdiff_t)index);
        m_handles.insert(m_handles.begin()+(std::ptrdiff_t)to, handle);
        // the strip's children stay in stop order, the order Tab goes in
        if(to+1 < m_handles.size())
            handle->bringNextToControl(m_handles[to+1], false);
        else
            handle->bringToFront();
    }
    if(m_selected == index)
        m_selected=to;
    else if((index < m_selected) && (to >= m_selected))
        --m_selected;
    else if((index > m_selected) && (to <= m_selected))
        ++m_selected;
    index=to;
    stopsMoved();
    return true;
}

void GradientEditor::userMove(size_t index, float t)
{
    if(!moveStop(index, t))
        return;

    if(index == m_selected)
    {
        m_syncing=true;
        m_position->setNumber(m_gradient->stop(index).position, false);
        m_syncing=false;
    }
    fireChanged(index);
}

size_t GradientEditor::addStopAt(float t)
{
    // the stops a drag's press found are not the gradient's now
    if(isDragging())
        endDrag();

    const size_t index=m_gradient->addStop(t);
    internal::GradientStopHandle *handle=m_strip->newChild<internal::GradientStopHandle>();

    applyHandleInput(handle);
    m_handles.insert(m_handles.begin()+(std::ptrdiff_t)index, handle);
    if(index+1 < m_handles.size())
        handle->bringNextToControl(m_handles[index+1], false);
    handle->setStopColor(m_gradient->stop(index).color);
    if(m_selected >= index)
        ++m_selected;
    select(index, true);
    stopsMoved();
    fireChanged(index);
    return index;
}

void GradientEditor::moveSelectedTo(float t)
{
    userMove(m_selected, t);
}

void GradientEditor::nudgeSelected(float delta)
{
    nudgeStop(m_selected, delta);
}

void GradientEditor::nudgeStop(size_t index, float delta)
{
    // a drag has the stop it moves
    if(isDragging() || (index >= m_gradient->stopCount()))
        return;

    select(index, true);
    userMove(index, m_gradient->stop(index).position+delta);
}

bool GradientEditor::deleteSelected()
{
    return deleteStop(m_selected);
}

bool GradientEditor::deleteStop(size_t index)
{
    // the gradient keeps its last stop, and a drag the stops it found
    if(isDragging() || (index >= m_gradient->stopCount()) || (m_gradient->stopCount() < 2))
        return false;

    select(index, true);
    m_gradient->removeStop(index);

    internal::GradientStopHandle *handle=m_handles[index];
    const bool focused=handle->isFocussed();

    m_handles.erase(m_handles.begin()+(std::ptrdiff_t)index);
    // the next stop, or the one before when it was the last
    m_selected=m_handles.size();
    select(std::min(index, m_handles.size()-1), true);
    if(focused)
        m_handles[m_selected]->focus();
    handle->setHidden(true);
    handle->delayedDelete();
    stopsMoved();
    fireChanged(m_selected);
    return true;
}

void GradientEditor::setSelectedColor(const gweni::Color &color)
{
    if(m_gradient->stop(m_selected).color == color)
        return;

    m_gradient->setStopColor(m_selected, color);
    m_handles[m_selected]->setStopColor(color);
    m_bar->addStateChange(StateChange_Background);
    if(!(m_colorEdit->getColor() == color))
    {
        m_syncing=true;
        m_colorEdit->setColor(color, false);
        m_syncing=false;
    }
    fireChanged(m_selected);
}

void GradientEditor::beginDrag(internal::GradientStopHandle *handle, int grab)
{
    const size_t index=indexOf(handle);

    if(index >= m_handles.size())
        return;

    if(isDragging())
        endDrag();
    select(index, true);
    m_drag.handle=handle;
    m_drag.stops=m_gradient->stops();
    m_drag.handles=m_handles;
    m_drag.grab=grab;
    m_drag.cancelled=false;
    gweni::MouseFocus=handle;
    handle->redraw();
}

void GradientEditor::dragTo(int x)
{
    if(!m_drag.handle || m_drag.cancelled)
        return;

    const Point at=m_strip->canvasPosToLocal(Point(x, 0));

    userMove(indexOf(m_drag.handle), stripPosition(at.x-m_drag.grab+HandleWidth/2));
}

void GradientEditor::endDrag()
{
    internal::GradientStopHandle *handle=m_drag.handle;

    if(!handle)
        return;

    m_drag=Drag();
    if(gweni::MouseFocus == handle)
        gweni::MouseFocus=nullptr;
    handle->redraw();
}

bool GradientEditor::cancelDrag()
{
    if(!m_drag.handle || m_drag.cancelled)
        return false;

    // the release still ends it; until then the mouse moves nothing
    m_drag.cancelled=true;
    if((m_gradient->stops() == m_drag.stops) && (m_handles == m_drag.handles))
        return true;

    // the stops and handles as the press found them, so two stops at one
    // position keep their order; the selection stays on its handle
    internal::GradientStopHandle *selected=m_handles[m_selected];
    const bool reordered=(m_handles != m_drag.handles);

    m_gradient->setStops(m_drag.stops);
    m_handles=m_drag.handles;
    if(reordered)
    {
        // the strip's children back in stop order, the order Tab goes in
        for(internal::GradientStopHandle *handle : m_handles)
            handle->bringToFront();
    }
    m_selected=indexOf(selected);
    for(size_t i=0; i < m_handles.size(); ++i)
        m_handles[i]->setStopColor(m_gradient->stop(i).color);
    syncRow();
    stopsMoved();
    fireChanged(indexOf(m_drag.handle));
    return true;
}

void GradientEditor::onColorEditChanged(event::Info /*info*/)
{
    if(m_syncing)
        return;
    setSelectedColor(m_colorEdit->getColor());
}

void GradientEditor::onPositionChanged(event::Info /*info*/)
{
    if(m_syncing)
        return;
    size_t index=m_selected;

    if(moveStop(index, (float)m_position->getNumber()))
        fireChanged(index);
}

void GradientEditor::setDisabled(bool active)
{
    if(active == isDisabled())
        return;

    if(active && isDragging())
        endDrag();
    ParentClass::setDisabled(active);
    m_colorEdit->setDisabled(active);
    m_position->setDisabled(active);
    for(internal::GradientStopHandle *handle : m_handles)
    {
        // disabled, a handle takes no focus or Tab, and loses one it had
        applyHandleInput(handle);
        if(active && handle->isFocussed())
            handle->unfocus();
        handle->redraw();
    }
}

void GradientEditor::calculateSize(skin::Base *skin, Dim dim)
{
    ParentClass::calculateSize(skin, dim);

    if(dim != Dim::Y)
        return;

    const Padding &padding=getPadding();
    const int row=std::max(m_colorEdit->getPreferredSize().height, m_position->getPreferredSize().height);

    m_preferredSize.height=preferredFromContent(padding.top+BarHeight+StripHeight+Gap+row+padding.bottom,
        Dim::Y);
}

void GradientEditor::layout(skin::Base *skin)
{
    ParentClass::layout(skin);

    const Padding &padding=getPadding();
    const int width=std::max(0, getWidth()-padding.left-padding.right);
    int y=padding.top;

    placeChild(m_bar, padding.left+HandleWidth/2, y, std::max(0, width-HandleWidth), BarHeight);
    y+=BarHeight;
    placeChild(m_strip, padding.left, y, width, StripHeight);
    y+=StripHeight+Gap;

    const Size edit=m_colorEdit->getPreferredSize();
    const Size field=m_position->getPreferredSize();
    const int row=std::max(edit.height, field.height);

    placeChild(m_colorEdit, padding.left, y+(row-edit.height)/2, edit.width, edit.height);
    placeChild(m_position, padding.left+std::max(0, width-field.width), y+(row-field.height)/2,
        field.width, field.height);
}

}//namespace controls
}//namespace gweni
