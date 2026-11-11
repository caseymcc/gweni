/*
 *  Gweni
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 *
 *  NodeGraph, Node, Pin and Link: the node graph's view, on the textured skin's
 *  real NodeGraph, Node, Pin and Link renderers over a recording StubRenderer
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
 *    no grid line's; a node or pin added and removed gives every id back;
 *  - links: a Straight link one segment, an Orthogonal one 3 (pins facing with
 *    room) or 5 (not), all horizontal or vertical, a Bezier LinkCurveMin..Max
 *    by its length on screen (more zoomed in), each from the from pin's
 *    anchor to the to pin's at the link's z in its thickness and colour (the
 *    from pin's by default), consecutive segments sharing their points and
 *    drawn with each other as neighbours; the slots taken once, the style's
 *    most, the unused hidden, so a zoom takes none;
 *  - a node moved writes its own subtree's slots and its links' alone, no
 *    other link's, and lays out nothing but its place;
 *  - a link to a node far off screen (beyond what a vertex holds at the
 *    zoom) drawn cut to the graph's view: every segment drawn inside it,
 *    one reaching its edge, the ones wholly outside hidden, a cut joint
 *    drawn without that neighbour;
 *  - a pin's new colour (setColor(), resetColor()) redrawing the links drawn
 *    in it (those from it with no colour of their own) and no others;
 *  - pins connected while they have a link (a Link deleted directly too),
 *    removeLink(), removePin() and removeNode() taking the links with them
 *    and giving every id back;
 *  - a title-bar drag, through the canvas's mouse input: the press selects an
 *    unselected node alone (drawn NodeSelected), brings it to the front and
 *    takes the selection; every move frame moves each selected node by the
 *    mouse's motion in graph units (half the canvas motion at zoom 2), writes
 *    no slot outside the moved nodes' subtrees and their links', changes no
 *    more controls than they hold, runs one layout pass entering no more
 *    controls than they hold and their ancestors; onNodesMoved fires once, on
 *    the release, with getMovedNodes() the selection, and not at all for a
 *    press let go where it was; a title bar disabled mid-drag still ends the
 *    drag on the release, and takes no press while disabled;
 *  - a link dragged out of a pin: the preview, a Link in the overlay layer
 *    from the pin to the mouse in graph units, over every node; a drop on a
 *    pin it may end on (an Input and an Output of two nodes, or what
 *    setLinkFilter() says) firing onLinkRequested with the Output first, then
 *    the link made and onLinkCreated, unless a handler vetoed; a drop on an
 *    unfit pin or a node's body making nothing and firing nothing; a drop
 *    where no node is firing onLinkDroppedOnEmpty with the graph position;
 *    Escape ending it with nothing made; the preview's ids given back; a
 *    drag whose capture something else took (the depth view) leaving no
 *    preview once the next drag starts, none left reading a deleted pin;
 *  - hover: a link hit within Link::HoverDistance on screen of its curve and
 *    not beyond, measured from the exact point at every zoom (1, 2.5 and 4,
 *    where a graph unit is 4 on-screen units), drawn in LinkHover's colour; a pin's marker
 *    a unit larger each way; a hover frame writing only the hovered item's
 *    slots and the one it left;
 *  - selection by click: a title bar, a body or a link selects its item
 *    alone, Shift adds, Ctrl toggles (a node Ctrl takes off is not dragged),
 *    a plain click on a node of a selection makes it the selection, a click
 *    on empty space clears it (not under Ctrl), onSelectionChanged firing
 *    once per click that changed it and not otherwise; a selected link drawn
 *    in NodeSelected's border colour;
 *  - the marquee: nothing drawn until the mouse is PanThreshold from the
 *    press, then its outline over the nodes and its fill under them; each
 *    move frame selecting the nodes and links it touches (with the earlier
 *    selection under Shift) and writing no slot but the marquee's and those
 *    of the items whose selection changed; the release firing
 *    onSelectionChanged once; Escape giving the earlier selection back with
 *    no event; the marquee's ids given back;
 *  - Delete with the graph focused (a press on it focuses it): each selected
 *    node's onNodeRemoveRequested, then (unless vetoed) the node and its links
 *    gone with onLinkRemoved for each and onNodeRemoved, each selected link
 *    left its onLinkRemoveRequested and onLinkRemoved, onSelectionChanged
 *    once, every id given back; a body control holding the keys keeps
 *    Delete;
 *  - context menus: a right click on empty space opens the graph's menu,
 *    getContextMenuPosition() the click's graph point, on a node (its body
 *    included) the node's, on a link the link's; a right drag pans and opens
 *    none; openContextMenuAt() opens the graph's at a graph point.
 */
#include "headlessFixture.h"

#include <gweni/gweni.h>
#include <gweni/controls/label.h>
#include <gweni/controls/nodeGraph.h>
#include <gweni/controls/menu.h>
#include <gweni/controls/textBox.h>
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

    void drawLine(size_t primitiveId, PointF p0, PointF p1, float thickness, int zIndex,
        const PointF *prev=nullptr, const PointF *next=nullptr) override
    {
        StubRenderer::drawLine(primitiveId, p0, p1, thickness, zIndex, prev, next);
        lastLine[primitiveId]=lines.back();
        hiddenSlots.erase(primitiveId);
    }

    void hidePrimitive(size_t primitiveId) override
    {
        StubRenderer::hidePrimitive(primitiveId);
        hiddenSlots.insert(primitiveId);
    }

    //! What each slot's last drawLine drew
    std::map<size_t, LineDraw> lastLine;
    //! The slots hidden since they were last drawn
    std::set<size_t> hiddenSlots;
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
        if(typeName == "Link")
            return std::make_unique<skin::textured::Link>();
        if(typeName == "NodeGraphMarquee")
            return std::make_unique<skin::textured::NodeGraphMarquee>();
        if(typeName == "NodeGraphMarqueeFill")
            return std::make_unique<skin::textured::NodeGraphMarqueeFill>();
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

namespace
{

//! The primitive ids of \p control's own skin Control.
std::vector<size_t> ownIds(const controls::Base *control)
{
    std::vector<size_t> ids, texts;

    if(const skin::Control *skinControl=control->getSkinControl())
        skinControl->appendIds(ids, texts);
    return ids;
}

//! A link's segments as its slots hold them, in order: the shown ones.
std::vector<test::StubRenderer::LineDraw> shownSegments(const DrawRecorder &renderer,
    const controls::Link *link)
{
    std::vector<test::StubRenderer::LineDraw> segments;

    for(size_t id : ownIds(link))
    {
        if(renderer.hiddenSlots.count(id) || !renderer.lastLine.count(id))
            continue;
        segments.push_back(renderer.lastLine.at(id));
    }
    return segments;
}

//! Where a graph point is in the link's render units: the link covers the
//! link layer, GraphExtent up and left of the graph's origin.
PointF linkPoint(const PointF &graph)
{
    return PointF(graph.x+controls::NodeGraph::GraphExtent, graph.y+controls::NodeGraph::GraphExtent);
}

struct LinkScene: Scene
{
    LinkScene()
    {
        a=graph->addNode("A", Point(20, 40));
        b=graph->addNode("B", Point(320, 140));
        out=a->addPin(controls::PinDirection::Output, "Out");
        in=b->addPin(controls::PinDirection::Input, "In");
    }

    controls::Node *a=nullptr;
    controls::Node *b=nullptr;
    controls::Pin *out=nullptr;
    controls::Pin *in=nullptr;
};

bool near(const PointF &p, const PointF &q)
{
    return (std::fabs(p.x-q.x) < 1e-3f) && (std::fabs(p.y-q.y) < 1e-3f);
}

}//namespace

TEST_CASE("NodeGraph: a link's segments run from pin to pin in its style, neighbours joined")
{
    for(controls::LinkStyle style : {controls::LinkStyle::Straight, controls::LinkStyle::Orthogonal,
        controls::LinkStyle::Bezier})
    {
        LinkScene scene;
        controls::Link *link=scene.graph->addLink(scene.out, scene.in, style);

        REQUIRE(link);
        CAPTURE(int(style));
        scene.frame();

        CHECK(link->getFrom() == scene.out);
        CHECK(link->getTo() == scene.in);
        CHECK(link->getGraph() == scene.graph);
        CHECK(scene.out->isConnected());
        CHECK(scene.in->isConnected());
        CHECK(scene.graph->getLinks() == std::vector<controls::Link *>{link});
        CHECK(scene.a->getLinks() == std::vector<controls::Link *>{link});

        // the slots the style can draw, taken at once
        const std::vector<size_t> ids=ownIds(link);
        CHECK(ids.size() == skin::textured::Link::slotsFor(style));

        std::vector<PointF> points;
        link->getPoints(points);
        REQUIRE(points.size() >= 2);
        CHECK(near(points.front(), scene.out->getAnchor()));
        CHECK(near(points.back(), scene.in->getAnchor()));

        const size_t expected=(style == controls::LinkStyle::Straight)?1:
            (style == controls::LinkStyle::Orthogonal)?3:points.size()-1;

        CHECK(points.size()-1 == expected);
        if(style == controls::LinkStyle::Bezier)
        {
            CHECK(expected >= size_t(controls::Link::LinkCurveMin));
            CHECK(expected <= size_t(controls::Link::LinkCurveMax));
        }

        const auto segments=shownSegments(scene.renderer, link);

        REQUIRE(segments.size() == expected);
        for(size_t i=0; i < segments.size(); ++i)
        {
            const auto &segment=segments[i];

            CAPTURE(i);
            CHECK(near(segment.p0, linkPoint(points[i])));
            CHECK(near(segment.p1, linkPoint(points[i+1])));
            CHECK(segment.thickness == link->getThickness());
            CHECK(segment.zIndex == link->getZIndex());
            CHECK((segment.color == tokyonight::PinOut.top));
            CHECK(segment.prev.has_value() == (i > 0));
            CHECK(segment.next.has_value() == (i+1 < segments.size()));
            if(segment.prev)
                CHECK(near(*segment.prev, segments[i-1].p0));
            if(segment.next)
                CHECK(near(*segment.next, segments[i+1].p1));
            if(style == controls::LinkStyle::Orthogonal)
                CHECK(((segment.p0.x == segment.p1.x) || (segment.p0.y == segment.p1.y)));
        }

        // under every node
        CHECK(link->getZIndex() < scene.a->getZIndex());
        CHECK(link->getZIndex() < scene.b->getZIndex());

        // a colour of its own, its slots alone
        link->setColor(Color(1, 2, 3));
        scene.frame();
        for(size_t id : scene.written())
            CHECK(std::find(ids.begin(), ids.end(), id) != ids.end());
        CHECK((shownSegments(scene.renderer, link).front().color == Color(1, 2, 3)));
        scene.frame();
        CHECK(scene.written().empty());
    }
}

TEST_CASE("NodeGraph: an Orthogonal link between pins that do not face each other goes round in 5")
{
    LinkScene scene;
    // B to the left of A: A's output leaves rightward, B's input enters from the left
    scene.a->setGraphPosition(Point(300, 40));
    scene.b->setGraphPosition(Point(20, 200));
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Orthogonal);

    scene.frame();

    std::vector<PointF> points;
    link->getPoints(points);
    REQUIRE(points.size() == 6);
    CHECK(points[1].x == doctest::Approx(points[0].x+controls::Link::LinkStub));
    CHECK(points[4].x == doctest::Approx(points[5].x-controls::Link::LinkStub));
    for(size_t i=0; i+1 < points.size(); ++i)
        CHECK(((points[i].x == points[i+1].x) || (points[i].y == points[i+1].y)));
    CHECK(shownSegments(scene.renderer, link).size() == 5);
}

TEST_CASE("NodeGraph: a Bezier takes more segments zoomed in, in the slots it took at first")
{
    LinkScene scene;
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Bezier);

    std::vector<PointF> points;

    scene.graph->setZoom(0.25f);
    scene.frame();

    const std::vector<size_t> ids=ownIds(link);
    const size_t low=shownSegments(scene.renderer, link).size();

    link->getPoints(points);
    CHECK(points.size()-1 == low);

    // zoomed in about the from pin, so the link starts in view
    scene.graph->zoomAt(4.0f, scene.graph->graphToLocal(scene.out->getAnchor()));
    scene.frame();
    link->getPoints(points);

    const size_t high=points.size()-1;

    CHECK(low == size_t(controls::Link::LinkCurveMin));
    CHECK(high == size_t(controls::Link::LinkCurveMax));
    // the same slots: a zoom takes and gives back none
    CHECK(ownIds(link) == ids);
    CHECK(scene.canvas->getLayoutStats().passes == 0);
}

TEST_CASE("NodeGraph: a node moved writes its own slots and its links' alone")
{
    LinkScene scene;
    controls::Node *c=scene.graph->addNode("C", Point(20, 300));
    controls::Pin *other=c->addPin(controls::PinDirection::Output, "Other");
    controls::Pin *second=scene.b->addPin(controls::PinDirection::Input, "Second");
    controls::Link *moving=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Bezier);
    controls::Link *still=scene.graph->addLink(other, second, controls::LinkStyle::Straight);

    scene.frame();
    scene.frame();
    CHECK(scene.written().empty());

    std::set<size_t> allowed=scene.subtreeIds(scene.a);
    const std::vector<size_t> linkIds=ownIds(moving);
    allowed.insert(linkIds.begin(), linkIds.end());
    const std::vector<size_t> stillIds=ownIds(still);

    for(int step=1; step <= 3; ++step)
    {
        scene.a->setGraphPosition(Point(20+step*9, 40+step*5));
        controls::resetChangeStats();
        scene.frame();
        CAPTURE(step);

        const std::set<size_t> written=scene.written();
        bool wroteLink=false;

        for(size_t id : written)
        {
            CHECK(allowed.count(id) == 1);
            CHECK(std::find(stillIds.begin(), stillIds.end(), id) == stillIds.end());
            if(std::find(linkIds.begin(), linkIds.end(), id) != linkIds.end())
                wroteLink=true;
        }
        CHECK(wroteLink);
        CHECK(scene.canvas->getLayoutStats().passes == 1);

        // the moved link ends at the moved pin
        const auto segments=shownSegments(scene.renderer, moving);
        REQUIRE(!segments.empty());
        CHECK(near(segments.front().p0, linkPoint(scene.out->getAnchor())));
    }

    // moving the node with the still link's to pin moves both links, and
    // only them besides its own subtree
    scene.b->setGraphPosition(Point(330, 150));
    scene.frame();
    std::set<size_t> both=scene.subtreeIds(scene.b);
    both.insert(linkIds.begin(), linkIds.end());
    both.insert(stillIds.begin(), stillIds.end());
    for(size_t id : scene.written())
        CHECK(both.count(id) == 1);
}

TEST_CASE("NodeGraph: a pin's new colour reaches the links drawn in it, and no others")
{
    LinkScene scene;
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);

    scene.frame();
    scene.frame();
    REQUIRE(scene.written().empty());

    std::vector<size_t> outIds, inIds, linkIds, texts;
    scene.out->getSkinControl()->appendIds(outIds, texts);
    scene.in->getSkinControl()->appendIds(inIds, texts);
    link->getSkinControl()->appendIds(linkIds, texts);
    REQUIRE(outIds.size() == 1);
    REQUIRE(inIds.size() == 1);

    // the frame's lines, each in \p color, and the slots it wrote
    auto drawn=[&](const Color &color, std::set<size_t> expected)
    {
        scene.frame();
        CHECK(scene.written() == expected);
        for(const auto &line : scene.renderer.lines)
            CHECK((line.color == color));
    };
    std::set<size_t> outAndLink(linkIds.begin(), linkIds.end());
    outAndLink.insert(outIds[0]);

    // the from pin's colour is the link's
    scene.out->setColor(Color(200, 40, 40));
    drawn(Color(200, 40, 40), outAndLink);
    CHECK(!scene.renderer.lines.empty());
    // the to pin's is not
    scene.in->setColor(Color(40, 40, 200));
    drawn(Color(), {inIds[0]});
    CHECK(scene.renderer.lines.empty());
    // the theme's again
    scene.out->resetColor();
    drawn(tokyonight::PinOut.top, outAndLink);
    CHECK(!scene.renderer.lines.empty());
    // a link with a colour of its own keeps it
    link->setColor(Color(40, 200, 40));
    scene.frame();
    scene.out->setColor(Color(200, 40, 40));
    drawn(Color(), {outIds[0]});
    CHECK(scene.renderer.lines.empty());
    scene.frame();
    CHECK(scene.written().empty());
}

TEST_CASE("NodeGraph: a link to a node far off screen is drawn cut to the graph's view")
{
    LinkScene scene;

    // at zoom 4, 30 000 graph units are 120 000 device px: past what a vertex holds
    scene.b->setGraphPosition(Point(30000, 9000));
    controls::Link *straight=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);
    controls::Link *curve=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Bezier);

    scene.frame();
    // zoomed in about the from pin, which stays in view
    scene.graph->zoomAt(4.0f, scene.graph->graphToLocal(scene.out->getAnchor()));
    scene.frame();

    // the graph's view in the links' units
    const PointF viewMin=linkPoint(scene.graph->localToGraph(PointF(0.0f, 0.0f)));
    const PointF viewMax=linkPoint(scene.graph->localToGraph(
        PointF(float(scene.graph->getWidth()), float(scene.graph->getHeight()))));
    // the margin a cut end may be outside it: the thickness and 4 canvas units
    const float margin=straight->getThickness()+4.0f/scene.graph->zoom()+0.01f;
    auto inView=[&](const PointF &p)
    {
        return (p.x >= viewMin.x-margin) && (p.x <= viewMax.x+margin) && (p.y >= viewMin.y-margin)
            && (p.y <= viewMax.y+margin);
    };

    for(controls::Link *link : {straight, curve})
    {
        const auto segments=shownSegments(scene.renderer, link);

        REQUIRE(!segments.empty());
        bool reachesEdge=false;

        for(const auto &segment : segments)
        {
            CHECK(inView(segment.p0));
            CHECK(inView(segment.p1));
            // a cut end is past the view's edge, within the margin
            if((segment.p1.x > viewMax.x) || (segment.p1.y > viewMax.y))
                reachesEdge=true;
        }
        CHECK(reachesEdge);
        // it starts at its from pin, which is in view
        CHECK(near(segments.front().p0, linkPoint(scene.out->getAnchor())));
        // the last shown segment is cut: it has no next neighbour
        CHECK(!segments.back().next.has_value());
    }

    // the curve's segments wholly outside the view are hidden
    CHECK(shownSegments(scene.renderer, curve).size() < ownIds(curve).size());
}

TEST_CASE("NodeGraph: removing a link, a pin or a node takes its links and gives every id back")
{
    LinkScene scene;

    scene.frame();

    const size_t primitives=scene.renderer.livePrimitives.size();
    const size_t texts=scene.renderer.liveTexts.size();
    controls::Pin *second=scene.a->addPin(controls::PinDirection::Output, "Second");
    controls::Link *one=scene.graph->addLink(scene.out, scene.in);
    controls::Link *two=scene.graph->addLink(second, scene.in, controls::LinkStyle::Orthogonal);

    CHECK(scene.graph->addLink(scene.out, scene.out) == nullptr);
    CHECK(scene.graph->addLink(scene.out, nullptr) == nullptr);
    scene.frame();
    CHECK(scene.in->getLinks() == std::vector<controls::Link *>{one, two});

    // the to pin keeps its other link
    scene.graph->removeLink(one);
    CHECK(!scene.out->isConnected());
    CHECK(scene.in->isConnected());
    CHECK(scene.graph->getLinks() == std::vector<controls::Link *>{two});
    scene.frame();

    // a link deleted directly, not through removeLink(), leaves its pins
    // as removeLink() would
    controls::Link *direct=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);
    CHECK(scene.out->isConnected());
    delete direct;
    CHECK(!scene.out->isConnected());
    CHECK(scene.in->isConnected());
    CHECK(scene.in->getLinks() == std::vector<controls::Link *>{two});
    CHECK(scene.graph->getLinks() == std::vector<controls::Link *>{two});
    scene.frame();

    // a pin removed takes its link
    scene.a->removePin(second);
    CHECK(!scene.in->isConnected());
    CHECK(scene.in->getLinks().empty());
    scene.frame();
    CHECK(scene.graph->getLinks().empty());

    // a node removed takes its links at once
    scene.a->removePin(scene.out);
    controls::Pin *fresh=scene.a->addPin(controls::PinDirection::Output, "Fresh");
    scene.graph->addLink(fresh, scene.in, controls::LinkStyle::Straight);
    scene.frame();
    scene.graph->removeNode(scene.a);
    CHECK(scene.graph->getLinks().empty());
    CHECK(!scene.in->isConnected());
    scene.frame();

    // back to the graph with node B alone: A's ids and its pins' and links'
    // gone, B's pin given back what it had
    controls::Node *a=scene.graph->addNode("A", Point(20, 40));
    controls::Pin *out=a->addPin(controls::PinDirection::Output, "Out");
    (void)out;
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == primitives);
    CHECK(scene.renderer.liveTexts.size() == texts);
    CHECK(scene.renderer.doubleAcquired == 0);
    CHECK(scene.renderer.doubleReleased == 0);
}

TEST_CASE("NodeGraph: a graph deleted with links on it, its nodes going first or last")
{
    for(bool nodesFirst : {false, true})
    {
        LinkScene scene;
        scene.graph->addLink(scene.out, scene.in);
        scene.frame();
        if(nodesFirst)
        {
            delete scene.a;
            CHECK(!scene.in->isConnected());
            scene.frame();
        }
        // the Scene deletes the canvas, the links and the nodes with it
    }
}

namespace
{

struct DragScene: Scene, event::Handler
{
    DragScene()
    {
        for(int i=0; i < 6; ++i)
        {
            controls::Node *node=graph->addNode("Node "+std::to_string(i), Point(20+(i%3)*190, 20+(i/3)*160));

            in.push_back(node->addPin(controls::PinDirection::Input, "In"));
            out.push_back(node->addPin(controls::PinDirection::Output, "Out"));
            nodes.push_back(node);
        }
        // 0 -> 1 -> 2, and 3 -> 4 on their own
        links.push_back(graph->addLink(out[0], in[1]));
        links.push_back(graph->addLink(out[1], in[2], controls::LinkStyle::Orthogonal));
        links.push_back(graph->addLink(out[3], in[4], controls::LinkStyle::Straight));
        graph->onNodesMoved.add(this, &DragScene::moved);
        frame();
        frame();
    }

    void moved(event::Info)
    {
        ++movedCalls;
    }

    //! The canvas point at the middle of \p node's title bar.
    Point titleOf(controls::Node *node)
    {
        controls::Base *bar=node->getTitleBar();

        return bar->localPosToCanvas(Point(bar->getWidth()/2, bar->getHeight()/2));
    }

    void move(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    void press(const Point &at)
    {
        move(at);
        canvas->inputMouseButton(0, true);
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
    }

    //! Every control in \p node's subtree, the node included.
    static size_t subtreeSize(controls::Base *node)
    {
        std::vector<controls::Base *> subtree{node};

        for(size_t i=0; i < subtree.size(); ++i)
            for(controls::Base *child : subtree[i]->getActualChildren())
                subtree.push_back(child);
        return subtree.size();
    }

    //! How many controls are above \p control, up to the canvas.
    static size_t depthOf(controls::Base *control)
    {
        size_t depth=0;

        for(controls::Base *parent=control->getActualParent(); parent; parent=parent->getActualParent())
            ++depth;
        return depth;
    }

    std::vector<controls::Node *> nodes;
    std::vector<controls::Pin *> in;
    std::vector<controls::Pin *> out;
    std::vector<controls::Link *> links;
    int movedCalls=0;
};

}//namespace

TEST_CASE("NodeGraph: a title-bar drag moves the node, writing its slots and its links' alone")
{
    DragScene scene;
    controls::Node *node=scene.nodes[1];
    const Point start=node->getGraphPosition();

    // the press selects it alone and brings it to the front
    scene.graph->selectNode(scene.nodes[4]);
    scene.press(scene.titleOf(node));
    scene.frame();
    CHECK(node->isSelected());
    CHECK(!scene.nodes[4]->isSelected());
    CHECK(scene.graph->getSelectedNodes() == std::vector<controls::Node *>{node});
    CHECK(scene.graph->getNodeLayer()->getChildren().back() == node);
    CHECK(scene.graph->draggingNodes());
    CHECK((scene.renderer.rounded[ownIds(node)[0]].borderColor == tokyonight::NodeSelected.border));
    scene.frame();

    // what a move may write: the node's subtree and its two links
    std::set<size_t> allowed=scene.subtreeIds(node);
    for(controls::Link *link : {scene.links[0], scene.links[1]})
    {
        const std::vector<size_t> ids=ownIds(link);
        allowed.insert(ids.begin(), ids.end());
    }
    const size_t subtree=DragScene::subtreeSize(node);
    const Point from=scene.titleOf(node);

    for(int step=1; step <= 6; ++step)
    {
        controls::resetChangeStats();
        scene.move(Point(from.x+step*7, from.y+step*3));
        scene.frame();
        CAPTURE(step);
        CHECK(node->getGraphPosition().x == start.x+step*7);
        CHECK(node->getGraphPosition().y == start.y+step*3);

        const std::set<size_t> written=scene.written();
        CHECK(!written.empty());
        for(size_t id : written)
            CHECK(allowed.count(id) == 1);
        CHECK(controls::getChangeStats().controls <= subtree+2);
        CHECK(scene.canvas->getLayoutStats().passes == 1);
        CHECK(scene.canvas->getLayoutStats().visited <= 2*(subtree+DragScene::depthOf(node))+8);
        CHECK(scene.movedCalls == 0);
    }

    scene.release();
    scene.frame();
    CHECK(scene.movedCalls == 1);
    CHECK(scene.graph->getMovedNodes() == std::vector<controls::Node *>{node});
    CHECK(!scene.graph->draggingNodes());

    // moves after the release move nothing
    scene.move(Point(from.x+100, from.y+100));
    scene.frame();
    CHECK(node->getGraphPosition().x == start.x+42);
    CHECK(scene.movedCalls == 1);
}

TEST_CASE("NodeGraph: a drag on a selected node moves the whole selection")
{
    DragScene scene;

    scene.graph->selectNode(scene.nodes[0]);
    scene.graph->selectNode(scene.nodes[2]);
    scene.frame();

    const Point start0=scene.nodes[0]->getGraphPosition();
    const Point start2=scene.nodes[2]->getGraphPosition();
    const Point still=scene.nodes[1]->getGraphPosition();
    // the moved nodes, their links (0->1, 1->2) and nothing of 3, 4 and 5
    std::set<size_t> allowed=scene.subtreeIds(scene.nodes[0]);
    const std::set<size_t> other=scene.subtreeIds(scene.nodes[2]);
    allowed.insert(other.begin(), other.end());
    for(controls::Link *link : {scene.links[0], scene.links[1]})
    {
        const std::vector<size_t> ids=ownIds(link);
        allowed.insert(ids.begin(), ids.end());
    }

    const Point from=scene.titleOf(scene.nodes[2]);

    scene.press(from);
    scene.frame();
    for(int step=1; step <= 3; ++step)
    {
        scene.move(Point(from.x-step*11, from.y+step*5));
        scene.frame();
        CAPTURE(step);
        CHECK(scene.nodes[0]->getGraphPosition().x == start0.x-step*11);
        CHECK(scene.nodes[2]->getGraphPosition().x == start2.x-step*11);
        CHECK(scene.nodes[0]->getGraphPosition().y == start0.y+step*5);
        CHECK(scene.nodes[1]->getGraphPosition().x == still.x);
        for(size_t id : scene.written())
            CHECK(allowed.count(id) == 1);
    }
    scene.release();
    scene.frame();
    CHECK(scene.movedCalls == 1);
    CHECK(scene.graph->getMovedNodes() == std::vector<controls::Node *>{scene.nodes[0], scene.nodes[2]});
    // the selection stays
    CHECK(scene.nodes[0]->isSelected());
    CHECK(scene.nodes[2]->isSelected());
}

TEST_CASE("NodeGraph: a drag at zoom 2 moves by the mouse's motion in graph units, a still press fires nothing")
{
    DragScene scene;
    controls::Node *node=scene.nodes[3];

    scene.graph->setZoom(2.0f);
    scene.frame();

    const Point start=node->getGraphPosition();
    const Point from=scene.titleOf(node);

    scene.press(from);
    scene.frame();
    scene.move(Point(from.x+20, from.y-10));
    scene.frame();
    CHECK(node->getGraphPosition().x == start.x+10);
    CHECK(node->getGraphPosition().y == start.y-5);
    // back where it started: the drag has still moved
    scene.move(from);
    scene.frame();
    scene.release();
    scene.frame();
    CHECK(scene.movedCalls == 1);

    // a press let go where it was moves nothing and fires nothing
    scene.press(scene.titleOf(node));
    scene.frame();
    scene.release();
    scene.frame();
    CHECK(scene.movedCalls == 1);
    CHECK(node->getGraphPosition().x == start.x);
}

TEST_CASE("NodeGraph: a title bar disabled mid-drag ends the drag on the release, and takes no press")
{
    DragScene scene;
    controls::Node *node=scene.nodes[2];
    const Point start=node->getGraphPosition();
    const Point from=scene.titleOf(node);

    scene.press(from);
    scene.frame();
    scene.move(Point(from.x+9, from.y+4));
    scene.frame();
    REQUIRE(scene.graph->draggingNodes());

    node->getTitleBar()->setDisabled(true);
    scene.frame();
    scene.release();
    scene.frame();
    CHECK(!scene.graph->draggingNodes());
    CHECK(scene.movedCalls == 1);
    CHECK(scene.graph->getMovedNodes() == std::vector<controls::Node *>{node});
    CHECK(node->getGraphPosition().x == start.x+9);

    // disabled, its press starts nothing
    scene.press(scene.titleOf(node));
    scene.frame();
    CHECK(!scene.graph->draggingNodes());
    scene.move(Point(from.x+30, from.y+30));
    scene.frame();
    scene.release();
    scene.frame();
    CHECK(node->getGraphPosition().x == start.x+9);
    CHECK(scene.movedCalls == 1);
}

TEST_CASE("NodeGraph: a node deleted while it is dragged leaves the drag")
{
    DragScene scene;
    controls::Node *node=scene.nodes[5];

    scene.graph->selectNode(scene.nodes[0]);
    scene.graph->selectNode(node);
    scene.press(scene.titleOf(scene.nodes[0]));
    scene.frame();
    scene.graph->removeNode(node);
    scene.frame();
    scene.move(Point(scene.titleOf(scene.nodes[0]).x+5, scene.titleOf(scene.nodes[0]).y));
    scene.frame();
    scene.release();
    scene.frame();
    CHECK(scene.movedCalls == 1);
    CHECK(scene.graph->getMovedNodes() == std::vector<controls::Node *>{scene.nodes[0]});
}

namespace
{

struct LinkDragScene: Scene, event::Handler
{
    LinkDragScene()
    {
        a=graph->addNode("A", Point(20, 40));
        b=graph->addNode("B", Point(320, 60));
        out=a->addPin(controls::PinDirection::Output, "Out");
        otherOut=a->addPin(controls::PinDirection::Output, "Other");
        in=b->addPin(controls::PinDirection::Input, "In");
        graph->onLinkRequested.add(this, &LinkDragScene::requested);
        graph->onLinkCreated.add(this, &LinkDragScene::created);
        graph->onLinkDroppedOnEmpty.add(this, &LinkDragScene::droppedOnEmpty);
        frame();
        frame();
    }

    void requested(event::Info)
    {
        ++requests;
        requestedFrom=graph->getRequestedFrom();
        requestedTo=graph->getRequestedTo();
        if(veto)
            graph->vetoLink();
    }

    void created(event::Info info)
    {
        ++creations;
        createdLink=gweni_cast<controls::Link>(info.control);
    }

    void droppedOnEmpty(event::Info info)
    {
        ++emptyDrops;
        emptyPoint=info.point;
        emptyPosition=graph->getDropPosition();
        emptyPin=graph->getDropPin();
    }

    Point canvasOf(const PointF &graphPoint) const
    {
        const PointF at=graphToCanvas(graphPoint);

        return Point(int(std::floor(at.x)), int(std::floor(at.y)));
    }

    Point anchorOf(controls::Pin *pin) const
    {
        return canvasOf(pin->getAnchor());
    }

    void move(const Point &at)
    {
        canvas->inputMouseMoved(at.x, at.y, 0, 0);
    }

    //! A drag out of \p pin to \p to, in a few steps, a frame each, held.
    void dragFrom(controls::Pin *pin, const Point &to)
    {
        const Point from=anchorOf(pin);

        move(from);
        frame();
        canvas->inputMouseButton(0, true);
        frame();
        for(int step=1; step <= 4; ++step)
        {
            move(Point(from.x+(to.x-from.x)*step/4, from.y+(to.y-from.y)*step/4));
            frame();
        }
    }

    void release()
    {
        canvas->inputMouseButton(0, false);
        frame();
    }

    controls::Node *a=nullptr;
    controls::Node *b=nullptr;
    controls::Pin *out=nullptr;
    controls::Pin *otherOut=nullptr;
    controls::Pin *in=nullptr;
    bool veto=false;
    int requests=0;
    int creations=0;
    int emptyDrops=0;
    controls::Pin *requestedFrom=nullptr;
    controls::Pin *requestedTo=nullptr;
    controls::Link *createdLink=nullptr;
    Point emptyPoint;
    PointF emptyPosition;
    controls::Pin *emptyPin=nullptr;
};

}//namespace

TEST_CASE("NodeGraph: a link dragged out of a pin and dropped on another is requested and made")
{
    LinkDragScene scene;
    const size_t primitives=scene.renderer.livePrimitives.size();
    const Point target=scene.anchorOf(scene.in);

    scene.dragFrom(scene.out, target);

    // the preview, over every node, ending at the mouse
    controls::Link *preview=scene.graph->getPreviewLink();
    REQUIRE(preview);
    CHECK(scene.graph->draggingLink());
    CHECK(preview->isPreview());
    CHECK(preview->getParent() == scene.graph->getOverlayLayer());
    CHECK(preview->getFrom() == scene.out);
    CHECK(preview->getZIndex() > scene.b->getZIndex());
    CHECK(scene.out->getLinks().empty());
    CHECK(scene.graph->getLinks().empty());
    const PointF end=scene.graph->canvasToGraph(PointF(target));
    CHECK(preview->getFreeEnd().x == doctest::Approx(end.x));
    CHECK(preview->getFreeEnd().y == doctest::Approx(end.y));
    const auto segments=shownSegments(scene.renderer, preview);
    REQUIRE(!segments.empty());
    CHECK(near(segments.front().p0, linkPoint(scene.out->getAnchor())));
    CHECK(near(segments.back().p1, linkPoint(end)));

    scene.release();
    CHECK(scene.requests == 1);
    CHECK(scene.requestedFrom == scene.out);
    CHECK(scene.requestedTo == scene.in);
    CHECK(scene.creations == 1);
    REQUIRE(scene.createdLink);
    CHECK(scene.createdLink->getFrom() == scene.out);
    CHECK(scene.createdLink->getTo() == scene.in);
    CHECK(scene.createdLink->getStyle() == scene.graph->getNewLinkStyle());
    CHECK(scene.graph->getLinks() == std::vector<controls::Link *>{scene.createdLink});
    CHECK(scene.out->isConnected());
    CHECK(scene.in->isConnected());
    CHECK(!scene.graph->draggingLink());
    CHECK(scene.graph->getPreviewLink() == nullptr);

    // the preview's ids back: what is live now is the new link's
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == primitives+ownIds(scene.createdLink).size());

    // dragged from the input, the link still runs from the output
    scene.graph->removeLink(scene.createdLink);
    scene.frame();
    scene.dragFrom(scene.in, scene.anchorOf(scene.otherOut));
    scene.release();
    CHECK(scene.requests == 2);
    CHECK(scene.requestedFrom == scene.otherOut);
    CHECK(scene.requestedTo == scene.in);
    CHECK(scene.creations == 2);
}

TEST_CASE("NodeGraph: a vetoed or unfit link is not made")
{
    LinkDragScene scene;

    scene.veto=true;
    scene.dragFrom(scene.out, scene.anchorOf(scene.in));
    scene.release();
    CHECK(scene.requests == 1);
    CHECK(scene.creations == 0);
    CHECK(scene.graph->getLinks().empty());
    CHECK(!scene.out->isConnected());

    // two outputs may not be linked without a filter that says so
    scene.veto=false;
    scene.dragFrom(scene.out, scene.anchorOf(scene.otherOut));
    scene.release();
    CHECK(scene.requests == 1);
    CHECK(scene.graph->getLinks().empty());

    // a filter that refuses every pair
    scene.graph->setLinkFilter([](controls::Pin *, controls::Pin *) { return false; });
    scene.dragFrom(scene.out, scene.anchorOf(scene.in));
    scene.release();
    CHECK(scene.requests == 1);
    CHECK(scene.graph->getLinks().empty());

    // one that takes the pins in the order dragged when they are alike
    scene.graph->setLinkFilter([](controls::Pin *from, controls::Pin *to) { return from != to; });
    scene.dragFrom(scene.otherOut, scene.anchorOf(scene.out));
    scene.release();
    CHECK(scene.requests == 2);
    CHECK(scene.requestedFrom == scene.otherOut);
    CHECK(scene.requestedTo == scene.out);
    CHECK(scene.creations == 1);

    // a drop on a node's body, off its pins, makes and fires nothing
    const int emptyDrops=scene.emptyDrops;
    controls::Base *body=scene.b->getBody();
    scene.dragFrom(scene.out, body->localPosToCanvas(Point(body->getWidth()/2, body->getHeight()/2)));
    scene.release();
    CHECK(scene.requests == 2);
    CHECK(scene.emptyDrops == emptyDrops);
}

TEST_CASE("NodeGraph: a link dropped where no node is fires onLinkDroppedOnEmpty, Escape ends a drag")
{
    LinkDragScene scene;
    const size_t primitives=scene.renderer.livePrimitives.size();

    scene.graph->setZoom(1.5f);
    scene.frame();

    const PointF spot(200.0f, 150.0f);

    scene.dragFrom(scene.out, scene.canvasOf(spot));
    scene.release();
    CHECK(scene.emptyDrops == 1);
    CHECK(scene.emptyPin == scene.out);
    CHECK(scene.emptyPosition.x == doctest::Approx(spot.x).epsilon(0.01));
    CHECK(scene.emptyPosition.y == doctest::Approx(spot.y).epsilon(0.01));
    CHECK(scene.emptyPoint.x == int(std::lround(scene.emptyPosition.x)));
    CHECK(scene.emptyPoint.y == int(std::lround(scene.emptyPosition.y)));
    CHECK(scene.graph->getLinks().empty());
    CHECK(scene.requests == 0);

    // Escape: no link, no event, the canvas's input back
    scene.dragFrom(scene.out, scene.anchorOf(scene.in));
    CHECK(scene.graph->draggingLink());
    scene.canvas->inputModifierKey(keys::Escape, true);
    scene.canvas->inputModifierKey(keys::Escape, false);
    scene.frame();
    CHECK(!scene.graph->draggingLink());
    CHECK(scene.canvas->inputCapture() == nullptr);
    scene.release();
    CHECK(scene.requests == 0);
    CHECK(scene.emptyDrops == 1);
    CHECK(scene.graph->getLinks().empty());
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == primitives);
}

TEST_CASE("NodeGraph: a link is hovered within HoverDistance of its curve, a hover frame writing it alone")
{
    LinkDragScene scene;
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);

    for(float zoom : {1.0f, 2.0f})
    {
        CAPTURE(zoom);
        scene.graph->setZoom(zoom);
        scene.graph->setPan(PointF(0.0f, 0.0f));
        scene.move(Point(0, 0));
        scene.frame();
        scene.frame();

        // the middle of the link, and the way across it, on screen
        const PointF p0=scene.graphToCanvas(scene.out->getAnchor());
        const PointF p1=scene.graphToCanvas(scene.in->getAnchor());
        const PointF middle((p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f);
        const float length=std::hypot(p1.x-p0.x, p1.y-p0.y);
        const PointF across(-(p1.y-p0.y)/length, (p1.x-p0.x)/length);
        auto at=[&](float off)
        {
            return Point(int(std::lround(middle.x+across.x*off-0.5f)), int(std::lround(middle.y+across.y*off-0.5f)));
        };

        scene.move(at(0.0f));
        scene.frame();
        CHECK(HoveredControl == link);
        // its slots alone, in LinkHover's colour
        const std::vector<size_t> ids=ownIds(link);
        for(size_t id : scene.written())
            CHECK(std::find(ids.begin(), ids.end(), id) != ids.end());
        CHECK((shownSegments(scene.renderer, link).front().color == tokyonight::LinkHover.top));

        scene.move(at(2.5f));
        scene.frame();
        CHECK(HoveredControl == link);
        CHECK(scene.written().empty());

        // past the distance it is the graph's
        scene.move(at(6.0f));
        scene.frame();
        CHECK(HoveredControl != link);
        for(size_t id : scene.written())
            CHECK(std::find(ids.begin(), ids.end(), id) != ids.end());
        CHECK((shownSegments(scene.renderer, link).front().color == tokyonight::PinOut.top));
    }
}

TEST_CASE("NodeGraph: a link's hover band is HoverDistance on screen at every zoom, measured from the exact point")
{
    LinkDragScene scene;
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);

    // at zoom 4 a graph unit is 4 on-screen units, so a hit-test from whole
    // graph units would be up to 2 off on each axis
    for(float zoom : {1.0f, 2.5f, 4.0f})
    {
        CAPTURE(zoom);
        // the link's middle at the graph's centre
        const PointF a=scene.out->getAnchor();
        const PointF b=scene.in->getAnchor();
        scene.graph->setZoom(zoom);
        scene.graph->setPan(PointF(300.0f-(a.x+b.x)*0.5f*zoom, 200.0f-(a.y+b.y)*0.5f*zoom));
        scene.move(Point(0, 0));
        scene.frame();
        scene.frame();

        const PointF p0=scene.graphToCanvas(a);
        const PointF p1=scene.graphToCanvas(b);
        const PointF middle((p0.x+p1.x)*0.5f, (p0.y+p1.y)*0.5f);
        const float length=std::hypot(p1.x-p0.x, p1.y-p0.y);
        const PointF across(-(p1.y-p0.y)/length, (p1.x-p0.x)/length);
        int inside=0;
        int outside=0;

        // every canvas unit across the link near the band's edges, its centre's
        // on-screen distance from the curve deciding
        for(int step=-28; step <= 28; ++step)
        {
            const float off=float(step)*0.25f;
            const Point at(int(std::floor(middle.x+across.x*off)), int(std::floor(middle.y+across.y*off)));
            const PointF centre(float(at.x)+0.5f, float(at.y)+0.5f);
            const float distance=link->distanceTo(scene.graph->canvasToGraph(centre))*zoom;

            if(std::abs(distance-controls::Link::HoverDistance) < 0.05f)
                continue;
            CAPTURE(at.x);
            CAPTURE(at.y);
            CAPTURE(distance);
            scene.move(at);
            scene.frame();
            CHECK((HoveredControl == link) == (distance <= controls::Link::HoverDistance));
            ++((distance <= controls::Link::HoverDistance)?inside:outside);
        }
        CHECK(inside > 0);
        CHECK(outside > 0);
    }
}

TEST_CASE("NodeGraph: a hovered pin's marker grows, writing its slot and the left item's")
{
    LinkDragScene scene;
    controls::Link *link=scene.graph->addLink(scene.out, scene.in, controls::LinkStyle::Straight);

    scene.frame();

    const std::vector<size_t> pinIds=ownIds(scene.out);
    const Rect rest=scene.renderer.rounded[pinIds[0]].rect;

    scene.move(scene.anchorOf(scene.out));
    scene.frame();
    CHECK(HoveredControl == scene.out);
    CHECK(scene.written() == std::set<size_t>(pinIds.begin(), pinIds.end()));
    const Rect hovered=scene.renderer.rounded[pinIds[0]].rect;
    CHECK((hovered == Rect(rest.x-1, rest.y-1, rest.w+2, rest.h+2)));

    // from the pin onto its link: the pin's slot and the link's
    const PointF p0=scene.graphToCanvas(scene.out->getAnchor());
    const PointF p1=scene.graphToCanvas(scene.in->getAnchor());
    scene.move(Point(int((p0.x+p1.x)*0.5f), int((p0.y+p1.y)*0.5f)));
    scene.frame();
    CHECK(HoveredControl == link);
    std::set<size_t> allowed(pinIds.begin(), pinIds.end());
    const std::vector<size_t> linkIds=ownIds(link);
    allowed.insert(linkIds.begin(), linkIds.end());
    for(size_t id : scene.written())
        CHECK(allowed.count(id) == 1);
    CHECK((scene.renderer.rounded[pinIds[0]].rect == rest));
}

TEST_CASE("NodeGraph: a link drag ends when its pin is deleted, and with its graph")
{
    {
        LinkDragScene scene;

        scene.dragFrom(scene.out, scene.anchorOf(scene.in));
        REQUIRE(scene.graph->draggingLink());
        scene.a->removePin(scene.out);
        CHECK(!scene.graph->draggingLink());
        CHECK(scene.canvas->inputCapture() == nullptr);
        scene.release();
        CHECK(scene.requests == 0);
    }
    {
        LinkDragScene scene;

        scene.dragFrom(scene.out, scene.anchorOf(scene.in));
        REQUIRE(scene.graph->draggingLink());
        delete scene.graph;
        scene.graph=nullptr;
        CHECK(scene.canvas->inputCapture() == nullptr);
        scene.frame();
    }
}

TEST_CASE("NodeGraph: a link drag whose capture was taken leaves no preview behind")
{
    LinkDragScene scene;

    // something else (the depth view) takes the canvas's input mid-drag, so
    // the drag never gets its release
    controls::InputCapture other;
    auto previews=[&]()
    {
        size_t count=0;

        for(controls::Base *child : scene.graph->getOverlayLayer()->getChildren())
        {
            if(gweni_cast<controls::Link>(child))
                ++count;
        }
        return count;
    };

    scene.dragFrom(scene.out, scene.anchorOf(scene.in));
    REQUIRE(previews() == 1);
    scene.canvas->setInputCapture(&other);
    scene.release();
    scene.canvas->setInputCapture(nullptr);
    CHECK(!scene.graph->draggingLink());

    // the next drag takes the earlier preview away with it
    scene.dragFrom(scene.otherOut, scene.anchorOf(scene.in));
    REQUIRE(scene.graph->draggingLink());
    CHECK(previews() == 1);
    scene.release();
    CHECK(previews() == 0);
    CHECK(scene.creations == 1);

    // the first pin gone and the graph panned: no preview reads it
    scene.a->removePin(scene.out);
    scene.graph->setPan(PointF(scene.graph->pan().x+10.0f, scene.graph->pan().y));
    scene.frame();
}

namespace
{

struct SelectScene: DragScene
{
    SelectScene()
    {
        graph->onSelectionChanged.add(this, &SelectScene::selectionChanged);
    }

    void selectionChanged(event::Info)
    {
        ++selectionEvents;
    }

    void click(const Point &at)
    {
        press(at);
        frame();
        release();
        frame();
    }

    void modifier(int key, bool down)
    {
        canvas->inputModifierKey(key, down);
    }

    //! A canvas point on the graph where no node or link is.
    Point empty() const
    {
        const Point origin=graph->localPosToCanvas(Point(0, 0));

        return Point(origin.x+graph->getWidth()-8, origin.y+graph->getHeight()-8);
    }

    //! The canvas point at a graph point.
    Point at(const PointF &graphPoint) const
    {
        const PointF p=graphToCanvas(graphPoint);

        return Point(int(std::floor(p.x)), int(std::floor(p.y)));
    }

    int selectionEvents=0;
};

}//namespace

TEST_CASE("NodeGraph: a click selects, Shift adds, Ctrl toggles, empty space clears")
{
    SelectScene scene;
    controls::Node *a=scene.nodes[0];
    controls::Node *b=scene.nodes[1];

    scene.click(scene.titleOf(a));
    CHECK(scene.graph->getSelectedNodes() == std::vector<controls::Node *>{a});
    CHECK(scene.selectionEvents == 1);

    // again: nothing changes, no event
    scene.click(scene.titleOf(a));
    CHECK(scene.selectionEvents == 1);

    // Shift adds, by the body too
    scene.modifier(keys::Shift, true);
    controls::Base *body=b->getBody();
    scene.click(body->localPosToCanvas(Point(body->getWidth()/2, body->getHeight()/2)));
    scene.modifier(keys::Shift, false);
    CHECK(a->isSelected());
    CHECK(b->isSelected());
    CHECK(scene.selectionEvents == 2);

    // a plain click on one of a selection makes it the selection
    scene.click(scene.titleOf(b));
    CHECK(scene.graph->getSelectedNodes() == std::vector<controls::Node *>{b});
    CHECK(scene.selectionEvents == 3);

    // Ctrl toggles: on, then off, and a node Ctrl takes off is not dragged
    scene.modifier(keys::Control, true);
    scene.click(scene.titleOf(a));
    CHECK(a->isSelected());
    const Point start=a->getGraphPosition();
    scene.press(scene.titleOf(a));
    scene.frame();
    CHECK(!a->isSelected());
    CHECK(!scene.graph->draggingNodes());
    scene.move(Point(scene.titleOf(a).x+30, scene.titleOf(a).y));
    scene.frame();
    scene.release();
    scene.frame();
    scene.modifier(keys::Control, false);
    CHECK(a->getGraphPosition().x == start.x);
    CHECK(scene.selectionEvents == 5);

    // a link: alone, then Shift adds a node to it
    controls::Link *link=scene.links[2];
    std::vector<PointF> points;
    link->getPoints(points);
    const PointF middle((points.front().x+points.back().x)*0.5f, (points.front().y+points.back().y)*0.5f);
    scene.move(scene.at(middle));
    scene.frame();
    REQUIRE(HoveredControl == link);
    scene.click(scene.at(middle));
    CHECK(scene.graph->getSelectedLinks() == std::vector<controls::Link *>{link});
    CHECK(scene.graph->getSelectedNodes().empty());
    CHECK((shownSegments(scene.renderer, link).front().color == tokyonight::LinkHover.top));
    scene.move(scene.empty());
    scene.frame();
    CHECK((shownSegments(scene.renderer, link).front().color == tokyonight::NodeSelected.border));
    scene.modifier(keys::Shift, true);
    scene.click(scene.titleOf(a));
    scene.modifier(keys::Shift, false);
    CHECK(link->isSelected());
    CHECK(a->isSelected());
    const int events=scene.selectionEvents;

    // a click on empty space clears, but not under Ctrl
    scene.modifier(keys::Control, true);
    scene.click(scene.empty());
    scene.modifier(keys::Control, false);
    CHECK(a->isSelected());
    CHECK(scene.selectionEvents == events);
    scene.click(scene.empty());
    CHECK(scene.graph->getSelectedNodes().empty());
    CHECK(scene.graph->getSelectedLinks().empty());
    CHECK(scene.selectionEvents == events+1);
    CHECK(!scene.graph->marqueeActive());
}

TEST_CASE("NodeGraph: a marquee selects what it touches, each frame writing only what changed")
{
    SelectScene scene;
    const size_t primitives=scene.renderer.livePrimitives.size();
    controls::Node *first=scene.nodes[0];

    // a node selected before, which the marquee does not touch
    scene.graph->selectNode(scene.nodes[3]);
    scene.frame();

    // from the empty point left of node 1, down and right in steps
    PointF frameMin, frameMax;
    scene.nodes[1]->getGraphFrame(frameMin, frameMax);
    const PointF from(frameMin.x-15.0f, frameMin.y-15.0f);

    scene.move(scene.at(from));
    scene.frame();
    scene.press(scene.at(from));
    scene.frame();
    CHECK(scene.graph->marqueeActive());
    // under the threshold nothing is drawn yet
    scene.move(Point(scene.at(from).x+1, scene.at(from).y+1));
    scene.frame();
    CHECK(scene.written().empty());

    std::map<controls::Base *, bool> before;
    auto selection=[&]()
    {
        std::map<controls::Base *, bool> state;

        for(controls::Node *node : scene.nodes)
            state[node]=node->isSelected();
        for(controls::Link *link : scene.links)
            state[link]=link->isSelected();
        return state;
    };

    bool sawNode=false, sawLink=false;

    for(int step=1; step <= 8; ++step)
    {
        before=selection();
        scene.move(scene.at(PointF(from.x+step*40.0f, from.y+step*25.0f)));
        scene.frame();
        CAPTURE(step);

        // the marquee's slots and those of the items that changed
        std::set<size_t> allowed;
        for(controls::Base *part : scene.graph->getOverlayLayer()->getChildren())
            if(gweni_cast<controls::internal::NodeGraphMarquee>(part))
                for(size_t id : ownIds(part))
                    allowed.insert(id);
        for(controls::Base *part : scene.graph->getLinkLayer()->getChildren())
            if(gweni_cast<controls::internal::NodeGraphMarqueeFill>(part))
                for(size_t id : ownIds(part))
                    allowed.insert(id);
        CHECK(allowed.size() == 5);
        const std::map<controls::Base *, bool> after=selection();
        for(const auto &entry : after)
        {
            if(entry.second == before[entry.first])
                continue;
            const std::set<size_t> ids=scene.subtreeIds(entry.first);
            allowed.insert(ids.begin(), ids.end());
            if(gweni_cast<controls::Node>(entry.first))
                sawNode=true;
            else
                sawLink=true;
        }
        for(size_t id : scene.written())
            CHECK(allowed.count(id) == 1);

        // what it touches, and nothing it does not
        PointF min, max;
        scene.graph->getMarqueeRect(min, max);
        for(controls::Node *node : scene.nodes)
        {
            PointF nodeMin, nodeMax;
            node->getGraphFrame(nodeMin, nodeMax);
            const bool touched=(nodeMin.x <= max.x) && (nodeMax.x >= min.x) && (nodeMin.y <= max.y)
                && (nodeMax.y >= min.y);
            CHECK(node->isSelected() == touched);
        }
        for(controls::Link *link : scene.links)
            CHECK(link->isSelected() == link->touches(min, max));
    }
    CHECK(sawNode);
    CHECK(sawLink);
    CHECK(!first->isSelected());
    // the one selected before, untouched, is off: a plain marquee replaces
    CHECK(!scene.nodes[3]->isSelected());

    scene.release();
    scene.frame();
    CHECK(scene.selectionEvents == 1);
    CHECK(!scene.graph->marqueeActive());
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == primitives);

    // under Shift the earlier selection stays; Escape gives it back
    scene.graph->clearSelection();
    scene.graph->selectNode(scene.nodes[5]);
    scene.frame();
    scene.modifier(keys::Shift, true);
    scene.press(scene.at(from));
    scene.frame();
    scene.move(scene.at(PointF(from.x+200.0f, from.y+120.0f)));
    scene.frame();
    CHECK(scene.nodes[5]->isSelected());
    CHECK(scene.nodes[1]->isSelected());
    scene.modifier(keys::Shift, false);
    scene.canvas->inputModifierKey(keys::Escape, true);
    scene.canvas->inputModifierKey(keys::Escape, false);
    scene.frame();
    CHECK(!scene.graph->marqueeActive());
    CHECK(scene.graph->getSelectedNodes() == std::vector<controls::Node *>{scene.nodes[5]});
    scene.release();
    scene.frame();
    CHECK(scene.selectionEvents == 1);
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == primitives);
}

namespace
{

struct RemoveScene: SelectScene
{
    RemoveScene()
    {
        graph->onNodeRemoveRequested.add(this, &RemoveScene::nodeRequested);
        graph->onLinkRemoveRequested.add(this, &RemoveScene::linkRequested);
        graph->onNodeRemoved.add(this, &RemoveScene::nodeRemoved);
        graph->onLinkRemoved.add(this, &RemoveScene::linkRemoved);
    }

    void nodeRequested(event::Info info)
    {
        nodeRequests.push_back(info.control);
        if(info.control == vetoed)
            graph->vetoRemove();
    }

    void linkRequested(event::Info info)
    {
        linkRequests.push_back(info.control);
        if(info.control == vetoed)
            graph->vetoRemove();
    }

    void nodeRemoved(event::Info info) { nodesRemoved.push_back(info.control); }
    void linkRemoved(event::Info info) { linksRemoved.push_back(info.control); }

    void pressDelete()
    {
        canvas->inputModifierKey(keys::Delete, true);
        canvas->inputModifierKey(keys::Delete, false);
        frame();
    }

    void rightClick(const Point &at)
    {
        move(at);
        frame();
        canvas->inputMouseButton(1, true);
        canvas->inputMouseButton(1, false);
        frame();
    }

    controls::Base *vetoed=nullptr;
    std::vector<controls::Base *> nodeRequests;
    std::vector<controls::Base *> linkRequests;
    std::vector<controls::Base *> nodesRemoved;
    std::vector<controls::Base *> linksRemoved;
};

}//namespace

TEST_CASE("NodeGraph: Delete removes the selection, each item asked first and told after")
{
    RemoveScene scene;
    const size_t primitives=scene.renderer.livePrimitives.size();

    // node 1 (links 0->1 and 1->2), node 4 (vetoed, link 3->4 stays) and
    // the link 3->4 too, which its request may remove on its own
    controls::Node *one=scene.nodes[1];
    controls::Node *four=scene.nodes[4];
    controls::Link *threeFour=scene.links[2];

    scene.click(scene.titleOf(one));
    CHECK(KeyboardFocus == scene.graph);
    scene.modifier(keys::Shift, true);
    scene.click(scene.titleOf(four));
    scene.modifier(keys::Shift, false);
    scene.graph->selectLink(threeFour);
    scene.vetoed=four;
    const int events=scene.selectionEvents;

    scene.pressDelete();
    CHECK(scene.nodeRequests == std::vector<controls::Base *>{one, four});
    CHECK(scene.nodesRemoved == std::vector<controls::Base *>{one});
    CHECK(scene.linkRequests == std::vector<controls::Base *>{threeFour});
    CHECK(scene.linksRemoved == std::vector<controls::Base *>{scene.links[0], scene.links[1], threeFour});
    CHECK(scene.selectionEvents == events+1);
    CHECK(scene.graph->getLinks().empty());
    CHECK(scene.graph->getNodes().size() == 5);
    CHECK(four->isSelected());
    CHECK(!scene.out[0]->isConnected());
    CHECK(!scene.in[2]->isConnected());
    scene.frame();

    // a body's TextBox holding the keys keeps Delete
    controls::TextBox *box=scene.nodes[0]->newChild<controls::TextBox>();
    box->setDock(DockPosition::Top);
    scene.frame();
    box->focus();
    scene.pressDelete();
    CHECK(scene.graph->getNodes().size() == 5);
    CHECK(scene.nodeRequests.size() == 2);

    // everything gone gives every id back
    scene.graph->focus();
    scene.vetoed=nullptr;
    for(controls::Node *node : scene.graph->getNodes())
        scene.graph->selectNode(node);
    scene.pressDelete();
    scene.frame();
    CHECK(scene.graph->getNodes().empty());
    CHECK(scene.renderer.livePrimitives.size() < primitives);
    const size_t emptyGraph=scene.renderer.livePrimitives.size();
    scene.graph->addNode("X", Point(0, 0));
    scene.frame();
    scene.graph->removeNode(scene.graph->getNodes().front());
    scene.frame();
    CHECK(scene.renderer.livePrimitives.size() == emptyGraph);
    CHECK(scene.renderer.doubleReleased == 0);
}

TEST_CASE("NodeGraph: a right click opens the menu of what it is on, with its graph position")
{
    RemoveScene scene;
    controls::Menu *graphMenu=scene.graph->createContextMenu();
    controls::Menu *nodeMenu=scene.nodes[0]->createContextMenu();
    controls::Menu *linkMenu=scene.links[2]->createContextMenu();

    graphMenu->addItem("Add");
    nodeMenu->addItem("Delete node");
    linkMenu->addItem("Delete link");
    scene.graph->setZoom(1.5f);
    scene.frame();

    // empty space: the graph's, at the click's graph point
    scene.rightClick(scene.empty());
    CHECK(scene.canvas->shownContextMenu() == graphMenu);
    const PointF expected=scene.graph->canvasToGraph(PointF(scene.empty()));
    CHECK(scene.graph->getContextMenuPosition().x == doctest::Approx(expected.x));
    CHECK(scene.graph->getContextMenuPosition().y == doctest::Approx(expected.y));
    scene.canvas->closeContextMenu();
    scene.frame();

    // a node's body: the node's
    controls::Base *body=scene.nodes[0]->getBody();
    scene.rightClick(body->localPosToCanvas(Point(body->getWidth()/2, body->getHeight()/2)));
    CHECK(scene.canvas->shownContextMenu() == nodeMenu);
    scene.canvas->closeContextMenu();
    scene.frame();

    // a link: the link's
    std::vector<PointF> points;
    scene.links[2]->getPoints(points);
    scene.rightClick(scene.at(PointF((points.front().x+points.back().x)*0.5f,
        (points.front().y+points.back().y)*0.5f)));
    CHECK(scene.canvas->shownContextMenu() == linkMenu);
    scene.canvas->closeContextMenu();
    scene.frame();

    // a right drag pans and opens none
    const PointF pan=scene.graph->pan();
    const Point from=scene.empty();
    scene.move(from);
    scene.frame();
    scene.canvas->inputMouseButton(1, true);
    scene.move(Point(from.x-40, from.y-30));
    scene.frame();
    scene.canvas->inputMouseButton(1, false);
    scene.frame();
    CHECK(scene.canvas->shownContextMenu() == nullptr);
    CHECK(scene.graph->pan().x == doctest::Approx(pan.x-40.0f));

    // from code, at a graph point
    scene.graph->openContextMenuAt(PointF(123.0f, 45.0f));
    scene.frame();
    CHECK(scene.canvas->shownContextMenu() == graphMenu);
    CHECK(scene.graph->getContextMenuPosition().x == 123.0f);
}
