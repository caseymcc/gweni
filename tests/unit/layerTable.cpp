/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  SlotRenderer's layer table (setLayerTable), on a SlotRenderer whose backend
 *  hooks do nothing. Off it holds nothing: shownLayerCount() and shownLayer()
 *  are 0 and a flush uploads no table. Turned on over slots drawn already, it
 *  counts every label a shown slot or run uses, so shownLayer(z) is the number
 *  of used labels below z, label 0 always counted (its depth is the clear
 *  value, so layer 0 stays its own and a slot there adds no layer), and an
 *  unused label between two used ones takes none,
 *  at once, before any flush; the next flush allocates it and uploads it whole
 *  (FrameStats::layerUploadCalls/layerUploadBytes, never uploadCalls/
 *  uploadBytes) and calls the listener once. From then on the table follows a
 *  hide, a show, a z change, a release and a clip cull of a primitive slot,
 *  and a text run hidden, shown, emptied, moved in z and released, each
 *  flush where a label's count went from or to 0 uploading the rows from the
 *  first one whose T changed and telling the listener, and a flush where none
 *  did (a second slot at a used label hidden, nothing changed) uploading
 *  nothing and telling nobody. setLayerTable(false) forgets it and turns the
 *  remap off; the remap cannot be turned on while the table is off.
 */
#include "headlessFixture.h"
#include "mirrorOnlyRenderer.h"

#ifdef GWENI_TEST_SLOT_RENDERER

#include <doctest/doctest.h>

#include <vector>

using namespace gweni;

namespace
{

//! A MirrorOnlyRenderer that records the remap switch and the table's uploads
class Recorder: public test::MirrorOnlyRenderer
{
public:
    struct Upload
    {
        size_t firstRow, rows;
    };

    //! One frame drawing `draw`
    template<typename Draw>
    void frame(Draw &&draw)
    {
        begin();
        draw();
        end();
    }
    void frame() { frame([]{}); }

    size_t primitive(int z, int x=0)
    {
        size_t id;

        generatePrimitive(id);
        ids.push_back(id);
        drawRoundedRect(id, Rect(x, 10, 20, 20), 2.0f, 0.0f, Color(10, 20, 30, 255), Color(10, 20, 30, 255),
            Color(), z);
        return id;
    }

    ~Recorder()
    {
        for(size_t id:ids)
            releasePrimitive(id);
    }

    std::vector<Upload> uploads;
    std::vector<size_t> ids;
    int allocations=0;
    int remapChanges=0;

protected:
    void allocateLayerTable() override { ++allocations; }
    void uploadLayerTableRows(size_t firstRow, size_t rows, const uint16_t *) override
    {
        uploads.push_back({firstRow, rows});
    }
    void layerRemapChanged() override { ++remapChanges; }
};

constexpr size_t kRowBytes=renderer::SlotRenderer::kLayerTableSide*sizeof(uint16_t);

}//namespace

TEST_CASE("Layer table: off it holds and uploads nothing, and the remap stays off")
{
    Recorder renderer;

    renderer.frame([&]{ renderer.primitive(5); });
    CHECK_FALSE(renderer.layerTable());
    CHECK(renderer.shownLayerCount() == 0);
    CHECK(renderer.shownLayer(9) == 0);
    renderer.setLayerRemap(true);
    CHECK_FALSE(renderer.layerRemap());
    CHECK(renderer.remapChanges == 0);
    renderer.frame();
    CHECK(renderer.allocations == 0);
    CHECK(renderer.uploads.empty());
    CHECK(renderer.getFrameStats().layerUploadCalls == 0);
}

TEST_CASE("Layer table: shown layers of primitive slots, following hide, show, z, release and a cull")
{
    Recorder renderer;
    size_t a=0, b=0, c=0, d=0;
    int listened=0;

    renderer.frame([&]
    {
        a=renderer.primitive(10);
        b=renderer.primitive(18);
        c=renderer.primitive(26);
        d=renderer.primitive(26, 40);
    });

    renderer.setLayerTable(true);
    renderer.setLayerTableListener([&]{ ++listened; });
    REQUIRE(renderer.layerTable());
    // counted at once, before a flush
    CHECK(renderer.shownLayerCount() == 4);
    CHECK(renderer.shownLayer(0) == 0);
    CHECK(renderer.shownLayer(10) == 1);
    CHECK(renderer.shownLayer(11) == 2);
    CHECK(renderer.shownLayer(17) == 2);
    CHECK(renderer.shownLayer(18) == 2);
    CHECK(renderer.shownLayer(26) == 3);
    CHECK(renderer.shownLayer(27) == 4);
    CHECK(renderer.shownLayer(65535) == 4);
    CHECK(renderer.shownLayer(70000) == 4);
    CHECK(renderer.uploads.empty());
    CHECK(listened == 0);

    // the first flush allocates it and sends it whole, outside uploadCalls
    renderer.frame();
    CHECK(renderer.allocations == 1);
    REQUIRE(renderer.uploads.size() == 1);
    CHECK(renderer.uploads[0].firstRow == 0);
    CHECK(renderer.uploads[0].rows == renderer::SlotRenderer::kLayerTableSide);
    CHECK(renderer.getFrameStats().layerUploadCalls == 1);
    CHECK(renderer.getFrameStats().layerUploadBytes == renderer::SlotRenderer::kLayerTableSide*kRowBytes);
    CHECK(renderer.getFrameStats().uploadCalls == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);
    CHECK(listened == 1);

    // nothing changed: nothing sent, nobody told
    renderer.frame();
    CHECK(renderer.uploads.size() == 1);
    CHECK(renderer.getFrameStats().layerUploadCalls == 0);
    CHECK(renderer.getFrameStats().layerUploadBytes == 0);
    CHECK(listened == 1);

    // one of two slots at label 26 hidden: the label is still in use
    renderer.frame([&]{ renderer.hidePrimitive(d); });
    CHECK(renderer.shownLayerCount() == 4);
    CHECK(renderer.uploads.size() == 1);
    CHECK(listened == 1);

    // hidden: label 18 takes no layer, those above it move down one
    renderer.frame([&]{ renderer.hidePrimitive(b); });
    CHECK(renderer.shownLayerCount() == 3);
    CHECK(renderer.shownLayer(18) == 2);
    CHECK(renderer.shownLayer(26) == 2);
    CHECK(renderer.shownLayer(27) == 3);
    REQUIRE(renderer.uploads.size() == 2);
    CHECK(renderer.uploads[1].firstRow == 0);
    CHECK(listened == 2);

    // shown again
    renderer.frame([&]{ renderer.showPrimitive(4, b); });
    CHECK(renderer.shownLayerCount() == 4);
    CHECK(renderer.shownLayer(26) == 3);
    CHECK(listened == 3);

    // a z change high up sends the rows from the first one whose T moved
    renderer.frame([&]
    {
        renderer.drawRoundedRect(c, Rect(0, 10, 20, 20), 2.0f, 0.0f, Color(10, 20, 30, 255),
            Color(10, 20, 30, 255), Color(), 1000);
    });
    CHECK(renderer.shownLayerCount() == 4);
    CHECK(renderer.shownLayer(26) == 3);
    CHECK(renderer.shownLayer(27) == 3);
    CHECK(renderer.shownLayer(1000) == 3);
    CHECK(renderer.shownLayer(1001) == 4);
    REQUIRE(renderer.uploads.size() == 4);
    CHECK(renderer.uploads[3].firstRow == 0);
    renderer.frame([&]
    {
        renderer.drawRoundedRect(c, Rect(0, 10, 20, 20), 2.0f, 0.0f, Color(10, 20, 30, 255),
            Color(10, 20, 30, 255), Color(), 2000);
    });
    REQUIRE(renderer.uploads.size() == 5);
    // T moved from label 1001 (row 3) up
    CHECK(renderer.uploads[4].firstRow == 1001/renderer::SlotRenderer::kLayerTableSide);
    CHECK(renderer.uploads[4].rows == renderer::SlotRenderer::kLayerTableSide-3);
    CHECK(renderer.getFrameStats().layerUploadBytes == renderer.uploads[4].rows*kRowBytes);
    CHECK(renderer.shownLayer(2001) == 4);

    // released
    renderer.frame([&]{ renderer.releasePrimitive(a); });
    renderer.ids.erase(renderer.ids.begin());
    CHECK(renderer.shownLayerCount() == 3);
    CHECK(renderer.shownLayer(18) == 1);

    // culled wholly outside its clip box: drawn nowhere, so no layer
    renderer.setClipCulling(true);
    renderer.frame([&]
    {
        renderer.setClipRegion(Rect(100, 100, 50, 50));
        renderer.drawRoundedRect(b, Rect(0, 10, 20, 20), 2.0f, 0.0f, Color(10, 20, 30, 255),
            Color(10, 20, 30, 255), Color(), 18);
    });
    CHECK(renderer.shownLayerCount() == 2);
    renderer.frame([&]
    {
        renderer.setClipRegion(Rect(0, 0, 640, 480));
        renderer.drawRoundedRect(b, Rect(0, 10, 20, 20), 2.0f, 0.0f, Color(10, 20, 30, 255),
            Color(10, 20, 30, 255), Color(), 18);
    });
    CHECK(renderer.shownLayerCount() == 3);

    renderer.setLayerRemap(true);
    CHECK(renderer.layerRemap());
    CHECK(renderer.remapChanges == 1);
    renderer.setLayerTable(false);
    CHECK_FALSE(renderer.layerTable());
    CHECK_FALSE(renderer.layerRemap());
    CHECK(renderer.remapChanges == 2);
    CHECK(renderer.shownLayerCount() == 0);
    const size_t uploads=renderer.uploads.size();

    renderer.frame([&]{ renderer.hidePrimitive(b); });
    CHECK(renderer.uploads.size() == uploads);
    CHECK(listened == 8);
}

TEST_CASE("Layer table: a text run's layer follows its hide, show, emptying, z and release")
{
    Recorder renderer;
    Font font;
    size_t text=0;

    font.facename="OpenSans";
    font.size=11;
    renderer.generateTextPrimitive(text);

    renderer.setLayerTable(true);
    renderer.frame([&]
    {
        renderer.primitive(4);
        renderer.renderText(text, font, Point(10, 10), 40, "abc");
    });
    CHECK(renderer.shownLayerCount() == 3);
    CHECK(renderer.shownLayer(40) == 2);
    CHECK(renderer.shownLayer(41) == 3);

    // label 0 is layer 0 already
    renderer.frame([&]{ renderer.primitive(0); });
    CHECK(renderer.shownLayerCount() == 3);
    CHECK(renderer.shownLayer(40) == 2);

    renderer.frame([&]{ renderer.hideText(text); });
    CHECK(renderer.shownLayerCount() == 2);
    renderer.frame([&]{ renderer.showText(text); });
    CHECK(renderer.shownLayerCount() == 3);

    // emptied: no glyph to draw
    renderer.frame([&]{ renderer.renderText(text, font, Point(10, 10), 40, ""); });
    CHECK(renderer.shownLayerCount() == 2);
    renderer.frame([&]{ renderer.renderText(text, font, Point(10, 10), 40, "abc"); });
    CHECK(renderer.shownLayerCount() == 3);

    // moved in z, the same string kept
    renderer.frame([&]{ renderer.renderText(text, font, Point(12, 10), 2, "abc"); });
    CHECK(renderer.shownLayerCount() == 3);
    CHECK(renderer.shownLayer(2) == 1);
    CHECK(renderer.shownLayer(4) == 2);
    CHECK(renderer.shownLayer(40) == 3);

    // a run counted when the table is turned on
    renderer.setLayerTable(false);
    renderer.setLayerTable(true);
    CHECK(renderer.shownLayerCount() == 3);

    renderer.frame([&]{ renderer.releaseTextPrimitive(text); });
    CHECK(renderer.shownLayerCount() == 2);
}

#endif//GWENI_TEST_SLOT_RENDERER
