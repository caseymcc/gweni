/*
 *  Gweni - events and the call boundary (python/src).
 *
 *  An event method (btn.on_press(fn), or @btn.on_press) adds a C++ wrapper of
 *  the callable to the control's event::Caller with the control as its
 *  Handler, so the callback goes with the control. The wrapper holds the
 *  callable through a Callable, whose last copy releases it with the GIL held,
 *  and not at all once the interpreter is finalizing (the atexit hook has
 *  closed a live App or Headless before then, so nothing is left to leak).
 *
 *  A callback runs with the GIL taken and inside the session's callback depth:
 *  an exception it raises is caught here and never unwinds through gweni. It
 *  is kept, and the outermost call into the module (a CallScope: a Headless
 *  driving method, run(), a setter) raises it when it ends, after the event
 *  or frame has finished: the first one, each later one put at the end of its
 *  __context__ chain. The driving methods are refused inside a callback
 *  (checkNotInCallback), so a frame never runs inside a frame.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */
#ifndef _gweni_python_events_h_
#define _gweni_python_events_h_

#include "control.h"

#include <gweni/events.h>

#include <nanobind/nanobind.h>

#include <memory>
#include <utility>

namespace gweni
{
namespace python
{

//! A Python callable shared by the copies of a C++ callback.
class Callable
{
public:
    explicit Callable(nanobind::object function): m_function(std::move(function)) {}
    ~Callable();

    Callable(const Callable &)=delete;
    Callable &operator=(const Callable &)=delete;

    const nanobind::object &function() const { return m_function; }

private:
    nanobind::object m_function;
};

using CallablePtr=std::shared_ptr<Callable>;

//! Calls \p callable(args...) as a callback: the GIL taken, the callback depth
//! raised, and an exception kept for the outermost CallScope.
template<typename... Args>
void callBack(const Callable &callable, Args &&... args);

//! The Python interpreter is finalizing: callables are no longer released.
bool finalizing();
void setFinalizing();

//! Keeps the exception being handled (the error indicator) for the outermost
//! CallScope; called with the GIL held.
void keepCallbackError();
//! Whether a callback is running now.
bool inCallback();
void enterCallback();
void leaveCallback();

//! A Headless driving method, run() or frame() called from a callback.
void checkNotInCallback(const char *what);

//! One call into the module that may run callbacks. The outermost one raises
//! what they raised once its work is done (finish()); one left by an
//! exception of its own drops them.
class CallScope
{
public:
    CallScope();
    ~CallScope();

    CallScope(const CallScope &)=delete;
    CallScope &operator=(const CallScope &)=delete;

    //! Raises the kept callback errors, if this is the outermost scope.
    void finish();

private:
    bool m_finished=false;
};

//! Runs \p work in a CallScope and returns what it returns.
template<typename Work>
auto inCall(Work &&work)
{
    CallScope scope;

    if constexpr(std::is_void_v<decltype(work())>)
    {
        work();
        scope.finish();
    }
    else
    {
        auto result=work();
        scope.finish();
        return result;
    }
}

//! What a callback gets: event::Info as Python sees it.
struct Event
{
    nanobind::object control;
    std::string string;
    std::pair<int, int> point;
    size_t integer=0;
};

Event toEvent(const event::Info &info);

//! Binds \p name (on_press, ...) on \p cls: a method taking a callable of one
//! Event, adding it to the control's \p caller, and returning it, so it is a
//! decorator too.
template<typename Class, typename T>
void bindEvent(Class &cls, const char *name, event::Caller T::*caller, const char *doc)
{
    cls.def(name, [caller](const Control &self, nanobind::object function) {
            T *control=resolve<T>(self);
            CallablePtr callable=std::make_shared<Callable>(function);

            (control->*caller).addCallback(control, [callable](event::Handler &, event::Info info) {
                    callBack(*callable, toEvent(info));
                });
            return function;
        }, nanobind::arg("callback"), doc);
}

void bindEvents(nanobind::module_ &m);

template<typename... Args>
void callBack(const Callable &callable, Args &&... args)
{
    nanobind::gil_scoped_acquire gil;

    if(finalizing())
        return;
    enterCallback();
    try
    {
        callable.function()(std::forward<Args>(args)...);
    }
    catch(nanobind::python_error &error)
    {
        error.restore();
        keepCallbackError();
    }
    catch(const std::exception &error)
    {
        // a C++ error out of the call itself (a cast): a RuntimeError, kept
        // the same way, never let through gweni
        PyErr_SetString(PyExc_RuntimeError, error.what());
        keepCallbackError();
    }
    catch(...)
    {
        PyErr_SetString(PyExc_RuntimeError, "gweni: unknown C++ exception in a callback");
        keepCallbackError();
    }
    leaveCallback();
}

}//namespace python
}//namespace gweni

#endif//_gweni_python_events_h_
