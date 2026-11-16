/*
 *  Gweni demo
 *  See license in gweni.h
 */
#ifndef _gweni_curveeditor_example_h_
#define _gweni_curveeditor_example_h_

#include <gweni/controls/base.h>
#include <gweni/curve.h>

namespace gweni
{
namespace controls
{
class CurveEditor;
class DragNumber;
class Label;
}

class CurveEditorDemo: public controls::Base
{
public:
    GWENI_CONTROL(CurveEditorDemo, controls::Base);

public:
    controls::CurveEditor *editor() const { return m_editor; }
    //! The x, y and z curves the editor shows.
    Curve &curve(size_t i) { return m_curves[i]; }
    controls::DragNumber *sampleField() const { return m_sample; }
    controls::Label *readout() const { return m_readout; }

private:
    void onChanged(event::Info info);
    void updateReadout();

    Curve m_curves[3];
    controls::CurveEditor *m_editor=nullptr;
    controls::DragNumber *m_sample=nullptr;
    controls::Label *m_readout=nullptr;
};

}//namespace gweni

#endif//_gweni_curveeditor_example_h_
