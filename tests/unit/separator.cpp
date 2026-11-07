/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Separator, Bullet and MenuDivider on a real Canvas, drawn by the textured
 *  skin's Separator and Bullet renderers on the StubRenderer, which records the
 *  rects they draw: a rule is one unit across and as long as its dock makes it,
 *  drawn as one rule icon (or two around a caption) one unit past it on both
 *  sides across; a caption of another width marks the separator Background and
 *  its Text, and moves the rule after it; MenuDivider draws through the
 *  Separator renderer; a bullet's circle sits in its column, centred on its line,
 *  in the text colour; and a settled frame draws nothing.
 *
 *  A docked rule is one icon of kind 6, its second slot hidden. A caption
 *  interrupts the rule captionGap() clear on both sides; a longer caption
 *  marks the separator StateChange_Background (its Text StateChange_Text), and
 *  an empty one gives the single rule back; a placed separator's caption is
 *  offset by its position. A Bullet's column is its font size (a larger font
 *  widens it), its circle drawn at its parent's coordinates and redrawn by
 *  setTextColor alone.
 */
#include "headlessFixture.h"

#include <gweni/controls/bullet.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/separator.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

bool sameRect(const Rect &a, const Rect &b)
{
    return a.x == b.x && a.y == b.y && a.w == b.w && a.h == b.h;
}

//! Records what the Separator and Bullet renderers draw.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        Rect rect;
        int kind;       //!< drawIcon's kind, -1 for drawRoundedRect
        int direction;
        Color color;
    };

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color, int) override
    {
        draws.push_back({primitiveId, rect, kind, direction, color});
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color fill, Color, Color, int) override
    {
        draws.push_back({primitiveId, rect, -1, 0, fill});
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hidden.push_back(primitiveId);
    }

    //! The draws into one slot this frame.
    std::vector<Draw> drawsInto(size_t id) const
    {
        std::vector<Draw> found;

        for(const Draw &draw : draws)
        {
            if(draw.id == id)
                found.push_back(draw);
        }
        return found;
    }

    bool hid(size_t id) const
    {
        for(size_t hiddenId : hidden)
        {
            if(hiddenId == id)
                return true;
        }
        return false;
    }

    std::vector<Draw> draws;
    std::vector<size_t> hidden;
};

//! The textured skin's Separator and Bullet renderers, a StubControl for the
//! rest. MenuDivider gets nothing, so it resolves to Separator through its
//! parent types as it does in the textured skin.
class SeparatorSkin: public test::StubSkin
{
public:
    explicit SeparatorSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Separator")
            return std::make_unique<skin::textured::Separator>();
        if(typeName == "Bullet")
            return std::make_unique<skin::textured::Bullet>();
        if(typeName == "MenuDivider")
            return nullptr;
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(300, 200);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        delete canvas;
    }

    void frame()
    {
        renderer.draws.clear();
        renderer.hidden.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    static const std::vector<size_t> &ids(controls::Base *control)
    {
        return control->getSkinControl()->getPrimitives();
    }

    DrawRecorder renderer;
    SeparatorSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
};

}//namespace

TEST_CASE("Separator: one unit across, the length its dock gives it, drawn one unit past it across")
{
    Scene scene;
    controls::Separator *horizontal=scene.canvas->newChild<controls::Separator>();
    controls::Separator *vertical=scene.canvas->newChild<controls::Separator>();

    horizontal->setDock(DockPosition::Top);
    vertical->setVertical();
    vertical->setDock(DockPosition::Left);
    scene.frame();

    CHECK(sameRect(horizontal->getBounds(), Rect(0, 0, 300, 1)));
    CHECK(sameRect(vertical->getBounds(), Rect(0, 1, 1, 199)));

    const std::vector<size_t> &h=Scene::ids(horizontal);
    const std::vector<size_t> &v=Scene::ids(vertical);

    REQUIRE(h.size() == 2);
    REQUIRE(v.size() == 2);

    const std::vector<DrawRecorder::Draw> rule=scene.renderer.drawsInto(h[0]);

    REQUIRE(rule.size() == 1);
    CHECK(rule[0].kind == 6);
    CHECK(rule[0].direction == 0);
    CHECK(sameRect(rule[0].rect, Rect(0, -1, 300, 3)));
    CHECK(scene.renderer.drawsInto(h[1]).empty());
    CHECK(scene.renderer.hid(h[1]));

    const std::vector<DrawRecorder::Draw> column=scene.renderer.drawsInto(v[0]);

    REQUIRE(column.size() == 1);
    CHECK(column[0].kind == 6);
    CHECK(column[0].direction == 1);
    CHECK(sameRect(column[0].rect, Rect(-1, 1, 3, 199)));
    CHECK(scene.renderer.hid(v[1]));

    // settled: nothing is drawn or hidden
    scene.frame();
    CHECK(scene.renderer.draws.empty());
    CHECK(scene.renderer.hidden.empty());
}

TEST_CASE("Separator: a caption interrupts the rule; a caption of another width moves the rule after it")
{
    Scene scene;
    controls::Separator *separator=scene.canvas->newChild<controls::Separator>();

    separator->setDock(DockPosition::Top);
    separator->setText("Section");
    scene.frame();

    REQUIRE(separator->captionShown());

    controls::Label *caption=separator->getCaption();
    const Rect captionBounds=caption->getBounds();

    // inset from the left, and the separator as high as the caption
    CHECK(captionBounds.x == separator->captionInset());
    CHECK(captionBounds.y == 0);
    CHECK(separator->getBounds().h == captionBounds.h);
    CHECK(separator->getBounds().w == 300);

    const std::vector<size_t> &ids=Scene::ids(separator);
    const int gap=separator->captionGap();
    const int h=captionBounds.h+2;

    {
        const std::vector<DrawRecorder::Draw> before=scene.renderer.drawsInto(ids[0]);
        const std::vector<DrawRecorder::Draw> after=scene.renderer.drawsInto(ids[1]);
        const int rightStart=captionBounds.x+captionBounds.w+gap;

        REQUIRE(before.size() == 1);
        REQUIRE(after.size() == 1);
        CHECK(sameRect(before[0].rect, Rect(0, -1, captionBounds.x-gap, h)));
        CHECK(sameRect(after[0].rect, Rect(rightStart, -1, 300-rightStart, h)));
    }

    // a longer caption: the Text gets the string, the separator only a redraw
    separator->setText("A longer section");
    scene.canvas->updateCanvas();

    const Rect longer=caption->getBounds();

    CHECK(longer.w > captionBounds.w);
    CHECK((separator->getStateChange()&controls::StateChange_Background) != 0);
    CHECK((separator->getStateChange()&controls::StateChange_Text) == 0);

    controls::Base *text=caption->getChildren().front();

    CHECK((text->getStateChange()&controls::StateChange_Text) != 0);

    scene.renderer.draws.clear();
    scene.renderer.hidden.clear();
    scene.canvas->renderCanvas();

    {
        const std::vector<DrawRecorder::Draw> after=scene.renderer.drawsInto(ids[1]);
        const int rightStart=longer.x+longer.w+gap;

        REQUIRE(after.size() == 1);
        CHECK(sameRect(after[0].rect, Rect(rightStart, -1, 300-rightStart, h)));
    }

    scene.frame();
    CHECK(scene.renderer.draws.empty());

    // no caption: one rule again, the second slot hidden
    separator->setText("");
    scene.frame();
    CHECK_FALSE(separator->captionShown());
    CHECK(separator->getBounds().h == 1);
    REQUIRE(scene.renderer.drawsInto(ids[0]).size() == 1);
    CHECK(sameRect(scene.renderer.drawsInto(ids[0])[0].rect, Rect(0, -1, 300, 3)));
    CHECK(scene.renderer.hid(ids[1]));
}

TEST_CASE("Separator: a caption inside a placed separator is offset by its position")
{
    Scene scene;
    controls::Base *panel=scene.canvas->newChild<controls::Base>();

    panel->setBounds(20, 30, 200, 100);

    controls::Separator *separator=panel->newChild<controls::Separator>();

    separator->setPos(5, 7);
    separator->setWidth(150);
    separator->setText("Cap");
    scene.frame();

    const Rect bounds=separator->getBounds();
    const Rect caption=separator->getCaption()->getBounds();
    const std::vector<size_t> &ids=Scene::ids(separator);
    const int gap=separator->captionGap();

    CHECK(bounds.x == 5);
    CHECK(bounds.w == 150);

    // the render offset is the panel's origin, so the rule is in its coordinates
    const std::vector<DrawRecorder::Draw> before=scene.renderer.drawsInto(ids[0]);
    const std::vector<DrawRecorder::Draw> after=scene.renderer.drawsInto(ids[1]);
    const int rightStart=bounds.x+caption.x+caption.w+gap;

    REQUIRE(before.size() == 1);
    REQUIRE(after.size() == 1);
    CHECK(sameRect(before[0].rect, Rect(5, 6, caption.x-gap, bounds.h+2)));
    CHECK(sameRect(after[0].rect, Rect(rightStart, 6, bounds.x+bounds.w-rightStart, bounds.h+2)));
}

TEST_CASE("MenuDivider draws through the Separator renderer, one unit high")
{
    Scene scene;
    controls::MenuDivider *divider=scene.canvas->newChild<controls::MenuDivider>();

    divider->setDock(DockPosition::Top);
    scene.frame();

    REQUIRE(divider->getSkinControl() != nullptr);
    CHECK(const_cast<skin::Control *>(divider->getSkinControl())->getTypeName() == "Separator");
    CHECK(sameRect(divider->getBounds(), Rect(0, 0, 300, 1)));

    const std::vector<DrawRecorder::Draw> rule=scene.renderer.drawsInto(Scene::ids(divider)[0]);

    REQUIRE(rule.size() == 1);
    CHECK(rule[0].kind == 6);
}

TEST_CASE("Bullet: the circle sits in its column, centred on the text's line, in the text colour")
{
    Scene scene;
    controls::Bullet *first=scene.canvas->newChild<controls::Bullet>();
    controls::Bullet *second=scene.canvas->newChild<controls::Bullet>();

    first->setText("Point");
    second->setText("A much longer point");
    first->setPos(10, 20);
    second->setPos(10, 40);
    scene.frame();

    // the default font is 10 units: a 10 + gap column, a 4 unit circle
    const int fontSize=static_cast<int>(first->getFont().size+0.5f);
    const int column=fontSize+first->columnGap();

    CHECK(first->textX() == column);
    CHECK(second->textX() == column);
    CHECK(first->getBounds().w == column+first->textWidth());

    const Rect circle=first->bulletRect();

    CHECK(circle.w == 4);
    CHECK(circle.h == 4);
    CHECK(circle.x == (fontSize-4)/2);
    CHECK(circle.y == first->textY()+(first->textHeight()-4)/2);
    CHECK(sameRect(second->bulletRect(), circle));

    // drawn in the bullet's parent's coordinates
    const std::vector<DrawRecorder::Draw> drawn=scene.renderer.drawsInto(Scene::ids(first)[0]);

    REQUIRE(drawn.size() == 1);
    CHECK(sameRect(drawn[0].rect, Rect(10+circle.x, 20+circle.y, 4, 4)));

    // a new text colour redraws the circle in it
    const Color red(255, 0, 0, 255);

    first->setTextColor(red);
    scene.canvas->updateCanvas();
    CHECK((first->getStateChange()&controls::StateChange_Background) != 0);
    scene.renderer.draws.clear();
    scene.canvas->renderCanvas();

    const std::vector<DrawRecorder::Draw> recoloured=scene.renderer.drawsInto(Scene::ids(first)[0]);

    REQUIRE(recoloured.size() == 1);
    CHECK(recoloured[0].color.r == 255);
    CHECK(recoloured[0].color.g == 0);
    CHECK(scene.renderer.drawsInto(Scene::ids(second)[0]).empty());

    scene.frame();
    CHECK(scene.renderer.draws.empty());

    // a larger font widens the column (the label keeps the font it creates)
    first->setFont(first->getFont().facename, 20, false);
    scene.frame();
    CHECK(first->textX() == 20+first->columnGap());
    CHECK(first->bulletRect().w == 8);
}
