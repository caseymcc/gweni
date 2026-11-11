#include "demo/nodeGraph.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/label.h"
#include "gweni/controls/menu.h"
#include "gweni/controls/menuItem.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/textBox.h"

#include <cmath>

namespace gweni
{

namespace
{

// a colour per kind of value, as a shader editor has them
const Color FloatPin(0xa1, 0xa1, 0xa1);
const Color VectorPin(0x63, 0x63, 0xc7);
const Color ColorPin(0xc7, 0xc7, 0x29);
const Color ShaderPin(0x63, 0xc7, 0x63);

controls::Pin *addPin(controls::Node *node, controls::PinDirection direction, const String &name,
    const Color &color, controls::PinShape shape)
{
    controls::Pin *pin=node->addPin(direction, name);

    pin->setColor(color);
    pin->setShape(shape);
    return pin;
}

controls::DragNumber *addNumber(controls::Node *node, double value, double min, double max)
{
    controls::DragNumber *number=node->newChild<controls::DragNumber>();

    number->setDock(DockPosition::Top);
    number->setHeight(20);
    number->setRange(min, max);
    number->setFormat("%.2f");
    number->setNumber(value, false);
    number->setMargin(Margin(0, 2, 0, 2));
    return number;
}

}//namespace

// A line saying what the input does, then the graph filling the rest.
GWENI_CONTROL_CONSTRUCTOR(NodeGraphDemo)
{
    setDock(DockPosition::Center);

    controls::Label *hint=newChild<controls::Label>();
    hint->setDock(DockPosition::Top);
    hint->setText("Wheel: zoom. Middle/right drag: pan. Drag a pin to link, a title to move, empty space"
        " to select. Right-click for menus, Delete removes.");

    m_graph=newChild<controls::NodeGraph>();
    m_graph->setDock(DockPosition::Center);
    m_graph->setPan(PointF(10.0f, 10.0f));

    controls::Node *coord=m_graph->addNode("Texture Coordinate", Point(10, 40));
    coord->setTitleColor(Color(0x8a, 0x3a, 0x3a));
    coord->setToolTipText("The surface's UV coordinates");
    controls::Pin *uv=addPin(coord, controls::PinDirection::Output, "UV", VectorPin, controls::PinShape::Square);
    addPin(coord, controls::PinDirection::Output, "Normal", VectorPin, controls::PinShape::Square);
    controls::Label *note=coord->newChild<controls::Label>();
    note->setDock(DockPosition::Top);
    note->setText("From the active UV map");

    controls::Node *noise=m_graph->addNode("Noise Texture", Point(220, 10));
    noise->setTitleColor(Color(0x7a, 0x4a, 0x2a));
    controls::Pin *vector=addPin(noise, controls::PinDirection::Input, "Vector", VectorPin,
        controls::PinShape::Square);
    addPin(noise, controls::PinDirection::Input, "Scale", FloatPin, controls::PinShape::Circle);
    controls::Pin *noiseFac=addPin(noise, controls::PinDirection::Output, "Fac", FloatPin,
        controls::PinShape::Circle);
    addPin(noise, controls::PinDirection::Output, "Color", ColorPin, controls::PinShape::Circle);
    addNumber(noise, 5.0, 0.0, 100.0);
    addNumber(noise, 2.0, 0.0, 16.0);

    controls::Node *ramp=m_graph->addNode("Color Ramp", Point(430, 220));
    ramp->setTitleColor(Color(0x3a, 0x5a, 0x7a));
    controls::Pin *rampFac=addPin(ramp, controls::PinDirection::Input, "Fac", FloatPin,
        controls::PinShape::Circle);
    addPin(ramp, controls::PinDirection::Output, "Color", ColorPin, controls::PinShape::Circle);
    controls::Pin *alpha=addPin(ramp, controls::PinDirection::Output, "Alpha", FloatPin,
        controls::PinShape::Circle);
    controls::ComboBox *interpolation=ramp->newChild<controls::ComboBox>();
    interpolation->setDock(DockPosition::Top);
    interpolation->setHeight(20);
    interpolation->addItem("Linear");
    interpolation->addItem("Ease");
    interpolation->addItem("Constant");

    controls::Node *mix=m_graph->addNode("Mix Shader", Point(650, 10));
    mix->setTitleColor(Color(0x2a, 0x6a, 0x4a));
    controls::Pin *mixFac=addPin(mix, controls::PinDirection::Input, "Fac", FloatPin, controls::PinShape::Circle);
    addPin(mix, controls::PinDirection::Input, "Shader", ShaderPin, controls::PinShape::Triangle);
    addPin(mix, controls::PinDirection::Input, "Shader", ShaderPin, controls::PinShape::Triangle);
    controls::Pin *shader=addPin(mix, controls::PinDirection::Output, "Shader", ShaderPin,
        controls::PinShape::Triangle);
    addNumber(mix, 0.5, 0.0, 1.0);

    controls::Node *output=m_graph->addNode("Material Output", Point(650, 260));
    output->setTitleColor(Color(0x6a, 0x2a, 0x5a));
    output->setToolTipText("What the renderer shades the surface with");
    controls::Pin *surface=addPin(output, controls::PinDirection::Input, "Surface", ShaderPin,
        controls::PinShape::Triangle);
    addPin(output, controls::PinDirection::Input, "Volume", ShaderPin, controls::PinShape::Triangle);
    controls::TextBox *materialName=output->newChild<controls::TextBox>();
    materialName->setDock(DockPosition::Top);
    materialName->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    materialName->setSize(60, 20);
    materialName->setText("Surface");

    // one link of each style
    m_graph->addLink(uv, vector, controls::LinkStyle::Orthogonal);
    m_graph->addLink(noiseFac, rampFac, controls::LinkStyle::Bezier);
    m_graph->addLink(alpha, mixFac, controls::LinkStyle::Straight);
    m_graph->addLink(shader, surface, controls::LinkStyle::Bezier);

    // the menus
    if(controls::Menu *menu=m_graph->createContextMenu())
    {
        menu->addItem("Add Value")->setAction(this, &NodeGraphDemo::onAddValue);
        menu->addItem("Add Math")->setAction(this, &NodeGraphDemo::onAddMath);
        menu->addItem("Add Mix")->setAction(this, &NodeGraphDemo::onAddMix);
    }
    m_graph->onContextMenu.add(this, &NodeGraphDemo::onGraphMenu);
    for(controls::Node *node : m_graph->getNodes())
        giveNodeMenu(node);
    for(controls::Link *link : m_graph->getLinks())
        giveLinkMenu(link);
    m_graph->onLinkCreated.add(this, &NodeGraphDemo::onLinkCreated);
    m_graph->onLinkDroppedOnEmpty.add(this, &NodeGraphDemo::onDroppedOnEmpty);
}

void NodeGraphDemo::giveNodeMenu(controls::Node *node)
{
    if(controls::Menu *menu=node->createContextMenu())
        menu->addItem("Delete node")->setAction(this, &NodeGraphDemo::onDeleteItem);
    node->onContextMenu.add(this, &NodeGraphDemo::onItemMenu);
}

void NodeGraphDemo::giveLinkMenu(controls::Link *link)
{
    if(controls::Menu *menu=link->createContextMenu())
    {
        menu->addItem("Straight")->setAction(this, &NodeGraphDemo::onStraight);
        menu->addItem("Bezier")->setAction(this, &NodeGraphDemo::onBezier);
        menu->addItem("Orthogonal")->setAction(this, &NodeGraphDemo::onOrthogonal);
        menu->addDivider();
        menu->addItem("Delete link")->setAction(this, &NodeGraphDemo::onDeleteItem);
    }
    link->onContextMenu.add(this, &NodeGraphDemo::onItemMenu);
}

// A right-click on empty space: the new node is not linked to anything.
void NodeGraphDemo::onGraphMenu(event::Info)
{
    m_addFrom=nullptr;
}

void NodeGraphDemo::onItemMenu(event::Info info)
{
    m_menuTarget=info.controlCaller;
}

// The kinds the "Add node" menu offers: a value, a maths operation and a
// mix, each with an input and an output of the float kind.
controls::Node *NodeGraphDemo::addNodeOfKind(int kind, const PointF &at, controls::Pin *from)
{
    static const char *const titles[]={"Value", "Math", "Mix"};
    controls::Node *node=m_graph->addNode(titles[kind], Point(int(std::lround(at.x)), int(std::lround(at.y))));

    node->setTitleColor(Color(0x4a, 0x4a, 0x6a));
    if(kind != 0)
        addPin(node, controls::PinDirection::Input, "In", FloatPin, controls::PinShape::Circle);
    addPin(node, controls::PinDirection::Output, "Out", FloatPin, controls::PinShape::Circle);
    if(kind == 1)
    {
        controls::ComboBox *operation=node->newChild<controls::ComboBox>();
        operation->setDock(DockPosition::Top);
        operation->setHeight(20);
        operation->addItem("Add");
        operation->addItem("Multiply");
    }
    else
        addNumber(node, kind == 0 ? 1.0 : 0.5, 0.0, 1.0);
    giveNodeMenu(node);

    // a link dropped where no node was ends on the new node
    if(from)
    {
        for(controls::Pin *pin : node->getPins())
        {
            if(m_graph->canLink(from, pin))
            {
                controls::Pin *a=nullptr;
                controls::Pin *b=nullptr;

                m_graph->canLink(from, pin, &a, &b);
                if(controls::Link *link=m_graph->addLink(a, b))
                    giveLinkMenu(link);
                break;
            }
        }
    }
    return node;
}

void NodeGraphDemo::onAddValue(event::Info)
{
    addNodeOfKind(0, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

void NodeGraphDemo::onAddMath(event::Info)
{
    addNodeOfKind(1, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

void NodeGraphDemo::onAddMix(event::Info)
{
    addNodeOfKind(2, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

// The item whose menu it is, removed as Delete would remove it: its
// requests fire and may veto.
void NodeGraphDemo::onDeleteItem(event::Info)
{
    if(controls::Node *node=gweni_cast<controls::Node>(m_menuTarget))
        m_graph->selectOnly(node);
    else if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        m_graph->selectOnly(link);
    else
        return;
    m_graph->removeSelection();
    m_menuTarget=nullptr;
}

void NodeGraphDemo::onStraight(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Straight);
}

void NodeGraphDemo::onBezier(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Bezier);
}

void NodeGraphDemo::onOrthogonal(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Orthogonal);
}

void NodeGraphDemo::onLinkCreated(event::Info info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(info.control))
        giveLinkMenu(link);
}

// The "Add node" menu where the link was let go; the node picked is linked to
// the pin it came from.
void NodeGraphDemo::onDroppedOnEmpty(event::Info)
{
    controls::Pin *from=m_graph->getDropPin();

    m_graph->openContextMenuAt(m_graph->getDropPosition());
    // after the menu's own handler, which clears it for a right-click
    m_addFrom=from;
}

}//namespace gweni
