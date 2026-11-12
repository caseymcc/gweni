/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_text_undostack_h_
#define _gweni_text_undostack_h_

#include <gweni/export.h>
#include <gweni/text/document.h>

#include <cstddef>
#include <vector>

namespace gweni
{
namespace text
{

//! What made an edit, for UndoStack's coalescing: typing (Insert) and the two
//! deletes join the step before them while the user keeps doing the same
//! thing; a Paste, a Cut or anything Other is a step of its own.
enum class EditKind
{
    Insert,
    DeleteBackward,
    DeleteForward,
    Paste,
    Cut,
    Other
};

//! A caret and the other end of its selection; anchor == caret is no
//! selection.
struct Selection
{
    Position anchor;
    Position caret;

    bool operator==(const Selection &other) const { return anchor == other.anchor && caret == other.caret; }
    bool operator!=(const Selection &other) const { return !(*this == other); }
};

//! One undo step: the edits it made, in the order they were applied, and the
//! selection before the first and after the last.
struct UndoStep
{
    std::vector<Edit> edits;
    Selection before;
    Selection after;
    EditKind kind=EditKind::Other;
    //! getTimeInSeconds() at its last edit, for the pause rule
    float lastTime=0.0f;
};

//! An undo history over applied Edit records (Document::apply()'s, or those a
//! control fills in for a String of its own: the stack only stores and
//! compares positions, so they can be counted either way).
//!
//! record() adds an edit that has been applied. It joins the current step
//! (typing coalesces) unless one of these starts a new one:
//! - the step was sealed (seal(), an undo or a redo since);
//! - its kind differs from the step's, or is Paste, Cut or Other;
//! - the selection before it is not the one the step left (the caret moved
//!   other than by the typing);
//! - it inserts a newline (each typed line is a step);
//! - more than interval() seconds of platform::getTimeInSeconds() passed
//!   since the step's last edit, so a capture's stepped clock makes this
//!   deterministic.
//! A new edit drops every step that could be redone. beginGroup() and
//! endGroup() make the edits between them part of one step (a paste over a
//! selection: the erase and the insert): the group's first edit joins the
//! current step or starts one by the rules above, as an edit of the group's
//! kind, and the rest join that step whatever they are. So a Paste group is
//! a step of its own, a letter typed over a selection starts one (the
//! selection before it is no caret the step left) that the next letters
//! join, and a letter typed at the caret through a group joins the typing
//! before it.
//!
//! undo() and redo() hand the step back for the caller to apply: undo by
//! applying each edit's inverse() from the last to the first and restoring
//! \p before, redo by applying the edits in order and restoring \p after.
class GWENI_EXPORT UndoStack
{
public:
    UndoStack();

    //! Adds \p edit, already applied, made by \p kind, with the selection
    //! before and after it.
    void record(const Edit &edit, EditKind kind, const Selection &before, const Selection &after);

    //! Every record() until the matching endGroup() goes into one step, the
    //! first by the rules as an edit of \p kind (a group with no record()
    //! changes nothing); groups nest, the outermost one counting.
    void beginGroup(EditKind kind=EditKind::Other);
    void endGroup();

    //! The next record() starts a new step.
    void seal() { m_sealed=true; }

    bool canUndo() const { return m_current > 0; }
    bool canRedo() const { return m_current < m_steps.size(); }
    //! The step to undo, now counted as undone, or null with none.
    const UndoStep *undo();
    //! The step to redo, now counted as done again, or null with none.
    const UndoStep *redo();

    //! The steps that can be undone and redone.
    size_t undoCount() const { return m_current; }
    size_t redoCount() const { return m_steps.size()-m_current; }

    //! Forgets every step.
    void clear();

    //! The most steps kept, the oldest dropped past it; 0 (the default) is
    //! no limit.
    void setLimit(size_t limit);
    size_t limit() const { return m_limit; }

    //! The pause in seconds after which typing starts a new step (1 s by
    //! default).
    void setInterval(float seconds) { m_interval=seconds; }
    float interval() const { return m_interval; }

private:
    bool joinsCurrent(const Edit &edit, EditKind kind, const Selection &before, float now) const;
    void startStep(EditKind kind, const Selection &before);
    void applyLimit();

    //! a vector, not a deque: an empty one allocates nothing, and every
    //! TextBox has one
    std::vector<UndoStep> m_steps;
    //! the steps below it are done, those from it on undone
    size_t m_current=0;
    bool m_sealed=true;
    size_t m_limit=0;
    float m_interval=1.0f;

    size_t m_groupDepth=0;
    EditKind m_groupKind=EditKind::Other;
    bool m_groupStarted=false;
};

//! Begins a group on construction and ends it on destruction.
class UndoGroup
{
public:
    UndoGroup(UndoStack &stack, EditKind kind=EditKind::Other): m_stack(stack) { m_stack.beginGroup(kind); }
    ~UndoGroup() { m_stack.endGroup(); }

    UndoGroup(const UndoGroup &)=delete;
    UndoGroup &operator=(const UndoGroup &)=delete;

private:
    UndoStack &m_stack;
};

}//namespace text
}//namespace gweni

#endif//_gweni_text_undostack_h_
