/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  text::UndoStack over a text::Document. Seeded random sequences of typing,
 *  newlines, backspaces, forward deletes, pastes and cuts, at a caret that
 *  sometimes jumps or holds a selection and on a clock that sometimes
 *  pauses past the interval, with a few steps undone and redone along the
 *  way: undoing every step gives the original text, every step's text
 *  before it on the way down, redoing every step gives each step's text
 *  after it on the way up to the final one, and each undo and redo hands
 *  back the selection recorded before and after its step. Then one case per
 *  coalescing rule: typing joins one step, and a caret move, a newline, a
 *  change of kind, a paste or cut (each its own step), a pause past the
 *  interval (platform::setTimeSource) and seal() each start a new one, as
 *  does an edit after an undo, which also drops the redo; a group is one
 *  step (a Paste one its own, an Insert one joining and joined by typing
 *  at the caret, an empty one nothing); setLimit() drops the
 *  oldest steps; clear() forgets them all.
 */
#include <gweni/text/undoStack.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

#include <cstdint>
#include <random>
#include <vector>

using namespace gweni;
using text::Document;
using text::Edit;
using text::EditKind;
using text::Position;
using text::Range;
using text::Selection;
using text::UndoStack;
using text::UndoStep;

namespace
{

//! A stepped clock for getTimeInSeconds(), restored on destruction.
struct Clock
{
    float now=0.0f;

    Clock() { platform::setTimeSource([this]() { return now; }); }
    ~Clock() { platform::setTimeSource(nullptr); }
};

Selection at(Position caret) { return {caret, caret}; }

bool hasSelection(const Selection &selection) { return selection.anchor != selection.caret; }

Range rangeOf(const Selection &selection)
{
    if(selection.caret < selection.anchor)
        return {selection.caret, selection.anchor};
    return {selection.anchor, selection.caret};
}

//! A Document edited as a text box edits it: a selection, and each edit
//! recorded on the stack with the selection before and after it.
struct Editor
{
    Document document;
    UndoStack stack;
    Selection selection;

    explicit Editor(const String &text=String()): document(text) {}

    //! Replaces \p range with \p text, the caret after it.
    void edit(Range range, const String &text, EditKind kind)
    {
        Selection before=selection;
        Edit edit=document.replace(range, text);

        selection=at(edit.textEnd);
        stack.record(edit, kind, before, selection);
    }

    void type(const String &text) { edit(rangeOf(selection), text, EditKind::Insert); }
    void paste(const String &text) { edit(rangeOf(selection), text, EditKind::Paste); }
    void cut()
    {
        if(hasSelection(selection))
            edit(rangeOf(selection), String(), EditKind::Cut);
    }

    //! false when there is nothing to delete
    bool backspace()
    {
        Range range=rangeOf(selection);

        if(range.empty())
        {
            if(range.begin.byte > 0)
                --range.begin.byte;
            else if(range.begin.line > 0)
                range.begin={range.begin.line-1, document.line(range.begin.line-1).size()};
            else
                return false;
        }
        edit(range, String(), EditKind::DeleteBackward);
        return true;
    }

    bool deleteForward()
    {
        Range range=rangeOf(selection);

        if(range.empty())
        {
            if(range.end.byte < document.line(range.end.line).size())
                ++range.end.byte;
            else if(range.end.line+1 < document.lineCount())
                range.end={range.end.line+1, 0};
            else
                return false;
        }
        edit(range, String(), EditKind::DeleteForward);
        return true;
    }

    void moveTo(Position caret) { selection=at(document.clamp(caret)); }

    //! Undoes one step: the inverses from the last edit to the first.
    const UndoStep *undo()
    {
        const UndoStep *step=stack.undo();

        if(step == nullptr)
            return nullptr;
        for(size_t i=step->edits.size(); i-- > 0;)
        {
            Edit inverse=step->edits[i].inverse();
            document.apply(inverse);
            REQUIRE(inverse.removed == step->edits[i].text);
        }
        selection=step->before;
        return step;
    }

    const UndoStep *redo()
    {
        const UndoStep *step=stack.redo();

        if(step == nullptr)
            return nullptr;
        for(const Edit &edit:step->edits)
        {
            Edit again=edit;
            document.apply(again);
            REQUIRE(again.removed == edit.removed);
            REQUIRE(again.textEnd == edit.textEnd);
        }
        selection=step->after;
        return step;
    }
};

Position randomPosition(std::mt19937 &random, const Document &document)
{
    size_t line=random()%document.lineCount();
    return {line, random()%(document.line(line).size()+1)};
}

const char *const Typed[]={"a", "b", "c", " ", "\xc3\xa9", "x"};
const char *const Pasted[]={"pasted", "two\nlines", "\n", "a\nb\nc", "", "\xe2\x82\xac"};

//! What the random test expects of each step on the stack, bottom first.
struct Expected
{
    String before;
    String after;
    Selection selectionBefore;
    Selection selectionAfter;
};

}//namespace

TEST_CASE("text::UndoStack: random edits undone to the original text and redone to the final one, selections restored")
{
    Clock clock;
    size_t joined=0;
    size_t started=0;

    for(uint32_t seed=1; seed <= 40; ++seed)
    {
        CAPTURE(seed);
        std::mt19937 random(seed);

        const String original=(seed%3 == 0)?String():String("first line\nsecond\n\nfourth");
        Editor editor(original);
        std::vector<Expected> steps;

        for(int action=0; action < 300; ++action)
        {
            CAPTURE(action);

            clock.now+=(random()%8 == 0)?1.5f:0.1f;

            unsigned roll=random()%100;

            if(roll < 6)
            {
                //a few steps back, some of them forward again; the next
                //edit drops what is left to redo
                size_t back=1+random()%3;
                for(size_t i=0; i < back && editor.stack.canUndo(); ++i)
                {
                    size_t index=editor.stack.undoCount()-1;
                    const UndoStep *step=editor.undo();
                    REQUIRE(step != nullptr);
                    REQUIRE(editor.document.text() == steps[index].before);
                    REQUIRE(step->before == steps[index].selectionBefore);
                }
                size_t forward=random()%(back+1);
                for(size_t i=0; i < forward && editor.stack.canRedo(); ++i)
                {
                    size_t index=editor.stack.undoCount();
                    const UndoStep *step=editor.redo();
                    REQUIRE(step != nullptr);
                    REQUIRE(editor.document.text() == steps[index].after);
                    REQUIRE(step->after == steps[index].selectionAfter);
                }
                continue;
            }
            if(roll < 20)
            {
                //the caret jumps, sometimes holding a selection
                editor.moveTo(randomPosition(random, editor.document));
                if(random()%2 == 0)
                    editor.selection.anchor=randomPosition(random, editor.document);
                continue;
            }

            String textBefore=editor.document.text();
            Selection selectionBefore=editor.selection;
                size_t undoBefore=editor.stack.undoCount();
            bool edited=true;

            if(roll < 60)
                editor.type(Typed[random()%(sizeof(Typed)/sizeof(Typed[0]))]);
            else if(roll < 66)
                editor.type("\n");
            else if(roll < 78)
                edited=editor.backspace();
            else if(roll < 88)
                edited=editor.deleteForward();
            else if(roll < 95)
                editor.paste(Pasted[random()%(sizeof(Pasted)/sizeof(Pasted[0]))]);
            else
            {
                edited=hasSelection(editor.selection);
                editor.cut();
            }
            if(!edited)
                continue;

            REQUIRE(editor.stack.redoCount() == 0);
            if(editor.stack.undoCount() == undoBefore+1)
            {
                //a new step: what could be redone is gone
                ++started;
                steps.resize(undoBefore);
                steps.push_back({textBefore, editor.document.text(), selectionBefore, editor.selection});
            }
            else
            {
                REQUIRE(editor.stack.undoCount() == undoBefore);
                ++joined;
                REQUIRE(steps.size() >= undoBefore);
                steps.resize(undoBefore);
                steps.back().after=editor.document.text();
                steps.back().selectionAfter=editor.selection;
            }
        }

        //the sequence may have ended on an undo, with steps left to redo
        const String final=steps.empty()?original:steps.back().after;
        REQUIRE(editor.stack.undoCount()+editor.stack.redoCount() == steps.size());
        if(editor.stack.redoCount() == 0)
            REQUIRE(editor.document.text() == final);

        //every step undone, newest first
        while(editor.stack.canUndo())
        {
            size_t index=editor.stack.undoCount()-1;
            const UndoStep *step=editor.undo();
            REQUIRE(editor.document.text() == steps[index].before);
            REQUIRE(step->before == steps[index].selectionBefore);
            REQUIRE(editor.selection == steps[index].selectionBefore);
        }
        REQUIRE(editor.document.text() == original);
        REQUIRE(editor.undo() == nullptr);

        //and every step redone
        while(editor.stack.canRedo())
        {
            size_t index=editor.stack.undoCount();
            const UndoStep *step=editor.redo();
            REQUIRE(editor.document.text() == steps[index].after);
            REQUIRE(step->after == steps[index].selectionAfter);
        }
        REQUIRE(editor.document.text() == final);
        REQUIRE(editor.redo() == nullptr);
    }

    //the sequences both join edits into steps and start new ones
    CAPTURE(joined);
    CAPTURE(started);
    REQUIRE(joined > 1000);
    REQUIRE(started > 1000);
}

TEST_CASE("text::UndoStack: typing at the caret joins one step")
{
    Clock clock;
    Editor editor;

    for(const char *c:{"h", "e", "l", "l", "o"})
    {
        clock.now+=0.2f;
        editor.type(c);
    }
    REQUIRE(editor.document.text() == "hello");
    REQUIRE(editor.stack.undoCount() == 1);

    //backspaces join a step of their own, as do forward deletes
    editor.backspace();
    editor.backspace();
    REQUIRE(editor.stack.undoCount() == 2);
    editor.moveTo({0, 0});
    editor.deleteForward();
    editor.deleteForward();
    REQUIRE(editor.document.text() == "l");
    REQUIRE(editor.stack.undoCount() == 3);

    const UndoStep *step=editor.undo();
    REQUIRE(step->edits.size() == 2);
    REQUIRE(editor.document.text() == "hel");
    REQUIRE(editor.selection == at({0, 0}));
    editor.undo();
    REQUIRE(editor.document.text() == "hello");
    REQUIRE(editor.selection == at({0, 5}));
    step=editor.undo();
    REQUIRE(step->edits.size() == 5);
    REQUIRE(step->before == at({0, 0}));
    REQUIRE(step->after == at({0, 5}));
    REQUIRE(editor.document.text().empty());
}

TEST_CASE("text::UndoStack: a caret move starts a new step")
{
    Clock clock;
    Editor editor;

    editor.type("a");
    editor.type("b");
    editor.moveTo({0, 0});
    editor.type("c");
    REQUIRE(editor.document.text() == "cab");
    REQUIRE(editor.stack.undoCount() == 2);

    //a selection made where the caret already was is a move too
    editor.selection.anchor={0, 0};
    editor.type("d");
    REQUIRE(editor.document.text() == "dab");
    REQUIRE(editor.stack.undoCount() == 3);

    editor.undo();
    REQUIRE(editor.document.text() == "cab");
    REQUIRE(editor.selection == Selection{{0, 0}, {0, 1}});
}

TEST_CASE("text::UndoStack: a typed newline starts a new step")
{
    Clock clock;
    Editor editor;

    editor.type("a");
    editor.type("b");
    editor.type("\n");
    editor.type("c");
    REQUIRE(editor.document.text() == "ab\nc");
    REQUIRE(editor.stack.undoCount() == 2);

    editor.undo();
    REQUIRE(editor.document.text() == "ab");
    REQUIRE(editor.selection == at({0, 2}));
    editor.undo();
    REQUIRE(editor.document.text().empty());
}

TEST_CASE("text::UndoStack: a change of kind starts a new step")
{
    Clock clock;
    Editor editor;

    editor.type("a");
    editor.type("b");
    editor.backspace();
    editor.type("c");
    REQUIRE(editor.document.text() == "ac");
    REQUIRE(editor.stack.undoCount() == 3);

    editor.moveTo({0, 0});
    editor.deleteForward();
    editor.backspace();
    REQUIRE(editor.stack.undoCount() == 4);
    //a backspace at the start of the text deletes nothing and records nothing
    REQUIRE(editor.document.text() == "c");
}

TEST_CASE("text::UndoStack: a paste and a cut are each a step of their own")
{
    Clock clock;
    Editor editor;

    editor.paste("one");
    editor.paste("two");
    REQUIRE(editor.stack.undoCount() == 2);

    editor.selection={{0, 0}, {0, 3}};
    editor.cut();
    editor.selection={{0, 0}, {0, 1}};
    editor.cut();
    REQUIRE(editor.document.text() == "wo");
    REQUIRE(editor.stack.undoCount() == 4);

    //typing after a paste is a new step too
    editor.moveTo({0, 2});
    editor.paste("!");
    editor.type("?");
    REQUIRE(editor.stack.undoCount() == 6);

    editor.undo();
    editor.undo();
    editor.undo();
    REQUIRE(editor.document.text() == "two");
    REQUIRE(editor.selection == Selection{{0, 0}, {0, 1}});
}

TEST_CASE("text::UndoStack: a pause past the interval starts a new step")
{
    Clock clock;
    Editor editor;

    REQUIRE(editor.stack.interval() == 1.0f);

    clock.now=10.0f;
    editor.type("a");
    clock.now=10.75f;
    editor.type("b");
    //0.75 s, then 1 s exactly: still the same step
    clock.now=11.75f;
    editor.type("c");
    REQUIRE(editor.stack.undoCount() == 1);

    clock.now=12.875f;
    editor.type("d");
    REQUIRE(editor.stack.undoCount() == 2);

    editor.stack.setInterval(0.25f);
    clock.now=13.0f;
    editor.type("e");
    REQUIRE(editor.stack.undoCount() == 2);
    clock.now=13.5f;
    editor.type("f");
    REQUIRE(editor.stack.undoCount() == 3);

    editor.undo();
    editor.undo();
    REQUIRE(editor.document.text() == "abc");
}

TEST_CASE("text::UndoStack: seal() starts a new step")
{
    Clock clock;
    Editor editor;

    editor.type("a");
    editor.stack.seal();
    editor.type("b");
    REQUIRE(editor.stack.undoCount() == 2);
}

TEST_CASE("text::UndoStack: an edit after an undo starts a new step and drops the redo")
{
    Clock clock;
    Editor editor;

    editor.type("a");
    editor.type("b");
    editor.type("\n");
    editor.type("c");
    editor.undo();
    REQUIRE(editor.stack.redoCount() == 1);
    REQUIRE(editor.document.text() == "ab");

    //the caret is where the undo left it, but the step is sealed
    editor.type("x");
    REQUIRE(editor.document.text() == "abx");
    REQUIRE(editor.stack.undoCount() == 2);
    REQUIRE(editor.stack.redoCount() == 0);
    REQUIRE(editor.redo() == nullptr);

    editor.undo();
    REQUIRE(editor.document.text() == "ab");
    //redo then type: no joining either
    editor.redo();
    editor.type("y");
    REQUIRE(editor.stack.undoCount() == 3);
}

TEST_CASE("text::UndoStack: a group is one step")
{
    Clock clock;
    Editor editor("hello world");

    editor.moveTo({0, 11});
    editor.type("!");
    {
        text::UndoGroup group(editor.stack, EditKind::Paste);
        editor.selection={{0, 0}, {0, 5}};
        editor.edit(rangeOf(editor.selection), String(), EditKind::Cut);
        editor.edit(rangeOf(editor.selection), "bye", EditKind::Insert);
        REQUIRE(editor.stack.undoCount() == 2);
        REQUIRE(editor.stack.redoCount() == 0);
    }
    REQUIRE(editor.document.text() == "bye world!");

    //a Paste step: the typing after it is a step of its own
    editor.type("?");
    REQUIRE(editor.stack.undoCount() == 3);

    editor.undo();
    const UndoStep *step=editor.undo();
    REQUIRE(step->kind == EditKind::Paste);
    REQUIRE(step->edits.size() == 2);
    REQUIRE(step->before == Selection{{0, 0}, {0, 5}});
    REQUIRE(editor.document.text() == "hello world!");

    editor.redo();
    REQUIRE(editor.document.text() == "bye world!");
    REQUIRE(editor.selection == at({0, 3}));

    //an Insert group (a letter typed over a selection) starts a step the
    //letters typed after it join; an empty group changes nothing
    editor.stack.clear();
    editor.selection={{0, 0}, {0, 3}};
    {
        text::UndoGroup group(editor.stack, EditKind::Insert);
        editor.edit(rangeOf(editor.selection), String(), EditKind::DeleteBackward);
        editor.edit(rangeOf(editor.selection), "h", EditKind::Insert);
    }
    editor.type("i");
    {
        text::UndoGroup group(editor.stack, EditKind::Insert);
    }
    editor.type("!");
    {
        //a group's first edit at the caret joins the typing before it
        text::UndoGroup group(editor.stack, EditKind::Insert);
        editor.edit(rangeOf(editor.selection), "?", EditKind::Insert);
    }
    REQUIRE(editor.document.text() == "hi!? world!");
    REQUIRE(editor.stack.undoCount() == 1);
    editor.undo();
    REQUIRE(editor.document.text() == "bye world!");
    REQUIRE(editor.selection == Selection{{0, 0}, {0, 3}});
}

TEST_CASE("text::UndoStack: setLimit keeps the newest steps, clear forgets them all")
{
    Clock clock;
    Editor editor;

    REQUIRE(editor.stack.limit() == 0);
    for(int i=0; i < 5; ++i)
        editor.paste(String(1, char('a'+i)));
    REQUIRE(editor.stack.undoCount() == 5);

    editor.stack.setLimit(3);
    REQUIRE(editor.stack.undoCount() == 3);
    editor.paste("f");
    REQUIRE(editor.stack.undoCount() == 3);
    while(editor.undo() != nullptr) {}
    REQUIRE(editor.document.text() == "abc");

    //with only steps to redo past the limit, the newest of them go
    editor.stack.setLimit(1);
    REQUIRE(editor.stack.undoCount() == 0);
    REQUIRE(editor.stack.redoCount() == 1);
    editor.redo();
    REQUIRE(editor.document.text() == "abcd");

    editor.stack.clear();
    REQUIRE(!editor.stack.canUndo());
    REQUIRE(!editor.stack.canRedo());
    editor.type("x");
    REQUIRE(editor.stack.undoCount() == 1);
}
