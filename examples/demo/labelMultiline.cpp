#include "demo/labelMultiline.h"
#include "gweni/controls/label.h"
#include "gweni/controls/crossSplitter.h"

namespace gweni
{

//LabelMultilineDemo::LabelMultilineDemo(const String &name):
//    Base()
GWENI_CONTROL_CONSTRUCTOR(LabelMultilineDemo)
{
    setDock(DockPosition::Center);
    controls::CrossSplitter *pSplitter=newChild<controls::CrossSplitter>();
    pSplitter->setDock(DockPosition::Center);
    pSplitter->setMargin(Margin(10, 10, 10, 10));
//    {
//        controls::Label *label=pSplitter->newChild<controls::Label>();
//        label->setText("I think we're losing sight of the real issue here, which is: what are we gonna call ourselves?\n\nErm, and I think it comes down to a choice between \"The League Against Salivating Monsters\" or my own personal preference, which is \"The Committee for the Liberation and Integration of Terrifying Organisms and their Rehabilitation Into Society.\" Erm, one drawback with that: the abbreviation is \"CLITORIS.\"");
//        label->setWrap(true);
//        pSplitter->setPanel(0, label);
//    }
//    {
//        controls::Label *label=pSplitter->newChild<controls::Label>();
//        label->setText("Lorem ipsum dolor sit amet, consectetur adipiscing elit. Sed ultrices pharetra scelerisque. Ut posuere velit a ligula suscipit ut lobortis ligula congue. Aliquam lacinia cursus est, quis aliquam nisl scelerisque vitae. Nunc porta eros sem, nec rhoncus eros. Integer elementum, quam vitae egestas dictum, mi quam gravida augue, non fringilla lacus nisi sit amet nunc. Maecenas dolor tellus, consequat sed sodales ut, aliquam ac enim. Nulla facilisi. Maecenas eleifend, velit a lobortis vehicula, nunc lacus egestas leo, volutpat egestas augue nulla nec turpis. Aenean convallis diam magna. Duis ac lacinia massa. In id dui vel dui laoreet congue. Aliquam suscipit quam et augue sagittis egestas. Integer posuere arcu sit amet lacus porttitor et malesuada enim mollis. Duis luctus est in purus consectetur sit amet volutpat tortor euismod. Nulla facilisi.");
//        label->setWrap(true);
//        pSplitter->setPanel(1, label);
//    }
//    {
//        controls::Label *label=pSplitter->newChild<controls::Label>();
//        label->setText("Integer eget rutrum nisi. Ut eget dui et turpis volutpat pulvinar non sed lacus. Proin vestibulum felis nec elit tristique non imperdiet eros pretium. Nullam pulvinar sem eleifend turpis eleifend vel dapibus dui sodales. Curabitur euismod hendrerit felis nec vestibulum. Suspendisse tempus gravida ligula a vestibulum. Phasellus et eros at justo hendrerit cursus. Phasellus rutrum porta diam, in mollis ante aliquam at. Vestibulum interdum ligula at massa auctor scelerisque. Sed at tincidunt risus. Donec ut est dui. Vestibulum blandit urna eu metus malesuada blandit");
//        label->setWrap(true);
//        pSplitter->setPanel(2, label);
//    }
//    {
//        controls::Label *label=pSplitter->newChild<controls::Label>();
//        label->setText("Nullam vel risus eget lacus consectetur rutrum. Curabitur eros libero, porta sed commodo vel, euismod non quam. Fusce bibendum posuere metus, nec mollis odio rutrum ac. Cras nec sapien et mauris dapibus pretium id quis dolor. Sed a velit vel tellus viverra sodales. Praesent tempor purus et elit ultrices tristique. Sed in enim nec elit molestie fermentum et quis enim. Nullam varius placerat lacus nec ultrices. Aliquam erat volutpat. Suspendisse potenti. Nullam euismod pulvinar luctus. Vestibulum ut dui nisi, eget tempus est. Vivamus molestie arcu non enim pulvinar sollicitudin. Pellentesque dapibus risus sit amet diam tempor faucibus accumsan ante porta. Phasellus quis facilisis quam. Fusce eget adipiscing magna.");
//        label->setWrap(true);
//        pSplitter->setPanel(3, label);
//    }
}

}//namespace gweni
