/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  Text::getClosestCharacter measures its line once, on the StubRenderer: each
 *  call makes exactly one renderer::Base::measureCarets() and no measureText()
 *  of its own, unwrapped and wrapped (a row of a wrapped Text is measured
 *  alone), and counts as one measure in controls::measureTextCalls(). Its
 *  answers are those of the per-prefix walk it replaced, computed here from
 *  measureText of each codepoint prefix: the Manhattan distance to each caret
 *  at y 0, the last of equal distances, nothing beyond 4096, at points on, off
 *  and between the carets, before and past the line, above and below it.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/text.h>
#include <gweni/utility.h>

#include <doctest/doctest.h>

#include <cstdlib>
#include <string>
#include <vector>

using namespace gweni;
using controls::internal::Text;

namespace
{

struct Scene
{
    test::StubRenderer renderer;
    test::StubSkin skin{&renderer};
    controls::Canvas *canvas;
    controls::Base *container;
    Text *text;

    //! One Text holding source in a container of the given width, wrapped or not
    Scene(const String &source, bool wrap, int width=600)
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);

        container=canvas->newChild<controls::Base>();
        container->setBounds(0, 0, width, 500);

        text=container->newChild<Text>();
        text->setWrap(wrap);
        text->setText(source);

        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    ~Scene()
    {
        delete canvas;
    }
};

//! The per-prefix walk: the byte index of the caret of str nearest p.
int referenceClosest(renderer::Base &renderer, const Font &font, const String &str, Point p)
{
    int distance=4096;
    size_t value=0;

    for(size_t i=0; ; i=utf8Next(str, i))
    {
        const int x=(str.empty() || i == 0)?0:renderer.measureText(font, str.substr(0, i)).x;
        const int d=std::abs(x-p.x)+std::abs(p.y);

        if(d <= distance)
        {
            distance=d;
            value=i;
        }

        if(i >= str.length())
            break;
    }
    return static_cast<int>(value);
}

//! Probes one row: every caret's x, one either side, the midpoints, before and
//! past the line, on, above and below the row at rowY (relative to the row).
std::vector<Point> probes(renderer::Base &renderer, const Font &font, const String &row, int rowY,
    int rowHeight)
{
    std::vector<int> xs;

    renderer.Base::measureCarets(font, row, xs);

    std::vector<int> px={-50, -1, xs.back()+1, xs.back()+50, 5000};

    for(size_t k=0; k < xs.size(); ++k)
    {
        px.insert(px.end(), {xs[k]-1, xs[k], xs[k]+1});
        if(k+1 < xs.size())
            px.push_back((xs[k]+xs[k+1])/2);
    }

    std::vector<Point> points;

    for(int dy:{-rowHeight, 0, rowHeight/2, rowHeight-1, 3*rowHeight})
    {
        for(int x:px)
            points.push_back(Point(x, rowY+dy));
    }
    return points;
}

}//namespace

TEST_CASE("getClosestCharacter: one measureCarets and no measureText per call, unwrapped")
{
    const String str="ab \xc3\xa9\xe2\x82\xac\xf0\x9f\x98\x80 cd";  // ab é€😀 cd
    Scene scene(str, false);
    Text *text=scene.text;
    const Font &font=text->getFont();
    const Padding padding=text->getPadding();
    const int rowHeight=text->getLineBox(0).h;

    for(const Point &q:probes(scene.renderer, font, str, 0, rowHeight))
    {
        const Point p(q.x+padding.left, q.y+padding.top);
        CAPTURE(p.x);
        CAPTURE(p.y);

        const size_t carets=scene.renderer.caretMeasures;
        const size_t measures=scene.renderer.measures;
        const size_t counted=controls::measureTextCalls();

        const int pos=text->getClosestCharacter(p);

        CHECK(scene.renderer.caretMeasures-carets == 1);
        CHECK(scene.renderer.measures-measures == 0);
        CHECK(controls::measureTextCalls()-counted == 1);
        CHECK(pos == referenceClosest(scene.renderer, font, str, q));
    }
}

TEST_CASE("getClosestCharacter: one measureCarets and no measureText per call, wrapped")
{
    // wrapped at 60 units: several rows of a word or two each
    const String str="one two \xc3\xa9t\xc3\xa9 four five\nsix seven";
    Scene scene(str, true, 60);
    Text *text=scene.text;
    const Font &font=text->getFont();

    const int lines=static_cast<int>(text->numLines());

    REQUIRE(lines > 2);

    const int rowHeight=text->getLineBox(1).y-text->getLineBox(0).y;

    REQUIRE(rowHeight > 0);

    for(int line=0; line < lines; ++line)
    {
        CAPTURE(line);

        const int start=text->getStartCharFromLine(line);
        const String row=str.substr(start, text->getLineLength(line));

        for(const Point &p:probes(scene.renderer, font, row, line*rowHeight, rowHeight))
        {
            CAPTURE(p.x);
            CAPTURE(p.y);

            const size_t carets=scene.renderer.caretMeasures;
            const size_t measures=scene.renderer.measures;
            const size_t counted=controls::measureTextCalls();

            text->getClosestCharacter(p);

            CHECK(scene.renderer.caretMeasures-carets == 1);
            CHECK(scene.renderer.measures-measures == 0);
            CHECK(controls::measureTextCalls()-counted == 1);
        }
    }
}
