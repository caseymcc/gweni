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
 *  Collapse hides retained labels, supplies delayed canvas tooltips, and
 *  changes the rail's width in one pass without entering unrelated siblings.
 *  Expanded width is the widest main/footer item or the app's fixed width.
 *  New and reparented rows adopt the container's collapse state. Badges are
 *  lazy attached children of the icon, survive both states, and count changes
 *  write only their slots, entering only their layout path if the size changes.
 */
#include "headlessFixture.h"

#include <gweni/controls.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

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
        drawnImages.push_back(texture.name);
        livePrimitives.insert(id);
        StubRenderer::drawTexturedRect(texture, id, rect, z, u1, v1, u2, v2);
    }

    void hidePrimitive(size_t id) override
    {
        livePrimitives.insert(id);
        StubRenderer::hidePrimitive(id);
    }

    std::vector<String> drawnImages;
};

//! Use the real retained Badge renderer to check its bubble and glyph slots.
class RailSkin: public test::StubSkin
{
public:
    explicit RailSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Badge")
            return std::make_unique<skin::textured::Badge>();
        return StubSkin::getControlRenderer(typeName);
    }
};

class CountingPanel: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(CountingPanel, controls::Base) {}
    int entries=0;

    void calculateSize(skin::Base *skin, Dim dim) override
    {
        if(!controls::Canvas::verifyingLayout())
            ++entries;
        ParentClass::calculateSize(skin, dim);
    }

    void arrange(skin::Base *skin, Dim dim) override
    {
        if(!controls::Canvas::verifyingLayout())
            ++entries;
        ParentClass::arrange(skin, dim);
    }
};

struct Scene
{
    RailRenderer renderer;
    RailSkin skin{&renderer};
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
        renderer.drawnImages.clear();
        controls::resetChangeStats();
        canvas->updateCanvas();
        canvas->renderCanvas();
        CHECK(canvas->getLayoutStats().passes <= 1);
        CHECK(canvas->getLayoutStats().renderLayout == 0);
        CHECK(controls::layoutVerifyFailures() == 0);
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

    void checkSettled()
    {
        frame();
        CHECK(canvas->getLayoutStats().passes == 0);
        CHECK(renderer.frameStats().primitiveSlotsWritten == 0);
        CHECK(renderer.frameStats().textSlotsWritten == 0);
    }
};

struct Counter: event::Handler
{
    void changed(event::Info info)
    {
        ++calls;
        caller=info.controlCaller;
        selected=info.control;
        integer=info.integer;
    }

    int calls=0;
    controls::Base *caller=nullptr;
    controls::Base *selected=nullptr;
    int integer=0;
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

TEST_CASE("NavigationRail: collapse retains labels and supplies delayed tooltips in one pass and expand restores automatic and fixed widths")
{
    Scene scene;
    auto *home=scene.rail->addItem("Home", "home.png");
    auto *inbox=scene.rail->addItem("Unanswered inbox items", "inbox.png");
    auto *settings=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    settings->setText("Settings");
    auto *header=scene.canvas->newChild<controls::Button>();
    header->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    header->setSize(300, 20);
    scene.rail->setHeader(header);
    auto *sibling=scene.canvas->newChild<CountingPanel>();
    sibling->setBounds(350, 0, 80, 40);
    Counter changes;
    scene.rail->onCollapsedChanged.add(&changes, &Counter::changed);
    scene.rail->setSelected(inbox);
    scene.frame();

    const int natural=2*controls::NavigationItem::PaddingX+controls::NavigationItem::IconSize
        +controls::NavigationItem::IconGap+inbox->getLabel()->getPreferredSize().width;
    const int collapsed=2*controls::NavigationItem::PaddingX+controls::NavigationItem::IconSize;
    CHECK(scene.rail->getWidth() == natural);
    CHECK(scene.rail->getWidth() < header->getPreferredSize().width);
    auto *label=inbox->getLabel();
    const size_t labelId=label->getControlId();
    const Rect bounds=inbox->getBounds();
    sibling->entries=0;
    scene.rail->setCollapsed(true);
    CHECK(scene.rail->isCollapsed());
    CHECK(changes.calls == 1);
    CHECK(changes.caller == scene.rail);
    CHECK(changes.integer == 1);
    for(auto *item : {home, inbox, settings})
    {
        CHECK(item->getLabel()->hidden());
        CHECK(item->getToolTipText() == item->getText());
    }
    scene.frame();
    CHECK(scene.rail->getWidth() == collapsed);
    CHECK(inbox->getWidth() == collapsed);
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(sibling->entries == 0);
    CHECK(scene.rail->getSelected() == inbox);
    CHECK(inbox->getLabel() == label);
    CHECK(controls::Base::getControlById(labelId) == label);
    scene.checkSettled();

    scene.rail->setCollapsed(true);
    CHECK(changes.calls == 1);
    scene.checkSettled();
    inbox->setText("Inbox");
    CHECK(inbox->getToolTipText() == "Inbox");
    scene.frame();
    const Point at=inbox->localPosToCanvas(Point(collapsed/2, inbox->getHeight()/2));
    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    scene.frame();
    CHECK(scene.canvas->toolTipControl() == nullptr);
    scene.clock+=scene.canvas->getToolTipDelay()+0.01;
    scene.frame();
    REQUIRE(scene.canvas->toolTipControl() != nullptr);
    CHECK(scene.canvas->toolTipControl()->visible());
    CHECK(scene.canvas->toolTipControl()->getText() == "Inbox");
    // Restore the same label before checking the expansion round trip.
    inbox->setText("Unanswered inbox items");
    scene.frame();
    scene.rail->setExpandedWidth(220);
    scene.checkSettled(); // a future expanded width does no work while collapsed
    scene.rail->setCollapsed(false);
    scene.frame();
    CHECK_FALSE(scene.rail->isCollapsed());
    CHECK(scene.rail->getWidth() == 220);
    CHECK(changes.calls == 2);
    CHECK(changes.integer == 0);
    CHECK_FALSE(scene.canvas->toolTipControl()->visible());
    for(auto *item : {home, inbox, settings})
    {
        CHECK_FALSE(item->getLabel()->hidden());
        CHECK(item->getToolTipText().empty());
    }
    scene.rail->setExpandedWidth(0);
    scene.frame();
    CHECK(scene.rail->getWidth() == natural);
    CHECK(inbox->getBounds().x == bounds.x);
    CHECK(inbox->getBounds().y == bounds.y);
    CHECK(inbox->getBounds().w == bounds.w);
    CHECK(inbox->getBounds().h == bounds.h);
    scene.rail->setCollapsed(false);
    scene.rail->setExpandedWidth(0);
    scene.checkSettled();

    settings->setText("The footer has the widest item now");
    scene.frame();
    CHECK(scene.rail->getWidth() == settings->getPreferredSize().width);
    scene.rail->setExpandedWidth(170);
    scene.frame();
    CHECK(scene.rail->getWidth() == 170);
    scene.rail->setCollapsed(true);
    scene.frame();
    CHECK(scene.rail->getWidth() == collapsed);
    scene.rail->setCollapsed(false);
    scene.frame();
    CHECK(scene.rail->getWidth() == 170);
}

TEST_CASE("NavigationRail: new and reparented main and footer items adopt the rail's collapse state")
{
    Scene scene;
    scene.rail->setCollapsed(true);
    auto *main=scene.rail->addItem("Main");
    auto *footer=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    footer->setText("Footer");
    scene.frame();
    for(auto *item : {main, footer})
    {
        CHECK(item->getLabel()->hidden());
        CHECK(item->getToolTipText() == item->getText());
    }
    auto *expanded=scene.canvas->newChild<controls::NavigationRail>();
    footer->setParent(expanded->getFooter());
    scene.frame();
    CHECK_FALSE(footer->getLabel()->hidden());
    CHECK(footer->getToolTipText().empty());
    footer->setParent(scene.rail);
    scene.frame();
    CHECK(footer->getLabel()->hidden());
    CHECK(footer->getToolTipText() == "Footer");
    scene.rail->setCollapsed(false);
    scene.frame();
    CHECK_FALSE(main->getLabel()->hidden());
    CHECK_FALSE(footer->getLabel()->hidden());
    scene.checkSettled();
}

TEST_CASE("NavigationRail: lazy icon badges survive collapse and count changes write only the badge and lay out only its path")
{
    Scene scene;
    auto *item=scene.rail->addItem("Inbox", "inbox.png");
    auto *other=scene.rail->addItem("Other", "other.png");
    scene.frame();
    REQUIRE(item->getBadge() == nullptr);
    const int width=scene.rail->getWidth();
    item->setBadgeCount(9);
    auto *badge=item->getBadge();
    REQUIRE(badge != nullptr);
    scene.frame();
    CHECK(badge->getParent() == item->getIcon());
    CHECK(badge->isAttached());
    CHECK((badge->getAttachedCorner() == Corner::TopRight));
    CHECK(badge->visible());
    CHECK(scene.rail->getWidth() == width);
    std::vector<size_t> primitives, texts;
    badge->getSkinControl()->appendIds(primitives, texts);
    REQUIRE(primitives.size() == 1);
    REQUIRE(texts.size() == 1);

    SUBCASE("expanded") {}
    SUBCASE("collapsed")
    {
        scene.rail->setCollapsed(true);
        scene.frame();
    }
    CHECK(badge->visible());
    scene.checkSettled();
    item->setBadgeCount(10);
    CHECK(item->getBadge() == badge);
    CHECK(other->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(badge->getCount() == 10);
    CHECK(badge->getText() == "10");
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    // canvas, rail, main container, row, icon, badge; each axis measure/arrange.
    CHECK(scene.canvas->getLayoutStats().visited == 6*4);
    CHECK(scene.renderer.writtenPrimitiveIds() == primitives);
    REQUIRE(scene.renderer.writtenTextIds().size() == 1);
    CHECK(scene.renderer.writtenTextIds()[0].textId == texts[0]);
    CHECK(controls::getChangeStats().controls == 1);
    scene.checkSettled();

    item->setBadgeCount(11);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.renderer.writtenPrimitiveIds() == primitives);
    REQUIRE(scene.renderer.writtenTextIds().size() == 1);
    CHECK(scene.renderer.writtenTextIds()[0].textId == texts[0]);
    scene.checkSettled();
    item->setBadgeCount(11);
    scene.checkSettled();

    item->setBadgeDot(true);
    scene.frame();
    CHECK(badge->isDot());
    CHECK(badge->getText().empty());
    CHECK(badge->getWidth() == controls::Badge::DotSize);
    item->setBadgeDot(false);
    scene.frame();
    CHECK(badge->getText() == "11");
    item->setBadgeDot(true);
    item->setBadgeCount(3);
    scene.frame();
    CHECK_FALSE(badge->isDot());
    CHECK(badge->getText() == "3");
    item->setBadgeCount(0);
    scene.frame();
    CHECK(badge->hidden());
    item->setBadgeDot(true);
    scene.frame();
    CHECK(badge->visible());
}

TEST_CASE("NavigationRail: badges on blank icons and footer items remain visible and return every id with the rail")
{
    Scene scene;
    delete scene.rail;
    scene.rail=nullptr;
    scene.frame();
    const auto baseline=test::IdBankCounts::now();
    const auto primitives=scene.renderer.livePrimitives;
    const auto texts=scene.renderer.liveTexts;
    scene.rail=scene.canvas->newChild<controls::NavigationRail>();
    auto *blank=scene.rail->addItem("No image");
    auto *footer=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    footer->setText("Account");
    blank->setBadgeDot(true);
    footer->setBadgeCount(42);
    scene.frame();
    CHECK(blank->getIcon()->getImageName().empty());
    CHECK(scene.renderer.drawnImages.empty());
    CHECK(blank->getIcon()->visible());
    CHECK(blank->getBadge()->visible());
    CHECK(footer->getBadge()->visible());
    const size_t badgeId=blank->getBadge()->getControlId();
    scene.rail->setCollapsed(true);
    scene.frame();
    CHECK(blank->getBadge()->visible());
    CHECK(footer->getBadge()->visible());
    blank->setImage("image.png");
    scene.frame();
    blank->setImage("");
    scene.frame();
    CHECK(blank->getIcon()->getImageName().empty());
    CHECK(scene.renderer.drawnImages.empty());
    CHECK(blank->getBadge()->visible());
    scene.rail->setCollapsed(false);
    scene.frame();
    CHECK(blank->getBadge()->visible());
    scene.checkSettled();
    delete scene.rail;
    scene.rail=nullptr;
    scene.frame();
    CHECK(controls::Base::getControlById(badgeId) == nullptr);
    CHECK(scene.renderer.livePrimitives == primitives);
    CHECK(scene.renderer.liveTexts == texts);
    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
    CHECK(scene.renderer.doubleReleased == 0);
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
