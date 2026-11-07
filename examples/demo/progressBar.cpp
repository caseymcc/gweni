#include "demo/progressBar.h"
#include <gweni/controls/layout/layout.h>
#include "gweni/controls/button.h"
#include "gweni/controls/radioButtonController.h"
#include "gweni/controls/progressBar.h"

namespace gweni
{

// The horizontal bars in a column on the left, the last one indeterminate, and
// under them a button that stops and starts both indeterminate bars; the
// vertical bars in a row on the right, the last one indeterminate.
GWENI_CONTROL_CONSTRUCTOR(ProgressBarDemo)
{
    setDock(DockPosition::Center);

    // each column is only as high (the row only as wide) as its content, so its
    // layout has no room to spread between the bars
    controls::Base *column=newChild<controls::Base>();

    column->setLayout(new controls::VerticalLayout());
    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 10);
    column->setWidth(200);

    const Margin barMargin(0, 0, 0, 8);

    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 20, 200, 20));
        pb->setMargin(barMargin);
        pb->setProgress(0.27f);
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 50, 200, 20));
        pb->setMargin(barMargin);
        pb->setProgress(0.66f);
        pb->setAlignment(Alignment::Right | Alignment::CenterV);
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 80, 200, 20));
        pb->setMargin(barMargin);
        pb->setProgress(0.88f);
        pb->setAlignment(Alignment::Left | Alignment::CenterV);
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 110, 200, 20));
        pb->setMargin(barMargin);
        pb->setAutoLabel(false);
        pb->setProgress(0.20f);
        pb->setAlignment(Alignment::Right | Alignment::CenterV);
        pb->setText("40,245 MB");
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 140, 200, 20));
        pb->setMargin(barMargin);
        pb->setAutoLabel(false);
        pb->setProgress(1.00f);
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 170, 200, 20));
        pb->setMargin(barMargin);
        pb->setAutoLabel(false);
        pb->setProgress(0.00f);
    }
    {
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setBounds(gweni::Rect(110, 200, 200, 20));
        pb->setMargin(barMargin);
        pb->setAutoLabel(false);
        pb->setProgress(0.50f);
    }
    {
        // work of unknown length; its auto label stays empty while it runs.
        // An Elastic bar is as high as its label, so this one is given the
        // labelled bars' height, and stopping it does not move the button.
        controls::ProgressBar *pb=column->newChild<controls::ProgressBar>();
        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        pb->setSize(200, 21);
        pb->setMargin(barMargin);
        pb->setProgress(0.40f);
        pb->setIndeterminate(true);
        m_indeterminate.push_back(pb);
    }

    controls::Base *row=newChild<controls::Base>();

    row->setLayout(new controls::HorizontalLayout());
    row->setSizeFlags({SizeFlag::Shrink, SizeFlag::Fixed});
    row->setPos(230, 10);
    row->setHeight(200);

    const Margin verticalMargin(0, 0, 8, 0);

    {
        controls::ProgressBar *pb=row->newChild<controls::ProgressBar>();
        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        pb->setSize(27, 200);
        pb->setMargin(verticalMargin);
        pb->setVertical();
        pb->setProgress(0.25f);
        pb->setAlignment(Alignment::Top | Alignment::CenterH);
    }
    {
        controls::ProgressBar *pb=row->newChild<controls::ProgressBar>();
        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        pb->setSize(27, 200);
        pb->setMargin(verticalMargin);
        pb->setVertical();
        pb->setProgress(0.40f);
    }
    {
        controls::ProgressBar *pb=row->newChild<controls::ProgressBar>();
        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        pb->setSize(27, 200);
        pb->setMargin(verticalMargin);
        pb->setVertical();
        pb->setAlignment(Alignment::Bottom | Alignment::CenterH);
        pb->setProgress(0.65f);
    }
    {
        controls::ProgressBar *pb=row->newChild<controls::ProgressBar>();
        pb->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
        pb->setSize(27, 200);
        pb->setMargin(verticalMargin);
        pb->setVertical();
        pb->setProgress(0.80f);
        pb->setIndeterminate(true);
        m_indeterminate.push_back(pb);
    }

    // at the foot of the column; Shrink fits it to its caption, within the
    // column's width (a Button starts 100 wide)
    controls::Button *toggle=column->newChild<controls::Button>();

    toggle->setWidth(200);
    toggle->setText("Stop / start the indeterminate bars");
    toggle->onPressCaller.add(this, &ProgressBarDemo::onToggleIndeterminate);
}

void ProgressBarDemo::onToggleIndeterminate(event::Info info)
{
    for(controls::ProgressBar *pb : m_indeterminate)
        pb->setIndeterminate(!pb->isIndeterminate());
}

}//namespace gweni
