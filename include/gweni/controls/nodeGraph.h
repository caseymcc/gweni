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
#include <vector>

namespace gweni
{
namespace controls
{

class Label;
class Node;
class NodeGraph;
class Pin;

//! Whether a pin takes a link in or sends one out.
enum class PinDirection
{
    Input,
    Output
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

//! A Node's title bar: its title Label, drawn by the Node's renderer.
class GWENI_EXPORT NodeTitleBar: public controls::Base
{
public:

    GWENI_CONTROL(NodeTitleBar, controls::Base);
};

//! A Node's column of pins on one side, and the row holding both columns: they
//! draw nothing and take no mouse input.
class GWENI_EXPORT NodePinColumn: public controls::Base
{
public:

    GWENI_CONTROL(NodePinColumn, controls::Base);
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

protected:

    friend class Node;

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

    void setUserId(uint64_t id) { m_userId=id; }
    uint64_t getUserId() const { return m_userId; }
    void setUserData(void *data) { m_userData=data; }
    void *getUserData() const { return m_userData; }

protected:

    friend class Pin;

    //! The column holding \p side's pins.
    internal::NodePinColumn *column(PinSide side) const;

    internal::NodeTitleBar *m_titleBar=nullptr;
    Label *m_title=nullptr;
    internal::NodePinColumn *m_pinRow=nullptr;
    internal::NodePinColumn *m_columns[2]={nullptr, nullptr};
    bool m_hasTitleColor=false;
    Color m_titleColor;
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
    //! Takes the node off the graph and deletes it before the next frame's
    //! layout (delayedDelete()), so a handler of its own may call it.
    void removeNode(Node *node);
    //! The nodes, back to front, without one removeNode() has taken off.
    std::vector<Node *> getNodes() const;

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

    internal::NodeGraphLayer *m_linkLayer=nullptr;
    internal::NodeGraphLayer *m_nodeLayer=nullptr;
    internal::NodeGraphLayer *m_overlayLayer=nullptr;
};

}//namespace controls
}//namespace gweni
#endif//_gweni_controls_nodeGraph_h_
