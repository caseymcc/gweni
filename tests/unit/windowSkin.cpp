/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin looks its renderers up by the control's exact type name,
 *  and Base::setSkin falls back to the parent types' renderers without a word.
 *  Its Window renderer was registered as "Window", so a WindowControl fell
 *  through ResizableControl to the Base renderer, which hides its slot, and a
 *  window drew no body. These cases go through the real TexturedSkin lookup.
 */
#include "headlessFixture.h"

#include <gweni/controls/windowControl.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! Records the slots drawn into as rounded rects and the styles they were shown with.
class RoundedRecorder: public test::StubRenderer
{
public:
    void drawRoundedRect(size_t primitiveId, Rect, float, float, Color, Color, Color, int) override
    {
        drawn.push_back(primitiveId);
    }

    void showPrimitive(int style, size_t primitiveId) override
    {
        shown.push_back({style, primitiveId});
    }

    bool drewInto(size_t id) const
    {
        for(size_t drawnId : drawn)
        {
            if(drawnId == id)
                return true;
        }
        return false;
    }

    int lastStyle(size_t id) const
    {
        int style=-1;

        for(const auto &entry : shown)
        {
            if(entry.second == id)
                style=entry.first;
        }
        return style;
    }

    std::vector<size_t> drawn;
    std::vector<std::pair<int, size_t>> shown;
};

}//namespace

// The skin is never init()'d: creating a renderer only looks up texture entry
// ids, which an unloaded skin answers with "none", and the SDF surfaces the
// converted renderers draw do not use the atlas.
TEST_CASE("the textured skin gives a WindowControl its Window renderer, which draws the window's body")
{
    const test::IdBankCounts baseline=test::IdBankCounts::now();

    RoundedRecorder renderer;
    {
        skin::textured::TexturedSkin skin(&renderer);
        controls::Canvas *canvas=new controls::Canvas(&skin);
        canvas->setSize(640, 480);

        controls::WindowControl *window=canvas->newChild<controls::WindowControl>();
        window->setSize(200, 150);
        window->setPos(20, 30);

        const skin::Control *control=window->getSkinControl();
        REQUIRE(control != nullptr);
        CHECK(dynamic_cast<const skin::textured::Window *>(control) != nullptr);

        canvas->updateCanvas();
        canvas->renderCanvas();

        const std::vector<size_t> &ids=window->getSkinControl()->getPrimitives();
        REQUIRE_FALSE(ids.empty());
        CHECK(renderer.drewInto(ids[0]));
        CHECK(renderer.lastStyle(ids[0]) == 4);

        delete canvas;
    }

    CHECK(test::IdBankCounts::now().livePrimitives() == baseline.livePrimitives());
    CHECK(test::IdBankCounts::now().liveTexts() == baseline.liveTexts());
}
