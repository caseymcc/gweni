#ifndef _gweni_translucency_example_h_
#define _gweni_translucency_example_h_

#include <gweni/controls/base.h>

#include <vector>

namespace gweni
{
namespace controls
{
class Button;
class Label;
}

//! Translucency composited in z order (#685): a translucent panel (its own
//! slot, TranslucencyPanelColor) over a column of Labels, whose text shows
//! through it dimmed; an opaque block behind the panel whose slot is taken
//! after the panel's (it is made on a canvas timer after the first frame and
//! sent to the back), which shows through it too; and an opaque Button on the
//! panel, whose rounded corners blend over the panel. A column of Labels at
//! the left stays clear of a centred dialog (the modal scrim's), and Labels at
//! the bottom right lie under the stats overlay. In ControlTest's --list, not
//! the demo shell.
class TranslucencyDemo: public controls::Base
{
public:
    GWENI_CONTROL(TranslucencyDemo, controls::Base);

public:
    //! The panel's colour, alpha included
    static Color panelColor();

    controls::Base *panel() const { return m_panel; }
    //! The opaque block behind the panel, null until the timer made it
    controls::Base *block() const { return m_block; }
    controls::Button *button() const { return m_button; }
    const std::vector<controls::Label *> &underPanel() const { return m_underPanel; }
    const std::vector<controls::Label *> &leftColumn() const { return m_leftColumn; }
    const std::vector<controls::Label *> &underOverlay() const { return m_underOverlay; }

private:
    controls::Base *m_panel=nullptr;
    controls::Base *m_block=nullptr;
    controls::Button *m_button=nullptr;
    std::vector<controls::Label *> m_underPanel;
    std::vector<controls::Label *> m_leftColumn;
    std::vector<controls::Label *> m_underOverlay;
};

//! TranslucencyDemo's translucent panel and its opaque block: plain
//! containers each drawn as one slot by the demo's renderers.
class TranslucencyPanel: public controls::Base
{
public:
    GWENI_CONTROL(TranslucencyPanel, controls::Base);
};

class TranslucencyBlock: public controls::Base
{
public:
    GWENI_CONTROL(TranslucencyBlock, controls::Base);
};

}//namespace gweni

#endif//_gweni_translucency_example_h_
