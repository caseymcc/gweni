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

#include <stdexcept>
#include <string>
#include <thread>

namespace gweni
{
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
};

Session &session();

//! Starts the session of a Headless or App on this thread.
void beginSession(const char *kind, controls::Canvas *canvas);
void endSession();

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

void bindSession(nanobind::module_ &m);

}//namespace python
}//namespace gweni

#endif//_gweni_python_session_h_
