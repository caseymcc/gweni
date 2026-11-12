/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A dock-None control is sized from the size it was asked for (setBounds /
 *  setSize outside the arrange code), its preferred size and its size flags,
 *  never from the bounds an earlier pass gave it: an Expand control that grew
 *  shrinks back once what made it grow is gone, inside a Base parent and inside
 *  a layout, and a canvas resize and back gives the first bounds again. A
 *  layout child is sized from its cell minus its margin, clamped at 0.
 *
 *  A size request caps a ScrollControl, so the library's own menus ask for no
 *  height: a Menu and a ComboBox's list are as high as their items.
 *
 *  A TextBox is Fixed: it is the size it asked for whatever its text, in a
 *  Base parent and in a layout cell (it scrolls its text, it is no Label that
 *  sizes to it), while docked Center it still fills its dock rect. So is
 *  every subclass with its default flags, at dock None and on each edge dock,
 *  whatever its text; a TextBoxMultiline docked Center fills it too.
 *
 *  A TextBox is sized by its request (sizedByRequest) on every flag: given a
 *  non-Fixed width it spans a Top dock, and its preferred width stays its
 *  request whatever is set or typed into it, so a parent sizing to its
 *  children does not follow the text. In Y only a wrapped box is: a one-line
 *  box with an Elastic height is as high as its text line. Base's dock
 *  branches leave a control Fixed on the dock's cross axis at its size: a
 *  CheckBox's and a RadioButton's 15x15 docked Left, a window's close button
 *  docked Right and a NumericUpDown's up button docked Top.
 */
#include "headlessFixture.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/dragNumber.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/menuStrip.h>
#include <gweni/controls/numericUpDown.h>
#include <gweni/controls/radioButton.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/windowControl.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <limits>
#include <string>

using namespace gweni;

namespace
{

//! Rect as text, so a failed check prints both sides.
std::string rectText(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

//! Prefers the size it is told to.
class Block: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Block, controls::Base)
    {}

    Size want;

    void calculateSize(skin::Base * /*skin*/, Dim dim) override
    {
        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }
};

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    ~Scene()
    {
        delete canvas;
    }

    void settle()
    {
        for(int i=0; i<10; ++i)
        {
            canvas->updateCanvas();
            canvas->renderCanvas();
            if(canvas->getLayoutStats().passes == 0)
                break;
        }
    }

    //! A panel docked Center in the canvas, so it takes the canvas size.
    controls::Base *centerPanel()
    {
        controls::Base *panel=canvas->newChild<controls::Base>("panel");
        panel->setDock(DockPosition::Center);
        return panel;
    }
};

}//namespace

TEST_CASE("dock None: Expand is at least its request and its preferred size, and shrinks back")
{
    Scene scene;
    Block *block=scene.centerPanel()->newChild<Block>("block");
    // placed where it asked, not aligned to the parent's top-left (the default)
    block->setAlignment(Alignment::Absolute);
    block->setBounds(30, 30, 200, 100);
    block->want=Size(50, 20);
    scene.settle();
    CHECK(rectText(block->getBounds()) == rectText(Rect(30, 30, 200, 100)));

    block->want=Size(300, 150);
    block->invalidate();
    scene.settle();
    CHECK(rectText(block->getBounds()) == rectText(Rect(30, 30, 300, 150)));

    // what made it grow is gone: back to the request, not stuck at 300x150
    block->want=Size(50, 20);
    block->invalidate();
    scene.settle();
    CHECK(rectText(block->getBounds()) == rectText(Rect(30, 30, 200, 100)));
    CHECK(rectText(block->getRequestedBounds()) == rectText(Rect(30, 30, 200, 100)));
}

TEST_CASE("dock None: Shrink, Elastic, Fixed and no request")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();

    Block *shrink=panel->newChild<Block>("shrink");
    shrink->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    shrink->setBounds(0, 0, 200, 100);
    shrink->want=Size(80, 120);

    Block *elastic=panel->newChild<Block>("elastic");
    elastic->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    elastic->setBounds(0, 0, 200, 100);
    elastic->want=Size(80, 120);

    controls::Base *fixed=panel->newChild<controls::Base>("fixed");
    fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    fixed->setBounds(0, 0, 70, 30);

    Block *unasked=panel->newChild<Block>("unasked");
    unasked->want=Size(40, 25);

    scene.settle();
    CHECK(shrink->getWidth() == 80);
    CHECK(shrink->getHeight() == 100);
    CHECK(elastic->getWidth() == 80);
    CHECK(elastic->getHeight() == 120);
    CHECK(fixed->getWidth() == 70);
    CHECK(fixed->getHeight() == 30);
    CHECK(unasked->getWidth() == 40);
    CHECK(unasked->getHeight() == 25);
    CHECK_FALSE(unasked->hasRequestedSize(Dim::X));

    // the arrange code's writes are not requests
    CHECK(rectText(shrink->getRequestedBounds()) == rectText(Rect(0, 0, 200, 100)));
    CHECK(rectText(elastic->getRequestedBounds()) == rectText(Rect(0, 0, 200, 100)));

    // Shrink grows back when its content does
    shrink->want=Size(300, 300);
    shrink->invalidate();
    scene.settle();
    CHECK(shrink->getWidth() == 200);
    CHECK(shrink->getHeight() == 100);
}

TEST_CASE("Fixed takes its minimum and maximum size from the request, per axis")
{
    Scene scene;
    controls::Base *control=scene.centerPanel()->newChild<controls::Base>("control");
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setBounds(0, 0, 70, 30);
    scene.settle();

    CHECK(control->getMinimumSize().width == 70);
    CHECK(control->getMinimumSize().height == 30);
    CHECK(control->getMaximumSize().width == 70);
    CHECK(control->getMaximumSize().height == 30);

    // a horizontal Shrink no longer decides the vertical maximum
    control->setSizeFlags({SizeFlag::Shrink, SizeFlag::Expand});
    control->setPreferredSize(Size(10, 10));
    CHECK(control->getMaximumSize().height == std::numeric_limits<int>::max());
}

TEST_CASE("setPos asks for a position only, setWidth for a width only")
{
    Scene scene;
    Block *block=scene.centerPanel()->newChild<Block>("block");
    block->setAlignment(Alignment::Absolute);
    block->setBounds(0, 0, 100, 50);
    block->want=Size(300, 20);
    scene.settle();
    REQUIRE(block->getWidth() == 300);

    // moving the grown control must not make its grown width the request
    block->setPos(40, 10);
    block->setHeight(60);
    CHECK(rectText(block->getRequestedBounds()) == rectText(Rect(40, 10, 100, 60)));

    block->want=Size(10, 10);
    block->invalidate();
    scene.settle();
    CHECK(rectText(block->getBounds()) == rectText(Rect(40, 10, 100, 60)));
}

TEST_CASE("a layout child gets its request up to the cell, and a canvas resize and back gives it again")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(30, 30, 200, 100);
    asked->want=Size(50, 20);

    Block *unasked=panel->newChild<Block>("unasked");
    unasked->want=Size(50, 20);

    scene.settle();
    // a layout places it; its size is its request
    CHECK(asked->getWidth() == 200);
    CHECK(asked->getHeight() == 100);
    // nobody asked: it fills the cell, as before
    CHECK(unasked->getWidth() == 640);

    const Rect askedBefore=asked->getBounds();
    const Rect unaskedBefore=unasked->getBounds();

    scene.canvas->setSize(1024, 768);
    scene.settle();
    CHECK(asked->getWidth() == 200);
    CHECK(asked->getHeight() == 100);
    CHECK(unasked->getWidth() == 1024);

    // narrower than the request: capped at the cell
    scene.canvas->setSize(150, 480);
    scene.settle();
    CHECK(asked->getWidth() == 150);

    scene.canvas->setSize(640, 480);
    scene.settle();
    CHECK(rectText(asked->getBounds()) == rectText(askedBefore));
    CHECK(rectText(unasked->getBounds()) == rectText(unaskedBefore));
}

TEST_CASE("a Fixed layout child advances a Vertical and a HorizontalLayout by its size and margins")
{
    for(const bool vertical:{true, false})
    {
        CAPTURE(vertical);
        Scene scene;
        controls::Base *panel=scene.centerPanel();
        if(vertical)
            panel->setLayout(new controls::VerticalLayout());
        else
            panel->setLayout(new controls::HorizontalLayout());

        Block *before=panel->newChild<Block>("before");
        before->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
        before->want=Size(50, 20);

        controls::Base *fixed=panel->newChild<controls::Base>("fixed");
        fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        fixed->setSize(70, 30);
        fixed->setMargin(Margin(3, 4, 5, 6));

        Block *after=panel->newChild<Block>("after");
        after->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
        after->want=Size(50, 20);

        scene.settle();
        // the layout's spacing is 2
        if(vertical)
        {
            CHECK(rectText(before->getBounds()) == rectText(Rect(0, 0, 50, 20)));
            CHECK(rectText(fixed->getBounds()) == rectText(Rect(3, 20+2+4, 70, 30)));
            CHECK(rectText(after->getBounds()) == rectText(Rect(0, 20+2+4+30+6+2, 50, 20)));
        }
        else
        {
            CHECK(rectText(before->getBounds()) == rectText(Rect(0, 0, 50, 20)));
            CHECK(rectText(fixed->getBounds()) == rectText(Rect(50+2+3, 4, 70, 30)));
            CHECK(rectText(after->getBounds()) == rectText(Rect(50+2+3+70+5+2, 0, 50, 20)));
        }
    }
}

TEST_CASE("a layout child with a margin is sized from its cell minus the margin")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *child=panel->newChild<controls::Base>("child");
    child->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    child->setMargin(Margin(10, 10, 10, 10));

    scene.settle();
    CHECK(rectText(child->getBounds()) == rectText(Rect(10, 10, 620, 460)));
}

TEST_CASE("a layout child's request wider than its cell minus the margin is capped there")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(0, 0, 700, 20);
    asked->want=Size(700, 20);
    asked->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    asked->setMargin(Margin(7, 3, 11, 5));

    scene.settle();
    // the cell is the panel width (the preferred 700+7+11 shrunk to it) and
    // 20+3+5 high
    CHECK(rectText(asked->getBounds()) == rectText(Rect(7, 3, 640-7-11, 20)));
}

TEST_CASE("Expand children with margins in a HorizontalLayout do not overlap or pass the panel edge")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::HorizontalLayout());

    controls::Base *c0=panel->newChild<controls::Base>("c0");
    controls::Base *c1=panel->newChild<controls::Base>("c1");
    for(controls::Base *c:{c0, c1})
    {
        c->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
        c->setMargin(Margin(5, 5, 5, 5));
    }

    scene.settle();
    CAPTURE(rectText(c0->getBounds()));
    CAPTURE(rectText(c1->getBounds()));
    CHECK(c0->getX() == 5);
    CHECK(c0->getWidth() > 0);
    CHECK(c1->getWidth() > 0);
    CHECK(c0->getX()+c0->getWidth()+5 <= c1->getX()-5);
    CHECK(c1->getX()+c1->getWidth() <= 640-5);
    CHECK(c0->getY() == 5);
    CHECK(c0->getHeight() == 480-5-5);
    CHECK(c1->getHeight() == 480-5-5);
}

TEST_CASE("a Fixed layout child with a margin keeps its requested size, also in a cell smaller than the margin")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *fixed=panel->newChild<controls::Base>("fixed");
    fixed->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    fixed->setSize(70, 30);
    fixed->setMargin(Margin(3, 4, 5, 6));

    scene.settle();
    CHECK(rectText(fixed->getBounds()) == rectText(Rect(3, 4, 70, 30)));

    scene.canvas->setSize(6, 8);
    scene.settle();
    CHECK(rectText(fixed->getBounds()) == rectText(Rect(3, 4, 70, 30)));
}

TEST_CASE("a layout cell smaller than the child's margins gives it size 0, never a negative size")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::Base *expand=panel->newChild<controls::Base>("expand");
    expand->setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    expand->setMargin(Margin(10, 10, 10, 10));

    Block *asked=panel->newChild<Block>("asked");
    asked->setBounds(0, 0, 50, 20);
    asked->setSizeFlags({SizeFlag::Shrink, SizeFlag::Shrink});
    asked->setMargin(Margin(10, 10, 10, 10));

    scene.canvas->setSize(15, 15);
    scene.settle();
    CAPTURE(rectText(expand->getBounds()));
    CAPTURE(rectText(asked->getBounds()));
    CHECK(expand->getX() == 10);
    CHECK(expand->getY() == 10);
    CHECK(expand->getWidth() == 0);
    CHECK(expand->getHeight() == 0);
    CHECK(asked->getWidth() == 0);
    CHECK(asked->getHeight() >= 0);
}

namespace
{

//! A panel with a layout holding a nested layout of the other direction, with
//! "inner" in it, and a Fixed 30x30 "after" past it along the row (vertical:
//! the column), whose position shows how long the outer layout made the
//! nested one.
struct NestedScene: Scene
{
    bool vertical;
    controls::LayoutItem *nested;
    Block *inner;
    controls::Base *after;

    NestedScene(bool vertical, SizeFlag innerFlag, int innerLength):
        vertical(vertical)
    {
        controls::Base *panel=centerPanel();
        if(vertical)
        {
            panel->setLayout(new controls::VerticalLayout());
            nested=new controls::HorizontalLayout();
        }
        else
        {
            panel->setLayout(new controls::HorizontalLayout());
            nested=new controls::VerticalLayout();
        }
        panel->getLayout()->addLayout(nested);

        inner=panel->newChild<Block>("inner");
        nested->addControl(inner);
        setInner(innerFlag, innerLength);

        after=panel->newChild<controls::Base>("after");
        after->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        after->setSize(30, 30);
    }

    //! inner's flag and length along the outer layout's direction; 20 and
    //! Fixed across it
    void setInner(SizeFlag flag, int length)
    {
        if(vertical)
        {
            inner->setSizeFlags({SizeFlag::Fixed, flag});
            inner->setSize(20, length);
            inner->want=Size(20, length);
        }
        else
        {
            inner->setSizeFlags({flag, SizeFlag::Fixed});
            inner->setSize(length, 20);
            inner->want=Size(length, 20);
        }
        inner->invalidate();
    }

    int nestedLength() const
    {
        return vertical?nested->getPreferredSize().height:nested->getPreferredSize().width;
    }

    SizeFlag nestedFlag() const
    {
        return vertical?nested->getSizeFlags().vertical:nested->getSizeFlags().horizontal;
    }

    int afterPos() const
    {
        return vertical?after->getY():after->getX();
    }
};

}//namespace

TEST_CASE("a nested layout's preferred length shrinks back with its child, as in a fresh layout")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        NestedScene scene(vertical, SizeFlag::Shrink, 300);
        scene.settle();
        REQUIRE(scene.nestedLength() == 300);

        scene.setInner(SizeFlag::Shrink, 50);
        scene.settle();

        NestedScene fresh(vertical, SizeFlag::Shrink, 50);
        fresh.settle();

        // not stuck at the 300 the child was before
        CHECK(fresh.nestedLength() == 50);
        CHECK(scene.nestedLength() == fresh.nestedLength());
        CHECK(rectText(scene.nested->getBounds()) == rectText(fresh.nested->getBounds()));
        CHECK(rectText(scene.inner->getBounds()) == rectText(fresh.inner->getBounds()));
        CHECK(rectText(scene.after->getBounds()) == rectText(fresh.after->getBounds()));

        // and laying it out again and again changes nothing
        const Rect nestedBounds=scene.nested->getBounds();
        const Rect afterBounds=scene.after->getBounds();
        for(int i=0; i<5; ++i)
        {
            scene.inner->invalidate();
            scene.settle();
        }
        CHECK(scene.nestedLength() == 50);
        CHECK(rectText(scene.nested->getBounds()) == rectText(nestedBounds));
        CHECK(rectText(scene.after->getBounds()) == rectText(afterBounds));
    }
}

TEST_CASE("a nested layout's size flag follows its child from Expand to Fixed, as in a fresh layout")
{
    for(const bool vertical:{false, true})
    {
        CAPTURE(vertical);
        NestedScene scene(vertical, SizeFlag::Expand, 50);
        scene.settle();
        // the Expand nested layout takes the extra space: "after" is at the far end
        REQUIRE(sizeFlagName(scene.nestedFlag()) == "Expand");
        REQUIRE(scene.afterPos() > 52);

        scene.setInner(SizeFlag::Fixed, 50);
        scene.settle();

        NestedScene fresh(vertical, SizeFlag::Fixed, 50);
        fresh.settle();

        CHECK(sizeFlagName(fresh.nestedFlag()) == "Fixed");
        CHECK(sizeFlagName(scene.nestedFlag()) == "Fixed");
        CHECK(rectText(scene.nested->getBounds()) == rectText(fresh.nested->getBounds()));
        CHECK(rectText(scene.inner->getBounds()) == rectText(fresh.inner->getBounds()));
        // the nested layout is its child's 50 long again, "after" 2 (the spacing) past it
        CHECK(fresh.afterPos() == 52);
        CHECK(rectText(scene.after->getBounds()) == rectText(fresh.after->getBounds()));
    }
}

namespace
{

//! The sum of the item heights and the 2 unit padding a Menu gives its inner
//! panel above and below them.
int itemsHeight(controls::Menu *menu)
{
    int height=0;

    for(controls::Base *item : menu->getChildren())
        height+=item->getHeight();

    return height+2+2;
}

controls::Menu *openMenuOf(controls::Canvas *canvas)
{
    for(controls::Base *child : canvas->getActualChildren())
    {
        auto *menu=dynamic_cast<controls::Menu *>(child);

        if(menu && !menu->hidden())
            return menu;
    }
    return nullptr;
}

}//namespace

TEST_CASE("a Menu asks for no height and is as high as its items")
{
    Scene scene;
    controls::Menu *menu=scene.canvas->newChild<controls::Menu>("menu");
    menu->setPos(20, 30);
    menu->addItem("One");
    menu->addItem("Two");
    menu->addItem("Three");
    scene.settle();

    CHECK_FALSE(menu->hasRequestedSize(Dim::Y));
    CHECK(menu->getHeight() > 10);
    CHECK(menu->getHeight() == itemsHeight(menu));
    CHECK(menu->getX() == 20);
    CHECK(menu->getY() == 30);

    // more items: taller, and still no bar
    menu->addItem("Four");
    menu->addItem("Five");
    scene.settle();
    CHECK(menu->getHeight() == itemsHeight(menu));
    CHECK(menu->getChildren().size() == 5);
}

TEST_CASE("a ComboBox's list fits its items each time it is opened")
{
    Scene scene;
    controls::ComboBox *combo=scene.canvas->newChild<controls::ComboBox>("combo");
    combo->setPos(10, 10);
    combo->addItem("One");
    combo->addItem("Two");
    combo->addItem("Three");
    scene.settle();

    combo->openList();
    scene.settle();
    controls::Menu *menu=openMenuOf(scene.canvas);
    REQUIRE(menu != nullptr);
    CHECK_FALSE(menu->hasRequestedSize(Dim::Y));
    CHECK(menu->getX() == combo->getX());
    CHECK(menu->getY() == combo->getY()+combo->getHeight());
    CHECK(menu->getWidth() == combo->getWidth());
    const int threeItems=menu->getHeight();
    CHECK(threeItems == itemsHeight(menu));

    // opened again with more items, it grows to fit them
    combo->closeList();
    scene.settle();
    combo->addItem("Four");
    combo->addItem("Five");
    combo->openList();
    scene.settle();
    CHECK(menu->getHeight() == itemsHeight(menu));
    CHECK(menu->getHeight() > threeItems);

    // and with fewer, it is not held at the height it had
    combo->closeList();
    scene.settle();
    combo->clearItems();
    combo->addItem("One");
    combo->openList();
    scene.settle();
    CHECK(menu->getHeight() == itemsHeight(menu));
    CHECK(menu->getHeight() < threeItems);
}

TEST_CASE("a MenuStrip in a layout is as high as its items, its 200x22 only the least it gets")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());
    controls::MenuStrip *strip=panel->newChild<controls::MenuStrip>("strip");
    controls::MenuItem *file=strip->addItem("File");
    strip->addItem("Edit");
    scene.settle();

    // not the cell's 480, and not capped at the 22 it asked for
    CHECK(strip->getHeight() == std::max(22, file->getHeight()));
    CHECK(strip->getWidth() == 200);
}

TEST_CASE("a TextBoxNumeric in a Base parent is the 26x16 it asked for, whatever its digits")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    controls::TextBoxNumeric *numeric=panel->newChild<controls::TextBoxNumeric>("numeric");
    numeric->setAlignment(Alignment::Absolute);
    numeric->setPos(105, 0);
    numeric->setSize(26, 16);

    for(const char *digits:{"", "5", "50", "255", "5", ""})
    {
        CAPTURE(digits);
        // each a setText after the last layout, so the digit count changes
        numeric->setText(digits);
        scene.settle();
        CHECK(rectText(numeric->getBounds()) == rectText(Rect(105, 0, 26, 16)));
    }
}

TEST_CASE("a TextBox in a VerticalLayout stays 200x20 with a text wider than 200")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());
    controls::TextBox *box=panel->newChild<controls::TextBox>("box");
    const String wide(100, 'w');
    box->setText(wide);
    scene.settle();

    // the text would widen it past its request, up to the 640 cell
    REQUIRE(scene.renderer.measureText(box->getFont(), wide).x > 200);
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 0, 200, 20)));

    // typing more scrolls the text; it never grows the box
    box->setText(wide+wide);
    scene.settle();
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 0, 200, 20)));
}

TEST_CASE("a TextBox docked Center fills its dock rect")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setAlignment(Alignment::Absolute);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    panel->setBounds(10, 10, 300, 40);
    controls::TextBox *box=panel->newChild<controls::TextBox>("box");
    box->setDock(DockPosition::Center);
    box->setText("short");
    scene.settle();
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 0, 300, 40)));

    // nor does a text wider than the rect move it
    box->setText(String(100, 'w'));
    scene.settle();
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 0, 300, 40)));
}

namespace
{

//! A string `font` measures at least `width` wide.
String atLeastWide(Scene &scene, const Font &font, int width)
{
    String s;
    while(scene.renderer.measureText(font, s).x < width)
        s+="w";
    return s;
}

//! A control of type T with its default flags, in a 300x100 Fixed panel at
//! dock `dock`, settled with a text far wider than it; its bounds must be
//! its request (`wantSize`), at the dock's edge, whichever the dock.
template<typename T>
void checkKeepsRequestDocked(Scene &scene, DockPosition dock, const Size &wantSize)
{
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setAlignment(Alignment::Absolute);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    panel->setBounds(0, 0, 300, 100);

    T *box=panel->newChild<T>("box");
    box->setDock(dock);
    scene.settle();

    const Size request(box->getRequestedBounds().w, box->getRequestedBounds().h);
    CHECK(request.width == wantSize.width);
    CHECK(request.height == wantSize.height);

    const String wide=atLeastWide(scene, box->getFont(), 600);
    for(const String &s:{String(), wide, String("x")})
    {
        box->setText(s);
        scene.settle();
        CHECK(box->getWidth() == wantSize.width);
        CHECK(box->getHeight() == wantSize.height);
    }

    if(dock == DockPosition::Right)
        CHECK(box->getX() == 300-wantSize.width);
    if(dock == DockPosition::Bottom)
        CHECK(box->getY() == 100-wantSize.height);

    delete panel;
}

template<typename T>
void checkKeepsRequestOnEveryDock(const Size &wantSize)
{
    for(const DockPosition dock:{DockPosition::None, DockPosition::Top, DockPosition::Bottom,
        DockPosition::Left, DockPosition::Right})
    {
        CAPTURE(static_cast<int>(dock));
        Scene scene;
        checkKeepsRequestDocked<T>(scene, dock, wantSize);
    }
}

}//namespace

TEST_CASE("a TextBox and each subclass with its default flags keeps its request at dock None and on each edge dock")
{
    SUBCASE("TextBox") { checkKeepsRequestOnEveryDock<controls::TextBox>(Size(200, 20)); }
    SUBCASE("TextBoxNumeric") { checkKeepsRequestOnEveryDock<controls::TextBoxNumeric>(Size(200, 20)); }
    SUBCASE("PasswordTextBox") { checkKeepsRequestOnEveryDock<controls::PasswordTextBox>(Size(200, 20)); }
    SUBCASE("TextBoxMultiline") { checkKeepsRequestOnEveryDock<controls::TextBoxMultiline>(Size(200, 20)); }
    SUBCASE("NumericUpDown") { checkKeepsRequestOnEveryDock<controls::NumericUpDown>(Size(100, 20)); }
    SUBCASE("DragNumberTextBox") { checkKeepsRequestOnEveryDock<controls::internal::DragNumberTextBox>(Size(200, 20)); }
}

TEST_CASE("a TextBoxMultiline docked Center fills its dock rect, whatever its text")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setAlignment(Alignment::Absolute);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    panel->setBounds(10, 10, 300, 120);
    controls::TextBoxMultiline *box=panel->newChild<controls::TextBoxMultiline>("box");
    box->setDock(DockPosition::Center);

    String lines;
    for(int i=0; i<30; ++i)
        lines+="line "+std::to_string(i)+"\n";

    for(const String &s:{String(), atLeastWide(scene, box->getFont(), 600), lines})
    {
        box->setText(s);
        scene.settle();
        CHECK(rectText(box->getBounds()) == rectText(Rect(0, 0, 300, 120)));
    }
}

TEST_CASE("an {Elastic, Fixed} TextBox docked Top spans its parent, whose width does not follow the text")
{
    Scene scene;
    // sizes to its children: no request, Elastic
    controls::Base *parent=scene.canvas->newChild<controls::Base>("parent");
    parent->setAlignment(Alignment::Absolute);
    parent->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    REQUIRE_FALSE(parent->hasRequestedSize(Dim::X));

    // a wider sibling, so the parent is wider than the box's 200
    Block *wider=parent->newChild<Block>("wider");
    wider->setDock(DockPosition::Top);
    wider->want=Size(350, 10);

    controls::TextBox *box=parent->newChild<controls::TextBox>("box");
    box->setDock(DockPosition::Top);
    box->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    box->setText("");
    scene.settle();

    REQUIRE(parent->getWidth() == 350);
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 10, 350, 20)));
    CHECK(box->getPreferredSize().width == 200);
    const Rect parentBefore=parent->getBounds();
    const Size parentPreferred=parent->getPreferredSize();

    const String wide=atLeastWide(scene, box->getFont(), 600);
    box->setText(wide);
    scene.settle();
    CHECK(box->getPreferredSize().width == 200);
    CHECK(parent->getPreferredSize().width == parentPreferred.width);
    CHECK(rectText(parent->getBounds()) == rectText(parentBefore));
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 10, 350, 20)));

    // typing it, a character a frame, neither
    box->setText("");
    scene.settle();
    box->focus();
    for(const char c:wide)
    {
        box->onChar(static_cast<UnicodeChar>(c));
        scene.settle();
    }
    REQUIRE(box->getText() == wide);
    CHECK(box->getPreferredSize().width == 200);
    CHECK(parent->getPreferredSize().width == parentPreferred.width);
    CHECK(rectText(parent->getBounds()) == rectText(parentBefore));
    CHECK(rectText(box->getBounds()) == rectText(Rect(0, 10, 350, 20)));

    // with nothing wider, the parent is the box's request
    wider->want=Size(0, 10);
    wider->invalidate();
    scene.settle();
    CHECK(parent->getWidth() == 200);
    CHECK(box->getWidth() == 200);
}

TEST_CASE("a Fixed control docked Left or Right keeps its size: a CheckBox, a RadioButton, a window's close button")
{
    Scene scene;
    controls::Base *panel=scene.centerPanel();
    panel->setLayout(new controls::VerticalLayout());

    controls::CheckBoxWithLabel *check=panel->newChild<controls::CheckBoxWithLabel>("check");
    check->label()->setText("A check box with a label");
    controls::LabeledRadioButton *radio=panel->newChild<controls::LabeledRadioButton>("radio");
    radio->getLabel()->setText("A radio button with a label");

    // a plain Fixed 15x15 docked Left in a taller parent
    controls::Base *tall=panel->newChild<controls::Base>("tall");
    tall->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    tall->setSize(300, 60);
    controls::Base *square=tall->newChild<controls::Base>("square");
    square->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    square->setSize(15, 15);
    square->setDock(DockPosition::Left);

    controls::WindowControl *window=scene.canvas->newChild<controls::WindowControl>("window");
    window->setBounds(50, 50, 300, 200);
    scene.settle();

    CHECK(check->checkBox()->getWidth() == 15);
    CHECK(check->checkBox()->getHeight() == 15);
    CHECK(radio->getRadioButton()->getWidth() == 15);
    CHECK(radio->getRadioButton()->getHeight() == 15);
    CHECK(rectText(square->getBounds()) == rectText(Rect(0, 0, 15, 15)));

    controls::Base *close=window->findChildByName("closeButton", true);
    REQUIRE(close != nullptr);
    CHECK(close->getWidth() == 29);
    CHECK(close->getHeight() == 24);
    CHECK(close->getX()+close->getWidth() == close->getParent()->getWidth()-close->getParent()->getPadding().right);
}

TEST_CASE("a NumericUpDown's up button docked Top keeps its height, with default and with spanning flags")
{
    for(const bool spanning:{false, true})
    {
        CAPTURE(spanning);
        Scene scene;
        controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
        panel->setAlignment(Alignment::Absolute);
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(0, 0, 300, 100);
        controls::NumericUpDown *number=panel->newChild<controls::NumericUpDown>("number");
        number->setDock(DockPosition::Top);
        if(spanning)
            number->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
        number->setText("12345678901234567890");
        scene.settle();

        CHECK(number->getWidth() == (spanning?300:100));
        CHECK(number->getHeight() == 20);
        CHECK(number->getPreferredSize().width == 100);

        controls::Base *up=nullptr;
        controls::Base *down=nullptr;
        for(controls::Base *child:number->getChildren())
            for(controls::Base *button:child->getChildren())
            {
                if(gweni_cast<controls::NumericUpDownButton_Up>(button))
                    up=button;
                else if(gweni_cast<controls::NumericUpDownButton_Down>(button))
                    down=button;
            }
        REQUIRE(up != nullptr);
        REQUIRE(down != nullptr);
        CHECK(up->getY() == 0);
        CHECK(up->getHeight() == 8);
        CHECK(down->getY() == 8);
        CHECK(down->getHeight() == 8);
        CHECK(up->getWidth() == up->getParent()->getWidth());
        // the splitter at the box's right, inside its padding
        CHECK(up->getParent()->getX()+up->getParent()->getWidth()
            == number->getWidth()-number->getPadding().right);
    }
}

TEST_CASE("an Elastic height on a one-line TextBox is its text line's, a wrapped one's its request")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>("panel");
    panel->setAlignment(Alignment::Absolute);
    panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    panel->setBounds(0, 0, 300, 200);

    controls::TextBox *line=panel->newChild<controls::TextBox>("line");
    line->setDock(DockPosition::Top);
    line->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    line->setText("one line");

    controls::TextBoxMultiline *wrapped=panel->newChild<controls::TextBoxMultiline>("wrapped");
    wrapped->setDock(DockPosition::Top);
    wrapped->setSizeFlags({SizeFlag::Elastic, SizeFlag::Elastic});
    String lines;
    for(int i=0; i<30; ++i)
        lines+="line "+std::to_string(i)+"\n";
    wrapped->setText(lines);
    scene.settle();

    // a property row's box: as high as its line, not the 20 it asked for
    const int lineHeight=scene.renderer.lineHeight(line->getFont())
        +line->getPadding().top+line->getPadding().bottom;
    REQUIRE(lineHeight != 20);
    CHECK(line->getHeight() == lineHeight);
    // thirty lines scroll inside the 20 it asked for
    CHECK(wrapped->getHeight() == 20);
    CHECK(wrapped->getPreferredSize().height == 20);
}
