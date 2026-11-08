/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  gweni_bench's scenes. Each builds its tree in code, so a ControlTest or
 *  Demo page can change without moving the numbers, and drives the same input
 *  every run. A new scene is a Scene subclass and one row in scenes().
 */
#include "bench.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/label.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/progressBar.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/windowControl.h>
#include <gweni/platforms/inputEvent.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <deque>

namespace gweni
{
namespace bench
{

namespace
{

std::string format(const char *pattern, int value)
{
    char text[64];
    std::snprintf(text, sizeof(text), pattern, value);
    return text;
}

void moveMouse(SceneContext &ctx, Point &mouse, const Point &to)
{
    const Point delta=(mouse.x < 0) ? Point(0, 0) : Point(to.x-mouse.x, to.y-mouse.y);

    mouse=to;
    ctx.canvas->inputMouseMoved(to.x, to.y, delta.x, delta.y);
}

// At a fixed position and size whatever its content asks for, as ControlTest's
// scenarios place a control.
void place(controls::Base *control, int x, int y, int w, int h)
{
    control->setAlignment(Alignment::Absolute);
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(x, y, w, h);
}

// \a count Labels named "<prefix>NNNNN" on \a parent, in a grid of 100x20-unit
// cells \a columns wide from (x, y); rows past the parent's bottom are
// off-screen but still laid out.
std::vector<controls::Label *> labelGrid(controls::Base *parent, int count, int x, int y,
    int columns, const char *prefix)
{
    const int cellW=100, cellH=20;
    std::vector<controls::Label *> labels;
    labels.reserve(size_t(count));
    columns=std::max(columns, 1);

    for(int i=0; i<count; ++i)
    {
        controls::Label *label=parent->newChild<controls::Label>();
        label->setBounds(x+(i%columns)*cellW, y+(i/columns)*cellH, cellW-4, cellH-2);
        label->setText(prefix+format("%05d", i));
        labels.push_back(label);
    }
    return labels;
}

void setTarget(SceneContext &ctx, const Point &size, float scale)
{
    if(!ctx.setTarget)
        return;     // the primer canvas
    if(!ctx.setTarget(size, scale))
    {
        std::fprintf(stderr, "gweni_bench: could not make a %dx%d target at scale %g\n",
            size.x, size.y, double(scale));
        std::exit(EXIT_FAILURE);
    }
}

//-----------------------------------------------------------------------------
// build / idle: an N-item tree of mixed controls. Of every 20 items, 16 go in a
// grid of nested ScrollControls (6 Labels, 4 Buttons, 3 TextBoxes, 3
// CheckBoxWithLabels), 2 are ListBox rows and 2 TreeControl nodes. The grid is
// pages of 100 cells (4 columns, 25 rows of 26 units, taller than the page, so
// each page scrolls), each page an inner ScrollControl stacked in one outer
// ScrollControl on the left; the ListBox and the TreeControl (every 8th node a
// top-level node, the others its children, all expanded) sit on the right.
// N counts the items; the summary's controls= counts every control in the
// tree, their parts included.
//-----------------------------------------------------------------------------
enum class Item { Label, Button, TextBox, CheckBox, ListRow, TreeNode };

const Item g_itemMix[20]=
{
    Item::Label, Item::Button, Item::TextBox, Item::CheckBox, Item::Label,
    Item::ListRow, Item::Label, Item::Button, Item::TextBox, Item::TreeNode,
    Item::Label, Item::CheckBox, Item::Button, Item::Label, Item::ListRow,
    Item::TextBox, Item::Label, Item::Button, Item::CheckBox, Item::TreeNode
};

class MixedTreeScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int pageCells=100;
        const int columns=4;
        const int cellW=150, cellH=26;
        const int rightX=920;

        controls::ScrollControl *outer=ctx.canvas->newChild<controls::ScrollControl>();
        outer->setBounds(10, 10, rightX-20, ctx.size.y-20);

        controls::ListBox *list=ctx.canvas->newChild<controls::ListBox>();
        list->setBounds(rightX, 10, ctx.size.x-rightX-10, ctx.size.y/2-15);

        controls::TreeControl *tree=ctx.canvas->newChild<controls::TreeControl>();
        tree->setBounds(rightX, ctx.size.y/2+5, ctx.size.x-rightX-10, ctx.size.y/2-15);

        controls::ScrollControl *page=nullptr;
        controls::TreeNode *topNode=nullptr;
        int cells=0, listRows=0, treeNodes=0;

        for(int i=0; i<ctx.n; ++i)
        {
            const Item item=g_itemMix[i%20];

            if(item == Item::ListRow)
            {
                list->addItem(format("Row %05d", listRows++));
                continue;
            }
            if(item == Item::TreeNode)
            {
                if(treeNodes%8 == 0)
                    topNode=tree->addNode(format("Node %05d", treeNodes));
                else
                    topNode->addNode(format("Leaf %05d", treeNodes));
                ++treeNodes;
                continue;
            }

            const int cell=cells%pageCells;
            if(cell == 0)
            {
                const int pageIndex=cells/pageCells;
                page=outer->newChild<controls::ScrollControl>();
                page->setBounds(0, pageIndex*310, columns*cellW+20, 300);
            }
            ++cells;

            const Rect bounds((cell%columns)*cellW+4, (cell/columns)*cellH+4, cellW-10, cellH-4);
            controls::Base *control=nullptr;

            switch(item)
            {
            case Item::Label:
            {
                controls::Label *label=page->newChild<controls::Label>();
                label->setText(format("Label %05d", i));
                control=label;
                break;
            }
            case Item::Button:
            {
                controls::Button *button=page->newChild<controls::Button>();
                button->setText(format("Button %05d", i));
                control=button;
                break;
            }
            case Item::TextBox:
            {
                controls::TextBox *textBox=page->newChild<controls::TextBox>();
                textBox->setText(format("Text %05d", i));
                control=textBox;
                break;
            }
            case Item::CheckBox:
            {
                controls::CheckBoxWithLabel *check=page->newChild<controls::CheckBoxWithLabel>();
                check->label()->setText(format("Check %05d", i));
                check->checkBox()->setChecked(i%3 == 0);
                control=check;
                break;
            }
            case Item::ListRow:
            case Item::TreeNode:
                break;
            }

            control->setBounds(bounds);
        }

        tree->expandAll();
    }
};

//-----------------------------------------------------------------------------
// hover-sweep: a ListBox of N rows as tall as the canvas; each measured frame
// moves the mouse one row down, wrapping at the last row fully inside the
// list, so every frame one row loses the hover and the next gains it. The last
// warm-up frame puts the mouse on row 0.
//-----------------------------------------------------------------------------
class HoverSweepScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_list=ctx.canvas->newChild<controls::ListBox>();
        m_list->setBounds(10, 10, 400, ctx.size.y-20);

        m_rows.reserve(size_t(ctx.n));
        for(int i=0; i<ctx.n; ++i)
            m_rows.push_back(m_list->addItem(format("Row %05d", i)));
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame < -1 || m_rows.empty())
            return;

        if(m_visible == 0)
            m_visible=visibleRows();

        // before any layout (--warmup 0) the rows have no size yet: step down
        // the list's top instead
        if(m_visible == 0)
        {
            const Point origin=m_list->localPosToCanvas(Point(0, 0));
            moveMouse(ctx, m_mouse, Point(origin.x+20, origin.y+4+frame+1));
            return;
        }

        controls::Base *row=m_rows[size_t((frame+1)%m_visible)];
        const Point at=row->localPosToCanvas(Point(20, row->getHeight()/2));
        moveMouse(ctx, m_mouse, at);
    }

private:
    // the rows from the top that lie wholly inside the list, at least 1 once
    // laid out
    int visibleRows()
    {
        const Point listTop=m_list->localPosToCanvas(Point(0, 0));
        const int listBottom=listTop.y+m_list->getHeight();
        int count=0;

        for(controls::Base *row : m_rows)
        {
            if(row->getHeight() <= 0)
                return 0;
            if(row->localPosToCanvas(Point(0, row->getHeight())).y > listBottom)
                break;
            ++count;
        }
        return std::max(count, 1);
    }

    controls::ListBox *m_list=nullptr;
    std::vector<controls::layout::TableRow *> m_rows;
    int m_visible=0;
    Point m_mouse=Point(-1, -1);
};

//-----------------------------------------------------------------------------
// set-text: N Labels in a grid of 100x20-unit cells filling the canvas width
// (rows past the canvas's bottom are off-screen but still laid out); each
// measured frame K of them, spread over the whole set, get a new string of the
// same length.
//-----------------------------------------------------------------------------
class SetTextScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int cellW=100, cellH=20;
        const int columns=std::max(1, (ctx.size.x-20)/cellW);

        m_labels.reserve(size_t(ctx.n));
        for(int i=0; i<ctx.n; ++i)
        {
            controls::Label *label=ctx.canvas->newChild<controls::Label>();
            label->setBounds(10+(i%columns)*cellW, 10+(i/columns)*cellH, cellW-4, cellH-2);
            label->setText(format("L%05d -----", i));
            m_labels.push_back(label);
        }
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0 || m_labels.empty())
            return;

        const int count=int(m_labels.size());
        const int k=std::min(std::max(ctx.k, 1), count);
        const int stride=count/k;

        for(int j=0; j<k; ++j)
        {
            const int index=(j*stride+frame)%count;
            char text[32];
            std::snprintf(text, sizeof(text), "L%05d %05d", index, frame%100000);
            m_labels[size_t(index)]->setText(text);
        }
    }

private:
    std::vector<controls::Label *> m_labels;
};

//-----------------------------------------------------------------------------
// type: a focused TextBoxMultiline, 600 units wide and the canvas's height,
// holding N words of 6 characters in paragraphs of 12 words, so each paragraph
// wraps to two lines. The caret starts at the beginning of the middle
// paragraph; each measured frame types one letter there, and every fourth a
// Backspace instead, so the text grows by half a character a frame. The caret
// blinks on the stepped clock, the same in every run.
//-----------------------------------------------------------------------------
class TypeScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        String text;
        text.reserve(size_t(ctx.n)*7);
        for(int i=0; i<ctx.n; ++i)
        {
            text+=format("w%05d", i);
            if(i+1 < ctx.n)
                text+=(i%12 == 11) ? '\n' : ' ';
        }

        size_t middle=text.find('\n', text.size()/2);
        middle=(middle == String::npos) ? text.size()/2 : middle+1;

        m_box=ctx.canvas->newChild<controls::TextBoxMultiline>();
        place(m_box, 10, 10, 600, ctx.size.y-20);
        m_box->setText(text);
        m_box->setCursorPos(int(middle));
        m_box->setCursorend(int(middle));
        m_box->focus();
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0)
            return;

        if(frame%4 == 3)
        {
            ctx.canvas->inputModifierKey(keys::Backspace, true);
            ctx.canvas->inputModifierKey(keys::Backspace, false);
        }
        else
            ctx.canvas->inputCharacter(UnicodeChar('a'+frame%26));
    }

private:
    controls::TextBoxMultiline *m_box=nullptr;
};

//-----------------------------------------------------------------------------
// scroll: a ScrollControl the canvas's height holding N Buttons, one per
// 30-unit row. A wheel notch moves the bar by its nudge, 30 units of the
// content, as a fraction of the scrollable range (content - viewable), so a
// notch scrolls 30*(1 - viewable/content) units: about one row at large N
// (29.2 at N=1000), much less at small N. The last warm-up frame puts the
// mouse over it; each measured frame turns the wheel one notch down, and once
// the bar is at the bottom one notch up, until it is back at the top. It is
// placed at a fixed size, since a ScrollControl left to its own size asks for
// all its content (issue #57) and would not scroll.
//-----------------------------------------------------------------------------
class ScrollScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_scroll=ctx.canvas->newChild<controls::ScrollControl>();
        place(m_scroll, 10, 10, 400, ctx.size.y-20);
        m_scroll->setScroll(false, true);

        for(int i=0; i<ctx.n; ++i)
        {
            controls::Button *button=m_scroll->newChild<controls::Button>();
            button->setText(format("Row %05d", i));
            button->setBounds(0, i*30, 360, 28);
        }

        for(controls::Base *child : m_scroll->getActualChildren())
            if(controls::VerticalScrollBar *bar=dynamic_cast<controls::VerticalScrollBar *>(child))
                m_bar=bar;
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame == -1 || (frame == 0 && m_mouse.x < 0))
        {
            const Point origin=m_scroll->localPosToCanvas(Point(0, 0));
            moveMouse(ctx, m_mouse, Point(origin.x+100, origin.y+m_scroll->getHeight()/2));
        }
        if(frame < 0)
            return;

        // A notch one way, or the other at the end. The wheel reports a notch
        // used even when the bar is already at its end (it clamps and still
        // notifies), so the end is told by the bar's own amount, which the
        // wheel sets at once (the content moves in the next layout).
        const float before=m_bar ? m_bar->getScrolledAmount() : 0.0f;
        ctx.canvas->inputMouseWheel(-60*m_direction);
        if(m_bar && m_bar->getScrolledAmount() == before)
        {
            m_direction=-m_direction;
            ctx.canvas->inputMouseWheel(-60*m_direction);
        }
    }

private:
    controls::ScrollControl *m_scroll=nullptr;
    controls::VerticalScrollBar *m_bar=nullptr;
    Point m_mouse=Point(-1, -1);
    int m_direction=1;      // 1 down, -1 up
};

//-----------------------------------------------------------------------------
// drag: N Labels in a grid filling the canvas, and over them one 360x300
// WindowControl holding a fixed 40 controls (20 Labels and 20 Buttons), so the
// moving subtree is the same at any N. The last warm-up frame presses on its
// title bar; each measured frame drags it 3 units right and 2 down (left and
// up for the next 30 frames, and so on), and the last one releases it.
//-----------------------------------------------------------------------------
class DragScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        labelGrid(ctx.canvas, ctx.n, 10, 10, (ctx.size.x-20)/100, "L");

        m_window=ctx.canvas->newChild<controls::WindowControl>();
        m_window->setTitle("Dragged");
        place(m_window, 200, 150, 360, 300);

        for(int i=0; i<40; ++i)
        {
            const Rect cell((i%2)*170+4, (i/2)*26+4, 160, 22);
            if(i%4 < 2)
            {
                controls::Label *label=m_window->newChild<controls::Label>();
                label->setText(format("Inside %02d", i));
                label->setBounds(cell);
            }
            else
            {
                controls::Button *button=m_window->newChild<controls::Button>();
                button->setText(format("Button %02d", i));
                button->setBounds(cell);
            }
        }
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame < -1)
            return;

        // 40,12 from the window's corner is on its title bar, left of the
        // close button
        if(!m_pressed)
        {
            moveMouse(ctx, m_mouse, m_window->localPosToCanvas(Point(0, 0))+Point(40, 12));
            ctx.canvas->inputMouseButton(0, true);
            m_pressed=true;
        }
        if(frame < 0)
            return;

        const int sign=((frame/30)%2 == 0) ? 1 : -1;
        moveMouse(ctx, m_mouse, m_mouse+Point(3*sign, 2*sign));

        if(frame == ctx.frames-1)
            ctx.canvas->inputMouseButton(0, false);
    }

private:
    controls::WindowControl *m_window=nullptr;
    Point m_mouse=Point(-1, -1);
    bool m_pressed=false;
};

//-----------------------------------------------------------------------------
// tree-toggle: a TreeControl the canvas's height with N nodes, all expanded.
// The second top-level node holds 50 leaves; the rest are groups of 10 (a
// top-level node, 3 children, 2 leaves under each). Measured frames alternate
// collapsing and expanding the second node, which hides or shows its 50
// leaves and moves every node below it.
//-----------------------------------------------------------------------------
class TreeToggleScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_tree=ctx.canvas->newChild<controls::TreeControl>();
        place(m_tree, 10, 10, 500, ctx.size.y-20);

        int made=0;
        auto group=[&](controls::TreeNode *top)
        {
            for(int c=0; c<3 && made < ctx.n; ++c)
            {
                controls::TreeNode *child=top->addNode(format("Child %05d", made++));
                for(int l=0; l<2 && made < ctx.n; ++l)
                    child->addNode(format("Leaf %05d", made++));
            }
        };

        group(m_tree->addNode(format("Node %05d", made++)));

        if(made < ctx.n)
        {
            m_toggle=m_tree->addNode(format("Toggle %05d", made++));
            for(int l=0; l<50 && made < ctx.n; ++l)
                m_toggle->addNode(format("Leaf %05d", made++));
        }

        while(made < ctx.n)
            group(m_tree->addNode(format("Node %05d", made++)));

        m_tree->expandAll();
    }

    void step(SceneContext &/*ctx*/, int frame) override
    {
        if(frame < 0 || !m_toggle)
            return;

        if(frame%2 == 0)
            m_toggle->close();
        else
            m_toggle->open();
    }

private:
    controls::TreeControl *m_tree=nullptr;
    controls::TreeNode *m_toggle=nullptr;
};

//-----------------------------------------------------------------------------
// add-remove: N Labels in a grid filling the canvas width. Each measured frame
// removes the oldest Label (deleted outright on even frames, delayedDelete()
// on odd ones) and adds a new one in its cell, as the canvas's last child; both
// change the tree order, so the next update phase renumbers z: its walk enters
// the whole tree, and it relabels only the new Label (a removal leaves a gap).
//-----------------------------------------------------------------------------
class AddRemoveScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const std::vector<controls::Label *> labels=labelGrid(ctx.canvas, ctx.n, 10, 10,
            (ctx.size.x-20)/100, "L");
        m_labels.assign(labels.begin(), labels.end());
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0 || m_labels.empty())
            return;

        controls::Label *oldest=m_labels.front();
        m_labels.pop_front();
        const Rect cell=oldest->getBounds();

        if(frame%2 == 0)
            delete oldest;
        else
            oldest->delayedDelete();

        controls::Label *label=ctx.canvas->newChild<controls::Label>();
        label->setBounds(cell);
        label->setText(format("A%05d -----", frame%100000));
        m_labels.push_back(label);
    }

private:
    std::deque<controls::Label *> m_labels;
};

//-----------------------------------------------------------------------------
// bring-to-front: 8 overlapping 420x320 WindowControls, cascaded 60 units
// right and 40 down, holding N Labels between them (a grid of 4 columns in
// each; most rows lie below the window and are clipped). Each measured frame
// brings the next window to the front, starting with the bottom one, which
// changes the tree order and renumbers z over the whole tree.
//-----------------------------------------------------------------------------
class BringToFrontScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int windows=8;

        for(int w=0; w<windows; ++w)
        {
            controls::WindowControl *window=ctx.canvas->newChild<controls::WindowControl>();
            window->setTitle(format("Window %d", w));
            place(window, 20+w*60, 20+w*40, 420, 320);

            const int count=ctx.n/windows+((w < ctx.n%windows) ? 1 : 0);
            labelGrid(window, count, 4, 4, 4, format("W%d.", w).c_str());
            m_windows.push_back(window);
        }
    }

    void step(SceneContext &/*ctx*/, int frame) override
    {
        if(frame < 0)
            return;

        m_windows[size_t(frame)%m_windows.size()]->bringToFront();
    }

private:
    std::vector<controls::WindowControl *> m_windows;
};

//-----------------------------------------------------------------------------
// resize: the build tree; measured frames alternate the canvas between 3/4 of
// --size and --size, with the offscreen target and the renderer's view
// resized to match, as a window's framebuffer-size callback does.
//-----------------------------------------------------------------------------
class ResizeScene: public MixedTreeScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0)
            return;

        const Point size=(frame%2 == 0) ? Point(ctx.size.x*3/4, ctx.size.y*3/4) : ctx.size;
        setTarget(ctx, size, 1.0f);
    }
};

//-----------------------------------------------------------------------------
// scale: the build tree; measured frames step Canvas::setScale through 1.5, 2
// and 1, the canvas keeping its logical size and the target and the view
// resized to it in device pixels, as a window's content-scale callback does.
// Every text run is laid out again at the new scale.
//-----------------------------------------------------------------------------
class ScaleScene: public MixedTreeScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0)
            return;

        const float scales[]={1.5f, 2.0f, 1.0f};
        setTarget(ctx, ctx.size, scales[frame%3]);
    }
};

//-----------------------------------------------------------------------------
// animate: N idle Labels in a grid left of a column holding K indeterminate
// ProgressBars and a focused TextBox, the clock stepping 1/60 s per frame: the
// bars' segments move every frame and the caret blinks. No input. The caret
// stays lit for 1.5 s (90 frames) after the focus before it starts to blink,
// so the scene warms up for 90 frames by default; its 60 measured frames then
// hold blink edges (measured frames 1 and 32).
//-----------------------------------------------------------------------------
class AnimateScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int columnX=ctx.size.x-320;

        labelGrid(ctx.canvas, ctx.n, 10, 10, (columnX-20)/100, "L");

        controls::TextBox *box=ctx.canvas->newChild<controls::TextBox>();
        place(box, columnX, 10, 300, 24);
        box->setText("Caret");
        box->focus();

        for(int i=0; i<std::max(ctx.k, 1); ++i)
        {
            controls::ProgressBar *bar=ctx.canvas->newChild<controls::ProgressBar>();
            place(bar, columnX, 44+i*24, 300, 18);
            bar->setIndeterminate(true);
        }
    }
};

}//namespace

const char *scalingName(Scaling scaling)
{
    return (scaling == Scaling::Local) ? "local" : "linear";
}

const std::vector<SceneInfo> &scenes()
{
    static const std::vector<SceneInfo> list=
    {
        {"build", "construct an N-item tree of mixed controls and run its first frame",
            true, 1000, 0, 1, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new MixedTreeScene()); }, true},
        {"idle", "the build tree, settled, with no input",
            true, 1000, 5, 10, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new MixedTreeScene()); }},
        {"hover-sweep", "move the mouse one row down a ListBox of N rows per frame",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new HoverSweepScene()); }},
        {"set-text", "K of N Labels get a new string of the same length per frame",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new SetTextScene()); }},
        {"type", "type into the middle of a focused TextBoxMultiline of N wrapped words",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new TypeScene()); }},
        {"scroll", "turn the wheel one notch per frame over a ScrollControl of N Button rows",
            true, 1000, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new ScrollScene()); }},
        {"drag", "drag a Window of 40 controls by its title bar over N Labels",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new DragScene()); }},
        {"tree-toggle", "collapse and expand a 50-leaf node near the top of an N-node TreeControl",
            true, 1000, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new TreeToggleScene()); }},
        {"add-remove", "add one Label and delete another per frame among N",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new AddRemoveScene()); }},
        {"bring-to-front", "bring the next of 8 Windows holding N Labels to the front per frame",
            true, 1000, 3, 16, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new BringToFrontScene()); }},
        {"resize", "the build tree, the canvas resized between 3/4 of --size and --size per frame",
            true, 1000, 3, 10, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new ResizeScene()); }},
        {"scale", "the build tree at Canvas::setScale 1.5, 2 and 1 in turn",
            true, 1000, 3, 9, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new ScaleScene()); }},
        {"animate", "K indeterminate ProgressBars and a blinking caret among N idle Labels",
            true, 1000, 90, 60, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new AnimateScene()); }},
    };
    return list;
}

}//namespace bench
}//namespace gweni
