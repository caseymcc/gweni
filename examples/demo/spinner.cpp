/*
  * Gweni
  * Copyright (c) 2010 Facepunch Studios
  * See license in gweni.h
 */

#include "demo/spinner.h"
#include "gweni/controls/button.h"
#include "gweni/controls/groupBox.h"
#include "gweni/controls/label.h"
#include "gweni/controls/layout/layout.h"
#include "gweni/controls/spinner.h"

namespace gweni
{

namespace
{

// A section's caption, docked Top in the column.
void caption(controls::Base *column, const char *text)
{
    controls::Label *label=column->newChild<controls::Label>();

    label->setText(text);
    label->setDock(DockPosition::Top);
    label->setMargin(Margin(0, 8, 0, 4));
}

// A row of controls side by side, docked Top in the column.
controls::Base *line(controls::Base *column)
{
    controls::Base *row=column->newChild<controls::Base>();
    controls::HorizontalLayout *layout=new controls::HorizontalLayout();

    layout->setSpacing(16);
    row->setLayout(layout);
    row->setDock(DockPosition::Top);
    return row;
}

// The rest of a row's width, after its controls, so they stay packed.
void endLine(controls::Base *row)
{
    controls::Base *rest=row->newChild<controls::Base>();

    rest->setSizeFlags({SizeFlag::Bloat, SizeFlag::Fixed});
    rest->setHeight(1);
}

controls::Button *button(controls::Base *row, const char *text, int width)
{
    controls::Button *control=row->newChild<controls::Button>();

    control->setText(text);
    control->setSizeFlags({SizeFlag::Fixed, SizeFlag::Fixed});
    control->setSize(width, 24);
    return control;
}

}//namespace

GWENI_CONTROL_CONSTRUCTOR(SpinnerDemo)
{
    setDock(DockPosition::Center);

    controls::Base *column=newChild<controls::Base>();

    column->setSizeFlags({SizeFlag::Fixed, SizeFlag::Shrink});
    column->setPos(10, 4);
    column->setWidth(500);

    const controls::Spinner::Size sizes[]={controls::Spinner::Size::Small,
        controls::Spinner::Size::Medium, controls::Spinner::Size::Large};

    caption(column, "Small, medium and large");
    {
        controls::Base *row=line(column);

        for(controls::Spinner::Size size : sizes)
        {
            controls::Spinner *spinner=row->newChild<controls::Spinner>();

            spinner->setSpinnerSize(size);
            m_spinners.push_back(spinner);
        }
        endLine(row);
    }

    caption(column, "With a track");
    {
        controls::Base *row=line(column);

        for(controls::Spinner::Size size : sizes)
        {
            controls::Spinner *spinner=row->newChild<controls::Spinner>();

            spinner->setSpinnerSize(size);
            spinner->setShowTrack(true);
            m_spinners.push_back(spinner);
            m_hideTarget=spinner;
        }
        m_hide=button(row, "Hide spinner", 110);
        m_hide->setIsToggle(true);
        m_hide->onToggleCaller.add(this, &SpinnerDemo::onHide);
        endLine(row);
    }

    caption(column, "In a button, beside its text");
    {
        controls::Base *row=line(column);

        m_saving=button(row, "Saving\xE2\x80\xA6", 110);
        // the tooltip on the button: one on the spinner would take the
        // button's hover and press over it
        m_saving->setToolTipText("Saving your changes");
        m_inButton=m_saving->newChild<controls::Spinner>();
        m_spinners.push_back(m_inButton);

        // the spinner at the button's left, centred down it, the text after it
        const int diameter=m_inButton->getDiameter();

        m_inButton->setAlignment(Alignment::Left|Alignment::CenterV);
        m_inButton->setMargin(Margin(8, 0, 0, 0));
        m_saving->setTextPadding(Padding(8+diameter+6, 0, 4, 0));
        m_saving->setTextAlignment(Alignment::Left|Alignment::CenterV);
        endLine(row);
    }

    caption(column, "In a card");
    {
        m_cardBox=column->newChild<controls::GroupBox>();
        m_cardBox->setText("Loading report");
        m_cardBox->setDock(DockPosition::Top);
        m_cardBox->setHeight(120);

        controls::Base *row=line(m_cardBox);

        row->setMargin(Margin(0, 8, 0, 8));
        m_card=row->newChild<controls::Spinner>();
        m_card->setSpinnerSize(controls::Spinner::Size::Medium);
        m_card->setShowTrack(true);
        m_card->setHideWhenStopped(false);
        m_spinners.push_back(m_card);

        controls::Label *text=row->newChild<controls::Label>();

        text->setText("Fetching the latest numbers\xE2\x80\xA6");
        // centred down the spinner's height
        text->setTextAlignment(Alignment::Left|Alignment::CenterV);
        text->setSizeFlags({SizeFlag::Elastic, SizeFlag::Fixed});
        text->setHeight(controls::Spinner::MediumDiameter);
        endLine(row);

        controls::Base *buttons=line(m_cardBox);

        m_start=button(buttons, "Start", 70);
        m_start->onPressCaller.add(this, &SpinnerDemo::onStart);
        m_stop=button(buttons, "Stop", 70);
        m_stop->onPressCaller.add(this, &SpinnerDemo::onStop);
        endLine(buttons);
    }
}

void SpinnerDemo::onStart(event::Info /*info*/)
{
    m_card->start();
}

void SpinnerDemo::onStop(event::Info /*info*/)
{
    m_card->stop();
}

void SpinnerDemo::onHide(event::Info /*info*/)
{
    m_hideTarget->setHidden(m_hide->getToggleState());
    m_hide->setText(m_hide->getToggleState()?"Show spinner":"Hide spinner");
}

}//namespace gweni
