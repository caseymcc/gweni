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
#include <gweni/controls/chart.h>
#include <gweni/controls/checkBox.h>
#include <gweni/controls/codeEditor.h>
#include <gweni/controls/label.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/logView.h>
#include <gweni/controls/nodeGraph.h>
#include <gweni/controls/progressBar.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/treeControl.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/controls/virtualList.h>
#include <gweni/controls/windowControl.h>
#include <gweni/platforms/inputEvent.h>
#include <gweni/skin.h>

#include <algorithm>
#include <cmath>
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
// Above them a bold title Label is the tree's second font (the skin's face and
// size, bold: below the SDF threshold, so an atlas of its own): its one run stays
// in one index chunk while the main font's runs take one chunk per 16383 glyph
// slots (1, 2 and 7 at N=1000, 3000 and 10000), so idle holds the debug text
// draw-list verify, which rebuilds each font's list into one shared scratch, to
// fonts whose chunk counts differ by more at a larger N (#481).
// N counts the items; the summary's controls= counts every control in the
// tree, their parts included.
//-----------------------------------------------------------------------------
enum class Item { Label, Button, TextBox, CheckBox, ListRow, TreeNode };

// The title's font. A Label keeps a pointer to its font and the scene is deleted
// before its canvas, so it lives as long as the process. Label::setFont(const
// Font &) loads nothing: the renderer makes its atlas when the title is first
// laid out, in a frame (the primer canvas's, which leaves it loaded, as it does
// the skin's font), not in Scene::build(), where the backends' own allocations
// for a new atlas would differ (backend_counters compares construct_allocs).
Font &titleFont()
{
    static Font font;
    return font;
}

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
        const int top=30;

        Font &font=titleFont();
        font=ctx.canvas->getSkin()->getDefaultFont();
        font.bold=true;

        controls::Label *title=ctx.canvas->newChild<controls::Label>();
        title->setFont(font);
        title->setText(format("Mixed controls: %d items", ctx.n));
        title->setBounds(10, 6, 400, top-10);

        controls::ScrollControl *outer=ctx.canvas->newChild<controls::ScrollControl>();
        outer->setBounds(10, top, rightX-20, ctx.size.y-top-10);

        controls::ListBox *list=ctx.canvas->newChild<controls::ListBox>();
        list->setBounds(rightX, top, ctx.size.x-rightX-10, ctx.size.y/2-top-5);

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
// vlist-variable-scroll: a VirtualList in variable mode filling the canvas's
// height, 400 wide, of N items of wrapped text from one to five lines (the
// default rows, wrapped, measured as Base::wrappedTextHeight at their width
// plus their padding), turned one wheel notch down per frame. The idle
// re-measure is off (budget 0), so a frame measures only the items it brings
// into view and does the same work at any N: N is the index's size alone.
//-----------------------------------------------------------------------------
class VariableListScrollScene: public Scene
{
public:
    static constexpr int PadX=6;
    static constexpr int PadY=3;

    void build(SceneContext &ctx) override
    {
        m_list=ctx.canvas->newChild<controls::VirtualList>();
        place(m_list, 10, 10, 400, ctx.size.y-20);
        m_list->setRemeasureBudget(0);
        m_list->setRowFactory([](controls::Base *parent)
            {
                auto *row=parent->newChild<controls::internal::VirtualListRow>();

                row->setWrap(true);
                row->setPadding(Padding(PadX, PadY, PadX, PadY));
                row->setTextPadding(Padding(0, 0, 0, 0));
                return row;
            });
        m_list->setRowBinder([](controls::Base *row, size_t index)
            {
                if(controls::Label *label=dynamic_cast<controls::Label *>(row))
                    label->setText(itemText(index));
            });
        m_list->setItemMeasurer([this](size_t index, int width)
            {
                return m_list->wrappedTextHeight(m_list->getSkin()->getDefaultFont(), itemText(index),
                    width-2*PadX)+2*PadY;
            });
        m_list->setItemCount(size_t(ctx.n));
    }

    void step(SceneContext &ctx, int frame) override
    {
        if(frame == -1 || (frame == 0 && m_mouse.x < 0))
        {
            const Point origin=m_list->localPosToCanvas(Point(0, 0));
            moveMouse(ctx, m_mouse, Point(origin.x+100, origin.y+m_list->getHeight()/2));
        }
        if(frame >= 0)
            ctx.canvas->inputMouseWheel(-60);
    }

private:
    // Item i: its number and 2 to 61 words, so one to five lines at 400 units
    static std::string itemText(size_t index)
    {
        static const char *const words[]={"variable", "height", "rows", "of", "wrapped", "text", "each",
            "measured", "as", "it", "comes", "into", "view"};
        const size_t count=2+(index*2654435761u>>5)%((index%4 == 0)?60:12);
        std::string text=format("Item %05d:", int(index));

        for(size_t i=0; i < count; ++i)
            text+=std::string(" ")+words[(index*7+i*3)%13];
        return text;
    }

    controls::VirtualList *m_list=nullptr;
    Point m_mouse=Point(-1, -1);
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
// tree-relabel: a TreeControl with an open node of 10 leaves and a closed node
// holding N leaves. Measured frames give the closed node a new label of the
// same length. Its rows count in its width though it is closed, and are
// measured again only when one of them changed, so a frame enters the same
// controls at any N.
//-----------------------------------------------------------------------------
class TreeRelabelScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_tree=ctx.canvas->newChild<controls::TreeControl>();
        place(m_tree, 10, 10, 500, ctx.size.y-20);

        controls::TreeNode *open=m_tree->addNode("Open");

        for(int l=0; l<10; ++l)
            open->addNode(format("Leaf %05d", l));
        open->open();

        m_closed=m_tree->addNode(format("Closed %05d", 0));
        for(int l=0; l<ctx.n; ++l)
            m_closed->addNode(format("Row %05d", l));
    }

    void step(SceneContext &/*ctx*/, int frame) override
    {
        if(frame < 0)
            return;

        m_closed->setText(format("Closed %05d", frame+1));
    }

private:
    controls::TreeControl *m_tree=nullptr;
    controls::TreeNode *m_closed=nullptr;
};

//-----------------------------------------------------------------------------
// add-remove: N Labels in a grid filling the canvas width. Each measured frame
// removes the oldest Label (deleted outright on even frames, delayedDelete()
// on odd ones) and adds a new one in its cell, as the canvas's last child; both
// change the tree order, so the next update phase renumbers z: its walk enters
// the canvas and the new Label, passing the clean ones, and relabels only the
// new Label (a removal leaves a gap, and walks nothing).
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

//-----------------------------------------------------------------------------
// hidden-pages: a fixed header row of 20 Buttons built first, K pages holding N
// Buttons between them (the first N%K pages one more), and a fixed footer row of
// 20 Buttons built last, each page filling the canvas between the rows with its
// Buttons in a grid of 60x20-unit cells (rows past the page's bottom are
// clipped but still drawn). Every control is drawn shown in the first frame, so
// the pages' primitive slots lie between the header's and the footer's, and the
// second frame hides the pages, as a TabControl leaves the pages switched away
// from. Each frame then hovers the next header Button. The shown slots are at
// both ends of the mirror, so a chunk drawn up to its last shown slot covers
// every hidden one below it (#508).
//-----------------------------------------------------------------------------
class HiddenPagesScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int rowH=30, cellW=60, cellH=20;
        const int pages=std::max(ctx.k, 1);
        const int columns=std::max(ctx.size.x/cellW, 1);

        m_header=buttonRow(ctx, 5, "H");
        for(int page=0, built=0; page<pages; ++page)
        {
            const int count=ctx.n/pages+((page < ctx.n%pages) ? 1 : 0);
            controls::Base *host=ctx.canvas->newChild<controls::Base>();

            place(host, 0, rowH+5, ctx.size.x, ctx.size.y-2*(rowH+5));
            for(int i=0; i<count; ++i, ++built)
            {
                controls::Button *button=host->newChild<controls::Button>();
                place(button, (i%columns)*cellW, (i/columns)*cellH, cellW-4, cellH-2);
                button->setText(format("P%05d", built));
            }
            m_pages.push_back(host);
        }
        buttonRow(ctx, ctx.size.y-rowH, "F");
    }

    void step(SceneContext &ctx, int /*frame*/) override
    {
        // the first frame draws the pages shown; the second hides them
        if(m_steps++ == 1)
        {
            for(controls::Base *page : m_pages)
                page->hide();
        }

        controls::Base *button=m_header[m_hovered++%m_header.size()];
        moveMouse(ctx, m_mouse, button->localPosToCanvas(Point(button->getWidth()/2, button->getHeight()/2)));
    }

private:
    std::vector<controls::Base *> buttonRow(SceneContext &ctx, int y, const char *prefix)
    {
        const int count=20, w=std::max(ctx.size.x/count, 8);
        std::vector<controls::Base *> row;

        for(int i=0; i<count; ++i)
        {
            controls::Button *button=ctx.canvas->newChild<controls::Button>();
            place(button, i*w, y, w-4, 24);
            button->setText(prefix+format("%02d", i));
            row.push_back(button);
        }
        return row;
    }

    std::vector<controls::Base *> m_header;
    std::vector<controls::Base *> m_pages;
    int m_steps=0;
    size_t m_hovered=0;
    Point m_mouse=Point(-1, -1);
};

//-----------------------------------------------------------------------------
// hidden-gaps: N Buttons in a grid of 60x20-unit cells over the canvas (rows
// past its bottom clipped but still drawn), all drawn shown in the first frame;
// the second hides every one but each (K+1)-th, so the primitive slots are runs
// of one shown slot between gaps of K hidden ones, N/(K+1) runs in all. No
// input after that. What a range costs against what a hidden slot in a merged
// gap costs is measured here, with --range-gap and --range-cap (#508).
//-----------------------------------------------------------------------------
class HiddenGapsScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        const int cellW=60, cellH=20;
        const int columns=std::max(ctx.size.x/cellW, 1);

        m_gap=std::max(ctx.k, 1);
        for(int i=0; i<ctx.n; ++i)
        {
            controls::Button *button=ctx.canvas->newChild<controls::Button>();
            place(button, (i%columns)*cellW, (i/columns)*cellH, cellW-4, cellH-2);
            button->setText(format("G%05d", i));
            m_buttons.push_back(button);
        }
    }

    void step(SceneContext &/*ctx*/, int /*frame*/) override
    {
        // the first frame draws every Button shown; the second hides the gaps
        if(m_steps++ != 1)
            return;
        for(size_t i=0; i<m_buttons.size(); ++i)
        {
            if(i%size_t(m_gap+1) != 0)
                m_buttons[i]->hide();
        }
    }

private:
    std::vector<controls::Base *> m_buttons;
    int m_gap=1;
    int m_steps=0;
};

}//namespace

const char *scalingName(Scaling scaling)
{
    return (scaling == Scaling::Local) ? "local" : "linear";
}

//-----------------------------------------------------------------------------
// graph-drag / graph-marquee / graph-pan / graph-zoom: a NodeGraph filling the
// canvas at zoom 1, holding N nodes in a grid of ceil(sqrt(N)) columns 200
// graph units apart and rows 140 apart, from 100 units below the graph's top
// (the band above them is empty space). Each node is a title, an input and an
// output pin and a Label body, and each node's output is linked by a Bezier to
// the next node's input in its row. Only the top-left few rows and columns
// are on screen; the rest are laid out and drawn (no culling) off it.
//-----------------------------------------------------------------------------
class GraphScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_graph=ctx.canvas->newChild<controls::NodeGraph>();
        place(m_graph, 0, 0, ctx.size.x, ctx.size.y);

        const int columns=std::max(1, int(std::ceil(std::sqrt(double(std::max(ctx.n, 1))))));
        controls::Pin *previous=nullptr;

        for(int i=0; i<ctx.n; ++i)
        {
            const int column=i%columns;
            controls::Node *node=m_graph->addNode(format("Node %05d", i),
                Point(20+column*200, 100+(i/columns)*140));
            controls::Pin *in=node->addPin(controls::PinDirection::Input, "In");
            controls::Pin *out=node->addPin(controls::PinDirection::Output, "Out");
            controls::Label *body=node->newChild<controls::Label>();

            body->setDock(DockPosition::Top);
            body->setText(format("Value %05d", i));
            if(previous && (column != 0))
                m_graph->addLink(previous, in);
            previous=out;
            if(i == 0)
                m_first=node;
        }
    }

protected:
    controls::NodeGraph *m_graph=nullptr;
    controls::Node *m_first=nullptr;
    Point m_mouse=Point(-1, -1);
};

// graph-drag: the first node dragged by its title bar, pressed in the last
// warm-up frame and released in the last measured one, 3 and 2 units a frame,
// turning back every 10: it and its one link move, nothing else.
class GraphDragScene: public GraphScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < -1)
            return;

        if(!m_pressed)
        {
            controls::Base *bar=m_first->getTitleBar();

            moveMouse(ctx, m_mouse, bar->localPosToCanvas(Point(bar->getWidth()/2, bar->getHeight()/2)));
            ctx.canvas->inputMouseButton(0, true);
            m_pressed=true;
        }
        if(frame < 0)
            return;

        const int sign=((frame/10)%2 == 0) ? 1 : -1;
        moveMouse(ctx, m_mouse, m_mouse+Point(3*sign, 2*sign));

        if(frame == ctx.frames-1)
            ctx.canvas->inputMouseButton(0, false);
    }

private:
    bool m_pressed=false;
};

// graph-marquee: a left drag from the empty band above the grid, pressed in the
// last warm-up frame and grown 6 and 4 units a frame down over the first node
// and its link, held: the marquee's slots and those of the items it
// (de)selects, nothing else.
class GraphMarqueeScene: public GraphScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < -1)
            return;

        if(!m_pressed)
        {
            moveMouse(ctx, m_mouse, m_graph->localPosToCanvas(Point(30, 40)));
            ctx.canvas->inputMouseButton(0, true);
            m_pressed=true;
        }
        if(frame < 0)
            return;

        // held through the last frame: the release's hover hit-test walks
        // the graph's nodes and links to find what is under the mouse, every
        // one of them with no spatial index (v1), so it is not a local cost
        moveMouse(ctx, m_mouse, m_mouse+Point(6, 4));
    }

private:
    bool m_pressed=false;
};

// graph-pan: a right drag on the empty band, pressed in the last warm-up frame,
// 5 and 3 units a frame, turning back every 10: every node's slots move, with
// no layout pass.
class GraphPanScene: public GraphScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < -1)
            return;

        if(!m_pressed)
        {
            moveMouse(ctx, m_mouse, m_graph->localPosToCanvas(Point(600, 40)));
            ctx.canvas->inputMouseButton(1, true);
            // past the pan's threshold before the measured frames
            moveMouse(ctx, m_mouse, m_mouse+Point(10, 0));
            m_pressed=true;
        }
        if(frame < 0)
            return;

        const int sign=((frame/10)%2 == 0) ? 1 : -1;
        moveMouse(ctx, m_mouse, m_mouse+Point(5*sign, 3*sign));
        if(frame == ctx.frames-1)
            ctx.canvas->inputMouseButton(1, false);
    }

private:
    bool m_pressed=false;
};

// graph-zoom: the wheel a notch in and a notch out in turn over the empty band:
// every node's slots move and every run is drawn at a new size, with no layout
// pass.
class GraphZoomScene: public GraphScene
{
public:
    void step(SceneContext &ctx, int frame) override
    {
        if(frame < 0)
            return;

        if(frame == 0)
            moveMouse(ctx, m_mouse, m_graph->localPosToCanvas(Point(600, 40)));
        ctx.canvas->inputMouseWheel((frame%2 == 0) ? 60 : -60);
    }
};

//-----------------------------------------------------------------------------
// code-edit and code-scroll: a CodeEditor with line numbers, 600 wide and the
// canvas's height, highlighted as YAML, over a generated N-line YAML document:
// blocks of 8 lines, a top-level key ("agent_<block>:") and 7 indented ones,
// every line's text a function of its index alone, so the lines on screen are
// the same strings at any N. Only the lines on screen are controls.
//-----------------------------------------------------------------------------
String codeDocument(int lines)
{
    static const char *const keys[]={"  model: opus", "  host: worker-3", "  tags: [ui, text]",
        "  limits:", "    tokens: 12000", "    minutes: 30", "  enabled: true"};
    String text;

    text.reserve(size_t(lines)*18);
    for(int i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        if(i%8 == 0)
            text+=format("agent_%d:", i/8);
        else
            text+=keys[i%8-1];
    }
    return text;
}

class CodeEditorScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_editor=ctx.canvas->newChild<controls::CodeEditor>();
        place(m_editor, 10, 10, 600, ctx.size.y-20);
        m_editor->setLineNumbers(true);
        m_editor->setLanguage("yaml");
        m_editor->setText(codeDocument(ctx.n));
    }

protected:
    controls::CodeEditor *m_editor=nullptr;
};

// code-edit: the caret before the ':' of a top-level key near line 500 (the
// middle at N=1000; the same line, and so the same strings and line numbers
// on screen, at any larger N, with every line past it still in the
// document), the editor focused. Each measured frame is one edit there, in a
// cycle of 10: two letters typed into the key, Return (the key has no
// indent, so the new line holds the ':'), a Backspace that joins the two
// lines again, two letters, Ctrl+Z (undoing them), a letter, then End and
// " |", which opens a block scalar (the key's 7 indented lines below it in
// the block, the next key ending it: the state its line ends in changed),
// and two Backspaces and Left, which close it and put the caret back before
// the ':'. The view is scrolled in the warm-up so that the line is 20 lines
// down it. The caret blinks on the stepped clock, the same in every run.
class CodeEditScene: public CodeEditorScene
{
public:
    void build(SceneContext &ctx) override
    {
        CodeEditorScene::build(ctx);

        m_line=size_t(std::min(500, ctx.n/2)/8*8);
        m_editor->setCaret({m_line, m_editor->getDocument().line(m_line).size()-1});
        m_editor->focus();
    }

    void step(SceneContext &ctx, int frame) override
    {
        // once laid out, the caret's line a third down the view, so the block
        // the cycle opens has its lines on screen below it
        if(!m_scrolled && m_editor->getLineHeight() > 0)
        {
            m_scrolled=true;
            m_editor->setVerticalScrollOffset(
                int(m_line-std::min<size_t>(m_line, 20))*m_editor->getLineHeight());
        }
        if(frame < 0)
            return;

        auto key=[&](int key)
            {
                ctx.canvas->inputModifierKey(key, true);
                ctx.canvas->inputModifierKey(key, false);
            };

        switch(frame%m_cycle)
        {
        case 2: key(keys::Return); break;
        case 3: key(keys::Backspace); break;
        case 6:
            ctx.canvas->inputModifierKey(keys::Control, true);
            ctx.canvas->inputCharacter(UnicodeChar('Z'));
            ctx.canvas->inputModifierKey(keys::Control, false);
            break;
        case 8:
            key(keys::End);
            ctx.canvas->inputCharacter(UnicodeChar(' '));
            ctx.canvas->inputCharacter(UnicodeChar('|'));
            break;
        case 9:
            key(keys::Backspace);
            key(keys::Backspace);
            key(keys::Left);
            break;
        default: ctx.canvas->inputCharacter(UnicodeChar('a'+frame%26)); break;
        }
    }

protected:
    size_t m_line=0;
    //! The edits' cycle: 8 leaves the block scalar out.
    int m_cycle=10;
    //! The view was scrolled to the caret's line (or is to stay as built).
    bool m_scrolled=false;
};

// code-edit-wrap: code-edit's first 8 edits (no block scalar) in a wrapped
// editor (setWrap, no gutter, no language) over the same blocks with a long
// note key last, which wraps into two or three rows at the editor's width. The caret is at the end of the note near line 500,
// so the letters typed lengthen a wrapped line, now and then giving it a
// row more and moving the rows below it, and Return and Backspace (the line
// well inside the view, so they scroll nothing) add and
// take a line of the variable-height list (insertItems, removeItems). The idle
// re-measure is off, so a frame measures only the lines it edits and the ones
// that come into view, whatever N.
String wrapDocument(int lines)
{
    static const char *const keys[]={"  model: opus", "  host: worker-3", "  tags: [ui, text]",
        "  limits:", "    tokens: 12000", "    minutes: 30"};
    String text;

    text.reserve(size_t(lines)*40);
    for(int i=0; i < lines; ++i)
    {
        if(i > 0)
            text+='\n';
        if(i%8 == 0)
            text+=format("agent_%d:", i/8);
        else if(i%8 == 7)
            text+="  note: read the brief and the comments first, build in the container, run the tests"
                " under the render lock and keep each change to the lines it needs";
        else
            text+=keys[i%8-1];
    }
    return text;
}

class CodeEditWrapScene: public CodeEditScene
{
public:
    void build(SceneContext &ctx) override
    {
        // no line numbers: a gutter as wide as the line count's digits would
        // make the wrap width, and so the rows, depend on N
        m_editor=ctx.canvas->newChild<controls::CodeEditor>();
        place(m_editor, 10, 10, 600, ctx.size.y-20);
        m_editor->setRemeasureBudget(0);
        m_editor->setText(wrapDocument(ctx.n));
        m_editor->setWrap(true);

        // the note line of the block there, the last line below N=8
        const size_t lines=m_editor->getDocument().lineCount();
        const size_t line=std::min(size_t(std::min(500, ctx.n/2)/8*8+7), lines-1);

        m_editor->setCaret({line, m_editor->getDocument().line(line).size()});
        // the caret's line a few lines above the viewport's bottom: a Return
        // moves the handful of rows below it and scrolls nothing, so a frame's
        // queue stays far under the quarter of the tree at which the render
        // drain walks the tree instead of sorting (one thumb entry more or
        // less, as the content's length rounds it, changes no path)
        m_editor->scrollToItem(line+3);
        m_editor->focus();
        // code-edit's first 8 edits, with no block scalar, in this view
        m_cycle=8;
        m_scrolled=true;
    }
};

// code-scroll: the view one line down per measured frame (scrollByRows, as a
// wheel's or a bar's step lands): every row on screen moves, one line comes
// into view and one goes, with their numbers.
class CodeScrollScene: public CodeEditorScene
{
public:
    void step(SceneContext &/*ctx*/, int frame) override
    {
        if(frame < 0)
            return;

        m_editor->scrollByRows(1);
    }
};

//-----------------------------------------------------------------------------
// log-append, log-append-detached and log-append-capped: a LogView 600x300,
// pre-filled with N rows, taking 17 a measured frame
// (1000 rows a second on the stepped 60 Hz clock). A row's text is a function
// of its distance from the log's end when filled (and of its sequence number
// once appended), never of N, so the rows on screen are the same strings at
// any N: wrapped text of one to four lines, a coloured call or result line,
// and every ninth a collapsed block. The idle re-measure is off, so a frame
// measures only the rows that come into view.
//
// log-append follows the log: every append frame moves every row on screen.
// log-append-detached is scrolled 300 rows up from the end: the appends
// write no row, only the thumb and the jump button's count. log-append-capped
// follows at a cap of 2000, so each appended row drops one: below N=2000 the
// log is filled up to the cap first.
//-----------------------------------------------------------------------------
controls::LogRow logBenchRow(int k)
{
    static const char *const words[]={"the", "agent", "reads", "a", "file", "and", "runs", "its", "tests",
        "then", "changes", "one", "line", "of", "the", "layout", "so", "every", "row", "wraps"};
    const int kind=((k%9)+9)%9;
    const unsigned h=unsigned(k)*2654435761u;

    if(kind == 4)
    {
        std::vector<String> lines;

        for(int i=0; i < 6+int(h>>28); ++i)
            lines.push_back(format("  %d  output line of the tool", i+1));
        return controls::LogRow::block(format("Bash: ctest -L gpu #%d", k%64), std::move(lines));
    }
    if(kind == 2)
        return controls::LogRow(format("-> Read logView.cpp (%d)", k%64), Color(0x7a, 0xa2, 0xf7));
    if(kind == 7)
        return controls::LogRow(format("<- ok, %d lines", k%64), Color(0x9e, 0xce, 0x6a));

    std::string text=format("assistant %d:", k%64);
    const int count=2+int((h>>8)%((kind == 1)?40u:10u));

    for(int i=0; i < count; ++i)
        text+=std::string(" ")+words[(unsigned(k)*7+unsigned(i)*3)%20];
    return controls::LogRow(text);
}

class LogAppendScene: public Scene
{
public:
    enum class Mode { Following, Detached, Capped };

    explicit LogAppendScene(Mode mode): m_mode(mode) {}

    static constexpr int RowsPerFrame=17;
    static constexpr int CappedRows=2000;
    //! Low enough that the bar's thumb is at its least length (half the bar's
    //! thickness) at N=1000 already, so a following log's thumb is the same
    //! at any N and moves on no frame (its rounding would land on frames set
    //! by N otherwise).
    static constexpr int Height=300;

    void build(SceneContext &ctx) override
    {
        m_log=ctx.canvas->newChild<controls::LogView>();
        place(m_log, 10, 10, 600, Height);
        m_log->setRemeasureBudget(0);
        // a cap of 2000 on a log at least that long, so every append drops
        // one (the first one the rows past it) at any N
        const int rows=(m_mode == Mode::Capped)?std::max(ctx.n, CappedRows):ctx.n;

        // row i is (rows-1-i) from the end: the same strings at the end at any N
        for(int i=0; i < rows; ++i)
            m_log->append(logBenchRow(-1-(rows-1-i)));
        if(m_mode == Mode::Capped)
            m_log->setMaxRows(CappedRows);
        if(m_mode == Mode::Detached && ctx.n > 0)
            m_log->scrollToRow(m_log->rowIdAt(size_t(std::max(0, ctx.n-300))));
    }

    void step(SceneContext &/*ctx*/, int /*frame*/) override
    {
        for(int i=0; i < RowsPerFrame; ++i)
            m_log->append(logBenchRow(m_appended++));
    }

private:
    Mode m_mode;
    controls::LogView *m_log=nullptr;
    int m_appended=0;
};

//-----------------------------------------------------------------------------
// chart-append: a Chart filling the canvas, its x range fixed to 0..N+64 and
// its value range to 0..100, the legend off, with one line series of N points
// (x = i, y a function of i). Each frame, warm-up included, appends the next
// point, so the measured frames' appends are at the same distance from the
// first at any N: the new segment's slot and the one before it (its joint)
// are written, nothing is laid out, and no tick moves. The chart's renderer
// still walks every point to compare it with what its slot holds, which no
// counter sees.
//-----------------------------------------------------------------------------
class ChartAppendScene: public Scene
{
public:
    void build(SceneContext &ctx) override
    {
        m_chart=ctx.canvas->newChild<controls::Chart>();
        place(m_chart, 10, 10, ctx.size.x-20, ctx.size.y-20);
        m_chart->setLegendVisible(false);
        m_chart->setXAxisType(controls::ChartXAxisType::Value);
        m_chart->setXRange(0.0, double(ctx.n+64));
        m_chart->setYRange(controls::ChartAxisSide::Left, 0.0, 100.0);
        m_series=m_chart->addSeries("values", controls::ChartSeriesType::Line);

        std::vector<controls::ChartPoint> points;

        points.reserve(size_t(ctx.n));
        for(int i=0; i<ctx.n; ++i)
            points.emplace_back(double(i), value(i));
        m_chart->setSeriesPoints(m_series, std::move(points));
        m_next=ctx.n;
    }

    void step(SceneContext &/*ctx*/, int /*frame*/) override
    {
        m_chart->appendPoint(m_series, double(m_next), value(m_next));
        ++m_next;
    }

private:
    static double value(int i)
    {
        return double(10+(i*37)%80);
    }

    controls::Chart *m_chart=nullptr;
    size_t m_series=0;
    int m_next=0;
};

const std::vector<SceneInfo> &scenes()
{
    static const std::vector<SceneInfo> list=
    {
        {"build", "construct an N-item tree of mixed controls under a bold title and run its first frame",
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
        {"vlist-variable-scroll", "turn the wheel one notch per frame over a variable-height VirtualList of N wrapped rows",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new VariableListScrollScene()); }},
        {"drag", "drag a Window of 40 controls by its title bar over N Labels",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new DragScene()); }},
        {"tree-toggle", "collapse and expand a 50-leaf node near the top of an N-node TreeControl",
            true, 1000, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new TreeToggleScene()); }},
        {"tree-relabel", "relabel a closed TreeControl node holding N rows per frame",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new TreeRelabelScene()); }},
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
        {"hidden-pages", "hover a header Button above K hidden pages of N Buttons, shown slots at both ends",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new HiddenPagesScene()); }},
        {"hidden-gaps", "N Buttons, all hidden but every K+1-th: runs of one shown slot between gaps of K",
            true, 1000, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new HiddenGapsScene()); }},
        {"code-edit", "type, break and join lines, undo and open and close a block scalar at line 500 of a highlighted CodeEditor of N YAML lines",
            true, 1000, 3, 24, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new CodeEditScene()); }},
        {"code-edit-wrap", "code-edit's edits at the end of a wrapped line in a wrapped CodeEditor of N lines",
            true, 1000, 3, 24, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new CodeEditWrapScene()); }},
        {"code-scroll", "scroll a highlighted CodeEditor of N YAML lines with line numbers one line per frame",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new CodeScrollScene()); }},
        {"log-append", "append 17 rows a frame to a following LogView of N wrapped rows",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new LogAppendScene(LogAppendScene::Mode::Following)); }},
        {"log-append-detached", "append 17 rows a frame to a LogView of N wrapped rows scrolled 300 rows up",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new LogAppendScene(LogAppendScene::Mode::Detached)); }},
        {"log-append-capped", "append 17 rows a frame to a following LogView of N rows capped at 2000",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new LogAppendScene(LogAppendScene::Mode::Capped)); }},
        {"graph-drag", "drag the first of N linked nodes in a NodeGraph by its title bar",
            true, 300, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new GraphDragScene()); }},
        {"graph-marquee", "grow a marquee from a NodeGraph's empty space over the first of N linked nodes",
            true, 300, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new GraphMarqueeScene()); }},
        {"graph-pan", "pan a NodeGraph of N linked nodes with a right drag",
            true, 300, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new GraphPanScene()); }},
        {"chart-append", "append a point per frame to a Chart's line of N points in a fixed x range",
            true, 1000, 3, 20, Scaling::Local,
            []{ return std::unique_ptr<Scene>(new ChartAppendScene()); }},
        {"graph-zoom", "zoom a NodeGraph of N linked nodes a wheel notch in and out in turn",
            true, 300, 3, 20, Scaling::Linear,
            []{ return std::unique_ptr<Scene>(new GraphZoomScene()); }},
    };
    return list;
}

}//namespace bench
}//namespace gweni
