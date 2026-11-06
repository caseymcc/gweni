/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's ColorDisplay renderer: a swatch is one primitive, a flat
 *  fill when the colour is opaque (or checkers are off) and a checker composite
 *  when it is translucent, and it repaints only when its value changes. Also
 *  checks that ColorPicker's setters reach its swatches.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorPicker.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <map>

using namespace gweni;

namespace
{

struct Draw
{
    Rect rect;
    Color fill;
    Color light, dark;      //!< the checker cells; unset for a flat fill
    Color border;
    float radius;
    float borderWidth;
    bool checker;
};

//! Records the last draw into each slot, and the last style shown on it.
class DrawRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color, Color borderColor, int) override
    {
        drawn[primitiveId]=Draw{rect, top, Color(), Color(), borderColor, radius, border, false};
    }

    void drawCheckerRect(size_t primitiveId, Rect rect, int, Color fill, Color light, Color dark,
        float radius, float border, Color borderColor) override
    {
        drawn[primitiveId]=Draw{rect, fill, light, dark, borderColor, radius, border, true};
    }

    void showPrimitive(int style, size_t primitiveId) override { shown[primitiveId]=style; }
    void hidePrimitive(size_t primitiveId) override { shown[primitiveId]=0; }

    std::map<size_t, Draw> drawn;
    std::map<size_t, int> shown;
};

class SwatchSkin: public test::StubSkin
{
public:
    explicit SwatchSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "ColorDisplay")
            return std::make_unique<skin::textured::ColorDisplay>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct SwatchScene
{
    SwatchScene()
    {
        canvas=new controls::Canvas(&skin);
        swatch=canvas->newChild<controls::internal::ColorDisplay>();
        swatch->setBounds(4, 4, 32, 24);
    }

    SwatchScene(const SwatchScene &)=delete;
    SwatchScene &operator=(const SwatchScene &)=delete;

    ~SwatchScene()
    {
        delete canvas;
    }

    void frame()
    {
        renderer.drawn.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    size_t id() const
    {
        const std::vector<size_t> &ids=swatch->getSkinControl()->getPrimitives();

        REQUIRE(ids.size() == 1);
        return ids[0];
    }

    bool drew() const { return renderer.drawn.count(id()) != 0; }
    const Draw &draw() const { return renderer.drawn.at(id()); }

    DrawRecorder renderer;
    SwatchSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::internal::ColorDisplay *swatch=nullptr;
};

}//namespace

// gweni::Rect and gweni::Color have no doctest stringifier (gweni::toString is
// ambiguous with doctest's), so their comparisons are wrapped in parentheses.

TEST_CASE("an opaque swatch is one flat fill in its colour with the ColorSwatch border")
{
    namespace tn=skin::textured::tokyonight;
    SwatchScene scene;

    scene.swatch->setColor(Color(10, 200, 30, 255));
    scene.frame();
    REQUIRE(scene.swatch->getSkinControl() != nullptr);
    REQUIRE(scene.drew());

    const Draw &draw=scene.draw();
    CHECK_FALSE(draw.checker);
    CHECK((draw.rect == scene.swatch->getRenderBounds()));
    CHECK((draw.rect == Rect(0, 0, 32, 24)));
    CHECK((draw.fill == Color(10, 200, 30, 255)));
    CHECK((draw.border == tn::ColorSwatch.border));
    CHECK(draw.radius == tn::ColorSwatch.radius);
    CHECK(draw.borderWidth == tn::ColorSwatch.borderWidth);
    CHECK(scene.renderer.shown[scene.id()] == 4);
}

TEST_CASE("a translucent swatch is one checker composite, and goes back to a flat fill at 255")
{
    namespace tn=skin::textured::tokyonight;
    SwatchScene scene;

    scene.frame();

    scene.swatch->setAlpha(128);
    scene.frame();
    REQUIRE(scene.drew());
    CHECK(scene.draw().checker);
    CHECK((scene.draw().fill == Color(255, 0, 0, 128)));
    CHECK((scene.draw().light == tn::ColorChecker.top));
    CHECK((scene.draw().dark == tn::ColorChecker.bottom));
    CHECK((scene.draw().border == tn::ColorSwatch.border));

    scene.swatch->setAlpha(255);
    scene.frame();
    REQUIRE(scene.drew());
    CHECK_FALSE(scene.draw().checker);

    // checkers off: translucent, but a plain fill
    scene.swatch->setDrawCheckers(false);
    scene.swatch->setAlpha(64);
    scene.frame();
    REQUIRE(scene.drew());
    CHECK_FALSE(scene.draw().checker);
    CHECK((scene.draw().fill == Color(255, 0, 0, 64)));
    CHECK_FALSE(scene.swatch->getDrawCheckers());
}

TEST_CASE("a swatch repaints only when its value changes")
{
    SwatchScene scene;

    scene.frame();
    scene.frame();
    CHECK_FALSE(scene.drew());

    // the same value through every setter: nothing to repaint
    scene.swatch->setColor(scene.swatch->getColor());
    scene.swatch->setRed(255);
    scene.swatch->setGreen(0);
    scene.swatch->setBlue(0);
    scene.swatch->setAlpha(255);
    scene.swatch->setDrawCheckers(true);
    scene.frame();
    CHECK_FALSE(scene.drew());

    // each channel setter repaints
    scene.swatch->setRed(1);
    scene.frame();
    CHECK(scene.drew());
    scene.swatch->setGreen(2);
    scene.frame();
    CHECK(scene.drew());
    scene.swatch->setBlue(3);
    scene.frame();
    CHECK(scene.drew());
    CHECK((scene.draw().fill == Color(1, 2, 3, 255)));

    // a checker toggle repaints only where it shows
    scene.swatch->setAlpha(100);
    scene.frame();
    CHECK(scene.draw().checker);
    scene.swatch->setDrawCheckers(false);
    scene.frame();
    REQUIRE(scene.drew());
    CHECK_FALSE(scene.draw().checker);
}

TEST_CASE("a translucent swatch shown again gets the checker style back, not style 4")
{
    SwatchScene scene;

    scene.swatch->setAlpha(100);
    scene.frame();
    CHECK(scene.renderer.shown[scene.id()] == 7);

    scene.swatch->setHidden(true);
    scene.frame();
    CHECK(scene.renderer.shown[scene.id()] == 0);

    scene.swatch->setHidden(false);
    scene.frame();
    CHECK(scene.renderer.shown[scene.id()] == 7);
}

TEST_CASE("ColorPicker's setters reach its channel swatches and Result")
{
    test::StubRenderer renderer;
    test::StubSkin skin(&renderer);
    controls::Canvas *canvas=new controls::Canvas(&skin);
    controls::ColorPicker *picker=canvas->newChild<controls::ColorPicker>();

    auto swatch=[&](const char *name)
    {
        controls::internal::ColorDisplay *display=
            gweni_cast<controls::internal::ColorDisplay>(picker->findChildByName(name, true));

        REQUIRE(display != nullptr);
        return display->getColor();
    };

    CHECK((swatch("Result") == Color(50, 60, 70, 255)));

    picker->setRed(200);
    picker->setGreen(100);
    picker->setBlue(20);
    picker->setAlpha(128);
    CHECK((picker->getColor() == Color(200, 100, 20, 128)));
    CHECK((swatch("Result") == Color(200, 100, 20, 128)));
    CHECK((swatch("Red") == Color(200, 0, 0, 255)));
    CHECK((swatch("Green") == Color(0, 100, 0, 255)));
    CHECK((swatch("Blue") == Color(0, 0, 20, 255)));
    CHECK((swatch("Alpha") == Color(255, 255, 255, 128)));

    picker->setColorByName("Green", 7);
    CHECK((swatch("Result") == Color(200, 7, 20, 128)));

    picker->setColor(Color(1, 2, 3, 4));
    CHECK((swatch("Result") == Color(1, 2, 3, 4)));

    delete canvas;
}
