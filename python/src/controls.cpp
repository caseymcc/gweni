/*
 *  Gweni - the bound control classes (python/src): Canvas, Panel (a plain
 *  controls::Base), Label and Button.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "handles.h"

#include <gweni/controls/button.h>
#include <gweni/controls/canvas.h>
#include <gweni/controls/label.h>
#include <gweni/postQueue.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

namespace nb=nanobind;

namespace gweni
{
namespace python
{


void bindControls(nb::module_ &m)
{
    auto canvas=nb::class_<Canvas, Control>(m, "Canvas",
        "The root of the tree (controls::Canvas): an App's or Headless's canvas.")
        .def("post", [](const Control &self, nb::typed<nb::callable, nb::object()> function) {
                // the one call allowed from any thread: no lookup in the tree
                PostTarget target=postTarget();

                if(!target.queue || target.canvasId != self.id)
                    return false;

                CallablePtr callable=std::make_shared<Callable>(function);

                return target.queue->post([callable]() { callBack(*callable); });
            }, nb::arg("callback"),
            "Runs callback() on the UI thread at the top of the next update, from any\n"
            "thread, waking an idle App. False, dropping it, once the canvas is gone.");
    // Canvas names itself in getTypeName() alone
    registerControl<Canvas, controls::Canvas, false>(canvas, "Canvas");

    auto panel=nb::class_<Panel, Control>(m, "Panel",
        "A plain container (controls::Base) that draws nothing.");
    registerControl<Panel, controls::Base>(panel);

    auto label=nb::class_<Label, Control>(m, "Label", "A line of text (controls::Label).")
        .def_prop_rw("text",
            [](const Control &self) { return resolve<controls::Label>(self)->getText(); },
            [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::Label>(self)->setText(text); });
            },
            "Its text.");
    registerControl<Label, controls::Label>(label);

    auto button=nb::class_<Button, Label>(m, "Button", "A push button (controls::Button).")
        .def_prop_rw("is_toggle",
            [](const Control &self) { return resolve<controls::Button>(self)->isToggle(); },
            [](const Control &self, bool toggle) { resolve<controls::Button>(self)->setIsToggle(toggle); },
            "Whether it stays down until pressed again.")
        .def_prop_rw("toggled",
            [](const Control &self) { return resolve<controls::Button>(self)->getToggleState(); },
            [](const Control &self, bool on) {
                inCall([&]() { resolve<controls::Button>(self)->setToggleState(on); });
            },
            "A toggle button's state; setting it fires on_toggle and on_toggle_on/off.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_press", &controls::Button::onPressCaller,
        "Pressed (clicked, or Space).");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_right_press",
        &controls::Button::onRightPressCaller, "Pressed with the right button.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_down", &controls::Button::onDownCaller,
        "The button went down.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_up", &controls::Button::onUpCaller,
        "The button came up.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_double_click",
        &controls::Button::onDoubleClickCaller, "Double-clicked.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_toggle", &controls::Button::onToggleCaller,
        "A toggle button changed state.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_toggle_on",
        &controls::Button::onToggleOnCaller, "A toggle button turned on.");
    bindEvent<nb::class_<Button, Label>, controls::Button>(button, "on_toggle_off",
        &controls::Button::onToggleOffCaller, "A toggle button turned off.");
    registerControl<Button, controls::Button>(button);
}

}//namespace python
}//namespace gweni
