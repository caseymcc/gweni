/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Slider renderer draws a track plus one tick per notch
 *  position, and the notch count can change after its primitives were
 *  generated. Its update() then has to resize its own slot list to 1+ticks and
 *  draw only into slots it owns; requiredPrimitives() only sizes the first
 *  generate().
 *
 *  It runs the textured skin's real Slider/SliderBar renderers on the
 *  StubRenderer. A renderer's update() may grow or shrink its m_primitives
 *  itself.
 */
#include "headlessFixture.h"

#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/verticalSlider.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <set>

using namespace gweni;

namespace
{

//! Records which slots were drawn into.
class DrawRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color, int) override
    {
        drawn.push_back(primitiveId);
    }

    std::vector<size_t> drawn;
};

//! The textured skin's Slider and SliderBar renderers, a StubControl for the
//! rest. The derived slider types get nothing, so they resolve to Slider
//! through the parent chain as they do in the textured skin.
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
        if(typeName == "HorizontalSlider" || typeName == "VerticalSlider")
            return nullptr;
        return StubSkin::getControlRenderer(typeName);
    }
};

template<typename SliderType>
struct SliderScene
{
    SliderScene()
    {
        canvas=new controls::Canvas(&skin);
        slider=canvas->newChild<SliderType>();
        slider->setSize(150, 20);
    }

    SliderScene(const SliderScene &)=delete;
    SliderScene &operator=(const SliderScene &)=delete;

    ~SliderScene()
    {
        delete canvas;
    }

    void frame()
    {
        renderer.drawn.clear();
        renderer.events.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    const std::vector<size_t> &sliderIds() const
    {
        return slider->getSkinControl()->getPrimitives();
    }

    std::set<size_t> ownedIds() const
    {
        std::set<size_t> ids(sliderIds().begin(), sliderIds().end());
        const std::vector<size_t> &bar=slider->getSliderBar()->getSkinControl()->getPrimitives();
        ids.insert(bar.begin(), bar.end());
        return ids;
    }

    //! Every slot drawn this frame belongs to the slider or its thumb.
    bool drewOnlyOwnSlots() const
    {
        const std::set<size_t> owned=ownedIds();

        for(size_t id : renderer.drawn)
        {
            if(owned.count(id) == 0)
                return false;
        }
        return true;
    }

    DrawRecorder renderer;
    SliderSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::Slider *slider=nullptr;
};

template<typename SliderType>
void checkNotchCountChanges(SliderScene<SliderType> &scene)
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    // not clamped: the track only, though the notch count defaults to 5
    scene.frame();
    REQUIRE(scene.slider->getSkinControl() != nullptr);
    CHECK(scene.sliderIds().size() == 1);
    CHECK(scene.drewOnlyOwnSlots());

    // clamped to 4 notches: 5 tick positions (both ends included)
    scene.slider->setNotchCount(4);
    scene.slider->setClampToNotches(true);
    scene.frame();
    CHECK(scene.sliderIds().size() == 1+5);
    CHECK(scene.renderer.drawn.size() == 1+5);
    CHECK(scene.drewOnlyOwnSlots());

    // more notches after the first generate: the list grows, track slot kept
    const size_t track=scene.sliderIds()[0];
    scene.slider->setNotchCount(10);
    scene.frame();
    CHECK(scene.sliderIds().size() == 1+11);
    CHECK(scene.sliderIds()[0] == track);
    CHECK(scene.renderer.drawn.size() == 1+11);
    CHECK(scene.drewOnlyOwnSlots());
    CHECK_FALSE(test::hasDuplicates(scene.sliderIds()));

    // fewer: the extra slots go back to the bank
    scene.slider->setNotchCount(2);
    scene.frame();
    CHECK(scene.sliderIds().size() == 1+3);
    CHECK(scene.sliderIds()[0] == track);
    CHECK(scene.renderer.drawn.size() == 1+3);
    CHECK(scene.drewOnlyOwnSlots());

    // no notches drawn once it stops clamping, and nothing left over
    scene.slider->setClampToNotches(false);
    scene.frame();
    CHECK(scene.sliderIds().size() == 1);
    CHECK(scene.drewOnlyOwnSlots());

    // the slider's 1 slot and the thumb's 1 are all that stay live
    CHECK(test::IdBankCounts::now().livePrimitives()-baseline.livePrimitives() == 2);
    CHECK(scene.renderer.doubleAcquired == 0);
    CHECK(scene.renderer.doubleReleased == 0);

    // an unchanged count is not a state change: nothing is re-emitted
    scene.frame();
    scene.slider->setNotchCount(scene.slider->getNotchCount());
    scene.frame();
    CHECK(scene.renderer.drawn.empty());
}

}//namespace

TEST_CASE("a HorizontalSlider's notch ticks follow setNotchCount after the first frame")
{
    SliderScene<controls::HorizontalSlider> scene;
    checkNotchCountChanges(scene);
}

TEST_CASE("a VerticalSlider's notch ticks follow setNotchCount after the first frame")
{
    SliderScene<controls::VerticalSlider> scene;
    checkNotchCountChanges(scene);
}
