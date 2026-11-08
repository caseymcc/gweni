/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Slider::setValueFormat: a slider showing its value as text (imgui's
 *  SliderFloat). It pins the format check (one conversion of a double, a
 *  stray %s or %d refused, the format kept), the strings the value is shown
 *  as across formats, ranges and notch clamps, and that the text is set, and
 *  StateChange_Text raised, only when the formatted string changes: a
 *  setFloatValue, a key step or a thumb drag that keeps the string marks the
 *  text with nothing and draws no text. With no format no text id is ever
 *  taken, and clearing the format gives its id back. The text is numbered and
 *  drawn above the thumb; a vertical slider's text column is the wider of the
 *  formatted min and max, so its width never changes with the value, and a
 *  horizontal slider is at least the text's line high.
 *
 *  It runs the textured skin's real Slider, SliderBar and Text renderers on a
 *  StubRenderer that records the rounded rects and text runs it is given.
 */
#include "headlessFixture.h"

#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

using namespace gweni;

namespace
{

//! Records the rounded rects and text runs drawn, with their z.
class DrawRecorder: public test::StubRenderer
{
public:
    struct Rect
    {
        size_t id;
        int z;
    };

    struct Run
    {
        size_t id;
        int z;
        String text;
    };

    void drawRoundedRect(size_t primitiveId, gweni::Rect, float, float, Color, Color, Color, int zIndex) override
    {
        rects.push_back({primitiveId, zIndex});
    }

    void renderText(size_t textId, const Font &, Point, int zIndex, const String &text) override
    {
        runs.push_back({textId, zIndex, text});
    }

    std::vector<Rect> rects;
    std::vector<Run> runs;
};

//! The textured skin's Slider, SliderBar and Text renderers, a StubControl for
//! the rest. The derived slider types resolve to Slider through the parent
//! chain, as they do in the textured skin.
class SliderSkin: public test::StubSkin
{
public:
    explicit SliderSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "Slider")
            return std::make_unique<skin::textured::Slider>();
        if(typeName == "SliderBar")
            return std::make_unique<skin::textured::SliderBar>();
        if(typeName == "Text")
            return std::make_unique<skin::textured::Text>();
        if(typeName == "HorizontalSlider" || typeName == "VerticalSlider")
            return nullptr;
        return StubSkin::getControlRenderer(typeName);
    }
};

template<typename SliderType>
struct Scene
{
    explicit Scene(int w=200, int h=20)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(400, 300);
        slider=canvas->newChild<SliderType>();
        slider->setPos(10, 10);
        slider->setSize(w, h);
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
    }

    void frame()
    {
        renderer.rects.clear();
        renderer.runs.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    //! Whether the value text has a text change pending.
    bool textMarked()
    {
        controls::internal::Text *text=slider->getValueTextControl();
        return text && (text->getStateChange() & controls::StateChange_Text);
    }

    DrawRecorder renderer;
    SliderSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Slider *slider=nullptr;
};

}//namespace

TEST_CASE("Slider value format: one conversion of a double, anything else refused")
{
    using controls::Slider;

    for(const char *format : {"%f", "%.0f", "%.2f", "%.0f %%", "%%%.1f", "x%-8.3gy", "%+.1f", "%e", "%10.2E",
            "%G", "%a", "%#08.3f", "Value: %.1f units"})
        CHECK_MESSAGE(Slider::validValueFormat(format), format);

    for(const char *format : {"", "abc", "%%", "100%", "%", "%.", "%s", "%d", "%i", "%x", "%c", "%p", "%n",
            "%f %f", "%.1f%s", "%*f", "%.*f", "%100f", "%.100f", "%lf", "%Lf", "%hf", "%.2f %"})
        CHECK_FALSE_MESSAGE(Slider::validValueFormat(format), format);

    // a NUL inside the string does not end the scan early
    CHECK_FALSE(Slider::validValueFormat(String("%.1f\0%s", 7)));
    CHECK_FALSE(Slider::validValueFormat(String("%\0", 2)));

    CHECK(Slider::formatValue("%.0f", 25.4f) == "25");
    CHECK(Slider::formatValue("%.2f", 0.5f) == "0.50");
    CHECK(Slider::formatValue("%.0f %%", 60.0f) == "60 %");
    CHECK(Slider::formatValue("%+.1f", 10.0f) == "+10.0");
    CHECK(Slider::formatValue("%+.1f", -2.5f) == "-2.5");
    CHECK(Slider::formatValue("[%6.1f]", 3.0f) == "[   3.0]");
    CHECK(Slider::formatValue("%s", 1.0f).empty());
    // the longest a format can ask for is still formatted whole
    CHECK(Slider::formatValue("%99.99f", 1.0f).size() == 101);

    Scene<controls::HorizontalSlider> scene;

    CHECK(scene.slider->setValueFormat("%.1f"));
    CHECK_FALSE(scene.slider->setValueFormat("%s"));
    CHECK_FALSE(scene.slider->setValueFormat("%d items"));
    CHECK(scene.slider->getValueFormat() == "%.1f");
}

TEST_CASE("Slider value text: the value as formatted, across ranges and notch clamps")
{
    Scene<controls::HorizontalSlider> scene;
    controls::Slider *slider=scene.slider;

    slider->setRange(0, 100);
    slider->setFloatValue(25);
    REQUIRE(slider->setValueFormat("%.0f"));
    CHECK(slider->getValueText() == "25");

    slider->setFloatValue(72.6f);
    CHECK(slider->getValueText() == "73");

    // clamped to the range
    slider->setFloatValue(250);
    CHECK(slider->getValueText() == "100");
    slider->setFloatValue(-3);
    CHECK(slider->getValueText() == "0");

    // a new range shows the same position in it
    slider->setFloatValue(50);
    REQUIRE(slider->setValueFormat("%.2f"));
    CHECK(slider->getValueText() == "50.00");
    slider->setRange(0, 1);
    CHECK(slider->getValueText() == "0.50");
    slider->setRange(-50, 50);
    REQUIRE(slider->setValueFormat("%+.1f"));
    CHECK(slider->getValueText() == "+0.0");
    slider->setFloatValue(10);
    CHECK(slider->getValueText() == "+10.0");

    // clamped to 10 notches over 0..10: the value snaps to whole numbers
    slider->setRange(0, 10);
    slider->setNotchCount(10);
    slider->setClampToNotches(true);
    REQUIRE(slider->setValueFormat("%.1f"));
    slider->setFloatValue(4.3f);
    CHECK(slider->getValueText() == "4.0");
    slider->setFloatValue(6.6f);
    CHECK(slider->getValueText() == "7.0");

    REQUIRE(slider->setValueFormat("%.0f %%"));
    slider->setRange(0, 100);
    slider->setFloatValue(60);
    CHECK(slider->getValueText() == "60 %");

    scene.frame();
    REQUIRE_FALSE(scene.renderer.runs.empty());
    CHECK(scene.renderer.runs.back().text == "60 %");
}

TEST_CASE("Slider value text: set and marked only when the formatted string changes")
{
    Scene<controls::HorizontalSlider> scene;
    controls::Slider *slider=scene.slider;

    slider->setRange(0, 100);
    slider->setFloatValue(25);
    REQUIRE(slider->setValueFormat("%.0f"));
    scene.frame();
    scene.frame();
    REQUIRE(scene.renderer.runs.empty());

    struct Step
    {
        float value;
        const char *shown;
        bool changes;
    };

    // values that keep and change the "%.0f" string, in turn
    const Step steps[]=
    {
        {25.2f, "25", false}, {25.4f, "25", false}, {25.6f, "26", true}, {26.1f, "26", false},
        {26.0f, "26", false}, {80.0f, "80", true}, {80.3f, "80", false}, {79.5f, "80", false},
        {79.4f, "79", true},
    };

    for(const Step &step : steps)
    {
        CAPTURE(step.value);
        slider->setFloatValue(step.value);
        CHECK(slider->getValueText() == step.shown);
        CHECK(scene.textMarked() == step.changes);

        scene.frame();

        const size_t textId=slider->getValueTextControl()->getSkinControl()->getPrimitives()[0];
        size_t runs=0;

        for(const DrawRecorder::Run &run : scene.renderer.runs)
        {
            if(run.id == textId)
            {
                ++runs;
                CHECK(run.text == step.shown);
            }
        }

        CHECK(runs == (step.changes?1:0));
    }

    // a key step changes the string, and the same value again marks nothing
    slider->onKeyRight(true);
    CHECK(slider->getValueText() == "80");
    CHECK(scene.textMarked());
    scene.frame();
    slider->setFloatValue(80);
    CHECK_FALSE(scene.textMarked());
    scene.frame();
    CHECK(scene.renderer.runs.empty());
}

TEST_CASE("Slider value text: a thumb drag that keeps the string draws no text")
{
    // 0..10 over a 215-unit slider, 15 of them the thumb: one unit of travel
    // is 0.05 of the value, so "%.0f" changes once in about 20 units
    Scene<controls::HorizontalSlider> scene(215, 20);
    controls::Slider *slider=scene.slider;

    slider->setRange(0, 10);
    slider->setFloatValue(5);
    REQUIRE(slider->setValueFormat("%.0f"));
    scene.frame();
    scene.frame();

    controls::Base *thumb=slider->getSliderBar();
    const Point at=thumb->localPosToCanvas();
    const int x=at.x+thumb->getWidth()/2;
    const int y=at.y+thumb->getHeight()/2;

    scene.canvas->inputMouseMoved(x, y, 0, 0);
    input::onMouseClicked(scene.canvas, 0, true);
    scene.frame();

    const size_t textId=slider->getValueTextControl()->getSkinControl()->getPrimitives()[0];
    const int startX=thumb->getX();
    int kept=0;
    int changed=0;

    for(int dx=1; dx<=24; ++dx)
    {
        CAPTURE(dx);
        const String shown=slider->getValueText();

        scene.canvas->inputMouseMoved(x+dx, y, 1, 0);

        const bool changes=(slider->getValueText() != shown);

        CHECK(scene.textMarked() == changes);
        scene.frame();

        bool drewText=false;

        for(const DrawRecorder::Run &run : scene.renderer.runs)
            drewText|=(run.id == textId);
        CHECK(drewText == changes);

        if(changes)
            ++changed;
        else
            ++kept;
    }

    // the thumb went along (to within the unit its value rounds it back by)
    CHECK(thumb->getX() >= startX+23);
    CHECK(kept > 20);
    CHECK(changed == 1);
    CHECK(slider->getValueText() == "6");
    input::onMouseClicked(scene.canvas, 0, false);
    scene.frame();
}

TEST_CASE("Slider value text: no text id with no format, given back when it is cleared")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    {
        Scene<controls::HorizontalSlider> scene;

        scene.slider->setFloatValue(0.5f);
        scene.frame();
        CHECK(scene.slider->getValueTextControl() == nullptr);
        CHECK(scene.slider->getValueText().empty());
        CHECK(scene.renderer.liveTexts.empty());
        CHECK(scene.renderer.runs.empty());

        // a refused format takes nothing either
        CHECK_FALSE(scene.slider->setValueFormat("%s"));
        scene.frame();
        CHECK(scene.renderer.liveTexts.empty());

        REQUIRE(scene.slider->setValueFormat("%.2f"));
        scene.frame();
        CHECK(scene.renderer.liveTexts.size() == 1);

        // another format keeps the same text id
        const size_t textId=*scene.renderer.liveTexts.begin();

        REQUIRE(scene.slider->setValueFormat("%.1f"));
        scene.frame();
        CHECK(scene.renderer.liveTexts == std::set<size_t>{textId});
        CHECK(scene.slider->getValueText() == "0.5");

        REQUIRE(scene.slider->setValueFormat(""));
        CHECK(scene.slider->getValueTextControl() == nullptr);
        scene.frame();
        CHECK(scene.renderer.liveTexts.empty());
        CHECK(scene.renderer.doubleAcquired == 0);
        CHECK(scene.renderer.doubleReleased == 0);
    }

    CHECK(test::IdBankCounts::now().liveTexts()-baseline.liveTexts() == 0);
}

TEST_CASE("Slider value text: numbered and drawn above the thumb")
{
    Scene<controls::HorizontalSlider> scene;
    controls::Slider *slider=scene.slider;

    slider->setFloatValue(0.5f);
    REQUIRE(slider->setValueFormat("%.2f"));
    scene.frame();

    controls::Base *thumb=slider->getSliderBar();
    controls::Base *text=slider->getValueTextControl();

    REQUIRE(text != nullptr);
    CHECK(thumb->getZIndex() > slider->getZIndex());
    CHECK(text->getZIndex() > thumb->getZIndex());

    const size_t thumbId=thumb->getSkinControl()->getPrimitives()[0];
    const size_t textId=text->getSkinControl()->getPrimitives()[0];
    int thumbZ=-1;
    int textZ=-1;

    for(const DrawRecorder::Rect &rect : scene.renderer.rects)
    {
        if(rect.id == thumbId)
            thumbZ=rect.z;
    }

    for(const DrawRecorder::Run &run : scene.renderer.runs)
    {
        if(run.id == textId)
            textZ=run.z;
    }

    REQUIRE(thumbZ >= 0);
    REQUIRE(textZ >= 0);
    CHECK(textZ > thumbZ);

    // centred in the slider
    CHECK(text->getX() == (slider->getWidth()-text->getWidth())/2);
    CHECK(text->getY() == (slider->getHeight()-text->getHeight())/2);
}

TEST_CASE("Slider value text: a horizontal slider is at least the text's line high")
{
    Scene<controls::HorizontalSlider> scene(200, 4);
    controls::Slider *slider=scene.slider;

    scene.frame();
    CHECK(slider->getHeight() == 4);

    REQUIRE(slider->setValueFormat("%.2f"));
    scene.frame();

    const int lineHeight=slider->getValueTextControl()->getHeight();

    CHECK(lineHeight > 4);
    CHECK(slider->getHeight() == lineHeight+2);
    CHECK(slider->getWidth() == 200);

    // a format with a longer string does not widen it
    REQUIRE(slider->setValueFormat("A very long value of %.2f units"));
    scene.frame();
    CHECK(slider->getWidth() == 200);

    // and no format gives the asked-for height back
    REQUIRE(slider->setValueFormat(""));
    scene.frame();
    CHECK(slider->getHeight() == 4);
}

TEST_CASE("Slider value text: a vertical slider's text column is fixed over the whole range")
{
    Scene<controls::VerticalSlider> scene(20, 200);
    controls::Slider *slider=scene.slider;

    slider->setRange(0, 100);
    scene.frame();
    CHECK(slider->getWidth() == 20);
    CHECK(slider->getTrackBounds().w == 20);
    CHECK(slider->getValueTextColumn() == 0);

    REQUIRE(slider->setValueFormat("%.0f"));
    scene.frame();

    controls::Base *text=slider->getValueTextControl();
    const Font &font=slider->getValueTextControl()->getFont();
    const int column=std::max(scene.renderer.measureText(font, "0").x, scene.renderer.measureText(font, "100").x);
    const int width=slider->getWidth();

    REQUIRE(column > 0);
    CHECK(slider->getValueTextColumn() == column);
    CHECK(width == 20+controls::Slider::ValueTextGap+column);
    CHECK(slider->getTrackBounds().w == 20);
    CHECK(slider->getSliderBar()->getWidth() == 20);
    CHECK(text->getX() == 20+controls::Slider::ValueTextGap);
    CHECK(slider->getHeight() == 200);

    for(float value : {0.0f, 3.0f, 50.0f, 99.0f, 100.0f, 7.0f})
    {
        CAPTURE(value);
        slider->setFloatValue(value);
        scene.frame();
        CHECK(slider->getWidth() == width);
        CHECK(slider->getValueTextColumn() == column);
        CHECK(slider->getTrackBounds().w == 20);
        CHECK(text->getX() == 20+controls::Slider::ValueTextGap);
        CHECK(text->getWidth() <= column);
        CHECK(text->getY() == (slider->getHeight()-text->getHeight())/2);
    }

    // a wider range widens the column once
    slider->setRange(-1000, 1000);
    scene.frame();
    CHECK(slider->getValueTextColumn() == scene.renderer.measureText(font, "-1000").x);
    CHECK(slider->getWidth() == 20+controls::Slider::ValueTextGap+slider->getValueTextColumn());
}

TEST_CASE("Slider value text: a disabled slider's text takes the disabled colour")
{
    Scene<controls::HorizontalSlider> scene;
    controls::Slider *slider=scene.slider;
    const skin::Base *skin=slider->getSkin();

    REQUIRE(slider->setValueFormat("%.1f"));
    CHECK((slider->getValueTextControl()->getTextColor() == skin->Colors.Label.Default));

    slider->setDisabled(true);
    CHECK((slider->getValueTextControl()->getTextColor() == skin->Colors.Button.Disabled));

    slider->setDisabled(false);
    CHECK((slider->getValueTextControl()->getTextColor() == skin->Colors.Label.Default));

    // a format set on a disabled slider starts in the disabled colour
    slider->setDisabled(true);
    REQUIRE(slider->setValueFormat(""));
    REQUIRE(slider->setValueFormat("%.0f"));
    CHECK((slider->getValueTextControl()->getTextColor() == skin->Colors.Button.Disabled));
}
