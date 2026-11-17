/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  The textured skin's Sequencer drawn offscreen on the backend under test at
 *  scales 1 and 1.5: where two keys of a track overlap, the one added later
 *  shows, and it is the one hitTest() picks. A track's keys share one layer,
 *  where the depth test keeps the pixels of the slot drawn first, so the
 *  renderer takes a track's key slots in descending id order (key i the
 *  list's i-th highest id): a later key's slot is drawn before an earlier
 *  one's. Checked with the earlier key selected and the later not, then the
 *  other way round, at one instant and half a key apart (where the earlier
 *  one's own half shows it), and with a third key added after the frame at
 *  the same instant. The keys are SequencerKey (yellow) and
 *  SequencerKeySelected (orange) discs; the pixel at a disc's centre is its
 *  fill, with no blend.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/sequencer.h>
#include <gweni/skins/textured/texturedSkin.h>
#include <gweni/skins/textured/tokyoNight.h>

#include <cmath>
#include <cstdlib>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::Rect;
using gweni::controls::Sequencer;
using gweni::controls::SequencerItem;
namespace tn=gweni::skin::textured::tokyonight;

bool same(const Color &a, const Color &b)
{
    return std::abs(a.r-b.r) <= 1 && std::abs(a.g-b.g) <= 1 && std::abs(a.b-b.b) <= 1;
}

struct Scene
{
    float scale;
    gweni::skin::textured::TexturedSkin *skin;
    gweni::controls::Canvas *canvas;
    Sequencer *seq;
    std::vector<uint8_t> pixels;

    explicit Scene(float s):
        scale(s)
    {
        const Point viewSize=gweni::test::viewSize();

        skin=new gweni::skin::textured::TexturedSkin(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=new gweni::controls::Canvas(skin);
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(std::lround(viewSize.x/scale)),
            static_cast<int>(std::lround(viewSize.y/scale)));

        seq=canvas->newChild<Sequencer>();
        seq->setSizeFlags({gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed});
        seq->setBounds(7, 9, 300, 120);
        seq->setFrameRate(10);
        seq->setTimeRange(0, 10);
        seq->addTrack("a");
        seq->addTrack("b");
    }

    ~Scene()
    {
        delete canvas;
        delete skin;

        gweni::renderer::SlotRenderer &renderer=gweni::test::gpuRenderer();

        // scale 1 for the next Scene, as tests/tabJoin.cpp's
        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    void settle()
    {
        for(int i=0; i < 3; ++i)
        {
            gweni::test::clearView(Color(0, 0, 0, 255));
            canvas->updateCanvas();
            canvas->renderCanvas();
        }
        gweni::test::readView(pixels);
    }

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    int snap(int v) const { return gweni::renderer::Base::snap(float(v), scale); }

    //! The device pixel at the sequencer's local point \p local.
    Color pixel(const Point &local) const
    {
        const Point canvasPoint=seq->localPosToCanvas(local);

        return at(snap(canvasPoint.x), snap(canvasPoint.y));
    }

    //! The centre of the key's disc, in the sequencer's coordinates.
    Point centre(size_t track, size_t index) const
    {
        Rect rect;

        REQUIRE(seq->getKeyRect(track, index, rect));
        return Point(rect.x+rect.w/2, rect.y+rect.h/2);
    }

    size_t picked(const Point &local) const
    {
        SequencerItem item;

        REQUIRE(seq->hitTest(local, item) == Sequencer::Part::Key);
        return item.index;
    }
};

}//namespace

TEST_CASE("Sequencer: the later of two overlapping keys shows, and is the one the hit test picks")
{
    for(const float scale : {1.0f, 1.5f})
    {
        CAPTURE(scale);

        Scene scene(scale);
        Sequencer *seq=scene.seq;

        // track 0: two keys at one instant; track 1: half a key apart
        seq->addKey(0, 5.0, 1);
        seq->addKey(0, 5.0, 2);
        seq->addKey(1, 5.0, 3);
        seq->addKey(1, 5.0, 4);
        scene.settle();
        // the layout gave the area: the second key of track 1 half a disc on
        seq->setKey(1, 1, 5.0+seq->timeAt(seq->getBarArea().x+Sequencer::KeySize/2)
            -seq->timeAt(seq->getBarArea().x));

        SUBCASE("the earlier key selected: the later, unselected one shows")
        {
            seq->select(SequencerItem(SequencerItem::Kind::Key, 0, 0));
            seq->select(SequencerItem(SequencerItem::Kind::Key, 1, 0));
            scene.settle();

            const Point both=scene.centre(0, 0);

            CHECK(same(scene.pixel(both), tn::SequencerKey.top));
            CHECK(scene.picked(both) == 1);

            // track 1: the overlap shows the later key, the earlier one's own
            // half shows it
            const Point earlier=scene.centre(1, 0);
            const Point later=scene.centre(1, 1);
            const Point overlap((earlier.x+later.x)/2, earlier.y);
            const Point own(earlier.x-Sequencer::KeySize/4, earlier.y);

            REQUIRE(later.x > earlier.x);
            CHECK(same(scene.pixel(overlap), tn::SequencerKey.top));
            CHECK(scene.picked(overlap) == 1);
            CHECK(same(scene.pixel(own), tn::SequencerKeySelected.top));
            CHECK(scene.picked(own) == 0);
        }
        SUBCASE("the later key selected: it shows")
        {
            seq->select(SequencerItem(SequencerItem::Kind::Key, 0, 1));
            seq->select(SequencerItem(SequencerItem::Kind::Key, 1, 1));
            scene.settle();

            const Point both=scene.centre(0, 0);
            const Point earlier=scene.centre(1, 0);
            const Point later=scene.centre(1, 1);
            const Point overlap((earlier.x+later.x)/2, earlier.y);

            CHECK(same(scene.pixel(both), tn::SequencerKeySelected.top));
            CHECK(scene.picked(both) == 1);
            CHECK(same(scene.pixel(overlap), tn::SequencerKeySelected.top));
            CHECK(scene.picked(overlap) == 1);

            // a third key added at the instant, unselected: in front of both
            seq->addKey(0, 5.0, 5);
            scene.settle();
            CHECK(same(scene.pixel(both), tn::SequencerKey.top));
            CHECK(scene.picked(both) == 2);
        }
    }
}
