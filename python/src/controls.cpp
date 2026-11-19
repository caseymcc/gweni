/*
 *  Gweni - the bound control classes (python/src): Canvas, Panel (a plain
 *  controls::Base), Label and Button.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>

#include <nanobind/stl/string.h>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

struct Canvas: Control {};
struct Panel: Control {};
struct Label: Control {};
struct Button: Label {};

void bindControls(nb::module_ &m)
{
    auto canvas=nb::class_<Canvas, Control>(m, "Canvas",
        "The root of the tree (controls::Canvas): an App's or Headless's canvas.");
    // Canvas names itself in getTypeName() alone
    registerControl<Canvas, controls::Canvas, false>(canvas, "Canvas");

    auto panel=nb::class_<Panel, Control>(m, "Panel",
        "A plain container (controls::Base) that draws nothing.");
    registerControl<Panel, controls::Base>(panel);

    auto label=nb::class_<Label, Control>(m, "Label", "A line of text (controls::Label).")
        .def_prop_rw("text",
            [](const Control &self) { return resolve<controls::Label>(self)->getText(); },
            [](const Control &self, const std::string &text) { resolve<controls::Label>(self)->setText(text); },
            "Its text.");
    registerControl<Label, controls::Label>(label);

    auto button=nb::class_<Button, Label>(m, "Button", "A push button (controls::Button).");
    registerControl<Button, controls::Button>(button);
}

}//namespace python
}//namespace gweni
