#include "demo/themeEditor.h"
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/progressBar.h"
#include "gweni/controls/textBox.h"
#include "gweni/skins/textured/texturedSkin.h"
#include "gweni/tools/themeEditor.h"

#include <memory>

namespace gweni
{

namespace
{
//! The ThemeEditorDemos alive, and the theme as the first of them found it.
int g_alive=0;
std::unique_ptr<skin::textured::TexturedSkin::ThemeValues> g_found;
}

const char *const ThemeEditorDemo::EditedSurface="ButtonNormal";
const gweni::Color ThemeEditorDemo::EditedTop(0x3d, 0x59, 0xa1);
// Each channel's top-to-bottom difference a multiple of 32: then no row of the
// rects drawn in it (up to 48 device px high at scale 2) is an exact x.5 value,
// which GL and Vulkan round apart (#458).
const gweni::Color ThemeEditorDemo::EditedBottom(0x1d, 0x39, 0x61);

// The controls on the left, the editor on the right. The editor is the demo's
// own child here, not the canvas's, so the page shows and hides it with the
// rest; it redraws the whole canvas all the same.
GWENI_CONTROL_CONSTRUCTOR(ThemeEditorDemo)
{
    setDock(DockPosition::Center);

    skin::textured::TexturedSkin *skin=dynamic_cast<skin::textured::TexturedSkin *>(getSkin());

    if(skin && (g_alive++ == 0))
        g_found.reset(new skin::textured::TexturedSkin::ThemeValues(skin->captureTheme()));

    controls::GroupBox *group=newChild<controls::GroupBox>();
    group->setText("Preview");
    group->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    group->setPos(10, 10);
    group->setSize(260, 220);

    controls::Button *ok=group->newChild<controls::Button>("ok");
    ok->setText("OK");
    ok->setPos(10, 10);
    ok->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    ok->setSize(100, 24);

    controls::Button *cancel=group->newChild<controls::Button>("cancel");
    cancel->setText("Cancel");
    cancel->setPos(120, 10);
    cancel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    cancel->setSize(100, 24);

    controls::CheckBoxWithLabel *check=group->newChild<controls::CheckBoxWithLabel>("check");
    check->label()->setText("Remember me");
    check->checkBox()->setChecked(true);
    check->setPos(10, 45);

    controls::TextBox *box=group->newChild<controls::TextBox>("name");
    box->setText("Theme");
    box->setPos(10, 75);
    box->setWidth(220);

    controls::HorizontalSlider *slider=group->newChild<controls::HorizontalSlider>("slider");
    slider->setPos(10, 110);
    slider->setSize(220, 20);
    slider->setRange(0, 100);
    slider->setFloatValue(40);

    controls::ProgressBar *progress=group->newChild<controls::ProgressBar>("progress");
    progress->setPos(10, 145);
    progress->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    progress->setSize(220, 20);
    progress->setProgress(0.65f);

    controls::Label *label=group->newChild<controls::Label>("label");
    label->setText("Every surface and text colour, live");
    label->setPos(10, 180);

    m_editor=newChild<tools::ThemeEditor>();
    // placed in a demo not laid out yet, which a clamp to it would put at 0,0
    m_editor->setClampMovement(false);
    m_editor->setPos(285, 5);
    m_editor->setSize(570, 530);

    if(tools::ThemeEditorNode *node=m_editor->surfaceNode(EditedSurface))
        node->open();
    m_editor->setSurfaceColor(EditedSurface, tools::ThemeSurfaceField::Top, EditedTop);
    m_editor->setSurfaceColor(EditedSurface, tools::ThemeSurfaceField::Bottom, EditedBottom);
}

ThemeEditorDemo::~ThemeEditorDemo()
{
    skin::textured::TexturedSkin *skin=dynamic_cast<skin::textured::TexturedSkin *>(getSkin());

    if(skin && g_found && (--g_alive == 0))
    {
        skin->applyTheme(*g_found);
        g_found.reset();
    }
}

}//namespace gweni
