/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/nodeGraphKit.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/menu.h"
#include "gweni/controls/menuItem.h"

#include <cmath>

namespace gweni
{
namespace nodegraphkit
{

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

NodeGraphMenus::NodeGraphMenus(controls::NodeGraph *graph)
    : m_graph(graph)
{
    if(controls::Menu *menu=m_graph->createContextMenu())
    {
        menu->addItem("Add Value")->setAction(this, &NodeGraphMenus::onAddValue);
        menu->addItem("Add Math")->setAction(this, &NodeGraphMenus::onAddMath);
        menu->addItem("Add Mix")->setAction(this, &NodeGraphMenus::onAddMix);
    }
    m_graph->onContextMenu.add(this, &NodeGraphMenus::onGraphMenu);
    for(controls::Node *node : m_graph->getNodes())
        giveNodeMenu(node);
    for(controls::Link *link : m_graph->getLinks())
        giveLinkMenu(link);
    m_graph->onLinkCreated.add(this, &NodeGraphMenus::onLinkCreated);
    m_graph->onLinkDroppedOnEmpty.add(this, &NodeGraphMenus::onDroppedOnEmpty);
}

void NodeGraphMenus::giveNodeMenu(controls::Node *node)
{
    if(controls::Menu *menu=node->createContextMenu())
        menu->addItem("Delete node")->setAction(this, &NodeGraphMenus::onDeleteItem);
    node->onContextMenu.add(this, &NodeGraphMenus::onItemMenu);
}

void NodeGraphMenus::giveLinkMenu(controls::Link *link)
{
    if(controls::Menu *menu=link->createContextMenu())
    {
        menu->addItem("Straight")->setAction(this, &NodeGraphMenus::onStraight);
        menu->addItem("Bezier")->setAction(this, &NodeGraphMenus::onBezier);
        menu->addItem("Orthogonal")->setAction(this, &NodeGraphMenus::onOrthogonal);
        menu->addDivider();
        menu->addItem("Delete link")->setAction(this, &NodeGraphMenus::onDeleteItem);
    }
    link->onContextMenu.add(this, &NodeGraphMenus::onItemMenu);
}

// A right-click on empty space: the new node is not linked to anything.
void NodeGraphMenus::onGraphMenu(event::Info)
{
    m_addFrom=nullptr;
}

void NodeGraphMenus::onItemMenu(event::Info info)
{
    m_menuTarget=info.controlCaller;
}

// The kinds the "Add node" menu offers: a value, a maths operation and a
// mix, each with an input and an output of the float kind.
controls::Node *NodeGraphMenus::addNodeOfKind(int kind, const PointF &at, controls::Pin *from)
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

void NodeGraphMenus::onAddValue(event::Info)
{
    addNodeOfKind(0, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

void NodeGraphMenus::onAddMath(event::Info)
{
    addNodeOfKind(1, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

void NodeGraphMenus::onAddMix(event::Info)
{
    addNodeOfKind(2, m_graph->getContextMenuPosition(), m_addFrom);
    m_addFrom=nullptr;
}

// The item whose menu it is, removed as Delete would remove it: its
// requests fire and may veto.
void NodeGraphMenus::onDeleteItem(event::Info)
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

void NodeGraphMenus::onStraight(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Straight);
}

void NodeGraphMenus::onBezier(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Bezier);
}

void NodeGraphMenus::onOrthogonal(event::Info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(m_menuTarget))
        link->setStyle(controls::LinkStyle::Orthogonal);
}

void NodeGraphMenus::onLinkCreated(event::Info info)
{
    if(controls::Link *link=gweni_cast<controls::Link>(info.control))
        giveLinkMenu(link);
}

// The "Add node" menu where the link was let go; the node picked is linked to
// the pin it came from.
void NodeGraphMenus::onDroppedOnEmpty(event::Info)
{
    controls::Pin *from=m_graph->getDropPin();

    m_graph->openContextMenuAt(m_graph->getDropPosition());
    // after the menu's own handler, which clears it for a right-click
    m_addFrom=from;
}

}//namespace nodegraphkit
}//namespace gweni
