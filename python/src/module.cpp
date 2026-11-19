/*
 *  Gweni - the Python module gweni's extension, _gweni (python/src).
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "session.h"

namespace nb=nanobind;

namespace gweni
{
namespace python
{
void bindControls(nb::module_ &m);
void bindEvents(nb::module_ &m);
void bindHeadless(nb::module_ &m);
void bindApp(nb::module_ &m);
}
}

NB_MODULE(_gweni, m)
{
    m.doc()="gweni, a retained-mode GUI library: the extension the gweni package wraps.";
    gweni::python::bindSession(m);
    gweni::python::bindControl(m);
    gweni::python::bindEvents(m);
    gweni::python::bindControls(m);
    gweni::python::bindHeadless(m);
    gweni::python::bindApp(m);
}
