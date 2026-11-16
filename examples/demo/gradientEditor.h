#ifndef _gweni_gradienteditor_example_h_
#define _gweni_gradienteditor_example_h_

#include <gweni/controls/base.h>
#include <gweni/gradient.h>

namespace gweni
{
namespace controls
{
class GradientEditor;
namespace internal
{
class ColorDisplay;
}
}

class GradientEditorDemo: public controls::Base
{
public:
    GWENI_CONTROL(GradientEditorDemo, controls::Base);

public:
    static constexpr int PreviewSwatches=32;

    //! The opaque, the translucent and the Constant ramp's editors.
    controls::GradientEditor *opaqueEditor() const { return m_editors[0]; }
    controls::GradientEditor *translucentEditor() const { return m_editors[1]; }
    controls::GradientEditor *constantEditor() const { return m_editors[2]; }
    //! The gradient the opaque editor edits, which the preview samples.
    Gradient &opaqueGradient() { return m_gradients[0]; }
    controls::internal::ColorDisplay *previewSwatch(int i) const { return m_preview[i]; }
    //! The preview's sample of the opaque gradient: swatch i at t i/31.
    static float previewPosition(int i) { return (float)i/(float)(PreviewSwatches-1); }

private:
    void onOpaqueChanged(event::Info info);
    void updatePreview();

    Gradient m_gradients[3];
    controls::GradientEditor *m_editors[3]={nullptr, nullptr, nullptr};
    controls::internal::ColorDisplay *m_preview[PreviewSwatches]={};
};

}//namespace gweni

#endif//_gweni_gradienteditor_example_h_
