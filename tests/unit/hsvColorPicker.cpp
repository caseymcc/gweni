/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  HSVColorPicker's colour, on a real Canvas with the real (un-init()'d)
 *  textured skin on a StubRenderer (#463).
 *
 *  setColor(c) then getColor() gives c back, r, g, b and a exactly, for
 *  (158,206,106), pure red, black and a grey: on a picker that has not been
 *  laid out, on one that has, and after its ColorLerpBox was resized. The R,
 *  G, B boxes read c's channels and the "after" swatch is c; reset also sets
 *  the "before" swatch. The box's getCursorPos() is (s*W, (1-v)*H) of c's
 *  HSV at the box's current size, to the nearest unit, and the slider's
 *  getSelectedDist() is c's hue/360 of its height, to the nearest unit; a
 *  colour with no hue (black, a grey) keeps the hue the box and the slider
 *  had. setColor(c, true) moves only the hue, and getColor() is then the
 *  box's resulting colour. A new picker is on the box's default colour,
 *  (255,128,0), its cursor in the top-right corner.
 *
 *  setColor fires onColorChanged zero times. A user change fires it once per
 *  change: a press in the box (getColor() the colour under the cursor, the
 *  alpha kept), a move while pressed, a press on the slider (the box takes
 *  its hue), and a value typed into the R, G or B box (getColor() the typed
 *  colour); a release where the press was and a move to the same place fire
 *  nothing.
 */
#include "headlessFixture.h"

#include <gweni/controls/colorControls.h>
#include <gweni/controls/colorPicker.h>
#include <gweni/controls/hsvColorPicker.h>
#include <gweni/controls/textBox.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <vector>

using namespace gweni;
using controls::HSVColorPicker;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    skin::textured::TexturedSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;

    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        // the hovered control, the focus and the button state are process-global
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Focuses \p field (all its text selected) and types \p text.
    void type(controls::TextBox *field, const char *text)
    {
        field->focus();
        field->onSelectAll(field);
        for(const char *c=text; *c; ++c)
            canvas->inputCharacter(static_cast<UnicodeChar>(*c));
    }
};

//! The picker's parts, found among its children.
struct Parts
{
    explicit Parts(HSVColorPicker *picker)
    {
        for(controls::Base *child : picker->getActualChildren())
        {
            if(auto *found=gweni_cast<controls::ColorLerpBox>(child))
                box=found;
            else if(auto *found=gweni_cast<controls::ColorSlider>(child))
                slider=found;
            else if(auto *found=gweni_cast<controls::internal::ColorDisplay>(child))
                (after?before:after)=found;   // the "after" swatch is made first
        }
        red=picker->findChild<controls::TextBoxNumeric>("RedBox");
        green=picker->findChild<controls::TextBoxNumeric>("GreenBox");
        blue=picker->findChild<controls::TextBoxNumeric>("BlueBox");
    }

    controls::ColorLerpBox *box=nullptr;
    controls::ColorSlider *slider=nullptr;
    controls::internal::ColorDisplay *after=nullptr;
    controls::internal::ColorDisplay *before=nullptr;
    controls::TextBoxNumeric *red=nullptr;
    controls::TextBoxNumeric *green=nullptr;
    controls::TextBoxNumeric *blue=nullptr;
};

bool sameColor(const Color &a, const Color &b)
{
    return a.r == b.r && a.g == b.g && a.b == b.b && a.a == b.a;
}

String describe(const Color &c)
{
    return std::to_string(+c.r)+","+std::to_string(+c.g)+","+std::to_string(+c.b)+","+std::to_string(+c.a);
}

//! The textbook RGB to HSV, in doubles: h in degrees, s and v in 0..1.
struct Hsv
{
    double h, s, v;
};

Hsv toHsv(const Color &c)
{
    const double r=c.r/255.0, g=c.g/255.0, b=c.b/255.0;
    const double max=std::max({r, g, b});
    const double min=std::min({r, g, b});
    const double delta=max-min;
    double h=0;

    if(delta > 0)
    {
        if(max == r)
            h=60*std::fmod((g-b)/delta, 6.0);
        else if(max == g)
            h=60*((b-r)/delta+2);
        else
            h=60*((r-g)/delta+4);
        if(h < 0)
            h+=360;
    }
    return {h, max > 0?delta/max:0, max};
}

//! Counts onColorChanged.
struct Counter: public event::Handler
{
    int count=0;
    void changed(event::Info) { ++count; }
};

const Color kSample(158, 206, 106, 255);
const Color kRed(255, 0, 0, 255);
const Color kBlack(0, 0, 0, 255);
const Color kGrey(128, 128, 128, 255);

//! setColor(c) on \p picker and everything it must show for it. \p hue is
//! the hue the picker must be at (c's own, or the kept one for a grey).
void checkShows(HSVColorPicker *picker, const Color &color, double hue)
{
    Parts parts(picker);
    REQUIRE(parts.box != nullptr);
    REQUIRE(parts.slider != nullptr);
    REQUIRE(parts.after != nullptr);
    REQUIRE(parts.red != nullptr);
    REQUIRE(parts.green != nullptr);
    REQUIRE(parts.blue != nullptr);

    const Hsv hsv=toHsv(color);

    CAPTURE(describe(color));
    CAPTURE(describe(picker->getColor()));
    CHECK(sameColor(picker->getColor(), color));
    CHECK(parts.red->getText() == std::to_string(+color.r));
    CHECK(parts.green->getText() == std::to_string(+color.g));
    CHECK(parts.blue->getText() == std::to_string(+color.b));
    CHECK(sameColor(parts.after->getColor(), color));

    const int width=parts.box->getWidth();
    const int height=parts.box->getHeight();
    CAPTURE(width);
    CAPTURE(height);
    CHECK(parts.box->getCursorPos().x == static_cast<int>(std::lround(hsv.s*width)));
    CHECK(parts.box->getCursorPos().y == static_cast<int>(std::lround((1-hsv.v)*height)));
    CHECK(parts.box->getHue() == static_cast<int>(hue));
    CHECK(parts.slider->getSelectedDist()
        == static_cast<int>(std::lround(hue/360*parts.slider->getHeight())));
}

}//namespace

TEST_CASE("HSVColorPicker: a new picker is on the box's default colour")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();

    checkShows(picker, Color(255, 128, 0, 255), toHsv(Color(255, 128, 0, 255)).h);
    scene.frame();
    checkShows(picker, Color(255, 128, 0, 255), toHsv(Color(255, 128, 0, 255)).h);
}

TEST_CASE("HSVColorPicker: setColor(c) then getColor() is c, before and after a layout")
{
    for(const bool laidOut : {false, true})
    {
        CAPTURE(laidOut);
        Scene scene;
        HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
        Counter counter;
        picker->onColorChanged.add(&counter, &Counter::changed);

        if(laidOut)
            scene.frame();

        picker->setColor(kSample);
        checkShows(picker, kSample, toHsv(kSample).h);
        picker->setColor(kRed);
        checkShows(picker, kRed, 0);
        // black and greys have no hue: the picker keeps red's
        picker->setColor(kBlack);
        checkShows(picker, kBlack, 0);
        picker->setColor(kSample);
        picker->setColor(kGrey);
        checkShows(picker, kGrey, toHsv(kSample).h);
        picker->setColor(kBlack);
        checkShows(picker, kBlack, toHsv(kSample).h);

        // the alpha is carried through
        const Color translucent(158, 206, 106, 77);
        picker->setColor(translucent);
        checkShows(picker, translucent, toHsv(translucent).h);

        // a programmatic set is not a change the user made
        CHECK(counter.count == 0);

        // and a frame keeps it
        scene.frame();
        checkShows(picker, translucent, toHsv(translucent).h);
        CHECK(counter.count == 0);
    }
}

TEST_CASE("HSVColorPicker: setColor with reset sets the before swatch too")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.before != nullptr);

    picker->setColor(kSample, false, true);
    CHECK(sameColor(parts.before->getColor(), kSample));
    CHECK(sameColor(picker->getDefaultColor(), kSample));
    picker->setColor(kRed);
    CHECK(sameColor(parts.before->getColor(), kSample));
    CHECK(sameColor(parts.after->getColor(), kRed));
}

TEST_CASE("HSVColorPicker: the cursor follows the box's size after setColor")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.box != nullptr);

    for(const Color &color : {kSample, kRed, kBlack, kGrey})
    {
        picker->setColor(kSample);
        picker->setColor(color);
        const double hue=(color.r == color.g && color.g == color.b)?toHsv(kSample).h:toHsv(color).h;

        parts.box->setSize(200, 100);
        checkShows(picker, color, hue);
        scene.frame();
        checkShows(picker, color, hue);
        parts.box->setSize(128, 128);
        scene.frame();
        checkShows(picker, color, hue);
    }
}

TEST_CASE("HSVColorPicker: setColor(c, true) moves only the hue")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.box != nullptr);
    Counter counter;
    picker->onColorChanged.add(&counter, &Counter::changed);

    picker->setColor(kSample);
    const Point cursor=parts.box->getCursorPos();

    // cyan's hue at the sample's saturation and value
    picker->setColor(Color(0, 255, 255, 255), true);
    CHECK(parts.box->getHue() == 180);
    CHECK(parts.box->getCursorPos().x == cursor.x);
    CHECK(parts.box->getCursorPos().y == cursor.y);
    CHECK(sameColor(picker->getColor(), parts.box->getSelectedColor()));
    CHECK(picker->getColor().g > picker->getColor().r);
    CHECK(picker->getColor().b > picker->getColor().r);
    CHECK(counter.count == 0);
}

TEST_CASE("HSVColorPicker: a press and a drag in the box fire onColorChanged once per change")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.box != nullptr);
    scene.frame();

    const Color translucent(158, 206, 106, 77);
    picker->setColor(translucent);

    Counter counter;
    picker->onColorChanged.add(&counter, &Counter::changed);

    const Point a=parts.box->localPosToCanvas(Point(64, 32));
    const Point b=parts.box->localPosToCanvas(Point(96, 100));

    parts.box->onMouseClickLeft(a.x, a.y, true);
    CHECK(counter.count == 1);
    CHECK(parts.box->getCursorPos().x == 64);
    CHECK(parts.box->getCursorPos().y == 32);
    Color expected=parts.box->getSelectedColor();
    expected.a=77;
    CHECK(sameColor(picker->getColor(), expected));
    CHECK(parts.red->getText() == std::to_string(+expected.r));
    CHECK(sameColor(parts.after->getColor(), expected));

    // the same place again changes nothing
    parts.box->onMouseMoved(a.x, a.y, 0, 0);
    CHECK(counter.count == 1);

    parts.box->onMouseMoved(b.x, b.y, 0, 0);
    CHECK(counter.count == 2);
    expected=parts.box->getSelectedColor();
    expected.a=77;
    CHECK(sameColor(picker->getColor(), expected));
    Color underCursor=parts.box->getColorAtPos(96, 100);
    underCursor.a=77;
    CHECK(sameColor(expected, underCursor));

    parts.box->onMouseClickLeft(b.x, b.y, false);
    CHECK(counter.count == 2);
    CHECK(sameColor(picker->getColor(), expected));
}

TEST_CASE("HSVColorPicker: a press on the slider gives the box its hue and fires once")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.slider != nullptr);
    scene.frame();

    Counter counter;
    picker->onColorChanged.add(&counter, &Counter::changed);

    // the default colour is at full saturation and value: row 64 is cyan
    REQUIRE(parts.slider->getHeight() == 128);
    const Point at=parts.slider->localPosToCanvas(Point(16, 64));

    parts.slider->onMouseClickLeft(at.x, at.y, true);
    CHECK(counter.count == 1);
    CHECK(parts.slider->getSelectedDist() == 64);
    CHECK(parts.box->getHue() == 180);
    CHECK(sameColor(picker->getColor(), Color(0, 255, 255, 255)));
    CHECK(parts.red->getText() == "0");
    CHECK(parts.green->getText() == "255");
    CHECK(parts.blue->getText() == "255");

    parts.slider->onMouseClickLeft(at.x, at.y, false);
    CHECK(counter.count == 1);
}

TEST_CASE("HSVColorPicker: a value typed into R, G or B fires onColorChanged once per change")
{
    Scene scene;
    HSVColorPicker *picker=scene.canvas->newChild<HSVColorPicker>();
    Parts parts(picker);
    REQUIRE(parts.red != nullptr);
    scene.frame();

    picker->setColor(kSample);

    Counter counter;
    picker->onColorChanged.add(&counter, &Counter::changed);

    scene.type(parts.red, "7");
    CHECK(counter.count == 1);
    CHECK(sameColor(picker->getColor(), Color(7, 206, 106, 255)));
    scene.type(parts.green, "9");
    CHECK(counter.count == 2);
    scene.type(parts.blue, "5");
    CHECK(counter.count == 3);
    CHECK(sameColor(picker->getColor(), Color(7, 9, 5, 255)));
    CHECK(sameColor(parts.after->getColor(), Color(7, 9, 5, 255)));

    // the box and the slider show the typed colour
    const Hsv hsv=toHsv(Color(7, 9, 5, 255));
    CHECK(parts.box->getCursorPos().x == static_cast<int>(std::lround(hsv.s*parts.box->getWidth())));
    CHECK(parts.box->getCursorPos().y == static_cast<int>(std::lround((1-hsv.v)*parts.box->getHeight())));
    CHECK(parts.box->getHue() == static_cast<int>(hsv.h));

    // a value clamped to the channel it already has changes nothing, and
    // the field shows the clamped value
    scene.type(parts.red, "2550");
    CHECK(counter.count == 6);
    CHECK(sameColor(picker->getColor(), Color(255, 9, 5, 255)));
    CHECK(parts.red->getText() == "255");
}
