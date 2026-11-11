#include "demo/nodeGraph.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/label.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/textBox.h"

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
    hint->setText("Wheel: zoom about the cursor. Middle or right drag, or Space and drag: pan.");

    m_graph=newChild<controls::NodeGraph>();
    m_graph->setDock(DockPosition::Center);
    m_graph->setPan(PointF(10.0f, 10.0f));

    controls::Node *coord=m_graph->addNode("Texture Coordinate", Point(10, 30));
    coord->setTitleColor(Color(0x8a, 0x3a, 0x3a));
    coord->setToolTipText("The surface's UV coordinates");
    addPin(coord, controls::PinDirection::Output, "UV", VectorPin, controls::PinShape::Square);
    addPin(coord, controls::PinDirection::Output, "Normal", VectorPin, controls::PinShape::Square);
    controls::Label *note=coord->newChild<controls::Label>();
    note->setDock(DockPosition::Top);
    note->setText("From the active UV map");

    controls::Node *noise=m_graph->addNode("Noise Texture", Point(230, 10));
    noise->setTitleColor(Color(0x7a, 0x4a, 0x2a));
    addPin(noise, controls::PinDirection::Input, "Vector", VectorPin, controls::PinShape::Square);
    addPin(noise, controls::PinDirection::Input, "Scale", FloatPin, controls::PinShape::Circle);
    addPin(noise, controls::PinDirection::Output, "Fac", FloatPin, controls::PinShape::Circle);
    addPin(noise, controls::PinDirection::Output, "Color", ColorPin, controls::PinShape::Circle);
    addNumber(noise, 5.0, 0.0, 100.0);
    addNumber(noise, 2.0, 0.0, 16.0);

    controls::Node *ramp=m_graph->addNode("Color Ramp", Point(230, 200));
    ramp->setTitleColor(Color(0x3a, 0x5a, 0x7a));
    addPin(ramp, controls::PinDirection::Input, "Fac", FloatPin, controls::PinShape::Circle);
    addPin(ramp, controls::PinDirection::Output, "Color", ColorPin, controls::PinShape::Circle);
    addPin(ramp, controls::PinDirection::Output, "Alpha", FloatPin, controls::PinShape::Circle);
    controls::ComboBox *interpolation=ramp->newChild<controls::ComboBox>();
    interpolation->setDock(DockPosition::Top);
    interpolation->setHeight(20);
    interpolation->addItem("Linear");
    interpolation->addItem("Ease");
    interpolation->addItem("Constant");

    controls::Node *mix=m_graph->addNode("Mix Shader", Point(450, 60));
    mix->setTitleColor(Color(0x2a, 0x6a, 0x4a));
    addPin(mix, controls::PinDirection::Input, "Fac", FloatPin, controls::PinShape::Circle);
    addPin(mix, controls::PinDirection::Input, "Shader", ShaderPin, controls::PinShape::Triangle);
    addPin(mix, controls::PinDirection::Input, "Shader", ShaderPin, controls::PinShape::Triangle);
    addPin(mix, controls::PinDirection::Output, "Shader", ShaderPin, controls::PinShape::Triangle);
    addNumber(mix, 0.5, 0.0, 1.0);

    controls::Node *output=m_graph->addNode("Material Output", Point(660, 120));
    output->setTitleColor(Color(0x6a, 0x2a, 0x5a));
    output->setToolTipText("What the renderer shades the surface with");
    controls::Pin *surface=addPin(output, controls::PinDirection::Input, "Surface", ShaderPin,
        controls::PinShape::Triangle);
    surface->setConnected(true);
    addPin(output, controls::PinDirection::Input, "Volume", ShaderPin, controls::PinShape::Triangle);
    controls::TextBox *materialName=output->newChild<controls::TextBox>();
    materialName->setDock(DockPosition::Top);
    materialName->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
    materialName->setSize(60, 20);
    materialName->setText("Surface");
}

}//namespace gweni
