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
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/treeControl.h>

#include <algorithm>
#include <cstdio>

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

}//namespace

const std::vector<SceneInfo> &scenes()
{
    static const std::vector<SceneInfo> list=
    {
        {"build", "construct an N-item tree of mixed controls and run its first frame",
            true, 1000, 0, 1, []{ return std::unique_ptr<Scene>(new MixedTreeScene()); }},
        {"idle", "the build tree, settled, with no input",
            true, 1000, 5, 10, []{ return std::unique_ptr<Scene>(new MixedTreeScene()); }},
        {"hover-sweep", "move the mouse one row down a ListBox of N rows per frame",
            true, 1000, 3, 20, []{ return std::unique_ptr<Scene>(new HoverSweepScene()); }},
        {"set-text", "K of N Labels get a new string of the same length per frame",
            true, 1000, 3, 20, []{ return std::unique_ptr<Scene>(new SetTextScene()); }},
    };
    return list;
}

}//namespace bench
}//namespace gweni
