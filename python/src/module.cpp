/*
 *  Gweni - the Python module gweni's extension, _gweni (python/src).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "session.h"

#include <nanobind/typing.h>

namespace nb=nanobind;

namespace gweni
{
namespace python
{
void bindControls(nb::module_ &m);
void bindEvents(nb::module_ &m);
void bindHeadless(nb::module_ &m);
void bindApp(nb::module_ &m);
void bindSimpleControls(nb::module_ &m);
void bindContainers(nb::module_ &m);
void bindNotify(nb::module_ &m);
}
}

NB_MODULE(_gweni, m)
{
    m.doc()="gweni, a retained-mode GUI library: the extension the gweni package wraps.";
    gweni::python::bindSession(m);
    gweni::python::bindControl(m);
    gweni::python::bindEvents(m);
    gweni::python::bindControls(m);
    gweni::python::bindContainers(m);
    gweni::python::bindSimpleControls(m);
    gweni::python::bindNotify(m);

    // the TypeVars the stubs' signatures name (nb::sig): add() gives back the
    // class it was given, an event method the callable it was given
    nb::list eventArgs;

    eventArgs.append(m.attr("Event"));

    nb::object callableOfEvent=nb::module_::import_("collections.abc").attr("Callable")[nb::make_tuple(
        eventArgs, nb::module_::import_("builtins").attr("object"))];

    m.attr("ControlT")=nb::type_var("ControlT", nb::arg("bound")=m.attr("Control"));
    m.attr("EventCallbackT")=nb::type_var("EventCallbackT", nb::arg("bound")=callableOfEvent);
    gweni::python::bindHeadless(m);
    gweni::python::bindApp(m);
}
