/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include <gweni/text/undoStack.h>
#include <gweni/platforms/platform.h>

namespace gweni
{
namespace text
{

UndoStack::UndoStack()
{}

bool UndoStack::joinsCurrent(const Edit &edit, EditKind kind, const Selection &before, float now) const
{
    if(m_sealed || m_current == 0 || m_current != m_steps.size())
        return false;

    const UndoStep &step=m_steps.back();

    if(step.kind != kind)
        return false;
    if(kind != EditKind::Insert && kind != EditKind::DeleteBackward && kind != EditKind::DeleteForward)
        return false;
    if(before != step.after)
        return false;
    if(edit.text.find('\n') != String::npos)
        return false;
    if(now-step.lastTime > m_interval)
        return false;
    return true;
}

void UndoStack::startStep(EditKind kind, const Selection &before)
{
    //a new edit drops what could be redone
    m_steps.erase(m_steps.begin()+m_current, m_steps.end());

    UndoStep step;
    step.kind=kind;
    step.before=before;
    m_steps.push_back(std::move(step));
    m_current=m_steps.size();
    m_sealed=false;
    applyLimit();
}

void UndoStack::record(const Edit &edit, EditKind kind, const Selection &before, const Selection &after)
{
    float now=platform::getTimeInSeconds();

    if(m_groupDepth > 0)
    {
        if(!m_groupStarted)
        {
            startStep(m_groupKind, before);
            m_groupStarted=true;
        }
    }
    else if(!joinsCurrent(edit, kind, before, now))
        startStep(kind, before);

    UndoStep &step=m_steps.back();
    step.edits.push_back(edit);
    step.after=after;
    step.lastTime=now;
}

void UndoStack::beginGroup(EditKind kind)
{
    if(m_groupDepth++ == 0)
    {
        m_groupKind=kind;
        m_groupStarted=false;
    }
}

void UndoStack::endGroup()
{
    if(m_groupDepth == 0)
        return;
    if(--m_groupDepth == 0 && m_groupStarted)
        m_sealed=true;
}

const UndoStep *UndoStack::undo()
{
    if(!canUndo())
        return nullptr;
    m_sealed=true;
    return &m_steps[--m_current];
}

const UndoStep *UndoStack::redo()
{
    if(!canRedo())
        return nullptr;
    m_sealed=true;
    return &m_steps[m_current++];
}

void UndoStack::clear()
{
    m_steps.clear();
    m_current=0;
    m_sealed=true;
    m_groupStarted=false;
}

void UndoStack::setLimit(size_t limit)
{
    m_limit=limit;
    applyLimit();
}

void UndoStack::applyLimit()
{
    if(m_limit == 0)
        return;

    //the oldest steps go first; past the limit with only steps to redo
    //left, the newest of those go
    while(m_steps.size() > m_limit && m_current > 0)
    {
        m_steps.pop_front();
        --m_current;
    }
    while(m_steps.size() > m_limit)
        m_steps.pop_back();
}

}//namespace text
}//namespace gweni
