
#include "demo/layoutDemo.h"

namespace gweni
{

GWENI_CONTROL_CONSTRUCTOR(LayoutDemo)
{
    controls::WindowControl *window=newChild<controls::WindowControl>();

    window->setTitle("Layout Window");
    window->setSize(200+rand()%100, 200+rand()%100);
    window->setPos(rand()%700, rand()%400);
    window->setDeleteOnClose(true);

    controls::GroupBox *verticalGroupBox1=window->newChild<controls::GroupBox>();

    verticalGroupBox1->setDock(DockPosition::Center);
    verticalGroupBox1->setName("VerticalGroup");
    verticalGroupBox1->setText("Veritcal Group");

    controls::VerticalLayout *verticalLayout1=new controls::VerticalLayout();

    verticalGroupBox1->setLayout(verticalLayout1);

    controls::Label *vb1_label=verticalGroupBox1->newChild<controls::Label>();

    vb1_label->setName("VerticalLabel");
    vb1_label->setText("Vertical Label");

    controls::Button *vb1_button1=verticalGroupBox1->newChild<controls::Button>();

    vb1_button1->setName("Button1");
    vb1_button1->setText("Button 1");
    
    controls::Button *vb1_button2=verticalGroupBox1->newChild<controls::Button>();

    vb1_button2->setName("Button2");
    vb1_button2->setText("Button 2");

    controls::HorizontalLayout *horizontalLayout1=new controls::HorizontalLayout();

    verticalLayout1->addLayout(horizontalLayout1);

    controls::Button *vb1_hbutton1=verticalGroupBox1->newChild<controls::Button>();

    vb1_hbutton1->setName("HButton1");
    vb1_hbutton1->setText("HButton 1");
    horizontalLayout1->addControl(vb1_hbutton1);

    controls::Button *vb1_hbutton2=verticalGroupBox1->newChild<controls::Button>();

    vb1_hbutton2->setName("HButton2");
    vb1_hbutton2->setText("HButton 2");
    horizontalLayout1->addControl(vb1_hbutton2);

    controls::Button *vb1_hbutton3=verticalGroupBox1->newChild<controls::Button>();

    vb1_hbutton3->setName("HButton3");
    vb1_hbutton3->setText("HButton 3");
    horizontalLayout1->addControl(vb1_hbutton3);

    controls::GroupBox *vb1_group=verticalGroupBox1->newChild<controls::GroupBox>();
    
    vb1_group->setName("RadioGroup");
    vb1_group->setText("Radio Group");

    controls::VerticalLayout *vb1_group_loyout=new controls::VerticalLayout();

    vb1_group->setLayout(vb1_group_loyout);

    controls::LabeledRadioButton *vb1_option1=vb1_group->newChild<controls::LabeledRadioButton>();

    vb1_option1->setName("Option1");
    vb1_option1->setText("Option 1");

    controls::LabeledRadioButton *vb1_option2=vb1_group->newChild<controls::LabeledRadioButton>();

    vb1_option2->setName("Option2");
    vb1_option2->setText("Option 2");

    controls::LabeledRadioButton *vb1_option3=vb1_group->newChild<controls::LabeledRadioButton>();

    vb1_option3->setName("Option3");
    vb1_option3->setText("Option 3");

    controls::GroupBox *hb1_group=verticalGroupBox1->newChild<controls::GroupBox>();

    hb1_group->setName("HRadioGroup");
    hb1_group->setText("HRadio Group");

    controls::HorizontalLayout *hb1_group_loyout=new controls::HorizontalLayout();

    hb1_group->setLayout(hb1_group_loyout);

    controls::LabeledRadioButton *hb1_option1=hb1_group->newChild<controls::LabeledRadioButton>();

    hb1_option1->setName("HOption1");
    hb1_option1->setText("HOption 1");

    controls::LabeledRadioButton *hb1_option2=hb1_group->newChild<controls::LabeledRadioButton>();

    hb1_option2->setName("HOption2");
    hb1_option2->setText("HOption 2");

    controls::LabeledRadioButton *hb1_option3=hb1_group->newChild<controls::LabeledRadioButton>();

    hb1_option3->setName("HOption3");
    hb1_option3->setText("HOption 3");

    controls::Label* vb1_multilineLabel=verticalGroupBox1->newChild<controls::Label>();
    
    vb1_multilineLabel->setName("MultilineLabel");
    vb1_multilineLabel->setText(
        "Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed ultrices pharetra scelerisque. "
        "Ut posuere velit ligula suscipit ut lobortis ligula congue. Aliquam lacinia cursus est, quis "
        "aliquam nisl scelerisque vitae. Nunc porta eros sem, nec rhoncus eros. Integer elementum, "
        "quam vitae egestas dictum, mi quam gravida augue, non fringilla lacus nisi sit amet nunc. "
        "Maecenas dolor tellus, consequat sed sodales ut, aliquam ac enim. Nulla facilisi. Maecenas "
        "eleifend, velit a lobortis vehicula, nunc lacus egestas leo, volutpat egestas augue.");
    vb1_multilineLabel->setWrap(true);
    vb1_multilineLabel->setAlignment(Alignment::Left|Alignment::Top);
    vb1_multilineLabel->setSizeFlags({SizeFlag::Elastic, SizeFlag::Bloat});
}

}//namespace gweni