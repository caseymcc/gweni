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
 *  The rail holds keyboard focus as one Tab stop: arrows/Home/End move its
 *  logical focus (skipping disabled/hidden rows and including footer rows),
 *  Return/Space select, and focus entry starts at the selected enabled row.
 *  The textured renderer paints theme surfaces with a leading selection
 *  border in the row's one slot, hides idle slots, and changes only the two
 *  rows/runs involved in selection. Theme edits reach retained rail slots.
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
    struct Draw
    {
        size_t id;
        Rect rect;
        float borderWidth;
        Color top, border;
        int z;
    };

    void drawRoundedRect(size_t id, Rect rect, float radius, float borderWidth,
        Color top, Color bottom, Color border, int z) override
    {
        draws.push_back({id, rect, borderWidth, top, border, z});
        StubRenderer::drawRoundedRect(id, rect, radius, borderWidth, top, bottom, border, z);
    }

    void drawTexturedRect(const Texture &texture, size_t id, Rect rect, int z,
        float u1, float v1, float u2, float v2) override
    {
        drawnImages.push_back(texture.name);
        livePrimitives.insert(id);
        StubRenderer::drawTexturedRect(texture, id, rect, z, u1, v1, u2, v2);
    }

    void hidePrimitive(size_t id) override
    {
        hidden.push_back(id);
        livePrimitives.insert(id);
        StubRenderer::hidePrimitive(id);
    }

    std::vector<String> drawnImages;
    std::vector<Draw> draws;
    std::vector<size_t> hidden;
};

//! Real retained rail/row, Badge and text renderers for slot/glyph checks.
class RailSkin: public test::StubSkin
{
public:
    explicit RailSkin(test::StubRenderer *renderer): StubSkin(renderer)
    {
        Colors.NavigationItem.Normal=skin::textured::tokyonight::Fg;
        Colors.NavigationItem.Selected=skin::textured::tokyonight::Cyan;
        Colors.NavigationItem.Disabled=skin::textured::tokyonight::Muted;
    }

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Badge")
            return std::make_unique<skin::textured::Badge>();
        if(typeName == "NavigationRail")
            return std::make_unique<skin::textured::NavigationRail>();
        if(typeName == "NavigationItem")
            return std::make_unique<skin::textured::NavigationItem>();
        if(typeName == "Text")
            return std::make_unique<skin::textured::Text>();
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
        for(int key : {keys::Tab, keys::Shift, keys::Space, keys::Return,
            keys::Up, keys::Down, keys::Home, keys::End})
            input::onKeyEvent(canvas, key, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        renderer.drawnImages.clear();
        renderer.draws.clear();
        renderer.hidden.clear();
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

    void press(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void hover(controls::Base *item)
    {
        const Point at=item?item->localPosToCanvas(Point(item->getWidth()/2, item->getHeight()/2)):
            Point(450, 300);
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    std::vector<size_t> primitives(controls::Base *control)
    {
        std::vector<size_t> ids, texts;
        control->getSkinControl()->appendIds(ids, texts);
        return ids;
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

const RailRenderer::Draw *drawFor(const Scene &scene, size_t id)
{
    for(const auto &draw : scene.renderer.draws)
        if(draw.id == id)
            return &draw;
    return nullptr;
}

bool has(const std::vector<size_t> &ids, size_t id)
{
    return std::find(ids.begin(), ids.end(), id) != ids.end();
}

}//namespace

TEST_CASE("NavigationRail: one Tab stop enters at the selection and Material keys focus before selecting across main and footer items")
{
    Scene scene;
    auto *home=scene.rail->addItem("Home");
    auto *inbox=scene.rail->addItem("Inbox");
    auto *disabled=scene.rail->addItem("Disabled");
    auto *hidden=scene.rail->addItem("Hidden");
    auto *settings=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    settings->setText("Settings");
    disabled->setDisabled(true);
    hidden->hide();
    auto *outside=scene.canvas->newChild<controls::Button>();
    auto *last=scene.canvas->newChild<controls::Button>();
    outside->setTabable(true);
    last->setTabable(true);
    outside->setKeyboardInputEnabled(true);
    last->setKeyboardInputEnabled(true);
    Counter changes;
    scene.rail->onSelectionChange.add(&changes, &Counter::changed);
    scene.rail->setSelected(inbox);
    scene.frame();

    CHECK_FALSE(home->isTabable());
    CHECK_FALSE(settings->isTabable());
    last->focus();
    scene.press(keys::Tab);
    CHECK(KeyboardFocus == scene.rail);
    CHECK(scene.rail->getFocused() == inbox);
    CHECK(inbox->isNavigationFocused());
    CHECK(changes.calls == 1);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.press(keys::Down);
    CHECK(scene.rail->getFocused() == settings);
    CHECK(scene.rail->getSelected() == inbox);
    CHECK(changes.calls == 1);
    scene.press(keys::Return);
    CHECK(scene.rail->getSelected() == settings);
    CHECK(changes.calls == 2);
    scene.press(keys::Return);
    CHECK(changes.calls == 2);
    scene.press(keys::Down); // wraps to first enabled item
    CHECK(scene.rail->getFocused() == home);
    CHECK(scene.rail->getSelected() == settings);
    scene.press(keys::Space);
    CHECK(scene.rail->getSelected() == home);
    CHECK(changes.calls == 3);
    scene.press(keys::End);
    CHECK(scene.rail->getFocused() == settings);
    scene.press(keys::Up);
    CHECK(scene.rail->getFocused() == inbox);
    scene.press(keys::Home);
    CHECK(scene.rail->getFocused() == home);
    CHECK(changes.calls == 3);
    scene.press(keys::Tab);
    CHECK(KeyboardFocus == outside);
    CHECK_FALSE(home->isNavigationFocused());
    input::onKeyEvent(scene.canvas, keys::Shift, true);
    scene.press(keys::Tab);
    input::onKeyEvent(scene.canvas, keys::Shift, false);
    CHECK(KeyboardFocus == scene.rail);
    CHECK(scene.rail->getFocused() == home);
    scene.checkSettled();

    scene.rail->setSelected(disabled);
    outside->focus();
    scene.frame();
    scene.rail->focus();
    scene.frame();
    CHECK(scene.rail->getFocused() == home);
    scene.click(settings);
    CHECK(KeyboardFocus == scene.rail);
    CHECK(scene.rail->getFocused() == settings);
    CHECK(scene.rail->getSelected() == settings);
    const int before=changes.calls;
    scene.click(disabled);
    CHECK(scene.rail->getSelected() == settings);
    CHECK(changes.calls == before);

    // A key from an arbitrary footer control must keep its own meaning.
    auto *field=scene.rail->getFooter()->newChild<controls::TextBox>();
    field->setText("Account");
    field->focus();
    scene.press(keys::Return);
    CHECK(scene.rail->getSelected() == settings);
    CHECK(changes.calls == before);
    scene.rail->focus();
    scene.frame();
    delete settings;
    CHECK(scene.rail->getFocused() == nullptr);
    scene.press(keys::Home);
    CHECK(scene.rail->getFocused() == home);
}

TEST_CASE("NavigationRail: disabled and hidden items are skipped by every navigation key and an empty rail does nothing")
{
    Scene scene;
    auto *disabled=scene.rail->addItem("Disabled");
    auto *home=scene.rail->addItem("Home");
    auto *hidden=scene.rail->addItem("Hidden");
    auto *inbox=scene.rail->addItem("Inbox");
    auto *footer=scene.rail->getFooter()->newChild<controls::NavigationItem>();
    footer->setText("Disabled footer");
    disabled->setDisabled(true);
    footer->setDisabled(true);
    hidden->hide();
    scene.frame();
    scene.rail->focus();
    scene.frame();
    CHECK(scene.rail->getFocused() == home);
    scene.press(keys::End);
    CHECK(scene.rail->getFocused() == inbox);
    scene.press(keys::Down);
    CHECK(scene.rail->getFocused() == home);
    scene.press(keys::Up);
    CHECK(scene.rail->getFocused() == inbox);
    scene.press(keys::Home);
    CHECK(scene.rail->getFocused() == home);
    home->setDisabled(true);
    scene.frame();
    scene.press(keys::Down);
    CHECK(scene.rail->getFocused() == inbox);
    scene.press(keys::Space);
    CHECK(scene.rail->getSelected() == inbox);
    inbox->setDisabled(true);
    scene.frame();
    const auto *selected=scene.rail->getSelected();
    for(int key : {keys::Up, keys::Down, keys::Home, keys::End, keys::Return, keys::Space})
    {
        scene.press(key);
        CHECK(scene.rail->getFocused() == nullptr);
        CHECK(scene.rail->getSelected() == selected);
    }
    scene.checkSettled();
    for(auto *item : {disabled, home, hidden, inbox, footer})
        delete item;
    scene.frame();
    for(int key : {keys::Up, keys::Down, keys::Home, keys::End, keys::Return, keys::Space})
        scene.press(key);
    CHECK(scene.rail->getSelected() == nullptr);
    CHECK(scene.rail->getFocused() == nullptr);
    scene.checkSettled();
}

TEST_CASE("NavigationRail: textured row surfaces hide idle slots and paint hover focus and a leading selection bar without layout")
{
    using namespace skin::textured::tokyonight;
    Scene scene;
    auto *home=scene.rail->addItem("Home", "home.png");
    auto *inbox=scene.rail->addItem("Inbox", "inbox.png");
    scene.frame();
    const auto homeIds=scene.primitives(home);
    const auto inboxIds=scene.primitives(inbox);
    const auto railIds=scene.primitives(scene.rail);
    REQUIRE(homeIds.size() == 1);
    REQUIRE(inboxIds.size() == 1);
    REQUIRE(railIds.size() == 1);
    CHECK(has(scene.renderer.hidden, homeIds[0]));
    REQUIRE(drawFor(scene, railIds[0]) != nullptr);
    CHECK((drawFor(scene, railIds[0])->top == NavigationRail.top));
    CHECK(home->getIcon()->getZIndex() > home->getZIndex());
    scene.checkSettled();

    scene.hover(home);
    REQUIRE(drawFor(scene, homeIds[0]) != nullptr);
    CHECK((drawFor(scene, homeIds[0])->top == NavItemHover.top));
    CHECK(scene.renderer.writtenPrimitiveIds() == homeIds);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK((home->getTextColor() == scene.skin.Colors.NavigationItem.Normal));
    scene.hover(nullptr);
    CHECK(has(scene.renderer.hidden, homeIds[0]));
    scene.rail->setSelected(home);
    scene.frame();
    REQUIRE(drawFor(scene, homeIds[0]) != nullptr);
    CHECK((drawFor(scene, homeIds[0])->top == NavItemSelected.top));
    CHECK((drawFor(scene, homeIds[0])->border == NavItemSelected.border));
    CHECK(drawFor(scene, homeIds[0])->borderWidth == NavItemSelected.borderWidth);
    CHECK(drawFor(scene, homeIds[0])->z == home->getZIndex());
    CHECK(scene.renderer.roundedEdges[homeIds[0]] == uint8_t(renderer::SquareTopLeft
        |renderer::SquareBottomLeft|renderer::NoBorderTop|renderer::NoBorderRight|renderer::NoBorderBottom));
    CHECK((home->getLabel()->getTextColor() == scene.skin.Colors.NavigationItem.Selected));
    scene.hover(home);
    REQUIRE(drawFor(scene, homeIds[0]) != nullptr);
    CHECK((drawFor(scene, homeIds[0])->top == NavItemSelectedHover.top));
    CHECK(scene.renderer.writtenPrimitiveIds() == homeIds);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    scene.hover(nullptr);
    scene.rail->focus();
    scene.frame();
    REQUIRE(drawFor(scene, homeIds[0]) != nullptr);
    CHECK((drawFor(scene, homeIds[0])->border == NavItemSelectedFocus.border));
    CHECK(scene.renderer.writtenPrimitiveIds() == homeIds);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.press(keys::Down);
    REQUIRE(drawFor(scene, inboxIds[0]) != nullptr);
    CHECK((drawFor(scene, inboxIds[0])->top == NavItemFocus.top));
    CHECK((drawFor(scene, inboxIds[0])->border == NavItemFocus.border));
    CHECK(scene.rail->getSelected() == home);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    scene.press(keys::Return);
    std::vector<size_t> both{homeIds[0], inboxIds[0]};
    std::sort(both.begin(), both.end());
    CHECK(scene.renderer.writtenPrimitiveIds() == both);
    CHECK(scene.renderer.frameStats().textSlotsWritten == home->getText().size()+inbox->getText().size());
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.checkSettled();

    inbox->setDisabled(true);
    scene.frame();
    REQUIRE(drawFor(scene, inboxIds[0]) != nullptr);
    CHECK((drawFor(scene, inboxIds[0])->top == NavItemSelectedDisabled.top));
    CHECK((inbox->getLabel()->getTextColor() == scene.skin.Colors.NavigationItem.Disabled));
    home->setDisabled(true);
    scene.frame();
    CHECK(has(scene.renderer.hidden, homeIds[0]));
    scene.rail->hide();
    scene.frame();
    CHECK(has(scene.renderer.hidden, inboxIds[0]));
    scene.checkSettled();
}

TEST_CASE("NavigationRail: selection rewrites label glyphs only when the theme text colour changes")
{
    Scene scene;
    auto *home=scene.rail->addItem("Home");
    auto *inbox=scene.rail->addItem("Inbox");
    scene.skin.Colors.NavigationItem.Selected=scene.skin.Colors.NavigationItem.Normal;
    scene.frame();
    scene.rail->setSelected(home);
    scene.frame();
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    scene.rail->setSelected(inbox);
    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 2);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.skin.Colors.NavigationItem.Selected=skin::textured::tokyonight::Cyan;
    inbox->redraw();
    scene.frame();
    CHECK(scene.renderer.frameStats().textSlotsWritten == inbox->getText().size());
    CHECK((inbox->getLabel()->getTextColor() == scene.skin.Colors.NavigationItem.Selected));
    scene.checkSettled();
}

TEST_CASE("NavigationRail: theme surfaces and text colours are registered and editing the rail surface repaints its retained slot")
{
    using namespace skin::textured;
    Scene scene;
    scene.frame();
    for(const char *name : {"NavigationRail", "NavItemHover", "NavItemDown", "NavItemFocus",
        "NavItemFocusHover", "NavItemSelected", "NavItemSelectedHover", "NavItemSelectedFocus",
        "NavItemSelectedFocusHover", "NavItemSelectedDisabled"})
        CHECK(tokyonight::surfaceRegistry().count(name) == 1);
    TexturedSkin textured(&scene.renderer);
    const auto colors=textured.textColorRegistry();
    CHECK((colors.at("NavItemText") == &textured.Colors.NavigationItem.Normal));
    CHECK((colors.at("NavItemSelectedText") == &textured.Colors.NavigationItem.Selected));
    CHECK((colors.at("NavItemDisabledText") == &textured.Colors.NavigationItem.Disabled));
    // Restore the global surface even if the assertion fails.
    const auto original=tokyonight::NavigationRail;
    struct RestoreSurface
    {
        tokyonight::Surface saved;
        ~RestoreSurface() { tokyonight::NavigationRail=saved; }
    } restore{original};
    tokyonight::NavigationRail.top=tokyonight::Cyan;
    scene.rail->redraw();
    scene.frame();
    const auto ids=scene.primitives(scene.rail);
    REQUIRE(drawFor(scene, ids[0]) != nullptr);
    CHECK((drawFor(scene, ids[0])->top == tokyonight::Cyan));
    CHECK(scene.renderer.writtenPrimitiveIds() == ids);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    tokyonight::NavigationRail=original;
    scene.rail->redraw();
    scene.frame();
    REQUIRE(drawFor(scene, ids[0]) != nullptr);
    CHECK((drawFor(scene, ids[0])->top == original.top));
    scene.checkSettled();
}

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
