/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A layout pass measures and places only the children that changed, when
 *  the rest cannot have moved (#430, docs/layout.md → "Which children a pass
 *  enters"), on a real Canvas with the StubRenderer. Each case builds a panel
 *  of many children, settles it, changes it, and checks the result against
 *  the full walk twice: layout verification's full pass on the changed frame
 *  finds no difference, and every child's bounds and preferred size, and the
 *  panel's, are those of a panel built in the changed state from the start.
 *
 *  Moving one dock-None child of a panel of many goes over the same few
 *  children (LayoutStats::childrenScanned) at 200 and at 400 children, and a
 *  child that grows past the panel's largest extent, or gives it back, sizes
 *  the panel as a full count does. Adding and
 *  deleting dock-None children, and hiding and showing one, are counted the
 *  same way (a hidden child is not compared: neither walk measures or places
 *  it). A docked child that changes its size still moves the docked and
 *  Center children after it, a dock that changes to None too, and a resized
 *  panel places all of its children again, the aligned ones moving with its
 *  content box.
 *
 *  A resize, of the panel or of a control holding the children in an inner
 *  panel (as a ScrollControl does), and a canvas scale change dirty every
 *  child, so the list is stopped before they are (overflowChangedChildren,
 *  #485); a one-child edit on the next frame still goes over the same few
 *  children as on a panel that never had one.
 *
 *  Layout verification's full pass counts and places every child again, so
 *  with it on each frame starts from a full walk's state. The last case runs
 *  many frames of mixed edits with it off and compares every frame with a
 *  panel built fresh in that state, so state the shortcut carries from frame
 *  to frame (the kept terms, the changed-children list, m_arrangeAll) cannot
 *  drift unseen.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! A control measuring as `want` when it has no children.
class Leaf: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Leaf, controls::Base)
    {}

    Size want{10, 10};

    void calculateSize(skin::Base *skin, Dim dim) override
    {
        if(!getActualChildren().empty())
        {
            ParentClass::calculateSize(skin, dim);
            return;
        }

        if(dim == Dim::X)
            m_preferredSize.width=want.width;
        else
            m_preferredSize.height=want.height;
    }
};

//! A control holding its children in an inner panel filling it, as a
//! GroupBox does: they are added to it and held by the panel.
class InnerPanelHost: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(InnerPanelHost, controls::Base)
    {
        m_innerPanel=newChild<controls::Base>();
        m_innerPanel->setDock(DockPosition::Center);
    }

    controls::Base *innerPanel() { return m_innerPanel; }
};

//! How one child of the panel is set up.
struct Spec
{
    Point pos;
    Size want{40, 16};
    DockPosition dock=DockPosition::None;
    Alignment alignment=Alignment::Absolute;
    Margin margin;
    bool hidden=false;
};

//! dock-None children on a grid, some of them aligned.
std::vector<Spec> grid(int count)
{
    std::vector<Spec> specs(static_cast<size_t>(count));

    for(int i=0; i<count; ++i)
    {
        Spec &spec=specs[static_cast<size_t>(i)];

        spec.pos=Point((i%10)*50, (i/10)*20);
        if(i%7 == 3)
            spec.alignment=Alignment::Right;
        else if(i%11 == 5)
            spec.alignment=Alignment::CenterH|Alignment::Bottom;
    }
    return specs;
}

//! A canvas holding one panel of children built from specs.
struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    //! The control the children are added to and sized by: the panel, or
    //! the InnerPanelHost holding them in its inner panel.
    controls::Base *owner;
    //! The control holding the children.
    controls::Base *panel;
    std::vector<Leaf *> children;

    explicit Scene(const std::vector<Spec> &specs, Size panelSize=Size(0, 0),
        const Padding &padding=Padding(2, 3, 4, 5), bool innerPanel=false)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        if(innerPanel)
        {
            InnerPanelHost *host=canvas->newChild<InnerPanelHost>();

            owner=host;
            panel=host->innerPanel();
        }
        else
            owner=panel=canvas->newChild<controls::Base>();
        owner->setPos(5, 5);
        owner->setPadding(padding);
        if(panelSize.width > 0)
            owner->setSize(panelSize.width, panelSize.height);

        for(const Spec &spec : specs)
            add(spec);

        settle();
    }

    ~Scene()
    {
        delete canvas;
    }

    Leaf *add(const Spec &spec)
    {
        Leaf *child=owner->newChild<Leaf>();

        child->want=spec.want;
        child->setPos(spec.pos.x, spec.pos.y);
        child->setDock(spec.dock);
        child->setAlignment(spec.alignment);
        child->setMargin(spec.margin);
        if(spec.hidden)
            child->hide();
        children.push_back(child);
        return child;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        frame();
        frame();
    }

    const controls::LayoutStats &stats() const { return canvas->getLayoutStats(); }
};

//! Runs `change` on a settled scene in one frame, with layout verification
//! on, and returns the children that frame's passes went over. The full pass
//! must find nothing to change.
template<typename Change>
size_t changedFrame(Scene &scene, Change change)
{
    const size_t failures=controls::layoutVerifyFailures();

    controls::setLayoutVerify(true);
    change();
    scene.frame();
    controls::setLayoutVerify(false);

    CHECK(controls::layoutVerifyFailures() == failures);
    CHECK(scene.stats().passes == 1);
    return scene.stats().childrenScanned;
}

//! The panel and every child laid out as in `fresh`, a scene built in the
//! state `scene` was changed to.
void checkSameAs(const Scene &scene, const Scene &fresh)
{
    CHECK((scene.panel->getBounds() == fresh.panel->getBounds()));
    CHECK(scene.panel->getPreferredSize().width == fresh.panel->getPreferredSize().width);
    CHECK(scene.panel->getPreferredSize().height == fresh.panel->getPreferredSize().height);
    REQUIRE(scene.children.size() == fresh.children.size());

    for(size_t i=0; i < scene.children.size(); ++i)
    {
        CAPTURE(i);
        // a hidden child is neither measured nor placed: it keeps what it had
        REQUIRE(scene.children[i]->hidden() == fresh.children[i]->hidden());
        if(scene.children[i]->hidden())
            continue;

        CHECK((scene.children[i]->getBounds() == fresh.children[i]->getBounds()));
        CHECK(scene.children[i]->getPreferredSize().width == fresh.children[i]->getPreferredSize().width);
        CHECK(scene.children[i]->getPreferredSize().height == fresh.children[i]->getPreferredSize().height);
    }
}

//! Moves child 57 of a grid of `count`, and checks the result: returns the
//! children the frame went over.
size_t moveOne(int count)
{
    std::vector<Spec> specs=grid(count);
    Scene scene(specs);

    const size_t scanned=changedFrame(scene, [&]{ scene.children[57]->setPos(123, 45); });

    specs[57].pos=Point(123, 45);
    Scene fresh(specs);
    checkSameAs(scene, fresh);
    CHECK(scene.children[57]->getX() == 123);
    CHECK(scene.children[57]->getY() == 45);
    return scanned;
}

}//namespace

TEST_CASE("changed children: moving one free child of many goes over a few children")
{
    const size_t at200=moveOne(200);
    const size_t at400=moveOne(400);

    CHECK(at200 == at400);
    CHECK(at200 < 40);
}

TEST_CASE("changed children: the child holding the largest extent grows and shrinks")
{
    std::vector<Spec> specs=grid(200);
    // one of the lowest, not aligned
    const size_t last=specs.size()-2;
    REQUIRE((specs[last].alignment == Alignment::Absolute));

    Scene scene(specs);
    const int width=scene.panel->getWidth();

    // the panel grows with it, so all of its children are placed again
    changedFrame(scene, [&]{
        scene.children[last]->want=Size(300, 90);
        scene.children[last]->invalidate();
    });
    CHECK(scene.panel->getWidth() > width);

    specs[last].want=Size(300, 90);
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
    }

    // given back: the largest of the others is counted again, in full
    changedFrame(scene, [&]{
        scene.children[last]->want=Size(2, 2);
        scene.children[last]->invalidate();
    });
    specs[last].want=Size(2, 2);
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
        CHECK(scene.panel->getWidth() == width);
    }
}

TEST_CASE("changed children: adding, deleting, hiding and showing free children")
{
    std::vector<Spec> specs=grid(200);
    Scene scene(specs);

    // the first one goes, a new one comes inside the panel, every frame
    for(int step=0; step < 3; ++step)
    {
        CAPTURE(step);
        const size_t scanned=changedFrame(scene, [&]{
            delete scene.children.front();
            scene.children.erase(scene.children.begin());
            specs.erase(specs.begin());

            Spec spec;
            spec.pos=Point(20+step*30, 50+step*40);
            scene.add(spec);
            specs.push_back(spec);
        });
        CHECK(scanned < 40);
    }
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
    }

    // one in the middle hidden, then shown again
    size_t scanned=changedFrame(scene, [&]{ scene.children[100]->hide(); });
    CHECK(scanned < 40);
    specs[100].hidden=true;
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
    }

    scanned=changedFrame(scene, [&]{ scene.children[100]->show(); });
    CHECK(scanned < 40);
    specs[100].hidden=false;
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
    }

    // the one holding the largest extent hidden: the panel shrinks to the
    // others
    const int width=scene.panel->getWidth();
    const size_t last=specs.size()-1;

    specs[last].pos=Point(700, 30);
    changedFrame(scene, [&]{ scene.children[last]->setPos(700, 30); });
    CHECK(scene.panel->getWidth() > width);
    changedFrame(scene, [&]{ scene.children[last]->hide(); });
    specs[last].hidden=true;
    {
        Scene fresh(specs);
        checkSameAs(scene, fresh);
        CHECK(scene.panel->getWidth() == width);
    }
}

TEST_CASE("changed children: a docked child that changes moves the children after it")
{
    std::vector<Spec> specs=grid(200);

    Spec left;
    left.dock=DockPosition::Left;
    left.want=Size(30, 10);
    Spec top;
    top.dock=DockPosition::Top;
    top.want=Size(10, 25);
    Spec center;
    center.dock=DockPosition::Center;
    specs.insert(specs.begin(), {left, top, left, center});

    Scene scene(specs, Size(700, 500));
    const int centerX=scene.children[3]->getX();
    const int secondLeftX=scene.children[2]->getX();

    // the first docked child grows: the second, the Top and the Center move
    const size_t scanned=changedFrame(scene, [&]{
        scene.children[0]->want=Size(55, 10);
        scene.children[0]->invalidate();
    });
    CHECK(scanned >= specs.size());
    CHECK(scene.children[2]->getX() == secondLeftX+25);
    CHECK(scene.children[3]->getX() == centerX+25);

    specs[0].want=Size(55, 10);
    {
        Scene fresh(specs, Size(700, 500));
        checkSameAs(scene, fresh);
    }

    // a dock that becomes None gives its room back to the children after it
    changedFrame(scene, [&]{ scene.children[0]->setDock(DockPosition::None); });
    specs[0].dock=DockPosition::None;
    {
        Scene fresh(specs, Size(700, 500));
        checkSameAs(scene, fresh);
        CHECK(scene.children[2]->getX() == secondLeftX-30);
    }
}

TEST_CASE("changed children: a resized panel places all of its children again")
{
    std::vector<Spec> specs=grid(200);
    Scene scene(specs, Size(700, 500));

    std::vector<Rect> before;
    for(Leaf *child : scene.children)
        before.push_back(child->getBounds());

    const size_t scanned=changedFrame(scene, [&]{ scene.panel->setSize(640, 420); });
    CHECK(scanned >= specs.size());

    Scene fresh(specs, Size(640, 420));
    checkSameAs(scene, fresh);

    // every aligned child moved with the content box, the others stayed
    for(size_t i=0; i < specs.size(); ++i)
    {
        CAPTURE(i);
        if(specs[i].alignment == Alignment::Absolute)
            CHECK((scene.children[i]->getBounds() == before[i]));
        else
            CHECK_FALSE((scene.children[i]->getBounds() == before[i]));
    }
}

namespace
{

const Size wholeSize(700, 500);
const Size resized(640, 420);

//! Child 57 of a grid of 200 moved on a settled scene at `size`: the
//! children that frame goes over on a panel that saw no whole-subtree change.
size_t moveOneScanned(Size size, bool innerPanel)
{
    Scene scene(grid(200), size, Padding(2, 3, 4, 5), innerPanel);

    return changedFrame(scene, [&]{ scene.children[57]->setPos(123, 45); });
}

//! A grid of 200 resized from wholeSize, then child 57 moved: the move goes
//! over what it does on a scene built at the new size, and both frames lay
//! out as a fresh build does.
void resizeThenMoveOne(bool innerPanel)
{
    std::vector<Spec> specs=grid(200);
    Scene scene(specs, wholeSize, Padding(2, 3, 4, 5), innerPanel);

    CHECK(changedFrame(scene, [&]{ scene.owner->setSize(resized.width, resized.height); }) >= specs.size());
    {
        Scene fresh(specs, resized, Padding(2, 3, 4, 5), innerPanel);
        checkSameAs(scene, fresh);
    }

    const size_t scanned=changedFrame(scene, [&]{ scene.children[57]->setPos(123, 45); });

    CHECK(scanned == moveOneScanned(resized, innerPanel));
    CHECK(scanned < 40);

    specs[57].pos=Point(123, 45);
    Scene fresh(specs, resized, Padding(2, 3, 4, 5), innerPanel);
    checkSameAs(scene, fresh);
}

}//namespace

TEST_CASE("changed children: after a resize, one moved child is walked alone")
{
    resizeThenMoveOne(false);
}

TEST_CASE("changed children: after a resize of an inner panel's host, one moved child is walked alone")
{
    std::vector<Spec> specs=grid(200);
    Scene scene(specs, wholeSize, Padding(2, 3, 4, 5), true);

    // held by the inner panel, not by the control they were added to
    REQUIRE(scene.children[0]->getParent() == scene.owner);
    REQUIRE(scene.panel->getChildren().size() == specs.size());

    resizeThenMoveOne(true);
}

TEST_CASE("changed children: after a scale change, one moved child is walked alone")
{
    for(bool innerPanel : {false, true})
    {
        CAPTURE(innerPanel);
        std::vector<Spec> specs=grid(200);
        Scene scene(specs, wholeSize, Padding(2, 3, 4, 5), innerPanel);

        CHECK(changedFrame(scene, [&]{ scene.canvas->setScale(2.0f); }) >= specs.size());

        const size_t scanned=changedFrame(scene, [&]{ scene.children[57]->setPos(123, 45); });

        CHECK(scanned == moveOneScanned(wholeSize, innerPanel));
        CHECK(scanned < 40);

        // laid out in canvas units, as at scale 1
        specs[57].pos=Point(123, 45);
        Scene fresh(specs, wholeSize, Padding(2, 3, 4, 5), innerPanel);
        fresh.canvas->setScale(2.0f);
        fresh.settle();
        checkSameAs(scene, fresh);
    }
}

namespace
{

//! A fixed sequence of pseudo-random numbers, the same on every run.
struct Lcg
{
    uint32_t state;

    int next(int bound)
    {
        state=state*1664525u+1013904223u;
        return static_cast<int>((state >> 8) % static_cast<uint32_t>(bound));
    }
};

//! `frames` frames of random edits to a panel of `count` children, with
//! layout verification off, each frame compared with a fresh build.
void editFrames(uint32_t seed, int count, int frames, Size panelSize)
{
    Lcg random{seed};
    std::vector<Spec> specs=grid(count);
    Padding padding(2, 3, 4, 5);
    Scene scene(specs, panelSize, padding);

    for(int frame=0; frame < frames; ++frame)
    {
        CAPTURE(seed);
        CAPTURE(frame);
        const size_t i=static_cast<size_t>(random.next(static_cast<int>(specs.size())));
        Spec &spec=specs[i];
        Leaf *child=scene.children[i];
        const int edit=random.next(10);
        CAPTURE(edit);

        switch(edit)
        {
        case 0:
        case 1:
            spec.pos=Point(random.next(480), random.next(380));
            child->setPos(spec.pos.x, spec.pos.y);
            break;
        case 2:
            spec.want=Size(5+random.next(80), 5+random.next(40));
            child->want=spec.want;
            child->invalidate();
            break;
        case 3:
            spec.hidden=!spec.hidden;
            child->setHidden(spec.hidden);
            break;
        case 4:
            spec.margin=Margin(random.next(4), random.next(4), random.next(4), random.next(4));
            child->setMargin(spec.margin);
            break;
        case 5:
        {
            // the child goes, another comes at the end
            delete child;
            scene.children.erase(scene.children.begin()+static_cast<std::ptrdiff_t>(i));
            specs.erase(specs.begin()+static_cast<std::ptrdiff_t>(i));

            Spec added;
            added.pos=Point(random.next(480), random.next(380));
            added.want=Size(5+random.next(80), 5+random.next(40));
            scene.add(added);
            specs.push_back(added);
            break;
        }
        case 6:
        {
            const Spec moved=spec;

            child->bringToFront();
            scene.children.erase(scene.children.begin()+static_cast<std::ptrdiff_t>(i));
            scene.children.push_back(child);
            specs.erase(specs.begin()+static_cast<std::ptrdiff_t>(i));
            specs.push_back(moved);
            break;
        }
        case 7:
        {
            static const DockPosition docks[]={DockPosition::None, DockPosition::Left,
                DockPosition::Top, DockPosition::Right, DockPosition::Bottom};

            spec.dock=docks[random.next(5)];
            child->setDock(spec.dock);
            break;
        }
        case 8:
            spec.alignment=(random.next(2) == 0) ? Alignment::Absolute : Alignment::Right|Alignment::CenterV;
            child->setAlignment(spec.alignment);
            break;
        default:
            if(random.next(4) == 0)
            {
                padding=Padding(random.next(6), random.next(6), random.next(6), random.next(6));
                scene.panel->setPadding(padding);
            }
            break;
        }

        scene.frame();

        Scene fresh(specs, panelSize, padding);
        checkSameAs(scene, fresh);
    }
}

}//namespace

TEST_CASE("changed children: many frames of edits without verification match a fresh build")
{
    const bool verify=controls::layoutVerifyEnabled();

    controls::setLayoutVerify(false);
    for(uint32_t seed=1; seed <= 3; ++seed)
    {
        // a panel sized by its children, and one of a fixed size
        editFrames(seed, 120, 40, Size(0, 0));
        editFrames(seed+100, 120, 40, Size(700, 500));
    }
    controls::setLayoutVerify(verify);
}
