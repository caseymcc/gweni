/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin looks its renderers up by the control's exact type name,
 *  and Base::setSkin falls back to the parent types' renderers without a word.
 *  Its Window renderer was registered as "Window", so a WindowControl fell
 *  through ResizableControl to the Base renderer, which hides its slot, and a
 *  window drew no body. These cases go through the real TexturedSkin lookup.
 *
 *  The title-bar buttons (WindowCloseButton, WindowMaximizeButton,
 *  WindowMinimizeButton) used to override render() without ever generating
 *  their primitives, so they drew nothing; now each has a renderer of its own,
 *  a surface slot and a glyph slot (an icon of drawIcon's kinds 2-5).
 *
 *  With the real, un-init()'d TexturedSkin on a recording StubRenderer: a
 *  WindowControl gets the Window renderer and draws its body. Its close button
 *  and standalone WindowMinimizeButton / WindowMaximizeButtons resolve their
 *  own renderers (not Button) and take 2 primitive ids each (a surface hidden
 *  at rest, a glyph of icon kind 2, 3 or 4 at the button's z+1 in FgDark,
 *  centred), kept while the window is hidden and given back when it is
 *  deleted. A hover and a press draw only that button's slots (the red
 *  WindowCloseHover / WindowCloseDown with a Bg cross, ButtonHover and an Fg
 *  glyph for maximise); moving off hides the surface again, a settled frame
 *  draws nothing, and the release closes the window. A disabled button hides
 *  its surface and draws a Muted glyph. setMaximized(true) marks
 *  StateChange_Background and redraws only the glyph, as kind 5; the same
 *  value again marks and draws nothing. setClosable(false) hides both slots.
 */
#include "headlessFixture.h"

#include <gweni/controls/windowButtons.h>
#include <gweni/controls/windowControl.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! Records the slots drawn into as rounded rects and the styles they were shown with.
class RoundedRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int) override
    {
        drawn.push_back(primitiveId);
        drawnTop.push_back(top);
    }

    void showPrimitive(int style, size_t primitiveId) override
    {
        shown.push_back({style, primitiveId});
    }

    bool drewInto(size_t id) const
    {
        for(size_t drawnId : drawn)
        {
            if(drawnId == id)
                return true;
        }
        return false;
    }

    bool drewInto(size_t id, const Color &top) const
    {
        for(size_t i=0; i < drawn.size(); ++i)
        {
            if(drawn[i] == id && drawnTop[i] == top)
                return true;
        }
        return false;
    }

    int lastStyle(size_t id) const
    {
        int style=-1;

        for(const auto &entry : shown)
        {
            if(entry.second == id)
                style=entry.first;
        }
        return style;
    }

    struct Icon
    {
        size_t id;
        Rect rect;
        int kind;
        Color color;
        int zIndex;
    };

    void drawIcon(size_t primitiveId, Rect rect, int kind, int, Color color, int zIndex) override
    {
        icons.push_back({primitiveId, rect, kind, color, zIndex});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    //! The last icon drawn into the slot, or null.
    const Icon *lastIcon(size_t id) const
    {
        for(auto it=icons.rbegin(); it != icons.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    bool hid(size_t id) const
    {
        return std::find(hidden.begin(), hidden.end(), id) != hidden.end();
    }

    //! Every slot drawn into, shown or hidden since the last clear().
    std::vector<size_t> touched() const
    {
        std::vector<size_t> ids(drawn);

        for(const Icon &icon : icons)
            ids.push_back(icon.id);
        for(size_t id : hidden)
            ids.push_back(id);
        for(const auto &entry : shown)
            ids.push_back(entry.second);
        return ids;
    }

    void clear()
    {
        drawn.clear();
        drawnTop.clear();
        shown.clear();
        icons.clear();
        hidden.clear();
    }

    std::vector<size_t> drawn;
    std::vector<Color> drawnTop;    //!< each drawn rect's top colour
    std::vector<std::pair<int, size_t>> shown;
    std::vector<Icon> icons;
    std::vector<size_t> hidden;
};

//! A window and a standalone minimise and maximise button on a real Canvas,
//! drawn by the (un-init()'d) textured skin on a RoundedRecorder.
struct WindowScene
{
    RoundedRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::WindowControl *window=nullptr;
    controls::WindowCloseButton *close=nullptr;
    controls::WindowMinimizeButton *minimize=nullptr;
    controls::WindowMaximizeButton *maximize=nullptr;

    WindowScene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        window=canvas->newChild<controls::WindowControl>();
        window->setSize(200, 150);
        window->setPos(20, 30);
        close=window->findChild<controls::WindowCloseButton>("closeButton", true);

        minimize=canvas->newChild<controls::WindowMinimizeButton>();
        minimize->setBounds(300, 10, 29, 24);
        maximize=canvas->newChild<controls::WindowMaximizeButton>();
        maximize->setBounds(340, 10, 29, 24);

        // on top, as a press on it would put it, so a press reorders nothing
        window->bringToFront();
    }

    WindowScene(const WindowScene &)=delete;
    WindowScene &operator=(const WindowScene &)=delete;

    ~WindowScene()
    {
        // the hovered control is process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void moveTo(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
        frame();
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        moveTo(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    static const std::vector<size_t> &ids(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    //! True if every slot drawn into or shown since the last frame() is one of
    //! the control's. The control the mouse leaves may hide its slot again (the
    //! Dragger's Base renderer does), which writes nothing to a hidden slot.
    bool drewOnly(controls::Base *control) const
    {
        const std::vector<size_t> &own=ids(control);
        std::vector<size_t> drawn(renderer.drawn);

        for(const RoundedRecorder::Icon &icon : renderer.icons)
            drawn.push_back(icon.id);
        for(const auto &entry : renderer.shown)
            drawn.push_back(entry.second);
        for(size_t id : drawn)
        {
            if(std::find(own.begin(), own.end(), id) == own.end())
                return false;
        }
        return true;
    }
};

}//namespace

// The skin is never init()'d: creating a renderer only looks up texture entry
// ids, which an unloaded skin answers with "none", and the SDF surfaces the
// converted renderers draw do not use the atlas.
TEST_CASE("the textured skin gives a WindowControl its Window renderer, which draws the window's body")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    RoundedRecorder renderer;
    {
        skin::textured::TexturedSkin skin(&renderer);
        controls::Canvas *canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        controls::WindowControl *window=canvas->newChild<controls::WindowControl>();
        window->setSize(200, 150);
        window->setPos(20, 30);

        const skin::Control *control=window->getSkinControl();
        REQUIRE(control != nullptr);
        CHECK(dynamic_cast<const skin::textured::Window *>(control) != nullptr);

        canvas->updateCanvas();
        canvas->renderCanvas();

        const std::vector<size_t> &ids=window->getSkinControl()->getPrimitives();
        REQUIRE_FALSE(ids.empty());
        CHECK(renderer.drewInto(ids[0]));
        CHECK(renderer.lastStyle(ids[0]) == 4);

        delete canvas;
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("the window buttons get their own textured renderers, two primitive ids each, given back on delete")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        WindowScene scene;
        REQUIRE(scene.close != nullptr);

        CHECK(dynamic_cast<const skin::textured::WindowCloseButton *>(scene.close->getSkinControl()) != nullptr);
        CHECK(dynamic_cast<const skin::textured::WindowMinimizeButton *>(scene.minimize->getSkinControl()) != nullptr);
        CHECK(dynamic_cast<const skin::textured::WindowMaximizeButton *>(scene.maximize->getSkinControl()) != nullptr);

        scene.frame();

        for(controls::Base *button : {(controls::Base *)scene.close, (controls::Base *)scene.minimize,
            (controls::Base *)scene.maximize})
        {
            CAPTURE(button->getTypeName());
            const std::vector<size_t> &ids=WindowScene::ids(button);

            REQUIRE(ids.size() == 2);
            CHECK(scene.renderer.livePrimitives.count(ids[0]) == 1);
            CHECK(scene.renderer.livePrimitives.count(ids[1]) == 1);
            // at rest the surface is transparent, so its slot is hidden; the glyph
            // sits one z past the button
            CHECK(scene.renderer.hid(ids[0]));
            CHECK(!scene.renderer.drewInto(ids[0]));
            const RoundedRecorder::Icon *glyph=scene.renderer.lastIcon(ids[1]);
            REQUIRE(glyph != nullptr);
            CHECK(glyph->zIndex == button->getZIndex()+1);
            CHECK((glyph->color == skin::textured::tokyonight::FgDark));
            // a fixed square centred in the button
            const Rect bounds=button->getRenderBounds();
            CHECK(glyph->rect.w == glyph->rect.h);
            CHECK(std::abs((glyph->rect.x*2+glyph->rect.w)-(bounds.x*2+bounds.w)) <= 1);
            CHECK(std::abs((glyph->rect.y*2+glyph->rect.h)-(bounds.y*2+bounds.h)) <= 1);
        }
        CHECK(scene.renderer.lastIcon(WindowScene::ids(scene.close)[1])->kind == 2);
        CHECK(scene.renderer.lastIcon(WindowScene::ids(scene.minimize)[1])->kind == 3);
        CHECK(scene.renderer.lastIcon(WindowScene::ids(scene.maximize)[1])->kind == 4);

        // a settled frame draws nothing
        scene.frame();
        CHECK(scene.renderer.touched().empty());

        // hidden, the window keeps its ids and hides the button's two slots
        const std::vector<size_t> closeIds=WindowScene::ids(scene.close);

        scene.window->hide();
        scene.frame();
        CHECK(scene.renderer.hid(closeIds[0]));
        CHECK(scene.renderer.hid(closeIds[1]));
        CHECK(scene.renderer.livePrimitives.count(closeIds[1]) == 1);

        // deleted, it gives them back
        scene.window->delayedDelete();
        scene.frame();
        CHECK(scene.renderer.livePrimitives.count(closeIds[0]) == 0);
        CHECK(scene.renderer.livePrimitives.count(closeIds[1]) == 0);
        CHECK(scene.renderer.doubleReleased == 0);
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}

TEST_CASE("a window button's hover and press repaint only its own two slots")
{
    namespace tn=skin::textured::tokyonight;

    WindowScene scene;
    REQUIRE(scene.close != nullptr);
    scene.frame();

    // start on the title bar, whose Dragger draws nothing
    const Point title=scene.window->localPosToCanvas();
    scene.moveTo(title.x+20, title.y+10);
    scene.frame();

    const std::vector<size_t> ids=WindowScene::ids(scene.close);

    scene.moveOnto(scene.close);
    CHECK(scene.close->isHovered());
    CHECK(scene.drewOnly(scene.close));
    CHECK(scene.renderer.drewInto(ids[0], tn::WindowCloseHover.top));
    REQUIRE(scene.renderer.lastIcon(ids[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(ids[1])->color == tn::Bg));

    // moving off gives the rest look back
    scene.moveTo(title.x+20, title.y+10);
    CHECK(scene.drewOnly(scene.close));
    CHECK(scene.renderer.hid(ids[0]));
    CHECK((scene.renderer.lastIcon(ids[1])->color == tn::FgDark));
    scene.frame();
    CHECK(scene.renderer.touched().empty());

    // pressed, the darker red; let go on it, it is a click and the window closes
    scene.moveOnto(scene.close);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(scene.close->isDepressed());
    CHECK(scene.drewOnly(scene.close));
    CHECK(scene.renderer.drewInto(ids[0], tn::WindowCloseDown.top));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(!scene.window->visible());

    // the same for the standalone maximise button, which takes the blue hover
    scene.moveTo(330, 50);
    scene.moveOnto(scene.maximize);
    CHECK(scene.drewOnly(scene.maximize));
    CHECK(scene.renderer.drewInto(WindowScene::ids(scene.maximize)[0], tn::ButtonHover.top));
    CHECK((scene.renderer.lastIcon(WindowScene::ids(scene.maximize)[1])->color == tn::Fg));

    // disabled: no fill, a muted glyph
    scene.moveTo(330, 50);
    scene.maximize->setDisabled(true);
    scene.frame();
    CHECK(scene.renderer.hid(WindowScene::ids(scene.maximize)[0]));
    REQUIRE(scene.renderer.lastIcon(WindowScene::ids(scene.maximize)[1]) != nullptr);
    CHECK((scene.renderer.lastIcon(WindowScene::ids(scene.maximize)[1])->color == tn::Muted));
}

TEST_CASE("setMaximized switches the maximise button's glyph on a change only")
{
    WindowScene scene;
    scene.frame();

    const std::vector<size_t> ids=WindowScene::ids(scene.maximize);
    REQUIRE(scene.renderer.lastIcon(ids[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(ids[1])->kind == 4);
    CHECK(!scene.maximize->isMaximized());

    scene.maximize->setMaximized(true);
    CHECK(scene.maximize->isMaximized());
    CHECK((scene.maximize->getStateChange()&controls::StateChange_Background) != 0);
    scene.frame();
    REQUIRE(scene.renderer.lastIcon(ids[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(ids[1])->kind == 5);
    CHECK(scene.drewOnly(scene.maximize));
    // the surface stays hidden: only the glyph slot is drawn
    CHECK(!scene.renderer.drewInto(ids[0]));

    // the same value again marks and draws nothing
    scene.maximize->setMaximized(true);
    CHECK(scene.maximize->getStateChange() == controls::StateChange_Nothing);
    scene.frame();
    CHECK(scene.renderer.touched().empty());

    scene.maximize->setMaximized(false);
    scene.frame();
    CHECK(scene.renderer.lastIcon(ids[1])->kind == 4);
}

TEST_CASE("WindowControl::setClosable(false) hides the close button's surface and glyph")
{
    WindowScene scene;
    REQUIRE(scene.close != nullptr);
    scene.frame();

    const std::vector<size_t> ids=WindowScene::ids(scene.close);

    scene.window->setClosable(false);
    scene.frame();
    CHECK(scene.renderer.hid(ids[0]));
    CHECK(scene.renderer.hid(ids[1]));
    CHECK(scene.renderer.icons.empty());

    scene.window->setClosable(true);
    scene.frame();
    REQUIRE(scene.renderer.lastIcon(ids[1]) != nullptr);
    CHECK(scene.renderer.lastIcon(ids[1])->kind == 2);
}
