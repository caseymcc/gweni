#ifndef _gweni_zoomPanel_example_h_
#define _gweni_zoomPanel_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace controls
{
class ZoomPanel;
}

//! Ordinary controls in a ZoomPanel at 1.25x, panned by a fraction of a
//! unit: a Button with a tooltip, a ComboBox, a DragNumber, a TextBox and a
//! ScrollControl of rows. The wheel zooms about the cursor, a middle or right
//! drag (or Space and a left drag) pans.
class ZoomPanelDemo: public controls::Base
{
public:
    GWENI_CONTROL(ZoomPanelDemo, controls::Base);

public:
    controls::ZoomPanel *panel() const { return m_panel; }

private:
    controls::ZoomPanel *m_panel=nullptr;
};

}//namespace gweni

#endif//_gweni_zoomPanel_example_h_
