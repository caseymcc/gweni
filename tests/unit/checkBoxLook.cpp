/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's CheckBox and RadioButton looks, on a recording
 *  StubRenderer and a real Canvas driven through the input handler: at rest the
 *  frame is CheckOff; hovering the box, or the label of its CheckBoxWithLabel /
 *  LabeledRadioButton row (whose click toggles it), paints CheckHover and
 *  leaving gives CheckOff back; a disabled box paints CheckDisabled however it
 *  is hovered, with a Muted tick or dot when checked instead of the Blue one.
 *  The RadioButton keeps its round frame (radius 1000) on every surface. A
 *  label hover marks the box with StateChange_Background and nothing else.
 */
#include "headlessFixture.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/radioButton.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace gweni;
namespace tn=gweni::skin::textured::tokyonight;

namespace
{

//! Records the surfaces, icons and hides the renderers emit.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        float radius;
        Color top;
        Color border;
    };

    struct Icon
    {
        size_t id;
        Color color;
    };

    void drawRoundedRect(size_t primitiveId, Rect, float radius, float, Color top, Color, Color border, int) override
    {
        draws.push_back({primitiveId, radius, top, border});
    }

    void drawIcon(size_t primitiveId, Rect, int, int, Color color, int) override
    {
        icons.push_back({primitiveId, color});
    }

    void clear()
    {
        draws.clear();
        icons.clear();
    }

    std::vector<Draw> draws;
    std::vector<Icon> icons;
};

//! The textured skin's CheckBox and RadioButton renderers, a StubControl for
//! the rest.
class CheckSkin: public test::StubSkin
{
public:
    explicit CheckSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "CheckBox")
            return std::make_unique<skin::textured::CheckBox>();
        if(typeName == "RadioButton")
            return std::make_unique<skin::textured::RadioButton>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    DrawRecorder renderer;
    CheckSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 200);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control and the mouse focus are process-global
        HoveredControl=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void hover(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        frame();
    }

    void moveOff()
    {
        canvas->inputMouseMoved(299, 199, 0, 0);
        frame();
    }

    static size_t id(controls::Base *control, size_t slot=0)
    {
        return control->getSkinControl()->getPrimitives().at(slot);
    }

    //! The last frame surface drawn into the control's first slot.
    const DrawRecorder::Draw *frameOf(controls::Base *control) const
    {
        const DrawRecorder::Draw *found=nullptr;

        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id == id(control))
                found=&draw;
        }
        return found;
    }

    bool framed(controls::Base *control, const tn::Surface &surface) const
    {
        const DrawRecorder::Draw *draw=frameOf(control);

        return draw && (draw->top == surface.top) && (draw->border == surface.border);
    }
};

} //namespace

TEST_CASE("CheckBox look: hovering the box or its label paints CheckHover, leaving gives CheckOff back")
{
    Scene scene;
    controls::CheckBoxWithLabel *row=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    row->label()->setText("Label");
    row->setPos(10, 10);
    controls::CheckBox *box=row->checkBox();

    scene.frame();
    CHECK(scene.framed(box, tn::CheckOff));

    scene.hover(box);
    REQUIRE(box->isHovered());
    CHECK(scene.framed(box, tn::CheckHover));

    scene.moveOff();
    CHECK(scene.framed(box, tn::CheckOff));

    scene.hover(row->label());
    REQUIRE(row->label()->isHovered());
    CHECK(box->isLabelHovered());
    CHECK(scene.framed(box, tn::CheckHover));

    scene.moveOff();
    CHECK_FALSE(box->isLabelHovered());
    CHECK(scene.framed(box, tn::CheckOff));
}

TEST_CASE("CheckBox look: a label hover marks the box's background and nothing else")
{
    Scene scene;
    controls::CheckBoxWithLabel *row=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    row->label()->setText("Label");
    controls::CheckBox *box=row->checkBox();
    scene.frame();

    box->setLabelHovered(true);
    CHECK(box->getStateChange() == controls::StateChange_Background);
    scene.frame();
    CHECK(box->getStateChange() == controls::StateChange_Nothing);

    // no change, no repaint
    box->setLabelHovered(true);
    CHECK(box->getStateChange() == controls::StateChange_Nothing);
    box->setLabelHovered(false);
    CHECK(box->getStateChange() == controls::StateChange_Background);
}

TEST_CASE("CheckBox look: a disabled box paints CheckDisabled however it is hovered, its tick Muted")
{
    Scene scene;
    controls::CheckBoxWithLabel *row=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    row->label()->setText("Label");
    row->setPos(10, 10);
    controls::CheckBox *box=row->checkBox();

    box->setChecked(true);
    scene.frame();
    REQUIRE(scene.renderer.icons.size() == 1);
    CHECK(scene.renderer.icons[0].id == Scene::id(box, 1));
    CHECK((scene.renderer.icons[0].color == tn::Blue));

    box->setDisabled(true);
    scene.frame();
    CHECK(scene.framed(box, tn::CheckDisabled));
    REQUIRE(scene.renderer.icons.size() == 1);
    CHECK((scene.renderer.icons[0].color == tn::Muted));

    scene.hover(box);
    CHECK(scene.framed(box, tn::CheckDisabled));

    scene.hover(row->label());
    CHECK(box->isLabelHovered());
    CHECK(scene.framed(box, tn::CheckDisabled));

    // unchecked and disabled: the same frame, no tick
    box->setChecked(false);
    scene.frame();
    CHECK(scene.framed(box, tn::CheckDisabled));
    CHECK(scene.renderer.icons.empty());
}

TEST_CASE("RadioButton look: round on every surface, lit by its label, muted dot when disabled")
{
    Scene scene;
    controls::LabeledRadioButton *row=scene.canvas->newChild<controls::LabeledRadioButton>();
    row->setPos(10, 10);
    controls::RadioButton *radio=row->getRadioButton();

    scene.frame();
    CHECK(scene.framed(radio, tn::CheckOff));
    CHECK(scene.frameOf(radio)->radius == 1000.0f);

    scene.hover(radio);
    CHECK(scene.framed(radio, tn::CheckHover));
    CHECK(scene.frameOf(radio)->radius == 1000.0f);

    scene.hover(row->getLabel());
    CHECK(scene.framed(radio, tn::CheckHover));

    scene.moveOff();
    CHECK(scene.framed(radio, tn::CheckOff));

    row->select();
    scene.frame();
    const auto dot=[&]() -> const DrawRecorder::Draw *
    {
        for(const DrawRecorder::Draw &draw : scene.renderer.draws)
        {
            if(draw.id == Scene::id(radio, 1))
                return &draw;
        }
        return nullptr;
    };
    REQUIRE(dot());
    CHECK((dot()->top == tn::Blue));

    radio->setDisabled(true);
    scene.frame();
    CHECK(scene.framed(radio, tn::CheckDisabled));
    CHECK(scene.frameOf(radio)->radius == 1000.0f);
    REQUIRE(dot());
    CHECK((dot()->top == tn::Muted));

    scene.hover(radio);
    CHECK(scene.framed(radio, tn::CheckDisabled));
}
