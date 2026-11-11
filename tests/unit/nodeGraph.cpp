/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  NodeGraph, Node and Pin: the node graph's view skeleton, on the textured
 *  skin's real NodeGraph, Node and Pin renderers over a recording StubRenderer
 *  (a StubControl for every other type). It checks:
 *  - a node's place: getGraphPosition() is what setGraphPosition() asked,
 *    negative points included, its content point drawn at pan()+p*zoom() in
 *    the graph, NodeWidth wide and as high as its title, pin row and body,
 *    taller with a control in its body and wider with a wider one;
 *  - its frame (the bounds less PinOverhang each side) and its title drawn at
 *    z+1 over the frame's top, square and borderless at the bottom, in the
 *    theme's NodeTitle or the node's title colour;
 *  - pins: Input on the left and Output on the right, setSide() moving one
 *    across, each marker centred on the frame's edge and inside the pin and
 *    the node, getAnchor() the marker's centre in graph units, a hit-test at
 *    an anchor finding the pin at any zoom; the marker a rounded rect (a
 *    circle, a square) filled while connected and a ring otherwise, a
 *    triangle the icon kind 7 or 8 pointing right; a pin edit writing its one
 *    slot;
 *  - the grid: one drawGrid at z+1 over the backdrop, over the graph's bounds,
 *    from the graph's origin at pan(), gridStep()*zoom() units apart, every
 *    GridMajorEvery-th line in GraphGridMajor's colour and the rest in
 *    GraphGrid's border, the step growing by GridMajorEvery while its lines
 *    would be under MinGridGap apart; a pan or zoom rewriting the grid's slot
 *    (and the nodes') with no layout pass, a settled frame writing nothing;
 *  - the app's edits: a node moved writes only its own subtree's slots and
 *    no grid line's; a node or pin added and removed gives every id back.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/controls/nodeGraph.h>
#include <gweni/inputHandler.h>
#include <gweni/skins/textured/texturedControls.h>
#include <gweni/tools/toolSupport.h>

#include <doctest/doctest.h>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <vector>

using namespace gweni;
namespace tokyonight=skin::textured::tokyonight;

namespace
{

//! Records each rounded rect and icon drawn, by slot.
class DrawRecorder: public test::StubRenderer
{
public:
    struct RoundedDraw
    {
        Rect rect;
        float radius=0.0f;
        float border=0.0f;
        Color top, bottom, borderColor;
        int zIndex=0;
        uint8_t edges=0;
    };

    struct IconDraw
    {
        Rect rect;
        int kind=0;
        int direction=0;
        Color color;
        int zIndex=0;
    };

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex) override
    {
        rounded[primitiveId]={rect, radius, border, top, bottom, borderColor, zIndex, 0};
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor, zIndex);
    }

    void drawRoundedRect(size_t primitiveId, Rect rect, float radius, float border, Color top,
        Color bottom, Color borderColor, int zIndex, uint8_t edges, float reach) override
    {
        StubRenderer::drawRoundedRect(primitiveId, rect, radius, border, top, bottom, borderColor,
            zIndex, edges, reach);
        rounded[primitiveId].edges=edges;
    }

    void drawIcon(size_t primitiveId, Rect rect, int kind, int direction, Color color,
        int zIndex) override
    {
        icons[primitiveId]={rect, kind, direction, color, zIndex};
        StubRenderer::drawIcon(primitiveId, rect, kind, direction, color, zIndex);
    }

    struct GridDraw
    {
        Rect rect;
        PointF origin;
        float spacing=0.0f;
        int majorEvery=0;
        Color minor, major;
        int zIndex=0;
    };

    void drawGrid(size_t primitiveId, Rect rect, PointF origin, float spacing, int majorEvery,
        Color minor, Color major, int zIndex) override
    {
        grids[primitiveId]={rect, origin, spacing, majorEvery, minor, major, zIndex};
        StubRenderer::drawGrid(primitiveId, rect, origin, spacing, majorEvery, minor, major, zIndex);
    }

    std::map<size_t, RoundedDraw> rounded;
    std::map<size_t, IconDraw> icons;
    std::map<size_t, GridDraw> grids;
};

//! The textured skin's node-graph renderers, a StubControl for the rest.
class GraphSkin: public test::StubSkin
{
public:
    explicit GraphSkin(test::StubRenderer *renderer): StubSkin(renderer) {}

    std::unique_ptr<skin::Control> getControlRenderer(std::string typeName) override
    {
        if(typeName == "NodeGraph")
            return std::make_unique<skin::textured::NodeGraph>();
        if(typeName == "Node")
            return std::make_unique<skin::textured::Node>();
        if(typeName == "Pin")
            return std::make_unique<skin::textured::Pin>();
        return StubSkin::getControlRenderer(typeName);
    }
};

struct Scene
{
    Scene()
    {
        canvas=new controls::Canvas(&skin);
        canvas->setSize(800, 600);
        graph=canvas->newChild<controls::NodeGraph>();
        graph->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        graph->setBounds(20, 30, 600, 400);
    }

    Scene(const Scene &)=delete;
    Scene &operator=(const Scene &)=delete;

    ~Scene()
    {
        HoveredControl=nullptr;
        delete canvas;
    }

    void frame()
    {
        renderer.lines.clear();
        canvas->updateCanvas();
        canvas->renderCanvas();
    }

    std::set<size_t> written() const
    {
        const std::vector<size_t> &ids=renderer.writtenPrimitiveIds();
        return std::set<size_t>(ids.begin(), ids.end());
    }

    //! Every primitive id under \p control's subtree.
    static std::set<size_t> subtreeIds(const controls::Base *control)
    {
        std::vector<size_t> primitives, texts;

        tools::appendSubtreeIds(control, primitives, texts);
        return std::set<size_t>(primitives.begin(), primitives.end());
    }

    //! The canvas point a graph point is drawn at.
    PointF graphToCanvas(const PointF &point) const
    {
        const PointF local=graph->graphToLocal(point);
        const Point origin=graph->localPosToCanvas(Point(0, 0));

        return PointF(origin.x+local.x, origin.y+local.y);
    }

    DrawRecorder renderer;
    GraphSkin skin{&renderer};
    controls::Canvas *canvas=nullptr;
    controls::NodeGraph *graph=nullptr;
};

//! The control's bounds in the coordinates of \p ancestor.
Rect boundsIn(const controls::Base *control, const controls::Base *ancestor)
{
    Rect rect=control->getBounds();

    for(const controls::Base *parent=control->getParent(); parent && (parent != ancestor);
        parent=parent->getParent())
    {
        rect.x+=parent->getX();
        rect.y+=parent->getY();
    }
    return rect;
}

bool inside(const Rect &inner, const Rect &outer)
{
    return (inner.x >= outer.x) && (inner.y >= outer.y) && (inner.x+inner.w <= outer.x+outer.w)
        && (inner.y+inner.h <= outer.y+outer.h);
}

}//namespace

TEST_CASE("NodeGraph: a node sits at its graph position, its frame inset by the pin overhang")
{
    Scene scene;
    controls::Node *node=scene.graph->addNode("Noise", Point(40, -25));

    scene.frame();

    CHECK(node->getGraph() == scene.graph);
    CHECK(node->getGraphPosition().x == 40);
    CHECK(node->getGraphPosition().y == -25);
    CHECK(node->getTitle() == "Noise");
    CHECK(node->getWidth() == controls::Node::NodeWidth);
    // the node layer is the inner panel's size, GraphExtent up and left of the
    // content's origin
    CHECK(node->getX() == 40+controls::NodeGraph::GraphExtent);
    CHECK(node->getY() == -25+controls::NodeGraph::GraphExtent);

    const Rect frame=node->getFrameRect();

    CHECK((frame == Rect(controls::Node::PinOverhang, 0, node->getWidth()-2*controls::Node::PinOverhang,
        node->getHeight())));
    CHECK(node->getTitleBar()->getHeight() == controls::Node::TitleHeight);
    CHECK(node->getTitleBar()->getX() == frame.x);
    CHECK(node->getTitleBar()->getWidth() == frame.w);

    // the frame and the title, the title at z+1, joined to the frame's bottom edge
    const skin::Control *control=node->getSkinControl();
    REQUIRE(control);
    std::vector<size_t> ids, texts;
    control->appendIds(ids, texts);
    REQUIRE(ids.size() == 2);
    const auto &body=scene.renderer.rounded[ids[0]];
    const auto &title=scene.renderer.rounded[ids[1]];

    CHECK(body.rect.w == frame.w);
    CHECK(body.rect.h == frame.h);
    CHECK((body.top == tokyonight::NodeBody.top));
    CHECK(body.zIndex == node->getZIndex());
    CHECK(title.zIndex == node->getZIndex()+1);
    CHECK(title.rect.x == body.rect.x);
    CHECK(title.rect.y == body.rect.y);
    CHECK(title.rect.w == body.rect.w);
    CHECK(title.rect.h == controls::Node::TitleHeight);
    CHECK((title.top == tokyonight::NodeTitle.top));
    CHECK((title.borderColor == tokyonight::NodeBody.border));
    CHECK(title.edges == (renderer::SquareBottomLeft|renderer::SquareBottomRight|renderer::NoBorderBottom));

    // the app's title colour, the node's slots alone
    node->setTitleColor(Color(200, 100, 50));
    scene.frame();
    CHECK((scene.renderer.rounded[ids[1]].top == Color(200, 100, 50)));
    const std::set<size_t> written=scene.written();
    CHECK(std::includes(written.begin(), written.end(), ids.begin()+1, ids.end()));
    for(size_t id : written)
        CHECK(scene.subtreeIds(node).count(id) == 1);

    // a control in the body makes the node taller; a wide one, wider
    const int height=node->getHeight();
    controls::Base *content=node->newChild<controls::Base>();

    CHECK(content->getActualParent() == node->getBody());
    CHECK(content->getParent() == node);
    content->setDock(DockPosition::Top);
    content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    content->setSize(300, 40);
    scene.frame();
    CHECK(node->getHeight() == height+40);
    CHECK(node->getWidth() == 300+2*(controls::Node::PinOverhang+controls::Node::BodyPadding));
}

TEST_CASE("NodeGraph: pins sit on the frame's edges, their anchors in graph units")
{
    Scene scene;
    controls::Node *node=scene.graph->addNode("Mix", Point(-100, 60));
    controls::Pin *a=node->addPin(controls::PinDirection::Input, "A");
    controls::Pin *b=node->addPin(controls::PinDirection::Input, "B");
    controls::Pin *out=node->addPin(controls::PinDirection::Output, "Result");

    scene.frame();

    CHECK(a->getNode() == node);
    CHECK(a->getLabel() == "A");
    CHECK(a->getSide() == controls::PinSide::Left);
    CHECK(out->getSide() == controls::PinSide::Right);
    CHECK(node->getPins() == std::vector<controls::Pin *>{a, b, out});

    const Rect frame=node->getFrameRect();

    for(controls::Pin *pin : {a, b, out})
    {
        CAPTURE(pin->getLabel());
        CHECK(pin->getHeight() == controls::Pin::RowHeight);

        const Rect marker=pin->getMarkerRect();
        const Rect inNode=boundsIn(pin, node);
        Rect markerInNode=marker;

        markerInNode.x+=inNode.x;
        markerInNode.y+=inNode.y;
        CHECK(marker.w == controls::Pin::MarkerSize);
        CHECK(inside(marker, Rect(0, 0, pin->getWidth(), pin->getHeight())));
        CHECK(inside(markerInNode, Rect(0, 0, node->getWidth(), node->getHeight())));

        // centred on the frame's edge on its side
        const int edge=(pin->getSide() == controls::PinSide::Left)?frame.x:frame.x+frame.w;
        CHECK(markerInNode.x+markerInNode.w/2 == edge);

        // the anchor is the marker's centre at the node's graph position
        const PointF anchor=pin->getAnchor();
        CHECK(anchor.x == doctest::Approx(-100+markerInNode.x+markerInNode.w*0.5));
        CHECK(anchor.y == doctest::Approx(60+markerInNode.y+markerInNode.h*0.5));
    }
    // the inputs one under the other
    CHECK(b->getAnchor().y == doctest::Approx(a->getAnchor().y+controls::Pin::RowHeight));

    // a hit-test at an anchor finds its pin, at any zoom and pan
    for(float zoom : {1.0f, 0.5f, 2.5f})
    {
        scene.graph->setZoom(zoom);
        scene.graph->setPan(PointF(310.25f, -40.5f));
        scene.frame();
        for(controls::Pin *pin : {a, b, out})
        {
            const PointF at=scene.graphToCanvas(pin->getAnchor());
            CAPTURE(zoom);
            CAPTURE(pin->getLabel());
            CHECK(scene.canvas->getControlAt(int(std::floor(at.x)), int(std::floor(at.y))) == pin);
        }
    }

    // setSide moves a pin to the other column
    a->setSide(controls::PinSide::Right);
    scene.frame();
    CHECK(node->getPins() == std::vector<controls::Pin *>{b, out, a});
    const Rect moved=boundsIn(a, node);
    CHECK(moved.x+a->getMarkerRect().x+controls::Pin::MarkerSize/2 == frame.x+frame.w);
}

TEST_CASE("NodeGraph: a pin's marker is its shape and colour, a ring until connected")
{
    Scene scene;
    controls::Node *node=scene.graph->addNode("Node", Point(0, 0));
    controls::Pin *in=node->addPin(controls::PinDirection::Input, "In");
    controls::Pin *out=node->addPin(controls::PinDirection::Output, "Out");

    scene.frame();

    std::vector<size_t> inIds, outIds, texts;
    in->getSkinControl()->appendIds(inIds, texts);
    out->getSkinControl()->appendIds(outIds, texts);
    REQUIRE(inIds.size() == 1);
    REQUIRE(outIds.size() == 1);

    // the theme's colours, a ring around the bottom fill
    auto marker=scene.renderer.rounded[inIds[0]];
    CHECK((marker.borderColor == tokyonight::PinIn.top));
    CHECK((marker.top == tokyonight::PinIn.bottom));
    CHECK((marker.border == tokyonight::PinIn.borderWidth));
    CHECK(marker.radius == doctest::Approx(controls::Pin::MarkerSize*0.5));
    CHECK((scene.renderer.rounded[outIds[0]].borderColor == tokyonight::PinOut.top));

    auto onlyWrites=[&](size_t id)
    {
        scene.frame();
        CHECK(scene.written() == std::set<size_t>{id});
    };

    in->setConnected(true);
    onlyWrites(inIds[0]);
    marker=scene.renderer.rounded[inIds[0]];
    CHECK((marker.top == tokyonight::PinIn.top));
    CHECK(marker.border == 0.0f);

    in->setColor(Color(10, 20, 30));
    onlyWrites(inIds[0]);
    CHECK((scene.renderer.rounded[inIds[0]].top == Color(10, 20, 30)));

    in->setShape(controls::PinShape::Square);
    onlyWrites(inIds[0]);
    CHECK((scene.renderer.rounded[inIds[0]].radius == tokyonight::PinIn.radius));

    out->setShape(controls::PinShape::Triangle);
    onlyWrites(outIds[0]);
    auto icon=scene.renderer.icons[outIds[0]];
    CHECK(icon.kind == 8);
    CHECK(icon.direction == 3);
    CHECK((icon.color == tokyonight::PinOut.top));
    CHECK(icon.rect.w == controls::Pin::MarkerSize);

    out->setConnected(true);
    onlyWrites(outIds[0]);
    CHECK(scene.renderer.icons[outIds[0]].kind == 7);

    // nothing more to draw
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("NodeGraph: the grid follows the zoom and the pan, a step apart, in one slot")
{
    Scene scene;

    scene.frame();

    const skin::Control *control=scene.graph->getSkinControl();
    REQUIRE(control);

    std::vector<size_t> ids, texts;
    control->appendIds(ids, texts);
    REQUIRE(ids.size() == 2);

    auto check=[&](float zoom, const PointF &pan)
    {
        const bool moved=(zoom != scene.graph->zoom()) || !(pan == scene.graph->pan());

        scene.graph->setZoom(zoom);
        scene.graph->setPan(pan);
        scene.frame();
        CAPTURE(zoom);
        CAPTURE(pan.x);
        CAPTURE(pan.y);

        // no layout for a zoom or a pan, and the grid's slot alone (the backdrop
        // drawn the same again)
        CHECK(scene.canvas->getLayoutStats().passes == 0);
        if(moved)
            CHECK((scene.written() == std::set<size_t>(ids.begin(), ids.end())));

        const float step=scene.graph->gridStep();
        const float gap=step*zoom;

        CHECK(gap >= controls::NodeGraph::MinGridGap);
        CHECK(((step == controls::NodeGraph::GridSpacing)
            || (gap/controls::NodeGraph::GridMajorEvery < controls::NodeGraph::MinGridGap)));

        const Rect bounds=scene.graph->getRenderBounds();
        const int z=scene.graph->getZIndex();
        const auto &grid=scene.renderer.grids[ids[1]];

        CHECK((grid.rect == bounds));
        CHECK(grid.origin.x == doctest::Approx(bounds.x+pan.x));
        CHECK(grid.origin.y == doctest::Approx(bounds.y+pan.y));
        CHECK(grid.spacing == doctest::Approx(gap));
        CHECK(grid.majorEvery == controls::NodeGraph::GridMajorEvery);
        CHECK((grid.minor == tokyonight::GraphGrid.border));
        CHECK((grid.major == tokyonight::GraphGridMajor.top));
        CHECK(grid.zIndex == z+1);

        // the backdrop under it
        CHECK(scene.renderer.rounded[ids[0]].zIndex == z);
        CHECK((scene.renderer.rounded[ids[0]].rect == bounds));
        CHECK((scene.renderer.rounded[ids[0]].top == tokyonight::GraphGrid.top));

        // a settled frame writes nothing
        scene.frame();
        CHECK(scene.written().empty());
    };

    check(1.0f, PointF(0.0f, 0.0f));
    check(1.0f, PointF(13.5f, -7.25f));
    check(2.0f, PointF(-101.0f, 33.0f));
    check(0.5f, PointF(4.0f, 4.0f));
    check(0.3f, PointF(0.0f, 0.0f));
    CHECK(scene.graph->gridStep() == float(controls::NodeGraph::GridSpacing*controls::NodeGraph::GridMajorEvery));
    check(0.25f, PointF(-3.0f, 2.0f));

    // a node in the graph: a pan rewrites the grid and the node, and lays nothing out
    controls::Node *node=scene.graph->addNode("Node", Point(30, 30));
    scene.frame();
    scene.graph->setPan(PointF(50.0f, 60.0f));
    scene.frame();
    CHECK(scene.canvas->getLayoutStats().passes == 0);
    std::set<size_t> expected=scene.subtreeIds(node);
    expected.insert(ids.begin(), ids.end());
    for(size_t id : scene.written())
        CHECK(expected.count(id) == 1);
}

TEST_CASE("NodeGraph: a node moved from code writes its own slots alone")
{
    Scene scene;
    std::vector<controls::Node *> nodes;

    for(int i=0; i < 20; ++i)
    {
        controls::Node *node=scene.graph->addNode("Node "+std::to_string(i), Point(i*30, i*20));

        node->addPin(controls::PinDirection::Input, "In");
        node->addPin(controls::PinDirection::Output, "Out");
        nodes.push_back(node);
    }
    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());

    controls::Node *moved=nodes[7];
    const std::set<size_t> own=scene.subtreeIds(moved);
    const std::set<size_t> graphIds=[&]()
    {
        std::vector<size_t> ids, texts;
        scene.graph->getSkinControl()->appendIds(ids, texts);
        return std::set<size_t>(ids.begin(), ids.end());
    }();

    for(int step=1; step <= 3; ++step)
    {
        moved->setGraphPosition(Point(200+step*7, 150-step*3));
        controls::resetChangeStats();
        scene.frame();
        CAPTURE(step);
        CHECK(moved->getGraphPosition().x == 200+step*7);
        CHECK(moved->getGraphPosition().y == 150-step*3);

        const std::set<size_t> written=scene.written();
        CHECK(!written.empty());
        for(size_t id : written)
        {
            CHECK(own.count(id) == 1);
            CHECK(graphIds.count(id) == 0);
        }
        // the moved node and what is in it, nothing else
        std::vector<controls::Base *> subtree{moved};
        for(size_t i=0; i < subtree.size(); ++i)
            for(controls::Base *child : subtree[i]->getActualChildren())
                subtree.push_back(child);
        CHECK(controls::getChangeStats().controls <= subtree.size());
        CHECK(scene.canvas->getLayoutStats().passes == 1);
    }
}

TEST_CASE("NodeGraph: nodes and pins added and removed from code give their ids back")
{
    Scene scene;

    scene.frame();

    const size_t primitives=scene.renderer.livePrimitives.size();
    const size_t texts=scene.renderer.liveTexts.size();
    controls::Node *a=scene.graph->addNode("A", Point(10, 10));
    controls::Node *b=scene.graph->addNode("B", Point(300, 10));
    controls::Pin *pin=a->addPin(controls::PinDirection::Output, "Out");

    b->addPin(controls::PinDirection::Input, "In");
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() > primitives);
    CHECK(scene.graph->getNodes() == std::vector<controls::Node *>{a, b});

    // the pin row hides with its last pin
    const int height=a->getHeight();
    a->removePin(pin);
    scene.frame();
    CHECK(a->getPins().empty());
    CHECK(a->getHeight() < height);

    scene.graph->removeNode(a);
    // off the list at once, deleted before the next layout
    CHECK(scene.graph->getNodes() == std::vector<controls::Node *>{b});
    scene.frame();
    scene.graph->removeNode(b);
    scene.frame();
    CHECK(scene.graph->getNodes().empty());
    CHECK(scene.renderer.livePrimitives.size() == primitives);
    CHECK(scene.renderer.liveTexts.size() == texts);
    CHECK(scene.renderer.doubleAcquired == 0);
    CHECK(scene.renderer.doubleReleased == 0);
}
