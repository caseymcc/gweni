/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The RichText unit tests' recorder: a StubRenderer that records each run and
 *  slot the textured skin's real RichText renderer draws, and a StubSkin that
 *  hands out that renderer for RichText (a StubControl for every other type).
 */
#ifndef _gweni_tests_unit_richTextFixture_h_
#define _gweni_tests_unit_richTextFixture_h_

#include "headlessFixture.h"

#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <memory>
#include <vector>

namespace test
{

//! Records the runs and slots the RichText renderer draws.
class DrawRecorder: public StubRenderer
{
public:
    struct TextDraw
    {
        size_t id;
        gweni::Font font;
        gweni::Point pos;
        int zIndex;
        gweni::String text;
        gweni::Color color;
    };

    struct SlotDraw
    {
        size_t id;
        bool rule;
        gweni::Rect rect;
        gweni::Color color;
        int zIndex;
    };

    void renderText(size_t textId, const gweni::Font &font, gweni::Point pos, int zIndex, const gweni::String &text) override
    {
        texts.push_back({ textId, font, pos, zIndex, text, drawColor });
        StubRenderer::renderText(textId, font, pos, zIndex, text);
    }

    void hideText(size_t textId) override
    {
        hiddenTexts.push_back(textId);
        StubRenderer::hideText(textId);
    }

    void drawRoundedRect(size_t primitiveId, gweni::Rect rect, float radius, float border, gweni::Color top, gweni::Color bottom,
        gweni::Color borderColor, int zIndex) override
    {
        slots.push_back({ primitiveId, false, rect, top, zIndex });
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawIcon(size_t primitiveId, gweni::Rect rect, int kind, int direction, gweni::Color color, int zIndex) override
    {
        CHECK(kind == 6);
        CHECK(direction == 0);
        slots.push_back({ primitiveId, true, rect, color, zIndex });
        StubRenderer::drawIcon(primitiveId, rect, kind, direction, color, zIndex);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        hiddenSlots.push_back(primitiveId);
        StubRenderer::hidePrimitive(primitiveId);
    }

    void clear()
    {
        texts.clear();
        slots.clear();
        hiddenTexts.clear();
        hiddenSlots.clear();
    }

    std::vector<TextDraw> texts;
    std::vector<SlotDraw> slots;
    std::vector<size_t> hiddenTexts;
    std::vector<size_t> hiddenSlots;
};

//! The textured skin's RichText renderer, a StubControl for the rest
class RichSkin: public StubSkin
{
public:
    explicit RichSkin(StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<gweni::skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "RichText")
            return std::make_unique<gweni::skin::textured::RichText>();
        return StubSkin::getControlRenderer(typeName);
    }
};

}//namespace test

#endif//_gweni_tests_unit_richTextFixture_h_
