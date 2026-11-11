/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#ifndef _gweni_controls_nodeGraph_h_
#define _gweni_controls_nodeGraph_h_

#include <gweni/controls/base.h>
#include <gweni/controls/zoomPanel.h>

#include <cstdint>
#include <functional>
#include <memory>
#include <vector>

namespace gweni
{
namespace controls
{

class Label;
class Link;
class Node;
class NodeGraph;
class Pin;

//! Whether a pin takes a link in or sends one out.
enum class PinDirection
{
    Input,
    Output
};

//! The way a Link runs from one pin to the other.
enum class LinkStyle
{
    //! one segment
    Straight,
    //! a cubic leaving and entering each pin along its side's outward
    //! direction, LinkCurveMin..LinkCurveMax segments by its length on screen
    Bezier,
    //! horizontal and vertical runs, 3 segments (5 when the pins do not face
    //! each other with room between them)
    Orthogonal
};

//! The marker drawn for a pin on its node's edge.
enum class PinShape
{
    Circle,
    Square,
    Triangle
};

//! The side of its node a pin sits on: its marker on that edge, its name inside.
enum class PinSide
{
    Left,
    Right
};

namespace internal
{

//! One of a NodeGraph's three layers, each the whole graph's size: the links,
//! under the nodes, the nodes, and the overlay (a link being dragged out, the
//! marquee) over them. It draws nothing and takes no mouse input itself, so a
//! point over none of its children is the graph's.
class GWENI_EXPORT NodeGraphLayer: public controls::Base
{
public:

    GWENI_CONTROL(NodeGraphLayer, controls::Base);
};

//! A Node's title bar: its title Label, drawn by the Node's renderer. A left
//! press on it starts its graph's node drag (NodeGraph::onNodesMoved), the
//! moves go on with it while the mouse is held, and the release ends it.
class GWENI_EXPORT NodeTitleBar: public controls::Base
{
public:

    GWENI_CONTROL(NodeTitleBar, controls::Base);

    void onMouseClickLeft(int x, int y, bool down) override;
    void onMouseMoved(int x, int y, int deltaX, int deltaY) override;

protected:

    bool m_pressed=false;
};

//! A Node's column of pins on one side, and the row holding both columns: they
//! draw nothing and take no mouse input.
class GWENI_EXPORT NodePinColumn: public controls::Base
{
public:

    GWENI_CONTROL(NodePinColumn, controls::Base);

    //! A column (or the row) moved in its node moves the pins in it: their
    //! links are drawn again.
    void onBoundsChanged(gweni::Rect oldBounds) override;
};

//! A Node's body, where the app's controls go (Node::getBody()); the Node's
//! renderer draws its background.
class GWENI_EXPORT NodeBody: public controls::Base
{
public:

    GWENI_CONTROL(NodeBody, controls::Base);
};

}//namespace internal

//! A pin on a Node's side: a marker (a circle, square or triangle) on the
//! node's edge and a name beside it, inside the node. An Input pin sits on the
//! left by default and an Output pin on the right (setSide()). Its colour is
//! the theme's PinIn or PinOut unless the app gives it one, and it is drawn
//! filled while it is connected (setConnected()) and as a ring otherwise.
//!
//! The pin is a view of the app's model: it holds a user id and pointer, and
//! nothing in it knows what its type or value is.
class GWENI_EXPORT Pin: public Base
{
public:

    GWENI_CONTROL(Pin, Base);

public:

    //! The marker's side in units: it is centred on the node's frame edge, so
    //! half of it reaches into the node's overhang (Node::PinOverhang).
    static constexpr int MarkerSize=10;
    //! The gap between the marker and the name.
    static constexpr int NameGap=4;
    //! The least height of a pin's row.
    static constexpr int RowHeight=20;

    PinDirection getDirection() const { return m_direction; }

    //! The pin's name, shown beside the marker.
    void setLabel(const String &label);
    const String &getLabel() const;
    Label *getLabelControl() const { return m_label; }

    //! The marker's colour; resetColor() goes back to the theme's (PinIn or
    //! PinOut by direction).
    void setColor(const Color &color);
    void resetColor();
    bool hasColor() const { return m_hasColor; }
    //! The app's colour, or a default Color without one (the renderer reads
    //! the theme's then).
    const Color &getColor() const { return m_color; }

    void setShape(PinShape shape);
    PinShape getShape() const { return m_shape; }

    //! Moves the pin to the node's other column; a pin with no node keeps it
    //! until it gets one.
    void setSide(PinSide side);
    PinSide getSide() const { return m_side; }

    //! Whether a link ends on it: the marker is filled, and a ring otherwise.
    //! A link made or removed sets it (on while the pin has a link); the app may
    //! set it too.
    void setConnected(bool connected);
    bool isConnected() const { return m_connected; }

    //! The app's id and pointer for the model pin this one shows.
    void setUserId(uint64_t id) { m_userId=id; }
    uint64_t getUserId() const { return m_userId; }
    void setUserData(void *data) { m_userData=data; }
    void *getUserData() const { return m_userData; }

    //! The node the pin is on, or null.
    Node *getNode() const { return m_node; }

    //! The marker's rect in the pin's own coordinates.
    Rect getMarkerRect() const;
    //! The marker's centre in graph units, where a link to the pin ends.
    PointF getAnchor() const;
    //! The way out of the pin, along x: +1 on the right side, -1 on the left.
    float outward() const { return (m_side == PinSide::Right)?1.0f:-1.0f; }

    //! The links ending on the pin, oldest first.
    const std::vector<Link *> &getLinks() const { return m_links; }

    ~Pin() override;

    void onBoundsChanged(gweni::Rect oldBounds) override;
    //! A left press (without Space held, which pans) drags a new link out of
    //! the pin: the graph's link drag (NodeGraph::draggingLink()).
    InputCapture *pressGesture(int button) override;

protected:

    friend class Node;
    friend class Link;
    friend class NodeGraph;

    //! The links ending on the pin are drawn again: it moved.
    void linksMoved();

    //! Puts the name on the side away from the marker.
    void placeName();

    PinDirection m_direction=PinDirection::Input;
    PinShape m_shape=PinShape::Circle;
    PinSide m_side=PinSide::Left;
    bool m_connected=false;
    bool m_hasColor=false;
    Color m_color;
    uint64_t m_userId=0;
    void *m_userData=nullptr;
    Node *m_node=nullptr;
    Label *m_label=nullptr;
    std::vector<Link *> m_links;
};

//! A link from one Pin to another, drawn as a polyline of drawLine segments
//! (Straight, Bezier or Orthogonal, getPoints()) from the one's anchor to the
//! other's, in its colour (the from pin's unless the app gives it one) and its
//! thickness in graph units, so it zooms with the graph. It is in its graph's
//! link layer, under every node, and covers that whole layer, so moving a
//! node moves no link: the link only draws its segments again, and its
//! renderer rewrites only the slots of the segments that moved.
//!
//! Made and removed through its NodeGraph (addLink(), removeLink()); deleting
//! a pin, or its node, deletes its links. It holds the app's id and pointer.
class GWENI_EXPORT Link: public Base
{
public:

    GWENI_CONTROL(Link, Base);

public:

    //! A Bezier's least and most segments: the count follows its control
    //! polygon's length on screen, one segment per LinkCurveStep units.
    static constexpr int LinkCurveMin=16;
    static constexpr int LinkCurveMax=24;
    static constexpr float LinkCurveStep=16.0f;
    //! An Orthogonal link's run straight out of a pin before it turns, and a
    //! Bezier's least handle, in graph units.
    static constexpr float LinkStub=20.0f;
    //! The most segments any style draws (the renderer's slots).
    static constexpr int MaxSegments=LinkCurveMax;
    //! How near its curve, in canvas units on screen, the mouse hovers a link.
    static constexpr float HoverDistance=4.0f;

    ~Link() override;

    //! The pins it joins; a pin deleted leaves it with null there (it is
    //! deleted with it).
    Pin *getFrom() const { return m_from; }
    Pin *getTo() const { return m_to; }

    void setStyle(LinkStyle style);
    LinkStyle getStyle() const { return m_style; }

    //! The colour; resetColor() goes back to the from pin's.
    void setColor(const Color &color);
    void resetColor();
    bool hasColor() const { return m_hasColor; }
    const Color &getColor() const { return m_color; }

    //! The thickness in graph units (2 by default).
    void setThickness(float thickness);
    float getThickness() const { return m_thickness; }

    void setUserId(uint64_t id) { m_userId=id; }
    uint64_t getUserId() const { return m_userId; }
    void setUserData(void *data) { m_userData=data; }
    void *getUserData() const { return m_userData; }

    //! The graph the link is in, or null.
    NodeGraph *getGraph() const;

    //! The polyline from the from pin's anchor to the to pin's, in graph
    //! units, at the graph's zoom (a Bezier's segment count), with no two
    //! points the same; empty while a pin is missing.
    void getPoints(std::vector<PointF> &points) const;

    //! An end moved: the segments are drawn again.
    void endpointsMoved();

    //! The link's own top-left in graph units: a point p of the link's own
    //! coordinates is the graph point p plus this.
    PointF graphOrigin() const;
    //! The distance from the graph point \p point to the link's curve, in
    //! graph units; a huge value for a link with no curve.
    float distanceTo(const PointF &point) const;

    //! A link being dragged out of a pin: it ends at a free point, not a pin,
    //! takes no mouse input and is no pin's link (NodeGraph::getPreviewLink()).
    bool isPreview() const { return m_preview; }
    //! The preview's free end, in graph units.
    const PointF &getFreeEnd() const { return m_freeEnd; }

    //! Hit only within HoverDistance on screen of the curve: the link covers
    //! its whole layer, and anywhere else is the graph's, or another link's.
    Base *getControlAt(int x, int y, bool bOnlyIfMouseEnabled=true) override;

protected:

    friend class NodeGraph;
    friend class Pin;

    //! The link leaves \p pin (deleted, or the link removed).
    void detach(Pin *pin);
    //! The preview's free end moved (graph units).
    void setFreeEnd(const PointF &point);

    Pin *m_from=nullptr;
    Pin *m_to=nullptr;
    LinkStyle m_style=LinkStyle::Bezier;
    bool m_hasColor=false;
    Color m_color;
    float m_thickness=2.0f;
    uint64_t m_userId=0;
    void *m_userData=nullptr;
    bool m_preview=false;
    PointF m_freeEnd;

    //! getPoints()'s last polyline and what it was worked out from, so a
    //! hit-test and the renderer share it until an end, the style or the zoom
    //! moves
    mutable std::vector<PointF> m_points;
    mutable bool m_pointsValid=false;
    mutable PointF m_pointsFrom;
    mutable PointF m_pointsTo;
    mutable float m_pointsZoom=0.0f;
};

//! A node of a NodeGraph: a title bar, a row of pins, inputs down the left
//! and outputs down the right, then a body holding any controls the app puts
//! there (getBody(), which newChild() fills), all at a position in graph units.
//!
//! Its frame, the rounded box its renderer draws, is the node's bounds less
//! PinOverhang on the left and right, so a pin's marker centred on the
//! frame's edge is still inside the node: it is drawn, and hit, without the
//! node's clip cutting it. The node is NodeWidth wide unless the app asks for
//! another width or its content needs more, and as high as its content.
//!
//! The title bar is filled in the theme's NodeTitle unless the app gives the
//! node a colour (setTitleColor()); the node's tooltip (setToolTipText()) shows
//! over the title bar and the body. The node holds the app's id and pointer
//! for the model node it shows.
class GWENI_EXPORT Node: public Base
{
public:

    GWENI_CONTROL(Node, Base);

public:

    //! How far a pin's marker reaches past the frame, on both sides.
    static constexpr int PinOverhang=Pin::MarkerSize/2+1;
    static constexpr int TitleHeight=22;
    //! The width a node asks for until the app asks for another, its frame
    //! and both overhangs.
    static constexpr int NodeWidth=160+2*PinOverhang;
    //! The body's padding inside the frame.
    static constexpr int BodyPadding=6;

    void setTitle(const String &title);
    const String &getTitle() const;
    Label *getTitleLabel() const { return m_title; }
    Base *getTitleBar() const;

    //! The title bar's fill; clearTitleColor() goes back to the theme's
    //! NodeTitle.
    void setTitleColor(const Color &color);
    void clearTitleColor();
    bool hasTitleColor() const { return m_hasTitleColor; }
    const Color &getTitleColor() const { return m_titleColor; }

    //! Where the node's top-left corner is in the graph, in graph units: a
    //! move, as an app's setPos() is, laying out only the node's place. Graph
    //! points run from -NodeGraph::GraphExtent to +GraphExtent on both axes.
    void setGraphPosition(const Point &position);
    Point getGraphPosition() const;

    //! A new pin at the end of its side's column, Input on the left and
    //! Output on the right.
    Pin *addPin(PinDirection direction, const String &name);
    //! Deletes the pin at once (not from a handler of the pin's own).
    void removePin(Pin *pin);
    //! The pins, the left column's from the top, then the right's.
    std::vector<Pin *> getPins() const;

    //! The body, where the node's own controls go.
    Base *getBody() const { return m_innerPanel; }

    //! The graph the node is in, or null.
    NodeGraph *getGraph() const;

    //! The frame's rect in the node's own coordinates: the bounds less
    //! PinOverhang on either side.
    Rect getFrameRect() const;

    ~Node() override;

    //! Whether the node is in its graph's selection (NodeGraph::selectNode()),
    //! drawn in the theme's NodeSelected.
    bool isSelected() const { return m_selected; }

    void setUserId(uint64_t id) { m_userId=id; }
    uint64_t getUserId() const { return m_userId; }
    void setUserData(void *data) { m_userData=data; }
    void *getUserData() const { return m_userData; }

    //! A move (or a new size) of the node moves its pins: their links are
    //! drawn again.
    void onBoundsChanged(gweni::Rect oldBounds) override;
    //! Every link ending on one of the node's pins.
    std::vector<Link *> getLinks() const;

protected:

    friend class Pin;
    friend class internal::NodePinColumn;
    friend class NodeGraph;

    //! The column holding \p side's pins.
    internal::NodePinColumn *column(PinSide side) const;
    //! The links of every pin are drawn again.
    void linksMoved();

    internal::NodeTitleBar *m_titleBar=nullptr;
    Label *m_title=nullptr;
    internal::NodePinColumn *m_pinRow=nullptr;
    internal::NodePinColumn *m_columns[2]={nullptr, nullptr};
    bool m_hasTitleColor=false;
    Color m_titleColor;
    bool m_selected=false;
    uint64_t m_userId=0;
    void *m_userData=nullptr;
};

//! A node-graph editor's view: a ZoomPanel holding Nodes at positions in
//! graph units over a grid. The app owns the graph's model; the NodeGraph
//! holds only view objects, each carrying the app's id or pointer, and edits
//! the app makes from code fire no request events.
//!
//! A graph point p is the ZoomPanel's content point p: it is drawn at the
//! panel's origin plus pan()+p*zoom(). The content is one panel GraphExtent
//! units each way from the graph's origin, so nodes may sit at negative
//! points too.
//!
//! The grid is the graph's own drawing, under everything in it: a line every
//! gridStep() graph units, each GridMajorEvery-th one brighter, following the
//! zoom and the pan, each line 1 unit thick on screen at any zoom. The step is
//! GridSpacing times the least power of GridMajorEvery that puts its lines at
//! least MinGridGap units apart on screen, so a zoom-out never draws lines
//! closer than that. The grid is one primitive however many lines it has
//! (renderer::Base::drawGrid), which a zoom or pan rewrites, with the nodes'.
class GWENI_EXPORT NodeGraph: public ZoomPanel
{
public:

    GWENI_CONTROL(NodeGraph, ZoomPanel);

public:

    ~NodeGraph() override;

    //! How far, in graph units, the graph reaches from its origin each way.
    static constexpr int GraphExtent=1<<16;
    //! The grid's finest step, in graph units.
    static constexpr int GridSpacing=20;
    //! Every this many lines one is a major line, and the factor the step
    //! grows by when it zooms out.
    static constexpr int GridMajorEvery=4;
    //! The least gap between two grid lines on screen, in canvas units.
    static constexpr float MinGridGap=8.0f;

    //! A new node at \p position (its top-left, graph units), in front of the
    //! others.
    Node *addNode(const String &title, const Point &position);
    //! Takes the node off the graph, its links with it (removeLink()), and
    //! deletes it before the next frame's layout (delayedDelete()), so a
    //! handler of its own may call it.
    void removeNode(Node *node);
    //! The nodes, back to front, without one removeNode() has taken off.
    std::vector<Node *> getNodes() const;

    //! A new link from \p from to \p to, both pins of nodes in this graph,
    //! marking both connected; null for a pin that is not, or the same pin
    //! twice. It is in the link layer, under every node.
    Link *addLink(Pin *from, Pin *to, LinkStyle style=LinkStyle::Bezier);
    //! Takes the link off its pins at once (setting each one's connected to
    //! whether it has another link) and deletes it before the next frame's
    //! layout.
    void removeLink(Link *link);
    //! The links, oldest first, without one removeLink() has taken off.
    std::vector<Link *> getLinks() const;

    //! The selection's nodes: each is drawn selected (Node::isSelected()).
    //! \p selected adds \p node to it or takes it off; selectOnly() makes it
    //! the selection's one node; clearSelection() empties it. Each marks only
    //! the nodes whose state changes.
    void selectNode(Node *node, bool selected=true);
    void selectOnly(Node *node);
    void clearSelection();
    //! The selected nodes, back to front.
    std::vector<Node *> getSelectedNodes() const;

    //! Whether a title-bar drag is moving nodes (a press held on a title bar).
    bool draggingNodes() const { return !m_dragged.empty(); }
    //! The nodes the last drag moved, while onNodesMoved runs and after it,
    //! until the next drag starts.
    const std::vector<Node *> &getMovedNodes() const { return m_moved; }

    //! Fired once when a title-bar drag that moved its nodes is let go, after
    //! the move (getMovedNodes() has them); a drag that moved nothing fires
    //! nothing. An app's own setGraphPosition() fires nothing.
    event::Caller onNodesMoved;

    //! Which pins a link dragged out of one may end on: \p filter(from, to)
    //! with \p from the Output end (the pin dragged from when both are of one
    //! direction). Without one, an Input and an Output of two nodes.
    void setLinkFilter(std::function<bool(Pin *from, Pin *to)> filter);
    //! Whether a link dragged from \p a may end on \p b, and the ends it would
    //! have: \p from and \p to, the Output first.
    bool canLink(Pin *a, Pin *b, Pin **from=nullptr, Pin **to=nullptr) const;

    //! The style of a link the user drags out (Bezier by default).
    void setNewLinkStyle(LinkStyle style) { m_newLinkStyle=style; }
    LinkStyle getNewLinkStyle() const { return m_newLinkStyle; }

    //! Whether a link is being dragged out of a pin, and its preview (in the
    //! overlay layer, ending at the mouse), or null.
    bool draggingLink() const;
    Link *getPreviewLink() const;
    //! Ends a link drag as Escape does: no link, no event.
    void cancelLinkDrag();

    //! Fired when a dragged link is dropped on a pin it may end on
    //! (canLink()), before the link is made: getRequestedFrom() and
    //! getRequestedTo() are its ends, and a handler that calls vetoLink()
    //! keeps it from being made.
    event::Caller onLinkRequested;
    void vetoLink() { m_linkVetoed=true; }
    Pin *getRequestedFrom() const { return m_requestedFrom; }
    Pin *getRequestedTo() const { return m_requestedTo; }
    //! Fired after a link the user dragged out is made, \p info.control the
    //! Link.
    event::Caller onLinkCreated;
    //! Fired when a dragged link is dropped on the graph where no node is,
    //! \p info.point the drop in whole graph units: getDropPosition() is the
    //! exact point and getDropPin() the pin it was dragged from. Nothing is
    //! made.
    event::Caller onLinkDroppedOnEmpty;
    const PointF &getDropPosition() const { return m_dropPosition; }
    Pin *getDropPin() const { return m_dropPin; }

    //! The graph's step between two grid lines at the current zoom.
    float gridStep() const;

    //! A point in the panel's own units as a graph point, and back.
    PointF localToGraph(const PointF &local) const { return toChildSpace(local); }
    PointF graphToLocal(const PointF &graph) const { return fromChildSpace(graph); }
    //! A canvas point as a graph point (canvasPosToLocalF() through the zoom).
    PointF canvasToGraph(const PointF &canvas) const;

    //! The layers, links, nodes and overlay, in that order of z.
    Base *getLinkLayer() const { return m_linkLayer; }
    Base *getNodeLayer() const { return m_nodeLayer; }
    Base *getOverlayLayer() const { return m_overlayLayer; }

    void arrange(skin::Base *skin, Dim dim) override;

protected:

    //! The grid follows the zoom and the pan: the graph draws its lines again.
    void spaceChanged(bool zoomed) override;

    friend class Node;
    friend class internal::NodeTitleBar;

    //! A left press on \p node's title bar at the canvas point \p at: an
    //! unselected node becomes the selection's one node, the pressed node goes
    //! to the front of the node layer, and the selection is taken to move.
    void beginNodeDrag(Node *node, const Point &at);
    //! The mouse at the canvas point \p at: every node taken moves by the
    //! mouse's motion from the press, in whole graph units.
    void dragNodes(const Point &at);
    //! The release: onNodesMoved if anything moved.
    void endNodeDrag();
    //! A node deleted is dropped from the drag and the moved nodes.
    void forgetNode(Node *node);

    friend class Pin;
    class LinkDrag;

    //! A pin deleted ends a link drag out of it.
    void forgetPin(Pin *pin);
    //! A left press on \p pin: the link drag, or null if the pin is not this
    //! graph's.
    InputCapture *beginLinkDrag(Pin *pin);
    //! The pin under the canvas point \p at, or null.
    Pin *pinAt(const Point &at);
    //! Whether the canvas point \p at is on the graph where no node is.
    bool emptyAt(const Point &at);
    //! The drop of a link dragged out of \p pin at the canvas point \p at.
    void dropLink(Pin *pin, const Point &at);

    std::unique_ptr<LinkDrag> m_linkDrag;
    std::function<bool(Pin *, Pin *)> m_linkFilter;
    LinkStyle m_newLinkStyle=LinkStyle::Bezier;
    bool m_linkVetoed=false;
    Pin *m_requestedFrom=nullptr;
    Pin *m_requestedTo=nullptr;
    PointF m_dropPosition;
    Pin *m_dropPin=nullptr;

    struct DraggedNode
    {
        Node *node;
        Point start;
    };

    //! the nodes a drag moves, with where each started
    std::vector<DraggedNode> m_dragged;
    //! the press in graph units
    PointF m_dragFrom;
    bool m_dragMoved=false;
    std::vector<Node *> m_moved;

    internal::NodeGraphLayer *m_linkLayer=nullptr;
    internal::NodeGraphLayer *m_nodeLayer=nullptr;
    internal::NodeGraphLayer *m_overlayLayer=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_nodeGraph_h_
