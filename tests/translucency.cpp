/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Translucent slots composite in z order (#685): what is behind a translucent
 *  primitive slot, slots and text alike, shows through it whatever order the
 *  slots and runs were taken in, and what is in front of it blends over it.
 *
 *  Each case is a scene of items, rects and text runs at distinct z, drawn in
 *  one frame and compared with its reference: the same items drawn one per
 *  frame, back to front, into the same target with no clear between the frames
 *  (each frame hiding the others), so every item blends over exactly what is
 *  behind it. Each pixel must be the reference's within 2 per channel (the
 *  backends' bound), and drawn in today's order (setTranslucentSort(false),
 *  slot-id order with every glyph after every slot) some pixel must not be, but
 *  where that order is z order:
 *
 *    text behind       a translucent slot in front of text blends over the
 *                      glyphs, the run taken before the slot and after it, and
 *                      a run on an opaque control's fill under the slot;
 *    two translucent   two overlapping translucent slots at different z blend
 *                      in z order, the one taken later behind the earlier one;
 *    opaque behind     an opaque slot taken later, behind a translucent one,
 *                      shows through it;
 *    opaque in front   an opaque slot and opaque text in front of a
 *                      translucent slot and text still hide them;
 *    AA edges          an opaque rounded rect in front of a translucent panel,
 *                      taken before it, blends its corner arcs over the panel
 *                      (no fringe of the background round it), and text in
 *                      front of the panel does the same;
 *    image             an image whose texels are half transparent (its tint
 *                      opaque), taken first, shows the opaque slot taken
 *                      later behind it, and one whose texels are all opaque
 *                      or transparent draws in today's order;
 *    moved             a panel moved over a run, and a run moved under a
 *                      panel, from a frame where they did not overlap, blend
 *                      in the frame of the move (the overlap index kept up
 *                      to date from the slots and runs that changed);
 *    chunks            two translucent slots and an opaque one behind them in
 *                      different index chunks (setIndexChunkSlots(64)).
 *
 *  The renderer is shared with the other cases, so each case releases its ids
 *  and puts the chunk size back when it ends.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <algorithm>
#include <cstdlib>
#include <functional>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::renderer::SlotRenderer;

const Color background(30, 40, 200, 255);

struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }
};

Frame readFrame()
{
    Frame frame;

    gweni::test::readView(frame.pixels);
    return frame;
}

//! One rect or text run of a scene: draw() writes it (shown), hide() hides it.
struct Item
{
    int z;
    std::function<void(SlotRenderer &)> draw;
    std::function<void(SlotRenderer &)> hide;
};

//! The ids a case takes, released (and that flushed) when it ends, and the chunk
//! size put back.
struct Ids
{
    size_t chunkSlots=gweni::test::gpuRenderer().indexChunkSlots();
    std::vector<size_t> prims;
    std::vector<size_t> texts;

    ~Ids()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        for(size_t id:prims)
            renderer.releasePrimitive(id);
        for(size_t id:texts)
            renderer.releaseTextPrimitive(id);
        renderer.setIndexChunkSlots(chunkSlots);
        renderer.begin();
        renderer.end();
    }

    //! \a count primitive ids, lowest first
    std::vector<size_t> primitives(size_t count)
    {
        std::vector<size_t> ids(count);

        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
        std::sort(ids.begin(), ids.end());
        prims.insert(prims.end(), ids.begin(), ids.end());
        return ids;
    }

    size_t text()
    {
        size_t id=0;

        gweni::test::gpuRenderer().generateTextPrimitive(id);
        texts.push_back(id);
        return id;
    }
};

Item rect(size_t id, const Rect &r, const Color &color, int z)
{
    return Item{z,
        [=](SlotRenderer &renderer) { renderer.setDrawColor(color); renderer.drawFilledRect(id, r, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item rounded(size_t id, const Rect &r, float radius, const Color &color, int z)
{
    return Item{z,
        [=](SlotRenderer &renderer) { renderer.drawRoundedRect(id, r, radius, 0.0f, color, color, color, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item text(size_t id, const Point &pos, const gweni::String &string, const Color &color, int z, float size=32.0f)
{
    Font font=gweni::test::openSans();

    font.size=size;
    return Item{z,
        [=](SlotRenderer &renderer)
        {
            renderer.setDrawColor(color);
            renderer.renderText(id, font, pos, z, string);
            renderer.showText(id);
        },
        [=](SlotRenderer &renderer) { renderer.hideText(id); }};
}

//! The scene drawn in one frame
Frame drawScene(const std::vector<Item> &items)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    gweni::test::clearView(background);
    renderer.begin();
    for(const Item &item:items)
        item.draw(renderer);
    renderer.end();
    return readFrame();
}

//! The scene drawn one item a frame, back to front, with no clear in between
Frame drawReference(std::vector<Item> items)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    std::stable_sort(items.begin(), items.end(), [](const Item &a, const Item &b) { return a.z < b.z; });
    gweni::test::clearView(background);
    for(size_t i=0; i<items.size(); ++i)
    {
        renderer.begin();
        for(size_t j=0; j<items.size(); ++j)
        {
            if(j == i)
                items[j].draw(renderer);
            else
                items[j].hide(renderer);
        }
        renderer.end();
    }
    return readFrame();
}

//! The pixels of \a frame more than 2 off \a reference in a channel, the first
//! of them logged
size_t badPixels(const Frame &frame, const Frame &reference)
{
    const Point size=gweni::test::viewSize();
    size_t bad=0;

    for(int y=0; y<size.y; ++y)
    {
        for(int x=0; x<size.x; ++x)
        {
            const Color a=frame.at(x, y);
            const Color b=reference.at(x, y);

            if((std::abs(a.r-b.r) > 2) || (std::abs(a.g-b.g) > 2) || (std::abs(a.b-b.b) > 2))
            {
                if(bad == 0)
                    MESSAGE("first at ", x, ",", y, ": got ", int(a.r), ",", int(a.g), ",", int(a.b),
                        " want ", int(b.r), ",", int(b.g), ",", int(b.b));
                ++bad;
            }
        }
    }
    return bad;
}

//! Every pixel of the scene's frame within 2 per channel of its reference's,
//! and, unless \a oldOrderDraws, some pixel not without the sorted pass
//! (setTranslucentSort(false), today's order)
void checkScene(const std::vector<Item> &items, bool oldOrderDraws=false)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Frame reference=drawReference(items);

    renderer.setTranslucentSort(false);

    const Frame oldOrder=drawScene(items);

    renderer.setTranslucentSort(true);

    const size_t oldBad=badPixels(oldOrder, reference);

    if(oldOrderDraws)
        CHECK(oldBad == 0);
    else
        CHECK(oldBad > 0);

    // last, so a case reads the sorted frame back
    const Frame frame=drawScene(items);

    CHECK(badPixels(frame, reference) == 0);
}

const Color panel(240, 230, 40, 128);
const Color ink(250, 250, 250, 255);
const Rect panelRect(20, 20, 200, 80);

}//namespace

TEST_CASE("Translucency: a translucent slot in front of text blends over the glyphs")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);

    SUBCASE("the run taken before the slot")
    {
        const size_t run=ids.text();

        checkScene({text(run, Point(30, 30), "HHHH", ink, 1), rect(prims[0], panelRect, panel, 5)});
    }
    SUBCASE("the run taken after the slot")
    {
        const size_t slot=prims[0];
        const size_t run=ids.text();

        checkScene({rect(slot, panelRect, panel, 5), text(run, Point(30, 30), "HHHH", ink, 1)});
    }
    SUBCASE("a control's text on its opaque fill under the slot")
    {
        const size_t run=ids.text();

        checkScene({rect(prims[1], Rect(10, 25, 180, 60), Color(60, 160, 60, 255), 1),
            text(run, Point(30, 30), "HHHH", ink, 2), rect(prims[0], panelRect, panel, 5)});
    }
}

TEST_CASE("Translucency: two translucent slots blend in z order whatever their ids")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);

    SUBCASE("the later slot behind")
    {
        checkScene({rect(prims[0], Rect(20, 20, 120, 80), Color(250, 20, 20, 128), 5),
            rect(prims[1], Rect(80, 50, 120, 80), Color(20, 250, 20, 128), 3)});
    }
    SUBCASE("the later slot in front")
    {
        // slot-id order is z order here, so today's order draws it too
        checkScene({rect(prims[0], Rect(20, 20, 120, 80), Color(250, 20, 20, 128), 3),
            rect(prims[1], Rect(80, 50, 120, 80), Color(20, 250, 20, 128), 5)}, true);
    }
}

TEST_CASE("Translucency: an opaque slot taken later behind a translucent one shows through it")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);

    checkScene({rect(prims[0], panelRect, panel, 5), rect(prims[1], Rect(60, 40, 60, 40), Color(200, 30, 200, 255), 2)});
}

TEST_CASE("Translucency: an opaque slot or opaque text in front still hides what is behind it")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(3);
    const size_t behindRun=ids.text();
    const size_t frontRun=ids.text();
    const Rect cover(40, 25, 70, 70);
    const Color coverColor(10, 200, 220, 255);

    checkScene({text(behindRun, Point(30, 30), "HHHH", ink, 1), rect(prims[0], panelRect, panel, 3),
        rect(prims[1], cover, coverColor, 5), text(frontRun, Point(130, 40), "HH", Color(0, 0, 0, 255), 7),
        rect(prims[2], Rect(125, 35, 90, 50), Color(250, 20, 20, 128), 6)});

    // the opaque rect's middle is its colour alone
    const Frame frame=readFrame();
    const Color middle=frame.at(cover.x+cover.w/2, cover.y+cover.h/2);

    CHECK(static_cast<int>(middle.r) == coverColor.r);
    CHECK(static_cast<int>(middle.g) == coverColor.g);
    CHECK(static_cast<int>(middle.b) == coverColor.b);
}

TEST_CASE("Translucency: an opaque rounded rect and text in front of a translucent panel blend their edges over it")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);
    const size_t run=ids.text();

    // the button taken first, so it is drawn before the panel today
    checkScene({rounded(prims[0], Rect(40, 40, 100, 40), 12.0f, Color(90, 90, 230, 255), 5),
        rect(prims[1], panelRect, panel, 3), text(run, Point(150, 40), "Ag", Color(20, 20, 20, 255), 4)});
}

TEST_CASE("Translucency: the same pixels with the slots in different index chunks")
{
    Ids ids;

    gweni::test::gpuRenderer().setIndexChunkSlots(64);

    const std::vector<size_t> prims=ids.primitives(140);

    // the slot taken first in front, the later ones behind, each in a chunk of its own
    checkScene({rect(prims[0], Rect(20, 20, 120, 80), Color(250, 20, 20, 128), 5),
        rect(prims[70], Rect(80, 50, 120, 80), Color(20, 250, 20, 128), 3),
        rect(prims[139], Rect(60, 40, 60, 80), Color(200, 30, 200, 255), 1)});
}

TEST_CASE("Translucency: an image with half-transparent texels shows the slot taken later behind it")
{
    Ids ids;
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const std::vector<size_t> prims=ids.primitives(2);
    const int side=16;
    std::vector<uint8_t> texels(side*side*4);
    gweni::Texture image;

    SUBCASE("half transparent")
    {
        for(size_t i=0; i<texels.size(); i+=4)
        {
            texels[i]=250;
            texels[i+1]=250;
            texels[i+2]=30;
            texels[i+3]=128;
        }
    }
    SUBCASE("opaque and transparent")
    {
        for(size_t i=0; i<texels.size(); i+=4)
        {
            texels[i]=250;
            texels[i+1]=250;
            texels[i+2]=30;
            // the edge between them left of the slot behind, which sees only
            // transparent texels (discarded)
            texels[i+3]=((i/4)%side < 2)?255:0;
        }
    }
    REQUIRE((renderer.loadTexturePixels(image, side, side, side*4, texels.data()) == gweni::Texture::Status::Loaded));

    const bool translucent=texels[3] == 128;
    const Item picture{5,
        [&](SlotRenderer &renderer)
        {
            renderer.setDrawColor(Color(255, 255, 255, 255));
            renderer.drawTexturedRect(image, prims[0], Rect(20, 20, 128, 128), 5, 0.0f, 0.0f, 1.0f, 1.0f);
        },
        [&](SlotRenderer &renderer) { renderer.hidePrimitive(prims[0]); }};

    checkScene({picture, rect(prims[1], Rect(60, 40, 60, 60), Color(200, 30, 200, 255), 2)}, !translucent);
    renderer.hidePrimitive(prims[0]);
    renderer.begin();
    renderer.end();
    renderer.freeTexture(image);
}

TEST_CASE("Translucency: a panel moved over text and text moved under a panel blend in the next frame")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);
    const size_t run=ids.text();
    const Item backdrop=rect(prims[1], Rect(10, 120, 200, 60), Color(60, 160, 60, 255), 1);

    SUBCASE("the panel moved")
    {
        const std::vector<Item> before{backdrop, text(run, Point(30, 130), "HHHH", ink, 2),
            rect(prims[0], Rect(240, 20, 150, 80), panel, 5)};
        const std::vector<Item> after{backdrop, text(run, Point(30, 130), "HHHH", ink, 2),
            rect(prims[0], Rect(20, 110, 150, 80), panel, 5)};
        const Frame reference=drawReference(after);

        drawScene(before);
        CHECK(badPixels(drawScene(after), reference) == 0);
    }
    SUBCASE("the run moved")
    {
        const std::vector<Item> before{backdrop, text(run, Point(250, 30), "HHHH", ink, 2),
            rect(prims[0], Rect(20, 110, 150, 80), panel, 5)};
        const std::vector<Item> after{backdrop, text(run, Point(30, 130), "HHHH", ink, 2),
            rect(prims[0], Rect(20, 110, 150, 80), panel, 5)};
        const Frame reference=drawReference(after);

        drawScene(before);
        CHECK(badPixels(drawScene(after), reference) == 0);
    }
}
