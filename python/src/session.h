/*
 *  Gweni - the Python module's session (python/src): the one App or Headless
 *  alive in the process, the thread that made it and its canvas, and the
 *  errors the module raises for the rules it keeps.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_python_session_h_
#define _gweni_python_session_h_

#include <nanobind/nanobind.h>

#include <functional>
#include <memory>
#include <mutex>
#include <stdexcept>
#include <string>
#include <thread>
#include <utility>
#include <unordered_set>

namespace gweni
{
class PostQueue;

namespace controls
{
class Canvas;
}

namespace python
{

//! gweni.ControlDeletedError: a handle whose control is gone
struct ControlDeleted: std::runtime_error
{
    using std::runtime_error::runtime_error;
};

//! gweni.ThreadError: the tree touched off the thread that made the App or
//! Headless
struct WrongThread: std::runtime_error
{
    using std::runtime_error::runtime_error;
};

//! The App or Headless alive now, if any (gweni_app keeps one at a time).
struct Session
{
    bool active=false;
    //! "Headless" or "App", for the messages
    const char *kind="";
    std::thread::id thread;
    controls::Canvas *canvas=nullptr;
    //! closes the App or Headless whatever the thread (the atexit hook)
    std::function<void()> close;
    //! the controls delete_later() may delete: those add() made and the
    //! dialogs message(), confirm() and prompt() opened. Any other is a part
    //! its owner keeps a pointer to (a CheckBoxWithLabel's box, a TabButton's
    //! page, a list's items), which delete_later() refuses.
    std::unordered_set<size_t> deletable;
};

Session &session();

//! Starts the session of a Headless or App on this thread; \p close is how
//! the atexit hook closes it.
void beginSession(const char *kind, controls::Canvas *canvas, std::function<void()> close);
void endSession();

//! The session canvas's post queue and id, for canvas.post() from any thread
//! (read under a lock of their own: the rest of Session is the UI thread's).
struct PostTarget
{
    std::shared_ptr<PostQueue> queue;
    size_t canvasId=0;
};
PostTarget postTarget();

//! Raises ThreadError when a session is alive and this is not its thread:
//! the one compare every call into the tree makes.
inline void checkThread()
{
    const Session &s=session();

    if(s.active && s.thread != std::this_thread::get_id())
        throw WrongThread(std::string("gweni: the control tree is used off the thread that made the ")
            +s.kind+" (only canvas.post() may be called from another thread)");
}

//! The package's resources/ (importlib.resources), for an App or Headless
//! given none.
std::string packageResources();

//! Raises ValueError unless \p size is positive in both axes and \p scale
//! finite and positive, before an App or Headless (\p who) opens with them.
void checkCanvasSize(const char *who, const std::pair<int, int> &size, float scale);

void bindSession(nanobind::module_ &m);

}//namespace python
}//namespace gweni

#endif//_gweni_python_session_h_
