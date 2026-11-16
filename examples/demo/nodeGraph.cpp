/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/nodeGraph.h"
#include "demo/nodeGraphKit.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/textBox.h"

namespace gweni
{

using nodegraphkit::addNumber;
using nodegraphkit::addPin;
using nodegraphkit::ColorPin;
using nodegraphkit::FloatPin;
using nodegraphkit::ShaderPin;
using nodegraphkit::VectorPin;

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
    m_menus.reset(new nodegraphkit::NodeGraphMenus(m_graph));
}

// The menus go first, their handler unhooked from the graph that the
// children's teardown deletes after.
NodeGraphDemo::~NodeGraphDemo()=default;

controls::Node *NodeGraphDemo::addNodeOfKind(int kind, const PointF &at, controls::Pin *from)
{
    return m_menus->addNodeOfKind(kind, at, from);
}

}//namespace gweni
