/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  ColorButton and ColorEdit on a real Canvas with the real (un-init()'d)
 *  textured skin on a recording StubRenderer, clicked through
 *  input::onMouseClicked, keyed through input::onKeyEvent and typed through
 *  Canvas::inputCharacter.
 *
 *  ColorButton::toHex/fromHex round trip "#RRGGBB" and "#RRGGBBAA" (the '#'
 *  optional, either case) and refuse anything else: a non-hex digit, a
 *  3-digit shorthand, eight digits without alpha, a space, an empty string.
 *  A ColorButton gets its own renderer, one slot: a plain rounded rect (style
 *  4) while opaque, a checker (style 7) while translucent, its border
 *  ColorSwatch's at rest and ColorButtonHover's while hovered, a hover
 *  drawing that one slot. setColor with its colour marks nothing; a new one
 *  marks StateChange_Background only and sets the tooltip to its hex value.
 *
 *  ColorEdit: the R, G, B, A fields (or the hex field) show the colour;
 *  typing applies a field once it holds a whole value, with one
 *  onColorChanged per change of the colour and none for a no-op ("0175" for
 *  175, the same colour set again); text that is no value ("1.5", "#12",
 *  "") changes nothing and is reverted when the field loses the focus, and
 *  an out-of-range number is clamped then ("300" to 255, "-5" to 0). After a
 *  field edit, the only pending changes in the ColorEdit are the swatch's
 *  Background and that field's text. Alpha off hides the A field and makes
 *  the colour opaque, the hex field six digits. A click on the swatch opens
 *  an HSVColorPicker in a Popup anchored below it, holding the colour
 *  exactly, alpha included (#463); a
 *  field edit moves the picker, a picker change updates the swatch and the
 *  fields keeping the alpha, with no loop back; Escape, a click outside and
 *  a second click on the swatch close it. Deleting the ColorEdit gives its
 *  ids back, the popup's included. A canvas deleted with a ColorEdit whose
 *  popup was opened is safe in both orders its children can be in: the popup
 *  after the ColorEdit's holder, and before it once the holder was brought to
 *  the front (the canvas deleted the popup first, and ~ColorEdit touched it:
 *  a heap-use-after-free in the ASAN tree), the popup open or closed. The
 *  popup is the ColorEdit's (Base::createPopup, #490): so it is in a
 *  WindowControl brought to the front after the popup was opened, or never
 *  opened at all; and a ColorEdit taken off the canvas and deleted after it
 *  touches nothing of the popup the canvas deleted.
 *
 *  HSVColorPicker::setColor before any layout leaves the picker on exactly
 *  that colour (#413: RGBtoHSV gave al_color_rgb_to_hsv 0-255 channels where it
 *  takes 0-1, so the value was 255 times too large and the cursor on the
 *  box's top edge). property::ColorSelector's swatch is a ColorButton showing
 *  the "r g b" or "r,g,b" value.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorButton.h>
#include <gweni/controls/colorControls.h>
#include <gweni/controls/colorEdit.h>
#include <gweni/controls/hsvColorPicker.h>
#include <gweni/controls/label.h>
#include <gweni/controls/popup.h>
#include <gweni/controls/property/colorSelector.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/windowControl.h>
#include <gweni/inputHandler.h>
#include <gweni/platforms/platform.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <cstdlib>
#include <vector>

using namespace gweni;
using controls::ColorButton;
using controls::ColorEdit;
using controls::ColorEditMode;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each slot's draws: its style (4 rounded, 7 checker) and border.
class SwatchRecorder: public test::StubRenderer
{
public:
    struct Draw
    {
        size_t id;
        int style;
        Color fill;
        Color border;
    };

    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color top, Color, Color border, int) override
    {
        draws.push_back({primitiveId, 4, top, border});
    }

    void drawCheckerRect(size_t primitiveId, Rect, int, Color fill, Color, Color, float, float,
        Color border) override
    {
        draws.push_back({primitiveId, 7, fill, border});
    }

    const Draw *last(size_t id) const
    {
        for(auto it=draws.rbegin(); it != draws.rend(); ++it)
        {
            if(it->id == id)
                return &*it;
        }
        return nullptr;
    }

    std::vector<Draw> draws;
};

struct Scene
{
    SwatchRecorder renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    float clock=10.0f;

    Scene()
    {
        // a second apart per press, so two presses are never a double click
        platform::setTimeSource([this]() { return clock; });
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
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
        renderer.draws.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    void moveOnto(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        canvas->inputMouseMoved(at.x+control->getWidth()/2, at.y+control->getHeight()/2, 0, 0);
        frame();
    }

    void leftClick(int x, int y)
    {
        clock+=1.0f;
        canvas->inputMouseMoved(x, y, 0, 0);
        input::onMouseClicked(canvas, 0, true);
        input::onMouseClicked(canvas, 0, false);
        frame();
    }

    void clickOn(controls::Base *control)
    {
        const Point at=control->localPosToCanvas();

        leftClick(at.x+control->getWidth()/2, at.y+control->getHeight()/2);
    }

    void key(int key)
    {
        input::onKeyEvent(canvas, key, true);
        input::onKeyEvent(canvas, key, false);
        frame();
    }

    //! Focuses \p field (all its text selected) and types \p text.
    void type(controls::TextBox *field, const char *text)
    {
        field->focus();
        field->onSelectAll(field);
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }

    ColorEdit *edit(const Color &color, ColorEditMode mode=ColorEditMode::Rgba)
    {
        ColorEdit *edit=canvas->newChild<ColorEdit>();

        edit->setMode(mode);
        edit->setColor(color, false);
        edit->setPos(20, 20);
        frame();
        return edit;
    }
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

//! Every control under \p control (itself included) with a pending change.
void marked(controls::Base *control, std::vector<controls::Base *> &out)
{
    if(control->getStateChange() != controls::StateChange_Nothing)
        out.push_back(control);
    for(controls::Base *child : control->getActualChildren())
        marked(child, out);
}

bool inside(controls::Base *control, controls::Base *ancestor)
{
    for(; control; control=control->getParent())
    {
        if(control == ancestor)
            return true;
    }
    return false;
}

//! Counts onColorChanged.
struct Counter: public event::Handler
{
    int count=0;
    void changed(event::Info) { ++count; }
};

}//namespace

TEST_CASE("ColorButton::toHex and fromHex round trip #RRGGBB and #RRGGBBAA and refuse anything else")
{
    const Color color(0x7a, 0xa2, 0xf7, 0x40);

    CHECK(ColorButton::toHex(color, false) == "#7AA2F7");
    CHECK(ColorButton::toHex(color, true) == "#7AA2F740");
    CHECK(ColorButton::toHex(Color(0, 1, 15, 255), true) == "#00010FFF");

    Color parsed;
    REQUIRE(ColorButton::fromHex("#7AA2F7", parsed, false));
    CHECK(sameColor(parsed, Color(0x7a, 0xa2, 0xf7, 255)));
    REQUIRE(ColorButton::fromHex("7aa2f740", parsed, true));
    CHECK(sameColor(parsed, color));
    REQUIRE(ColorButton::fromHex(ColorButton::toHex(color, true), parsed, true));
    CHECK(sameColor(parsed, color));
    // six digits with alpha allowed: opaque
    REQUIRE(ColorButton::fromHex("#000000", parsed, true));
    CHECK(sameColor(parsed, Color(0, 0, 0, 255)));

    for(const char *bad : {"", "#", "#7AA2F", "#7AA2G7", "#FFF", "7AA2 F7", " #7AA2F7", "#7AA2F7 ", "##7AA2F7"})
    {
        CAPTURE(bad);
        Color kept(1, 2, 3, 4);
        CHECK_FALSE(ColorButton::fromHex(bad, kept, true));
        CHECK(sameColor(kept, Color(1, 2, 3, 4)));
    }
    Color kept(1, 2, 3, 4);
    CHECK_FALSE(ColorButton::fromHex("#7AA2F740", kept, false));
    CHECK(sameColor(kept, Color(1, 2, 3, 4)));
}

TEST_CASE("a ColorButton draws one slot, a checker while translucent, its border following hover")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        ColorButton *button=scene.canvas->newChild<ColorButton>();
        button->setPos(10, 10);
        button->setColor(Color(122, 162, 247, 255));
        CHECK(button->getToolTipText() == "#7AA2F7");
        scene.frame();

        REQUIRE(dynamic_cast<const skin::textured::ColorButton *>(button->getSkinControl()) != nullptr);
        const std::vector<size_t> &ids=button->getSkinControl()->getPrimitives();
        REQUIRE(ids.size() == 1);
        const SwatchRecorder::Draw *draw=scene.renderer.last(ids[0]);
        REQUIRE(draw != nullptr);
        CHECK(draw->style == 4);
        CHECK(sameColor(draw->fill, Color(122, 162, 247, 255)));
        CHECK(sameColor(draw->border, tokyonight::ColorSwatch.border));

        // the same colour again marks nothing and draws nothing
        button->setColor(Color(122, 162, 247, 255));
        CHECK(button->getStateChange() == controls::StateChange_Nothing);
        scene.frame();
        CHECK(scene.renderer.draws.empty());

        // translucent: a checker, Background alone, the tooltip with its alpha
        button->setColor(Color(247, 118, 142, 128));
        CHECK(button->getStateChange() == controls::StateChange_Background);
        CHECK(button->getToolTipText() == "#F7768E80");
        scene.frame();
        REQUIRE(scene.renderer.draws.size() == 1);
        CHECK(scene.renderer.draws[0].id == ids[0]);
        CHECK(scene.renderer.draws[0].style == 7);

        // hovered: its one slot again, with the hover border
        scene.moveOnto(button);
        REQUIRE(HoveredControl == button);
        REQUIRE(scene.renderer.draws.size() == 1);
        CHECK(scene.renderer.draws[0].id == ids[0]);
        CHECK(sameColor(scene.renderer.draws[0].border, tokyonight::ColorButtonHover.border));
        scene.frame();
        CHECK(scene.renderer.draws.empty());
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("ColorEdit's RGBA fields apply a whole value as it is typed, once per change")
{
    Scene scene;
    ColorEdit *edit=scene.edit(Color(224, 175, 104, 200));
    Counter counter;
    edit->onColorChanged.add(&counter, &Counter::changed);

    REQUIRE(edit->getField(3) != nullptr);
    CHECK(edit->getField(0)->getText() == "224");
    CHECK(edit->getField(1)->getText() == "175");
    CHECK(edit->getField(2)->getText() == "104");
    CHECK(edit->getField(3)->getText() == "200");
    CHECK(gweni_cast<controls::TextBoxNumeric>(edit->getField(0)) != nullptr);
    CHECK(sameColor(edit->getSwatch()->getColor(), Color(224, 175, 104, 200)));

    // all of it selected, typing replaces it: 9, then 90
    scene.type(edit->getField(0), "90");
    CHECK(counter.count == 2);
    CHECK(sameColor(edit->getColor(), Color(90, 175, 104, 200)));
    CHECK(sameColor(edit->getSwatch()->getColor(), edit->getColor()));
    CHECK(edit->getSwatch()->getToolTipText() == "#5AAF68C8");

    // no-ops: the same number written another way, the same colour set again
    edit->getField(1)->setText("0175");
    edit->setColor(Color(90, 175, 104, 200));
    CHECK(counter.count == 2);

    // setColor shows the colour everywhere, firing once
    edit->setColor(Color(1, 2, 3, 4));
    CHECK(counter.count == 3);
    CHECK(edit->getField(0)->getText() == "1");
    CHECK(edit->getField(3)->getText() == "4");
    edit->setColor(Color(5, 6, 7, 8), false);
    CHECK(counter.count == 3);
    CHECK(edit->getField(2)->getText() == "7");
}

TEST_CASE("ColorEdit refuses what is no value, reverting it, and clamps a number when the field loses the focus")
{
    Scene scene;
    ColorEdit *edit=scene.edit(Color(224, 175, 104, 200));
    controls::TextBox *red=edit->getField(0);
    controls::TextBox *green=edit->getField(1);
    Counter counter;
    edit->onColorChanged.add(&counter, &Counter::changed);

    for(const char *bad : {"1.5", "", "-", "300", "-5"})
    {
        CAPTURE(bad);
        red->focus();
        red->setText(bad);
        CHECK(counter.count == 0);
        CHECK(edit->getColor().r == 224);
        green->focus();
        if(String(bad) == "300")
        {
            CHECK(edit->getColor().r == 255);
            CHECK(red->getText() == "255");
            CHECK(counter.count == 1);
            edit->setColor(Color(224, 175, 104, 200), false);
            counter.count=0;
        }
        else if(String(bad) == "-5")
        {
            CHECK(edit->getColor().r == 0);
            CHECK(red->getText() == "0");
            CHECK(counter.count == 1);
            edit->setColor(Color(224, 175, 104, 200), false);
            counter.count=0;
        }
        else
        {
            CHECK(red->getText() == "224");
            CHECK(counter.count == 0);
        }
    }

    // Return moves the focus on, which applies and canonicalises the field
    red->focus();
    red->setText("007");
    CHECK(edit->getColor().r == 7);
    scene.key(keys::Return);
    CHECK_FALSE(red->isFocussed());
    CHECK(red->getText() == "7");
}

TEST_CASE("ColorEdit's hex field takes six digits, eight with alpha, the '#' and either case")
{
    Scene scene;
    ColorEdit *edit=scene.edit(Color(158, 206, 106, 255), ColorEditMode::Hex);
    controls::TextBox *hex=edit->getField(0);
    Counter counter;
    edit->onColorChanged.add(&counter, &Counter::changed);

    REQUIRE(hex != nullptr);
    CHECK(edit->getField(1) == nullptr);
    CHECK(gweni_cast<controls::TextBoxNumeric>(hex) == nullptr);
    CHECK(hex->getText() == "#9ECE6AFF");

    hex->focus();
    hex->setText("#12");
    CHECK(counter.count == 0);
    hex->setText("1a2b3c");
    CHECK(counter.count == 1);
    CHECK(sameColor(edit->getColor(), Color(0x1a, 0x2b, 0x3c, 255)));
    // left as typed until the focus goes, then written as the colour
    CHECK(hex->getText() == "1a2b3c");
    hex->setText("#1A2B3C44");
    CHECK(counter.count == 2);
    CHECK(edit->getColor().a == 0x44);
    hex->setText("#1A2B3C4");
    hex->unfocus();
    CHECK(hex->getText() == "#1A2B3C44");
    CHECK(counter.count == 2);

    // alpha off: opaque, six digits, eight refused
    edit->setAlphaEnabled(false);
    CHECK(counter.count == 3);
    CHECK(edit->getColor().a == 255);
    CHECK(hex->getText() == "#1A2B3C");
    hex->focus();
    hex->setText("#11223344");
    CHECK(counter.count == 3);
    hex->setText("#112233");
    CHECK(counter.count == 4);
    CHECK(sameColor(edit->getColor(), Color(0x11, 0x22, 0x33, 255)));
}

TEST_CASE("alpha off hides ColorEdit's A field and makes the colour opaque")
{
    Scene scene;
    ColorEdit *edit=scene.edit(Color(10, 20, 30, 40));
    Counter counter;
    edit->onColorChanged.add(&counter, &Counter::changed);

    CHECK(edit->getField(3)->visible());
    edit->setAlphaEnabled(false);
    CHECK(counter.count == 1);
    CHECK(edit->getColor().a == 255);
    CHECK_FALSE(edit->getField(3)->visible());
    edit->setColor(Color(10, 20, 30, 40));
    CHECK(edit->getColor().a == 255);
    CHECK(counter.count == 1);
    edit->setAlphaEnabled(true);
    CHECK(edit->getField(3)->visible());
    CHECK(edit->getField(3)->getText() == "255");
}

TEST_CASE("a ColorEdit field edit marks only the swatch's Background and that field's text")
{
    Scene scene;
    ColorEdit *edit=scene.edit(Color(224, 175, 104, 200));
    controls::TextBox *red=edit->getField(0);
    scene.frame();

    // an edit from outside the keyboard: no focus, no caret
    red->setText("10");
    std::vector<controls::Base *> pending;
    marked(edit, pending);

    CHECK(edit->getSwatch()->getStateChange() == controls::StateChange_Background);
    bool redText=false;
    for(controls::Base *control : pending)
    {
        CAPTURE(control->getTypeName());
        if(control == edit->getSwatch())
            continue;
        REQUIRE(inside(control, red));
        CHECK(control != red);
        CHECK(control->getStateChange() == controls::StateChange_Text);
        redText=true;
    }
    CHECK(redText);
    scene.frame();

    // typed: nothing outside the swatch and the field it is typed into
    scene.type(edit->getField(2), "5");
    pending.clear();
    marked(edit, pending);
    for(controls::Base *control : pending)
    {
        CAPTURE(control->getTypeName());
        CHECK((control == edit->getSwatch() || inside(control, edit->getField(2))));
    }
    CHECK(edit->getSwatch()->getStateChange() == controls::StateChange_Background);
}

TEST_CASE("a click on ColorEdit's swatch opens an HSV picker in a Popup below it, kept in step with the fields")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();
    {
        Scene scene;
        ColorEdit *edit=scene.edit(Color(224, 175, 104, 200));
        Counter counter;
        edit->onColorChanged.add(&counter, &Counter::changed);

        CHECK(edit->getPopup() == nullptr);
        scene.clickOn(edit->getSwatch());
        REQUIRE(edit->getPopup() != nullptr);
        REQUIRE(edit->getPicker() != nullptr);
        CHECK(edit->isPickerOpen());
        CHECK(scene.canvas->shownPopup() == edit->getPopup());
        CHECK(edit->getPopup()->getAnchor() == edit->getSwatch());
        const Point swatch=edit->getSwatch()->localPosToCanvas();
        const Point popup=edit->getPopup()->localPosToCanvas();
        CHECK(popup.y >= swatch.y+edit->getSwatch()->getHeight());
        CHECK(popup.x == swatch.x);
        // opening it shows the colour and changes nothing
        CHECK(sameColor(edit->getPicker()->getColor(), edit->getColor()));
        CHECK(counter.count == 0);
        CHECK(sameColor(edit->getColor(), Color(224, 175, 104, 200)));

        // a field edit moves the picker
        edit->getField(1)->setText("20");
        CHECK(counter.count == 1);
        CHECK(sameColor(edit->getPicker()->getColor(), Color(224, 20, 104, 200)));
        CHECK(sameColor(edit->getColor(), Color(224, 20, 104, 200)));

        // a click in the picker's saturation/value box reaches the swatch and
        // the fields, keeping the alpha, once
        controls::ColorLerpBox *box=nullptr;
        for(controls::Base *child : edit->getPicker()->getChildren())
        {
            if(!box)
                box=gweni_cast<controls::ColorLerpBox>(child);
        }
        REQUIRE(box != nullptr);
        const Point boxAt=box->localPosToCanvas();
        scene.leftClick(boxAt.x+box->getWidth()/2, boxAt.y+box->getHeight()/4);
        CHECK(edit->isPickerOpen());
        CHECK(counter.count == 2);
        const Color picked=edit->getColor();
        CHECK(sameColor(picked, edit->getPicker()->getColor()));
        CHECK(picked.a == 200);
        CHECK(sameColor(edit->getSwatch()->getColor(), picked));
        CHECK(edit->getField(1)->getText() == toString(+picked.g));

        // Escape, a click outside and a second click on the swatch close it
        scene.key(keys::Escape);
        CHECK_FALSE(edit->isPickerOpen());
        scene.clickOn(edit->getSwatch());
        CHECK(edit->isPickerOpen());
        CHECK(sameColor(edit->getPicker()->getColor(), picked));
        scene.leftClick(600, 460);
        CHECK_FALSE(edit->isPickerOpen());
        scene.clickOn(edit->getSwatch());
        CHECK(edit->isPickerOpen());
        scene.clickOn(edit->getSwatch());
        CHECK_FALSE(edit->isPickerOpen());
        CHECK(counter.count == 2);

        // closed, a field edit leaves the picker as it was
        const Color before=edit->getPicker()->getColor();
        edit->getField(0)->setText("99");
        CHECK(sameColor(edit->getPicker()->getColor(), before));

        delete edit;
        scene.frame();
        CHECK(scene.canvas->shownPopup() == nullptr);
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("a canvas deleted with a ColorEdit whose picker was opened deletes the popup once, in either order")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    for(const bool front : {false, true})
    {
        for(const bool leaveOpen : {false, true})
        {
            CAPTURE(front);
            CAPTURE(leaveOpen);
            SwatchRecorder renderer;
            skin::textured::TexturedSkin skin{&renderer};
            controls::Canvas *canvas=new controls::Canvas(&skin);
            canvas->setSize(640, 480);

            // a holder, as a Window holding the field would be
            controls::Base *holder=canvas->newChild<controls::Base>();
            holder->setBounds(0, 0, 300, 200);
            ColorEdit *edit=holder->newChild<ColorEdit>();
            edit->setPos(10, 10);
            canvas->updateCanvas();
            canvas->renderCanvas();

            edit->togglePicker();
            REQUIRE(edit->getPopup() != nullptr);
            if(!leaveOpen)
                edit->togglePicker();
            CHECK(edit->isPickerOpen() == leaveOpen);
            // the popup is now after the holder; bringing the holder to the
            // front puts it after the popup, which the canvas then deletes first
            if(front)
                holder->bringToFront();
            canvas->updateCanvas();
            canvas->renderCanvas();

            HoveredControl=nullptr;
            KeyboardFocus=nullptr;
            MouseFocus=nullptr;
            delete canvas;
        }
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("a canvas deleted with a ColorEdit in a WindowControl brought to the front, or detached first (#490)")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    // 0: never opened, 1: opened and closed, 2: open
    for(const int opened : {0, 1, 2})
    {
        for(const bool detach : {false, true})
        {
            CAPTURE(opened);
            CAPTURE(detach);
            SwatchRecorder renderer;
            skin::textured::TexturedSkin skin{&renderer};
            controls::Canvas *canvas=new controls::Canvas(&skin);
            canvas->setSize(640, 480);

            auto *window=canvas->newChild<controls::WindowControl>();
            window->setBounds(20, 20, 320, 240);
            ColorEdit *edit=window->newChild<ColorEdit>();
            edit->setPos(10, 10);
            canvas->updateCanvas();
            canvas->renderCanvas();

            if(opened > 0)
            {
                edit->togglePicker();
                REQUIRE(edit->getPopup() != nullptr);
                CHECK(edit->getPopup()->getOwner() == edit);
            }
            if(opened == 1)
                edit->togglePicker();
            window->bringToFront();
            canvas->updateCanvas();
            canvas->renderCanvas();
            // taken off the canvas, its popup stays there: the canvas deletes
            // it, and the ColorEdit is deleted after the canvas
            if(detach)
                edit->setParent(nullptr);

            HoveredControl=nullptr;
            KeyboardFocus=nullptr;
            MouseFocus=nullptr;
            delete canvas;
            if(detach)
                delete edit;
        }
    }
    const test::IdBankCounts after=test::IdBankCounts::now();
    CHECK(after.livePrimitives() == baseline.livePrimitives());
    CHECK(after.liveTexts() == baseline.liveTexts());
}

TEST_CASE("HSVColorPicker::setColor before any layout leaves the picker on that colour (#413)")
{
    Scene scene;
    controls::HSVColorPicker *picker=scene.canvas->newChild<controls::HSVColorPicker>();

    for(const Color &color : {Color(158, 206, 106, 255), Color(224, 175, 104, 255), Color(10, 20, 30, 255)})
    {
        picker->setColor(color);
        CHECK(sameColor(picker->getColor(), color));
    }
}

TEST_CASE("property::ColorSelector's swatch is a ColorButton showing its value")
{
    Scene scene;
    controls::property::ColorSelector *selector=scene.canvas->newChild<controls::property::ColorSelector>();

    REQUIRE(gweni_cast<ColorButton>(selector->m_button) != nullptr);
    CHECK(String(selector->m_button->getTypeName()) == "ColorButton");
    selector->setPropertyValue("255 0 0", false);
    CHECK(sameColor(selector->m_button->getColor(), Color(255, 0, 0, 255)));
    selector->setPropertyValue("10,20,30", false);
    CHECK(sameColor(selector->m_button->getColor(), Color(10, 20, 30, 255)));
    // not a colour: kept
    selector->setPropertyValue("red", false);
    CHECK(sameColor(selector->m_button->getColor(), Color(10, 20, 30, 255)));
    scene.frame();
    CHECK(dynamic_cast<const skin::textured::ColorButton *>(selector->m_button->getSkinControl()) != nullptr);
}
