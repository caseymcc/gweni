/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Only the app's calls are requests; bounds the library computes are not.
 *  A control places the children it owns (Placement::Owner) through
 *  Base::placeChild, which records no request: the generic arrange leaves such
 *  a child alone and it does not count in its parent's preferred size. So the
 *  preferred sizes of a CrossSplitter, a Properties and a slider come from
 *  requests and content only, and are the same after a trip to another canvas
 *  size as on a fresh canvas. A request made while a pass runs is counted in
 *  LayoutStats::requests.
 *
 *  A wrapped Text asks for the width the app asked its parent to be, less the
 *  parent's padding, or with no such request for its longest word; its height
 *  is that of its lines at the width it is given.
 *
 *  Outside a pass too, what the library writes is computed: a slider's value,
 *  a click or a drag on its thumb and a drag of a CrossSplitter bar leave
 *  every request as it was. A window dragged by its title bar is where the
 *  user put it, a request.
 */
#include "headlessFixture.h"

#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/label.h>
#include <gweni/controls/layout/layout.h>
#include <gweni/controls/properties.h>
#include <gweni/controls/text.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/controls/windowControl.h>

#include <doctest/doctest.h>

#include <string>
#include <vector>

using namespace gweni;

namespace
{

std::string text(const Rect &r)
{
    return "("+std::to_string(r.x)+", "+std::to_string(r.y)+", "+std::to_string(r.w)+", "+std::to_string(r.h)+")";
}

//! One line per control, depth-first: type, bounds and preferred size, so two
//! trees compare as text and a failure shows the line that differs.
void listTree(controls::Base *control, int depth, std::vector<std::string> &out)
{
    const Size &p=control->getPreferredSize();

    out.push_back(std::string(static_cast<size_t>(depth)*2, ' ')+control->getTypeName()+
        " bounds="+text(control->getBounds())+" pref=("+std::to_string(p.width)+", "+
        std::to_string(p.height)+")");

    for(auto &&child : control->getChildren())
        listTree(child, depth+1, out);
}

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    //! Requests counted over every frame() so far.
    int requests=0;
    const char *requestedType=nullptr;

    explicit Scene(int width=640, int height=480)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(width, height);
    }

    ~Scene()
    {
        delete canvas;
    }

    controls::LayoutStats frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &stats=canvas->getLayoutStats();

        requests+=stats.requests;
        if(!requestedType)
            requestedType=stats.requestedType;
        return stats;
    }

    void settle()
    {
        for(int i=0; i<10 && frame().passes != 0; ++i)
            ;
    }

    std::vector<std::string> tree()
    {
        std::vector<std::string> out;

        listTree(canvas, 0, out);
        return out;
    }
};

void checkSameTree(const std::vector<std::string> &a, const std::vector<std::string> &b)
{
    REQUIRE(a.size() == b.size());

    for(size_t i=0; i<a.size(); ++i)
        CHECK(a[i] == b[i]);
}

//! Places its one part itself, 5 in from each side and 7 high, from layout().
class Holder: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Holder, controls::Base)
    {
        part=newChild<controls::Base>("part");
        part->setPlacement(controls::Placement::Owner);
        // Ignored: its owner places it.
        part->setDock(DockPosition::Center);
        part->setSize(300, 300);
    }

    controls::Base *part;

protected:
    void layout(skin::Base *skin) override
    {
        placeChild(part, 5, 5, getWidth()-10, 7);
        ParentClass::layout(skin);
    }
};

//! Moves its one child with setPos from layout(): a request made in a pass.
class Requester: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Requester, controls::Base)
    {
        child=newChild<controls::Base>("child");
    }

    controls::Base *child;

protected:
    void layout(skin::Base *skin) override
    {
        child->setPos(getWidth()/2, 0);
        ParentClass::layout(skin);
    }
};

}//namespace

TEST_CASE("an Owner child is placed by its owner alone, records no request and does not count in its size")
{
    Scene scene;
    Holder *holder=scene.canvas->newChild<Holder>();

    holder->setBounds(10, 10, 100, 50);

    const Rect requested=holder->part->getRequestedBounds();

    scene.settle();

    // The generic arrange would have docked it Center over the whole holder.
    CHECK(text(holder->part->getBounds()) == text(Rect(5, 5, 90, 7)));
    // Only the constructor's setSize asked: placeChild left the request alone.
    CHECK(text(holder->part->getRequestedBounds()) == text(requested));
    // Its 300x300 does not count in the holder's size, which is only padding.
    CHECK(holder->getPreferredSize().width == 0);
    CHECK(holder->getPreferredSize().height == 0);
    CHECK(scene.requests == 0);

    holder->setSize(200, 50);
    scene.settle();

    CHECK(text(holder->part->getBounds()) == text(Rect(5, 5, 190, 7)));
    CHECK(text(holder->part->getRequestedBounds()) == text(requested));
    CHECK(scene.requests == 0);
}

TEST_CASE("a child given back to the generic arrange is docked and counted again")
{
    Scene scene;
    controls::Base *parent=scene.canvas->newChild<controls::Base>("parent");
    controls::Label *child=parent->newChild<controls::Label>("child");

    parent->setPos(10, 10);
    child->setText("Owned");
    child->setPos(4, 6);
    child->setPlacement(controls::Placement::Owner);
    scene.settle();

    CHECK(parent->getPreferredSize().width == 0);
    CHECK(parent->getPreferredSize().height == 0);

    child->setPlacement(controls::Placement::Parent);
    scene.settle();

    // Counted at the position it asked for, with its preferred size.
    const Size &preferred=child->getPreferredSize();

    CHECK(preferred.width > 0);
    CHECK(parent->getPreferredSize().width == 4+preferred.width);
    CHECK(parent->getPreferredSize().height == 6+preferred.height);
    CHECK(text(child->getBounds()) == text(Rect(4, 6, preferred.width, preferred.height)));
}

TEST_CASE("a request recorded while a pass runs is counted, with the first type that made it")
{
    Scene scene;
    Requester *requester=scene.canvas->newChild<Requester>();

    requester->setBounds(0, 0, 100, 20);

    // App calls between frames are requests too, but no pass is running.
    const controls::LayoutStats stats=scene.frame();

    CHECK(stats.requests >= 1);
    REQUIRE(stats.requestedType != nullptr);
    CHECK(std::string(stats.requestedType) == "Base");
    CHECK(requester->child->getRequestedBounds().x == 50);
}

TEST_CASE("CrossSplitter's preferred sizes do not depend on the canvas sizes it had before")
{
    // LabelMultilineDemo's arrangement: wrapped Labels in the four panels of a
    // CrossSplitter docked Center with margin 10 in a VerticalLayout.
    auto build=[](Scene &scene)
    {
        controls::Base *host=scene.canvas->newChild<controls::Base>("host");
        host->setLayout(new controls::VerticalLayout());
        host->setDock(DockPosition::Center);

        controls::CrossSplitter *splitter=host->newChild<controls::CrossSplitter>("splitter");
        splitter->setDock(DockPosition::Center);
        splitter->setMargin(Margin(10, 10, 10, 10));

        for(int i=0; i<4; ++i)
        {
            controls::Label *label=splitter->newChild<controls::Label>();

            label->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit, sed do eiusmod");
            label->setWrap(true);
            splitter->setPanel(i, label);
        }
    };

    Scene fresh;
    build(fresh);
    fresh.settle();

    Scene trip;
    build(trip);
    trip.settle();

    const std::vector<std::string> first=trip.tree();

    trip.canvas->setSize(1024, 768);
    trip.settle();
    trip.canvas->setSize(640, 480);
    trip.settle();

    checkSameTree(trip.tree(), first);
    checkSameTree(trip.tree(), fresh.tree());
    CHECK(trip.requests == 0);
    CHECK(fresh.requests == 0);
}

TEST_CASE("Properties' preferred sizes do not depend on the widths it had before")
{
    // One Properties docked Top, one inside a dock-None box with no size,
    // which starts narrower than the split (the nested one in a PropertyTree).
    auto build=[](Scene &scene)
    {
        controls::Properties *top=scene.canvas->newChild<controls::Properties>("top");
        top->setDock(DockPosition::Top);
        top->add("First Name");
        top->add("Middle Name");
        top->add("Last Name");

        controls::Base *box=scene.canvas->newChild<controls::Base>("box");
        box->setPos(0, 200);

        controls::Properties *nested=box->newChild<controls::Properties>("nested");
        nested->setDock(DockPosition::Top);
        nested->add("Name");
        nested->add("Age");
    };

    Scene fresh;
    build(fresh);
    fresh.settle();

    Scene trip;
    build(trip);
    trip.settle();

    const std::vector<std::string> first=trip.tree();

    trip.canvas->setSize(1024, 768);
    trip.settle();
    trip.canvas->setSize(640, 480);
    trip.settle();

    checkSameTree(trip.tree(), first);
    checkSameTree(trip.tree(), fresh.tree());
    CHECK(trip.requests == 0);
    CHECK(fresh.requests == 0);

    // A row asks for its label's margin, the split (80) and its property.
    for(auto &&child : fresh.canvas->getChildren())
    {
        for(auto &&grandChild : child->getChildren())
        {
            auto *properties=gweni_cast<controls::Properties>(grandChild);

            if(!properties)
                continue;

            for(auto &&row : properties->getChildren())
            {
                auto *propertyRow=gweni_cast<controls::PropertyRow>(row);

                if(!propertyRow)
                    continue;

                const int expected=propertyRow->getLabel()->getMargin().left+80+
                    propertyRow->getProperty()->getPreferredSize().width;

                CHECK(propertyRow->getPreferredSize().width == expected);
            }
        }
    }
}

TEST_CASE("a VerticalSlider asks for its request whatever its value, and fits a smaller layout cell in one pass")
{
    Scene scene;
    controls::Base *host=scene.canvas->newChild<controls::Base>("host");

    host->setBounds(0, 0, 100, 60);
    host->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    host->setLayout(new controls::VerticalLayout());

    controls::VerticalSlider *slider=host->newChild<controls::VerticalSlider>();

    slider->setSize(20, 200);
    // Cut to a cell that is too short, as a Shrink child is.
    slider->setSizeFlags({SizeFlag::Expand, SizeFlag::Shrink});
    slider->setRange(0, 1);
    slider->setFloatValue(0);

    const controls::LayoutStats first=scene.frame();

    CHECK(first.passes == 1);
    CHECK(slider->getPreferredSize().height == 200);
    // The cell is the host's 60: the slider gets it, not its 200.
    CHECK(slider->getHeight() == 60);

    const Rect atZero=slider->getBounds();

    slider->setFloatValue(1);

    const controls::LayoutStats second=scene.frame();

    CHECK(second.passes <= 1);
    CHECK(slider->getPreferredSize().height == 200);
    CHECK(text(slider->getBounds()) == text(atZero));
    CHECK(scene.frame().passes == 0);
    CHECK(scene.requests == 0);
}

TEST_CASE("a wrapped Text asks for its parent's requested width, or with none for its longest word")
{
    const String words="aa bbbbbbbbbb ccc dddd";

    SUBCASE("the app asked the parent for a width")
    {
        Scene scene;
        controls::Base *box=scene.canvas->newChild<controls::Base>("box");

        box->setSize(200, 100);
        box->setPadding(Padding(3, 3, 3, 3));

        auto *wrapped=box->newChild<controls::internal::Text>();

        wrapped->setWrap(true);
        wrapped->setText(words);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == 200-3-3);
        CHECK(scene.requests == 0);
    }

    SUBCASE("no width was asked: the longest word, the height from the width it is laid out at")
    {
        Scene scene;
        controls::Base *box=scene.canvas->newChild<controls::Base>("box");

        box->setDock(DockPosition::Top);

        auto *wrapped=box->newChild<controls::internal::Text>();

        wrapped->setWrap(true);
        wrapped->setText(words);
        scene.settle();

        const int longest=scene.renderer.measureText(wrapped->getFont(), "bbbbbbbbbb").x;
        const int lineHeight=scene.renderer.measureText(wrapped->getFont(), " ").y;

        CHECK(!box->hasRequestedSize(Dim::X));
        CHECK(wrapped->getPreferredSize().width == longest);
        // 640 wide: one line.
        CHECK(wrapped->numLines() == 1);
        CHECK(wrapped->getPreferredSize().height == lineHeight);

        // As wide as the longest word: several lines, the height theirs.
        scene.canvas->setSize(longest, 480);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == longest);
        CHECK(wrapped->numLines() > 1);
        CHECK(wrapped->getPreferredSize().height == static_cast<int>(wrapped->numLines())*lineHeight);

        scene.canvas->setSize(640, 480);
        scene.settle();

        CHECK(wrapped->getPreferredSize().width == longest);
        CHECK(wrapped->numLines() == 1);
        CHECK(scene.requests == 0);
    }
}

namespace
{

//! A control's request and whether a size was asked, as text.
std::string request(const controls::Base *control)
{
    return text(control->getRequestedBounds())+" w="+std::to_string(control->hasRequestedSize(Dim::X))+
        " h="+std::to_string(control->hasRequestedSize(Dim::Y));
}

//! The requests of \p control and of its whole subtree, depth-first.
void listRequests(controls::Base *control, std::vector<std::string> &out)
{
    out.push_back(std::string(control->getTypeName())+" "+request(control));

    for(auto &&child : control->getActualChildren())
        listRequests(child, out);
}

std::vector<std::string> requests(controls::Base *control)
{
    std::vector<std::string> out;

    listRequests(control, out);
    return out;
}

//! Presses \p control at \p from (canvas coordinates), moves the mouse to
//! \p to and lets go, through the control's own mouse handlers.
void drag(controls::Base *control, const Point &from, const Point &to)
{
    control->onMouseClickLeft(from.x, from.y, true);
    control->onMouseMoved(to.x, to.y, to.x-from.x, to.y-from.y);
    control->onMouseClickLeft(to.x, to.y, false);
}

}

TEST_CASE("a slider's value change, a click and a drag on its thumb record no request")
{
    Scene scene;
    controls::VerticalSlider *slider=scene.canvas->newChild<controls::VerticalSlider>();

    slider->setBounds(10, 10, 20, 200);
    slider->setRange(0, 1);
    slider->setFloatValue(0);
    scene.settle();

    const std::vector<std::string> before=requests(slider);
    controls::Base *thumb=slider->getChildren().front();
    const int atZero=thumb->getY();

    slider->setFloatValue(1);
    scene.settle();
    CHECK(thumb->getY() != atZero);
    checkSameTree(requests(slider), before);

    // a click on the track jumps the thumb there, which sets the value
    const Point track=slider->localPosToCanvas(Point(10, 150));

    slider->onMouseClickLeft(track.x, track.y, true);
    slider->onMouseClickLeft(track.x, track.y, false);
    scene.settle();
    CHECK(slider->getFloatValue() < 0.5f);
    checkSameTree(requests(slider), before);

    // a drag of the thumb itself, through its Dragger
    const float clicked=slider->getFloatValue();
    const Point grab=thumb->localPosToCanvas(Point(2, 2));

    drag(thumb, grab, Point(grab.x, grab.y-60));
    scene.settle();
    CHECK(slider->getFloatValue() > clicked);
    checkSameTree(requests(slider), before);
    CHECK(scene.requests == 0);
}

TEST_CASE("dragging a CrossSplitter bar records no request")
{
    Scene scene;
    controls::CrossSplitter *splitter=scene.canvas->newChild<controls::CrossSplitter>();

    splitter->setBounds(0, 0, 400, 300);
    scene.settle();

    const std::vector<std::string> before=requests(splitter);
    const float horizontal=splitter->getHorizontalFraction();
    const float vertical=splitter->getVerticalFraction();

    // the bars are its first three children: top/bottom, left/right, centre
    for(int i=0; i < 3; ++i)
    {
        controls::Base *bar=*std::next(splitter->getChildren().begin(), i);
        const Point grab=bar->localPosToCanvas(Point(2, 2));

        drag(bar, grab, Point(grab.x+40, grab.y+30));
        scene.settle();
        checkSameTree(requests(splitter), before);
    }

    CHECK(splitter->getHorizontalFraction() != horizontal);
    CHECK(splitter->getVerticalFraction() != vertical);
    CHECK(scene.requests == 0);
}

TEST_CASE("a window dragged by its title bar asks to be where it was dropped")
{
    Scene scene;
    controls::WindowControl *window=scene.canvas->newChild<controls::WindowControl>();

    window->setBounds(20, 30, 200, 150);
    scene.settle();

    controls::Base *titleBar=nullptr;

    for(auto &&child : window->getActualChildren())
    {
        if(std::string(child->getTypeName()) == "Dragger")
            titleBar=child;
    }
    REQUIRE(titleBar != nullptr);

    const Point grab=titleBar->localPosToCanvas(Point(50, 5));

    drag(titleBar, grab, Point(grab.x+40, grab.y+25));
    scene.settle();

    CHECK(text(window->getRequestedBounds()) == text(Rect(60, 55, 200, 150)));
    CHECK(text(window->getBounds()) == text(Rect(60, 55, 200, 150)));
    // a size was asked by the app, and stays asked
    CHECK(window->hasRequestedSize(Dim::X));
    CHECK(window->hasRequestedSize(Dim::Y));
    // made outside any pass: nothing is counted
    CHECK(scene.requests == 0);
}
