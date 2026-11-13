/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Badge on a real Canvas with the StubRenderer, drawn by the
 *  textured skin's Badge renderer (every other type a StubControl), its
 *  anchors Buttons in a plain panel:
 *  - made on its anchor it is attached to the top-right corner, hidden at a
 *    count of 0; 9, 10 and 100 read "9", "10" and "99+" (a cap of 9: "9+"), 0
 *    hides it again unless setShowZero(), and setDot() shows a plain dot of
 *    DotSize;
 *  - it is centred on each corner of its anchor, and follows the anchor's
 *    resize; a pure move of the anchor enters neither the badge nor the
 *    anchor's Text;
 *  - neither it nor its count is in the anchor's preferred size; a count of
 *    the same width (3 to 4) lays nothing out and writes the badge's bubble
 *    and its run alone, and a wider one (9 to 10) lays out the way down to
 *    the badge in one pass, entering no sibling, and writes the same;
 *  - its bubble and its run are drawn at its getAttachedZIndex(), the z the
 *    anchor's parent reserves: in front of the next Button and that Button's
 *    Text, behind an open Popup and the ToolTip box;
 *  - it hides and shows with its anchor, and is deleted with it, its
 *    primitive and text ids back;
 *  - a click on its overhang reaches the control under it, on its part
 *    inside the anchor the anchor.
 */
#include "headlessFixture.h"

#include <gweni/controls/badge.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/toolTip.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>
#include <string>

using namespace gweni;

namespace
{

//! Records the z of every rounded rect and text run drawn.
class ZRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        rectZ[primitiveId]=zIndex;
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void renderText(size_t textId, const Font &font, Point pos, int zIndex, const String &text) override
    {
        textZ[textId]=zIndex;
        texts[textId]=text;
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }

    std::map<size_t, int> rectZ;
    std::map<size_t, int> textZ;
    std::map<size_t, String> texts;
};

//! The textured Badge renderer; a StubControl for every other type.
class BadgeSkin: public test::StubSkin
{
public:
    explicit BadgeSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Badge")
            return std::make_unique<skin::textured::Badge>();
        return StubSkin::getControlRenderer(typeName);
    }
};

//! A Badge that counts its layout entries.
class CountingBadge: public controls::Badge
{
public:
    GWENI_CONTROL_INLINE(CountingBadge, controls::Badge)
    {}

    int entries=0;

    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::calculateSize(skin, dim);
    }

protected:
    void arrange(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::arrange(skin, dim);
    }
};

struct Scene
{
    ZRecorder renderer;
    BadgeSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    double clock=0.0;

    Scene()
    {
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        panel=canvas->newChild<controls::Base>();
        panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        panel->setBounds(0, 0, 400, 200);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        input::onMouseClicked(canvas, 0, false);
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
        platform::setTimeSource(nullptr);
    }

    //! A Button in the panel at \p bounds, its size fixed.
    controls::Button *button(const Rect &bounds, const String &text="Inbox")
    {
        controls::Button *button=panel->newChild<controls::Button>();

        button->setText(text);
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setBounds(bounds);
        return button;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
            frame();
    }

    //! The ids the control's skin Control holds.
    static void ids(const controls::Base *control, std::vector<size_t> &primitives, std::vector<size_t> &texts)
    {
        primitives.clear();
        texts.clear();
        if(const skin::Control *drawn=control->getSkinControl())
            drawn->appendIds(primitives, texts);
    }
};

//! "x,y,w,h", so a failed check shows both rects
std::string str(const Rect &rect)
{
    return std::to_string(rect.x)+","+std::to_string(rect.y)+","
        +std::to_string(rect.w)+","+std::to_string(rect.h);
}

//! Each written primitive id, and each text id with glyphs written.
std::vector<size_t> writtenTexts(const test::StubRenderer &renderer)
{
    std::vector<size_t> ids;

    for(const renderer::WrittenText &written : renderer.writtenTextIds())
        ids.push_back(written.textId);
    return ids;
}

}//namespace

TEST_CASE("Badge: the count, the cap, show-zero and the dot")
{
    Scene scene;
    controls::Button *anchor=scene.button(Rect(20, 20, 80, 24));
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    scene.settle();
    CHECK(badge->isAttached());
    CHECK(badge->getParent() == anchor);
    CHECK((badge->getAttachedCorner() == Corner::TopRight));
    CHECK(badge->hidden());
    CHECK(badge->getText().empty());

    // the stub measures 4.4 units a character, 11 high: a digit is a circle
    // of the line's height, a longer number its text and 4 units each side
    badge->setCount(9);
    scene.settle();
    CHECK_FALSE(badge->hidden());
    CHECK(badge->getText() == "9");
    CHECK(badge->getWidth() == 12);
    CHECK(badge->getHeight() == 11);

    badge->setCount(10);
    scene.settle();
    CHECK(badge->getText() == "10");
    CHECK(badge->getWidth() == 16);

    badge->setCount(100);
    scene.settle();
    CHECK(badge->getText() == "99+");
    CHECK(badge->getWidth() == 21);

    badge->setCap(9);
    CHECK(badge->getText() == "9+");

    badge->setCount(0);
    scene.settle();
    CHECK(badge->hidden());

    badge->setShowZero(true);
    scene.settle();
    CHECK_FALSE(badge->hidden());
    CHECK(badge->getText() == "0");

    badge->setShowZero(false);
    badge->setDot(true);
    scene.settle();
    CHECK_FALSE(badge->hidden());
    CHECK(badge->getText().empty());
    CHECK(badge->getWidth() == controls::Badge::DotSize);
    CHECK(badge->getHeight() == controls::Badge::DotSize);
    // a dot is centred on the corner too
    CHECK(badge->getX() == 80-controls::Badge::DotSize/2);
}

TEST_CASE("Badge: centred on each corner of its anchor, following its resize, and a pure move enters neither it nor the anchor's Text")
{
    Scene scene;
    controls::Button *anchor=scene.button(Rect(50, 40, 80, 30));
    const Corner corners[]={Corner::TopLeft, Corner::TopRight, Corner::BottomLeft, Corner::BottomRight};
    std::vector<CountingBadge *> badges;

    for(Corner corner : corners)
    {
        CountingBadge *badge=anchor->newChild<CountingBadge>();

        badge->attach(anchor, corner);
        badge->setCount(5);
        badges.push_back(badge);
    }
    scene.settle();

    // 12 x 11, centred: the extra unit of 11 below the corner
    CHECK(str(badges[0]->getBounds()) == str(Rect(-6, -5, 12, 11)));
    CHECK(str(badges[1]->getBounds()) == str(Rect(74, -5, 12, 11)));
    CHECK(str(badges[2]->getBounds()) == str(Rect(-6, 25, 12, 11)));
    CHECK(str(badges[3]->getBounds()) == str(Rect(74, 25, 12, 11)));

    anchor->setSize(100, 40);
    scene.settle();
    CHECK(str(badges[1]->getBounds()) == str(Rect(94, -5, 12, 11)));
    CHECK(str(badges[3]->getBounds()) == str(Rect(94, 35, 12, 11)));

    for(CountingBadge *badge : badges)
        badge->entries=0;
    anchor->setPos(60, 50);
    scene.frame();

    CHECK(anchor->getX() == 60);
    CHECK(str(badges[3]->getBounds()) == str(Rect(94, 35, 12, 11)));
    for(CountingBadge *badge : badges)
        CHECK(badge->entries == 0);
    // the canvas, the panel and the moved anchor, measured and arranged in X
    // and Y: neither its Text nor a badge
    CHECK(scene.canvas->getLayoutStats().visited == 3*4);
}

TEST_CASE("Badge: not in its anchor's size, and a count change writes the badge's slots alone and lays out the badge only when its width changes")
{
    Scene scene;
    controls::Button *anchor=scene.panel->newChild<controls::Button>();
    controls::Button *sibling=scene.button(Rect(200, 20, 80, 24), "Other");

    anchor->setText("Inbox");
    anchor->setPos(20, 20);
    scene.settle();

    const Size before=anchor->getPreferredSize();
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    badge->setCount(3);
    scene.settle();
    CHECK(anchor->getPreferredSize().width == before.width);
    CHECK(anchor->getPreferredSize().height == before.height);

    std::vector<size_t> primitives, texts;

    Scene::ids(badge, primitives, texts);
    REQUIRE(primitives.size() == 1);
    REQUIRE(texts.size() == 1);

    SUBCASE("3 to 4: the same width, nothing laid out")
    {
        badge->setCount(4);
        scene.frame();

        CHECK(scene.canvas->getLayoutStats().passes == 0);
        CHECK(scene.renderer.writtenPrimitiveIds() == primitives);
        CHECK(writtenTexts(scene.renderer) == texts);
        CHECK(scene.renderer.texts[texts[0]] == "4");
    }

    SUBCASE("9 to 10 to 100: wider, the way down to the badge laid out, no sibling entered")
    {
        badge->setCount(9);
        scene.settle();

        for(int count : {10, 100})
        {
            const int width=badge->getWidth();

            badge->setCount(count);
            scene.frame();

            CHECK(badge->getWidth() > width);
            CHECK(scene.canvas->getLayoutStats().passes == 1);
            // the canvas, the panel, the anchor and the badge, each X and Y
            CHECK(scene.canvas->getLayoutStats().visited == 4*4);
            CHECK(scene.renderer.writtenPrimitiveIds() == primitives);
            CHECK(writtenTexts(scene.renderer) == texts);
            CHECK(anchor->getPreferredSize().width == before.width);
            CHECK(anchor->getWidth() == before.width);
            CHECK(sibling->getX() == 200);
        }
        CHECK(scene.renderer.texts[texts[0]] == "99+");
    }
}

TEST_CASE("Badge: drawn in front of the next sibling and its Text, behind an open Popup and the ToolTip box")
{
    Scene scene;
    controls::Button *anchor=scene.button(Rect(20, 20, 80, 24));
    controls::Button *next=scene.button(Rect(100, 20, 80, 24), "Next");
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    next->setToolTipText("tip");
    badge->setCount(7);
    scene.settle();

    std::vector<size_t> primitives, texts;

    Scene::ids(badge, primitives, texts);
    REQUIRE(primitives.size() == 1);
    REQUIRE(texts.size() == 1);

    const int z=badge->getAttachedZIndex();

    CHECK(z == scene.panel->getOverlayZIndex());
    CHECK(scene.renderer.rectZ[primitives[0]] == z);
    CHECK(scene.renderer.textZ[texts[0]] == z);
    CHECK(z > next->getZIndex());
    for(controls::Base *child : next->getChildren())
        CHECK(z > child->getZIndex());

    // a popup, opened on the canvas, is after the panel's whole range
    controls::Popup *popup=next->createPopup();

    popup->setSize(60, 40);
    popup->open(next);
    scene.settle();
    REQUIRE(popup->visible());
    CHECK(badge->getAttachedZIndex() < popup->getZIndex());

    popup->close();
    scene.settle();

    // the ToolTip box is the canvas's last child
    const Point at=next->localPosToCanvas(Point(40, 12));

    scene.canvas->inputMouseMoved(at.x, at.y, 0, 0);
    scene.frame();
    scene.clock+=scene.canvas->getToolTipDelay()+0.1;
    scene.settle();

    controls::ToolTip *box=scene.canvas->toolTipControl();

    REQUIRE(box != nullptr);
    REQUIRE(scene.canvas->toolTipShown());
    CHECK(badge->getAttachedZIndex() < box->getZIndex());
}

TEST_CASE("Badge: hidden and shown with its anchor, deleted with it")
{
    Scene scene;
    controls::Button *anchor=scene.button(Rect(20, 20, 80, 24));

    scene.settle();

    const size_t livePrimitives=scene.renderer.livePrimitives.size();
    const size_t liveTexts=scene.renderer.liveTexts.size();
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    badge->setCount(12);
    scene.settle();
    REQUIRE(badge->visible());
    CHECK(scene.renderer.livePrimitives.size() == livePrimitives+1);
    CHECK(scene.renderer.liveTexts.size() == liveTexts+1);

    anchor->hide();
    scene.frame();
    CHECK_FALSE(badge->visible());
    CHECK_FALSE(badge->hidden());

    anchor->show();
    scene.frame();
    CHECK(badge->visible());

    // the ids of the anchor and the Text it holds, held before the badge
    size_t anchorPrimitives=0;
    size_t anchorTexts=0;
    std::vector<size_t> primitives, texts;

    Scene::ids(anchor, primitives, texts);
    anchorPrimitives+=primitives.size();
    anchorTexts+=texts.size();
    for(controls::Base *child : anchor->getChildren())
    {
        if(child == badge)
            continue;
        Scene::ids(child, primitives, texts);
        anchorPrimitives+=primitives.size();
        anchorTexts+=texts.size();
    }

    delete anchor;
    scene.settle();
    // the badge's back with them
    CHECK(scene.renderer.livePrimitives.size() == livePrimitives-anchorPrimitives);
    CHECK(scene.renderer.liveTexts.size() == liveTexts-anchorTexts);
    CHECK(scene.renderer.doubleReleased == 0);
}

TEST_CASE("Badge: a click on its overhang reaches the control under it, on its inner part the anchor")
{
    Scene scene;
    controls::Button *anchor=scene.button(Rect(20, 20, 80, 24));
    controls::Button *next=scene.button(Rect(100, 20, 80, 24), "Next");
    controls::Badge *badge=anchor->newChild<controls::Badge>();

    badge->setCount(42);
    scene.settle();
    REQUIRE(badge->getX()+badge->getWidth() > 80);

    const Rect bounds=badge->getBounds();
    // on the canvas: the anchor's origin added
    const int inside=20+bounds.x+1;
    const int overhangX=20+bounds.x+bounds.w-2;
    const int y=20+2;

    CHECK(scene.canvas->getControlAt(inside, y) == anchor);
    CHECK(scene.canvas->getControlAt(overhangX, y) == next);
    // above the anchor, on nothing but the panel
    CHECK(scene.canvas->getControlAt(inside, 20+bounds.y+1) == scene.panel);
}
