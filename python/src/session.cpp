/*
 *  Gweni - the Python module's session (session.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "session.h"

#include <gweni/controls/canvas.h>
#include <gweni/postQueue.h>

#include <nanobind/stl/string.h>

#include <cmath>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

Session &session()
{
    static Session s;
    return s;
}

namespace
{
std::mutex postMutex;
PostTarget postTargetNow;
}

void beginSession(const char *kind, controls::Canvas *canvas, std::function<void()> close)
{
    Session &s=session();

    s.active=true;
    s.kind=kind;
    s.thread=std::this_thread::get_id();
    s.canvas=canvas;
    s.close=std::move(close);

    std::lock_guard<std::mutex> lock(postMutex);

    postTargetNow.queue=canvas->postHandle();
    postTargetNow.canvasId=canvas->getControlId();
}

void endSession()
{
    {
        std::lock_guard<std::mutex> lock(postMutex);

        postTargetNow=PostTarget();
    }
    session()=Session();
}

PostTarget postTarget()
{
    std::lock_guard<std::mutex> lock(postMutex);

    return postTargetNow;
}

std::string packageResources()
{
    nb::object files=nb::module_::import_("importlib.resources").attr("files")("gweni");

    return nb::cast<std::string>(nb::str(files.attr("joinpath")("resources")));
}

void checkCanvasSize(const char *who, const std::pair<int, int> &size, float scale)
{
    if((size.first <= 0) || (size.second <= 0))
        throw nb::value_error((std::string("gweni.")+who+": size must be positive in both axes, not ("
            +std::to_string(size.first)+", "+std::to_string(size.second)+")").c_str());
    if(!std::isfinite(scale) || (scale <= 0.0f))
        throw nb::value_error((std::string("gweni.")+who+": scale must be a positive number, not "
            +std::to_string(scale)).c_str());
}

void bindSession(nb::module_ &m)
{
    nb::exception<ControlDeleted>(m, "ControlDeletedError", PyExc_ReferenceError);
    nb::exception<WrongThread>(m, "ThreadError", PyExc_RuntimeError);
}

}//namespace python
}//namespace gweni
