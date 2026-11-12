/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  A Label sized to its text shows every inked pixel of its run (#571).
 *  measureText measures pens, not ink, so a glyph's ink can reach past the
 *  measured box: a leading 'J' a pixel left of it at scale 1, a trailing 'g'
 *  at 10 px times 1.25 two device columns right of it, LCD text's filter
 *  fringe a pixel left of the first glyph. The sweep puts one unwrapped
 *  Label per string on a textured-skin canvas, every printable ASCII glyph
 *  after an 'A' (the last glyph) and before one (the first), in OpenSans at
 *  8 to 24 px and canvas scales 1, 1.25, 1.5, 1.75 and 2, in grayscale and,
 *  where the device blends it, LCD text: below 18 px measured as coverage
 *  (drawn as coverage, or as SDF once size*scale reaches the threshold), from
 *  18 px as SDF. Each string is drawn again by itself, unclipped, at its
 *  Text's place, and around each Label (its box and half the gap to its
 *  neighbours) the Label's frame must be that run's pixels, byte for byte.
 *
 *  It also measures what a clip margin must be to show that ink: for each
 *  run, the fewest canvas units m for which [snap(x-m), snap(x+w+m)) holds
 *  every inked column, which must be at most Label::inkMargin(size), the
 *  margin the Label clips its Text with. With GWENI_INK_SWEEP=1 in the
 *  environment it prints the most every size needed (per scale, mode and end)
 *  and how far the ink reached past the box in device px, left, right, above
 *  and below. On main before #571, where a Label clipped its Text at its
 *  bounds, every size, scale and mode had runs whose ink it cut off. *
 *  Text that does not lie wholly inside its Label is still cut at the
 *  Label's bounds: a Label narrower than its text ("JAg", ink past both
 *  sides) shows the run alone with everything outside its bounds cut away,
 *  and a TextBox's text scrolled to its end, or back to its start with its
 *  end past the box, changes no pixel outside the box. The widened rect is
 *  cut by the region the Label is in: Labels at both edges of a
 *  ScrollControl's viewport, their runs reaching past it, change no pixel
 *  outside the viewport. And the rect is stamped on the slots already drawn
 *  when it changes: a Label shrunk below its text and grown back, or given a
 *  string that does not fit and the first back, starting from either state,
 *  draws each state's pixels as a fresh canvas built in it does.
 */
#include <doctest/doctest.h>

#include "gpuRenderer.h"

#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/controls/scrollControl.h>
#include <gweni/controls/textBox.h>
#include <gweni/skins/textured/texturedSkin.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Font;
using gweni::Point;
using gweni::Rect;
using gweni::String;
using gweni::renderer::SlotRenderer;
using gweni::renderer::TextAA;

const Color background(0, 0, 0, 255);
const Color white(255, 255, 255, 255);

const float scales[]={ 1.0f, 1.25f, 1.5f, 1.75f, 2.0f };

bool inked(const std::vector<uint8_t> &frame, int x, int y)
{
    const uint8_t *pixel=&frame[(y*gweni::test::viewSize().x+x)*4];

    return (pixel[0] != background.r) || (pixel[1] != background.g) || (pixel[2] != background.b);
}

//! OpenSans at \p size px, kept for the process: a Text holds a pointer to
//! its font.
const Font &openSansAt(int size)
{
    static std::map<int, Font> fonts;
    auto found=fonts.find(size);

    if(found == fonts.end())
    {
        Font font=gweni::test::openSans();

        font.size=float(size);
        found=fonts.emplace(size, font).first;
    }
    return found->second;
}

//! Puts the shared renderer's text AA mode back when a case ends.
struct RestoreTextAA
{
    RestoreTextAA(): aa(gweni::test::gpuRenderer().textAA()) {}
    ~RestoreTextAA() { gweni::test::gpuRenderer().setTextAA(aa); }

    TextAA aa;
};

//! A textured-skin canvas over the whole view at a scale, with as many
//! unwrapped white Labels as a batch of strings needs.
struct Sweep
{
    explicit Sweep(float scale): scale(scale)
    {
        const Point viewSize=gweni::test::viewSize();

        skin=std::make_unique<gweni::skin::textured::TexturedSkin>(&gweni::test::gpuRenderer());
        REQUIRE(skin->init("DefaultSkin.json"));
        skin->setDefaultFont("OpenSans.ttf", 11);

        canvas=std::make_unique<gweni::controls::Canvas>(skin.get());
        canvas->setScale(scale);
        canvas->setSize(static_cast<int>(std::lround(viewSize.x/scale)),
            static_cast<int>(std::lround(viewSize.y/scale)));
    }

    ~Sweep()
    {
        canvas.reset();
        skin.reset();

        SlotRenderer &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.begin();
        renderer.end();
    }

    gweni::controls::Label *label(size_t i)
    {
        while(labels.size() <= i)
        {
            gweni::controls::Label *label=canvas->newChild<gweni::controls::Label>();

            label->setTextColor(white);
            labels.push_back(label);
        }
        return labels[i];
    }

    std::vector<uint8_t> frame()
    {
        std::vector<uint8_t> pixels;

        gweni::test::clearView(background);
        canvas->updateCanvas();
        canvas->renderCanvas();
        gweni::test::readView(pixels);
        return pixels;
    }

    float scale;
    std::unique_ptr<gweni::skin::textured::TexturedSkin> skin;
    std::unique_ptr<gweni::controls::Canvas> canvas;
    std::vector<gweni::controls::Label *> labels;
};

//! What one run's ink did against its Label.
struct Result
{
    String text;
    int over[4]={ 0, 0, 0, 0 }; //!< device px of ink past the box: left, right, above, below
    int margin=0; //!< the fewest canvas units of clip margin that show every inked column
    int differing=0; //!< pixels around the Label unlike the run's alone
};

//! The worst of the runs of one size, scale, mode and end.
struct Worst
{
    int over[4]={ 0, 0, 0, 0 };
    int margin=0;
    String failing; //!< the glyph of every run whose Label differs
    String unexplained; //!< those of them whose ink stays inside the box
};

//! \p runs (each a canvas-unit origin and a string) drawn by themselves in
//! white on new text ids at \p scale, clipped to the view alone; the frame.
std::vector<uint8_t> runsAlone(const Font &font, float scale, const std::vector<std::pair<Point, String>> &runs)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point viewSize=gweni::test::viewSize();
    std::vector<size_t> ids(runs.size(), 0);
    std::vector<uint8_t> alone;

    for(size_t &id:ids)
        renderer.generateTextPrimitive(id);
    renderer.setScale(scale);
    renderer.setClipRegion(Rect(0, 0, static_cast<int>(std::ceil(viewSize.x/scale)),
        static_cast<int>(std::ceil(viewSize.y/scale))));
    gweni::test::clearView(background);
    renderer.begin();
    renderer.setDrawColor(white);
    for(size_t i=0; i < runs.size(); ++i)
        renderer.renderText(ids[i], font, runs[i].first, 1, runs[i].second);
    renderer.end();
    gweni::test::readView(alone);
    for(size_t id:ids)
        renderer.releaseTextPrimitive(id);
    renderer.begin();
    renderer.end();
    return alone;
}

//! The strings of \p size px in batches that fit the canvas, each laid out as
//! Labels and drawn alone; one Result per string, in order.
std::vector<Result> sweepSize(Sweep &sweep, int size, const std::vector<String> &strings)
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    const Point viewSize=gweni::test::viewSize();
    const float scale=sweep.scale;
    const Font &font=openSansAt(size);
    // the canvas's units, and the gaps between the Labels: wider than any
    // overhang, so each Label's region holds its own run's ink alone
    const int width=sweep.canvas->getWidth();
    const int height=sweep.canvas->getHeight();
    const int hgap=2*static_cast<int>(std::ceil(size/4.0))+4;
    const int vgap=4+static_cast<int>(std::ceil(size/6.0));
    const int lineHeight=renderer.measureText(font, "A").y;
    std::vector<Result> results;

    REQUIRE(lineHeight+vgap < height);

    size_t next=0;

    while(next < strings.size())
    {
        // a batch: rows of Labels, gap apart
        std::vector<Rect> boxes;
        int x=hgap/2+1, y=vgap/2+1;

        for(; next < strings.size(); ++next)
        {
            const Point measured=renderer.measureText(font, strings[next]);

            if(x+measured.x+hgap/2 >= width)
            {
                x=hgap/2+1;
                y+=lineHeight+vgap;
            }
            if(y+lineHeight+vgap/2 >= height)
                break;

            gweni::controls::Label *label=sweep.label(boxes.size());

            label->show();
            label->setFont(font);
            label->setText(strings[next]);
            label->setPos(x, y);
            boxes.push_back(Rect(x, y, measured.x, measured.y));
            x+=measured.x+hgap;
        }
        REQUIRE(!boxes.empty());

        const size_t first=next-boxes.size();

        for(size_t i=boxes.size(); i < sweep.labels.size(); ++i)
            sweep.labels[i]->hide();

        sweep.frame();

        const std::vector<uint8_t> labelled=sweep.frame();

        // each Label's Text box, as laid out
        for(size_t i=0; i < boxes.size(); ++i)
        {
            gweni::controls::Label *label=sweep.labels[i];
            const Point at=label->localPosToCanvas(Point(label->textX(), label->textY()));

            CAPTURE(strings[first+i]);
            REQUIRE(at.x == boxes[i].x);
            REQUIRE(at.y == boxes[i].y);
            REQUIRE(label->textWidth() == boxes[i].w);
            REQUIRE(label->textHeight() == boxes[i].h);
            REQUIRE(label->getWidth() == boxes[i].w);
        }

        // the runs alone, the Labels hidden, unclipped but by the view
        for(gweni::controls::Label *label:sweep.labels)
            label->hide();
        sweep.frame();

        std::vector<std::pair<Point, String>> runs;

        for(size_t i=0; i < boxes.size(); ++i)
            runs.emplace_back(Point(boxes[i].x, boxes[i].y), strings[first+i]);

        const std::vector<uint8_t> alone=runsAlone(font, scale, runs);

        for(size_t i=0; i < boxes.size(); ++i)
        {
            const Rect &box=boxes[i];
            Result result;

            result.text=strings[first+i];
            CAPTURE(result.text);

            // the box and half the gap round it, in device px
            const int left=SlotRenderer::snap(float(box.x), scale);
            const int right=SlotRenderer::snap(float(box.x+box.w), scale);
            const int top=SlotRenderer::snap(float(box.y), scale);
            const int bottom=SlotRenderer::snap(float(box.y+box.h), scale);
            const int x0=std::max(0, SlotRenderer::snap(float(box.x-hgap/2), scale));
            const int x1=std::min(viewSize.x, SlotRenderer::snap(float(box.x+box.w+hgap/2), scale));
            const int y0=std::max(0, SlotRenderer::snap(float(box.y-vgap/2), scale));
            const int y1=std::min(viewSize.y, SlotRenderer::snap(float(box.y+box.h+vgap/2), scale));
            int inkLeft=x1, inkRight=x0, inkTop=y1, inkBottom=y0;

            for(int py=y0; py < y1; ++py)
            {
                for(int px=x0; px < x1; ++px)
                {
                    const size_t at=(py*viewSize.x+px)*4;

                    if(!std::equal(&labelled[at], &labelled[at]+3, &alone[at]))
                        ++result.differing;
                    if(!inked(alone, px, py))
                        continue;
                    inkLeft=std::min(inkLeft, px);
                    inkRight=std::max(inkRight, px+1);
                    inkTop=std::min(inkTop, py);
                    inkBottom=std::max(inkBottom, py+1);
                }
            }
            REQUIRE(inkLeft < inkRight);
            // the region is wider than the ink: nothing of it reaches the gap's middle
            REQUIRE(inkLeft > x0);
            REQUIRE(inkRight < x1);
            REQUIRE(inkTop > y0);
            REQUIRE(inkBottom < y1);

            result.over[0]=std::max(0, left-inkLeft);
            result.over[1]=std::max(0, inkRight-right);
            result.over[2]=std::max(0, top-inkTop);
            result.over[3]=std::max(0, inkBottom-bottom);
            while((SlotRenderer::snap(float(box.x-result.margin), scale) > inkLeft)
                || (SlotRenderer::snap(float(box.x+box.w+result.margin), scale) < inkRight))
                ++result.margin;
            results.push_back(result);
        }
    }
    return results;
}

}//namespace

TEST_CASE("Label ink: a Label sized to its text shows every inked pixel of its run")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;
    const bool report=std::getenv("GWENI_INK_SWEEP") != nullptr;

    // every printable glyph as the last, after an 'A', and as the first, before one
    std::vector<String> last, first;

    for(char c='!'; c <= '~'; ++c)
    {
        last.push_back(String("A")+c);
        first.push_back(String(1, c)+"A");
    }

    // (mode, size, scale index, end) -> the worst run
    std::map<std::tuple<int, int, int, int>, Worst> worst;
    std::vector<TextAA> modes;

    for(TextAA aa:{ TextAA::Grayscale, TextAA::LcdRGB })
    {
        renderer.setTextAA(aa);
        if(renderer.textAA() != aa)
            continue;
        modes.push_back(aa);

        for(int s=0; s < 5; ++s)
        {
            Sweep sweep(scales[s]);

            for(int size=8; size <= 24; ++size)
            {
                for(int end=0; end < 2; ++end)
                {
                    const std::vector<String> &strings=(end == 0)?last:first;
                    const std::vector<Result> results=sweepSize(sweep, size, strings);
                    Worst &w=worst[std::make_tuple(static_cast<int>(aa), size, s, end)];

                    for(const Result &result:results)
                    {
                        for(int side=0; side < 4; ++side)
                            w.over[side]=std::max(w.over[side], result.over[side]);
                        w.margin=std::max(w.margin, result.margin);
                        if(result.differing == 0)
                            continue;
                        w.failing+=result.text[(end == 0)?1:0];
                        if(!result.over[0] && !result.over[1] && !result.over[2] && !result.over[3])
                            w.unexplained+=result.text[(end == 0)?1:0];
                    }

                    CAPTURE(static_cast<int>(aa));
                    CAPTURE(size);
                    CAPTURE(scales[s]);
                    const String glyph=(end == 0)?"last":"first";

                    CAPTURE(glyph);
                    CAPTURE(w.failing);
                    CHECK(w.failing.empty());
                    CHECK(w.margin <= gweni::controls::Label::inkMargin(float(size)));
                }
            }
        }
    }

    if(!report)
        return;

    // the table: per mode and end, a row per size, a column per scale, each the
    // margin needed (canvas units) and the overhang (device px: left/right,
    // and above/below where there is any) of the worst runs; then the margin
    // each size needs over every scale, mode and end
    for(TextAA aa:modes)
    {
        for(int end=0; end < 2; ++end)
        {
            std::printf("\n%s, glyph %s: margin [L/R device px] (glyphs whose Label differs)\n",
                (aa == TextAA::Grayscale)?"grayscale":"LCD", (end == 0)?"last":"first");
            for(int size=8; size <= 24; ++size)
            {
                std::printf("%2d px", size);
                for(int s=0; s < 5; ++s)
                {
                    const Worst &w=worst[std::make_tuple(static_cast<int>(aa), size, s, end)];

                    std::printf(" | x%.2f m=%d [%d/%d]", scales[s], w.margin, w.over[0], w.over[1]);
                    if(w.over[2] || w.over[3])
                        std::printf(" v%d/%d", w.over[2], w.over[3]);
                    std::printf(" (%zu)", w.failing.size());
                    if(!w.unexplained.empty())
                        std::printf(" inside the box: %s", w.unexplained.c_str());
                }
                std::printf("\n");
            }
        }
    }
    std::printf("\nmargin needed per size, over every scale, mode and end:\n");
    for(int size=8; size <= 24; ++size)
    {
        int margin=0;

        for(const auto &entry:worst)
        {
            if(std::get<1>(entry.first) == size)
                margin=std::max(margin, entry.second.margin);
        }
        std::printf("%2d px: %d (Label::inkMargin = %d)\n", size, margin,
            gweni::controls::Label::inkMargin(float(size)));
    }
}

namespace
{

//! A canvas rect's device pixels at \p scale, its edges snapped as the
//! renderer snaps them: [x0, x1) x [y0, y1).
struct DeviceRect
{
    int x0, y0, x1, y1;

    bool contains(int x, int y) const { return (x >= x0) && (x < x1) && (y >= y0) && (y < y1); }
};

DeviceRect deviceRect(const Rect &rect, float scale)
{
    return { SlotRenderer::snap(float(rect.x), scale), SlotRenderer::snap(float(rect.y), scale),
        SlotRenderer::snap(float(rect.x+rect.w), scale), SlotRenderer::snap(float(rect.y+rect.h), scale) };
}

//! \p control's bounds in canvas units.
Rect canvasRect(gweni::controls::Base *control)
{
    const Point at=control->localPosToCanvas();

    return Rect(at.x, at.y, control->getWidth(), control->getHeight());
}

//! The pixels where \p a and \p b differ, outside \p rect only if \p outside.
int differing(const std::vector<uint8_t> &a, const std::vector<uint8_t> &b, const DeviceRect *outside=nullptr)
{
    const Point viewSize=gweni::test::viewSize();
    int count=0;

    for(int y=0; y < viewSize.y; ++y)
    {
        for(int x=0; x < viewSize.x; ++x)
        {
            const size_t at=(y*viewSize.x+x)*4;

            if(outside && outside->contains(x, y))
                continue;
            if(!std::equal(&a[at], &a[at]+3, &b[at]))
                ++count;
        }
    }
    return count;
}

//! Whether \p frame has ink outside \p rect.
bool inkOutside(const std::vector<uint8_t> &frame, const DeviceRect &rect)
{
    const Point viewSize=gweni::test::viewSize();

    for(int y=0; y < viewSize.y; ++y)
    {
        for(int x=0; x < viewSize.x; ++x)
        {
            if(!rect.contains(x, y) && inked(frame, x, y))
                return true;
        }
    }
    return false;
}

//! \p frame with every pixel outside \p rect the background.
std::vector<uint8_t> keepInside(std::vector<uint8_t> frame, const DeviceRect &rect)
{
    const Point viewSize=gweni::test::viewSize();

    for(int y=0; y < viewSize.y; ++y)
    {
        for(int x=0; x < viewSize.x; ++x)
        {
            if(rect.contains(x, y))
                continue;

            uint8_t *pixel=&frame[(y*viewSize.x+x)*4];

            pixel[0]=background.r;
            pixel[1]=background.g;
            pixel[2]=background.b;
        }
    }
    return frame;
}

//! A few frames, so the layout and the render queue settle; the last one.
std::vector<uint8_t> settle(Sweep &sweep)
{
    sweep.frame();
    sweep.frame();
    return sweep.frame();
}

//! A Label of \p text in \p font at (6, 3), its size fixed at \p width wide and
//! the text's height high.
gweni::controls::Label *fixedLabel(Sweep &sweep, const Font &font, const String &text, int width)
{
    gweni::controls::Label *label=sweep.label(0);

    label->setFont(font);
    label->setText(text);
    label->setSizeFlags(gweni::SizeFlags(gweni::SizeFlag::Fixed, gweni::SizeFlag::Fixed));
    label->setBounds(6, 3, width, gweni::test::gpuRenderer().measureText(font, text).y);
    return label;
}

//! The frame of a fresh canvas at \p scale holding fixedLabel(text, width).
std::vector<uint8_t> freshFixedLabel(float scale, const Font &font, const String &text, int width)
{
    Sweep sweep(scale);
    gweni::controls::Label *label=fixedLabel(sweep, font, text, width);
    const std::vector<uint8_t> frame=settle(sweep);

    REQUIRE(label->getWidth() == width);
    return frame;
}

}//namespace

// The margin is for text that fits: a Label narrower than its text (its Text
// keeps the measured width, so it reaches past the Label's right edge) is cut
// at its bounds on both sides, as before #571.
TEST_CASE("Label ink: a Label narrower than its text clips it at its bounds")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;
    const Font &font=openSansAt(10);
    // ink past the pens on both sides: a leading 'J', a trailing 'g'
    const String text="JAg";

    renderer.setTextAA(TextAA::Grayscale);
    for(float scale:{ 1.0f, 1.25f, 2.0f })
    {
        CAPTURE(scale);

        Sweep sweep(scale);
        const Point measured=renderer.measureText(font, text);
        gweni::controls::Label *label=fixedLabel(sweep, font, text, measured.x-4);
        const std::vector<uint8_t> frame=settle(sweep);
        const Rect bounds=canvasRect(label);
        const Point at=label->localPosToCanvas(Point(label->textX(), label->textY()));

        REQUIRE((bounds == Rect(6, 3, measured.x-4, measured.y)));
        REQUIRE(label->textWidth() == measured.x);

        label->hide();
        sweep.frame();

        const std::vector<uint8_t> alone=runsAlone(font, scale, { { at, text } });
        const DeviceRect device=deviceRect(bounds, scale);

        // the run does reach past the Label, and the Label shows it up to its bounds alone
        REQUIRE(inkOutside(alone, device));
        CHECK(differing(frame, keepInside(alone, device)) == 0);
    }
}

// A TextBox's text scrolled to its caret lies partly outside the box, so it is
// cut at the box's bounds: nothing of it draws outside them, whether scrolled
// to the end (the text's start left of the box) or back to the start (its end
// right of it).
TEST_CASE("Label ink: a TextBox's scrolled text clips at its bounds")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;

    renderer.setTextAA(TextAA::Grayscale);
    for(float scale:{ 1.0f, 1.25f })
    {
        CAPTURE(scale);

        Sweep sweep(scale);
        const std::vector<uint8_t> empty=settle(sweep);
        gweni::controls::TextBox *box=sweep.canvas->newChild<gweni::controls::TextBox>();

        box->setBounds(20, 10, 90, 20);
        box->setText("JumpingQuicklyTheWizardsJinxJuggledFoxesAgain");
        box->focus();
        box->moveCaretToend();

        const std::vector<uint8_t> atEnd=settle(sweep);
        const DeviceRect device=deviceRect(canvasRect(box), scale);

        REQUIRE(box->textX() < 0);
        CHECK(differing(atEnd, empty, &device) == 0);

        box->moveCaretToStart();

        const std::vector<uint8_t> atStart=settle(sweep);

        REQUIRE(box->textX() >= 0);
        REQUIRE(box->textRight() > box->getWidth());
        CHECK(differing(atStart, empty, &device) == 0);
        CHECK(differing(atStart, atEnd) > 0);
    }
}

// The widened rect is cut by the region the Label is in: Labels at the left
// and right edges of a ScrollControl's viewport, whose runs reach past it,
// change no pixel outside the viewport.
TEST_CASE("Label ink: a Label in a ScrollControl draws nothing outside its viewport")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;
    const Font &font=openSansAt(10);

    renderer.setTextAA(TextAA::Grayscale);
    for(float scale:{ 1.25f, 2.0f })
    {
        CAPTURE(scale);

        Sweep sweep(scale);
        gweni::controls::ScrollControl *scroll=sweep.canvas->newChild<gweni::controls::ScrollControl>();

        scroll->setBounds(30, 10, 120, 40);
        scroll->setScroll(false, false);

        gweni::controls::Label *left=scroll->newChild<gweni::controls::Label>();
        gweni::controls::Label *right=scroll->newChild<gweni::controls::Label>();

        for(gweni::controls::Label *label:{ left, right })
        {
            label->setFont(font);
            label->setTextColor(white);
        }
        left->setText("JA");
        right->setText("Ag");
        left->setPos(0, 4);
        settle(sweep);

        const Rect &viewport=scroll->getViewport();
        const Point viewportAt=scroll->localPosToCanvas(Point(viewport.x, viewport.y));
        const Rect onCanvas(viewportAt.x, viewportAt.y, viewport.w, viewport.h);

        right->setPos(viewport.w-right->getWidth(), 4);

        const std::vector<uint8_t> frame=settle(sweep);

        REQUIRE(canvasRect(left).x == onCanvas.x);
        REQUIRE(canvasRect(right).x+right->getWidth() == onCanvas.x+onCanvas.w);

        // the runs reach past the viewport on both sides
        std::vector<std::pair<Point, String>> runs;

        for(gweni::controls::Label *label:{ left, right })
            runs.emplace_back(label->localPosToCanvas(Point(label->textX(), label->textY())), label->getText());
        left->hide();
        right->hide();

        const std::vector<uint8_t> without=settle(sweep);
        const std::vector<uint8_t> alone=runsAlone(font, scale, runs);
        const DeviceRect device=deviceRect(onCanvas, scale);

        REQUIRE(inkOutside(alone, device));
        CHECK(differing(frame, without) > 0);
        CHECK(differing(frame, without, &device) == 0);
    }
}

// The clip rect follows the fit both ways, and its change reaches the slots
// already drawn: a Label shrunk below its text and grown back, or given a
// longer string and the first back, draws what a fresh build of each state
// draws, starting from either state.
TEST_CASE("Label ink: a Label's fit toggled draws as a fresh build of each state")
{
    SlotRenderer &renderer=gweni::test::gpuRenderer();
    RestoreTextAA restore;
    const Font &font=openSansAt(10);
    const String text="JAg";
    const String longer="JAgg";

    renderer.setTextAA(TextAA::Grayscale);
    for(float scale:{ 1.25f, 2.0f })
    {
        CAPTURE(scale);

        const int fits=renderer.measureText(font, text).x;
        const int narrow=fits-4;

        SUBCASE("the Label resized")
        {
            const std::vector<uint8_t> freshFits=freshFixedLabel(scale, font, text, fits);
            const std::vector<uint8_t> freshNarrow=freshFixedLabel(scale, font, text, narrow);

            REQUIRE(differing(freshFits, freshNarrow) > 0);

            for(bool startFitting:{ true, false })
            {
                CAPTURE(startFitting);

                Sweep sweep(scale);
                gweni::controls::Label *label=fixedLabel(sweep, font, text, startFitting?fits:narrow);

                CHECK(differing(settle(sweep), startFitting?freshFits:freshNarrow) == 0);
                label->setWidth(startFitting?narrow:fits);
                CHECK(differing(settle(sweep), startFitting?freshNarrow:freshFits) == 0);
                label->setWidth(startFitting?fits:narrow);
                CHECK(differing(settle(sweep), startFitting?freshFits:freshNarrow) == 0);
            }
        }

        SUBCASE("the Label's text changed")
        {
            const std::vector<uint8_t> freshFits=freshFixedLabel(scale, font, text, fits);
            const std::vector<uint8_t> freshLonger=freshFixedLabel(scale, font, longer, fits);

            REQUIRE(differing(freshFits, freshLonger) > 0);

            for(bool startFitting:{ true, false })
            {
                CAPTURE(startFitting);

                Sweep sweep(scale);
                gweni::controls::Label *label=fixedLabel(sweep, font, startFitting?text:longer, fits);

                CHECK(differing(settle(sweep), startFitting?freshFits:freshLonger) == 0);
                label->setText(startFitting?longer:text);
                CHECK(differing(settle(sweep), startFitting?freshLonger:freshFits) == 0);
                label->setText(startFitting?text:longer);
                CHECK(differing(settle(sweep), startFitting?freshFits:freshLonger) == 0);
            }
        }
    }
}
