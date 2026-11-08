/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Button renderer (ButtonTemplate<>) on a recording
 *  StubRenderer, a Button on a real Canvas clicked through the input handler:
 *  a plain button and an off toggle paint ButtonNormal, ButtonHover while
 *  hovered and ButtonDown while pressed; a toggle that is on paints ButtonOn,
 *  ButtonOnHover while hovered (moving off gives ButtonOn back) and ButtonDown
 *  while pressed, and the release flips it and paints the new state's surface;
 *  disabled, an on toggle paints ButtonOnDisabled and an off one
 *  ButtonDisabled. A plain button whose toggle state is set (setToggleState
 *  sets it on any Button) still paints as a plain one. The on surfaces differ
 *  from the off ones they stand beside. A flip marks only that button and
 *  writes only its slot, and a settled frame draws and writes nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace gweni;

namespace tn=skin::textured::tokyonight;

namespace
{

//! Records the Button renderer's rounded-rect draws, still counted as writes.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Color top, bottom, border;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float borderWidth, Color top,
        Color bottom, Color border, int zIndex) override
    {
        draws.push_back({primitiveId, top, bottom, border});
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, borderWidth, top, bottom, border, zIndex);
    }

    std::vector<Draw> draws;
};

//! The textured skin's Button renderer, a StubControl for the rest.
class ButtonSkin: public test::StubSkin
{
public:
    explicit ButtonSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Button")
            return std::make_unique<skin::textured::ButtonTemplate<skin::textured::DefaultButton>>();
        return StubSkin::getControlRenderer(typeName);
    }
};

bool sameSurface(const DrawRecorder::Draw &draw, const tn::Surface &surface)
{
    return draw.top == surface.top && draw.bottom == surface.bottom && draw.border == surface.border;
}

struct Scene
{
    DrawRecorder renderer;
    ButtonSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per click, so two clicks are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 200);
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

    //! A button in its own 80x20 cell of row \p row.
    controls::Button *button(const char *text, int row, bool toggle)
    {
        controls::Button *button=canvas->newChild<controls::Button>();

        button->setText(text);
        button->setIsToggle(toggle);
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setBounds(10, 10+row*30, 80, 20);
        return button;
    }

    void frame()
    {
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    Point centre(controls::Base *control)
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
        canvas->inputMouseMoved(299, 199, 0, 0);
        frame();
    }

    //! The press alone, on the hovered control, then a frame.
    void press()
    {
        clock+=1.0f;
        canvas->inputMouseButton(0, true);
        frame();
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    static size_t id(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives().at(0);
    }

    //! Whether this frame drew \p control's slot, and in \p surface alone.
    bool painted(controls::Base *control, const tn::Surface &surface) const
    {
        bool drew=false;

        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id != id(control))
                continue;
            if(!sameSurface(draw, surface))
                return false;
            drew=true;
        }
        return drew;
    }

    bool drewInto(controls::Base *control) const
    {
        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id == id(control))
                return true;
        }
        return false;
    }
};

bool sameColours(const tn::Surface &a, const tn::Surface &b)
{
    return a.top == b.top && a.bottom == b.bottom && a.border == b.border;
}

}//namespace

TEST_CASE("Textured Button: the on surfaces differ from the ones beside them")
{
    for(const tn::Surface *off : {&tn::ButtonNormal, &tn::ButtonHover, &tn::ButtonDown})
    {
        CHECK(!sameColours(tn::ButtonOn, *off));
        CHECK(!sameColours(tn::ButtonOnHover, *off));
    }
    CHECK(!sameColours(tn::ButtonOn, tn::ButtonOnHover));
    CHECK(!sameColours(tn::ButtonOnDisabled, tn::ButtonDisabled));

    // a theme sets them by name
    auto &registry=tn::surfaceRegistry();

    CHECK(registry.at("ButtonOn") == &tn::ButtonOn);
    CHECK(registry.at("ButtonOnHover") == &tn::ButtonOnHover);
    CHECK(registry.at("ButtonOnDisabled") == &tn::ButtonOnDisabled);
}

TEST_CASE("Textured Button: a plain button and an off toggle paint the off surfaces")
{
    Scene scene;
    controls::Button *plain=scene.button("Plain", 0, false);
    controls::Button *toggle=scene.button("Toggle", 1, true);

    scene.frame();
    CHECK(scene.painted(plain, tn::ButtonNormal));
    CHECK(scene.painted(toggle, tn::ButtonNormal));

    for(controls::Button *button : {plain, toggle})
    {
        CAPTURE(button->getText());
        scene.hover(button);
        CHECK(scene.painted(button, tn::ButtonHover));
        scene.press();
        CHECK(scene.painted(button, tn::ButtonDown));
        scene.canvas->inputMouseButton(0, false);
        scene.moveOff();
        CHECK(scene.painted(button, button == toggle ? tn::ButtonOn : tn::ButtonNormal));
    }
    CHECK(!plain->getToggleState());
    CHECK(toggle->getToggleState());

    // the toggle state of a Button that is no toggle does not show
    plain->setToggleState(true);
    scene.frame();
    CHECK(scene.painted(plain, tn::ButtonNormal));
    plain->setDisabled(true);
    scene.frame();
    CHECK(scene.painted(plain, tn::ButtonDisabled));
}

TEST_CASE("Textured Button: an on toggle paints ButtonOn, ButtonOnHover, ButtonDown and flips on release")
{
    Scene scene;
    controls::Button *toggle=scene.button("Toggle", 0, true);

    toggle->setToggleState(true);
    scene.frame();
    CHECK(scene.painted(toggle, tn::ButtonOn));

    scene.hover(toggle);
    CHECK(scene.painted(toggle, tn::ButtonOnHover));
    scene.moveOff();
    CHECK(scene.painted(toggle, tn::ButtonOn));

    // pressed, it is ButtonDown whatever its state; the release turns it off
    scene.hover(toggle);
    scene.press();
    CHECK(toggle->getToggleState());
    CHECK(scene.painted(toggle, tn::ButtonDown));
    scene.release();
    CHECK(!toggle->getToggleState());
    CHECK(scene.painted(toggle, tn::ButtonHover));

    // and back on
    scene.press();
    CHECK(scene.painted(toggle, tn::ButtonDown));
    scene.release();
    CHECK(toggle->getToggleState());
    CHECK(scene.painted(toggle, tn::ButtonOnHover));
    scene.moveOff();
    CHECK(scene.painted(toggle, tn::ButtonOn));

    // a press let go off the toggle keeps its state
    scene.hover(toggle);
    scene.press();
    scene.canvas->inputMouseMoved(299, 199, 0, 0);
    scene.release();
    CHECK(toggle->getToggleState());
    CHECK(scene.painted(toggle, tn::ButtonOn));
}

TEST_CASE("Textured Button: a disabled toggle paints ButtonOnDisabled when on, ButtonDisabled when off")
{
    Scene scene;
    controls::Button *on=scene.button("On", 0, true);
    controls::Button *off=scene.button("Off", 1, true);

    on->setToggleState(true);
    on->setDisabled(true);
    off->setDisabled(true);
    scene.frame();
    CHECK(scene.painted(on, tn::ButtonOnDisabled));
    CHECK(scene.painted(off, tn::ButtonDisabled));

    // hovered and clicked, a disabled toggle stays as it is
    scene.hover(on);
    scene.press();
    scene.release();
    CHECK(on->getToggleState());
    CHECK(!scene.drewInto(on));

    on->setToggleState(false);
    scene.frame();
    CHECK(scene.painted(on, tn::ButtonDisabled));
    on->setDisabled(false);
    off->setToggleState(true);
    scene.frame();
    CHECK(scene.painted(off, tn::ButtonOnDisabled));
}

TEST_CASE("Textured Button: a flip marks only that button and writes only its slot")
{
    Scene scene;
    std::vector<controls::Button *> buttons;

    for(int i=0; i < 4; ++i)
        buttons.push_back(scene.button("Button", i, i != 3));
    scene.frame();
    scene.frame();
    CHECK(scene.renderer.draws.empty());
    CHECK(scene.renderer.writtenPrimitiveIds().empty());

    for(bool state : {true, false})
    {
        CAPTURE(state);
        buttons[1]->setToggleState(state);
        for(size_t i=0; i < buttons.size(); ++i)
        {
            CAPTURE(i);
            CHECK(((buttons[i]->getStateChange()&controls::StateChange_Background) != 0) == (i == 1));
        }

        scene.frame();
        CHECK(scene.painted(buttons[1], state ? tn::ButtonOn : tn::ButtonNormal));
        CHECK(scene.renderer.writtenPrimitiveIds() == std::vector<size_t>{Scene::id(buttons[1])});

        // a settled frame draws and writes nothing
        scene.frame();
        CHECK(scene.renderer.draws.empty());
        CHECK(scene.renderer.writtenPrimitiveIds().empty());
        CHECK(scene.renderer.writtenTextIds().empty());
    }
}
