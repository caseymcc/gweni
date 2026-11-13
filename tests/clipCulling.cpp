/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's clip cull (setClipCulling): a primitive quad or a text run
 *  drawn wholly outside the clip box it is drawn with is left out of the draws,
 *  which would only rasterise it for the fragment shaders' clip test to discard
 *  every pixel of it. Drawn offscreen and read back, on every backend.
 *
 *  A slot drawn in its box, then wholly out of it, then in again is drawn, cut
 *  (FrameStats::primitiveSlotsCulled one up, primitiveSlotsDrawn one down, no
 *  pixel of it) and drawn again as at first; hidden while culled it is not
 *  counted, and shown while still culled it stays undrawn and counted; shown
 *  after a draw whose box covers it (as a control shown again is drawn in the
 *  region it is in now) it is drawn. A line whose round cap and anti-aliased
 *  rim reach into the box past its segment's end is not culled, and draws what
 *  it draws with the cull off. A box that only touches a slot's edge culls
 *  it, and one reaching a pixel over that edge, on each side, does not (the
 *  shaders' half-open test at pixel centres). A slot culled and drawn again
 *  uploads what it uploads with the cull off: the cull leaves its vertexes as
 *  drawn, so only a moved clip box is sent. A settled frame builds no range. A text run
 *  (FrameStats::textRunsCulled) follows the same transitions, its glyph slots'
 *  clip boxes written as without the cull, and a culled run is out of its
 *  font's draw list (textRunInDrawList), so the shader is not left to discard
 *  it. A box that cuts through a run on any side, keeping only its ink's
 *  outermost column or row, leaves it drawn and in the draw list (the run's
 *  bounds against the box's edge). Every culled frame is the one the cull off
 *  draws, pixel for pixel.
 *
 *  A ScrollControl of Button rows on a textured-skin canvas: a row scrolled
 *  wholly out of the viewport has its slot and its label's run culled (the run
 *  out of the draw list), and scrolled back it is drawn again, the frame the one before the scroll.
 *
 *  The renderer is shared with the other cases, so each case puts the clip
 *  region back to the whole view and the cull as it found it when it ends
 *  (Guard).
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/skin.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <utility>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(0, 0, 255, 255);
const Color white(255, 255, 255, 255);

SlotRenderer &renderer()
{
    return gweni::test::gpuRenderer();
}

Rect wholeView()
{
    return Rect(Point(0, 0), gweni::test::viewSize());
}

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    //! Pixels that are not the background.
    int drawn() const
    {
        int count=0;

        for(size_t i=0; i+3 < pixels.size(); i+=4)
            if((pixels[i] != background.r) || (pixels[i+1] != background.g) || (pixels[i+2] != background.b))
                ++count;
        return count;
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    gweni::test::clearView(background);

    renderer().begin();
    draw(renderer());
    renderer().end();

    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

Frame emptyFrame()
{
    return drawFrame([](SlotRenderer &) {});
}

//! Turns the cull on, and puts the clip region back to the whole view and the
//! cull as it found it.
struct Guard
{
    Guard()
        : culling(renderer().clipCulling())
    {
        renderer().setClipCulling(true);
    }

    ~Guard()
    {
        renderer().setClipRegion(wholeView());
        renderer().setClipCulling(culling);
    }

    bool culling;
};

//! Primitive ids for one test case, released (and that flushed) when it ends.
struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            renderer().generatePrimitive(id);
        std::sort(ids.begin(), ids.end());
    }

    ~Primitives()
    {
        for(size_t id:ids)
            renderer().releasePrimitive(id);
        renderer().begin();
        renderer().end();
    }

    std::vector<size_t> ids;
};

//! A text id for one test case, released (and that flushed) when it ends.
struct TextId
{
    TextId() { renderer().generateTextPrimitive(id); }

    ~TextId()
    {
        renderer().releaseTextPrimitive(id);
        renderer().begin();
        renderer().end();
    }

    size_t id=0;
};

const SlotRenderer::FrameStats &stats()
{
    return renderer().getFrameStats();
}

const Rect rect(40, 30, 100, 50);
const Rect inside(20, 20, 200, 100);    // covers rect
const Rect outside(300, 30, 100, 50);   // misses it

}//namespace

TEST_CASE("Clip cull: a slot drawn out of its box is culled, and drawn again back in it")
{
    Guard guard;
    Primitives prims(1);
    const size_t id=prims.ids[0];

    emptyFrame();
    const size_t culledBefore=stats().primitiveSlotsCulled;
    const Frame empty=emptyFrame();

    auto draw=[&](const Rect &clip)
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.drawFilledRect(id, rect, 1);
            });
    };

    const Frame in=draw(inside);
    const size_t drawnIn=stats().primitiveSlotsDrawn;

    CHECK(in.drawn() == rect.w*rect.h);
    CHECK_FALSE(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore);

    const Frame out=draw(outside);

    CHECK(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore+1);
    CHECK(stats().primitiveSlotsDrawn < drawnIn);
    CHECK(out.pixels == empty.pixels);

    // drawn again culled where it was: nothing written, nothing to rebuild
    draw(outside);
    CHECK(stats().primitiveSlotsWritten == 0);
    CHECK(stats().primitiveRangeBuilds == 0);

    const Frame back=draw(inside);

    CHECK_FALSE(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore);
    CHECK(stats().primitiveSlotsDrawn == drawnIn);
    CHECK(back.pixels == in.pixels);

    // a settled frame writes nothing and builds no range
    emptyFrame();
    CHECK(stats().primitiveSlotsWritten == 0);
    CHECK(stats().primitiveRangeBuilds == 0);

    // with the cull off, the slot out of its box is drawn, to the same pixels
    renderer().setClipCulling(false);
    const Frame unculled=draw(outside);

    CHECK_FALSE(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore);
    CHECK(unculled.pixels == out.pixels);
}

TEST_CASE("Clip cull: a slot touching its box's edge is culled, and one overlapping it by a pixel is not")
{
    Guard guard;
    Primitives prims(1);
    const size_t id=prims.ids[0];

    // rect covers device px [40,140) x [30,80); each box touches one of its
    // edges from outside, then reaches one pixel row or column over it
    struct Side
    {
        const char *name;
        Rect touching;
        Rect overlapping;
        int overlapPixels;
    };
    const Side sides[]={
        {"left", Rect(0, 0, 40, 160), Rect(0, 0, 41, 160), rect.h},
        {"right", Rect(140, 0, 100, 160), Rect(139, 0, 101, 160), rect.h},
        {"top", Rect(0, 0, 300, 30), Rect(0, 0, 300, 31), rect.w},
        {"bottom", Rect(0, 80, 300, 80), Rect(0, 79, 300, 81), rect.w},
    };

    auto draw=[&](const Rect &clip)
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.drawFilledRect(id, rect, 1);
            });
    };

    for(const Side &side:sides)
    {
        INFO("side: ", std::string(side.name));

        draw(side.touching);
        CHECK(renderer().primitiveCulled(id));

        const Frame overlap=draw(side.overlapping);

        CHECK_FALSE(renderer().primitiveCulled(id));
        CHECK(overlap.drawn() == side.overlapPixels);

        renderer().setClipCulling(false);
        CHECK(draw(side.overlapping).pixels == overlap.pixels);
        renderer().setClipCulling(true);
    }
}

TEST_CASE("Clip cull: a slot culled and drawn again uploads what it uploads with the cull off")
{
    Guard guard;
    Primitives prims(1);
    const size_t id=prims.ids[0];

    // the slot's quad stays where it is and only its box moves, as a resize
    // moves a viewport past a row: the vertexes are the same bytes every frame,
    // so only the clip box is sent, whether the cull culls the slot or not
    auto draw=[&](const Rect &clip)
    {
        drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.drawFilledRect(id, rect, 1);
            });
        return std::make_pair(stats().uploadCalls, stats().uploadBytes);
    };
    auto run=[&]()
    {
        std::vector<std::pair<size_t, size_t>> uploads;

        draw(inside);
        uploads.push_back(draw(outside));
        uploads.push_back(draw(outside));
        uploads.push_back(draw(inside));
        return uploads;
    };

    const auto culled=run();

    renderer().setClipCulling(false);
    const auto unculled=run();

    CHECK(culled == unculled);
    // drawn again culled where it was, nothing is sent
    CHECK(culled[1] == std::make_pair(size_t(0), size_t(0)));
}

TEST_CASE("Clip cull: a culled slot hidden is not counted, and shown while still culled stays undrawn")
{
    Guard guard;
    Primitives prims(1);
    const size_t id=prims.ids[0];

    emptyFrame();
    const size_t culledBefore=stats().primitiveSlotsCulled;
    const Frame empty=emptyFrame();

    drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.setClipRegion(outside);
            renderer.drawFilledRect(id, rect, 1);
        });
    CHECK(stats().primitiveSlotsCulled == culledBefore+1);

    const Frame hidden=drawFrame([&](SlotRenderer &renderer) { renderer.hidePrimitive(id); });

    CHECK(stats().primitiveSlotsCulled == culledBefore);
    CHECK(hidden.pixels == empty.pixels);

    const size_t drawnHidden=stats().primitiveSlotsDrawn;
    const Frame shown=drawFrame([&](SlotRenderer &renderer) { renderer.showPrimitive(1, id); });

    CHECK(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore+1);
    CHECK(stats().primitiveSlotsDrawn == drawnHidden);
    CHECK(shown.pixels == empty.pixels);
}

TEST_CASE("Clip cull: a culled slot hidden, then drawn in a box that covers it and shown, is drawn")
{
    Guard guard;
    Primitives prims(1);
    const size_t id=prims.ids[0];

    const Frame in=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.setClipRegion(inside);
            renderer.drawFilledRect(id, rect, 1);
        });

    REQUIRE(in.drawn() == rect.w*rect.h);

    emptyFrame();
    const size_t culledBefore=stats().primitiveSlotsCulled;

    drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setClipRegion(outside);
            renderer.drawFilledRect(id, rect, 1);
        });
    drawFrame([&](SlotRenderer &renderer) { renderer.hidePrimitive(id); });
    CHECK(stats().primitiveSlotsCulled == culledBefore);

    // as paintSurface shows a control again: drawn in the region it is in now,
    // then shown
    const Frame shown=drawFrame([&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(white);
            renderer.setClipRegion(inside);
            renderer.drawFilledRect(id, rect, 1);
            renderer.showPrimitive(1, id);
        });

    CHECK_FALSE(renderer().primitiveCulled(id));
    CHECK(stats().primitiveSlotsCulled == culledBefore);
    CHECK(shown.pixels == in.pixels);
}

TEST_CASE("Clip cull: a line whose cap reaches into its box past the segment's end is not culled")
{
    Guard guard;
    Primitives prims(2);

    // a horizontal segment ending at x 100 and a box from x 101: its round cap
    // and anti-aliased rim cover the box's first column, the segment does not.
    // The second segment, its quad wholly left of the box, is culled.
    auto draw=[&]
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(Rect(101, 0, 200, 200));
                renderer.drawLine(prims.ids[0], gweni::PointF(10.0f, 50.0f), gweni::PointF(100.0f, 50.0f), 4.0f, 1);
                renderer.drawLine(prims.ids[1], gweni::PointF(10.0f, 100.0f), gweni::PointF(80.0f, 100.0f), 4.0f, 1);
            });
    };

    const Frame culled=draw();

    CHECK_FALSE(renderer().primitiveCulled(prims.ids[0]));
    CHECK(renderer().primitiveCulled(prims.ids[1]));
    CHECK(culled.drawn() > 0);

    renderer().setClipCulling(false);
    const Frame unculled=draw();

    CHECK(unculled.pixels == culled.pixels);
}

TEST_CASE("Clip cull: a text run out of its box is culled, and drawn again back in it")
{
    Guard guard;
    TextId text;
    const gweni::String string="Culled text";
    const Point pos(40, 40);

    emptyFrame();
    const size_t culledBefore=stats().textRunsCulled;
    const Frame empty=emptyFrame();

    auto draw=[&](const Rect &clip)
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.renderText(text.id, gweni::test::openSans(), pos, 1, string);
            });
    };

    const Frame in=draw(inside);

    CHECK(in.drawn() > 0);
    CHECK_FALSE(renderer().textRunCulled(text.id));
    CHECK(renderer().textRunInDrawList(text.id));
    CHECK(stats().textRunsCulled == culledBefore);

    const Frame out=draw(outside);

    CHECK(renderer().textRunCulled(text.id));
    CHECK_FALSE(renderer().textRunInDrawList(text.id));
    CHECK(stats().textRunsCulled == culledBefore+1);
    // the glyphs' clip boxes are written as without the cull
    CHECK(stats().textSlotsWritten > 0);
    CHECK(out.pixels == empty.pixels);

    // laid out again where it was: nothing written
    draw(outside);
    CHECK(stats().textSlotsWritten == 0);

    const Frame back=draw(inside);

    CHECK_FALSE(renderer().textRunCulled(text.id));
    CHECK(renderer().textRunInDrawList(text.id));
    CHECK(stats().textRunsCulled == culledBefore);
    CHECK(back.pixels == in.pixels);

    renderer().setClipCulling(false);
    const Frame unculled=draw(outside);

    CHECK_FALSE(renderer().textRunCulled(text.id));
    CHECK(renderer().textRunInDrawList(text.id));
    CHECK(unculled.pixels == out.pixels);
}

TEST_CASE("Clip cull: a culled text run hidden is not counted, shown still culled stays undrawn, and laid out in a covering box is drawn")
{
    Guard guard;
    TextId text;
    const gweni::String string="Culled text";
    const Point pos(40, 40);

    auto layout=[&](SlotRenderer &renderer, const Rect &clip)
    {
        renderer.setDrawColor(white);
        renderer.setClipRegion(clip);
        renderer.renderText(text.id, gweni::test::openSans(), pos, 1, string);
    };

    const Frame in=drawFrame([&](SlotRenderer &renderer) { layout(renderer, inside); });

    REQUIRE(in.drawn() > 0);

    emptyFrame();
    const size_t culledBefore=stats().textRunsCulled;
    const Frame empty=drawFrame([&](SlotRenderer &renderer) { renderer.hideText(text.id); });

    drawFrame([&](SlotRenderer &renderer) { renderer.showText(text.id); layout(renderer, outside); });
    CHECK(stats().textRunsCulled == culledBefore+1);

    const Frame hidden=drawFrame([&](SlotRenderer &renderer) { renderer.hideText(text.id); });

    CHECK(stats().textRunsCulled == culledBefore);
    CHECK(hidden.pixels == empty.pixels);

    const Frame shown=drawFrame([&](SlotRenderer &renderer) { renderer.showText(text.id); });

    CHECK(renderer().textRunCulled(text.id));
    CHECK_FALSE(renderer().textRunInDrawList(text.id));
    CHECK(stats().textRunsCulled == culledBefore+1);
    CHECK(shown.pixels == empty.pixels);

    drawFrame([&](SlotRenderer &renderer) { renderer.hideText(text.id); });

    // shown again as a Text control is: laid out in the region it is in now
    const Frame back=drawFrame([&](SlotRenderer &renderer)
        {
            layout(renderer, inside);
            renderer.showText(text.id);
        });

    CHECK_FALSE(renderer().textRunCulled(text.id));
    CHECK(renderer().textRunInDrawList(text.id));
    CHECK(stats().textRunsCulled == culledBefore);
    CHECK(back.pixels == in.pixels);
}

TEST_CASE("Clip cull: a row scrolled out of a ScrollControl is culled, and drawn again scrolled back")
{
    Guard guard;
    const gweni::Point viewSize=gweni::test::viewSize();
    gweni::skin::textured::TexturedSkin *skin=new gweni::skin::textured::TexturedSkin(&renderer());

    REQUIRE(skin->init("DefaultSkin.json"));
    skin->setDefaultFont("OpenSans.ttf", 11);

    gweni::controls::Canvas *canvas=new gweni::controls::Canvas(skin);

    canvas->setSize(viewSize.x, viewSize.y);

    gweni::controls::ScrollControl *scroll=canvas->newChild<gweni::controls::ScrollControl>();

    scroll->setAlignment(gweni::Alignment::Absolute);
    scroll->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    scroll->setBounds(20, 40, 200, 100);
    scroll->setScroll(false, true);

    std::vector<gweni::controls::Button *> rows;

    for(int i=0; i<20; ++i)
    {
        gweni::controls::Button *button=scroll->newChild<gweni::controls::Button>();

        button->setText("Row");
        button->setBounds(0, i*30, 160, 28);
        rows.push_back(button);
    }

    auto frame=[&]
    {
        gweni::test::clearView(background);
        canvas->updateCanvas();
        canvas->renderCanvas();

        Frame shot;

        gweni::test::readView(shot.pixels);
        return shot;
    };
    // the row's background slot and its label's run
    auto ids=[&](gweni::controls::Base *control, std::vector<size_t> &primitives, std::vector<size_t> &texts)
    {
        std::vector<gweni::controls::Base *> stack{control};

        while(!stack.empty())
        {
            gweni::controls::Base *at=stack.back();

            stack.pop_back();
            if(const gweni::skin::Control *skinControl=at->getSkinControl())
                skinControl->appendIds(primitives, texts);
            for(gweni::controls::Base *child:at->getChildren())
                stack.push_back(child);
        }
    };

    // the ids are taken as the first frame draws
    frame();

    std::vector<size_t> rowPrimitives, rowTexts;

    ids(rows[0], rowPrimitives, rowTexts);
    REQUIRE_FALSE(rowPrimitives.empty());
    REQUIRE_FALSE(rowTexts.empty());

    const Frame top=frame();
    const size_t slotsCulled=stats().primitiveSlotsCulled;
    const size_t runsCulled=stats().textRunsCulled;

    // the first row is in the viewport; the last ones are below it, culled
    CHECK_FALSE(renderer().primitiveCulled(rowPrimitives[0]));
    CHECK_FALSE(renderer().textRunCulled(rowTexts[0]));
    CHECK(renderer().textRunInDrawList(rowTexts[0]));
    CHECK(slotsCulled > 0);

    // the first rows wholly above the viewport
    scroll->setVerticalScrollOffset(150);
    frame();
    frame();

    CHECK(renderer().primitiveCulled(rowPrimitives[0]));
    CHECK(renderer().textRunCulled(rowTexts[0]));
    CHECK_FALSE(renderer().textRunInDrawList(rowTexts[0]));

    scroll->setVerticalScrollOffset(0);
    frame();
    const Frame back=frame();

    CHECK_FALSE(renderer().primitiveCulled(rowPrimitives[0]));
    CHECK_FALSE(renderer().textRunCulled(rowTexts[0]));
    CHECK(renderer().textRunInDrawList(rowTexts[0]));
    CHECK(stats().primitiveSlotsCulled == slotsCulled);
    CHECK(stats().textRunsCulled == runsCulled);
    CHECK(back.pixels == top.pixels);

    delete canvas;
    delete skin;
    renderer().begin();
    renderer().end();
}

TEST_CASE("Clip cull: a text run its box cuts through on any side is not culled")
{
    Guard guard;
    TextId text;
    const gweni::String string="Culled text";
    const Point pos(40, 40);

    auto draw=[&](const Rect &clip)
    {
        return drawFrame([&](SlotRenderer &renderer)
            {
                renderer.setDrawColor(white);
                renderer.setClipRegion(clip);
                renderer.renderText(text.id, gweni::test::openSans(), pos, 1, string);
            });
    };

    // the ink's extent, from the run drawn whole
    const Frame whole=draw(wholeView());
    const gweni::Point view=gweni::test::viewSize();
    int x0=view.x, x1=-1, y0=view.y, y1=-1;

    for(int y=0; y<view.y; ++y)
        for(int x=0; x<view.x; ++x)
        {
            const size_t i=(size_t(y)*view.x+x)*4;

            if((whole.pixels[i] != background.r) || (whole.pixels[i+1] != background.g) || (whole.pixels[i+2] != background.b))
            {
                x0=std::min(x0, x); x1=std::max(x1, x);
                y0=std::min(y0, y); y1=std::max(y1, y);
            }
        }
    REQUIRE(x1 > x0+2);
    REQUIRE(y1 > y0+2);

    // each box keeps the ink's outermost column or row on one side alone
    struct Side
    {
        const char *name;
        Rect box;
    };
    const Side sides[]={
        {"left", Rect(x1, 0, view.x-x1, view.y)},
        {"right", Rect(0, 0, x0+1, view.y)},
        {"top", Rect(0, y1, view.x, view.y-y1)},
        {"bottom", Rect(0, 0, view.x, y0+1)},
    };

    for(const Side &side:sides)
    {
        INFO("side: ", std::string(side.name));

        const Frame cut=draw(side.box);

        CHECK_FALSE(renderer().textRunCulled(text.id));
        CHECK(renderer().textRunInDrawList(text.id));
        CHECK(cut.drawn() > 0);

        renderer().setClipCulling(false);
        CHECK(draw(side.box).pixels == cut.pixels);
        renderer().setClipCulling(true);
    }
}
