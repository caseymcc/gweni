/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  CheckBoxWithLabel and LabeledRadioButton disabled through the row's own
 *  setDisabled, on a real Canvas clicked through the input handler: the flag
 *  reaches the box and the label (and setDisabled(false) takes it off all
 *  three); while disabled, a click on the box or the label and onKeySpace on
 *  the row (down and up) toggle nothing and fire no onChecked/onCheckChanged,
 *  and once enabled again each of them toggles it. A RadioButtonController
 *  option disabled with its row's setDisabled is not selected by a click on its
 *  radio or its label and fires no onSelectionChange. On a recording
 *  StubRenderer with the textured CheckBox renderer, the row's
 *  setDisabled(true) repaints the box with CheckDisabled and a Muted tick and
 *  gives the label the disabled text colour, marks StateChange_Background on
 *  the box, the label and the row (which draws nothing) and on no other
 *  control, and the settled frame after it writes nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/radioButton.h>
#include <gweni/controls/radioButtonController.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace gweni;
namespace tn=gweni::skin::textured::tokyonight;

namespace
{

//! Records the surfaces and icons the renderers emit, each still counted as
//! written by the StubRenderer.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Color top;
        Color border;
    };

    struct Icon
    {
        size_t id;
        Color color;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float borderWidth, Color top,
        Color bottom, Color border, int zIndex) override
    {
        draws.push_back({primitiveId, top, border});
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, borderWidth, top, bottom, border, zIndex);
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int zIndex) override
    {
        icons.push_back({primitiveId, color});
        StubRenderer::drawIcon(primitiveId, rect, kind, direction, color, zIndex);
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

    void frame()
    {
        renderer.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! A whole left click on the control, then a frame.
    void click(controls::Base *control)
    {
        clock+=1.0f;
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        canvas->inputMouseButton(0, true);
        canvas->inputMouseButton(0, false);
        frame();
    }

    //! Space down and up on the row, then a frame.
    void space(controls::Base *row)
    {
        row->onKeySpace(true);
        row->onKeySpace(false);
        frame();
    }

    static size_t id(controls::Base *control, size_t slot=0)
    {
        return control->getSkinControl()->getPrimitives().at(slot);
    }

    //! The last surface drawn into the control's first slot.
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

//! Counts a Caller's calls.
class Counter: public event::Handler
{
public:
    void count(event::Info)
    {
        ++calls;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
};

//! Every control under (and including) the given one with a pending change.
void collectMarked(controls::Base *control, std::vector<controls::Base *> &marked)
{
    if(control->getStateChange() != controls::StateChange_Nothing)
        marked.push_back(control);

    for(controls::Base *child : control->getChildren())
        collectMarked(child, marked);
}

bool contains(const std::vector<controls::Base *> &controls, controls::Base *control)
{
    for(controls::Base *entry : controls)
    {
        if(entry == control)
            return true;
    }
    return false;
}

} //namespace

TEST_CASE("CheckBox disabled: the row's setDisabled reaches the box and the label, and comes off them")
{
    Scene scene;
    controls::CheckBoxWithLabel *check=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    controls::LabeledRadioButton *radio=scene.canvas->newChild<controls::LabeledRadioButton>();
    scene.frame();

    check->setDisabled(true);
    radio->setDisabled(true);
    CHECK(check->isDisabled());
    CHECK(check->checkBox()->isDisabled());
    CHECK(check->label()->isDisabled());
    CHECK(radio->isDisabled());
    CHECK(radio->getRadioButton()->isDisabled());
    CHECK(radio->getLabel()->isDisabled());

    check->setDisabled(false);
    radio->setDisabled(false);
    CHECK_FALSE(check->isDisabled());
    CHECK_FALSE(check->checkBox()->isDisabled());
    CHECK_FALSE(check->label()->isDisabled());
    CHECK_FALSE(radio->isDisabled());
    CHECK_FALSE(radio->getRadioButton()->isDisabled());
    CHECK_FALSE(radio->getLabel()->isDisabled());
}

TEST_CASE("CheckBox disabled: a disabled CheckBoxWithLabel ignores clicks and Space, an enabled one toggles again")
{
    Scene scene;
    controls::CheckBoxWithLabel *row=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    row->label()->setText("Setting");
    row->setPos(10, 10);
    controls::CheckBox *box=row->checkBox();
    Counter checks, changes;

    checks.listen(box->onChecked);
    changes.listen(box->onCheckChanged);
    row->setDisabled(true);
    scene.frame();

    scene.click(box);
    CHECK_FALSE(box->isChecked());
    scene.click(row->label());
    CHECK_FALSE(box->isChecked());
    scene.space(row);
    CHECK_FALSE(box->isChecked());
    CHECK(checks.calls == 0);
    CHECK(changes.calls == 0);

    row->setDisabled(false);
    scene.frame();

    scene.click(box);
    CHECK(box->isChecked());
    CHECK(checks.calls == 1);
    CHECK(changes.calls == 1);
    scene.click(row->label());
    CHECK_FALSE(box->isChecked());
    CHECK(changes.calls == 2);
    scene.space(row);
    CHECK(box->isChecked());
    CHECK(checks.calls == 2);
    CHECK(changes.calls == 3);
}

TEST_CASE("CheckBox disabled: a disabled LabeledRadioButton ignores clicks and Space, an enabled one selects again")
{
    Scene scene;
    controls::LabeledRadioButton *row=scene.canvas->newChild<controls::LabeledRadioButton>();
    row->setPos(10, 10);
    controls::RadioButton *radio=row->getRadioButton();
    Counter checks, changes;

    checks.listen(radio->onChecked);
    changes.listen(radio->onCheckChanged);
    row->setDisabled(true);
    scene.frame();

    scene.click(radio);
    CHECK_FALSE(radio->isChecked());
    scene.click(row->getLabel());
    CHECK_FALSE(radio->isChecked());
    scene.space(row);
    CHECK_FALSE(radio->isChecked());
    CHECK(checks.calls == 0);
    CHECK(changes.calls == 0);

    row->setDisabled(false);
    scene.frame();

    scene.click(radio);
    CHECK(radio->isChecked());
    CHECK(checks.calls == 1);
    CHECK(changes.calls == 1);

    // a click does not uncheck a radio; Space (on key down) toggles it
    scene.space(row);
    CHECK_FALSE(radio->isChecked());
    CHECK(changes.calls == 2);

    scene.click(row->getLabel());
    CHECK(radio->isChecked());
    CHECK(checks.calls == 2);
    CHECK(changes.calls == 3);
}

TEST_CASE("CheckBox disabled: a disabled RadioButtonController option cannot be selected")
{
    Scene scene;
    controls::RadioButtonController *group=scene.canvas->newChild<controls::RadioButtonController>();
    group->setSize(200, 100);
    controls::LabeledRadioButton *a=group->addOption("Option A");
    controls::LabeledRadioButton *b=group->addOption("Option B");
    Counter changes;

    changes.listen(group->onSelectionChange);
    b->setDisabled(true);
    scene.frame();

    scene.click(b->getRadioButton());
    scene.click(b->getLabel());
    CHECK_FALSE(b->getRadioButton()->isChecked());
    CHECK(group->getSelected() == nullptr);
    CHECK(changes.calls == 0);

    scene.click(a->getLabel());
    CHECK(group->getSelected() == a);
    CHECK(changes.calls == 1);

    scene.click(b->getRadioButton());
    CHECK(group->getSelected() == a);
    CHECK(a->getRadioButton()->isChecked());
    CHECK(changes.calls == 1);
}

TEST_CASE("CheckBox disabled: the row's setDisabled repaints the box and the label alone")
{
    Scene scene;
    controls::CheckBoxWithLabel *row=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    row->label()->setText("Setting");
    row->setPos(10, 10);
    controls::CheckBoxWithLabel *other=scene.canvas->newChild<controls::CheckBoxWithLabel>();
    other->label()->setText("Other");
    other->setPos(10, 40);
    controls::CheckBox *box=row->checkBox();

    box->setChecked(true);
    scene.frame();
    scene.frame();
    REQUIRE(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK((row->label()->getTextColor() == scene.skin.Colors.Button.Normal));

    row->setDisabled(true);
    std::vector<controls::Base *> marked;
    collectMarked(scene.canvas, marked);
    CHECK(marked.size() == 3);
    CHECK(contains(marked, row));
    CHECK(contains(marked, box));
    CHECK(contains(marked, row->label()));
    CHECK(box->getStateChange() == controls::StateChange_Background);
    CHECK(row->label()->getStateChange() == controls::StateChange_Background);
    CHECK((row->label()->getTextColor() == scene.skin.Colors.Button.Disabled));

    scene.frame();
    CHECK(scene.framed(box, tn::CheckDisabled));
    REQUIRE(scene.renderer.icons.size() == 1);
    CHECK(scene.renderer.icons[0].id == Scene::id(box, 1));
    CHECK((scene.renderer.icons[0].color == tn::Muted));
    CHECK(scene.frameOf(other->checkBox()) == nullptr);

    scene.frame();
    CHECK(scene.renderer.frameStats().primitiveSlotsWritten == 0);
    CHECK(scene.renderer.frameStats().textSlotsWritten == 0);
}
