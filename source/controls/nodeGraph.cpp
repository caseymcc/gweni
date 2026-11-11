/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */


#include <gweni/controls/nodeGraph.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/gweni.h>
#include <gweni/inputHandler.h>

#include <algorithm>
#include <cmath>
#include <limits>

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

// The bar's node is its parent, and the node's graph does the moving, so a
// drag moves the whole selection.
void NodeTitleBar::onMouseClickLeft(int x, int y, bool down)
{
    Node *node=gweni_cast<Node>(getActualParent());
    NodeGraph *graph=node?node->getGraph():nullptr;

    // Disabled refuses a press only: a release always ends the drag, so a bar
    // disabled mid-drag does not leave it stuck.
    if(down)
    {
        if(!graph || isDisabled())
            return;

        m_pressed=true;
        gweni::MouseFocus=this;
        graph->beginNodeDrag(node, Point(x, y));
    }
    else if(m_pressed)
    {
        m_pressed=false;
        if(gweni::MouseFocus == this)
            gweni::MouseFocus=nullptr;
        if(graph)
            graph->endNodeDrag();
    }
}

void NodeTitleBar::onMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/)
{
    if(!m_pressed)
        return;

    Node *node=gweni_cast<Node>(getActualParent());

    if(NodeGraph *graph=node?node->getGraph():nullptr)
        graph->dragNodes(Point(x, y));
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

void NodeBody::onMouseClickLeft(int /*x*/, int /*y*/, bool down)
{
    Node *node=gweni_cast<Node>(getActualParent());
    NodeGraph *graph=node?node->getGraph():nullptr;

    if(down && graph && !isDisabled())
        graph->pressNode(node);
}

// The whole overlay layer, at its top-left, as a Link covers its own: a new
// rect draws again with nothing laid out.
GWENI_CONTROL_CONSTRUCTOR(NodeGraphMarquee)
{
    setPlacement(Placement::Owner);
    setMouseInputEnabled(false);
    setBounds(0, 0, 2*NodeGraph::GraphExtent, 2*NodeGraph::GraphExtent);
}

GWENI_CONTROL_CONSTRUCTOR(NodeGraphMarqueeFill)
{
}

void NodeGraphMarquee::setRect(const Rect &rect)
{
    if(m_rect == rect)
        return;

    m_rect=rect;
    addStateChange(StateChange_Background);
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
    // the graph found by its type alone: the node may be going already, and
    // a graph that is going is no NodeGraph any more
    for(Base *parent=getParent(); parent; parent=parent->getParent())
    {
        if(NodeGraph *graph=gweni_cast<NodeGraph>(parent))
        {
            graph->forgetPin(this);
            break;
        }
    }

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

InputCapture *Pin::pressGesture(int button)
{
    // Space and a left drag pan the graph, as anywhere on it
    if((button != 0) || isDisabled() || input::isKeyDown(keys::Space) || !m_node)
        return nullptr;

    NodeGraph *graph=m_node->getGraph();

    return graph?graph->beginLinkDrag(this):nullptr;
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

// Out of its graph's drag; a graph being deleted is no NodeGraph any more
// (getGraph() is null), and has nothing to drop.
Node::~Node()
{
    if(NodeGraph *graph=getGraph())
        graph->forgetNode(this);
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

void Node::getGraphFrame(PointF &min, PointF &max) const
{
    const Point position=getGraphPosition();
    const Rect frame=getFrameRect();

    min=PointF(float(position.x+frame.x), float(position.y+frame.y));
    max=PointF(min.x+float(frame.w), min.y+float(frame.h));
}

void Node::onMouseClickLeft(int /*x*/, int /*y*/, bool down)
{
    NodeGraph *graph=getGraph();

    if(down && graph && !isDisabled())
        graph->pressNode(this);
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
    // hit near its curve only (getControlAt())
    setMouseInputEnabled(true);
    setBounds(0, 0, 2*NodeGraph::GraphExtent, 2*NodeGraph::GraphExtent);
}

// Deleted directly, not through removeLink(): its pins are still there.
Link::~Link()
{
    for(Pin *pin : {m_from, m_to})
    {
        if(!pin)
            continue;
        detach(pin);
        pin->setConnected(!pin->m_links.empty());
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
    m_pointsValid=false;
    addStateChange(StateChange_Background);
}

void Link::setStyle(LinkStyle style)
{
    if(m_style == style)
        return;

    m_style=style;
    m_pointsValid=false;
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
    m_pointsValid=false;
    addStateChange(StateChange_Background);
}

void Link::setFreeEnd(const PointF &point)
{
    if(m_freeEnd == point)
        return;

    m_freeEnd=point;
    endpointsMoved();
}

// The link in its layer, the layer in the inner panel, the inner panel in
// the graph's content, whose origin is the graph's
PointF Link::graphOrigin() const
{
    PointF origin(0.0f, 0.0f);
    const NodeGraph *graph=getGraph();

    for(const Base *control=this; control && (control != graph); control=control->getActualParent())
    {
        origin.x+=float(control->getX());
        origin.y+=float(control->getY());
    }
    return origin;
}

// The nearest point of each segment, as the line style measures it
float Link::distanceTo(const PointF &point) const
{
    const std::vector<PointF> &points=polyline();

    float nearest=std::numeric_limits<float>::max();

    for(size_t i=0; i+1 < points.size(); ++i)
    {
        const PointF &a=points[i];
        const PointF &b=points[i+1];
        const float dx=b.x-a.x;
        const float dy=b.y-a.y;
        const float length=dx*dx+dy*dy;
        float t=(length > 0.0f)?((point.x-a.x)*dx+(point.y-a.y)*dy)/length:0.0f;

        t=std::min(std::max(t, 0.0f), 1.0f);
        nearest=std::min(nearest, std::hypot(a.x+dx*t-point.x, a.y+dy*t-point.y));
    }
    return nearest;
}

// A segment meets the rect where the part of it between the rect's sides on
// both axes is not empty (Liang-Barsky)
bool Link::touches(const PointF &min, const PointF &max) const
{
    const std::vector<PointF> &points=polyline();
    for(size_t i=0; i+1 < points.size(); ++i)
    {
        const PointF &a=points[i];
        const PointF &b=points[i+1];
        const float d[2]={b.x-a.x, b.y-a.y};
        const float start[2]={a.x, a.y};
        const float lo[2]={min.x, min.y};
        const float hi[2]={max.x, max.y};
        float t0=0.0f;
        float t1=1.0f;
        bool inside=true;

        for(int axis=0; (axis < 2) && inside; ++axis)
        {
            if(d[axis] == 0.0f)
            {
                inside=(start[axis] >= lo[axis]) && (start[axis] <= hi[axis]);
                continue;
            }

            float enter=(lo[axis]-start[axis])/d[axis];
            float leave=(hi[axis]-start[axis])/d[axis];

            if(enter > leave)
                std::swap(enter, leave);
            t0=std::max(t0, enter);
            t1=std::min(t1, leave);
            inside=t0 <= t1;
        }
        if(inside)
            return true;
    }
    return false;
}

void Link::onMouseClickLeft(int /*x*/, int /*y*/, bool down)
{
    NodeGraph *graph=getGraph();

    if(down && graph && !m_preview && !isDisabled())
        graph->pressLink(this);
}

Base *Link::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    countHitTestVisit();
    if(hidden() || m_preview || (bOnlyIfMouseEnabled && !getMouseInputEnabled()))
        return nullptr;

    const NodeGraph *graph=getGraph();

    if(!graph)
        return nullptr;

    // the graph's exact point while it walks; called on its own, the middle
    // of the graph unit the point is in
    PointF point=graph->m_hitPoint;

    if(!graph->m_hitTesting)
    {
        const PointF origin=graphOrigin();

        point=PointF(float(x)+0.5f+origin.x, float(y)+0.5f+origin.y);
    }
    return (distanceTo(point)*graph->zoom() <= HoverDistance)?this:nullptr;
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

// A pin's end is its anchor, leaving it along its side's way out; a
// preview's free end is approached as an opposite pin would be.
const std::vector<PointF> &Link::polyline() const
{
    std::vector<PointF> &points=m_points;

    const Pin *pinA=m_from?m_from:m_to;
    const Pin *pinB=m_from?m_to:nullptr;

    if(!pinA || (!pinB && !m_preview))
    {
        points.clear();
        m_pointsValid=false;
        return points;
    }

    const PointF a=pinA->getAnchor();
    const PointF b=pinB?pinB->getAnchor():m_freeEnd;
    // each end's way out of its pin
    const float outA=pinA->outward();
    const float outB=pinB?pinB->outward():-outA;
    const NodeGraph *graph=getGraph();
    const float zoom=graph?graph->zoom():1.0f;

    if(m_pointsValid && (m_pointsFrom == a) && (m_pointsTo == b) && (m_pointsZoom == zoom))
        return points;

    points.clear();

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

    m_pointsValid=true;
    m_pointsFrom=a;
    m_pointsTo=b;
    m_pointsZoom=zoom;
    return points;
}

void Link::getPoints(std::vector<PointF> &points) const
{
    points=polyline();
}


///////////////////////////////////////////////////////////////////////////
// NodeGraph

// A link dragged out of a pin, from the press to its release, with the
// canvas's input: the preview, a Link in the overlay layer, follows the
// mouse; the release drops it (NodeGraph::dropLink()), Escape ends it with
// nothing made, and the wheel zooms about the mouse as over the graph.
class NodeGraph::LinkDrag: public InputCapture
{
public:

    explicit LinkDrag(NodeGraph *graph): m_graph(graph) {}

    ~LinkDrag() override { end(); }

    void start(Canvas *canvas, Pin *pin)
    {
        m_canvas=canvas;
        m_pin=pin;
        m_mouse=input::getMousePosition();

        Link *preview=m_graph->m_overlayLayer->newChild<Link>();

        preview->m_from=pin;
        preview->m_preview=true;
        preview->m_style=m_graph->m_newLinkStyle;
        preview->setMouseInputEnabled(false);
        preview->setFreeEnd(m_graph->canvasToGraph(PointF(m_mouse)));
        m_preview=preview;
    }

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }
    Link *preview() const { return m_preview; }

    void captureMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/) override
    {
        m_mouse=Point(x, y);
        follow();
    }

    void captureMouseButton(int button, bool down) override
    {
        if((button != 0) || down)
            return;

        Canvas *canvas=m_canvas;
        Pin *pin=m_pin;
        const Point mouse=m_mouse;
        const Point at=input::getMousePosition();

        end();
        if(pin)
            m_graph->dropLink(pin, mouse);
        // the hover follows the mouse again, from where it was at the press
        if(canvas && ((at.x != mouse.x) || (at.y != mouse.y)))
            canvas->inputMouseMoved(mouse.x, mouse.y, mouse.x-at.x, mouse.y-at.y);
    }

    void captureMouseWheel(int delta) override
    {
        if(m_graph->isDisabled())
            return;

        m_graph->wheelAt(delta, m_mouse);
        follow();
    }

    void captureKey(int key, bool down) override
    {
        if(down && (key == keys::Escape))
            end();
    }

    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

    //! The graph is going: the canvas's input back, the preview going with
    //! the overlay layer.
    void abandon()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
        m_pin=nullptr;
        // the pin may go before it, and never had it as a link
        if(m_preview)
            m_preview->m_from=nullptr;
        m_preview=nullptr;
    }

    //! A pin deleted mid-drag ends it.
    void forget(Pin *pin)
    {
        if(pin == m_pin)
            end();
    }

    //! Gives the canvas its input back and the preview's ids.
    void end()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
        m_pin=nullptr;
        if(m_preview)
        {
            // it never was the pin's link
            m_preview->m_from=nullptr;
            m_preview->delayedDelete();
            m_preview=nullptr;
        }
    }

private:

    void follow()
    {
        if(m_preview)
            m_preview->setFreeEnd(m_graph->canvasToGraph(PointF(m_mouse)));
    }

    NodeGraph *m_graph;
    Canvas *m_canvas=nullptr;
    Pin *m_pin=nullptr;
    Link *m_preview=nullptr;
    Point m_mouse;
};

// A left drag on the graph's empty space, from the press to its release, with
// the canvas's input. Once the mouse is PanThreshold units from the press it
// draws the marquee (a NodeGraphMarquee in the overlay layer, made then) and
// every move selects the nodes and links it touches, with the selection from
// before the press kept under a Ctrl or Shift press; a release that never got
// that far is a click on empty space, which clears the selection (but under
// Ctrl or Shift). Escape gives back the selection from before the press.
class NodeGraph::MarqueeDrag: public InputCapture
{
public:

    explicit MarqueeDrag(NodeGraph *graph): m_graph(graph) {}

    ~MarqueeDrag() override { end(); }

    void start(Canvas *canvas)
    {
        m_canvas=canvas;
        m_press=input::getMousePosition();
        m_mouse=m_press;
        m_from=m_graph->canvasToGraph(PointF(m_press));
        m_additive=input::isControlDown() || input::isShiftDown();
        m_moved=false;
        m_graph->snapshotSelection();
    }

    bool active() const { return m_canvas && (m_canvas->inputCapture() == this); }
    bool moved() const { return m_moved; }
    const PointF &min() const { return m_min; }
    const PointF &max() const { return m_max; }

    void captureMouseMoved(int x, int y, int /*deltaX*/, int /*deltaY*/) override
    {
        m_mouse=Point(x, y);
        if(!m_moved && (std::abs(x-m_press.x) < PanThreshold) && (std::abs(y-m_press.y) < PanThreshold))
            return;

        m_moved=true;
        update();
    }

    void captureMouseButton(int button, bool down) override
    {
        if((button != 0) || down)
            return;

        Canvas *canvas=m_canvas;
        const bool moved=m_moved;
        const bool additive=m_additive;
        const Point mouse=m_mouse;
        const Point at=input::getMousePosition();

        end();
        // a click on empty space
        if(!moved && !additive)
            m_graph->clearSelection();
        if(m_graph->selectionChangedSinceSnapshot())
            m_graph->onSelectionChanged.call(m_graph);
        if(canvas && ((at.x != mouse.x) || (at.y != mouse.y)))
            canvas->inputMouseMoved(mouse.x, mouse.y, mouse.x-at.x, mouse.y-at.y);
    }

    void captureMouseWheel(int delta) override
    {
        if(m_graph->isDisabled())
            return;

        // the press stays at its graph point, the mouse's moves with the zoom
        m_graph->wheelAt(delta, m_mouse);
        if(m_moved)
            update();
    }

    void captureKey(int key, bool down) override
    {
        if(down && (key == keys::Escape))
        {
            end();
            m_graph->restoreSnapshot();
        }
    }

    void captureCanvasDeleted(Canvas * /*canvas*/) override { m_canvas=nullptr; }

    //! Gives the canvas its input back and hides the marquee.
    void end()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
        for(internal::NodeGraphMarquee *part : {static_cast<internal::NodeGraphMarquee *>(m_graph->m_marqueeOutline),
            static_cast<internal::NodeGraphMarquee *>(m_graph->m_marqueeFill)})
        {
            part->setRect(Rect());
            part->hide();
        }
    }

    //! The graph is going: its input back, the marquee going with the layers.
    void abandon()
    {
        if(active())
            m_canvas->setInputCapture(nullptr);
        m_canvas=nullptr;
    }

private:

    // The rect from the press to the mouse, drawn on whole graph units, and
    // the selection it makes: what it touches, and what was selected before
    // under Ctrl or Shift. Only an item whose state changes is marked.
    void update()
    {
        const PointF to=m_graph->canvasToGraph(PointF(m_mouse));

        m_min=PointF(std::min(m_from.x, to.x), std::min(m_from.y, to.y));
        m_max=PointF(std::max(m_from.x, to.x), std::max(m_from.y, to.y));

        internal::NodeGraphMarquee *outline=m_graph->m_marqueeOutline;
        internal::NodeGraphMarqueeFill *fill=m_graph->m_marqueeFill;

        outline->show();
        fill->show();

        // the marquee's own units are the graph's, GraphExtent from its origin
        const int x0=int(std::floor(m_min.x))+GraphExtent;
        const int y0=int(std::floor(m_min.y))+GraphExtent;
        const int x1=int(std::ceil(m_max.x))+GraphExtent;
        const int y1=int(std::ceil(m_max.y))+GraphExtent;

        outline->setRect(Rect(x0, y0, x1-x0, y1-y0));
        fill->setRect(Rect(x0, y0, x1-x0, y1-y0));

        for(Base *child : m_graph->m_nodeLayer->getChildren())
        {
            Node *node=gweni_cast<Node>(child);

            if(!node || m_graph->getCanvas()->isPendingDelete(node))
                continue;

            PointF frameMin, frameMax;

            node->getGraphFrame(frameMin, frameMax);

            const bool touched=(frameMin.x <= m_max.x) && (frameMax.x >= m_min.x)
                && (frameMin.y <= m_max.y) && (frameMax.y >= m_min.y);

            m_graph->selectNode(node, touched || (m_additive && node->m_selectedBefore));
        }
        for(Base *child : m_graph->m_linkLayer->getChildren())
        {
            Link *link=gweni_cast<Link>(child);

            if(!link || m_graph->getCanvas()->isPendingDelete(link))
                continue;
            m_graph->selectLink(link, link->touches(m_min, m_max) || (m_additive && link->m_selectedBefore));
        }
    }

    NodeGraph *m_graph;
    Canvas *m_canvas=nullptr;
    Point m_press;
    Point m_mouse;
    PointF m_from;
    PointF m_min;
    PointF m_max;
    bool m_additive=false;
    bool m_moved=false;
};

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

    // The marquee's parts, made now and hidden until a marquee shows them: the
    // fill first in the link layer, under every link, and the outline in the
    // overlay layer. Made later, the fill would be a control put in among the
    // links, whose z the canvas makes room for by relabelling the nodes after
    // it once the gaps are spent (a large graph).
    m_marqueeFill=m_linkLayer->newChild<internal::NodeGraphMarqueeFill>();
    m_marqueeOutline=m_overlayLayer->newChild<internal::NodeGraphMarquee>();
    m_marqueeFill->hide();
    m_marqueeOutline->hide();

    m_linkDrag=std::make_unique<LinkDrag>(this);
    m_marquee=std::make_unique<MarqueeDrag>(this);

    // a press on it takes the keys (Delete), unless a control under it does
    setKeyboardInputEnabled(true);
    setTabable(false);
}

// The drag's preview goes with the overlay layer, after this; the drag gives
// the canvas's input back first.
NodeGraph::~NodeGraph()
{
    if(m_linkDrag)
        m_linkDrag->abandon();
    if(m_marquee)
        m_marquee->abandon();
}

InputCapture *NodeGraph::pressGesture(int button)
{
    Canvas *canvas=getCanvas();

    // where a context menu opened by this press places what it adds; the
    // ZoomPanel gives an unmoved right press back as a click, which opens it
    if(button == 1)
        m_contextPosition=canvasToGraph(PointF(input::getMousePosition()));

    if((button == 0) && canvas && !isDisabled() && !input::isKeyDown(keys::Space) && !m_marquee->active()
        && !m_linkDrag->active() && emptyAt(input::getMousePosition()))
    {
        m_marquee->start(canvas);
        // the press goes to the capture, not to the canvas's focus search
        focus();
        return m_marquee.get();
    }
    return ParentClass::pressGesture(button);
}

void NodeGraph::removeSelection()
{
    snapshotSelection();

    for(Node *node : getSelectedNodes())
    {
        m_removeVetoed=false;
        onNodeRemoveRequested.call(this, event::Info(node));
        if(m_removeVetoed)
            continue;

        const std::vector<Link *> links=node->getLinks();

        removeNode(node);
        for(Link *link : links)
            onLinkRemoved.call(this, event::Info(link));
        onNodeRemoved.call(this, event::Info(node));
    }
    for(Link *link : getSelectedLinks())
    {
        m_removeVetoed=false;
        onLinkRemoveRequested.call(this, event::Info(link));
        if(m_removeVetoed)
            continue;

        removeLink(link);
        onLinkRemoved.call(this, event::Info(link));
    }
    m_removeVetoed=false;

    // what went is no longer in the selection
    bool changed=false;

    for(Base *child : m_nodeLayer->getChildren())
    {
        Node *node=gweni_cast<Node>(child);

        if(node && node->m_selectedBefore && getCanvas() && getCanvas()->isPendingDelete(node))
            changed=true;
    }
    for(Base *child : m_linkLayer->getChildren())
    {
        Link *link=gweni_cast<Link>(child);

        if(link && link->m_selectedBefore && getCanvas() && getCanvas()->isPendingDelete(link))
            changed=true;
    }
    if(changed)
        onSelectionChanged.call(this);
}

bool NodeGraph::onKeyDelete(bool down)
{
    if(down && !isDisabled() && !m_linkDrag->active() && !m_marquee->active() && m_dragged.empty())
        removeSelection();
    return true;
}

void NodeGraph::openContextMenuAt(const PointF &at)
{
    Canvas *canvas=getCanvas();

    if(!canvas || !getContextMenu())
        return;

    m_contextPosition=at;

    const PointF local=graphToLocal(at);

    canvas->showContextMenu(this, localPosToCanvas(Point(int(std::floor(local.x)), int(std::floor(local.y)))));
}

bool NodeGraph::marqueeActive() const
{
    return m_marquee->active();
}

void NodeGraph::getMarqueeRect(PointF &min, PointF &max) const
{
    if(m_marquee->active() && m_marquee->moved())
    {
        min=m_marquee->min();
        max=m_marquee->max();
    }
    else
        min=max=PointF(0.0f, 0.0f);
}

void NodeGraph::setLinkFilter(std::function<bool(Pin *from, Pin *to)> filter)
{
    m_linkFilter=std::move(filter);
}

bool NodeGraph::canLink(Pin *a, Pin *b, Pin **from, Pin **to) const
{
    if(!a || !b || (a == b) || !a->getNode() || !b->getNode() || (a->getNode()->getGraph() != this)
        || (b->getNode()->getGraph() != this))
        return false;

    // the Output first; two of one direction in the order dragged
    Pin *first=a;
    Pin *second=b;

    if((a->getDirection() == PinDirection::Input) && (b->getDirection() == PinDirection::Output))
        std::swap(first, second);

    const bool allowed=m_linkFilter?m_linkFilter(first, second)
        :((first->getDirection() != second->getDirection()) && (first->getNode() != second->getNode()));

    if(allowed)
    {
        if(from)
            *from=first;
        if(to)
            *to=second;
    }
    return allowed;
}

bool NodeGraph::draggingLink() const
{
    return m_linkDrag->active();
}

Link *NodeGraph::getPreviewLink() const
{
    return m_linkDrag->active()?m_linkDrag->preview():nullptr;
}

void NodeGraph::cancelLinkDrag()
{
    m_linkDrag->end();
}

void NodeGraph::forgetPin(Pin *pin)
{
    m_linkDrag->forget(pin);
}

InputCapture *NodeGraph::beginLinkDrag(Pin *pin)
{
    Canvas *canvas=getCanvas();

    if(!canvas || isDisabled() || m_linkDrag->active() || !pin->getNode() || (pin->getNode()->getGraph() != this))
        return nullptr;

    m_linkDrag->start(canvas, pin);
    return m_linkDrag.get();
}

Pin *NodeGraph::pinAt(const Point &at)
{
    Canvas *canvas=getCanvas();

    for(Base *control=canvas?canvas->getControlAt(at.x, at.y):nullptr; control; control=control->getParent())
    {
        if(Pin *pin=gweni_cast<Pin>(control))
            return (pin->getNode() && (pin->getNode()->getGraph() == this))?pin:nullptr;
        if(control == this)
            break;
    }
    return nullptr;
}

bool NodeGraph::emptyAt(const Point &at)
{
    Canvas *canvas=getCanvas();

    for(Base *control=canvas?canvas->getControlAt(at.x, at.y):nullptr; control; control=control->getParent())
    {
        if(gweni_cast<Node>(control) || gweni_cast<Link>(control))
            return false;
        if(control == this)
            return true;
    }
    return false;
}

void NodeGraph::dropLink(Pin *pin, const Point &at)
{
    if(Pin *target=pinAt(at))
    {
        Pin *from=nullptr;
        Pin *to=nullptr;

        if(!canLink(pin, target, &from, &to))
            return;

        m_linkVetoed=false;
        m_requestedFrom=from;
        m_requestedTo=to;
        onLinkRequested.call(this);
        m_requestedFrom=nullptr;
        m_requestedTo=nullptr;
        if(m_linkVetoed)
            return;

        if(Link *link=addLink(from, to, m_newLinkStyle))
            onLinkCreated.call(this, event::Info(link));
        return;
    }

    if(!emptyAt(at))
        return;

    m_dropPosition=canvasToGraph(PointF(at));
    m_dropPin=pin;

    event::Info info(this);

    info.point=Point(int(std::lround(m_dropPosition.x)), int(std::lround(m_dropPosition.y)));
    onLinkDroppedOnEmpty.call(this, info);
    m_dropPin=nullptr;
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

void NodeGraph::selectNode(Node *node, bool selected)
{
    if(!node || (node->getParent() != m_nodeLayer) || (node->m_selected == selected))
        return;

    node->m_selected=selected;
    node->addStateChange(StateChange_Background);
}

void NodeGraph::selectLink(Link *link, bool selected)
{
    if(!link || (link->getParent() != m_linkLayer) || (link->m_selected == selected))
        return;

    link->m_selected=selected;
    link->addStateChange(StateChange_Background);
}

void NodeGraph::selectOnly(Node *node)
{
    for(Node *other : getSelectedNodes())
    {
        if(other != node)
            selectNode(other, false);
    }
    for(Link *link : getSelectedLinks())
        selectLink(link, false);
    selectNode(node, true);
}

void NodeGraph::selectOnly(Link *link)
{
    for(Node *node : getSelectedNodes())
        selectNode(node, false);
    for(Link *other : getSelectedLinks())
    {
        if(other != link)
            selectLink(other, false);
    }
    selectLink(link, true);
}

void NodeGraph::clearSelection()
{
    for(Node *node : getSelectedNodes())
        selectNode(node, false);
    for(Link *link : getSelectedLinks())
        selectLink(link, false);
}

std::vector<Link *> NodeGraph::getSelectedLinks() const
{
    std::vector<Link *> selected;

    for(Link *link : getLinks())
    {
        if(link->isSelected())
            selected.push_back(link);
    }
    return selected;
}

// Walks the layers' children themselves, with nothing allocated: a marquee
// frame takes a snapshot only at its press, but compares at its end.
void NodeGraph::snapshotSelection()
{
    for(Base *child : m_nodeLayer->getChildren())
    {
        if(Node *node=gweni_cast<Node>(child))
            node->m_selectedBefore=node->m_selected;
    }
    for(Base *child : m_linkLayer->getChildren())
    {
        if(Link *link=gweni_cast<Link>(child))
            link->m_selectedBefore=link->m_selected;
    }
}

bool NodeGraph::selectionChangedSinceSnapshot() const
{
    for(Base *child : m_nodeLayer->getChildren())
    {
        Node *node=gweni_cast<Node>(child);

        if(node && (node->m_selectedBefore != node->m_selected))
            return true;
    }
    for(Base *child : m_linkLayer->getChildren())
    {
        Link *link=gweni_cast<Link>(child);

        if(link && (link->m_selectedBefore != link->m_selected))
            return true;
    }
    return false;
}

void NodeGraph::restoreSnapshot()
{
    for(Base *child : m_nodeLayer->getChildren())
    {
        if(Node *node=gweni_cast<Node>(child))
            selectNode(node, node->m_selectedBefore);
    }
    for(Base *child : m_linkLayer->getChildren())
    {
        if(Link *link=gweni_cast<Link>(child))
            selectLink(link, link->m_selectedBefore);
    }
}

void NodeGraph::pressNode(Node *node)
{
    snapshotSelection();
    if(input::isControlDown())
        selectNode(node, !node->isSelected());
    else if(input::isShiftDown())
        selectNode(node, true);
    else
        selectOnly(node);
    node->bringToFront();
    if(selectionChangedSinceSnapshot())
        onSelectionChanged.call(this);
}

void NodeGraph::pressLink(Link *link)
{
    snapshotSelection();
    if(input::isControlDown())
        selectLink(link, !link->isSelected());
    else if(input::isShiftDown())
        selectLink(link, true);
    else
        selectOnly(link);
    if(selectionChangedSinceSnapshot())
        onSelectionChanged.call(this);
}

std::vector<Node *> NodeGraph::getSelectedNodes() const
{
    std::vector<Node *> selected;

    for(Node *node : getNodes())
    {
        if(node->isSelected())
            selected.push_back(node);
    }
    return selected;
}

// Ctrl toggles the node, Shift adds it, and a plain press keeps a selected
// node's selection so it moves whole (a click that does not move makes it the
// one selected node at the release) or makes an unselected one the only one.
// A node Ctrl takes off is not dragged.
void NodeGraph::beginNodeDrag(Node *node, const Point &at)
{
    snapshotSelection();
    m_pressedNode=node;
    m_pressedWasSelected=node->isSelected();
    m_pressModified=input::isControlDown() || input::isShiftDown();

    if(input::isControlDown())
        selectNode(node, !node->isSelected());
    else if(input::isShiftDown())
        selectNode(node, true);
    else if(!node->isSelected())
        selectOnly(node);

    // only the pressed node's subtree takes new z
    node->bringToFront();

    m_dragged.clear();
    m_moved.clear();
    m_dragMoved=false;
    if(node->isSelected())
    {
        for(Node *selected : getSelectedNodes())
            m_dragged.push_back({selected, selected->getGraphPosition()});
    }
    m_dragFrom=canvasToGraph(PointF(at));
}

void NodeGraph::dragNodes(const Point &at)
{
    if(m_dragged.empty())
        return;

    const PointF to=canvasToGraph(PointF(at));
    const Point delta(int(std::lround(to.x-m_dragFrom.x)), int(std::lround(to.y-m_dragFrom.y)));

    for(const DraggedNode &dragged : m_dragged)
    {
        const Point position(dragged.start.x+delta.x, dragged.start.y+delta.y);
        const Point now=dragged.node->getGraphPosition();

        if((now.x == position.x) && (now.y == position.y))
            continue;

        dragged.node->setGraphPosition(position);
        m_dragMoved=true;
    }
}

void NodeGraph::endNodeDrag()
{
    const bool moved=m_dragMoved;

    // a plain click on a node of a selection makes it the selection
    if(!moved && m_pressedNode && m_pressedWasSelected && !m_pressModified)
        selectOnly(m_pressedNode);
    m_pressedNode=nullptr;

    m_moved.clear();
    for(const DraggedNode &dragged : m_dragged)
        m_moved.push_back(dragged.node);
    m_dragged.clear();
    m_dragMoved=false;

    if(moved)
        onNodesMoved.call(this);
    if(selectionChangedSinceSnapshot())
        onSelectionChanged.call(this);
}

void NodeGraph::forgetNode(Node *node)
{
    if(m_pressedNode == node)
        m_pressedNode=nullptr;
    m_dragged.erase(std::remove_if(m_dragged.begin(), m_dragged.end(),
        [node](const DraggedNode &dragged) { return dragged.node == node; }), m_dragged.end());
    m_moved.erase(std::remove(m_moved.begin(), m_moved.end(), node), m_moved.end());
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

Base *NodeGraph::getControlAt(int x, int y, bool bOnlyIfMouseEnabled)
{
    m_hitPoint=toChildSpace(PointF(float(x)+0.5f, float(y)+0.5f));
    m_hitTesting=true;

    Base *found=ParentClass::getControlAt(x, y, bOnlyIfMouseEnabled);

    m_hitTesting=false;
    return found;
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
