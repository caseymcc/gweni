/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  controls::Chip on a real Canvas with the StubRenderer, drawn by the
 *  textured skin's Chip renderer (every other type, its remove button
 *  included, a StubControl), the stub skin's chip text colours set apart:
 *  - each tone draws its surface (ChipNeutral .. ChipDanger) with its text
 *    colour, and palette index i its entry i mod 8 (9 is 1, -1 is 7), a tone
 *    leaving the palette again; a tone change writes the chip's slot alone
 *    and lays nothing out;
 *  - a clickable chip fires onPressCaller once for a click, draws its hover
 *    and its press looks (its fill moved toward its border) and stays: the
 *    chip never deletes itself;
 *  - one that is not clickable takes no press: no onPressCaller, nothing held,
 *    its hover drawing its rest look;
 *  - setRemovable() adds a remove button in the right padding: the chip is
 *    wider by it, the button's click fires onRemove once and the chip stays;
 *  - a disabled chip draws ButtonDisabled's colours in its own pill radius
 *    (not the button's), its text in the disabled colour, hides its remove
 *    button at the same width, and a click there fires neither onRemove nor
 *    onPressCaller; enabled again, the button shows and removes;
 *  - setClickable(false) during a press lets the mouse go;
 *  - a text of the same width lays nothing out, and a wider one the way down
 *    to the chip in one pass, entering no sibling.
 */
#include "headlessFixture.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/chip.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <doctest/doctest.h>

#include <map>
#include <string>

using namespace gweni;
namespace tokyonight=gweni::skin::textured::tokyonight;

namespace
{

//! Records the fill, border colour and radius of every rounded rect drawn.
class FillRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        fill[primitiveId]=top;
        borderOf[primitiveId]=borderColor;
        radiusOf[primitiveId]=radius;
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    std::map<size_t, Color> fill;
    std::map<size_t, Color> borderOf;
    std::map<size_t, float> radiusOf;
};

//! The textured Chip renderer; a StubControl for every other type.
class ChipSkin: public test::StubSkin
{
public:
    explicit ChipSkin(test::StubRenderer *renderer): StubSkin(renderer)
    {
        Colors.Chip.Neutral=Color(1, 0, 0);
        Colors.Chip.Accent=Color(2, 0, 0);
        Colors.Chip.Success=Color(3, 0, 0);
        Colors.Chip.Warning=Color(4, 0, 0);
        Colors.Chip.Danger=Color(5, 0, 0);
        for(int i=0; i < controls::Chip::PaletteSize; ++i)
            Colors.Chip.Palette[i]=Color(10+i, 0, 0);
    }

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Chip")
            return std::make_unique<skin::textured::Chip>();
        return StubSkin::getControlRenderer(typeName);
    }
};

//! A plain control that counts its layout entries.
class Probe: public controls::Base
{
public:
    GWENI_CONTROL_INLINE(Probe, controls::Base)
    {}

    int entries=0;

protected:
    void calculateSize(skin::Base *skin, Dim dim) override
    {
        ++entries;
        ParentClass::calculateSize(skin, dim);
    }
};

struct Scene: public event::Handler
{
    FillRecorder renderer;
    ChipSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *panel;
    int presses=0;
    int removes=0;

    Scene()
    {
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
    }

    controls::Chip *chip(const String &text)
    {
        controls::Chip *chip=panel->newChild<controls::Chip>();

        chip->setText(text);
        chip->setPos(20, 20);
        chip->onPressCaller.add(this, &Scene::pressed);
        chip->onRemove.add(this, &Scene::removed);
        return chip;
    }

    void pressed(event::Info) { ++presses; }
    void removed(event::Info) { ++removes; }

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

    //! The mouse to \p control's centre, a frame.
    void hover(controls::Base *control)
    {
        const Point at=control->localPosToCanvas(Point(control->getWidth()/2, control->getHeight()/2));

        canvas->inputMouseMoved(at.x, at.y, 0, 0);
        frame();
    }

    void away()
    {
        canvas->inputMouseMoved(390, 290, 0, 0);
        frame();
    }

    //! A click on \p control's centre, a frame for the press and one for the release.
    void click(controls::Base *control)
    {
        hover(control);
        canvas->inputMouseButton(0, true);
        frame();
        canvas->inputMouseButton(0, false);
        frame();
    }

    size_t slotOf(const controls::Base *control) const
    {
        std::vector<size_t> primitives, texts;

        control->getSkinControl()->appendIds(primitives, texts);
        REQUIRE(primitives.size() == 1);
        return primitives[0];
    }

    Color fillOf(const controls::Base *control) { return renderer.fill[slotOf(control)]; }
};

bool same(const Color &a, const Color &b)
{
    return (a.r == b.r) && (a.g == b.g) && (a.b == b.b) && (a.a == b.a);
}

}//namespace

TEST_CASE("Chip: each tone and palette entry draws its surface and text colour")
{
    Scene scene;
    controls::Chip *chip=scene.chip("area");

    scene.settle();
    CHECK((chip->getTone() == controls::ChipTone::Neutral));
    CHECK(chip->getPaletteIndex() == -1);
    CHECK(same(scene.fillOf(chip), tokyonight::ChipNeutral.top));
    CHECK(same(chip->getTextColor(), Color(1, 0, 0)));

    const struct
    {
        controls::ChipTone tone;
        const tokyonight::Surface *surface;
        Color text;
    } tones[]={
        {controls::ChipTone::Accent, &tokyonight::ChipAccent, Color(2, 0, 0)},
        {controls::ChipTone::Success, &tokyonight::ChipSuccess, Color(3, 0, 0)},
        {controls::ChipTone::Warning, &tokyonight::ChipWarning, Color(4, 0, 0)},
        {controls::ChipTone::Danger, &tokyonight::ChipDanger, Color(5, 0, 0)},
        {controls::ChipTone::Neutral, &tokyonight::ChipNeutral, Color(1, 0, 0)},
    };

    for(const auto &tone : tones)
    {
        chip->setTone(tone.tone);
        scene.frame();
        CHECK(same(scene.fillOf(chip), tone.surface->top));
        CHECK(same(chip->getTextColor(), tone.text));
    }

    const tokyonight::Surface *const palette[]={&tokyonight::ChipPalette0, &tokyonight::ChipPalette1,
        &tokyonight::ChipPalette2, &tokyonight::ChipPalette3, &tokyonight::ChipPalette4,
        &tokyonight::ChipPalette5, &tokyonight::ChipPalette6, &tokyonight::ChipPalette7};

    for(int index : {0, 3, 7, 9, 16, -1})
    {
        const int entry=((index%8)+8)%8;

        chip->setPaletteIndex(index);
        scene.frame();
        CHECK(chip->getPaletteIndex() == entry);
        CHECK(same(scene.fillOf(chip), palette[entry]->top));
        CHECK(same(chip->getTextColor(), Color(10+entry, 0, 0)));
    }

    // a tone again leaves the palette
    chip->setTone(controls::ChipTone::Danger);
    scene.frame();
    CHECK(chip->getPaletteIndex() == -1);
    CHECK(same(scene.fillOf(chip), tokyonight::ChipDanger.top));

    // a tone change: the chip's slot alone, nothing laid out
    chip->setTone(controls::ChipTone::Success);
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    CHECK(scene.renderer.writtenPrimitiveIds() == std::vector<size_t>{scene.slotOf(chip)});
}

TEST_CASE("Chip: a clickable chip draws hover and press looks and fires onPressCaller once")
{
    Scene scene;
    controls::Chip *chip=scene.chip("project");

    chip->setTone(controls::ChipTone::Accent);
    chip->setClickable(true);
    scene.settle();

    const Color rest=scene.fillOf(chip);

    scene.hover(chip);
    const Color hover=scene.fillOf(chip);

    CHECK_FALSE(same(hover, rest));

    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(chip->isDepressed());
    CHECK_FALSE(same(scene.fillOf(chip), hover));
    CHECK_FALSE(same(scene.fillOf(chip), rest));

    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.presses == 1);
    CHECK_FALSE(chip->isDepressed());

    scene.away();
    CHECK(same(scene.fillOf(chip), rest));

    // it stays: the app removes it
    CHECK(chip->getParent() == scene.panel);
    CHECK(scene.canvas->getControlAt(chip->localPosToCanvas(Point(2, 2)).x,
        chip->localPosToCanvas(Point(2, 2)).y) == chip);
}

TEST_CASE("Chip: one that is not clickable ignores the press and draws no hover look")
{
    Scene scene;
    controls::Chip *chip=scene.chip("area");

    chip->setTone(controls::ChipTone::Warning);
    scene.settle();

    const Color rest=scene.fillOf(chip);

    scene.hover(chip);
    CHECK(chip->isHovered());
    CHECK(same(scene.fillOf(chip), rest));

    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    CHECK_FALSE(chip->isDepressed());
    CHECK(same(scene.fillOf(chip), rest));
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.presses == 0);
}

TEST_CASE("Chip: the remove button, in the right padding, fires onRemove once and the chip stays")
{
    Scene scene;
    controls::Chip *chip=scene.chip("tag");

    scene.settle();

    const int plain=chip->getWidth();

    chip->setRemovable(true);
    scene.settle();

    controls::ChipRemoveButton *button=chip->getRemoveButton();

    REQUIRE(button != nullptr);
    CHECK(chip->isRemovable());
    CHECK(button->getPlacement() == controls::Placement::Owner);
    // wider by the square, which sits in the padding, clear of the text
    CHECK(chip->getWidth() > plain);
    CHECK(chip->getWidth()-plain >= controls::Chip::RemoveButtonSize-controls::Chip::PaddingX);
    CHECK(button->getX() >= chip->textRight());
    CHECK(button->getX()+button->getWidth() <= chip->getWidth());
    CHECK(button->getY() == (chip->getHeight()-controls::Chip::RemoveButtonSize)/2);

    scene.click(button);
    CHECK(scene.removes == 1);
    CHECK(scene.presses == 0);
    CHECK(chip->getParent() == scene.panel);
    CHECK(chip->getRemoveButton() == button);

    const int width=chip->getWidth();

    scene.hover(button);
    scene.away();
    CHECK(chip->getWidth() == width);

    chip->setRemovable(false);
    scene.settle();
    CHECK(chip->getRemoveButton() == nullptr);
    CHECK(chip->getWidth() == plain);
}

TEST_CASE("Chip: a disabled chip keeps its pill, hides its remove button and fires nothing")
{
    Scene scene;
    controls::Chip *chip=scene.chip("tag");

    scene.skin.Colors.Button.Disabled=Color(7, 7, 7);
    chip->setTone(controls::ChipTone::Success);
    chip->setClickable(true);
    chip->setRemovable(true);
    scene.settle();

    controls::ChipRemoveButton *button=chip->getRemoveButton();
    const size_t slot=scene.slotOf(chip);
    const float pill=scene.renderer.radiusOf[slot];
    const int width=chip->getWidth();

    REQUIRE(button != nullptr);
    CHECK(button->visible());
    CHECK(pill == doctest::Approx(0.5f*float(chip->getHeight())));

    chip->setDisabled(true);
    scene.settle();
    CHECK(same(scene.fillOf(chip), tokyonight::ButtonDisabled.top));
    CHECK(same(scene.renderer.borderOf[slot], tokyonight::ButtonDisabled.border));
    // its own shape, not a button's corners
    CHECK(scene.renderer.radiusOf[slot] == doctest::Approx(pill));
    CHECK(scene.renderer.radiusOf[slot] != doctest::Approx(tokyonight::ButtonDisabled.radius));
    CHECK(same(chip->getTextColor(), Color(7, 7, 7)));
    CHECK(button->hidden());
    CHECK(chip->getWidth() == width);

    scene.click(button);
    CHECK(scene.removes == 0);
    CHECK(scene.presses == 0);

    chip->setDisabled(false);
    scene.away();
    scene.settle();
    CHECK_FALSE(button->hidden());
    CHECK(same(scene.fillOf(chip), tokyonight::ChipSuccess.top));
    scene.click(button);
    CHECK(scene.removes == 1);

    // a removable chip made while disabled starts with the button hidden
    controls::Chip *other=scene.chip("other");

    other->setPos(20, 80);
    other->setDisabled(true);
    other->setRemovable(true);
    scene.settle();
    CHECK(other->getRemoveButton()->hidden());
}

TEST_CASE("Chip: setClickable(false) during a press lets the mouse go")
{
    Scene scene;
    controls::Chip *chip=scene.chip("project");

    chip->setClickable(true);
    scene.settle();
    scene.hover(chip);
    scene.canvas->inputMouseButton(0, true);
    scene.frame();
    REQUIRE(MouseFocus == chip);

    chip->setClickable(false);
    CHECK(MouseFocus == nullptr);
    CHECK_FALSE(chip->isDepressed());
    scene.canvas->inputMouseButton(0, false);
    scene.frame();
    CHECK(scene.presses == 0);
}

TEST_CASE("Chip: a text of the same width lays nothing out, a wider one only the way down to the chip")
{
    Scene scene;
    controls::Chip *chip=scene.chip("abcd");
    Probe *sibling=scene.panel->newChild<Probe>();

    sibling->setBounds(200, 20, 40, 20);
    scene.settle();

    const int width=chip->getWidth();

    chip->setText("wxyz");
    scene.frame();
    CHECK(chip->getWidth() == width);
    CHECK(scene.canvas->getLayoutStats().passes == 0);

    sibling->entries=0;
    chip->setText("a longer area");
    scene.frame();
    CHECK(chip->getWidth() > width);
    CHECK(scene.canvas->getLayoutStats().passes == 1);
    CHECK(sibling->entries == 0);
}
