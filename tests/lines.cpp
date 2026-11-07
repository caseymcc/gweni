/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  OpenGLCore's line segments (style 3, drawLine), drawn offscreen and read back
 *  pixel by pixel at scales 1, 1.5 and 2, against a model of what drawLine
 *  places: a stroke of T = max(1, snap(thickness)) whole device px, whose edges
 *  across a horizontal or vertical segment are on pixel boundaries, and a pixel's
 *  coverage the part of it between the stroke's two edges, each a straight edge
 *  T/2 from the segment across the pixel, normal to the direction from the
 *  nearest point of the segment (round caps) to the pixel's centre:
 *
 *    thickness  a horizontal or vertical segment is T whole fully covered rows
 *               (columns) along its length with nothing partly covered beside
 *               them, and its round caps reach T/2 past its endpoints;
 *    diagonal   a 45 degree segment's coverage summed across a row is within
 *               5 % of T*sqrt(2), and swapping its endpoints draws the same;
 *    joints     the segments of a polyline at one z leave no pixel any of them
 *               covers fully less than fully covered, and a pixel several of
 *               them cover is between the most covering one and all of them
 *               blended (a partly covered pixel writes its depth up to half a
 *               layer behind, see the style 3 branch of the fragment shader).
 *               At 50 % alpha, a pixel the first segment drawn over it covers
 *               fully is blended once however many others cover it; where the
 *               first covers it partly (the anti-aliased rim of a cap's disc), a
 *               later one blends over it too, so a translucent joint can be
 *               darker on that rim, never more than all of them blended;
 *    outside    every pixel farther than T/2 + sqrt(2)/2 (half a pixel's diagonal)
 *               from the segment, T/2 + 0.5 for a horizontal or vertical one, is
 *               untouched,
 *               depth included, and the clip region and the +/-32 K device px
 *               cull apply as to every slot;
 *    retained   drawing the same segment again writes and uploads nothing,
 *               moving one endpoint rewrites that slot only, and hiding and
 *               showing it keeps style 3.
 *
 *  The renderer is shared with the other cases, so each case puts the scale, the
 *  render offset and the clip region back when it ends.
 *
 *  GWENI_LINES_DUMP=<dir> writes the polyline frame at scale 1.5 to
 *  <dir>/lines_1.5x.png, to look at.
 */
#include <doctest/doctest.h>

#include "capture.h"
#include "gpuRenderer.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <string>
#include <vector>

namespace
{

using gweni::Color;
using gweni::Point;
using gweni::PointF;
using gweni::Rect;
using gweni::renderer::OpenGLCore;

const Color background(0, 0, 255, 255);
const Color red(255, 0, 0, 255);

const float scales[]={1.0f, 1.5f, 2.0f};

//! The render offset each case draws at, so a logical position is not a multiple
//! of the scale.
const Point offset(3, 2);

//! One frame read back from the offscreen framebuffer, top-down RGBA8.
struct Frame
{
    std::vector<uint8_t> pixels;

    Color at(int x, int y) const
    {
        const uint8_t *p=&pixels[(static_cast<size_t>(y)*gweni::test::viewSize().x+x)*4];

        return Color(p[0], p[1], p[2], p[3]);
    }

    //! The line's coverage at (x,y): red over blue, so red's channel is it.
    double coverage(int x, int y) const
    {
        return at(x, y).r/255.0;
    }
};

//! Clears to `background`, draws one frame with `draw` and reads it back.
template<typename Draw>
Frame drawFrame(Draw &&draw)
{
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    const Point size=gweni::test::viewSize();

    glClearColor(background.r/255.0f, background.g/255.0f, background.b/255.0f, 1.0f);
    glClearDepth(1.0);
    glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);

    renderer.begin();
    draw(renderer);
    renderer.end();

    Frame frame;

    gweni::capture::readFramebufferRgba(size.x, size.y, frame.pixels);
    return frame;
}

//! Puts the scale, the render offset and the clip region back, so the next case
//! draws at 1x over the whole view.
struct ScaleGuard
{
    explicit ScaleGuard(float scale)
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setScale(scale);
        renderer.setRenderOffset(offset);
    }

    ~ScaleGuard()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        renderer.setScale(1.0f);
        renderer.setRenderOffset(Point(0, 0));
        renderer.setClipRegion(Rect(Point(0, 0), gweni::test::viewSize()));
    }
};

//! Primitive ids for one test case, released (and that flushed) when it ends, so the
//! next case's frame starts empty.
struct Primitives
{
    explicit Primitives(size_t count)
        : ids(count)
    {
        for(size_t &id:ids)
            gweni::test::gpuRenderer().generatePrimitive(id);
        std::sort(ids.begin(), ids.end());
    }

    ~Primitives()
    {
        OpenGLCore &renderer=gweni::test::gpuRenderer();

        for(size_t id:ids)
            renderer.releasePrimitive(id);
        renderer.begin();
        renderer.end();
    }

    std::vector<size_t> ids;
};

//! A segment as drawLine places it, in device px.
struct Segment
{
    double ax, ay, bx, by;
    int stroke;

    //! Distance from the centre of pixel (x,y) to the segment, and the unit
    //! direction from the segment's nearest point to it (the segment's normal on it).
    double distance(int x, int y, double *nx=nullptr, double *ny=nullptr) const
    {
        const double px=x+0.5-ax;
        const double py=y+0.5-ay;
        const double dx=bx-ax;
        const double dy=by-ay;
        const double length2=dx*dx+dy*dy;
        const double h=length2 > 0.0 ? std::min(std::max((px*dx+py*dy)/length2, 0.0), 1.0) : 0.0;
        const double vx=px-dx*h;
        const double vy=py-dy*h;
        const double d=std::hypot(vx, vy);

        if(nx && ny)
        {
            if(d > 1e-4)
                *nx=vx/d, *ny=vy/d;
            else if(length2 > 0.0)
                *nx=-dy/std::sqrt(length2), *ny=dx/std::sqrt(length2);
            else
                *nx=0.0, *ny=1.0;
        }
        return d;
    }

    //! The part of pixel (x,y) between the stroke's edges.
    double coverage(int x, int y) const
    {
        double nx, ny;
        const double d=distance(x, y, &nx, &ny);
        const double r=0.5*stroke;

        return std::min(std::max(pixelArea(r-d, nx, ny)+pixelArea(r+d, nx, ny)-1.0, 0.0), 1.0);
    }

    //! The part of a pixel inside a straight edge e px from its centre, with
    //! normal (nx,ny): the fragment shader's pixelArea().
    static double pixelArea(double e, double nx, double ny)
    {
        const double a=std::max(std::abs(nx), std::abs(ny));
        const double b=std::min(std::abs(nx), std::abs(ny));
        const double h1=0.5*(a-b);
        const double h2=0.5*(a+b);

        if(e <= -h2)
            return 0.0;
        if(e >= h2)
            return 1.0;
        if(e < -h1)
            return (e+h2)*(e+h2)/(2.0*a*b);
        if(e > h1)
            return 1.0-(h2-e)*(h2-e)/(2.0*a*b);
        return 0.5+e/a;
    }

    //! Whether pixel (x,y) is past every pixel the stroke can reach part of.
    bool beyond(int x, int y) const
    {
        return distance(x, y) > 0.5*stroke+std::sqrt(0.5)+0.01;
    }
};

//! The stroke drawLine gives `thickness` at `scale`.
int strokeAt(float thickness, float scale)
{
    return std::max(1, OpenGLCore::snap(thickness, scale));
}

//! Where drawLine puts a segment: each endpoint coordinate's stroke starts on a
//! whole px, floor(v*scale - T/2 + 0.5) with the render offset, and the endpoint
//! is its middle.
Segment deviceSegment(PointF p0, PointF p1, float thickness, float scale)
{
    const int stroke=strokeAt(thickness, scale);
    auto place=[&](float v, int origin)
    {
        return std::floor((double(v)+origin)*double(scale)-0.5*stroke+0.5)+0.5*stroke;
    };

    return {place(p0.x, offset.x), place(p0.y, offset.y), place(p1.x, offset.x), place(p1.y, offset.y),
        stroke};
}

bool isBackground(const Color &c)
{
    return std::abs(c.r-background.r) <= 2 && std::abs(c.g-background.g) <= 2
        && std::abs(c.b-background.b) <= 2;
}

bool isLine(const Color &c)
{
    return std::abs(c.r-red.r) <= 2 && std::abs(c.g-red.g) <= 2 && std::abs(c.b-red.b) <= 2;
}

//! The device px the segments can reach, and 4 more on each side.
struct Area
{
    int x0, y0, x1, y1;
};

Area areaOf(const std::vector<Segment> &segments)
{
    Area area{1<<30, 1<<30, -(1<<30), -(1<<30)};

    for(const Segment &s : segments)
    {
        const double r=0.5*s.stroke+4.0;

        area.x0=std::min(area.x0, int(std::floor(std::min(s.ax, s.bx)-r)));
        area.y0=std::min(area.y0, int(std::floor(std::min(s.ay, s.by)-r)));
        area.x1=std::max(area.x1, int(std::ceil(std::max(s.ax, s.bx)+r)));
        area.y1=std::max(area.y1, int(std::ceil(std::max(s.ay, s.by)+r)));
    }
    area.x0=std::max(area.x0, 0);
    area.y0=std::max(area.y0, 0);
    area.x1=std::min(area.x1, gweni::test::viewSize().x);
    area.y1=std::min(area.y1, gweni::test::viewSize().y);
    return area;
}

//! Pixels of the frame that are not the background, anywhere.
int notBackground(const Frame &frame)
{
    const Point size=gweni::test::viewSize();
    int count=0;

    for(int y=0; y<size.y; ++y)
        for(int x=0; x<size.x; ++x)
            if(!isBackground(frame.at(x, y)))
                ++count;
    return count;
}

//! Pixels of one segment's frame whose coverage is not the model's (within 3/255),
//! and pixels outside it (its coverage 0) that are not exactly the background.
struct Mismatches
{
    int coverage=0;
    int outside=0;
    double worst=0.0;
};

Mismatches compareWithModel(const Frame &frame, const Segment &segment)
{
    const Area area=areaOf({segment});
    Mismatches result;

    for(int y=area.y0; y<area.y1; ++y)
    {
        for(int x=area.x0; x<area.x1; ++x)
        {
            const double expected=segment.coverage(x, y);
            const double error=std::abs(frame.coverage(x, y)-expected);

            result.worst=std::max(result.worst, error);
            if(error > 3.0/255.0)
                ++result.coverage;
            if(segment.beyond(x, y) && !isBackground(frame.at(x, y)))
                ++result.outside;
        }
    }
    return result;
}

//! The lengths of the runs of line pixels along a row (dx=1) or a column (dy=1).
std::vector<int> runs(const Frame &frame, int x, int y, int dx, int dy, int count)
{
    std::vector<int> lengths;
    int run=0;

    for(int i=0; i<count; ++i)
    {
        if(isLine(frame.at(x+i*dx, y+i*dy)))
            ++run;
        else if(run > 0)
        {
            lengths.push_back(run);
            run=0;
        }
    }
    if(run > 0)
        lengths.push_back(run);
    return lengths;
}

}//namespace

TEST_CASE("Line (style 3): a horizontal or vertical segment is max(1, round(t*s)) whole rows (columns), with round caps")
{
    Primitives prims(1);
    const float thicknesses[]={1.0f, 1.5f, 2.0f, 3.0f};
    // the endpoints' fractions, so every phase of the device grid is met
    const float phases[]={0.0f, 0.25f, 0.5f, 0.75f};

    for(float scale : scales)
    {
        ScaleGuard guard(scale);

        for(float thickness : thicknesses)
        {
            for(float phase : phases)
            {
                for(int vertical=0; vertical <= 1; ++vertical)
                {
                    const PointF p0=vertical ? PointF(30+phase, 8) : PointF(20, 30+phase);
                    const PointF p1=vertical ? PointF(30+phase, 60) : PointF(120, 30+phase);
                    const Segment segment=deviceSegment(p0, p1, thickness, scale);
                    const int s=segment.stroke;

                    CAPTURE(scale);
                    CAPTURE(thickness);
                    CAPTURE(phase);
                    CAPTURE(vertical);
                    CHECK(s == std::max(1, int(std::lround(thickness*scale+0.001))));

                    const Frame frame=drawFrame([&](OpenGLCore &renderer)
                        {
                            renderer.setDrawColor(red);
                            renderer.drawLine(prims.ids[0], p0, p1, thickness, 1);
                        });

                    const Mismatches model=compareWithModel(frame, segment);

                    CAPTURE(model.worst);
                    CHECK(model.coverage == 0);
                    CHECK(model.outside == 0);

                    // across the stroke, between its endpoints: s fully covered
                    // rows (columns) starting at a whole px, and nothing partly
                    // covered beside them
                    const int first=int(std::lround((vertical ? segment.ax : segment.ay)-0.5*s));
                    const int along0=int(std::ceil(vertical ? segment.ay : segment.ax));
                    const int along1=int(std::floor(vertical ? segment.by : segment.bx));
                    int wrong=0;

                    for(int along=along0; along<along1; ++along)
                    {
                        for(int across=first-3; across<first+s+3; ++across)
                        {
                            const Color pixel=vertical ? frame.at(across, along) : frame.at(along, across);
                            const bool inStroke=across >= first && across < first+s;

                            if(inStroke ? !isLine(pixel) : !isBackground(pixel))
                                ++wrong;
                        }
                    }
                    CHECK(wrong == 0);

                    const int middle=(along0+along1)/2;

                    if(vertical)
                        CHECK(runs(frame, first-3, middle, 1, 0, s+6) == std::vector<int>{s});
                    else
                        CHECK(runs(frame, middle, first-3, 0, 1, s+6) == std::vector<int>{s});

                    // the round cap past the second endpoint: on the stroke's middle
                    // row it is covered to T/2 past the endpoint and nothing after;
                    // the corner of its square is not fully covered
                    const double end=vertical ? segment.by : segment.bx;
                    const int last=int(std::floor(end+0.5*s))-1;
                    const int mid=first+s/2;
                    auto pixelAt=[&](int along, int across)
                    {
                        return vertical ? frame.at(across, along) : frame.at(along, across);
                    };

                    CHECK(pixelAt(last, mid).r >= 128);
                    CHECK(isBackground(pixelAt(last+2, mid)));
                    if(s >= 2)
                        CHECK(!isLine(pixelAt(last, first)));
                }
            }
        }
    }
}

TEST_CASE("Line (style 3): a 45 degree segment covers t*s*sqrt(2) a row, and drawing it backwards draws the same")
{
    Primitives prims(1);
    const float thicknesses[]={1.0f, 2.0f, 3.0f};

    for(float scale : scales)
    {
        ScaleGuard guard(scale);

        for(float thickness : thicknesses)
        {
            const PointF p0(20, 5);
            const PointF p1(60, 45);
            const Segment segment=deviceSegment(p0, p1, thickness, scale);
            const int s=segment.stroke;

            CAPTURE(scale);
            CAPTURE(thickness);

            const Frame forward=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.setDrawColor(red);
                    renderer.drawLine(prims.ids[0], p0, p1, thickness, 1);
                });
            const Frame backward=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.setDrawColor(red);
                    renderer.drawLine(prims.ids[0], p1, p0, thickness, 1);
                });

            const Mismatches model=compareWithModel(forward, segment);

            CAPTURE(model.worst);
            CHECK(model.coverage == 0);
            CHECK(model.outside == 0);

            // each row away from the caps: the stroke's area per row
            const Area area=areaOf({segment});
            const double expected=s*std::sqrt(2.0);
            double worst=0.0;
            int rows=0;

            for(int y=int(segment.ay)+s+2; y<int(segment.by)-s-2; ++y, ++rows)
            {
                double sum=0.0;

                for(int x=area.x0; x<area.x1; ++x)
                    sum+=forward.coverage(x, y);
                worst=std::max(worst, std::abs(sum-expected)/expected);
            }
            CAPTURE(worst);
            CHECK(rows > 20);
            CHECK(worst <= 0.05);

            // p0 and p1 swapped: the same pixels, give or take rounding
            int different=0;

            for(int y=area.y0; y<area.y1; ++y)
            {
                for(int x=area.x0; x<area.x1; ++x)
                {
                    const Color a=forward.at(x, y);
                    const Color b=backward.at(x, y);

                    if(std::abs(a.r-b.r) > 1 || std::abs(a.g-b.g) > 1 || std::abs(a.b-b.b) > 1)
                        ++different;
                }
            }
            CHECK(different == 0);
        }
    }
}

TEST_CASE("Line (style 3): the segments of a polyline join without a gap and blend once where they overlap")
{
    // a gentle and a sharp zigzag, each of three segments sharing their endpoints
    const std::vector<std::vector<PointF>> polylines=
    {
        {PointF(15, 45), PointF(50, 10), PointF(85, 48), PointF(105, 12)},
        {PointF(120, 10), PointF(150, 50), PointF(160, 12), PointF(205, 40)},
    };
    const float thicknesses[]={1.0f, 2.0f, 3.0f};
    const Color translucent(255, 0, 0, 128);
    Primitives prims(6);

    for(float scale : scales)
    {
        ScaleGuard guard(scale);

        for(float thickness : thicknesses)
        {
            std::vector<Segment> segments;

            for(const std::vector<PointF> &points : polylines)
                for(size_t i=0; i+1<points.size(); ++i)
                    segments.push_back(deviceSegment(points[i], points[i+1], thickness, scale));

            auto draw=[&](const Color &color)
            {
                return drawFrame([&](OpenGLCore &renderer)
                    {
                        size_t slot=0;

                        renderer.setDrawColor(color);
                        for(const std::vector<PointF> &points : polylines)
                            for(size_t i=0; i+1<points.size(); ++i)
                                renderer.drawLine(prims.ids[slot++], points[i], points[i+1], thickness, 1);
                    });
            };

            CAPTURE(scale);
            CAPTURE(thickness);

            const Frame opaque=draw(red);
            const Frame half=draw(translucent);

            if(scale == 1.5f && thickness == 2.0f)
            {
                if(const char *dir=std::getenv("GWENI_LINES_DUMP"))
                    gweni::capture::writeRgbaPng(opaque.pixels, gweni::test::viewSize().x,
                        gweni::test::viewSize().y, std::string(dir)+"/lines_1.5x.png");
            }

            // the colour of one 50 % blend of red over the background
            // one 50 % blend of red over the background
            const Color once(128, 0, 127, 255);

            const Area area=areaOf(segments);
            int gaps=0;
            int outside=0;
            int unbounded=0;
            int darker=0;
            int rim=0;

            for(int y=area.y0; y<area.y1; ++y)
            {
                for(int x=area.x0; x<area.x1; ++x)
                {
                    double most=0.0;
                    double uncovered=1.0;
                    double halfUncovered=1.0;
                    bool beyond=true;
                    double firstCoverage=0.0;

                    for(const Segment &segment : segments)
                    {
                        const double c=segment.coverage(x, y);

                        if(firstCoverage == 0.0)
                            firstCoverage=c;
                        most=std::max(most, c);
                        uncovered*=1.0-c;
                        halfUncovered*=1.0-0.5*c;
                        beyond=beyond && segment.beyond(x, y);
                    }

                    // at 50 %: blended once where the first segment drawn over the
                    // pixel covers it fully (the later ones fail the depth test);
                    // where it covers it partly, a later one blends over it too,
                    // never more than all of them blended
                    const Color pixel=half.at(x, y);

                    if(firstCoverage == 1.0)
                    {
                        if(std::abs(pixel.r-once.r) > 2 || std::abs(pixel.b-once.b) > 2)
                            ++darker;
                    }
                    else if(pixel.r/255.0 > 1.0-halfUncovered+3.0/255.0)
                        ++rim;

                    const double coverage=opaque.coverage(x, y);

                    if(most == 1.0)
                    {
                        if(!isLine(opaque.at(x, y)))
                            ++gaps;
                    }
                    else if(beyond)
                    {
                        if(!isBackground(opaque.at(x, y)) || !isBackground(half.at(x, y)))
                            ++outside;
                    }
                    else if(coverage < most-3.0/255.0 || coverage > 1.0-uncovered+3.0/255.0)
                        ++unbounded;
                }
            }
            // the centre line of every segment: no pixel on it left uncovered, nor
            // less covered than that segment covers it (a slanted 1 px stroke
            // covers no pixel fully)
            int thin=0;

            for(const Segment &segment : segments)
            {
                const double length=std::hypot(segment.bx-segment.ax, segment.by-segment.ay);

                for(double t=0.0; t <= length; t+=0.25)
                {
                    const int x=int(std::floor(segment.ax+(segment.bx-segment.ax)*t/length));
                    const int y=int(std::floor(segment.ay+(segment.by-segment.ay)*t/length));

                    if(opaque.coverage(x, y) < std::max(0.25, segment.coverage(x, y)-3.0/255.0))
                        ++thin;
                }
            }
            CHECK(thin == 0);
            CHECK(gaps == 0);
            CHECK(darker == 0);
            CHECK(rim == 0);
            CHECK(outside == 0);
            CHECK(unbounded == 0);
        }
    }
}

TEST_CASE("Line (style 3): pixels a line leaves uncovered write no depth, and the clip region and the 32 K cull apply")
{
    Primitives prims(2);
    const Color green(0, 255, 0, 255);
    const PointF p0(20, 10);
    const PointF p1(90, 40);

    for(float scale : scales)
    {
        ScaleGuard guard(scale);
        const Segment segment=deviceSegment(p0, p1, 2.0f, scale);
        const Area area=areaOf({segment});

        CAPTURE(scale);

        SUBCASE("a rect at a lower z drawn after the line shows wherever the line leaves a pixel uncovered")
        {
            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.setDrawColor(red);
                    renderer.drawLine(prims.ids[0], p0, p1, 2.0f, 2);
                    renderer.drawRoundedRect(prims.ids[1], Rect(10, 2, 100, 50), 0.0f, 0.0f,
                        green, green, green, 1);
                });
            int wrong=0;

            for(int y=area.y0; y<area.y1; ++y)
            {
                for(int x=area.x0; x<area.x1; ++x)
                {
                    const Color pixel=frame.at(x, y);
                    const double c=segment.coverage(x, y);

                    if(c == 1.0 ? !isLine(pixel) : (c == 0.0 && (pixel.g < 250 || pixel.r > 5)))
                        ++wrong;
                }
            }
            CHECK(wrong == 0);
        }

        SUBCASE("the clip region cuts it")
        {
            const Rect clip(40, 0, 30, 60);
            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.setClipRegion(clip);
                    renderer.setDrawColor(red);
                    renderer.drawLine(prims.ids[0], p0, p1, 2.0f, 1);
                });
            const int x0=OpenGLCore::snap(float(clip.x), scale);
            const int x1=OpenGLCore::snap(float(clip.x+clip.w), scale);
            int outsideClip=0;
            int inside=0;

            for(int y=area.y0; y<area.y1; ++y)
            {
                for(int x=area.x0; x<area.x1; ++x)
                {
                    const bool shown=!isBackground(frame.at(x, y));

                    if(x < x0 || x >= x1)
                        outsideClip+=shown;
                    else
                    {
                        inside+=shown;
                        if(std::abs(frame.coverage(x, y)-segment.coverage(x, y)) > 3.0/255.0)
                            ++outsideClip;
                    }
                }
            }
            CHECK(outsideClip == 0);
            CHECK(inside > 0);
        }

        SUBCASE("an endpoint past +/-32767 device px draws nothing")
        {
            const Frame frame=drawFrame([&](OpenGLCore &renderer)
                {
                    renderer.setDrawColor(red);
                    renderer.drawLine(prims.ids[0], p0, PointF(90, 40000), 2.0f, 1);
                });

            CHECK(notBackground(frame) == 0);
        }
    }
}

TEST_CASE("Line (style 3): a segment drawn again writes nothing, a moved endpoint its slot only, and hide/show keeps style 3")
{
    Primitives prims(3);
    OpenGLCore &renderer=gweni::test::gpuRenderer();
    ScaleGuard guard(1.5f);
    std::vector<PointF> points={PointF(15, 45), PointF(50, 10), PointF(85, 48), PointF(105, 12)};

    auto draw=[&]()
    {
        return drawFrame([&](OpenGLCore &renderer)
            {
                renderer.setDrawColor(red);
                for(size_t i=0; i<3; ++i)
                    renderer.drawLine(prims.ids[i], points[i], points[i+1], 2.0f, 1);
            });
    };
    auto styleOf=[&](size_t id)
    {
        const OpenGLCore::MirrorSnapshot mirror=renderer.snapshotMirror();
        const std::vector<uint8_t> &bytes=mirror.primitives.at(id);
        uint16_t style=0;

        std::copy(bytes.begin(), bytes.begin()+sizeof(style), reinterpret_cast<uint8_t *>(&style));
        return style;
    };

    const Frame first=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 3);
    CHECK(styleOf(prims.ids[0]) == 3);

    const Frame again=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 0);
    CHECK(renderer.getFrameStats().uploadBytes == 0);
    CHECK(again.pixels == first.pixels);

    // the last endpoint is the third segment's alone
    points[3]=PointF(110, 20);

    const Frame moved=draw();

    CHECK(renderer.getFrameStats().primitiveSlotsWritten == 1);
    CHECK(moved.pixels != first.pixels);

    points[3]=PointF(105, 12);
    draw();

    renderer.hidePrimitive(prims.ids[1]);
    CHECK(styleOf(prims.ids[1]) == 0);

    const Frame hidden=drawFrame([](OpenGLCore &) {});

    CHECK(hidden.pixels != first.pixels);

    renderer.showPrimitive(3, prims.ids[1]);
    CHECK(styleOf(prims.ids[1]) == 3);

    const Frame shown=drawFrame([](OpenGLCore &) {});

    CHECK(shown.pixels == first.pixels);
}
