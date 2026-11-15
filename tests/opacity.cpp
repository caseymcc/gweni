/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Control opacity in SlotRenderer (#686): renderer::Base::setOpacity(surface,
 *  text), which the render drain sets to each control's effective levels, is
 *  multiplied into the alpha of every colour a draw packs, the text level for
 *  glyphs, icons (style 5) and images (style 2), the surface level for the rest.
 *  On every backend:
 *
 *    surface level  a fill at alpha 0xe0 drawn at 0.5 is the same frame, pixel
 *                   for pixel, as the fill drawn at alpha 0x70 at 1, and that
 *                   pixel is the 0x70 blend over the background within 2; so
 *                   is a rounded rect, a line, a checker (opaque at 1) and a
 *                   bilinear rect; glyphs and an icon drawn with it stay as
 *                   at 1.
 *    text level     glyphs, an icon and an image drawn at a text level of 0.5
 *                   are half-way between the background and themselves at 1
 *                   (within 2), a fill drawn with them stays as at 1; and a
 *                   run drawn again at another level writes its glyph slots.
 *    level 0        a fill, a rounded rect, an icon, an image and a run at
 *                   level 0, taken before an opaque fill behind them, leave
 *                   the frame the fill alone gives: nothing drawn, no depth.
 *                   The run at level 0 is in no draw.
 *    per element    a panel at 0.5 and a button on it at 0.5 (the button's
 *                   slot taken first) blend in z order: the panel's fill shows
 *                   through the button, as the two drawn one a frame back to
 *                   front; and a checker at 0.5 (drawn opaque at 1) shows the
 *                   fill behind it, taken after it.
 *    near 0         a fill and a grid at a level above 0 that rounds their
 *                   every alpha to 0 (0.001; a 0x20 fill at 0.01), and such a
 *                   fill shown again, leave the frame the fill behind gives:
 *                   those two styles write depth at alpha 0, so they are hidden.
 *    spans          a run of colour spans at a text level of 0.5 is half-way to
 *                   the background, and a span's new colour at that level keeps it.
 *    level 1        with every level at 1 the slots' bytes are those of a
 *                   renderer that never had a level set.
 *
 *  The renderer is shared with the other cases, so each case puts the levels
 *  back to 1 and releases its ids when it ends.
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
using gweni::PointF;
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

//! The ids a case takes, released (and that flushed) when it ends, with the
//! levels put back to 1.
struct Ids
{
    std::vector<size_t> prims;
    std::vector<size_t> texts;

    ~Ids()
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setOpacity(1.0f, 1.0f);
        for(size_t id:prims)
            renderer.releasePrimitive(id);
        for(size_t id:texts)
            renderer.releaseTextPrimitive(id);
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

//! One draw of a scene at the levels it is made at: draw() writes it (shown) at
//! them, hide() hides it.
struct Item
{
    int z;
    float surface;
    float text;
    std::function<void(SlotRenderer &)> draw;
    std::function<void(SlotRenderer &)> hide;
};

Item atLevels(Item item, float surface, float text)
{
    item.surface=surface;
    item.text=text;
    return item;
}

Item fill(size_t id, const Rect &r, const Color &color, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer) { renderer.setDrawColor(color); renderer.drawFilledRect(id, r, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item rounded(size_t id, const Rect &r, const Color &color, const Color &border, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer) { renderer.drawRoundedRect(id, r, 6.0f, 2.0f, color, color, border, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item line(size_t id, PointF p0, PointF p1, const Color &color, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer) { renderer.setDrawColor(color); renderer.drawLine(id, p0, p1, 3.0f, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item checker(size_t id, const Rect &r, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer)
        {
            renderer.drawCheckerRect(id, r, z, Color(250, 20, 20, 128), Color(255, 255, 255, 255),
                Color(90, 90, 90, 255), 0.0f, 0.0f, Color(0, 0, 0, 0));
        },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item bilinear(size_t id, const Rect &r, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer)
        {
            renderer.drawBilinearRect(id, r, z, Color(255, 0, 0, 255), Color(0, 255, 0, 255),
                Color(0, 0, 255, 255), Color(255, 255, 255, 255), 0.0f, 0.0f, Color(0, 0, 0, 0));
        },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item icon(size_t id, const Rect &r, const Color &color, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer) { renderer.drawIcon(id, r, 0, 0, color, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item image(size_t id, const gweni::Texture &texture, const Rect &r, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=, &texture](SlotRenderer &renderer)
        {
            renderer.setDrawColor(Color(255, 255, 255, 255));
            renderer.drawTexturedRect(texture, id, r, z, 0.0f, 0.0f, 1.0f, 1.0f);
        },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

Item text(size_t id, const Point &pos, const gweni::String &string, const Color &color, int z)
{
    Font font=gweni::test::openSans();

    font.size=32.0f;
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer)
        {
            renderer.setDrawColor(color);
            renderer.renderText(id, font, pos, z, string);
            renderer.showText(id);
        },
        [=](SlotRenderer &renderer) { renderer.hideText(id); }};
}

Item grid(size_t id, const Rect &r, const Color &minor, const Color &major, int z)
{
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer) { renderer.drawGrid(id, r, PointF(0.0f, 0.0f), 8.0f, 4, minor, major, z); },
        [=](SlotRenderer &renderer) { renderer.hidePrimitive(id); }};
}

//! A run of three colour spans (red, green, blue words) with the draw colour
//! between them
Item spanText(size_t id, const Point &pos, const std::vector<gweni::renderer::ColorSpan> &spans, int z)
{
    Font font=gweni::test::openSans();

    font.size=32.0f;
    return Item{z, 1.0f, 1.0f,
        [=](SlotRenderer &renderer)
        {
            renderer.setDrawColor(Color(250, 250, 250, 255));
            renderer.renderText(id, font, pos, z, "HHH HHH HHH", spans);
            renderer.showText(id);
        },
        [=](SlotRenderer &renderer) { renderer.hideText(id); }};
}

void drawItem(SlotRenderer &renderer, const Item &item)
{
    renderer.setOpacity(item.surface, item.text);
    item.draw(renderer);
    renderer.setOpacity(1.0f, 1.0f);
}

//! The scene drawn in one frame
Frame drawScene(const std::vector<Item> &items)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    gweni::test::clearView(background);
    renderer.begin();
    for(const Item &item:items)
        drawItem(renderer, item);
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
                drawItem(renderer, items[j]);
            else
                items[j].hide(renderer);
        }
        renderer.end();
    }
    return readFrame();
}

//! The pixels in \a rect (the whole view by default) of \a frame for which
//! \a bad holds against \a reference, the first logged
size_t countPixels(const Frame &frame, const Frame &reference,
    const std::function<bool(Color, Color)> &bad, Rect rect=Rect(0, 0, 0, 0))
{
    const Point size=gweni::test::viewSize();
    size_t count=0;

    if(rect.w == 0)
        rect=Rect(0, 0, size.x, size.y);
    for(int y=rect.y; y<rect.y+rect.h; ++y)
    {
        for(int x=rect.x; x<rect.x+rect.w; ++x)
        {
            const Color a=frame.at(x, y);
            const Color b=reference.at(x, y);

            if(bad(a, b))
            {
                if(count == 0)
                    MESSAGE("first at ", x, ",", y, ": got ", int(a.r), ",", int(a.g), ",", int(a.b),
                        " against ", int(b.r), ",", int(b.g), ",", int(b.b));
                ++count;
            }
        }
    }
    return count;
}

bool differs(Color a, Color b, int bound)
{
    return (std::abs(a.r-b.r) > bound) || (std::abs(a.g-b.g) > bound) || (std::abs(a.b-b.b) > bound);
}

//! The pixels of \a frame more than 2 off \a reference in a channel
size_t offBy2(const Frame &frame, const Frame &reference, Rect rect=Rect(0, 0, 0, 0))
{
    return countPixels(frame, reference, [](Color a, Color b) { return differs(a, b, 2); }, rect);
}

//! The pixels of \a frame that are not \a reference's exactly
size_t notExact(const Frame &frame, const Frame &reference, Rect rect=Rect(0, 0, 0, 0))
{
    return countPixels(frame, reference, [](Color a, Color b) { return differs(a, b, 0); }, rect);
}

//! The background and \a full mixed half and half, per channel
Frame halfway(const Frame &bg, const Frame &full)
{
    Frame mixed=full;

    for(size_t i=0; i<mixed.pixels.size(); ++i)
        mixed.pixels[i]=uint8_t((int(bg.pixels[i])+int(full.pixels[i])+1)/2);
    return mixed;
}

//! A 32x32 opaque image of four coloured quarters, loaded into the shared
//! renderer and freed with it
struct Quarters
{
    gweni::Texture texture;

    Quarters()
    {
        std::vector<uint8_t> pixels(32*32*4);

        for(int y=0; y<32; ++y)
        {
            for(int x=0; x<32; ++x)
            {
                uint8_t *p=&pixels[(y*32+x)*4];

                p[0]=(x < 16)?240:20;
                p[1]=(y < 16)?220:30;
                p[2]=((x < 16) == (y < 16))?40:230;
                p[3]=255;
            }
        }
        texture.name="opacity-quarters";
        REQUIRE(gweni::test::gpuRenderer().loadTexturePixels(texture, 32, 32, 32*4, pixels.data())
            == gweni::Texture::Status::Loaded);
    }

    Quarters(const Quarters &)=delete;
    Quarters &operator=(const Quarters &)=delete;

    ~Quarters() { gweni::test::gpuRenderer().freeTexture(texture); }
};

}//namespace

TEST_CASE("Opacity: the surface level scales a fill's alpha, 0xe0 at 0.5 drawn as 0x70")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(1);
    const Rect r(20, 20, 120, 60);

    const Frame bg=drawScene({});
    const Frame at70=drawScene({fill(prims[0], r, Color(240, 230, 40, 0x70), 3)});
    const Frame halfE0=drawScene({atLevels(fill(prims[0], r, Color(240, 230, 40, 0xe0), 3), 0.5f, 1.0f)});

    CHECK(notExact(halfE0, at70) == 0);

    // the blend of 0x70 over the background, within the backends' bound
    const Color got=halfE0.at(60, 50);
    const float a=0x70/255.0f;
    const Color want(uint8_t(std::lround(240*a+background.r*(1-a))),
        uint8_t(std::lround(230*a+background.g*(1-a))), uint8_t(std::lround(40*a+background.b*(1-a))), 255);

    CHECK(!differs(got, want, 2));
    // and it is translucent: the background shows
    CHECK(differs(got, Color(240, 230, 40, 255), 20));
    (void)bg;
}

TEST_CASE("Opacity: the surface level reaches every shape, and leaves glyphs and icons")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(6);
    const size_t run=ids.text();
    const std::vector<Item> shapes{
        rounded(prims[0], Rect(10, 10, 80, 50), Color(240, 120, 40, 255), Color(255, 255, 255, 255), 2),
        line(prims[1], PointF(100.0f, 20.0f), PointF(180.0f, 60.0f), Color(40, 240, 120, 255), 2),
        checker(prims[2], Rect(200, 10, 64, 48), 2),
        bilinear(prims[3], Rect(10, 80, 80, 50), 2)};
    const std::vector<Item> others{
        icon(prims[4], Rect(110, 80, 32, 32), Color(250, 250, 250, 255), 2),
        text(run, Point(160, 80), "HH", Color(250, 250, 250, 255), 2)};

    std::vector<Item> all=shapes;

    all.insert(all.end(), others.begin(), others.end());

    const Frame bg=drawScene({});
    const Frame full=drawScene(all);
    std::vector<Item> half;

    for(const Item &item:all)
        half.push_back(atLevels(item, 0.5f, 1.0f));

    const Frame frame=drawScene(half);
    const Frame mixed=halfway(bg, full);

    // each shape half-way to the background, each glyph and icon as at 1
    CHECK(offBy2(frame, mixed, Rect(10, 10, 80, 50)) == 0);
    CHECK(offBy2(frame, mixed, Rect(200, 10, 64, 48)) == 0);
    CHECK(offBy2(frame, mixed, Rect(10, 80, 80, 50)) == 0);
    CHECK(offBy2(frame, mixed, Rect(100, 18, 82, 44)) == 0);
    CHECK(notExact(frame, full, Rect(110, 80, 32, 32)) == 0);
    CHECK(notExact(frame, full, Rect(160, 80, 120, 50)) == 0);
    // and something of the shapes is not as at 1
    CHECK(notExact(frame, full, Rect(10, 10, 80, 50)) > 0);
    CHECK(notExact(frame, full, Rect(200, 10, 64, 48)) > 0);
}

TEST_CASE("Opacity: the text level scales glyphs, icons and images, apart from the surface level")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(3);
    const size_t run=ids.text();
    Quarters quarters;
    const std::vector<Item> all{
        fill(prims[0], Rect(10, 10, 60, 40), Color(240, 120, 40, 255), 2),
        icon(prims[1], Rect(90, 10, 32, 32), Color(250, 250, 250, 255), 2),
        image(prims[2], quarters.texture, Rect(140, 10, 32, 32), 2),
        text(run, Point(10, 60), "HHHH", Color(250, 250, 250, 255), 2)};

    const Frame bg=drawScene({});
    const Frame full=drawScene(all);
    std::vector<Item> half;

    for(const Item &item:all)
        half.push_back(atLevels(item, 1.0f, 0.5f));

    const Frame frame=drawScene(half);
    const Frame mixed=halfway(bg, full);

    CHECK(notExact(frame, full, Rect(10, 10, 60, 40)) == 0);
    CHECK(offBy2(frame, mixed, Rect(90, 10, 32, 32)) == 0);
    CHECK(offBy2(frame, mixed, Rect(140, 10, 32, 32)) == 0);
    CHECK(offBy2(frame, mixed, Rect(10, 60, 200, 50)) == 0);
    CHECK(notExact(frame, full, Rect(10, 60, 200, 50)) > 0);
    CHECK(notExact(frame, full, Rect(140, 10, 32, 32)) > 0);

    // the same run at another level: its glyph slots written again, the
    // colours alone (the run's text, font and place unchanged)
    SlotRenderer &renderer=gweni::test::gpuRenderer();

    drawScene(all);
    renderer.begin();
    renderer.end();
    gweni::test::clearView(background);
    renderer.begin();
    renderer.setOpacity(1.0f, 0.25f);
    all[3].draw(renderer);
    renderer.setOpacity(1.0f, 1.0f);
    renderer.end();
    CHECK(renderer.frameStats().textSlotsWritten == 4);
}

TEST_CASE("Opacity: at level 0 nothing is drawn and nothing behind it is hidden")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(5);
    const size_t run=ids.text();
    Quarters quarters;
    // taken first, in front: drawn at level 0 they must not keep the fill
    // behind them, taken last, off any pixel by depth
    const std::vector<Item> front{
        fill(prims[0], Rect(20, 20, 100, 60), Color(240, 230, 40, 255), 6),
        rounded(prims[1], Rect(130, 20, 100, 60), Color(240, 120, 40, 255), Color(255, 255, 255, 255), 6),
        icon(prims[2], Rect(240, 20, 40, 40), Color(250, 250, 250, 255), 6),
        image(prims[3], quarters.texture, Rect(240, 64, 32, 32), 6),
        text(run, Point(30, 30), "HHHH", Color(250, 250, 250, 255), 6)};
    const Item behind=fill(prims[4], Rect(10, 10, 300, 100), Color(200, 30, 30, 255), 1);

    const Frame reference=drawScene({behind});

    SUBCASE("both levels 0")
    {
        std::vector<Item> items;

        for(const Item &item:front)
            items.push_back(atLevels(item, 0.0f, 0.0f));
        items.push_back(behind);
        CHECK(notExact(drawScene(items), reference) == 0);
        // and the run at text level 0 is in no draw: the fill's chunk alone
        drawScene({behind});
        CHECK(gweni::test::gpuRenderer().frameStats().drawCalls == 1);
        drawScene(items);
        CHECK(gweni::test::gpuRenderer().frameStats().drawCalls == 1);
    }
    SUBCASE("each level 0 for what follows it, the other at 0.5")
    {
        std::vector<Item> items;

        items.push_back(atLevels(front[0], 0.0f, 0.5f));
        items.push_back(atLevels(front[1], 0.0f, 0.5f));
        items.push_back(atLevels(front[2], 0.5f, 0.0f));
        items.push_back(atLevels(front[3], 0.5f, 0.0f));
        items.push_back(atLevels(front[4], 0.5f, 0.0f));
        items.push_back(behind);
        CHECK(notExact(drawScene(items), reference) == 0);
    }
    SUBCASE("a slot shown again at level 0 stays hidden")
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        drawScene({front[0], behind});
        gweni::test::clearView(background);
        renderer.begin();
        renderer.hidePrimitive(prims[0]);
        renderer.setOpacity(0.0f, 0.0f);
        renderer.showPrimitive(1, prims[0]);
        renderer.setOpacity(1.0f, 1.0f);
        renderer.end();
        CHECK(notExact(readFrame(), reference) == 0);
    }
}

TEST_CASE("Opacity: a level that rounds every alpha to 0 draws nothing and hides nothing behind it")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(4);
    // taken first, in front, as in the level-0 case: a fill and a grid write
    // depth at alpha 0, so a level above 0 that leaves them no alpha must hide them
    const std::vector<Item> front{
        fill(prims[0], Rect(20, 20, 100, 60), Color(240, 230, 40, 255), 6),
        fill(prims[1], Rect(130, 20, 100, 60), Color(240, 230, 40, 0x20), 6),
        grid(prims[2], Rect(20, 85, 210, 20), Color(240, 240, 240, 255), Color(255, 255, 255, 255), 6)};
    const Item behind=fill(prims[3], Rect(10, 10, 300, 100), Color(200, 30, 30, 255), 1);

    const Frame reference=drawScene({behind});

    SUBCASE("at 0.001")
    {
        std::vector<Item> items;

        for(const Item &item:front)
            items.push_back(atLevels(item, 0.001f, 1.0f));
        items.push_back(behind);
        CHECK(notExact(drawScene(items), reference) == 0);
    }
    SUBCASE("a faint colour at 0.01")
    {
        // 0x20 at 0.01 is alpha 0; 0xff at 0.01 is 3, and draws
        CHECK(notExact(drawScene({atLevels(front[1], 0.01f, 1.0f), behind}), reference) == 0);
        CHECK(notExact(drawScene({atLevels(front[0], 0.01f, 1.0f), behind}), reference) > 0);
    }
    SUBCASE("a fill shown again at such a level stays hidden")
    {
        SlotRenderer &renderer=gweni::test::gpuRenderer();

        drawScene({atLevels(front[0], 0.001f, 1.0f), behind});
        gweni::test::clearView(background);
        renderer.begin();
        renderer.hidePrimitive(prims[0]);
        renderer.setOpacity(0.001f, 1.0f);
        renderer.showPrimitive(1, prims[0]);
        renderer.setOpacity(1.0f, 1.0f);
        renderer.end();
        CHECK(notExact(readFrame(), reference) == 0);
    }
}

TEST_CASE("Opacity: the text level scales a run's colour spans")
{
    using gweni::renderer::ColorSpan;

    Ids ids;
    const size_t run=ids.text();
    const std::vector<ColorSpan> spans{ColorSpan{0, 3, Color(250, 30, 30, 255)},
        ColorSpan{4, 7, Color(30, 250, 30, 255)}, ColorSpan{8, 11, Color(250, 250, 30, 255)}};
    std::vector<ColorSpan> other=spans;

    other[1].color=Color(250, 30, 250, 255);

    const Rect area(10, 20, 300, 60);
    const Frame bg=drawScene({});
    const Frame full=drawScene({spanText(run, Point(10, 20), spans, 2)});
    const Frame half=drawScene({atLevels(spanText(run, Point(10, 20), spans, 2), 1.0f, 0.5f)});

    // every glyph half-way to the background, its span's colour included
    CHECK(offBy2(half, halfway(bg, full), area) == 0);
    CHECK(notExact(half, full, area) > 0);

    // a span's new colour at that level (the glyph colours alone rewritten)
    // keeps the level
    const Frame otherFull=drawScene({spanText(run, Point(10, 20), other, 2)});

    drawScene({atLevels(spanText(run, Point(10, 20), spans, 2), 1.0f, 0.5f)});

    const Frame otherHalf=drawScene({atLevels(spanText(run, Point(10, 20), other, 2), 1.0f, 0.5f)});

    CHECK(offBy2(otherHalf, halfway(bg, otherFull), area) == 0);
}

TEST_CASE("Opacity: a translucent panel's fill shows through its translucent button")
{
    Ids ids;
    // the button's slot first: slot-id order would draw it behind the panel
    const std::vector<size_t> prims=ids.primitives(2);
    const std::vector<Item> items{
        atLevels(rounded(prims[0], Rect(60, 40, 120, 40), Color(40, 220, 90, 255), Color(255, 255, 255, 255), 5),
            0.5f, 1.0f),
        atLevels(fill(prims[1], Rect(20, 20, 260, 100), Color(240, 230, 40, 255), 2), 0.5f, 1.0f)};

    const Frame reference=drawReference(items);
    const Frame frame=drawScene(items);

    CHECK(offBy2(frame, reference) == 0);

    // the button's middle is neither the button over the background nor the
    // button alone: the panel shows through it
    Item panelHidden=items[1];

    panelHidden.draw=panelHidden.hide;

    const Frame buttonAlone=drawScene({items[0], panelHidden});

    CHECK(differs(frame.at(120, 60), buttonAlone.at(120, 60), 10));
}

TEST_CASE("Opacity: a checker at a surface level of 0.5 shows the fill behind it, taken after it")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);
    const std::vector<Item> items{atLevels(checker(prims[0], Rect(60, 40, 64, 48), 5), 0.5f, 1.0f),
        fill(prims[1], Rect(20, 20, 260, 100), Color(240, 230, 40, 255), 2)};

    CHECK(offBy2(drawScene(items), drawReference(items)) == 0);
}

TEST_CASE("Opacity: at level 1 a slot is written as by a renderer that never had a level set")
{
    Ids ids;
    const std::vector<size_t> prims=ids.primitives(2);
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const auto draw=[&]()
        {
            renderer.setDrawColor(Color(240, 230, 40, 0xe0));
            renderer.drawFilledRect(prims[0], Rect(5, 5, 20, 20), 3);
            renderer.drawCheckerRect(prims[1], Rect(30, 5, 20, 20), 3, Color(250, 20, 20, 128),
                Color(255, 255, 255, 255), Color(90, 90, 90, 255), 0.0f, 0.0f, Color(0, 0, 0, 0));
        };

    renderer.begin();
    draw();
    renderer.end();

    // drawn again at 1, after a level was set and put back: the same bytes,
    // so nothing is written
    renderer.begin();
    renderer.setOpacity(0.5f, 0.5f);
    renderer.setOpacity(1.0f, 1.0f);
    draw();
    renderer.end();
    CHECK(renderer.frameStats().primitiveSlotsWritten == 0);

    // and at 0.5 both are written
    renderer.begin();
    renderer.setOpacity(0.5f, 0.5f);
    draw();
    renderer.setOpacity(1.0f, 1.0f);
    renderer.end();
    CHECK(renderer.frameStats().primitiveSlotsWritten == 2);
}
