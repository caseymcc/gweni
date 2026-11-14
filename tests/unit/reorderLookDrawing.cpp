/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured reorder looks honour shouldDrawBackground independently of
 *  control visibility. Cancellation during layout can clear their slots in
 *  the same frame without moving or hiding the controls, which would dirty
 *  an already-arranged list. Taking a look into use again paints its held
 *  slot, including when a new drag starts before the posted hide runs.
 */
#include "headlessFixture.h"

#include <gweni/controls/reorderable.h>
#include <gweni/skins/textured/texturedControls.h>

#include <doctest/doctest.h>

#include <memory>
#include <set>

using namespace gweni;

namespace
{

class LookRenderer: public test::StubRenderer
{
public:
    std::set<size_t> hiddenSlots;

    void hidePrimitive(size_t id) override
    {
        hiddenSlots.insert(id);
        StubRenderer::hidePrimitive(id);
    }

    void drawRoundedRect(size_t id, Rect rect, float radius, float border,
        Color top, Color bottom, Color edge, int z) override
    {
        hiddenSlots.erase(id);
        StubRenderer::drawRoundedRect(id, rect, radius, border, top, bottom, edge, z);
    }
};

}//namespace

TEST_CASE("Reorder looks: cancelling their drawing clears the slot and reuse paints it again")
{
    LookRenderer renderer;
    test::StubSkin skin(&renderer);
    controls::Canvas canvas(&skin);
    controls::Base *control=canvas.newChild<controls::internal::DropIndicator>();
    control->setSize(100, 20);
    std::unique_ptr<skin::Control> painter;

    SUBCASE("DropIndicator") { painter=std::make_unique<skin::textured::DropIndicator>(); }
    SUBCASE("DragSourceVeil") { painter=std::make_unique<skin::textured::DragSourceVeil>(); }

    painter->generate(&renderer, control);
    REQUIRE(renderer.livePrimitives.size() == 1);
    const size_t slot=*renderer.livePrimitives.begin();
    painter->update(&renderer, control);
    CHECK(renderer.hiddenSlots.empty());

    control->setShouldDrawBackground(false);
    painter->update(&renderer, control);
    CHECK(control->visible());
    CHECK(renderer.hiddenSlots.count(slot) == 1);

    control->setShouldDrawBackground(true);
    painter->update(&renderer, control);
    CHECK(renderer.hiddenSlots.empty());
    CHECK(renderer.livePrimitives.size() == 1);
    painter->remove(&renderer, control);
}
