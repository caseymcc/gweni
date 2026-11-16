/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Breadcrumb on a real Canvas with the textured skin and a StubRenderer:
 *  path edits preserve every retained item's controls and ids, acquiring
 *  and releasing only the changed tail; clicks report their index, with the
 *  current item plain and inactive unless explicitly enabled. Chevron and
 *  text separators share the registered theme colour, while a hover or
 *  press writes the affected item's surface alone. Settled frames do no
 *  layout or rendering work.
 *
 *  Collapsing: at a series of widths, from the whole path down, each step
 *  of the width collapses one more item after the first ones (default 1,
 *  or setItemsBeforeCollapse) into the ellipsis, keeping the last ones
 *  (default 1, or setItemsAfterCollapse), in one layout pass, and growing
 *  back gives each layout back; below the least collapse the last item is
 *  cut at the right edge. The ellipsis's menu lists the collapsed items in
 *  path order, is filled again only when that set changed since it was
 *  last opened, and a pick fires onItemClicked with its index. The keys:
 *  one Tab stop at the first shown item, Left/Right/Home/End over the
 *  shown items and the ellipsis, Return and Space activate, the ellipsis
 *  opening its menu with its first item highlighted; a focused part draws
 *  BreadcrumbItemFocus's ring over an opaque fill (a border takes its fill's
 *  alpha, so a ring round the transparent rest look would not show).
 *  A shortened path bounds its collapsed range before another input event
 *  can reopen the menu. Disabled paths release focus, take no keys or
 *  clicks, and use the registered disabled text colour. A handler that pops
 *  the path from a Return or a menu pick leaves the focus on a shown part,
 *  so Tab and the arrows still work.
 */
#include "headlessFixture.h"

#include <gweni/controls/breadcrumb.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/menuItem.h>
#include <gweni/gweni.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

class Recorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t id, Rect rect, float radius, float width, Color top, Color bottom,
        Color border, int z) override
    {
        fills[id]=top;
        borders[id]={width, border};
        StubRenderer::drawRoundedRect(id, rect, radius, width, top, bottom, border, z);
    }

    void drawIcon(size_t id, Rect rect, int kind, int direction, Color color, int z) override
    {
        icons[id]={kind, direction};
        iconColors[id]=color;
        StubRenderer::drawIcon(id, rect, kind, direction, color, z);
    }

    std::map<size_t, Color> fills;
    std::map<size_t, std::pair<float, Color>> borders;
    std::map<size_t, std::pair<int, int>> icons;
    std::map<size_t, Color> iconColors;
};

struct Scene
{
    Recorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Breadcrumb *path=nullptr;
    double clock=nextClock();

    static double nextClock()
    {
        static double clock=0;
        return clock+=1000;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 200);
        path=canvas->newChild<controls::Breadcrumb>();
        path->setItems({"Home", "Projects", "gweni"});
        // the mouse position is process-global: start clear of the path, not
        // where an earlier case left it
        canvas->inputMouseMoved(639, 199, 0, 0);
    }

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        for(int key : {keys::Shift, keys::Tab, keys::Alt})
            input::onKeyEvent(canvas, key, false);
        HoveredControl=nullptr;
        MouseFocus=nullptr;
        KeyboardFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void hover(size_t index)
    {
        auto *item=path->getItemControl(index);
        const Point at=item->localPosToCanvas();

        canvas->inputMouseMoved(at.x+item->getWidth()/2, at.y+item->getHeight()/2, 1, 0);
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! Lays the path out \p width wide (the Breadcrumb's own width).
    void setWidth(int width)
    {
        canvas->setSize(width+canvas->getWidth()-path->getWidth(), canvas->getHeight());
        frame();
    }

    void click(size_t index)
    {
        clock+=1;
        hover(index);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }
};

//! The shown items, by index, and -1 for the ellipsis, left to right.
std::vector<int> shown(controls::Breadcrumb *path)
{
    std::vector<int> parts;

    for(size_t i=0; i < path->getItemCount(); ++i)
    {
        if(i == path->getCollapsedBegin() && path->getCollapsedEnd() > i)
        {
            CHECK(!path->getEllipsisControl()->hidden());
            parts.push_back(-1);
        }
        if(!path->getItemControl(i)->hidden())
            parts.push_back(static_cast<int>(i));
    }
    return parts;
}

//! The width the path needs with [1, 1+k) collapsed (k 0: nothing).
int neededWidth(controls::Breadcrumb *path, size_t begin, size_t end)
{
    const auto width=[&](size_t i)
        {
            auto *separator=path->getSeparatorControl(i);

            return path->getItemControl(i)->getPreferredSize().width+(separator?separator->getPreferredSize().width:0);
        };
    int total=0;

    for(size_t i=0; i < path->getItemCount(); ++i)
    {
        if(i < begin || i >= end)
            total+=width(i);
    }
    if(end > begin)
    {
        auto *separator=path->getSeparatorControl(begin);

        total+=path->getEllipsisControl()->getPreferredSize().width+(separator?separator->getPreferredSize().width:0);
    }
    return total;
}

//! The menu's shown items, in order.
void collectItems(controls::Base *control, std::vector<controls::MenuItem *> &items)
{
    for(controls::Base *child : control->getChildren())
    {
        if(auto *item=gweni_cast<controls::MenuItem>(child))
        {
            if(!item->hidden())
                items.push_back(item);
        }
        else
            collectItems(child, items);
    }
}

std::vector<String> menuLabels(controls::Breadcrumb *path)
{
    std::vector<controls::MenuItem *> items;
    std::vector<String> labels;

    collectItems(path->getEllipsisControl()->getMenu(), items);
    for(auto *item : items)
        labels.push_back(item->getText());
    return labels;
}

controls::MenuItem *menuItem(controls::Breadcrumb *path, const String &label)
{
    std::vector<controls::MenuItem *> items;

    collectItems(path->getEllipsisControl()->getMenu(), items);
    for(auto *item : items)
    {
        if(item->getText() == label)
            return item;
    }
    return nullptr;
}

size_t acquisitions(const test::StubRenderer &renderer, test::StubRenderer::Op op)
{
    return static_cast<size_t>(std::count_if(renderer.events.begin(), renderer.events.end(),
        [op](const auto &event) { return event.op == op; }));
}

}//namespace

TEST_CASE("Breadcrumb: path edits preserve the retained controls and slots")
{
    Scene scene;
    auto *home=scene.path->getItemControl(0);
    auto *projects=scene.path->getItemControl(1);
    auto *current=scene.path->getItemControl(2);
    auto *separator=scene.path->getSeparatorControl(1);

    scene.frame();
    const auto homeIds=home->getSkinControl()->getPrimitives();
    const auto projectIds=projects->getSkinControl()->getPrimitives();
    const auto separatorIds=separator->getSkinControl()->getPrimitives();
    const auto counts=test::IdBankCounts::now();

    scene.renderer.events.clear();
    scene.path->push("source", "source-id");
    scene.frame();
    CHECK(scene.path->getItemCount() == 4);
    CHECK(scene.path->getItemData(3) == "source-id");
    CHECK(scene.path->getItemControl(0) == home);
    CHECK(scene.path->getItemControl(1) == projects);
    CHECK(scene.path->getItemControl(2) == current);
    CHECK(scene.path->getSeparatorControl(1) == separator);
    CHECK(home->getSkinControl()->getPrimitives() == homeIds);
    CHECK(projects->getSkinControl()->getPrimitives() == projectIds);
    CHECK(separator->getSkinControl()->getPrimitives() == separatorIds);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::AcquirePrimitive) == 2);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::AcquireText) == 2);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::ReleasePrimitive) == 0);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::ReleaseText) == 0);
    CHECK(!current->isCurrent());

    scene.renderer.events.clear();
    scene.path->popTo(2);
    scene.frame();
    CHECK(scene.path->getItemCount() == 3);
    CHECK(current->isCurrent());
    CHECK(scene.path->getItemControl(2) == current);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::AcquirePrimitive) == 0);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::AcquireText) == 0);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::ReleasePrimitive) == 2);
    CHECK(acquisitions(scene.renderer, test::StubRenderer::Op::ReleaseText) == 2);
    CHECK(test::IdBankCounts::now().livePrimitives() == counts.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == counts.liveTexts());

    scene.path->setItemData(0, "home-id");
    CHECK(scene.path->getItem(0).name == "home-id");
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    CHECK(scene.renderer.writtenTextIds().empty());
    scene.path->setItems({"Home", "Projects", "gweni"});
    scene.frame();
    CHECK(scene.path->getItemData(0).empty());
    CHECK(scene.path->getItemControl(0) == home);
    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    CHECK(scene.renderer.writtenTextIds().empty());
    scene.path->popTo(99);
    CHECK(scene.path->getItemCount() == 3);
    scene.path->setItems({});
    scene.frame();
    CHECK(scene.path->getItemCount() == 0);
    CHECK(scene.path->getItemControl(0) == nullptr);
    CHECK(scene.path->getSeparatorControl(0) == nullptr);
}

TEST_CASE("Breadcrumb: clicks report the index and the current item is opt in")
{
    Scene scene;
    std::vector<size_t> clicked;

    scene.path->onItemClicked.addCallback(scene.path, [&](event::Handler &, event::Info info)
    {
        CHECK(info.control == scene.path);
        clicked.push_back(info.integer);
    });
    scene.frame();
    scene.click(0);
    scene.click(1);
    scene.click(2);
    CHECK(clicked == std::vector<size_t>{0, 1});
    CHECK(!scene.path->getItemControl(2)->isClickable());
    scene.path->getItemControl(2)->doAction();
    CHECK(clicked == std::vector<size_t>{0, 1});

    scene.path->setLastItemClickable(true);
    scene.click(2);
    CHECK(clicked == std::vector<size_t>{0, 1, 2});
    scene.path->setLastItemClickable(false);
    scene.path->setDisabled(true);
    scene.click(0);
    scene.path->getItemControl(0)->doAction();
    CHECK(clicked == std::vector<size_t>{0, 1, 2});
    scene.path->setDisabled(false);
    scene.path->onItemClicked.addCallback(scene.path, [&](event::Handler &, event::Info info)
    {
        scene.path->popTo(info.integer);
    });
    scene.click(1);
    scene.frame();
    CHECK(scene.path->getItemCount() == 2);
    CHECK(clicked.back() == 1);
}

TEST_CASE("Breadcrumb: theme separators and hover write only the affected parts")
{
    Scene scene;
    auto *home=scene.path->getItemControl(0);
    auto *last=scene.path->getItemControl(2);
    auto *separator=scene.path->getSeparatorControl(1);

    scene.frame();
    CHECK((home->getTextColor() == scene.skin.Colors.Breadcrumb.Text));
    CHECK((last->getTextColor() == scene.skin.Colors.Breadcrumb.CurrentText));
    CHECK((separator->getTextColor() == scene.skin.Colors.Breadcrumb.Separator));
    CHECK(separator->getWidth() == controls::internal::BreadcrumbSeparator::ChevronWidth);
    CHECK(separator->getX() == home->right());
    CHECK(scene.path->getItemControl(1)->getX() == separator->right());
    const size_t separatorId=separator->getSkinControl()->getPrimitives().at(0);
    const size_t homeId=home->getSkinControl()->getPrimitives().at(0);

    CHECK(scene.renderer.icons.at(separatorId) == std::pair<int, int>{1, 3});
    CHECK((scene.renderer.iconColors.at(separatorId) == scene.skin.Colors.Breadcrumb.Separator));
    auto registry=scene.skin.textColorRegistry();

    CHECK((registry.at("BreadcrumbText") == &scene.skin.Colors.Breadcrumb.Text));
    CHECK((registry.at("BreadcrumbCurrentText") == &scene.skin.Colors.Breadcrumb.CurrentText));
    CHECK((registry.at("BreadcrumbSeparator") == &scene.skin.Colors.Breadcrumb.Separator));
    auto &surfaces=skin::textured::tokyonight::surfaceRegistry();

    CHECK(surfaces.at("BreadcrumbItem") == &skin::textured::tokyonight::BreadcrumbItem);
    CHECK(surfaces.at("BreadcrumbItemHover") == &skin::textured::tokyonight::BreadcrumbItemHover);
    CHECK(surfaces.at("BreadcrumbItemDown") == &skin::textured::tokyonight::BreadcrumbItemDown);

    scene.hover(0);
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds() == home->getSkinControl()->getPrimitives());
    CHECK(scene.renderer.writtenTextIds().empty());
    CHECK((scene.renderer.fills.at(homeId) == skin::textured::tokyonight::BreadcrumbItemHover.top));
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds() == home->getSkinControl()->getPrimitives());
    CHECK(scene.renderer.writtenTextIds().empty());
    CHECK((scene.renderer.fills.at(homeId) == skin::textured::tokyonight::BreadcrumbItemDown.top));
    scene.canvas->inputMouseButton(0, false);
    scene.canvas->inputMouseMoved(639, 199, 1, 0);
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds() == home->getSkinControl()->getPrimitives());
    // the press focused it: the focus look at rest, until the focus goes
    CHECK(KeyboardFocus == home);
    CHECK((scene.renderer.fills.at(homeId) == skin::textured::tokyonight::BreadcrumbItemFocus.top));
    home->unfocus();
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds() == home->getSkinControl()->getPrimitives());
    CHECK((scene.renderer.fills.at(homeId) == skin::textured::tokyonight::BreadcrumbItem.top));

    scene.path->setSeparatorText("/");
    scene.frame();
    CHECK(separator->getText() == "/");
    CHECK((separator->getTextColor() == scene.skin.Colors.Breadcrumb.Separator));
    scene.path->setSeparatorText("");
    scene.frame();
    CHECK(separator->getText().empty());
    CHECK(separator->getWidth() == controls::internal::BreadcrumbSeparator::ChevronWidth);
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    CHECK(scene.renderer.writtenTextIds().empty());
    CHECK(scene.canvas->getLayoutStats().passes == 0);
}

TEST_CASE("Breadcrumb: the middle collapses one item per step and grows back")
{
    Scene scene;
    auto *path=scene.path;

    path->setItems({"Home", "Projects", "gweni", "source", "controls", "base.cpp"});
    scene.frame();
    const int whole=path->getPreferredSize().width;

    scene.setWidth(whole);
    CHECK(shown(path) == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK(path->getEllipsisControl()->hidden());
    CHECK(!path->isLastItemCut());

    std::vector<std::vector<Rect>> layouts;
    const auto bounds=[&]()
        {
            // the shown parts' bounds; a hidden one keeps where it was
            const auto of=[](controls::Base *part) { return part->hidden()?Rect():part->getBounds(); };
            std::vector<Rect> rects{of(path->getEllipsisControl())};

            for(size_t i=0; i < path->getItemCount(); ++i)
            {
                rects.push_back(of(path->getItemControl(i)));
                if(auto *separator=path->getSeparatorControl(i))
                    rects.push_back(of(separator));
            }
            return rects;
        };

    layouts.push_back(bounds());
    // each width is the least that fits with k items collapsed; one unit less
    // takes the next
    for(size_t k=1; k <= 4; ++k)
    {
        const int needed=neededWidth(path, 1, 1+k);

        scene.setWidth(needed);
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK(path->getCollapsedBegin() == 1);
        CHECK(path->getCollapsedEnd() == 1+k);
        CHECK(!path->isLastItemCut());
        CHECK(path->getItemControl(5)->right() <= path->getWidth());
        if(k < 4)
        {
            scene.setWidth(needed-1);
            CHECK(path->getCollapsedEnd() == 2+k);
        }
        scene.setWidth(needed);
        layouts.push_back(bounds());
    }
    CHECK(shown(path) == std::vector<int>{0, -1, 5});
    // the ellipsis sits after the first collapsed item's separator
    CHECK(path->getSeparatorControl(1)->visible());
    CHECK(path->getEllipsisControl()->getX() == path->getSeparatorControl(1)->right());
    CHECK(path->getSeparatorControl(5)->getX() == path->getEllipsisControl()->right());
    for(size_t i=2; i < 5; ++i)
        CHECK(path->getSeparatorControl(i)->hidden());

    // growing back gives each earlier layout back
    for(size_t k=4; k-- > 0;)
    {
        scene.setWidth(k == 0?whole:neededWidth(path, 1, 1+k));
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK((bounds() == layouts[k]));
    }
    CHECK(shown(path) == std::vector<int>{0, 1, 2, 3, 4, 5});
    CHECK(path->getEllipsisControl()->hidden());

    // two before and two after
    path->setItemsBeforeCollapse(2);
    path->setItemsAfterCollapse(2);
    scene.setWidth(whole-1);
    CHECK(shown(path) == std::vector<int>{0, 1, -1, 3, 4, 5});
    scene.setWidth(neededWidth(path, 2, 4)-1);
    CHECK(shown(path) == std::vector<int>{0, 1, -1, 4, 5});
    scene.setWidth(10);
    CHECK(shown(path) == std::vector<int>{0, 1, -1, 4, 5});
    // none before: the ellipsis comes first, with no separator
    path->setItemsBeforeCollapse(0);
    path->setItemsAfterCollapse(0);
    CHECK(path->getItemsAfterCollapse() == 1);
    scene.setWidth(neededWidth(path, 0, 5));
    CHECK(shown(path) == std::vector<int>{-1, 5});
    CHECK(path->getEllipsisControl()->getX() == 0);
    CHECK(!path->isLastItemCut());
}

TEST_CASE("Breadcrumb: the last item is cut when the least collapse does not fit")
{
    Scene scene;
    auto *path=scene.path;

    path->setItems({"Home", "Projects", "gweni", "a much longer current item"});
    scene.frame();
    const int least=neededWidth(path, 1, 3);
    auto *last=path->getItemControl(3);

    scene.setWidth(least-10);
    CHECK(shown(path) == std::vector<int>{0, -1, 3});
    CHECK(path->isLastItemCut());
    CHECK(last->right() == path->getWidth());
    CHECK(last->getWidth() == last->getPreferredSize().width-10);
    // narrower than everything before it: no width at all
    scene.setWidth(5);
    CHECK(last->getWidth() == 0);
    CHECK(path->isLastItemCut());
    // two items cannot collapse: the last is cut
    path->setItems({"Home", "a much longer current item"});
    scene.frame();
    scene.setWidth(path->getPreferredSize().width-4);
    CHECK(path->getEllipsisControl()->hidden());
    CHECK(path->isLastItemCut());
    CHECK(path->getItemControl(1)->getWidth() == path->getItemControl(1)->getPreferredSize().width-4);
    scene.setWidth(path->getPreferredSize().width);
    CHECK(!path->isLastItemCut());
}

TEST_CASE("Breadcrumb: the ellipsis menu lists the collapsed items and is filled only on a change")
{
    Scene scene;
    auto *path=scene.path;
    std::vector<size_t> clicked;

    path->setItems({"Home", "Projects", "gweni", "source", "controls", "base.cpp"});
    path->onItemClicked.addCallback(path, [&](event::Handler &, event::Info info) { clicked.push_back(info.integer); });
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 4));
    REQUIRE(path->getCollapsedEnd() == 4);
    auto *ellipsis=path->getEllipsisControl();

    CHECK(path->getMenuBuildCount() == 0);
    ellipsis->doAction();
    scene.frame();
    CHECK(ellipsis->isMenuOpen());
    CHECK(path->getMenuBuildCount() == 1);
    CHECK(menuLabels(path) == std::vector<String>{"Projects", "gweni", "source"});
    // a settled frame with the menu open writes nothing
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    CHECK(scene.renderer.writtenTextIds().empty());
    ellipsis->closeMenu();
    scene.frame();
    ellipsis->openMenu();
    scene.frame();
    CHECK(path->getMenuBuildCount() == 1);
    ellipsis->closeMenu();

    // a narrower path collapses one more: filled again at the next open
    scene.setWidth(neededWidth(path, 1, 5));
    ellipsis->openMenu();
    scene.frame();
    CHECK(path->getMenuBuildCount() == 2);
    CHECK(menuLabels(path) == std::vector<String>{"Projects", "gweni", "source", "controls"});
    ellipsis->closeMenu();
    // a collapsed item's new label is a change too
    path->setItems({"Home", "Work", "gweni", "source", "controls", "base.cpp"});
    scene.frame();
    ellipsis->openMenu();
    scene.frame();
    CHECK(path->getMenuBuildCount() == 3);
    CHECK(menuLabels(path) == std::vector<String>{"Work", "gweni", "source", "controls"});

    // a pick fires its path index and closes the menu
    REQUIRE(menuItem(path, "source"));
    menuItem(path, "source")->doAction();
    scene.frame();
    CHECK(clicked == std::vector<size_t>{3});
    CHECK(!ellipsis->isMenuOpen());
    scene.click(0);
    CHECK(clicked == std::vector<size_t>{3, 0});
}

TEST_CASE("Breadcrumb: one Tab stop and the arrows over the shown items and the ellipsis")
{
    Scene scene;
    auto *path=scene.path;
    std::vector<size_t> clicked;
    controls::Button *before=scene.canvas->newChild<controls::Button>();

    before->setTabable(true);
    before->setKeyboardInputEnabled(true);
    before->setDock(DockPosition::Bottom);
    path->setItems({"Home", "Projects", "gweni", "source", "base.cpp"});
    path->onItemClicked.addCallback(path, [&](event::Handler &, event::Info info) { clicked.push_back(info.integer); });
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 3));
    REQUIRE(shown(path) == std::vector<int>{0, -1, 3, 4});
    auto *ellipsis=path->getEllipsisControl();
    auto *home=path->getItemControl(0);
    auto *source=path->getItemControl(3);

    before->focus();
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == home);
    CHECK(path->getTabStop() == home);
    // the current item takes no focus: Right stops at source
    scene.key(keys::Right);
    CHECK(KeyboardFocus == ellipsis);
    scene.key(keys::Right);
    CHECK(KeyboardFocus == source);
    scene.key(keys::Right);
    CHECK(KeyboardFocus == source);
    scene.key(keys::Left);
    CHECK(KeyboardFocus == ellipsis);
    scene.key(keys::Home);
    CHECK(KeyboardFocus == home);
    scene.key(keys::End);
    CHECK(KeyboardFocus == source);
    // one Tab stop: Tab leaves the path
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == before);
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == home);

    scene.key(keys::Return);
    CHECK(clicked == std::vector<size_t>{0});
    scene.key(keys::Space);
    CHECK(clicked == std::vector<size_t>{0, 0});
    scene.key(keys::Right);
    scene.key(keys::Return);
    CHECK(ellipsis->isMenuOpen());
    REQUIRE(ellipsis->getMenu()->getHighlightedItem());
    CHECK(ellipsis->getMenu()->getHighlightedItem()->getText() == "Projects");
    scene.key(keys::Down);
    scene.key(keys::Return);
    CHECK(clicked == std::vector<size_t>{0, 0, 2});
    CHECK(!ellipsis->isMenuOpen());
    CHECK(KeyboardFocus == ellipsis);
    scene.key(keys::Space);
    CHECK(ellipsis->isMenuOpen());
    scene.key(keys::Escape);
    CHECK(!ellipsis->isMenuOpen());
    input::onKeyEvent(scene.canvas, keys::Alt, true);
    scene.key(keys::Down);
    input::onKeyEvent(scene.canvas, keys::Alt, false);
    CHECK(ellipsis->isMenuOpen());
    scene.key(keys::Escape);

    // a focused item draws the focus ring in its slot
    scene.key(keys::Left);
    CHECK(KeyboardFocus == home);
    scene.frame();
    const size_t homeId=home->getSkinControl()->getPrimitives().at(0);

    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    scene.key(keys::Right);
    CHECK(scene.renderer.borders.at(homeId).first == 0.0f);
    CHECK(scene.renderer.borders.at(ellipsis->getSkinControl()->getPrimitives().at(0)).first > 0.0f);
    // the ring takes its fill's alpha: the focus look has a fill of its own
    CHECK((scene.renderer.fills.at(ellipsis->getSkinControl()->getPrimitives().at(0))
        == skin::textured::tokyonight::BreadcrumbItemFocus.top));
    CHECK(skin::textured::tokyonight::BreadcrumbItemFocus.top.a == 255);

    // disabled: no Tab stop
    path->setDisabled(true);
    CHECK(path->getTabStop() == nullptr);
}

TEST_CASE("Breadcrumb: shortening the path bounds the menu before the next layout")
{
    Scene scene;
    auto *path=scene.path;
    auto *ellipsis=path->getEllipsisControl();

    path->setItems({"Home", "Projects", "gweni", "source", "controls", "base.cpp"});
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 5));
    ellipsis->openMenu();
    scene.frame();
    REQUIRE(menuLabels(path) == std::vector<String>{"Projects", "gweni", "source", "controls"});

    path->onItemClicked.addCallback(path, [path](event::Handler &, event::Info info)
    {
        path->popTo(info.integer);
    });
    REQUIRE(menuItem(path, "source"));
    menuItem(path, "source")->doAction();
    // No frame: another input event or app call can arrive before layout.
    CHECK(path->getItemCount() == 4);
    CHECK(path->getCollapsedBegin() == 1);
    CHECK(path->getCollapsedEnd() == 3);
    CHECK(!path->getItemControl(3)->hidden());
    ellipsis->openMenu();
    CHECK(menuLabels(path) == std::vector<String>{"Projects", "gweni"});
    ellipsis->closeMenu();

    path->popTo(1);
    CHECK(path->getCollapsedBegin() == 0);
    CHECK(path->getCollapsedEnd() == 0);
    CHECK(ellipsis->hidden());
    CHECK(!path->getItemControl(1)->hidden());
    ellipsis->openMenu();
    CHECK(!ellipsis->isMenuOpen());

    // setItems uses the same removal path, including a completely empty path.
    scene.frame();
    path->setItems({"Home", "Projects", "gweni", "source", "controls", "base.cpp"});
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 5));
    path->setItems({"Home", "Work", "current"});
    CHECK(path->getCollapsedEnd() == 2);
    ellipsis->openMenu();
    CHECK(menuLabels(path) == std::vector<String>{"Work"});
    path->setItems({});
    CHECK(path->getCollapsedBegin() == 0);
    CHECK(path->getCollapsedEnd() == 0);
    CHECK(ellipsis->hidden());
    CHECK(!ellipsis->isMenuOpen());
    ellipsis->openMenu();
    CHECK(!ellipsis->isMenuOpen());
    scene.frame();
}

TEST_CASE("Breadcrumb: disabling releases focus and keys until enabled")
{
    Scene scene;
    auto *path=scene.path;
    std::vector<size_t> clicked;

    path->onItemClicked.addCallback(path, [&](event::Handler &, event::Info info) { clicked.push_back(info.integer); });
    path->setItems({"Home", "Projects", "gweni", "source", "base.cpp"});
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 3));
    auto *home=path->getItemControl(0);
    auto *ellipsis=path->getEllipsisControl();

    home->focus();
    path->setDisabled(true);
    CHECK(KeyboardFocus == nullptr);
    CHECK(path->getTabStop() == nullptr);
    CHECK(!home->getKeyboardInputEnabled());
    CHECK(!ellipsis->getKeyboardInputEnabled());
    CHECK_FALSE(home->onKeyReturn(true));
    CHECK_FALSE(home->onKeySpace(true));
    scene.click(0);
    CHECK(KeyboardFocus == nullptr);
    scene.key(keys::Return);
    scene.key(keys::Space);
    CHECK(clicked.empty());

    // Path edits while disabled must not give a new or former current item keys.
    path->push("next");
    CHECK(!path->getItemControl(4)->getKeyboardInputEnabled());
    CHECK(!path->getItemControl(5)->getKeyboardInputEnabled());
    path->setLastItemClickable(true);
    CHECK(!path->getItemControl(5)->getKeyboardInputEnabled());
    path->popTo(4);
    CHECK(!path->getItemControl(4)->getKeyboardInputEnabled());
    path->setDisabled(false);
    CHECK(home->getKeyboardInputEnabled());
    CHECK(path->getItemControl(4)->getKeyboardInputEnabled());
    path->setLastItemClickable(false);
    CHECK_FALSE(path->getItemControl(4)->getKeyboardInputEnabled());
    scene.frame();
    scene.click(0);
    CHECK(KeyboardFocus == home);
    scene.key(keys::Return);
    CHECK(clicked == std::vector<size_t>{0, 0});

    // The menu's focus hand-back cannot refocus a disabled ellipsis.
    ellipsis->openMenu();
    scene.frame();
    REQUIRE(ellipsis->isMenuOpen());
    path->setDisabled(true);
    CHECK(!ellipsis->isMenuOpen());
    CHECK(KeyboardFocus == nullptr);
    scene.frame();
    scene.frame();
    CHECK(scene.renderer.writtenPrimitiveIds().empty());
    CHECK(scene.renderer.writtenTextIds().empty());
    path->setDisabled(false);
    CHECK(ellipsis->getKeyboardInputEnabled());
    ellipsis->focus();
    scene.key(keys::Return);
    CHECK(ellipsis->isMenuOpen());
}

TEST_CASE("Breadcrumb: disabled links and ellipsis use the registered theme colour")
{
    Scene scene;
    auto *path=scene.path;

    scene.skin.setThemeColors();
    path->setItems({"Home", "Projects", "gweni", "source", "base.cpp"});
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 3));
    path->setDisabled(true);
    scene.frame();
    CHECK_FALSE((path->getItemControl(0)->getTextColor() == scene.skin.Colors.Breadcrumb.Text));
    CHECK_FALSE((path->getItemControl(4)->getTextColor() == scene.skin.Colors.Breadcrumb.CurrentText));
    CHECK_FALSE((path->getEllipsisControl()->getTextColor() == scene.skin.Colors.Breadcrumb.Text));
    auto registry=scene.skin.textColorRegistry();

    REQUIRE(registry.count("BreadcrumbDisabledText") == 1);
    const Color disabled(110, 90, 140);

    *registry.at("BreadcrumbDisabledText")=disabled;
    path->getItemControl(0)->updateColors();
    path->getItemControl(4)->updateColors();
    path->getEllipsisControl()->updateColors();
    CHECK((path->getItemControl(0)->getTextColor() == disabled));
    CHECK((path->getItemControl(4)->getTextColor() == disabled));
    CHECK((path->getEllipsisControl()->getTextColor() == disabled));
    path->setDisabled(false);
    scene.frame();
    CHECK((path->getItemControl(0)->getTextColor() == scene.skin.Colors.Breadcrumb.Text));
    CHECK((path->getItemControl(4)->getTextColor() == scene.skin.Colors.Breadcrumb.CurrentText));
    CHECK((path->getEllipsisControl()->getTextColor() == scene.skin.Colors.Breadcrumb.Text));
}


TEST_CASE("Breadcrumb: navigating from the keyboard keeps the focus in the path")
{
    Scene scene;
    auto *path=scene.path;
    controls::Button *before=scene.canvas->newChild<controls::Button>();

    before->setTabable(true);
    before->setKeyboardInputEnabled(true);
    before->setDock(DockPosition::Bottom);
    // the app navigates: a click pops the path to the clicked item
    path->onItemClicked.addCallback(path, [path](event::Handler &, event::Info info)
    {
        path->popTo(info.integer);
    });
    path->setItems({"Home", "Projects", "gweni", "source", "base.cpp"});
    scene.frame();

    // Return on an item makes it the current one, which takes no focus: the
    // focus goes to the nearest part before it
    before->focus();
    scene.key(keys::Tab);
    scene.key(keys::Right);
    scene.key(keys::Right);
    REQUIRE(KeyboardFocus == path->getItemControl(2));
    scene.key(keys::Return);
    CHECK(path->getItemCount() == 3);
    CHECK(KeyboardFocus == path->getItemControl(1));
    scene.key(keys::Left);
    CHECK(KeyboardFocus == path->getItemControl(0));
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == before);
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == path->getItemControl(0));

    // a menu pick whose pop empties the collapse hides the focused ellipsis:
    // the focus goes to a shown part
    path->setItems({"Home", "Projects", "gweni", "source", "base.cpp"});
    scene.frame();
    scene.setWidth(neededWidth(path, 1, 4));
    REQUIRE(shown(path) == std::vector<int>{0, -1, 4});
    auto *ellipsis=path->getEllipsisControl();

    scene.key(keys::Right);
    REQUIRE(KeyboardFocus == ellipsis);
    scene.key(keys::Return);
    REQUIRE(ellipsis->isMenuOpen());
    scene.key(keys::Return);
    CHECK(path->getItemCount() == 2);
    CHECK(ellipsis->hidden());
    CHECK(KeyboardFocus == path->getItemControl(0));
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == before);
    scene.key(keys::Tab);
    CHECK(KeyboardFocus == path->getItemControl(0));

    // a pop with no part left to take it drops the focus
    path->setLastItemClickable(true);
    path->getItemControl(1)->focus();
    REQUIRE(KeyboardFocus == path->getItemControl(1));
    path->setItems({"Home"});
    CHECK(KeyboardFocus == path->getItemControl(0));
    path->setLastItemClickable(false);
    CHECK(KeyboardFocus == nullptr);
}
