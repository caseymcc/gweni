#ifndef _gweni_themeEditor_example_h_
#define _gweni_themeEditor_example_h_

#include <gweni/controls/base.h>

namespace gweni
{
namespace tools
{
class ThemeEditor;
}

//! A few controls (a GroupBox of Buttons, a check box, a TextBox, a Slider, a
//! ProgressBar and a Label) and the theme editor beside them, ButtonNormal's
//! node open and its top and bottom colours edited to a deep blue, which the
//! Buttons and the editor's own node headers show.
//!
//! The surfaces are process globals, so the edit outlives the demo unless it
//! is undone: the first ThemeEditorDemo alive keeps the theme as it found it,
//! and the last one to go gives it back, so the demos after it (--shot-all, a
//! second build in the same process) draw as if it had never been.
class ThemeEditorDemo: public controls::Base
{
public:
    GWENI_CONTROL(ThemeEditorDemo, controls::Base);

public:
    ~ThemeEditorDemo() override;

    tools::ThemeEditor *editor() const { return m_editor; }

    //! The surface and the colours the demo gives its top and bottom.
    static const char *const EditedSurface;
    static const gweni::Color EditedTop;
    static const gweni::Color EditedBottom;

private:
    tools::ThemeEditor *m_editor=nullptr;
};

}//namespace gweni

#endif//_gweni_themeEditor_example_h_
