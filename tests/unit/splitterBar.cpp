/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's SplitterBar renderer, on a recording StubRenderer: a
 *  bar takes one slot and draws its render bounds at its own z in the
 *  SplitterBar surface for its state (disabled before pressed before hovered
 *  before rest). A bar that does not draw its background has its slot hidden
 *  in every state, hovered and pressed included, and turning background
 *  drawing off and on at runtime hides and shows it again. A press and a
 *  release each mark the bar (Dragger repaints on both). A CrossSplitter's
 *  three bars start off; setSplittersVisible() turns all three on and off,
 *  splittersVisible() reads it back, and the setter asks the canvas for a
 *  frame. A bar draws by default. No frame writes a slot the bars do not own,
 *  and deleting the controls gives the slots back.
 */
#include "headlessFixture.h"

#include <gweni/controls/crossSplitter.h>
#include <gweni/controls/splitterBar.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <map>
#include <set>
#include <vector>

using namespace gweni;
namespace tokyonight=gweni::skin::textured::tokyonight;

namespace
{

//! Records every draw and hide, and keeps what each slot ends up showing.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Slot
    {
        bool shown=false;
        Rect rect;
        Color top;
        int zIndex=0;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color,
        int zIndex) override
    {
        Slot &slot=slots[primitiveId];

        slot.shown=true;
        slot.rect=rect;
        slot.top=top;
        slot.zIndex=zIndex;
        written.push_back(primitiveId);
    }

    void showPrimitive(int style, size_t primitiveId) override
    {
        slots[primitiveId].shown=(style != 0);
        written.push_back(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        slots[primitiveId].shown=false;
        written.push_back(primitiveId);
    }

    std::map<size_t, Slot> slots;
    std::vector<size_t> written;   //!< this frame's draws, shows and hides
};

//! The textured skin's SplitterBar renderer, a StubControl for the rest.
class SplitterSkin: public test::StubSkin
{
public:
    explicit SplitterSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "SplitterBar")
            return std::make_unique<skin::textured::SplitterBar>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    DrawRecorder renderer;
    SplitterSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    //! Process-global like the input handler's last click times, so a later
    //! case's first press is never a double click of an earlier case's last.
    static float &clock()
    {
        static float time=10.0f;
        return time;
    }

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([]() { return clock(); });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.written.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    Point centre(controls::Base *control) const
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void hover(controls::Base *control)
    {
        const Point at=centre(control);

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void moveOff()
    {
        canvas->inputMouseMoved(399, 299, 0, 0);
        frame();
    }

    void mouseButton(bool down)
    {
        clock()+=1.0f;
        canvas->inputMouseButton(0, down);
    }

    static size_t id(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives().at(0);
    }

    const DrawRecorder::Slot &slot(controls::Base *control)
    {
        return renderer.slots[id(control)];
    }

    //! Every slot written this frame is one of \p owned.
    bool wroteOnly(const std::set<size_t> &owned) const
    {
        for(size_t written : renderer.written)
        {
            if(owned.count(written) == 0)
                return false;
        }
        return true;
    }
};

controls::SplitterBar *newBar(Scene &scene)
{
    controls::SplitterBar *bar=scene.canvas->newChild<controls::SplitterBar>();

    bar->setBounds(40, 60, 200, 5);
    return bar;
}

}//namespace

TEST_CASE("SplitterBar: one slot over its bounds, the surface of its state")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        controls::SplitterBar *bar=newBar(scene);

        scene.frame();
        REQUIRE(bar->getSkinControl() != nullptr);
        REQUIRE(bar->getSkinControl()->getPrimitives().size() == 1);
        CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives()+1);

        const std::set<size_t> owned={Scene::id(bar)};

        CHECK(scene.wroteOnly(owned));
        CHECK(scene.slot(bar).shown);
        CHECK((scene.slot(bar).rect == bar->getRenderBounds()));
        CHECK(scene.slot(bar).zIndex == bar->getZIndex());
        CHECK((scene.slot(bar).top == tokyonight::SplitterBar.top));

        scene.hover(bar);
        CHECK(bar->isHovered());
        CHECK((scene.slot(bar).top == tokyonight::SplitterBarHover.top));
        CHECK(scene.wroteOnly(owned));

        // the press marks the bar in the update phase, before any frame
        scene.mouseButton(true);
        CHECK(bar->isDepressed());
        CHECK((bar->getStateChange() & controls::StateChange_Background) != 0);
        scene.frame();
        CHECK(scene.slot(bar).shown);
        CHECK((scene.slot(bar).top == tokyonight::SplitterBarDown.top));
        CHECK(scene.wroteOnly(owned));

        // disabled wins over pressed and hovered
        bar->setDisabled(true);
        scene.frame();
        CHECK((scene.slot(bar).top == tokyonight::SplitterBarDisabled.top));
        bar->setDisabled(false);
        scene.frame();
        CHECK((scene.slot(bar).top == tokyonight::SplitterBarDown.top));

        // the release marks it again and gives the hover look back
        scene.mouseButton(false);
        CHECK_FALSE(bar->isDepressed());
        CHECK((bar->getStateChange() & controls::StateChange_Background) != 0);
        scene.frame();
        CHECK((scene.slot(bar).top == tokyonight::SplitterBarHover.top));

        scene.moveOff();
        CHECK((scene.slot(bar).top == tokyonight::SplitterBar.top));
        CHECK((scene.slot(bar).rect == bar->getRenderBounds()));

        // a settled frame writes nothing
        scene.frame();
        CHECK(scene.renderer.written.empty());

        // hidden with the control
        bar->setHidden(true);
        scene.frame();
        CHECK_FALSE(scene.slot(bar).shown);
        bar->setHidden(false);
        scene.frame();
        CHECK(scene.slot(bar).shown);
    }
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}

TEST_CASE("SplitterBar: a bar that does not draw its background stays hidden")
{
    Scene scene;
    controls::SplitterBar *bar=newBar(scene);

    bar->setShouldDrawBackground(false);
    scene.frame();
    REQUIRE(bar->getSkinControl() != nullptr);
    CHECK(bar->getSkinControl()->getPrimitives().size() == 1);
    CHECK_FALSE(scene.slot(bar).shown);

    scene.hover(bar);
    CHECK(bar->isHovered());
    CHECK_FALSE(scene.slot(bar).shown);

    scene.mouseButton(true);
    scene.frame();
    CHECK(bar->isDepressed());
    CHECK_FALSE(scene.slot(bar).shown);

    // turned on while pressed: shown, in the pressed look
    bar->setShouldDrawBackground(true);
    scene.frame();
    CHECK(scene.slot(bar).shown);
    CHECK((scene.slot(bar).top == tokyonight::SplitterBarDown.top));

    // and off again
    bar->setShouldDrawBackground(false);
    scene.frame();
    CHECK_FALSE(scene.slot(bar).shown);

    scene.mouseButton(false);
    scene.frame();
    CHECK_FALSE(scene.slot(bar).shown);

    scene.moveOff();
    CHECK_FALSE(scene.slot(bar).shown);

    bar->setShouldDrawBackground(true);
    scene.frame();
    CHECK(scene.slot(bar).shown);
    CHECK((scene.slot(bar).top == tokyonight::SplitterBar.top));
    CHECK(scene.wroteOnly({Scene::id(bar)}));
}

TEST_CASE("SplitterBar: CrossSplitter's bars are off until setSplittersVisible(true)")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        controls::CrossSplitter *splitter=scene.canvas->newChild<controls::CrossSplitter>();

        splitter->setBounds(0, 0, 300, 200);

        std::vector<controls::SplitterBar *> bars;

        for(controls::Base *child : splitter->getActualChildren())
        {
            if(controls::SplitterBar *bar=dynamic_cast<controls::SplitterBar *>(child))
                bars.push_back(bar);
        }
        REQUIRE(bars.size() == 3);

        scene.frame();
        CHECK_FALSE(splitter->splittersVisible());

        std::set<size_t> owned;

        for(controls::SplitterBar *bar : bars)
        {
            REQUIRE(bar->getSkinControl() != nullptr);
            REQUIRE(bar->getSkinControl()->getPrimitives().size() == 1);
            owned.insert(Scene::id(bar));
            CHECK_FALSE(bar->shouldDrawBackground());
            CHECK_FALSE(scene.slot(bar).shown);
        }
        CHECK(owned.size() == 3);

        // the setter asks for a frame, as a WindowCanvas needs
        CHECK_FALSE(scene.canvas->needsRedraw());
        splitter->setSplittersVisible(true);
        CHECK(splitter->splittersVisible());
        CHECK(scene.canvas->needsRedraw());
        scene.frame();

        for(controls::SplitterBar *bar : bars)
        {
            CHECK(bar->shouldDrawBackground());
            CHECK(scene.slot(bar).shown);
            CHECK((scene.slot(bar).rect == bar->getRenderBounds()));
            CHECK((scene.slot(bar).top == tokyonight::SplitterBar.top));
        }
        CHECK(scene.wroteOnly(owned));

        splitter->setSplittersVisible(false);
        CHECK_FALSE(splitter->splittersVisible());
        CHECK(scene.canvas->needsRedraw());
        scene.frame();

        for(controls::SplitterBar *bar : bars)
            CHECK_FALSE(scene.slot(bar).shown);
        CHECK(scene.wroteOnly(owned));
    }
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
}
