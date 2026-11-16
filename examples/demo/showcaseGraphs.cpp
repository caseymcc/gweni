/*
  * Gweni demo
  * See license in gweni.h
 */

#include "demo/showcaseGraphs.h"
#include "demo/nodeGraphKit.h"
#include "gweni/controls/button.h"
#include "gweni/controls/comboBox.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/nodeGraph.h"
#include "gweni/controls/scrollControl.h"
#include "gweni/controls/textBox.h"

#include <string>

namespace gweni
{

using nodegraphkit::addNumber;
using nodegraphkit::addPin;
using nodegraphkit::ColorPin;
using nodegraphkit::FloatPin;
using nodegraphkit::ShaderPin;

namespace
{

// The graph's view, in graph units: the zoom, and the graph point drawn at the
// graph's top-left corner. Fixed here, not fitted to the nodes, so the view
// is the same whatever size the page is first given.
const float GraphZoom=0.9f;
const PointF GraphOrigin(-8.0f, -8.0f);

// Where the panel draws its content's origin, in its own units.
const PointF PanelPan(6.0f, 6.0f);

}//namespace

GWENI_CONTROL_CONSTRUCTOR(ShowcaseZoomPanel)
{
}

void ShowcaseZoomPanel::spaceChanged(bool zoomed)
{
    ParentClass::spaceChanged(zoomed);
    if(zoomed && zoomChanged)
        zoomChanged(zoom());
}

// The zoom panel's section docked right at a fixed width, the node graph's
// filling the rest: a layout row (showcase::initPage()) would size the two by
// their content.
GWENI_CONTROL_CONSTRUCTOR(ShowcaseGraphsDemo)
{
    // Docked, so it fills the shell's page and ControlTest's canvas alike.
    setDock(DockPosition::Center);
    setPadding(Padding(6, 6, 6, 6));

    // A GroupBox is a Label, as wide as its content: the width is a plain
    // cell's, Fixed so a dock gives it its request, which the frame fills
    // (showcase::addSection()).
    controls::Base *rightCell=newChild<controls::Base>();

    rightCell->setDock(DockPosition::Right);
    rightCell->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
    rightCell->setWidth(310);
    rightCell->setMargin(Margin(6, 0, 0, 0));

    controls::GroupBox *right=rightCell->newChild<controls::GroupBox>();

    right->setText("Zoom panel");
    right->setDock(DockPosition::Center);

    controls::GroupBox *left=newChild<controls::GroupBox>();

    left->setText("Node graph");
    left->setDock(DockPosition::Center);

    {
        controls::GroupBox *section=left;
        controls::Label *hint=section->newChild<controls::Label>();

        hint->setDock(DockPosition::Top);
        hint->setText("Right-click: add a node, restyle a link. Drag a pin to link.");

        m_graph=section->newChild<controls::NodeGraph>();
        m_graph->setDock(DockPosition::Center);
        m_graph->setZoom(GraphZoom);
        m_graph->setPan(PointF(-GraphOrigin.x*GraphZoom, -GraphOrigin.y*GraphZoom));

        controls::Node *time=m_graph->addNode("Time", Point(0, 10));
        time->setTitleColor(Color(0x8a, 0x3a, 0x3a));
        controls::Pin *seconds=addPin(time, controls::PinDirection::Output, "Seconds", FloatPin,
            controls::PinShape::Circle);
        addPin(time, controls::PinDirection::Output, "Frame", FloatPin, controls::PinShape::Circle);
        controls::Label *since=time->newChild<controls::Label>();
        since->setDock(DockPosition::Top);
        since->setText("Since the app started");

        controls::Node *image=m_graph->addNode("Image Texture", Point(0, 260));
        image->setTitleColor(Color(0x7a, 0x4a, 0x2a));
        controls::Pin *texel=addPin(image, controls::PinDirection::Output, "Color", ColorPin,
            controls::PinShape::Circle);
        addPin(image, controls::PinDirection::Output, "Alpha", FloatPin, controls::PinShape::Circle);
        controls::ComboBox *filter=image->newChild<controls::ComboBox>();
        filter->setDock(DockPosition::Top);
        filter->setHeight(20);
        filter->addItem("Linear");
        filter->addItem("Closest");
        filter->addItem("Cubic");

        controls::Node *wave=m_graph->addNode("Sine Wave", Point(190, 60));
        wave->setTitleColor(Color(0x3a, 0x5a, 0x7a));
        controls::Pin *phase=addPin(wave, controls::PinDirection::Input, "Phase", FloatPin,
            controls::PinShape::Circle);
        addPin(wave, controls::PinDirection::Input, "Speed", FloatPin, controls::PinShape::Circle);
        controls::Pin *value=addPin(wave, controls::PinDirection::Output, "Value", FloatPin,
            controls::PinShape::Circle);
        addNumber(wave, 2.0, 0.0, 10.0);

        controls::Node *mix=m_graph->addNode("Mix Shader", Point(190, 290));
        mix->setTitleColor(Color(0x2a, 0x6a, 0x4a));
        controls::Pin *fac=addPin(mix, controls::PinDirection::Input, "Fac", FloatPin, controls::PinShape::Circle);
        controls::Pin *color=addPin(mix, controls::PinDirection::Input, "Color", ColorPin,
            controls::PinShape::Circle);
        controls::Pin *shader=addPin(mix, controls::PinDirection::Output, "Shader", ShaderPin,
            controls::PinShape::Triangle);
        addNumber(mix, 0.5, 0.0, 1.0);

        controls::Node *output=m_graph->addNode("Material Output", Point(380, 170));
        output->setTitleColor(Color(0x6a, 0x2a, 0x5a));
        output->setToolTipText("What the renderer shades the surface with");
        controls::Pin *surface=addPin(output, controls::PinDirection::Input, "Surface", ShaderPin,
            controls::PinShape::Triangle);
        addPin(output, controls::PinDirection::Input, "Volume", ShaderPin, controls::PinShape::Triangle);
        controls::TextBox *name=output->newChild<controls::TextBox>();
        name->setDock(DockPosition::Top);
        name->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
        name->setSize(60, 20);
        name->setText("Water");

        // a link of each style; "Frame", "Alpha", "Speed" and "Volume" stay
        // open, their markers hollow
        m_graph->addLink(seconds, phase, controls::LinkStyle::Bezier);
        m_graph->addLink(value, fac, controls::LinkStyle::Orthogonal);
        m_graph->addLink(texel, color, controls::LinkStyle::Straight);
        m_graph->addLink(shader, surface, controls::LinkStyle::Bezier);

        m_menus.reset(new nodegraphkit::NodeGraphMenus(m_graph));
        m_notes.push_back("NodeGraph: five nodes, a Bezier, an Orthogonal and a Straight link,"
            " four pins left open");
        m_notes.push_back("NodeGraph: at zoom 0.9; right-click for the Add node, node and link menus");
    }
    {
        controls::GroupBox *section=right;
        controls::Base *row=section->newChild<controls::Base>();

        row->setDock(DockPosition::Top);
        row->setHeight(22);
        row->setMargin(Margin(0, 0, 0, 4));

        controls::Label *caption=row->newChild<controls::Label>();
        caption->setDock(DockPosition::Left);
        caption->setText("Zoom");
        caption->setMargin(Margin(0, 0, 6, 0));

        controls::Button *reset=row->newChild<controls::Button>();
        reset->setDock(DockPosition::Right);
        reset->setText("Reset");
        reset->setSizeFlags({SizeFlag::Fixed, SizeFlag::Expand});
        reset->setWidth(60);
        reset->onPressCaller.add(this, &ShowcaseGraphsDemo::onReset);

        m_zoomField=row->newChild<controls::DragNumber>();
        m_zoomField->setDock(DockPosition::Center);
        m_zoomField->setMargin(Margin(0, 0, 6, 0));
        m_zoomField->setRange(0.25, 4.0);
        m_zoomField->setSpeed(0.01);
        m_zoomField->setFormat("%.2fx");
        m_zoomField->setNumber(PanelZoom, false);
        m_zoomField->onValueChanged.add(this, &ShowcaseGraphsDemo::onZoomField);

        controls::Label *notes=section->newChild<controls::Label>();
        notes->setDock(DockPosition::Bottom);
        notes->setWrap(true);
        notes->setMargin(Margin(0, 4, 0, 0));
        notes->setText("Wheel: zoom about the cursor. Right or middle drag: pan. The text is drawn at"
            " the zoomed size, so it stays sharp, and a zoom lays nothing out.");

        m_panel=section->newChild<ShowcaseZoomPanel>();
        m_panel->setDock(DockPosition::Center);
        m_panel->setContentSize(Point(190, 270));
        m_panel->setZoom(PanelZoom);
        m_panel->setPan(PanelPan);

        controls::Label *title=m_panel->newChild<controls::Label>();
        title->setText("Inside a ZoomPanel");
        title->setPos(4, 2);

        controls::Button *button=m_panel->newChild<controls::Button>();
        button->setText("With Tooltip");
        button->setToolTipText("A tooltip opened from inside the panel");
        button->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        button->setPos(4, 24);
        button->setSize(110, 22);

        controls::ComboBox *combo=m_panel->newChild<controls::ComboBox>();
        combo->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        combo->setPos(4, 54);
        combo->setSize(170, 20);
        combo->addItem("Ease in");
        combo->addItem("Ease out");
        combo->addItem("Linear");

        controls::DragNumber *number=m_panel->newChild<controls::DragNumber>();
        number->setPos(4, 82);
        number->setSize(170, 20);
        number->setRange(0, 100);
        number->setFormat("%.1f");
        number->setNumber(42.5, false);

        controls::ScrollControl *scroll=m_panel->newChild<controls::ScrollControl>();
        scroll->setPos(4, 112);
        scroll->setSize(170, 150);
        scroll->setScroll(false, true);
        for(int i=0; i < 12; ++i)
        {
            controls::Label *line=scroll->newChild<controls::Label>();
            line->setText("Keyframe "+std::to_string(i+1));
            line->setPos(6, 4+i*18);
        }

        // set after the panel's first zoom, so the field starts as it was set
        m_panel->zoomChanged=[this](float zoom) { m_zoomField->setNumber(zoom, false); };
        m_notes.push_back("ZoomPanel: a Button, a ComboBox, a DragNumber and a ScrollControl at 1.5x");
    }
}

ShowcaseGraphsDemo::~ShowcaseGraphsDemo()=default;

// About the panel's centre, so the content under the middle stays there.
void ShowcaseGraphsDemo::onZoomField(event::Info)
{
    m_panel->zoomAt(float(m_zoomField->getNumber()),
        PointF(m_panel->getWidth()*0.5f, m_panel->getHeight()*0.5f));
}

void ShowcaseGraphsDemo::onReset(event::Info)
{
    m_panel->setZoom(PanelZoom);
    m_panel->setPan(PanelPan);
}

}//namespace gweni
