/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/nodeGraph.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>

#include <algorithm>
#include <cmath>

namespace gweni
{
namespace controls
{
namespace internal
{

GWENI_CONTROL_CONSTRUCTOR(NodeGraphLayer)
{
    setMouseInputEnabled(false);
}

GWENI_CONTROL_CONSTRUCTOR(NodeTitleBar)
{
}

GWENI_CONTROL_CONSTRUCTOR(NodePinColumn)
{
    setMouseInputEnabled(false);
}

void NodePinColumn::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);

    // a column's node, or the row's
    for(Base *parent=getActualParent(); parent; parent=parent->getActualParent())
    {
        if(Node *node=gweni_cast<Node>(parent))
        {
            node->linksMoved();
            break;
        }
    }
}

GWENI_CONTROL_CONSTRUCTOR(NodeBody)
{
}

}//namespace internal


///////////////////////////////////////////////////////////////////////////
// Pin

// A row of its column, RowHeight high and the column's width, its name in the
// padded box beside the marker (placeName()).
GWENI_CONTROL_CONSTRUCTOR(Pin)
{
    setDock(DockPosition::Top);
    setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    setHeight(RowHeight);

    m_label=newChild<Label>();
    m_label->setDock(DockPosition::Center);
    placeName();
}

void Pin::placeName()
{
    // the marker's centre is PinOverhang in from the pin's edge on its side
    const int inset=Node::PinOverhang+MarkerSize/2+NameGap;

    if(m_side == PinSide::Left)
    {
        setPadding(Padding(inset, 0, 0, 0));
        m_label->setTextAlignment(Alignment::Left|Alignment::CenterV);
    }
    else
    {
        setPadding(Padding(0, 0, inset, 0));
        m_label->setTextAlignment(Alignment::Right|Alignment::CenterV);
    }
}

void Pin::setLabel(const String &label)
{
    m_label->setText(label);
}

const String &Pin::getLabel() const
{
    return m_label->getText();
}

void Pin::setColor(const Color &color)
{
    if(m_hasColor && (m_color == color))
        return;

    m_hasColor=true;
    m_color=color;
    addStateChange(StateChange_Background);
}

void Pin::resetColor()
{
    if(!m_hasColor)
        return;

    m_hasColor=false;
    m_color=Color();
    addStateChange(StateChange_Background);
}

void Pin::setShape(PinShape shape)
{
    if(m_shape == shape)
        return;

    m_shape=shape;
    addStateChange(StateChange_Background);
}

void Pin::setSide(PinSide side)
{
    if(m_side == side)
        return;

    m_side=side;
    placeName();
    addStateChange(StateChange_Background);
    if(m_node)
        setParent(m_node->column(side));
    linksMoved();
}

// A pin deleted takes its links with it: each leaves its other pin first, so
// none points at this one while it goes.
Pin::~Pin()
{
    std::vector<Link *> links;

    links.swap(m_links);
    for(Link *link : links)
    {
        link->detach(this);
        if(Pin *other=link->getFrom()?link->getFrom():link->getTo())
        {
            link->detach(other);
            other->setConnected(!other->m_links.empty());
        }
        delete link;
    }
}

void Pin::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);
    linksMoved();
}

void Pin::linksMoved()
{
    for(Link *link : m_links)
        link->endpointsMoved();
}

void Pin::setConnected(bool connected)
{
    if(m_connected == connected)
        return;

    m_connected=connected;
    addStateChange(StateChange_Background);
}

Rect Pin::getMarkerRect() const
{
    const int centre=(m_side == PinSide::Left)?Node::PinOverhang:getWidth()-Node::PinOverhang;

    return Rect(centre-MarkerSize/2, (getHeight()-MarkerSize)/2, MarkerSize, MarkerSize);
}

// The pin is in its column, in the node's pin row, in the node: their offsets
// add up to the pin's place in the node, at the node's graph position.
PointF Pin::getAnchor() const
{
    const Rect marker=getMarkerRect();
    PointF anchor(float(marker.x)+marker.w*0.5f, float(marker.y)+marker.h*0.5f);

    for(const Base *control=this; control && (control != m_node); control=control->getActualParent())
    {
        anchor.x+=float(control->getX());
        anchor.y+=float(control->getY());
    }

    if(m_node)
    {
        const Point position=m_node->getGraphPosition();

        anchor.x+=float(position.x);
        anchor.y+=float(position.y);
    }
    return anchor;
}


///////////////////////////////////////////////////////////////////////////
// Node

// Docked from the top: the title bar and the body inset by the overhang on
// both sides, so they are the frame's width, and the pin row the node's whole
// width, so a marker centred on the frame's edge is inside it. Expand on both
// axes: the width asked for (NodeWidth by default) or the content's, and the
// content's height.
GWENI_CONTROL_CONSTRUCTOR(Node)
{
    setSizeFlags({SizeFlag::Expand, SizeFlag::Expand});
    setSize(NodeWidth, TitleHeight);

    m_titleBar=newChild<internal::NodeTitleBar>();
    m_titleBar->setDock(DockPosition::Top);
    m_titleBar->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_titleBar->setHeight(TitleHeight);
    m_titleBar->setMargin(Margin(PinOverhang, 0, PinOverhang, 0));

    m_title=m_titleBar->newChild<Label>();
    m_title->setDock(DockPosition::Center);
    m_title->setPadding(Padding(BodyPadding+2, 0, BodyPadding, 0));
    m_title->setTextAlignment(Alignment::Left|Alignment::CenterV);
    m_title->makeColorBright();

    m_pinRow=newChild<internal::NodePinColumn>();
    m_pinRow->setDock(DockPosition::Top);
    m_pinRow->setPadding(Padding(0, 3, 0, 3));
    // shown once it has a pin
    m_pinRow->hide();

    m_columns[0]=m_pinRow->newChild<internal::NodePinColumn>();
    m_columns[0]->setDock(DockPosition::Left);
    m_columns[1]=m_pinRow->newChild<internal::NodePinColumn>();
    m_columns[1]->setDock(DockPosition::Right);

    m_innerPanel=newChild<internal::NodeBody>();
    m_innerPanel->setDock(DockPosition::Top);
    m_innerPanel->setMargin(Margin(PinOverhang, 0, PinOverhang, 0));
    m_innerPanel->setPadding(Padding(BodyPadding, BodyPadding/2, BodyPadding, BodyPadding));
}

void Node::setTitle(const String &title)
{
    m_title->setText(title);
}

const String &Node::getTitle() const
{
    return m_title->getText();
}

Base *Node::getTitleBar() const
{
    return m_titleBar;
}

void Node::setTitleColor(const Color &color)
{
    if(m_hasTitleColor && (m_titleColor == color))
        return;

    m_hasTitleColor=true;
    m_titleColor=color;
    addStateChange(StateChange_Background);
}

void Node::clearTitleColor()
{
    if(!m_hasTitleColor)
        return;

    m_hasTitleColor=false;
    m_titleColor=Color();
    addStateChange(StateChange_Background);
}

// The node is in its graph's node layer, which is the graph's inner panel's
// size at its top-left, GraphExtent up and left of the graph's origin.
void Node::setGraphPosition(const Point &position)
{
    const int offset=getGraph()?NodeGraph::GraphExtent:0;

    setPos(position.x+offset, position.y+offset);
}

Point Node::getGraphPosition() const
{
    const int offset=getGraph()?NodeGraph::GraphExtent:0;
    const Rect &requested=getRequestedBounds();

    return Point(requested.x-offset, requested.y-offset);
}

void Node::onBoundsChanged(Rect oldBounds)
{
    ParentClass::onBoundsChanged(oldBounds);
    linksMoved();
}

void Node::linksMoved()
{
    for(internal::NodePinColumn *column : m_columns)
    {
        // none yet while the node is made
        if(!column)
            continue;
        for(Base *child : column->getChildren())
        {
            if(Pin *pin=gweni_cast<Pin>(child))
                pin->linksMoved();
        }
    }
}

std::vector<Link *> Node::getLinks() const
{
    std::vector<Link *> links;

    for(Pin *pin : getPins())
    {
        for(Link *link : pin->getLinks())
        {
            if(std::find(links.begin(), links.end(), link) == links.end())
                links.push_back(link);
        }
    }
    return links;
}

internal::NodePinColumn *Node::column(PinSide side) const
{
    return m_columns[(side == PinSide::Left)?0:1];
}

Pin *Node::addPin(PinDirection direction, const String &name)
{
    const PinSide side=(direction == PinDirection::Input)?PinSide::Left:PinSide::Right;
    Pin *pin=column(side)->newChild<Pin>();

    pin->m_direction=direction;
    pin->m_node=this;
    pin->m_side=side;
    pin->placeName();
    pin->setLabel(name);
    m_pinRow->show();
    return pin;
}

void Node::removePin(Pin *pin)
{
    if(!pin || (pin->m_node != this))
        return;

    delete pin;

    if(m_columns[0]->getChildren().empty() && m_columns[1]->getChildren().empty())
        m_pinRow->hide();
}

std::vector<Pin *> Node::getPins() const
{
    std::vector<Pin *> pins;

    for(internal::NodePinColumn *column : m_columns)
    {
        for(Base *child : column->getChildren())
        {
            if(Pin *pin=gweni_cast<Pin>(child))
                pins.push_back(pin);
        }
    }
    return pins;
}

NodeGraph *Node::getGraph() const
{
    // node layer, inner panel, graph
    Base *layer=getParent();

    if(!layer || !gweni_cast<internal::NodeGraphLayer>(layer))
        return nullptr;

    for(Base *parent=layer->getParent(); parent; parent=parent->getParent())
    {
        if(NodeGraph *graph=gweni_cast<NodeGraph>(parent))
            return graph;
    }
    return nullptr;
}

Rect Node::getFrameRect() const
{
    return Rect(PinOverhang, 0, std::max(getWidth()-2*PinOverhang, 0), getHeight());
}


///////////////////////////////////////////////////////////////////////////
// Link

// The whole link layer, at its top-left, placed by no one: a node moving
// moves no link, which only draws its segments again (endpointsMoved()).
GWENI_CONTROL_CONSTRUCTOR(Link)
{
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setBounds(0, 0, 2*NodeGraph::GraphExtent, 2*NodeGraph::GraphExtent);
}

Link::~Link()
{
    for(Pin *pin : {m_from, m_to})
    {
        if(pin)
            detach(pin);
    }
}

void Link::detach(Pin *pin)
{
    std::vector<Link *> &links=pin->m_links;

    links.erase(std::remove(links.begin(), links.end(), this), links.end());
    if(m_from == pin)
        m_from=nullptr;
    if(m_to == pin)
        m_to=nullptr;
    addStateChange(StateChange_Background);
}

void Link::setStyle(LinkStyle style)
{
    if(m_style == style)
        return;

    m_style=style;
    addStateChange(StateChange_Background);
}

void Link::setColor(const Color &color)
{
    if(m_hasColor && (m_color == color))
        return;

    m_hasColor=true;
    m_color=color;
    addStateChange(StateChange_Background);
}

void Link::resetColor()
{
    if(!m_hasColor)
        return;

    m_hasColor=false;
    m_color=Color();
    addStateChange(StateChange_Background);
}

void Link::setThickness(float thickness)
{
    if(m_thickness == thickness)
        return;

    m_thickness=thickness;
    addStateChange(StateChange_Background);
}

void Link::endpointsMoved()
{
    addStateChange(StateChange_Background);
}

NodeGraph *Link::getGraph() const
{
    for(Base *parent=getParent(); parent; parent=parent->getParent())
    {
        if(NodeGraph *graph=gweni_cast<NodeGraph>(parent))
            return graph;
    }
    return nullptr;
}

void Link::getPoints(std::vector<PointF> &points) const
{
    points.clear();
    if(!m_from || !m_to)
        return;

    const PointF a=m_from->getAnchor();
    const PointF b=m_to->getAnchor();
    // each end's way out of its pin
    const float outA=m_from->outward();
    const float outB=m_to->outward();
    auto add=[&](const PointF &point)
    {
        if(points.empty() || !(points.back() == point))
            points.push_back(point);
    };

    add(a);
    switch(m_style)
    {
    case LinkStyle::Straight:
        break;

    case LinkStyle::Bezier:
    {
        // the handles along the pins' ways out, half the run across, at least
        // LinkStub
        const float handle=std::max(LinkStub, 0.5f*std::fabs(b.x-a.x));
        const PointF c0(a.x+outA*handle, a.y);
        const PointF c1(b.x+outB*handle, b.y);
        const NodeGraph *graph=getGraph();
        const float zoom=graph?graph->zoom():1.0f;
        auto distance=[](const PointF &p, const PointF &q) { return std::hypot(q.x-p.x, q.y-p.y); };
        const float length=(distance(a, c0)+distance(c0, c1)+distance(c1, b))*zoom;
        const int segments=std::min(std::max(int(std::lround(length/LinkCurveStep)), LinkCurveMin),
            LinkCurveMax);

        for(int i=1; i < segments; ++i)
        {
            const double t=double(i)/segments;
            const double u=1.0-t;
            const double w0=u*u*u;
            const double w1=3.0*u*u*t;
            const double w2=3.0*u*t*t;
            const double w3=t*t*t;

            add(PointF(static_cast<float>(w0*a.x+w1*c0.x+w2*c1.x+w3*b.x),
                static_cast<float>(w0*a.y+w1*c0.y+w2*c1.y+w3*b.y)));
        }
        break;
    }

    case LinkStyle::Orthogonal:
    {
        // out of each pin by LinkStub; facing pins with room between them
        // turn once each at the middle of the run, any others go round
        // through the middle of the rise
        const PointF s0(a.x+outA*LinkStub, a.y);
        const PointF s1(b.x+outB*LinkStub, b.y);

        if((outA == -outB) && ((s1.x-s0.x)*outA >= 0.0f))
        {
            const float middle=0.5f*(a.x+b.x);

            add(PointF(middle, a.y));
            add(PointF(middle, b.y));
        }
        else
        {
            const float middle=0.5f*(a.y+b.y);

            add(s0);
            add(PointF(s0.x, middle));
            add(PointF(s1.x, middle));
            add(s1);
        }
        break;
    }
    }
    add(b);
}


///////////////////////////////////////////////////////////////////////////
// NodeGraph

// The layers fill the inner panel, each owner-placed at its top-left; the
// inner panel is placed GraphExtent up and left of the content's origin
// (arrange()), so the graph's origin is the panel's content origin.
GWENI_CONTROL_CONSTRUCTOR(NodeGraph)
{
    m_linkLayer=m_innerPanel->newChild<internal::NodeGraphLayer>();
    m_nodeLayer=m_innerPanel->newChild<internal::NodeGraphLayer>();
    m_overlayLayer=m_innerPanel->newChild<internal::NodeGraphLayer>();

    for(Base *layer : {static_cast<Base *>(m_linkLayer), static_cast<Base *>(m_nodeLayer),
        static_cast<Base *>(m_overlayLayer)})
        layer->setPlacement(Placement::Owner);
}

Node *NodeGraph::addNode(const String &title, const Point &position)
{
    Node *node=m_nodeLayer->newChild<Node>();

    node->setTitle(title);
    node->setGraphPosition(position);
    return node;
}

void NodeGraph::removeNode(Node *node)
{
    if(!node || (node->getParent() != m_nodeLayer))
        return;

    for(Link *link : node->getLinks())
        removeLink(link);
    node->delayedDelete();
}

Link *NodeGraph::addLink(Pin *from, Pin *to, LinkStyle style)
{
    if(!from || !to || (from == to) || !from->getNode() || !to->getNode()
        || (from->getNode()->getGraph() != this) || (to->getNode()->getGraph() != this))
        return nullptr;

    Link *link=m_linkLayer->newChild<Link>();

    link->m_from=from;
    link->m_to=to;
    link->m_style=style;
    from->m_links.push_back(link);
    to->m_links.push_back(link);
    from->setConnected(true);
    to->setConnected(true);
    return link;
}

void NodeGraph::removeLink(Link *link)
{
    if(!link || (link->getParent() != m_linkLayer))
        return;

    for(Pin *pin : {link->getFrom(), link->getTo()})
    {
        if(!pin)
            continue;
        link->detach(pin);
        pin->setConnected(!pin->m_links.empty());
    }
    link->delayedDelete();
}

std::vector<Link *> NodeGraph::getLinks() const
{
    std::vector<Link *> links;
    const Canvas *canvas=const_cast<NodeGraph *>(this)->getCanvas();

    for(Base *child : m_linkLayer->getChildren())
    {
        Link *link=gweni_cast<Link>(child);

        if(link && !(canvas && canvas->isPendingDelete(link)))
            links.push_back(link);
    }
    return links;
}

std::vector<Node *> NodeGraph::getNodes() const
{
    std::vector<Node *> nodes;
    const Canvas *canvas=const_cast<NodeGraph *>(this)->getCanvas();

    for(Base *child : m_nodeLayer->getChildren())
    {
        Node *node=gweni_cast<Node>(child);

        if(node && !(canvas && canvas->isPendingDelete(node)))
            nodes.push_back(node);
    }
    return nodes;
}

float NodeGraph::gridStep() const
{
    float step=float(GridSpacing);

    // a zoom no lower than MinZoomLimit takes at most a few steps up
    while(step*zoom() < MinGridGap)
        step*=float(GridMajorEvery);
    return step;
}

PointF NodeGraph::canvasToGraph(const PointF &canvas) const
{
    return toChildSpace(canvasPosToLocalF(canvas));
}

void NodeGraph::arrange(skin::Base *skin, Dim dim)
{
    const int side=2*GraphExtent;

    if(dim == Dim::X)
    {
        placeChild(m_innerPanel, -GraphExtent, -GraphExtent, side, m_innerPanel->getHeight());
        for(Base *layer : m_innerPanel->getChildren())
            placeChild(layer, 0, 0, side, layer->getHeight());
    }
    else
    {
        placeChild(m_innerPanel, -GraphExtent, -GraphExtent, m_innerPanel->getWidth(), side);
        for(Base *layer : m_innerPanel->getChildren())
            placeChild(layer, 0, 0, layer->getWidth(), side);
    }

    // ZoomPanel's arrange would place the inner panel at the content's origin
    Base::arrange(skin, dim);
}

void NodeGraph::spaceChanged(bool zoomed)
{
    ParentClass::spaceChanged(zoomed);
    addStateChange(StateChange_Background);
}

}//namespace controls
}//namespace gweni
