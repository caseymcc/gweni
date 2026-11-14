/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  NavigationRail's core composition on a real Canvas with a StubRenderer:
 *  main and footer NavigationItems share one SelectableGroup, a click or
 *  setSelected changes the selection once (a repeated selection does nothing),
 *  unrelated items cannot be selected through the rail, and reparenting a row
 *  moves its membership. Items have retained ImagePanel and Label children;
 *  the header precedes the main items and the footer sits at the bottom, all
 *  laid out in one pass. Selection lays nothing out and marks only its rows
 *  and text, settled frames write nothing, and deleting the rail returns its
 *  children's ids without firing selection events from destruction.
 */
#include "headlessFixture.h"

#include <gweni/controls.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

//! ImagePanel acquires its slot from the bank directly, bypassing the
//! renderer's generatePrimitive(). Account for it on first use too.
class RailRenderer: public test::StubRenderer
{
public:
    void drawTexturedRect(const Texture &texture, size_t id, Rect rect, int z,
        float u1, float v1, float u2, float v2) override
    {
        livePrimitives.insert(id);
        StubRenderer::drawTexturedRect(texture, id, rect, z, u1, v1, u2, v2);
    }

    void hidePrimitive(size_t id) override
    {
        livePrimitives.insert(id);
        StubRenderer::hidePrimitive(id);
    }
};

struct Scene
{
    RailRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::NavigationRail *rail=nullptr;
    double clock=10;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(480, 320);
        rail=canvas->newChild<controls::NavigationRail>("rail");
    }

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
        CHECK(canvas->getLayoutStats().passes <= 1);
        CHECK(canvas->getLayoutStats().renderLayout == 0);
    }

    void click(controls::Base *item)
    {
        clock+=1;
        const Point at=item->localPosToCanvas(Point(item->getWidth()/2, item->getHeight()/2));
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }
};

struct Counter: event::Handler
{
    void changed(event::Info info)
    {
        ++calls;
        caller=info.controlCaller;
        selected=info.control;
    }

    int calls=0;
    controls::Base *caller=nullptr;
    controls::Base *selected=nullptr;
};

}//namespace

TEST_CASE("NavigationRail: clicks and setSelected notify once and selection lays nothing out")
{
    Scene scene;
    auto *home=scene.rail->addItem("Home");
    auto *inbox=scene.rail->addItem("Inbox");
    auto *other=scene.rail->addItem("Other");
    Counter changes;
    scene.rail->onSelectionChange.add(&changes, &Counter::changed);
    scene.frame();

    CHECK(scene.rail->getSelected() == nullptr);
    CHECK_FALSE(home->isToggleable());
    REQUIRE(home->getGroup() != nullptr);
    CHECK(inbox->getGroup() == home->getGroup());

    scene.click(home);
    CHECK(scene.rail->getSelected() == home);
    CHECK(changes.calls == 1);
    CHECK(changes.caller == scene.rail);
    CHECK(changes.selected == home);
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    scene.click(home);
    CHECK(home->isSelected());
    CHECK(changes.calls == 1);
    scene.click(inbox);
    CHECK(scene.rail->getSelected() == inbox);
    CHECK_FALSE(home->isSelected());
    CHECK(changes.calls == 2);
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    scene.rail->setSelected(other);
    CHECK(scene.rail->getSelected() == other);
    CHECK(changes.calls == 3);
    CHECK_FALSE(inbox->isSelected());
    CHECK((inbox->getStateChange()&controls::StateChange_Background) != 0);
    CHECK((other->getStateChange()&controls::StateChange_Background) != 0);
    CHECK(home->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    scene.rail->setSelected(other);
    CHECK(changes.calls == 3);
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);

    auto *outsider=scene.canvas->newChild<controls::NavigationItem>();
    scene.rail->setSelected(outsider);
    CHECK(scene.rail->getSelected() == other);
    CHECK(changes.calls == 3);
    scene.rail->setSelected(nullptr);
    CHECK(scene.rail->getSelected() == nullptr);
    CHECK(changes.calls == 4);
    CHECK(changes.selected == nullptr);
    scene.rail->setSelected(nullptr);
    CHECK(changes.calls == 4);
}

TEST_CASE("NavigationRail: footer items share selection and reparenting moves their membership")
{
    Scene scene;
    auto *home=scene.rail->addItem("Home");
    auto *settings=scene.rail->getFooter()->newChild<controls::NavigationItem>("settings");
    settings->setText("Settings");
    Counter changes;
    scene.rail->onSelectionChange.add(&changes, &Counter::changed);
    scene.frame();

    REQUIRE(settings->getGroup() == home->getGroup());
    CHECK_FALSE(settings->isToggleable());
    scene.rail->setSelected(home);
    scene.click(settings);
    CHECK(scene.rail->getSelected() == settings);
    CHECK_FALSE(home->isSelected());
    CHECK(changes.calls == 2);
    CHECK(changes.selected == settings);

    auto *otherRail=scene.canvas->newChild<controls::NavigationRail>();
    auto *other=otherRail->addItem("Other");
    settings->setParent(otherRail->getFooter());
    CHECK(scene.rail->getSelected() == nullptr);
    CHECK(changes.calls == 3);
    CHECK(settings->getGroup() == other->getGroup());
    CHECK(otherRail->getSelected() == settings);

    settings->setParent(scene.canvas);
    CHECK(settings->getGroup() == nullptr);
    CHECK(otherRail->getSelected() == nullptr);
    CHECK_FALSE(settings->isToggleable());
    settings->setParent(scene.rail);
    CHECK(settings->getGroup() == home->getGroup());
    CHECK(scene.rail->getSelected() == settings);
    CHECK(changes.calls == 4);

    // A deleted selected row is silently forgotten, as in SelectableGroup.
    delete settings;
    CHECK(scene.rail->getSelected() == nullptr);
    CHECK(changes.calls == 4);
    scene.rail->setSelected(home);
    CHECK(changes.calls == 5);
}

TEST_CASE("NavigationRail: owned icon and label, header above items, footer below, all ids returned")
{
    Scene scene;
    scene.frame();
    const auto initial=test::IdBankCounts::now();
    const size_t primitives=scene.renderer.livePrimitives.size();
    const size_t texts=scene.renderer.liveTexts.size();
    auto *item=scene.rail->addItem("Inbox", "icon.png", "inbox");
    auto *footer=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    footer->setText("Settings");
    auto *header=scene.canvas->newChild<controls::Button>("menu");
    header->setText("Menu");
    scene.rail->setHeader(header);
    Counter changes;
    scene.rail->onSelectionChange.add(&changes, &Counter::changed);
    scene.rail->setSelected(item);
    scene.frame();

    CHECK(item->getName() == "inbox");
    CHECK(item->getText() == "Inbox");
    CHECK(item->getLabel()->getText() == "Inbox");
    CHECK(item->getLabel()->getParent() == item);
    CHECK(item->getIcon()->getParent() == item);
    CHECK(item->getIcon()->getImageName() == "icon.png");
    CHECK_FALSE(item->getIcon()->hidden());
    CHECK(item->getIcon()->getWidth() == controls::NavigationItem::IconSize);
    CHECK(item->getIcon()->getHeight() == controls::NavigationItem::IconSize);
    CHECK(item->getLabel()->getX() > item->getIcon()->right());
    CHECK(scene.rail->getHeader() == header);
    CHECK((header->getDock() == DockPosition::Top));
    CHECK((scene.rail->getFooter()->getDock() == DockPosition::Bottom));
    CHECK(header->localPosToCanvas().y+header->getHeight() <= item->localPosToCanvas().y);
    CHECK(footer->localPosToCanvas().y+footer->getHeight()
        == scene.rail->localPosToCanvas().y+scene.rail->getHeight());

    auto *icon=item->getIcon();
    item->setImage("");
    CHECK(item->getIcon() == icon);
    CHECK(icon->hidden());
    item->setImage("other.png");
    CHECK(item->getIcon() == icon);
    CHECK(icon->getImageName() == "other.png");
    CHECK_FALSE(icon->hidden());

    // Replacing the header transfers ownership and deletes the previous one.
    const size_t oldHeaderId=header->getControlId();
    auto *replacement=scene.canvas->newChild<controls::Button>();
    scene.rail->setHeader(replacement);
    CHECK(scene.rail->getHeader() == replacement);
    CHECK(controls::Base::getControlById(oldHeaderId) == nullptr);
    scene.rail->setHeader(nullptr);
    CHECK(scene.rail->getHeader() == nullptr);
    scene.frame();
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);

    // Include the empty rail's ids in the deletion baseline.
    std::vector<size_t> railPrimitives, railTexts;
    scene.rail->getSkinControl()->appendIds(railPrimitives, railTexts);
    for(controls::Base *part : scene.rail->getActualChildren())
    {
        if(const auto *skinControl=part->getSkinControl())
            skinControl->appendIds(railPrimitives, railTexts);
    }
    const size_t itemId=item->getControlId();
    const size_t footerId=footer->getControlId();
    delete scene.rail;
    scene.rail=nullptr;
    scene.frame();
    CHECK(changes.calls == 1);
    CHECK(controls::Base::getControlById(itemId) == nullptr);
    CHECK(controls::Base::getControlById(footerId) == nullptr);
    CHECK(scene.renderer.livePrimitives.size() == primitives-railPrimitives.size());
    CHECK(scene.renderer.liveTexts.size() == texts-railTexts.size());
    CHECK(test::IdBankCounts::now().livePrimitives() == initial.livePrimitives()-railPrimitives.size());
    CHECK(test::IdBankCounts::now().liveTexts() == initial.liveTexts()-railTexts.size());
    CHECK(scene.renderer.doubleReleased == 0);
}
