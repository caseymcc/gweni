/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's ColorLerpBox and ColorSlider renderers: the SV box is one
 *  bilinear fill and a marker, the hue strip six gradient segments and a marker,
 *  and both repaint only when what they draw has changed.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorControls.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <map>

using namespace gweni;

namespace
{

struct Draw
{
    Rect rect;
    int zIndex;
    Color c00, c10, c01, c11;   //!< corners; a rounded rect's top in c00/c10, bottom in c01/c11
    bool bilinear;
};

//! Records the last draw into each slot this frame.
class DrawRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float, float, Color top, Color bottom,
        Color, int zIndex) override
    {
        drawn[primitiveId]=Draw{rect, zIndex, top, top, bottom, bottom, false};
    }

    void drawBilinearRect(size_t primitiveId, Rect rect, int zIndex, Color c00, Color c10,
        Color c01, Color c11, float, float, Color) override
    {
        drawn[primitiveId]=Draw{rect, zIndex, c00, c10, c01, c11, true};
    }

    std::map<size_t, Draw> drawn;
};

class ColorSkin: public test::StubSkin
{
public:
    explicit ColorSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "ColorLerpBox")
            return std::make_unique<skin::textured::ColorLerpBox>();
        if(typeName == "ColorSlider")
            return std::make_unique<skin::textured::ColorSlider>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct ColorScene
{
    ColorScene()
    {
        canvas=new controls::Canvas(&skin);
        box=canvas->newChild<controls::ColorLerpBox>();
        slider=canvas->newChild<controls::ColorSlider>();
        slider->setPos(140, 0);
    }

    ColorScene(const ColorScene &)=delete;
    ColorScene &operator=(const ColorScene &)=delete;

    ~ColorScene()
    {
        delete canvas;
    }

    void frame()
    {
        renderer.drawn.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const Draw &drawOf(size_t id) const
    {
        static const Draw none{};
        auto it=renderer.drawn.find(id);
        return it == renderer.drawn.end()?none:it->second;
    }

    //! The control's slots, in the order the renderer draws them.
    static std::vector<size_t> sortedIds(controls::Base *control)
    {
        std::vector<size_t> ids=control->getSkinControl()->getPrimitives();
        std::sort(ids.begin(), ids.end());
        return ids;
    }

    //! A press at local (x, y), a drag to local (toX, toY), a release there.
    static void drag(controls::Base *control, int x, int y, int toX, int toY)
    {
        const Point from=control->localPosToCanvas(Point(x, y));
        const Point to=control->localPosToCanvas(Point(toX, toY));

        control->onMouseClickLeft(from.x, from.y, true);
        control->onMouseMoved(to.x, to.y, 0, 0);
        control->onMouseClickLeft(to.x, to.y, false);
    }

    DrawRecorder renderer;
    ColorSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::ColorLerpBox *box=nullptr;
    controls::ColorSlider *slider=nullptr;
};

}//namespace

// gweni::Rect and gweni::Color have no doctest stringifier (gweni::toString is
// ambiguous with doctest's), so their comparisons are wrapped in parentheses.

TEST_CASE("the SV box draws one bilinear field and a marker centred on its cursor")
{
    ColorScene scene;

    scene.frame();
    REQUIRE(scene.box->getSkinControl() != nullptr);

    const std::vector<size_t> ids=ColorScene::sortedIds(scene.box);
    REQUIRE(ids.size() == 2);

    const Draw &field=scene.drawOf(ids[0]);
    CHECK(field.bilinear);
    CHECK((field.rect == Rect(0, 0, 128, 128)));
    CHECK(field.zIndex == scene.box->getZIndex());
    CHECK((field.c00 == Color(255, 255, 255, 255)));
    CHECK((field.c10 == scene.box->getHueColor()));
    CHECK((field.c01 == Color(0, 0, 0, 255)));
    CHECK((field.c11 == Color(0, 0, 0, 255)));

    // the cursor starts in the corner: the marker is clamped inside the box
    const int size=skin::textured::ColorLerpBox::MarkerSize;
    CHECK((scene.drawOf(ids[1]).rect == Rect(0, 0, size, size)));
    CHECK(scene.drawOf(ids[1]).zIndex == scene.box->getZIndex()+1);

    // dragged to (96, 32): only the marker moves
    ColorScene::drag(scene.box, 10, 10, 96, 32);
    scene.frame();
    CHECK((scene.drawOf(ids[1]).rect == Rect(96-size/2, 32-size/2, size, size)));
    CHECK((scene.drawOf(ids[0]).rect == field.rect));

    // a new hue moves the top-right corner
    scene.box->setColor(Color(0, 255, 255, 255));
    scene.frame();
    CHECK((scene.drawOf(ids[0]).c10 == Color(0, 255, 255, 255)));
}

TEST_CASE("the hue strip's six segments cover every row, behind the marker")
{
    ColorScene scene;

    scene.frame();
    REQUIRE(scene.slider->getSkinControl() != nullptr);

    const std::vector<size_t> ids=ColorScene::sortedIds(scene.slider);
    REQUIRE(ids.size() == 7);

    const int z=scene.slider->getZIndex();
    const int height=scene.slider->getHeight();
    std::vector<int> cover(height, 0);
    std::vector<int> topZ(height, -1);

    for(size_t i=0; i<6; ++i)
    {
        const Draw &segment=scene.drawOf(ids[i]);

        CHECK_FALSE(segment.bilinear);
        // OpenGLCore draws 2*(h/2) rows, so an odd height would drop a row
        CHECK(segment.rect.h%2 == 0);
        CHECK(segment.rect.x == skin::textured::ColorSlider::Inset);
        // the three in front come after the three behind, in slot order
        CHECK(segment.zIndex == (i < 3?z:z+1));

        for(int y=segment.rect.y; y<segment.rect.y+segment.rect.h; ++y)
        {
            REQUIRE(y >= 0);
            REQUIRE(y < height);
            ++cover[y];
            topZ[y]=std::max(topZ[y], segment.zIndex);
        }
    }

    // every row drawn, and where two overlap the front one is on top
    for(int y=0; y<height; ++y)
    {
        CHECK(cover[y] >= 1);
        CHECK(cover[y] <= 2);
        if(cover[y] == 2)
            CHECK(topZ[y] == z+1);
    }

    // segment i covers hue arc i, rows i*h/6..(i+1)*h/6, and reaches at most
    // two rows into its neighbours; each stop is the strip's colour at its edge
    std::vector<Draw> segments;
    for(size_t i=0; i<6; ++i)
        segments.push_back(scene.drawOf(ids[i]));
    std::sort(segments.begin(), segments.end(),
        [](const Draw &a, const Draw &b) { return a.rect.y < b.rect.y; });

    for(int i=0; i<6; ++i)
    {
        const Rect &rect=segments[i].rect;

        CHECK(rect.y <= i*height/6);
        CHECK(rect.y >= i*height/6-2);
        CHECK(rect.y+rect.h >= (i+1)*height/6);
        CHECK(rect.y+rect.h <= (i+1)*height/6+2);
        CHECK((segments[i].c00 == scene.slider->getColorAtHeight(rect.y)));
        CHECK((segments[i].c01 == scene.slider->getColorAtHeight(rect.y+rect.h)));
    }
    CHECK((segments[0].c00 == Color(255, 0, 0, 255)));
    CHECK((segments[5].c01 == Color(255, 0, 0, 255)));

    // the marker, last and in front, follows a drag
    const int markerHeight=skin::textured::ColorSlider::MarkerHeight;
    CHECK(scene.drawOf(ids[6]).zIndex == z+2);
    CHECK((scene.drawOf(ids[6]).rect == Rect(0, 0, scene.slider->getWidth(), markerHeight)));

    ColorScene::drag(scene.slider, 16, 10, 16, 64);
    scene.frame();
    CHECK(scene.slider->getSelectedDist() == 64);
    CHECK((scene.drawOf(ids[6]).rect == Rect(0, 64-markerHeight/2, scene.slider->getWidth(), markerHeight)));
}

TEST_CASE("the SV box and hue strip repaint only when what they draw changes")
{
    ColorScene scene;

    scene.frame();
    scene.frame();
    CHECK(scene.renderer.drawn.empty());

    // hovering without a press, and setting the colour they already show
    const Point over=scene.box->localPosToCanvas(Point(50, 50));
    scene.box->onMouseMoved(over.x, over.y, 0, 0);
    scene.slider->onMouseMoved(over.x, over.y, 0, 0);
    scene.box->setColor(Color(255, 128, 0, 255));
    scene.slider->setColor(Color(255, 0, 0, 255));
    scene.frame();
    CHECK(scene.renderer.drawn.empty());

    // a drag repaints the SV box only
    ColorScene::drag(scene.box, 50, 50, 60, 70);
    scene.frame();
    CHECK(scene.renderer.drawn.size() == 2);
    for(size_t id : scene.box->getSkinControl()->getPrimitives())
        CHECK(scene.renderer.drawn.count(id) == 1);

    // a new hue repaints the strip
    scene.slider->setColor(Color(0, 255, 255, 255));
    scene.frame();
    CHECK(scene.renderer.drawn.size() == 7);
}
