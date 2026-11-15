/*
 *  Gweni demo
 *  See license in gweni.h
 */
#include "demo/opacity.h"
#include "gweni/anim.h"
#include "gweni/controls/button.h"
#include "gweni/controls/checkBox.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/horizontalSlider.h"
#include "gweni/controls/label.h"
#include "gweni/controls/progressBar.h"
#include "gweni/controls/textBox.h"

#include <cmath>
#include <string>

namespace gweni
{

namespace
{

#if GWENI_ANIMATE
//! The panel faded out and back in: the sliders' levels times |1 - 2t| over
//! the fade's length, linear, then the sliders' levels again.
class OpacityFade: public anim::TimedAnimation
{
public:
    explicit OpacityFade(OpacityDemo *demo)
        : TimedAnimation(OpacityDemo::FadeSeconds, 0.0f, 1.0f), m_demo(demo)
    {}

    void run(float delta) override { m_demo->applyLevels(std::fabs(1.0f-2.0f*delta)); }
    void onFinish() override { m_demo->applyLevels(); }

private:
    OpacityDemo *m_demo;
};
#endif

}//namespace

GWENI_CONTROL_CONSTRUCTOR(OpacityDemo)
{
    setDock(DockPosition::Center);

    auto label=[](controls::Base *parent, const String &text, int x, int y)
    {
        controls::Label *made=parent->newChild<controls::Label>();

        made->setText(text);
        made->setPos(x, y);
        made->sizeToContents();
        return made;
    };
    auto slider=[this, &label](const String &caption, int y)
    {
        label(this, caption, 10, y);

        controls::HorizontalSlider *made=newChild<controls::HorizontalSlider>();

        made->setBounds(10, y+18, 160, 20);
        made->setRange(0.0f, 1.0f);
        made->setFloatValue(1.0f);
        made->onValueChanged.add(this, &OpacityDemo::onLevelChanged);
        return made;
    };

    m_surface=slider("Panel surface level", 10);
    m_text=slider("Panel text level", 60);

    m_fade=newChild<controls::Button>();
    m_fade->setText("Fade out and in");
    m_fade->setBounds(10, 112, 160, 24);
    m_fade->onPressCaller.add(this, &OpacityDemo::onFade);

    // what the panel hides at 1, built first so the panel is in front
    for(int i=0; i < 8; ++i)
        label(this, "Behind the panel, line "+std::to_string(i+1), 220, 44+26*i);

    m_panel=newChild<controls::GroupBox>();
    m_panel->setText("A panel");
    m_panel->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    m_panel->setBounds(200, 20, 300, 240);

    label(m_panel, "A label in the panel", 10, 6);

    controls::Button *button=m_panel->newChild<controls::Button>();
    button->setText("A button");
    button->setBounds(10, 30, 120, 24);

    controls::CheckBoxWithLabel *check=m_panel->newChild<controls::CheckBoxWithLabel>();
    check->setPos(10, 64);
    check->label()->setText("A check box");
    check->checkBox()->setChecked(true);

    controls::TextBox *text=m_panel->newChild<controls::TextBox>();
    text->setText("Some text");
    text->setBounds(10, 92, 200, 24);

    controls::ProgressBar *progress=m_panel->newChild<controls::ProgressBar>();
    progress->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    progress->setBounds(10, 126, 200, 16);
    progress->setProgress(0.6f);

    // its own level replaces the panel's: always drawn at 1
    m_opaque=m_panel->newChild<controls::Button>();
    m_opaque->setText("Always opaque");
    m_opaque->setBounds(10, 152, 160, 24);
    m_opaque->setOpacity(1.0f);
}

void OpacityDemo::applyLevels(float factor)
{
    m_panel->setOpacity(m_surface->getFloatValue()*factor, m_text->getFloatValue()*factor);
}

void OpacityDemo::onLevelChanged(event::Info)
{
    applyLevels();
}

void OpacityDemo::onFade(event::Info)
{
    startFade();
}

void OpacityDemo::startFade()
{
#if GWENI_ANIMATE
    anim::cancel(m_panel);
    anim::add(m_panel, new OpacityFade(this));
#else
    applyLevels();
#endif
}

}//namespace gweni
