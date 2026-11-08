/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  VectorField on a real Canvas with the real (un-init()'d) textured skin on a
 *  recording StubRenderer, dragged and clicked through input::onMouseClicked
 *  and Canvas::inputMouseMoved, keyed through input::onKeyEvent and typed
 *  through Canvas::inputCharacter.
 *
 *  The components split the row's width in whole units with Gap between
 *  them, each (width-gaps)/count wide and the last taking the remainder, so
 *  a row grown by a unit widens its last component alone; with axis tags each
 *  component is a TagWidth tag and its field, at 2, 3 and 4 components and
 *  several widths. onChanged fires once per real change, for drags, typed
 *  edits, key steps and setValue() on each component, with the component's
 *  index in info.integer and its field in info.control, and never for a
 *  value that stays or doEvents false. A change in one component leaves
 *  nothing pending outside that field. Tab and Shift+Tab step through the
 *  fields in order, with and without tags, and wrap. Adding components or
 *  tags takes ids, removing them gives them back, and deleting the row gives
 *  every id back. The shared range, speed, format and rounding reach the
 *  components added later; a refused format changes nothing. The textured
 *  VectorFieldTag renderer draws each tag in its axis's surface, the
 *  disabled field's while disabled, and the row itself draws nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/dragNumber.h>
#include <gweni/controls/text.h>
#include <gweni/controls/vectorField.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <map>
#include <vector>

using namespace gweni;
using controls::DragNumber;
using controls::VectorField;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each slot's last rounded-rect fill.
class FillRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color, int) override
    {
        fills[primitiveId]=top;
        ++draws;
    }

    std::map<size_t, Color> fills;
    int draws=0;
};

struct Scene
{
    FillRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=nextClock();

    //! The input handler keeps the last click's time and place across cases,
    //! so each case's clock starts well after the last one's.
    static float nextClock()
    {
        static float start=0.0f;

        start+=1000.0f;
        return start;
    }

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button and key state are
        // process-global
        input::onMouseClicked(canvas, 0, false);
        for(int key : {keys::Shift, keys::Alt, keys::Control, keys::Tab, keys::Return})
            input::onKeyEvent(canvas, key, false);
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
    }

    VectorField *row(int components, int width=300, bool tags=false)
    {
        VectorField *field=canvas->newChild<VectorField>();

        field->setComponentCount(components);
        field->setAxisTags(tags);
        field->setPos(20, 20);
        field->setSize(width, 20);
        frame();
        return field;
    }

    Point centreOf(controls::Base *control) const
    {
        const Point at=control->localPosToCanvas();

        return Point(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void move(int x, int y)
    {
        canvas->inputMouseMoved(x, y, 0, 0);
    }

    Point press(controls::Base *control)
    {
        const Point at=centreOf(control);

        clock+=1.0f;
        move(at.x, at.y);
        input::onMouseClicked(canvas, 0, true);
        return at;
    }

    void release()
    {
        input::onMouseClicked(canvas, 0, false);
    }

    void doubleClick(controls::Base *control)
    {
        press(control);
        release();
        input::onMouseClicked(canvas, 0, true);
        release();
        frame();
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    void type(const char *text)
    {
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }
};

//! Records onChanged's component and field.
struct Changes: public event::Handler
{
    std::vector<size_t> components;
    std::vector<controls::Base *> fields;

    void changed(event::Info info)
    {
        components.push_back(info.integer);
        fields.push_back(info.control);
    }
};

//! The controls in \p control's subtree with a change pending and not under
//! \p allowed.
void pendingOutside(controls::Base *control, controls::Base *allowed, std::vector<controls::Base *> &out)
{
    bool under=false;

    for(controls::Base *at=control; at; at=at->getParent())
    {
        if(at == allowed)
            under=true;
    }
    if(!under && control->getStateChange() != controls::StateChange_Nothing)
        out.push_back(control);
    for(controls::Base *child : control->getActualChildren())
        pendingOutside(child, allowed, out);
}

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

}//namespace

TEST_CASE("VectorField: the components split the width in whole units, the last taking the remainder")
{
    for(int count=VectorField::MinComponents; count <= VectorField::MaxComponents; ++count)
    {
        for(int width : {300, 301, 302, 303, 97, 64})
        {
            CAPTURE(count);
            CAPTURE(width);

            int firstX, firstW;
            VectorField::componentSpan(width, count, 0, firstX, firstW);
            CHECK(firstX == 0);

            int end=0;
            for(int i=0; i < count; ++i)
            {
                int x, w;
                VectorField::componentSpan(width, count, i, x, w);

                CHECK(x == (i == 0 ? 0 : end+VectorField::Gap));
                if(i < count-1)
                    CHECK(w == firstW);
                else
                    CHECK((w >= firstW && w < firstW+count));
                end=x+w;
            }
            // the row's width exactly
            CHECK(end == width);
        }

        // a unit wider: only the last component grows, unless the extra unit
        // makes a whole unit more for every component
        for(int width=300; width < 300+count; ++width)
        {
            const int available=width-VectorField::Gap*(count-1);

            if((available+1)%count == 0)
                continue;
            for(int i=0; i < count-1; ++i)
            {
                int x, w, x2, w2;
                VectorField::componentSpan(width, count, i, x, w);
                VectorField::componentSpan(width+1, count, i, x2, w2);
                CHECK(x == x2);
                CHECK(w == w2);
            }
        }
    }

    // no room for the gaps: no width
    int x, w;
    VectorField::componentSpan(5, 4, 3, x, w);
    CHECK(w == 0);
}

TEST_CASE("VectorField: the fields and tags are placed at their spans, at 2, 3 and 4 components")
{
    for(bool tags : {false, true})
    {
        for(int count=2; count <= 4; ++count)
        {
            for(int width : {300, 301, 250})
            {
                CAPTURE(tags);
                CAPTURE(count);
                CAPTURE(width);

                Scene scene;
                VectorField *row=scene.row(count, width, tags);

                REQUIRE(row->getComponentCount() == count);
                CHECK(row->getWidth() == width);
                CHECK(row->getHeight() == 20);
                for(int i=0; i < count; ++i)
                {
                    int x, w;
                    VectorField::componentSpan(width, count, i, x, w);

                    DragNumber *field=row->getField(i);
                    const int tag=tags?VectorField::TagWidth:0;

                    REQUIRE(field != nullptr);
                    CHECK((field->getBounds() == Rect(x+tag, 0, w-tag, 20)));
                    if(tags)
                    {
                        REQUIRE(row->getTag(i) != nullptr);
                        CHECK((row->getTag(i)->getBounds() == Rect(x, 0, tag, 20)));
                        CHECK(row->getTag(i)->getText() == VectorField::axisName(i));
                    }
                    else
                        CHECK(row->getTag(i) == nullptr);
                }
                CHECK(row->getField(count) == nullptr);
                CHECK(row->getField(-1) == nullptr);

                // resized in a later frame, placed again
                row->setSize(width+40, 20);
                scene.frame();
                int x, w;
                VectorField::componentSpan(width+40, count, count-1, x, w);
                CHECK(row->getField(count-1)->getX() == x+(tags?VectorField::TagWidth:0));
                CHECK(row->getField(count-1)->right() == width+40);
            }
        }
    }
}

TEST_CASE("VectorField: a Fixed row is the size asked for, an Elastic one its fields' minimum")
{
    Scene scene;
    VectorField *fixed=scene.row(3, 300, true);

    CHECK(fixed->getPreferredSize().width == 300);
    CHECK(fixed->getPreferredSize().height == 20);

    VectorField *elastic=scene.row(3, 300, true);
    elastic->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    scene.frame();
    const int tagged=3*(VectorField::MinFieldWidth+VectorField::TagWidth)+2*VectorField::Gap;
    CHECK(elastic->getPreferredSize().width == tagged);
    CHECK(elastic->getWidth() == tagged);

    // a component more, and the tags off
    elastic->setComponentCount(4);
    elastic->setAxisTags(false);
    scene.frame();
    CHECK(elastic->getPreferredSize().width == 4*VectorField::MinFieldWidth+3*VectorField::Gap);
    CHECK(elastic->getField(3)->right() == elastic->getWidth());
}

TEST_CASE("VectorField: onChanged carries the component for drags, typed edits, key steps and setValue")
{
    Scene scene;
    VectorField *row=scene.row(4, 400, true);
    Changes changes;

    row->onChanged.add(&changes, &Changes::changed);

    for(int i=0; i < 4; ++i)
    {
        CAPTURE(i);
        DragNumber *field=row->getField(i);

        // a drag of two units: two changes of 0.1
        changes.components.clear();
        changes.fields.clear();
        const Point at=scene.press(field);
        scene.move(at.x+1, at.y);
        scene.move(at.x+2, at.y);
        scene.move(at.x+2, at.y+5);     // vertical only: no change
        scene.release();
        scene.frame();
        CHECK(row->getValue(i) == doctest::Approx(0.2));
        CHECK(changes.components == std::vector<size_t>{size_t(i), size_t(i)});
        CHECK(changes.fields == std::vector<controls::Base *>{field, field});

        // typed: once; the same value typed again: never
        changes.components.clear();
        scene.doubleClick(field);
        REQUIRE(field->isEditing());
        scene.type("5");
        scene.key(keys::Return);
        CHECK(row->getValue(i) == 5.0);
        CHECK(changes.components == std::vector<size_t>{size_t(i)});
        scene.doubleClick(field);
        scene.type("5");
        scene.key(keys::Return);
        CHECK(changes.components.size() == 1);

        // a key step on the focused field
        REQUIRE(KeyboardFocus == field);
        scene.key(keys::Up);
        CHECK(row->getValue(i) == doctest::Approx(5.1));
        CHECK(changes.components == std::vector<size_t>{size_t(i), size_t(i)});

        // setValue: a change, the same value, and no events
        changes.components.clear();
        row->setValue(i, 7.0);
        row->setValue(i, 7.0);
        row->setValue(i, 8.0, false);
        CHECK(changes.components == std::vector<size_t>{size_t(i)});
        CHECK(row->getValue(i) == 8.0);
        scene.frame();
    }

    // setValues: one event per component that changed
    changes.components.clear();
    row->setValues({8.0, 1.0, 8.0, 2.0});
    CHECK(changes.components == std::vector<size_t>{1, 3});
    CHECK((row->getValues() == std::array<double, 4>{8.0, 1.0, 8.0, 2.0}));
    row->setValues({0.0, 0.0, 0.0, 0.0}, false);
    CHECK(changes.components.size() == 2);

    // out of range: nothing
    row->setValue(4, 1.0);
    CHECK(row->getValue(4) == 0.0);
    CHECK(changes.components.size() == 2);
}

TEST_CASE("VectorField: a change in one component leaves nothing pending outside its field")
{
    Scene scene;
    VectorField *row=scene.row(3, 300, true);
    DragNumber *y=row->getField(1);

    scene.frame();

    row->setValue(1, 2.5);
    std::vector<controls::Base *> outside;
    pendingOutside(scene.canvas, y, outside);
    CHECK(outside.empty());
    scene.frame();

    // a drag frame of the Y field, the press's look settled first
    const Point at=scene.press(y);
    scene.frame();
    scene.move(at.x+3, at.y);
    CHECK(row->getValue(1) == doctest::Approx(2.8));
    outside.clear();
    pendingOutside(scene.canvas, y, outside);
    CHECK(outside.empty());
    scene.release();
    scene.frame();

    // the other components kept their values
    CHECK(row->getValue(0) == 0.0);
    CHECK(row->getValue(2) == 0.0);
}

TEST_CASE("VectorField: Tab and Shift+Tab step through the fields in order and wrap")
{
    for(bool tags : {false, true})
    {
        CAPTURE(tags);
        Scene scene;
        VectorField *row=scene.row(4, 400, tags);

        row->getField(0)->focus();
        for(int i=1; i < 4; ++i)
        {
            scene.key(keys::Tab);
            CHECK(KeyboardFocus == row->getField(i));
        }
        // the only tabable controls on the canvas: back to the first
        scene.key(keys::Tab);
        CHECK(KeyboardFocus == row->getField(0));

        // Shift held: no frame between the presses, since a frame releases a
        // key held across a focus change (it was pressed for another control)
        input::onKeyEvent(scene.canvas, keys::Shift, true);
        for(int i : {3, 2, 1, 0})
        {
            input::onKeyEvent(scene.canvas, keys::Tab, true);
            input::onKeyEvent(scene.canvas, keys::Tab, false);
            CHECK(KeyboardFocus == row->getField(i));
        }
        input::onKeyEvent(scene.canvas, keys::Shift, false);

        // tags turned on afterwards keep the order
        row->setAxisTags(!tags);
        scene.frame();
        row->getField(0)->focus();
        for(int i=1; i < 4; ++i)
        {
            scene.key(keys::Tab);
            CHECK(KeyboardFocus == row->getField(i));
        }
    }
}

TEST_CASE("VectorField: components and tags take ids when added and give them back when removed")
{
    Scene scene;
    const test::IdBankCounts before=test::IdBankCounts::now();
    VectorField *row=scene.row(2, 300, false);
    const test::IdBankCounts two=test::IdBankCounts::now();

    // the row's own (Base) Control's slot, and per field its slot and its Text
    CHECK(two.livePrimitives()-before.livePrimitives() == 3);
    CHECK(two.liveTexts()-before.liveTexts() == 2);

    row->setComponentCount(4);
    scene.frame();
    const test::IdBankCounts four=test::IdBankCounts::now();
    CHECK(four.livePrimitives()-two.livePrimitives() == 2);
    CHECK(four.liveTexts()-two.liveTexts() == 2);

    // a tag each: its slot and its Text
    row->setAxisTags(true);
    scene.frame();
    const test::IdBankCounts tagged=test::IdBankCounts::now();
    CHECK(tagged.livePrimitives()-four.livePrimitives() == 4);
    CHECK(tagged.liveTexts()-four.liveTexts() == 4);

    // down to three: a field and a tag go
    row->setComponentCount(3);
    scene.frame();
    const test::IdBankCounts three=test::IdBankCounts::now();
    CHECK(tagged.livePrimitives()-three.livePrimitives() == 2);
    CHECK(tagged.liveTexts()-three.liveTexts() == 2);
    CHECK(row->getField(3) == nullptr);
    CHECK(row->getTag(3) == nullptr);

    row->setAxisTags(false);
    scene.frame();
    const test::IdBankCounts untagged=test::IdBankCounts::now();
    CHECK(three.livePrimitives()-untagged.livePrimitives() == 3);
    CHECK(three.liveTexts()-untagged.liveTexts() == 3);

    // clamped to 2..4
    row->setComponentCount(9);
    CHECK(row->getComponentCount() == 4);
    row->setComponentCount(0);
    CHECK(row->getComponentCount() == 2);
    scene.frame();

    row->delayedDelete();
    scene.frame();
    scene.frame();
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == before.livePrimitives());
    CHECK(after.liveTexts() == before.liveTexts());
}

TEST_CASE("VectorField: the shared range, speed, format and rounding reach components added later")
{
    Scene scene;
    VectorField *row=scene.row(2);

    row->setRange(0.0, 10.0);
    row->setSpeed(0.5);
    CHECK(row->setFormat("%.1f"));
    row->setValues({20.0, -3.0, 0.0, 0.0});
    CHECK(row->getValue(0) == 10.0);
    CHECK(row->getValue(1) == 0.0);

    row->setComponentCount(4);
    for(int i=0; i < 4; ++i)
    {
        CAPTURE(i);
        DragNumber *field=row->getField(i);
        CHECK(field->getMin() == 0.0);
        CHECK(field->getMax() == 10.0);
        CHECK(field->getSpeed() == 0.5);
        CHECK(field->getFormat() == "%.1f");
    }
    CHECK(row->getField(3)->getText() == "0.0");

    // a refused format changes nothing
    CHECK_FALSE(row->setFormat("%s"));
    for(int i=0; i < 4; ++i)
        CHECK(row->getField(i)->getFormat() == "%.1f");

    // one component's range alone
    row->setRange(2, 5.0, 6.0);
    CHECK(row->getValue(2) == 5.0);
    CHECK(row->getField(1)->getMax() == 10.0);

    row->setInteger(true);
    row->setComponentCount(2);
    row->setComponentCount(3);
    CHECK(row->getField(2)->isInteger());
    CHECK(row->getField(2)->getMax() == 10.0);

    row->clearRange();
    CHECK_FALSE(row->getField(0)->hasRange());
    scene.frame();
}

TEST_CASE("VectorField: the tags draw their axis's surface, the disabled one while disabled, the row nothing")
{
    Scene scene;
    VectorField *row=scene.row(4, 400, true);
    const tokyonight::Surface *axes[]=
    {
        &tokyonight::VectorTagX, &tokyonight::VectorTagY, &tokyonight::VectorTagZ, &tokyonight::VectorTagW
    };

    // the row itself draws nothing: no fill in its own slots
    REQUIRE(row->getSkinControl() != nullptr);
    for(size_t id : row->getSkinControl()->getPrimitives())
        CHECK(scene.renderer.fills.count(id) == 0);
    for(int i=0; i < 4; ++i)
    {
        CAPTURE(i);
        controls::internal::VectorFieldTag *tag=row->getTag(i);
        REQUIRE(tag->getSkinControl() != nullptr);
        const std::vector<size_t> &ids=tag->getSkinControl()->getPrimitives();
        REQUIRE(ids.size() == 1);
        CHECK(sameColor(scene.renderer.fills[ids[0]], axes[i]->top));
        CHECK(sameColor(tag->getTextColor(), scene.skin.Colors.AxisTagText));
    }

    row->setDisabled(true);
    scene.frame();
    for(int i=0; i < 4; ++i)
    {
        CAPTURE(i);
        controls::internal::VectorFieldTag *tag=row->getTag(i);
        CHECK(tag->isDisabled());
        CHECK(row->getField(i)->isDisabled());
        CHECK(sameColor(scene.renderer.fills[tag->getSkinControl()->getPrimitives()[0]],
            tokyonight::DragNumberDisabled.top));
        CHECK(sameColor(tag->getTextColor(), scene.skin.Colors.Button.Disabled));
    }

    // a component added while disabled is disabled too; a settled frame draws nothing
    row->setComponentCount(2);
    row->setComponentCount(3);
    CHECK(row->getField(2)->isDisabled());
    CHECK(row->getTag(2)->isDisabled());
    scene.frame();
    scene.renderer.draws=0;
    scene.frame();
    CHECK(scene.renderer.draws == 0);
}
