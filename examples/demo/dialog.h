#ifndef _gweni_dialog_example_h_
#define _gweni_dialog_example_h_

#include <gweni/controls/base.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/dialogs/dialog.h>

namespace gweni
{
namespace controls
{
class Button;
class Label;
class LogView;
}

//! The standard dialogs (dialogs::message, confirm, prompt): a button that
//! opens each, and a Label showing the result its handler got.
class DialogDemo: public controls::Base
{
public:
    GWENI_CONTROL(DialogDemo, controls::Base);

public:
    void onMessage(event::Info);
    void onYesNo(event::Info);
    void onOkCancel(event::Info);
    void onPrompt(event::Info);

    //! The handler each dialog gets: writes "<what>: <result> <text>".
    dialogs::ResultHandler show(const String &what);

    controls::Button *m_message=nullptr;
    controls::Button *m_yesNo=nullptr;
    controls::Button *m_okCancel=nullptr;
    controls::Button *m_prompt=nullptr;
    controls::Label *m_result=nullptr;
};

//! A large dialog over a page that keeps changing, as a dashboard's task popup
//! over its task list: "Open task" opens a Dialog 1200 units wide and 0.92 of
//! the canvas high (both clamped to the canvas) holding a form and a log. While
//! it is open a canvas timer ticks once a second (on the canvas clock, so a
//! frozen clock ticks never; no timer runs while it is closed), counting in a
//! Label behind the dialog and in one inside it, and appending a log row. Its
//! "Force release" button opens a confirm modal to it, whose Yes logs it; its
//! "Close" button (the default, so Enter) and Escape close it. The timer is
//! this page's, so deleting the page stops it; the dialog is the canvas's and
//! stays until it is closed.
class LargeDialogDemo: public controls::Base
{
public:
    GWENI_CONTROL(LargeDialogDemo, controls::Base);

public:
    void onOpen(event::Info);
    void onForceRelease(event::Info);
    //! One tick: both counters and a log row.
    void tick();

    //! The open dialog, or null (looked up by id: it deletes itself on close).
    dialogs::Dialog *dialog() const;

    controls::Button *m_open=nullptr;
    controls::Label *m_behind=nullptr;
    //! inside the dialog, valid while dialog() is not null
    controls::Label *m_inside=nullptr;
    controls::LogView *m_log=nullptr;
    controls::Button *m_forceRelease=nullptr;

    size_t m_dialogId=0;
    controls::TimerHandle m_timer;
    int m_ticks=0;
};

}//namespace gweni

#endif//_gweni_dialog_example_h_
