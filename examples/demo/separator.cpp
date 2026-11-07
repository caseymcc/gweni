#include "demo/separator.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/bullet.h"
#include "gweni/controls/button.h"
#include "gweni/controls/label.h"
#include "gweni/controls/separator.h"

namespace gweni
{

// Rules between labels, one with a caption, a vertical rule between two
// buttons in a row, and a bulleted list.
GWENI_CONTROL_CONSTRUCTOR(SeparatorDemo)
{
    setDock(DockPosition::Center);

    // The page fills its parent; the column is only as high as its content,
    // so its layout has no room to spread between the rows.
    controls::Base *content=newChild<controls::Base>();
    controls::VerticalLayout *column=new controls::VerticalLayout();

    content->setLayout(column);
    content->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    content->setPos(10, 10);
    content->setWidth(420);

    const Margin ruleMargin(0, 6, 0, 6);

    content->newChild<controls::Label>()->setText("A label above a plain rule");
    content->newChild<controls::Separator>()->setMargin(ruleMargin);
    content->newChild<controls::Label>()->setText("A label between two rules");
    {
        controls::Separator *captioned=content->newChild<controls::Separator>();

        captioned->setText("Captioned rule");
        captioned->setMargin(ruleMargin);
    }
    content->newChild<controls::Label>()->setText("A label below the captioned rule");

    {
        controls::HorizontalLayout *row=new controls::HorizontalLayout();

        column->addLayout(row);

        controls::Button *left=content->newChild<controls::Button>();

        left->setText("Left of the rule");
        left->setMargin(Margin(0, 6, 0, 6));
        row->addControl(left);

        controls::Separator *vertical=content->newChild<controls::Separator>();

        vertical->setVertical();
        vertical->setMargin(Margin(8, 0, 8, 0));
        row->addControl(vertical);

        controls::Button *right=content->newChild<controls::Button>();

        right->setText("Right of the rule");
        right->setMargin(Margin(0, 6, 0, 6));
        row->addControl(right);
    }

    {
        controls::Separator *list=content->newChild<controls::Separator>();

        list->setText("Bullets");
        list->setMargin(ruleMargin);
    }
    content->newChild<controls::Bullet>()->setText("The first point");
    content->newChild<controls::Bullet>()->setText("A second, longer point");
    content->newChild<controls::Bullet>()->setText("The third point");
}

}//namespace gweni
