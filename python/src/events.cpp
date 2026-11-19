/*
 *  Gweni - events and the call boundary (events.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "events.h"

#include <gweni/controls/canvas.h>
#include <gweni/postQueue.h>

#include <nanobind/stl/optional.h>
#include <nanobind/stl/pair.h>
#include <nanobind/stl/string.h>

#include <atomic>
#include <deque>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

namespace
{

std::atomic<bool> interpreterFinalizing{false};
//! the callbacks running now (the UI thread's, under the GIL)
int callbackDepth=0;
//! the CallScopes open now
int scopeDepth=0;
//! the first error a callback raised since the outermost scope began, the
//! later ones on its __context__ chain
nb::object firstError;

void dropErrors()
{
    firstError=nb::object();
}

}//namespace

Callable::~Callable()
{
    if(finalizing())
    {
        // the interpreter is going: the object is left as it is
        m_function.release();
        return;
    }

    nb::gil_scoped_acquire gil;

    m_function.reset();
}

bool finalizing()
{
    return interpreterFinalizing.load();
}

void setFinalizing()
{
    interpreterFinalizing=true;
}

bool callbackErrorPending()
{
    return firstError.is_valid();
}

bool inCallback()
{
    return callbackDepth > 0;
}

void enterCallback()
{
    ++callbackDepth;
}

void leaveCallback()
{
    --callbackDepth;
}

void checkNotInCallback(const char *what)
{
    if(inCallback())
        throw std::runtime_error(std::string("gweni: ")+what+" called from inside a callback");
}

void keepCallbackError()
{
    nb::object error=nb::steal(PyErr_GetRaisedException());

    if(!error.is_valid())
        return;
    if(scopeDepth == 0)
    {
        // no call into the module will raise it: report it as Python does an
        // error nothing can catch
        PyErr_SetRaisedException(error.release().ptr());
        PyErr_WriteUnraisable(nullptr);
        return;
    }
    if(!firstError.is_valid())
    {
        firstError=error;
        return;
    }

    // at the end of the first one's __context__ chain, once
    nb::object tail=firstError;

    for(;;)
    {
        if(tail.is(error))
            return;

        nb::object context=nb::steal(PyException_GetContext(tail.ptr()));

        if(!context.is_valid() || context.is_none())
            break;
        tail=context;
    }
    PyException_SetContext(tail.ptr(), error.release().ptr());
}

CallScope::CallScope()
{
    ++scopeDepth;
}

CallScope::~CallScope()
{
    if(m_finished)
        return;
    // left by an exception of its own, which is what the caller sees
    if(--scopeDepth == 0)
        dropErrors();
}

void CallScope::finish()
{
    m_finished=true;
    if(--scopeDepth > 0 || !firstError.is_valid())
        return;

    nb::object error=firstError;

    dropErrors();
    PyErr_SetRaisedException(error.release().ptr());
    throw nb::python_error();
}

Event toEvent(const event::Info &info)
{
    Event event;

    event.control=wrap(info.control != nullptr ? info.control : info.controlCaller);
    event.string=info.string;
    event.point=std::make_pair(info.point.x, info.point.y);
    event.integer=info.integer;
    return event;
}

const char *eventSignature(const char *name)
{
    static std::deque<std::string> signatures;

    signatures.push_back(std::string("def ")+name+"(self, callback: EventCallbackT, /) -> EventCallbackT");
    return signatures.back().c_str();
}

void callEvent(const Callable &callable, const event::Info &info, size_t controlId)
{
    nb::gil_scoped_acquire gil;

    if(finalizing())
        return;

    Event event=toEvent(info);

    if(controlId != 0)
        event.control=wrap(controls::Base::getControlById(controlId));
    callBack(callable, event);
}

void bindEvents(nb::module_ &m)
{
    nb::class_<Event>(m, "Event", "What an event callback gets (event::Info).")
        .def_prop_ro("control", [](const Event &event) { return nb::borrow<Typed<std::optional<Control>>>(event.control); },
            "The control the event is about, or None.")
        .def_ro("string", &Event::string, "The event's text, where it has one.")
        .def_ro("point", &Event::point, "The event's point, where it has one.")
        .def_ro("integer", &Event::integer, "The event's number, where it has one.")
        .def("__repr__", [](const Event &event) {
                return "<gweni.Event control="+nb::cast<std::string>(nb::repr(event.control))
                    +" string="+nb::cast<std::string>(nb::repr(nb::str(event.string.c_str())))
                    +" point=("+std::to_string(event.point.first)+", "+std::to_string(event.point.second)+")"
                    +" integer="+std::to_string(event.integer)+">";
            });

    // An App or Headless still open as the interpreter exits is closed first,
    // releasing every callable while Python can; after it, none is released.
    m.def("_close_at_exit", []() {
            Session &s=session();

            if(s.active && s.close)
            {
                std::function<void()> close=s.close;

                close();
            }
            setFinalizing();
        }, "The atexit hook: closes a live App or Headless.");
    nb::module_::import_("atexit").attr("register")(m.attr("_close_at_exit"));
}

}//namespace python
}//namespace gweni
