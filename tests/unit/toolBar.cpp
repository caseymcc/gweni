/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ToolBar's overflow on the StubRenderer. A toolbar whose container is wide
 *  enough shows every item and no overflow button; its preferred width is
 *  every item's and its minimumWidth() the AlwaysShow items' and the button's.
 *  As the container narrows, the Normal items overflow from the end one at a
 *  time, hidden, the overflow button shown at the right end with the row
 *  keeping its room; AlwaysShow items and an addControl item with no overflow
 *  text never go, AlwaysOverflow items are always gone (the button showing
 *  however wide the bar is). A separator never starts or ends the shown row
 *  nor follows another shown one. As it widens the items come back in order,
 *  the button going once all fit. Every one of those width changes lays out
 *  in one update-phase pass, with layout verification finding nothing, and
 *  changes no preferred size of the toolbar.
 */
#include "headlessFixture.h"

#include <gweni/controls/separator.h>
#include <gweni/controls/splitButton.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/toolBar.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;
using controls::OverflowPriority;
using controls::ToolBar;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *box;
    ToolBar *bar;

    Scene()
    {
        controls::setLayoutVerify(true);
        canvas=new controls::Canvas(&skin);
        canvas->setSize(1200, 300);
        box=canvas->newChild<controls::Base>();
        box->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        box->setBounds(0, 0, 1100, 100);
        bar=box->newChild<ToolBar>();
    }

    ~Scene()
    {
        delete canvas;
        controls::setLayoutVerify(false);
    }

    const controls::LayoutStats &frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        return canvas->getLayoutStats();
    }

    void settle()
    {
        for(int i=0; i < 10 && frame().passes != 0; ++i)
            ;
    }

    //! The shown items' names, in order, "|" for a separator and "+" for
    //! the overflow button.
    std::string shown() const
    {
        std::string s;

        for(controls::Base *item : bar->getItems())
        {
            if(item->hidden())
                continue;
            if(!s.empty())
                s+=" ";
            s+=gweni_cast<controls::Separator>(item)?"|":item->getName();
        }
        if(!bar->getOverflowButton()->hidden())
            s+=s.empty()?"+":" +";
        return s;
    }

    //! A container width that gives the bar exactly \p barWidth.
    void setWidth(int barWidth)
    {
        box->setBounds(0, 0, barWidth, 100);
    }
};

void checkOnePass(const controls::LayoutStats &stats)
{
    INFO("re-dirtied by " << std::string(stats.redirtiedType?stats.redirtiedType:"none")
        << "#" << stats.redirtiedId);
    CHECK(stats.passes == 1);
    CHECK(stats.redirtiedType == nullptr);
    CHECK_FALSE(stats.renderLayout);
}

controls::ToolButton *button(ToolBar *bar, const char *name)
{
    controls::ToolButton *b=bar->addButton(std::string(name)+" button");

    b->setName(name);
    return b;
}

//! a b | c d | e f: six buttons of the same width.
void fill(ToolBar *bar)
{
    button(bar, "a1");
    button(bar, "b1");
    bar->addSeparator();
    button(bar, "c1");
    button(bar, "d1");
    bar->addSeparator();
    button(bar, "e1");
    button(bar, "f1");
}

}//namespace

TEST_CASE("ToolBar: Normal items overflow from the end one at a time, in one pass, and come back in order")
{
    Scene scene;

    fill(scene.bar);
    scene.settle();

    const size_t failures=controls::layoutVerifyFailures();
    const int full=scene.bar->getPreferredSize().width;
    const int item=scene.bar->getItems()[0]->getWidth();
    const int button=scene.bar->getOverflowButton()->getPreferredSize().width;
    const int height=scene.bar->getPreferredSize().height;

    CHECK(item > 0);
    CHECK(full == 2*ToolBar::Padding+6*item+2*ToolBar::SeparatorSlot+7*ToolBar::Spacing);
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");
    CHECK(scene.bar->getOverflowedItems().empty());

    scene.setWidth(full);
    scene.settle();
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");

    // each width the bar of the first k items and the button needs
    const auto rowOf=[&](int items, int separators)
        {
            return 2*ToolBar::Padding+items*item+separators*ToolBar::SeparatorSlot
                +(items+separators)*ToolBar::Spacing+button;
        };
    const std::vector<std::pair<int, std::string>> steps=
    {
        {full-1, "a1 b1 | c1 d1 | e1 +"},
        {rowOf(5, 2)-1, "a1 b1 | c1 d1 +"},
        {rowOf(4, 1)-1, "a1 b1 | c1 +"},
        {rowOf(3, 1)-1, "a1 b1 +"},
        {rowOf(2, 0)-1, "a1 +"},
        {rowOf(1, 0)-1, "+"},
    };
    std::vector<size_t> serials;

    for(const auto &step : steps)
    {
        INFO("width " << step.first);
        scene.setWidth(step.first);
        checkOnePass(scene.frame());
        CHECK(scene.shown() == step.second);
        CHECK(scene.bar->getPreferredSize().width == full);
        CHECK(scene.bar->getPreferredSize().height == height);
        CHECK(controls::layoutVerifyFailures() == failures);
        CHECK(scene.frame().passes == 0);
        serials.push_back(scene.bar->getOverflowSerial());
    }
    CHECK(scene.bar->getOverflowedItems().size() == 8);
    for(size_t i=1; i < serials.size(); ++i)
        CHECK(serials[i] != serials[i-1]);

    // and back, in order
    for(size_t n=steps.size()-1; n > 0; --n)
    {
        INFO("width " << steps[n-1].first);
        scene.setWidth(steps[n-1].first);
        checkOnePass(scene.frame());
        CHECK(scene.shown() == steps[n-1].second);
        CHECK(controls::layoutVerifyFailures() == failures);
    }
    scene.setWidth(full);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 b1 | c1 d1 | e1 f1");
    CHECK(scene.bar->getOverflowedItems().empty());
    CHECK(controls::layoutVerifyFailures() == failures);
}

TEST_CASE("ToolBar: AlwaysShow and AlwaysOverflow items, and a control with no menu form, keep their places")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    controls::ToolButton *a=button(bar, "a1");
    button(bar, "b1");
    controls::ToolButton *c=button(bar, "c1");
    controls::TextBox *search=bar->newChild<controls::TextBox>();

    search->setName("s1");
    search->setWidth(80);
    bar->addControl(search);
    controls::ToolButton *d=button(bar, "d1");

    bar->setOverflowPriority(c, OverflowPriority::AlwaysShow);
    bar->setOverflowPriority(d, OverflowPriority::AlwaysOverflow);
    scene.settle();

    // d is in the menu however wide the bar is
    CHECK(scene.shown() == "a1 b1 c1 s1 +");
    CHECK(bar->isOverflowed(d));
    CHECK_FALSE(bar->canOverflow(search));
    CHECK(bar->getPreferredSize().width > bar->minimumWidth());

    scene.setWidth(bar->minimumWidth());
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 s1 +");

    // narrower still: the AlwaysShow items stay, cut by the toolbar
    scene.setWidth(bar->minimumWidth()/2);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 s1 +");

    bar->setOverflowPriority(d, OverflowPriority::Normal);
    bar->setOverflowPriority(a, OverflowPriority::AlwaysShow);
    scene.setWidth(1100);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 b1 c1 s1 d1");

    // with an overflow text it may go
    bar->setOverflowText(search, "Search");
    CHECK(bar->canOverflow(search));
}

TEST_CASE("ToolBar: a separator never starts or ends the shown row, nor follows another")
{
    Scene scene;
    ToolBar *bar=scene.bar;

    bar->addSeparator();
    controls::ToolButton *a=button(bar, "a1");
    bar->addSeparator();
    controls::ToolButton *b=button(bar, "b1");
    bar->addSeparator();
    button(bar, "c1");
    bar->addSeparator();

    scene.settle();
    CHECK(scene.shown() == "a1 | b1 | c1");

    // b gone: its separators would meet
    bar->setOverflowPriority(b, OverflowPriority::AlwaysOverflow);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "a1 | c1 +");

    bar->setOverflowPriority(a, OverflowPriority::AlwaysOverflow);
    checkOnePass(scene.frame());
    CHECK(scene.shown() == "c1 +");
}

TEST_CASE("ToolBar: an item changed while overflowed comes back at its new size")
{
    Scene scene;

    fill(scene.bar);
    scene.settle();

    const size_t failures=controls::layoutVerifyFailures();
    const int full=scene.bar->getPreferredSize().width;
    controls::ToolButton *f=gweni_cast<controls::ToolButton>(scene.bar->getItems().back());
    const int width=f->getWidth();

    scene.setWidth(full-1);
    scene.settle();
    REQUIRE(f->hidden());

    // its new text is measured though it is hidden: the bar's preferred
    // width follows it
    f->setText("a much longer caption");
    checkOnePass(scene.frame());
    CHECK(scene.bar->getPreferredSize().width > full);
    CHECK(f->getPreferredSize().width > width);

    scene.setWidth(scene.bar->getPreferredSize().width);
    checkOnePass(scene.frame());
    CHECK_FALSE(f->hidden());
    CHECK(f->getWidth() == f->getPreferredSize().width);
    CHECK(f->getWidth() > width);
    CHECK(controls::layoutVerifyFailures() == failures);
}
