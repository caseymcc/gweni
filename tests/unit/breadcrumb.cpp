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
 */
#include "headlessFixture.h"

#include <gweni/controls/breadcrumb.h>
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
        StubRenderer::drawRoundedRect(id, rect, radius, width, top, bottom, border, z);
    }

    void drawIcon(size_t id, Rect rect, int kind, int direction, Color color, int z) override
    {
        icons[id]={kind, direction};
        iconColors[id]=color;
        StubRenderer::drawIcon(id, rect, kind, direction, color, z);
    }

    std::map<size_t, Color> fills;
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
    }

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
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

    void click(size_t index)
    {
        clock+=1;
        hover(index);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
    }
};

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
