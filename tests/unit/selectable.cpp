/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Selectable and SelectableGroup on a real Canvas, clicked through the input
 *  handler: a click toggles a row (or only selects it, untoggleable), Space and
 *  Return press it, onSelect/onDeselect fire once per real change and never
 *  for a no-op or a doEvents=false setSelected, a disabled row ignores clicks
 *  and keys; a group keeps at most one row selected and fires
 *  onSelectionChange once per change, a removed or deleted selected row leaves
 *  it with none, and a group destroyed first lets its rows go. The textured
 *  skin's Selectable renderer hides the slot of an idle row and paints the
 *  hover, press and selection surfaces, and a selection change marks only the
 *  rows involved.
 *
 *  Space and Return press a focused row, and a Return press bubbled up from
 *  a TextBox in the row (a DataTableRow cell) does not. A row added selected becomes the
 *  group's selection, and a selected row may be deleted outright or delayed.
 *  The renderer, on a recording StubRenderer, paints the hover, press,
 *  selected, selected+hover and selected+disabled surfaces with their text
 *  colours, a settled frame draws nothing, and a group's selection change
 *  marks only the two rows involved.
 */
#include "headlessFixture.h"

#include <gweni/controls/selectable.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

using namespace gweni;

namespace
{

//! Records the Selectable renderer's draws and hides.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        Color top;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color, Color, int) override
    {
        draws.push_back({primitiveId, rect, top});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    std::vector<Draw> draws;
    std::vector<size_t> hidden;
};

//! The textured skin's Selectable renderer, a StubControl for the rest.
class SelectableSkin: public test::StubSkin
{
public:
    explicit SelectableSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Selectable")
            return std::make_unique<skin::textured::Selectable>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    DrawRecorder renderer;
    SelectableSkin skin{&renderer};
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
        input::onKeyEvent(canvas, keys::Space, false);
        input::onKeyEvent(canvas, keys::Return, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    controls::Selectable *row(const char *text)
    {
        controls::Selectable *row=canvas->newChild<controls::Selectable>();

        row->setText(text);
        return row;
    }

    void frame()
    {
        renderer.draws.clear();
        renderer.hidden.clear();
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

    void press(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    static size_t id(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives().at(0);
    }

    bool drewInto(size_t id, const Color &top) const
    {
        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id == id && draw.top == top)
                return true;
        }
        return false;
    }

    bool drewInto(size_t id) const
    {
        for(const DrawRecorder::Draw &draw : renderer.draws)
        {
            if(draw.id == id)
                return true;
        }
        return false;
    }

    bool hid(size_t id) const
    {
        for(size_t hidden : renderer.hidden)
        {
            if(hidden == id)
                return true;
        }
        return false;
    }
};

//! Counts a Caller's calls and keeps the control of the last one.
class Counter: public event::Handler
{
public:
    void count(event::Info info)
    {
        ++calls;
        last=info.control;
    }

    void listen(event::Caller &caller)
    {
        caller.add(this, &Counter::count);
    }

    int calls=0;
    controls::Base *last=nullptr;
};

}//namespace

TEST_CASE("Selectable: a click toggles it, each real change fires its event once")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");
    Counter selects, deselects;

    selects.listen(row->onSelect);
    deselects.listen(row->onDeselect);
    scene.frame();

    // docked Top, Expand along x: the canvas's width
    CHECK(row->getWidth() == 300);
    CHECK(!row->isSelected());

    scene.click(row);
    CHECK(row->isSelected());
    CHECK(selects.calls == 1);
    CHECK(deselects.calls == 0);

    scene.click(row);
    CHECK(!row->isSelected());
    CHECK(selects.calls == 1);
    CHECK(deselects.calls == 1);

    // a no-op, and a change without events
    row->setSelected(false);
    CHECK(deselects.calls == 1);
    row->setSelected(true, false);
    CHECK(row->isSelected());
    CHECK(selects.calls == 1);
    row->setSelected(true);
    CHECK(selects.calls == 1);
    row->setSelected(false, false);
    CHECK(deselects.calls == 1);
}

TEST_CASE("Selectable: untoggleable, a click only selects")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");
    Counter deselects;

    deselects.listen(row->onDeselect);
    row->setToggleable(false);
    scene.frame();

    scene.click(row);
    CHECK(row->isSelected());
    scene.click(row);
    CHECK(row->isSelected());
    CHECK(deselects.calls == 0);
}

TEST_CASE("Selectable: Space and Return toggle the focused row, it is tabable")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");

    scene.frame();
    CHECK(row->isTabable());

    row->focus();
    scene.press(keys::Space);
    CHECK(row->isSelected());
    scene.press(keys::Return);
    CHECK(!row->isSelected());
}

TEST_CASE("Selectable: a Return press bubbled from a TextBox inside the row does not press it")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");
    controls::TextBox *box=row->newChild<controls::TextBox>();

    box->setText("cell");
    scene.frame();

    box->focus();
    scene.press(keys::Return);
    CHECK(!row->isSelected());

    // the row's own Return still presses it
    row->focus();
    scene.press(keys::Return);
    CHECK(row->isSelected());
}

TEST_CASE("Selectable: a disabled row ignores clicks and keys")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");
    Counter selects;

    selects.listen(row->onSelect);
    row->setDisabled(true);
    scene.frame();

    scene.click(row);
    CHECK(!row->isSelected());

    row->focus();
    scene.press(keys::Space);
    CHECK(!row->isSelected());
    CHECK(selects.calls == 0);
}

TEST_CASE("SelectableGroup: at most one selected, one event per change")
{
    Scene scene;
    controls::Selectable *a=scene.row("A");
    controls::Selectable *b=scene.row("B");
    controls::Selectable *c=scene.row("C");
    controls::SelectableGroup group;
    Counter changes, aDeselects;

    group.add(a);
    group.add(b);
    group.add(c);
    changes.listen(group.onSelectionChange);
    aDeselects.listen(a->onDeselect);
    scene.frame();

    CHECK(group.getSelected() == nullptr);
    CHECK(!a->isToggleable());

    scene.click(a);
    CHECK(group.getSelected() == a);
    CHECK(changes.calls == 1);
    CHECK(changes.last == a);

    scene.click(b);
    CHECK(group.getSelected() == b);
    CHECK(!a->isSelected());
    CHECK(b->isSelected());
    CHECK(!c->isSelected());
    CHECK(changes.calls == 2);
    CHECK(changes.last == b);
    CHECK(aDeselects.calls == 1);

    // a click on the selected row keeps it
    scene.click(b);
    CHECK(b->isSelected());
    CHECK(group.getSelected() == b);
    CHECK(changes.calls == 2);

    // from code: selecting one moves the selection, deselecting leaves none
    c->setSelected(true);
    CHECK(group.getSelected() == c);
    CHECK(!b->isSelected());
    CHECK(changes.calls == 3);
    c->setSelected(false);
    CHECK(group.getSelected() == nullptr);
    CHECK(changes.calls == 4);
    CHECK(changes.last == nullptr);

    // without events, the selection still moves
    a->setSelected(true, false);
    b->setSelected(true, false);
    CHECK(group.getSelected() == b);
    CHECK(!a->isSelected());
    CHECK(changes.calls == 4);
}

TEST_CASE("SelectableGroup: a row added selected becomes the selection, a removed one leaves")
{
    Scene scene;
    controls::Selectable *a=scene.row("A");
    controls::Selectable *b=scene.row("B");
    controls::SelectableGroup group;
    controls::SelectableGroup other;

    group.add(a);
    a->setSelected(true);
    b->setSelected(true);
    group.add(b);
    CHECK(group.getSelected() == b);
    CHECK(!a->isSelected());

    // moving to another group takes the row and its selection with it
    other.add(b);
    CHECK(b->getGroup() == &other);
    CHECK(group.getSelected() == nullptr);
    CHECK(other.getSelected() == b);
    CHECK(group.getMembers().size() == 1);

    other.remove(b);
    CHECK(b->getGroup() == nullptr);
    CHECK(b->isToggleable());
    CHECK(b->isSelected());
    CHECK(other.getSelected() == nullptr);
    CHECK(other.getMembers().empty());
}

TEST_CASE("SelectableGroup: deleting the selected row leaves the group with none")
{
    Scene scene;
    controls::Selectable *a=scene.row("A");
    controls::Selectable *b=scene.row("B");
    controls::Selectable *c=scene.row("C");
    controls::SelectableGroup group;

    group.add(a);
    group.add(b);
    group.add(c);
    scene.frame();

    scene.click(a);
    REQUIRE(group.getSelected() == a);
    delete a;
    CHECK(group.getSelected() == nullptr);
    CHECK(group.getMembers().size() == 2);

    // a delayed delete, gone once the canvas has processed it
    scene.click(b);
    REQUIRE(group.getSelected() == b);
    b->delayedDelete();
    scene.frame();
    CHECK(group.getSelected() == nullptr);
    CHECK(group.getMembers().size() == 1);
    CHECK(group.getMembers()[0] == c);

    scene.click(c);
    CHECK(group.getSelected() == c);
}

TEST_CASE("SelectableGroup: destroyed before its rows, it lets them go")
{
    Scene scene;
    controls::Selectable *row=scene.row("Row");

    {
        controls::SelectableGroup group;

        group.add(row);
        row->setSelected(true);
    }

    CHECK(row->getGroup() == nullptr);
    CHECK(row->isToggleable());
    CHECK(row->isSelected());

    scene.frame();
    scene.click(row);
    CHECK(!row->isSelected());
}

TEST_CASE("Selectable renderer: an idle row is hidden, hover, press and selection paint their surfaces")
{
    namespace tn=skin::textured::tokyonight;

    Scene scene;
    controls::Selectable *row=scene.row("Row");
    controls::Selectable *other=scene.row("Other");

    scene.frame();
    const size_t id=Scene::id(row);

    CHECK(scene.hid(id));
    CHECK(!scene.drewInto(id));
    CHECK((row->getTextColor() == scene.skin.Colors.Selectable.Normal));

    scene.hover(row);
    CHECK(scene.drewInto(id, tn::SelectableHover.top));
    CHECK(!scene.drewInto(Scene::id(other)));
    CHECK((row->getTextColor() == scene.skin.Colors.Selectable.Hover));

    // the press alone, before the release
    scene.clock+=1.0f;
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK(scene.drewInto(id, tn::SelectableDown.top));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(row->isSelected());
    CHECK(scene.drewInto(id, tn::SelectableSelectedHover.top));
    CHECK((row->getTextColor() == scene.skin.Colors.Selectable.Selected));

    scene.moveOff();
    CHECK(scene.drewInto(id, tn::SelectableSelected.top));

    // a settled frame draws nothing
    scene.frame();
    CHECK(scene.renderer.draws.empty());

    row->setDisabled(true);
    scene.frame();
    CHECK(scene.drewInto(id, tn::SelectableSelectedDisabled.top));
    CHECK((row->getTextColor() == scene.skin.Colors.Selectable.Disabled));

    row->setSelected(false);
    scene.frame();
    CHECK(scene.hid(id));
    CHECK(!scene.drewInto(id));
}

TEST_CASE("SelectableGroup: a selection change marks only the two rows involved")
{
    Scene scene;
    std::vector<controls::Selectable *> rows;
    controls::SelectableGroup group;

    for(const char *text : {"A", "B", "C", "D"})
    {
        rows.push_back(scene.row(text));
        group.add(rows.back());
    }
    rows[0]->setSelected(true);
    scene.frame();

    rows[2]->setSelected(true);
    for(size_t i=0; i < rows.size(); ++i)
    {
        const bool involved=(i == 0 || i == 2);

        CAPTURE(i);
        CHECK(((rows[i]->getStateChange()&controls::StateChange_Background) != 0) == involved);
    }

    scene.frame();
    CHECK(scene.drewInto(Scene::id(rows[2]), skin::textured::tokyonight::SelectableSelected.top));
    CHECK(scene.hid(Scene::id(rows[0])));
    CHECK(!scene.drewInto(Scene::id(rows[1])));
    CHECK(!scene.drewInto(Scene::id(rows[3])));
}
