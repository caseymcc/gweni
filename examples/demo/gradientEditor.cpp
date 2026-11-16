#include "demo/gradientEditor.h"
#include "gweni/controls/colorPicker.h"
#include "gweni/controls/gradientEditor.h"
#include "gweni/controls/label.h"

namespace gweni
{

// Three GradientEditors, each under a Label naming it: an opaque three-stop
// ramp, a translucent one whose spans show the checkerboard under the bar, and
// a Constant one holding each stop's colour to the next. Under them a strip
// of PreviewSwatches swatches samples the opaque ramp's evaluate() at even
// steps, set again on its onChanged (a swatch whose colour stays marks
// nothing).
GWENI_CONTROL_CONSTRUCTOR(GradientEditorDemo)
{
    setDock(DockPosition::Center);

    m_gradients[0]=Gradient({{0.0f, Color(0x1a, 0x1b, 0x26, 255)}, {0.45f, Color(0x7a, 0xa2, 0xf7, 255)},
        {1.0f, Color(0xff, 0x9e, 0x64, 255)}});
    m_gradients[1]=Gradient({{0.0f, Color(0xf7, 0x76, 0x8e, 255)}, {0.6f, Color(0x9e, 0xce, 0x6a, 128)},
        {1.0f, Color(0x7d, 0xcf, 0xff, 0)}});
    m_gradients[2]=Gradient({{0.0f, Color(0x3d, 0x59, 0xa1, 255)}, {0.25f, Color(0x9e, 0xce, 0x6a, 255)},
        {0.5f, Color(0xe0, 0xaf, 0x68, 255)}, {0.75f, Color(0xf7, 0x76, 0x8e, 255)}},
        Gradient::Interpolation::Constant);

    static const char *const captions[3]={"Opaque", "Translucent, over the checkerboard", "Constant"};
    int y=10;

    for(int i=0; i < 3; ++i)
    {
        controls::Label *label=newChild<controls::Label>();

        label->setText(captions[i]);
        label->setPos(10, y);

        controls::GradientEditor *editor=newChild<controls::GradientEditor>();

        editor->setPos(10, y+20);
        editor->setWidth(420);
        editor->setGradient(&m_gradients[i]);
        m_editors[i]=editor;
        y+=110;
    }

    controls::Label *label=newChild<controls::Label>();

    label->setText("evaluate() of the opaque ramp");
    label->setPos(10, y);
    for(int i=0; i < PreviewSwatches; ++i)
    {
        controls::internal::ColorDisplay *swatch=newChild<controls::internal::ColorDisplay>();

        swatch->setPos(15+i*13, y+20);
        swatch->setSize(12, 24);
        m_preview[i]=swatch;
    }
    updatePreview();

    m_editors[0]->onChanged.add(this, &GradientEditorDemo::onOpaqueChanged);
}

void GradientEditorDemo::onOpaqueChanged(event::Info /*info*/)
{
    updatePreview();
}

void GradientEditorDemo::updatePreview()
{
    for(int i=0; i < PreviewSwatches; ++i)
        m_preview[i]->setColor(m_gradients[0].evaluate(previewPosition(i)));
}

}//namespace gweni
