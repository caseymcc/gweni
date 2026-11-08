/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A scroll bar's geometry comes from its skin's Metrics.ScrollBarThickness
 *  (T), not from its arrow buttons' empty captions. In a ScrollControl that
 *  overflows both ways, on a real Canvas with the StubSkin, for T 12 and 20
 *  (the second given by a skin swap, so the bar reads T when it lays out): the
 *  vertical bar is T wide and the horizontal one T high, each the length the
 *  ScrollControl gives it; each arrow button is TxT at its end of the bar;
 *  the thumb is viewable/content of the track (the bar less 2T) long, at least
 *  T/2, and lies on the track, never over a button, at scrolled 0, 0.5 and 1,
 *  at the track's start and end at 0 and 1; and the hit test finds the arrow
 *  button at every one of its pixels, so a click on an arrow nudges and never
 *  grabs the thumb. The thumb is hidden when the track is no longer than it:
 *  content that fits (bars kept shown) or a bar too short for its track to
 *  pass T/2. A TabControl's overflow buttons and its tab-list button are the
 *  14x14 its constructor asks for. Every frame lays out in one pass with no layout request.
 */
#include "headlessFixture.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/horizontalScrollBar.h>
#include <gweni/controls/scrollBarBar.h>
#include <gweni/controls/scrollBarButton.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/tabControl.h>
#include <gweni/controls/verticalScrollBar.h>
#include <gweni/inputHandler.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <vector>

using namespace gweni;

namespace
{

//! The parts of one scroll bar, found among its children.
struct BarParts
{
    controls::BaseScrollBar *bar=nullptr;
    std::vector<controls::internal::ScrollBarButton *> buttons;
    controls::internal::ScrollBarBar *thumb=nullptr;
};

template<typename BarType>
BarParts findBar(controls::Base *scrollControl)
{
    BarParts parts;

    for(controls::Base *child : scrollControl->getActualChildren())
    {
        if(BarType *bar=gweni_cast<BarType>(child))
            parts.bar=bar;
    }
    REQUIRE(parts.bar != nullptr);

    for(controls::Base *child : parts.bar->getChildren())
    {
        if(auto *button=gweni_cast<controls::internal::ScrollBarButton>(child))
            parts.buttons.push_back(button);
        else if(auto *thumb=gweni_cast<controls::internal::ScrollBarBar>(child))
            parts.thumb=thumb;
    }
    REQUIRE(parts.buttons.size() == 2);
    REQUIRE(parts.thumb != nullptr);
    return parts;
}

struct Scene
{
    test::TwoSkins skins;
    controls::Canvas *canvas=nullptr;
    controls::ScrollControl *scroll=nullptr;
    controls::Button *content=nullptr;

    int requests=0;
    int maxPasses=0;
    bool renderLayout=false;

    //! A \p width x \p height ScrollControl at (20, 30) holding \p contentW x
    //! \p contentH, its bars hidden when not needed unless \p keepBars.
    Scene(int thickness, int contentW, int contentH, int width=200, int height=150,
        bool keepBars=false)
    {
        skins.skinA.Metrics.ScrollBarThickness=thickness;
        canvas=new controls::Canvas(&skins.skinA);
        canvas->setSize(400, 400);

        scroll=canvas->newChild<controls::ScrollControl>();
        scroll->setPos(20, 30);
        scroll->setSize(width, height);
        scroll->setAutoHideBars(!keepBars);

        content=scroll->newChild<controls::Button>();
        content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        content->setSize(contentW, contentH);
        frame();
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        KeyboardFocus=nullptr;
        MouseFocus=nullptr;
        delete canvas;
    }

    void frame()
    {
        canvas->updateCanvas();
        canvas->renderCanvas();

        const controls::LayoutStats &layout=canvas->getLayoutStats();

        requests+=layout.requests;
        maxPasses=std::max(maxPasses, layout.passes);
        renderLayout=renderLayout || layout.renderLayout;
    }

    void checkOnePass() const
    {
        CHECK(requests == 0);
        CHECK(maxPasses <= 1);
        CHECK(!renderLayout);
    }

    BarParts vertical() { return findBar<controls::VerticalScrollBar>(scroll); }
    BarParts horizontal() { return findBar<controls::HorizontalScrollBar>(scroll); }
};

//! The bar's thickness, its buttons and its thumb for thickness \p t, the bar
//! \p vertical or not. \p viewable and \p content are what the bar ranges.
void checkBar(const BarParts &parts, bool vertical, int t, int viewable, int content)
{
    const Rect &bar=parts.bar->getBounds();
    const int length=vertical?bar.h:bar.w;
    const int track=length-2*t;

    INFO("vertical ", vertical, ", T ", t, ", bar ", bar.x, ",", bar.y, " ", bar.w, "x", bar.h);
    CHECK((vertical?bar.w:bar.h) == t);
    CHECK(parts.bar->getPreferredSize().width == (vertical?t:2*t));
    CHECK(parts.bar->getPreferredSize().height == (vertical?2*t:t));

    // TxT at the two ends
    CHECK((parts.buttons[0]->getBounds() == Rect(0, 0, t, t)));
    CHECK((parts.buttons[1]->getBounds() ==
        (vertical?Rect(0, length-t, t, t):Rect(length-t, 0, t, t))));

    // viewable/content of the track, at least T/2
    const Rect &thumb=parts.thumb->getBounds();
    const int thumbLength=vertical?thumb.h:thumb.w;
    const int expected=std::max(t/2, static_cast<int>(static_cast<float>(viewable)/content*track));

    CHECK(thumbLength == expected);
    CHECK((vertical?thumb.w:thumb.h) == t);
    CHECK((vertical?thumb.x:thumb.y) == 0);
    CHECK_FALSE(parts.thumb->hidden());
}

//! The thumb lies on the track, between the buttons, at \p amount of its travel.
void checkThumbOnTrack(const BarParts &parts, bool vertical, int t, float amount)
{
    const Rect &bar=parts.bar->getBounds();
    const Rect &thumb=parts.thumb->getBounds();
    const int length=vertical?bar.h:bar.w;
    const int start=vertical?thumb.y:thumb.x;
    const int size=vertical?thumb.h:thumb.w;

    INFO("vertical ", vertical, ", T ", t, ", amount ", amount, ", thumb at ", start, " size ", size);
    CHECK(start >= t);
    CHECK(start+size <= length-t);
    CHECK(start == t+static_cast<int>(amount*(length-2*t-size)));

    if(amount == 0.f)
        CHECK(start == t);
    if(amount == 1.f)
        CHECK(start+size == length-t);
}

//! The hit test finds each arrow button at every one of its pixels.
void checkButtonsHit(controls::Canvas *canvas, const BarParts &parts)
{
    for(controls::internal::ScrollBarButton *button : parts.buttons)
    {
        const Point origin=button->localPosToCanvas(Point(0, 0));
        int missed=0;

        for(int y=0; y < button->getHeight(); ++y)
        {
            for(int x=0; x < button->getWidth(); ++x)
            {
                if(canvas->getControlAt(origin.x+x, origin.y+y) != button)
                    ++missed;
            }
        }
        INFO("button at ", origin.x, ",", origin.y);
        CHECK(missed == 0);
    }
}

}//namespace

TEST_CASE("A scroll bar is its skin's thickness, with square arrows and the thumb between them")
{
    for(const int t : {12, 20})
    {
        // the skin's thickness given at construction, or by a skin swap after
        // the first layout at the other one
        for(const bool swapped : {false, true})
        {
            INFO("T ", t, ", through a skin swap: ", swapped);

            Scene scene(swapped?(t == 12?20:12):t, 600, 500);

            if(swapped)
            {
                scene.skins.skinB.Metrics.ScrollBarThickness=t;
                scene.canvas->setSkin(&scene.skins.skinB, true);
                scene.frame();
                scene.frame();
            }

            BarParts vertical=scene.vertical();
            BarParts horizontal=scene.horizontal();
            const Margin &inset=scene.scroll->getInnerPanel()->getMargin();
            const Rect &bounds=scene.scroll->getBounds();
            // both bars shown: each takes T from the other's viewport
            const int viewportW=bounds.w-inset.left-inset.right-t;
            const int viewportH=bounds.h-inset.top-inset.bottom-t;

            REQUIRE(vertical.bar->visible());
            REQUIRE(horizontal.bar->visible());

            // along the ScrollControl's right edge and bottom, beside each other
            CHECK((vertical.bar->getBounds() == Rect(bounds.w-t, 0, t, bounds.h)));
            CHECK((horizontal.bar->getBounds() == Rect(0, bounds.h-t, bounds.w-t, t)));

            checkBar(vertical, true, t, viewportH, 500);
            checkBar(horizontal, false, t, viewportW, 600);

            for(const float amount : {0.f, 0.5f, 1.f})
            {
                vertical.bar->setScrolledAmount(amount, true);
                horizontal.bar->setScrolledAmount(amount, true);
                scene.frame();

                checkThumbOnTrack(vertical, true, t, amount);
                checkThumbOnTrack(horizontal, false, t, amount);
                checkButtonsHit(scene.canvas, vertical);
                checkButtonsHit(scene.canvas, horizontal);
            }

            scene.checkOnePass();
        }
    }
}

TEST_CASE("A scroll bar's thumb is hidden when the track is no longer than it")
{
    const int t=12;

    SUBCASE("content that fits, the bars kept shown")
    {
        Scene scene(t, 50, 50, 200, 150, true);

        for(const BarParts &parts : {scene.vertical(), scene.horizontal()})
        {
            REQUIRE(parts.bar->visible());
            CHECK(parts.thumb->hidden());
        }
        scene.checkOnePass();
    }

    SUBCASE("a bar too short for its track to pass T/2")
    {
        // the vertical bar 2T+T/2 long: a track of T/2, the thumb's least
        Scene scene(t, 50, 500, 200, 2*t+t/2);
        BarParts vertical=scene.vertical();

        REQUIRE(vertical.bar->visible());
        CHECK(vertical.bar->getHeight() == 2*t+t/2);
        CHECK(vertical.thumb->hidden());

        // one unit longer, and it shows
        scene.scroll->setSize(200, 2*t+t/2+1);
        scene.frame();
        CHECK_FALSE(vertical.thumb->hidden());
        scene.checkOnePass();
    }
}

TEST_CASE("A TabControl's overflow and tab-list buttons are the 14x14 its constructor asks for")
{
    test::StubRenderer renderer;
    test::StubSkin skin(&renderer);
    controls::Canvas *canvas=new controls::Canvas(&skin);

    canvas->setSize(400, 300);

    controls::TabControl *tabs=canvas->newChild<controls::TabControl>();

    tabs->setSize(300, 200);
    tabs->addPage("One");
    tabs->addPage("Two");
    canvas->updateCanvas();
    canvas->renderCanvas();

    int found=0;

    for(controls::Base *child : tabs->getActualChildren())
    {
        if(auto *button=gweni_cast<controls::internal::ScrollBarButton>(child))
        {
            CHECK(button->getWidth() == 14);
            CHECK(button->getHeight() == 14);
            ++found;
        }
    }
    // the left and right scroll buttons and the tab-list button
    CHECK(found == 3);

    delete canvas;
}
