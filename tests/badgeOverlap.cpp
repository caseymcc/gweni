/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A Badge's draw order and clip, drawn by the textured skin offscreen on the
 *  backend under test at scale 1.
 *
 *  A badge on a Button's top-right corner overhangs the next Button, whose
 *  text starts under it. With the badge hidden, the pixels inside the bubble's
 *  fully covered middle hold that Button's fill and some of its glyphs; with
 *  it shown they are exactly those the badge draws with the next Button hidden:
 *  the bubble and its number are in front of the next Button's fill and
 *  glyphs (its parent reserves the z after its subtree for it).
 *
 *  A badge on a Button flush with a ScrollControl's viewport top is cut there:
 *  the rows above the ScrollControl are those drawn with the badge hidden,
 *  while inside the viewport its lower half shows.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/badge.h>
#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Rect;

struct Frame
{
    std::vector<uint8_t> pixels;

    bool sameAt(const Frame &other, int x, int y) const
    {
        const size_t at=(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4;

        for(int c=0; c < 3; ++c)
        {
            if(std::abs(int(pixels[at+c])-int(other.pixels[at+c])) > 1)
                return false;
        }
        return true;
    }
};

//! The pixels of \p rect (canvas units, scale 1) where \p a and \p b differ.
size_t differences(const Frame &a, const Frame &b, const Rect &rect)
{
    size_t count=0;

    for(int y=std::max(0, rect.y); y < rect.y+rect.h; ++y)
    {
        for(int x=std::max(0, rect.x); x < rect.x+rect.w; ++x)
        {
            if(!a.sameAt(b, x, y))
                ++count;
        }
    }
    return count;
}

//! A textured-skin canvas over the whole view.
struct Scene
{
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;

    Scene()
    {
        const gweni::Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setSize(viewSize.x, viewSize.y);
    }

    ~Scene()
    {
        delete canvas;
        delete skin;
    }

    Frame settle()
    {
        Frame shot;

        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(shot.pixels);
        return shot;
    }

    gweni::controls::Button *button(gweni::controls::Base *parent, const char *text, const Rect &bounds)
    {
        gweni::controls::Button *button=parent->newChild<gweni::controls::Button>();

        button->setText(text);
        button->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        button->setBounds(bounds);
        return button;
    }
};

}//namespace

TEST_CASE("Badge overlap: a badge over the next Button draws in front of its fill and glyphs")
{
    Scene scene;
    gweni::controls::Base *row=scene.canvas->newChild<gweni::controls::Base>();

    row->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    row->setBounds(20, 40, 300, 80);

    gweni::controls::Button *anchor=scene.button(row, "Inbox", Rect(0, 20, 80, 24));
    gweni::controls::Button *next=scene.button(row, "WWWWWWWWWW", Rect(80, 20, 120, 24));

    // its text from its top-left corner, under the badge
    next->setTextAlignment(gweni::Alignment::Left|gweni::Alignment::Top);
    next->setTextPadding(gweni::Padding(0, 0, 0, 0));

    gweni::controls::Badge *badge=anchor->newChild<gweni::controls::Badge>();

    // a wide pill, so its middle is a rect of whole fill
    badge->setCap(999999);
    badge->setCount(888888);

    const Frame shown=scene.settle();
    const Rect bounds=badge->getCanvasBounds();
    const int radius=bounds.h/2;
    // the fully covered middle, over the next Button only
    const Rect middle(std::max(bounds.x+radius, next->getCanvasBounds().x+1), bounds.y+2,
        bounds.x+bounds.w-radius-std::max(bounds.x+radius, next->getCanvasBounds().x+1), bounds.h-4);
    const Rect nextRect=next->getCanvasBounds();

    REQUIRE(middle.w > 4);
    REQUIRE(middle.h > 4);
    REQUIRE(middle.x >= nextRect.x);
    REQUIRE(middle.y+middle.h > nextRect.y+4);

    // under it, with it hidden: the next Button's fill and glyphs
    badge->hide();
    const Frame without=scene.settle();

    next->setText("");
    const Frame fillOnly=scene.settle();

    badge->show();
    next->hide();
    const Frame alone=scene.settle();

    // glyphs of the next Button are under the middle, and so is its fill
    CHECK(differences(without, fillOnly, middle) > 0);
    CHECK(differences(fillOnly, alone, middle) == middle.w*middle.h);

    // with the badge, exactly what it draws alone
    CHECK(differences(shown, alone, middle) == 0);
}

TEST_CASE("Badge overlap: a badge on a Button flush with a ScrollControl's viewport top is cut there")
{
    Scene scene;
    gweni::controls::ScrollControl *scroll=scene.canvas->newChild<gweni::controls::ScrollControl>();

    scroll->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
    scroll->setBounds(40, 60, 200, 80);
    scroll->setScroll(false, true);

    gweni::controls::Button *anchor=scene.button(scroll, "Flush top", Rect(10, 0, 80, 22));
    gweni::controls::Badge *badge=anchor->newChild<gweni::controls::Badge>();

    badge->setCount(42);

    const Frame shown=scene.settle();
    const Rect bounds=badge->getCanvasBounds();
    const Rect scrollRect=scroll->getCanvasBounds();

    REQUIRE(bounds.y < scrollRect.y);
    REQUIRE(bounds.y+bounds.h > scrollRect.y+4);

    badge->hide();
    const Frame hidden=scene.settle();

    const Rect above(bounds.x, bounds.y, bounds.w, scrollRect.y-bounds.y);
    const Rect inside(bounds.x, scrollRect.y+2, bounds.w, bounds.y+bounds.h-scrollRect.y-2);

    // above the ScrollControl nothing of it
    CHECK(differences(shown, hidden, above) == 0);
    // inside the viewport its lower half
    CHECK(differences(shown, hidden, inside) > 0);
}
