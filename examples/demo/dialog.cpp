#include "demo/dialog.h"
#include "gweni/controls/button.h"
#include "gweni/controls/canvas.h"
#include "gweni/controls/dialogs/query.h"
#include "gweni/controls/label.h"
#include "gweni/controls/logView.h"
#include "gweni/controls/scrollControl.h"
#include "gweni/controls/textBox.h"

namespace gweni
{

namespace
{
const char *resultName(dialogs::Result result)
{
    switch(result)
    {
    case dialogs::Result::Ok: return "Ok";
    case dialogs::Result::Yes: return "Yes";
    case dialogs::Result::No: return "No";
    case dialogs::Result::Cancel: return "Cancel";
    }
    return "?";
}

controls::Button *columnButton(controls::Base *parent, const String &text, int y)
{
    controls::Button *button=parent->newChild<controls::Button>();

    button->setText(text);
    button->setPos(10, y);
    // room for the longest label; each takes its own text's width
    button->setWidth(200);
    return button;
}
}

// Four buttons in a column, each opening one of the standard dialogs, and the
// result of the last one closed below them.
GWENI_CONTROL_CONSTRUCTOR(DialogDemo)
{
    setDock(DockPosition::Center);

    m_message=columnButton(this, "Message", 10);
    m_message->onPressCaller.add(this, &DialogDemo::onMessage);
    m_yesNo=columnButton(this, "Confirm (Yes / No)", 34);
    m_yesNo->onPressCaller.add(this, &DialogDemo::onYesNo);
    m_okCancel=columnButton(this, "Confirm (OK / Cancel)", 58);
    m_okCancel->onPressCaller.add(this, &DialogDemo::onOkCancel);
    m_prompt=columnButton(this, "Prompt", 82);
    m_prompt->onPressCaller.add(this, &DialogDemo::onPrompt);

    m_result=newChild<controls::Label>();
    m_result->setText("Result: none yet");
    m_result->setPos(10, 112);
}

dialogs::ResultHandler DialogDemo::show(const String &what)
{
    // the demo may be gone by the time the dialog closes
    const size_t id=getControlId();

    return [id, what](dialogs::Result result, const String &text)
    {
        if(DialogDemo *demo=gweni_cast<DialogDemo>(controls::Base::getControlById(id)))
            demo->m_result->setText("Result: "+what+" "+resultName(result)+(text.empty() ? "" : " \""+text+"\""));
    };
}

void DialogDemo::onMessage(event::Info)
{
    dialogs::message(getCanvas(), "Saved", "The task's settings were saved.", show("message"));
}

void DialogDemo::onYesNo(event::Info)
{
    dialogs::confirm(getCanvas(), "Delete task", "Delete task #42 and its 3 runs? This cannot be undone.",
        dialogs::Buttons::YesNo, show("yes/no"));
}

void DialogDemo::onOkCancel(event::Info)
{
    dialogs::confirm(getCanvas(), "Release lock", "Release the gpu lock held by run #2686?",
        dialogs::Buttons::OkCancel, show("ok/cancel"));
}

void DialogDemo::onPrompt(event::Info)
{
    dialogs::prompt(getCanvas(), "Rename", "The task's new title:", "Add standard dialogs", show("prompt"));
}


GWENI_CONTROL_CONSTRUCTOR(LargeDialogDemo)
{
    setDock(DockPosition::Center);

    m_open=columnButton(this, "Open task", 10);
    m_open->onPressCaller.add(this, &LargeDialogDemo::onOpen);

    m_behind=newChild<controls::Label>();
    m_behind->setText("Ticks behind the dialog: 0");
    m_behind->setPos(10, 40);
}

dialogs::Dialog *LargeDialogDemo::dialog() const
{
    return m_dialogId ? gweni_cast<dialogs::Dialog>(controls::Base::getControlById(m_dialogId)) : nullptr;
}

void LargeDialogDemo::onOpen(event::Info)
{
    controls::Canvas *canvas=getCanvas();

    if(!canvas || dialog())
        return;

    dialogs::Dialog *task=canvas->newChild<dialogs::Dialog>();

    task->setTitle("Task #42: add standard dialogs");
    task->setDialogSize(dialogs::Extent::units(1200), dialogs::Extent::fraction(0.92f));

    controls::ScrollControl *content=task->getContent();

    m_inside=content->newChild<controls::Label>();
    m_inside->setDock(DockPosition::Top);
    m_inside->setMargin(Margin(4, 4, 4, 4));
    m_inside->setText("Ticks inside the dialog: "+std::to_string(m_ticks));

    for(const char *field : {"Title", "Branch", "Areas"})
    {
        controls::Label *label=content->newChild<controls::Label>();

        label->setDock(DockPosition::Top);
        label->setMargin(Margin(4, 4, 4, 0));
        label->setText(field);

        controls::TextBox *box=content->newChild<controls::TextBox>();

        box->setDock(DockPosition::Top);
        box->setMargin(Margin(4, 2, 4, 4));
        box->setSizeFlags({SizeFlag::Expand, box->getSizeFlags().vertical});
        box->setText(String(field) == "Title" ? "Add standard dialogs" :
            String(field) == "Branch" ? "mm/607-add-standard-dialogs" : "canvas, popup-menu");
    }

    m_log=content->newChild<controls::LogView>();
    m_log->setDock(DockPosition::Top);
    m_log->setMargin(Margin(4, 4, 4, 4));
    m_log->setSizeFlags({SizeFlag::Expand, SizeFlag::Fixed});
    m_log->setHeight(m_log->heightForRows(12));
    m_log->append(controls::LogRow("run #2686 started on localworker"));

    m_forceRelease=task->addButton("Force release");
    m_forceRelease->onPressCaller.add(this, &LargeDialogDemo::onForceRelease);
    task->setDefaultButton(task->addButton("Close", dialogs::Result::Ok));

    // the timer stops with the dialog, whatever closes it
    const size_t id=getControlId();

    task->setResultHandler([id](dialogs::Result, const String &)
    {
        if(LargeDialogDemo *demo=gweni_cast<LargeDialogDemo>(controls::Base::getControlById(id)))
        {
            if(controls::Canvas *canvas=demo->getCanvas())
                canvas->cancel(demo->m_timer);
            demo->m_timer=controls::TimerHandle();
            demo->m_dialogId=0;
            demo->m_inside=nullptr;
            demo->m_log=nullptr;
            demo->m_forceRelease=nullptr;
        }
    });

    m_dialogId=task->getControlId();
    m_timer=canvas->every(1.0, [this]() { tick(); }, this);
    task->open();
}

void LargeDialogDemo::onForceRelease(event::Info)
{
    const size_t id=getControlId();

    dialogs::confirm(getCanvas(), "Force release", "Force-release the gpu lock held by run #2686?",
        dialogs::Buttons::YesNo, [id](dialogs::Result result, const String &)
    {
        LargeDialogDemo *demo=gweni_cast<LargeDialogDemo>(controls::Base::getControlById(id));

        if(demo && demo->m_log && (result == dialogs::Result::Yes))
            demo->m_log->append(controls::LogRow("gpu lock force-released"));
    });
}

void LargeDialogDemo::tick()
{
    ++m_ticks;
    m_behind->setText("Ticks behind the dialog: "+std::to_string(m_ticks));

    if(dialog())
    {
        m_inside->setText("Ticks inside the dialog: "+std::to_string(m_ticks));
        m_log->append(controls::LogRow("tick "+std::to_string(m_ticks)+": 3 of 5 agents busy"));
    }
}

}//namespace gweni
