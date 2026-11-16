/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/curveEditor.h"
#include "gweni/controls/curveEditor.h"
#include "gweni/controls/dragNumber.h"
#include "gweni/controls/label.h"

#include <cstdio>

namespace gweni
{

namespace
{

Curve::Key key(double time, double value, Curve::TangentMode mode, double in=0.0, double out=0.0,
    bool broken=false)
{
    Curve::Key k;

    k.time=time;
    k.value=value;
    k.mode=mode;
    k.inTangent=in;
    k.outTangent=out;
    k.broken=broken;
    return k;
}

}//namespace

// A CurveEditor over a position's x, y and z curves across 10 seconds, every
// tangent mode among their keys (Smooth, Linear, Constant, a joined Free key
// and a broken one), framed to show them all; under it a DragNumber picks a
// time and a Label shows the three curves' evaluate() there, set again when
// the time or a curve changes.
GWENI_CONTROL_CONSTRUCTOR(CurveEditorDemo)
{
    using Mode=Curve::TangentMode;

    setDock(DockPosition::Center);

    m_curves[0]=Curve({key(0.0, 0.2, Mode::Smooth), key(2.5, 0.8, Mode::Smooth), key(5.0, 0.35, Mode::Smooth),
        key(7.5, 0.65, Mode::Constant), key(10.0, 0.3, Mode::Smooth)});
    m_curves[1]=Curve({key(0.0, -0.4, Mode::Linear), key(3.0, 0.1, Mode::Smooth),
        key(6.0, -0.2, Mode::Free, -0.35, 0.3, true), key(10.0, 0.0, Mode::Smooth)});
    m_curves[2]=Curve({key(0.0, 0.9, Mode::Free, -0.2, -0.2), key(4.0, 0.55, Mode::Smooth),
        key(8.0, 1.05, Mode::Free, 0.0, 0.0), key(10.0, 0.7, Mode::Linear)});

    m_editor=newChild<controls::CurveEditor>();
    m_editor->setPos(10, 10);
    m_editor->setSize(620, 340);
    m_editor->addCurve(&m_curves[0]);
    m_editor->addCurve(&m_curves[1]);
    m_editor->addCurve(&m_curves[2]);
    m_editor->setView(-0.5, 10.5, -0.6, 1.2);
    m_editor->onChanged.add(this, &CurveEditorDemo::onChanged);

    controls::Label *caption=newChild<controls::Label>();

    caption->setText("evaluate() at");
    caption->setPos(10, 364);

    m_sample=newChild<controls::DragNumber>();
    m_sample->setPos(90, 360);
    m_sample->setWidth(80);
    m_sample->setRange(0.0, 10.0);
    m_sample->setSpeed(0.05);
    m_sample->setFormat("%.2f s");
    m_sample->setNumber(4.0, false);
    m_sample->onValueChanged.add(this, &CurveEditorDemo::onChanged);

    m_readout=newChild<controls::Label>();
    m_readout->setPos(180, 364);
    updateReadout();
}

void CurveEditorDemo::onChanged(event::Info /*info*/)
{
    updateReadout();
}

void CurveEditorDemo::updateReadout()
{
    const double t=m_sample->getNumber();
    char text[96];

    std::snprintf(text, sizeof(text), "x %.3f   y %.3f   z %.3f", m_curves[0].evaluate(t), m_curves[1].evaluate(t),
        m_curves[2].evaluate(t));
    m_readout->setText(text);
    m_readout->sizeToContents();
}

}//namespace gweni
