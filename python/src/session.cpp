/*
 *  Gweni - the Python module's session (session.h).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "session.h"

#include <nanobind/stl/string.h>

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

void beginSession(const char *kind, controls::Canvas *canvas)
{
    Session &s=session();

    s.active=true;
    s.kind=kind;
    s.thread=std::this_thread::get_id();
    s.canvas=canvas;
}

void endSession()
{
    session()=Session();
}

std::string packageResources()
{
    nb::object files=nb::module_::import_("importlib.resources").attr("files")("gweni");

    return nb::cast<std::string>(nb::str(files.attr("joinpath")("resources")));
}

void bindSession(nb::module_ &m)
{
    nb::exception<ControlDeleted>(m, "ControlDeletedError", PyExc_ReferenceError);
    nb::exception<WrongThread>(m, "ThreadError", PyExc_RuntimeError);
}

}//namespace python
}//namespace gweni
