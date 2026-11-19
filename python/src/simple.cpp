/*
 *  Gweni - the simple controls (python/src): CheckBox, CheckBoxWithLabel,
 *  RadioButtonController and its RadioButtons, the text boxes, the sliders,
 *  ComboBox, ListBox, ProgressBar, Switch, Spinner and Chip, each with its
 *  state as properties and its events. A setter that can fire an event runs
 *  in a CallScope (inCall), so what a callback raises is raised by it.
 *
 *  Copyright (c) 2010 Facepunch Studios
 *  MIT License, see LICENSE
 */

#include "control.h"
#include "events.h"
#include "handles.h"

#include <gweni/controls/checkBox.h>
#include <gweni/controls/chip.h>
#include <gweni/controls/comboBox.h>
#include <gweni/controls/horizontalSlider.h>
#include <gweni/controls/label.h>
#include <gweni/controls/layout/table.h>
#include <gweni/controls/listBox.h>
#include <gweni/controls/menuItem.h>
#include <gweni/controls/progressBar.h>
#include <gweni/controls/radioButton.h>
#include <gweni/controls/radioButtonController.h>
#include <gweni/controls/spinner.h>
#include <gweni/controls/switch.h>
#include <gweni/controls/textBox.h>
#include <gweni/controls/verticalSlider.h>

#include <nanobind/stl/string.h>
#include <nanobind/stl/tuple.h>

namespace nb=nanobind;

namespace gweni
{
namespace python
{

void bindSimpleControls(nb::module_ &m)
{
    // CheckBox
    auto checkBox=nb::class_<CheckBox, Button>(m, "CheckBox", "A box that is checked or not (controls::CheckBox).")
        .def_prop_rw("checked",
            [](const Control &self) { return resolve<controls::CheckBox>(self)->isChecked(); },
            [](const Control &self, bool checked) {
                inCall([&]() { resolve<controls::CheckBox>(self)->setChecked(checked); });
            }, "Whether it is checked; setting it fires on_check_changed and on_checked/on_unchecked.");
    bindEvent<decltype(checkBox), controls::CheckBox>(checkBox, "on_checked", &controls::CheckBox::onChecked,
        "It was checked.");
    bindEvent<decltype(checkBox), controls::CheckBox>(checkBox, "on_unchecked", &controls::CheckBox::onUnChecked,
        "It was unchecked.");
    bindEvent<decltype(checkBox), controls::CheckBox>(checkBox, "on_check_changed",
        &controls::CheckBox::onCheckChanged, "It was checked or unchecked.");
    registerControl<CheckBox, controls::CheckBox>(checkBox);

    auto checkBoxWithLabel=nb::class_<CheckBoxWithLabel, Control>(m, "CheckBoxWithLabel",
        "A CheckBox with a clickable label beside it (controls::CheckBoxWithLabel).")
        .def_prop_rw("text",
            [](const Control &self) { return resolve<controls::CheckBoxWithLabel>(self)->label()->getText(); },
            [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::CheckBoxWithLabel>(self)->label()->setText(text); });
            }, "The label's text.")
        .def_prop_rw("checked",
            [](const Control &self) { return resolve<controls::CheckBoxWithLabel>(self)->checkBox()->isChecked(); },
            [](const Control &self, bool checked) {
                inCall([&]() { resolve<controls::CheckBoxWithLabel>(self)->checkBox()->setChecked(checked); });
            }, "Whether its box is checked.")
        .def_prop_rw("label_on_left",
            [](const Control &self) {
                return resolve<controls::CheckBoxWithLabel>(self)->checkBox()->getDock() == DockPosition::Right;
            },
            [](const Control &self, bool left) { resolve<controls::CheckBoxWithLabel>(self)->setLabelOnLeft(left); },
            "The text on the left of the box (default: right).")
        .def_prop_ro("check_box", [](const Control &self) {
                return wrap(resolve<controls::CheckBoxWithLabel>(self)->checkBox());
            }, "Its CheckBox.");
    auto boxOf=[](controls::CheckBoxWithLabel *c) -> event::Caller & { return c->checkBox()->onCheckChanged; };
    bindPartEvent<decltype(checkBoxWithLabel), controls::CheckBoxWithLabel>(checkBoxWithLabel, "on_check_changed",
        boxOf, "Its box was checked or unchecked (the event's control is the row).");
    bindPartEvent<decltype(checkBoxWithLabel), controls::CheckBoxWithLabel>(checkBoxWithLabel, "on_checked",
        [](controls::CheckBoxWithLabel *c) -> event::Caller & { return c->checkBox()->onChecked; },
        "Its box was checked.");
    bindPartEvent<decltype(checkBoxWithLabel), controls::CheckBoxWithLabel>(checkBoxWithLabel, "on_unchecked",
        [](controls::CheckBoxWithLabel *c) -> event::Caller & { return c->checkBox()->onUnChecked; },
        "Its box was unchecked.");
    registerControl<CheckBoxWithLabel, controls::CheckBoxWithLabel>(checkBoxWithLabel);

    // RadioButtonController and its rows
    auto radioButton=nb::class_<RadioButton, Control>(m, "RadioButton",
        "One option of a RadioButtonController (controls::LabeledRadioButton): made by\n"
        "the controller's add_option().")
        .def_prop_rw("text",
            [](const Control &self) { return resolve<controls::LabeledRadioButton>(self)->getText(); },
            [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::LabeledRadioButton>(self)->setText(text); });
            }, "Its label.")
        .def_prop_ro("selected", [](const Control &self) {
                return resolve<controls::LabeledRadioButton>(self)->getRadioButton()->isChecked();
            }, "Whether it is the controller's selected option.")
        .def("select", [](const Control &self) {
                inCall([&]() { resolve<controls::LabeledRadioButton>(self)->select(); });
            }, "Makes it the selected option (fires the controller's on_selection_change).");
    registerControl<RadioButton, controls::LabeledRadioButton, false>(radioButton);

    auto radioController=nb::class_<RadioButtonController, Control>(m, "RadioButtonController",
        "A group of RadioButtons of which one at most is selected (controls::RadioButtonController).")
        .def("add_option", [](const Control &self, const std::string &text, const std::string &name) {
                return inCall([&]() {
                    return wrap(resolve<controls::RadioButtonController>(self)->addOption(text, name));
                });
            }, nb::arg("text"), nb::arg("name")="", "Adds an option; returns its RadioButton.")
        .def_prop_ro("selected", [](const Control &self) {
                return wrap(resolve<controls::RadioButtonController>(self)->getSelected());
            }, "The selected RadioButton, or None.")
        .def_prop_ro("selected_name", [](const Control &self) {
                controls::LabeledRadioButton *selected=resolve<controls::RadioButtonController>(self)->getSelected();
                return selected ? selected->getName() : std::string();
            }, "The selected option's name (\"\" with none).");
    bindEvent<decltype(radioController), controls::RadioButtonController>(radioController, "on_selection_change",
        &controls::RadioButtonController::onSelectionChange, "Another option was selected.");
    registerControl<RadioButtonController, controls::RadioButtonController>(radioController);

    // the text boxes
    auto textBox=nb::class_<TextBox, Label>(m, "TextBox", "A line of editable text (controls::TextBox).")
        .def_prop_rw("placeholder",
            [](const Control &self) { return resolve<controls::TextBox>(self)->getPlaceholder(); },
            [](const Control &self, const std::string &text) { resolve<controls::TextBox>(self)->setPlaceholder(text); },
            "A hint shown while it is empty and unfocused.")
        .def_prop_rw("editable",
            [](const Control &self) { return resolve<controls::TextBox>(self)->isEditable(); },
            [](const Control &self, bool editable) {
                inCall([&]() { resolve<controls::TextBox>(self)->setEditable(editable); });
            }, "Whether typing changes it.")
        .def_prop_rw("max_length",
            [](const Control &self) { return resolve<controls::TextBox>(self)->getMaxTextLength(); },
            [](const Control &self, int length) { resolve<controls::TextBox>(self)->setMaxTextLength(length); },
            "The most bytes it takes (-1: no limit).")
        .def_prop_rw("error",
            [](const Control &self) { return resolve<controls::TextBox>(self)->getError(); },
            [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::TextBox>(self)->setError(text); });
            }, "The validation message under the field (\"\" for none).")
        .def_prop_rw("helper_text",
            [](const Control &self) { return resolve<controls::TextBox>(self)->getHelperText(); },
            [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::TextBox>(self)->setHelperText(text); });
            }, "A hint under the field, shown while there is no error.")
        .def_prop_ro("selection", [](const Control &self) {
                return std::string(resolve<controls::TextBox>(self)->getSelection());
            }, "The selected text.")
        .def_prop_rw("cursor",
            [](const Control &self) { return resolve<controls::TextBox>(self)->getCursorPos(); },
            [](const Control &self, int pos) {
                controls::TextBox *box=resolve<controls::TextBox>(self);

                box->setCursorPos(pos);
                box->setCursorend(pos);
            }, "The caret, as a byte offset.");
    bindEvent<decltype(textBox), controls::TextBox>(textBox, "on_text_changed", &controls::TextBox::onTextChangedCaller,
        "Its text changed (typing, or setting text).");
    bindEvent<decltype(textBox), controls::TextBox>(textBox, "on_return_pressed",
        &controls::TextBox::onReturnPressedCaller, "Return was pressed in it.");
    bindEvent<decltype(textBox), controls::TextBox>(textBox, "on_lost_focus", &controls::TextBox::onLostFocusCaller,
        "It lost the keyboard focus.");
    bindEvent<decltype(textBox), controls::TextBox>(textBox, "on_error_changed", &controls::TextBox::onErrorChanged,
        "Its error message changed.");
    registerControl<TextBox, controls::TextBox>(textBox);

    auto multiline=nb::class_<TextBoxMultiline, TextBox>(m, "TextBoxMultiline",
        "Several lines of editable text (controls::TextBoxMultiline).");
    registerControl<TextBoxMultiline, controls::TextBoxMultiline>(multiline);

    auto numeric=nb::class_<TextBoxNumeric, TextBox>(m, "TextBoxNumeric",
        "A TextBox that takes a number only (controls::TextBoxNumeric).")
        .def_prop_ro("value", [](const Control &self) {
                return resolve<controls::TextBoxNumeric>(self)->GetFloatFromText();
            }, "Its text as a number (0 for none).");
    registerControl<TextBoxNumeric, controls::TextBoxNumeric>(numeric);

    auto password=nb::class_<PasswordTextBox, TextBox>(m, "PasswordTextBox",
        "A TextBox showing a mask character for each one typed (controls::PasswordTextBox).")
        .def_prop_rw("mask",
            [](const Control &self) { return resolve<controls::PasswordTextBox>(self)->getPasswordChar(); },
            [](const Control &self, const std::string &mask) {
                if(!resolve<controls::PasswordTextBox>(self)->setPasswordChar(mask))
                    throw nb::value_error("gweni.PasswordTextBox.mask: not one character");
            }, "The character shown for each one of the password (one codepoint).");
    registerControl<PasswordTextBox, controls::PasswordTextBox>(password);

    // the sliders
    auto slider=nb::class_<Slider, Control>(m, "Slider",
        "A value on a track with a thumb (controls::Slider); add a HorizontalSlider or a\n"
        "VerticalSlider.")
        .def_prop_rw("value",
            [](const Control &self) { return resolve<controls::Slider>(self)->getFloatValue(); },
            [](const Control &self, float value) {
                inCall([&]() { resolve<controls::Slider>(self)->setFloatValue(value); });
            }, "Its value, in its range; setting it fires on_value_changed.")
        .def_prop_rw("range",
            [](const Control &self) {
                controls::Slider *s=resolve<controls::Slider>(self);
                return std::make_tuple(s->getMin(), s->getMax());
            },
            [](const Control &self, std::tuple<float, float> range) {
                inCall([&]() { resolve<controls::Slider>(self)->setRange(std::get<0>(range), std::get<1>(range)); });
            }, "(minimum, maximum).")
        .def_prop_rw("notch_count",
            [](const Control &self) { return resolve<controls::Slider>(self)->getNotchCount(); },
            [](const Control &self, int count) { resolve<controls::Slider>(self)->setNotchCount(count); },
            "How many notches the track shows.")
        .def_prop_rw("clamp_to_notches",
            [](const Control &self) { return resolve<controls::Slider>(self)->getClampToNotches(); },
            [](const Control &self, bool clamp) { resolve<controls::Slider>(self)->setClampToNotches(clamp); },
            "Whether a value snaps to the nearest notch.")
        .def_prop_rw("value_format",
            [](const Control &self) { return resolve<controls::Slider>(self)->getValueFormat(); },
            [](const Control &self, const std::string &format) {
                if(!resolve<controls::Slider>(self)->setValueFormat(format))
                    throw nb::value_error("gweni.Slider.value_format: not one conversion of a double");
            }, "A printf format showing the value as text (\"%.2f\"; \"\" for none).");
    bindEvent<decltype(slider), controls::Slider>(slider, "on_value_changed", &controls::Slider::onValueChanged,
        "Its value changed.");
    registerControl<Slider, controls::Slider, false>(slider);
    auto horizontal=nb::class_<HorizontalSlider, Slider>(m, "HorizontalSlider", "A Slider along x.");
    registerControl<HorizontalSlider, controls::HorizontalSlider>(horizontal);
    auto vertical=nb::class_<VerticalSlider, Slider>(m, "VerticalSlider", "A Slider along y.");
    registerControl<VerticalSlider, controls::VerticalSlider>(vertical);

    // ComboBox and ListBox
    auto comboBox=nb::class_<ComboBox, Button>(m, "ComboBox",
        "A button opening a list to choose one item from (controls::ComboBox).")
        .def("add_item", [](const Control &self, const std::string &label, const std::string &name) {
                return inCall([&]() { return wrap(resolve<controls::ComboBox>(self)->addItem(label, name)); });
            }, nb::arg("label"), nb::arg("name")="", "Adds an item; returns its handle.")
        .def("select", [](const Control &self, const std::string &name) {
                inCall([&]() { resolve<controls::ComboBox>(self)->selectItemByName(name); });
            }, nb::arg("name"), "Selects the item of that name (fires on_selection).")
        .def("clear_items", [](const Control &self) {
                inCall([&]() { resolve<controls::ComboBox>(self)->clearItems(); });
            }, "Removes every item.")
        .def_prop_ro("selected_text", [](const Control &self) {
                controls::Label *item=resolve<controls::ComboBox>(self)->getSelectedItem();
                return item ? item->getText() : std::string();
            }, "The selected item's label (\"\" with none).")
        .def_prop_ro("selected_name", [](const Control &self) {
                controls::Label *item=resolve<controls::ComboBox>(self)->getSelectedItem();
                return item ? item->getName() : std::string();
            }, "The selected item's name (\"\" with none).")
        .def_prop_ro("is_open", [](const Control &self) { return resolve<controls::ComboBox>(self)->isMenuOpen(); },
            "Whether its list is open.");
    bindEvent<decltype(comboBox), controls::ComboBox>(comboBox, "on_selection", &controls::ComboBox::onSelection,
        "An item was chosen.");
    registerControl<ComboBox, controls::ComboBox>(comboBox);

    auto listBox=nb::class_<ListBox, Control>(m, "ListBox", "A scrolling list of rows to select (controls::ListBox).")
        .def("add_item", [](const Control &self, const std::string &label, const std::string &name) {
                return inCall([&]() { return wrap(resolve<controls::ListBox>(self)->addItem(label, name)); });
            }, nb::arg("label"), nb::arg("name")="", "Adds a row; returns its handle.")
        .def("select", [](const Control &self, const std::string &text) {
                inCall([&]() { resolve<controls::ListBox>(self)->selectByString(text); });
            }, nb::arg("text"), "Selects the row with that text (fires on_row_selected).")
        .def("clear", [](const Control &self) { inCall([&]() { resolve<controls::ListBox>(self)->clear(); }); },
            "Removes every row.")
        .def_prop_ro("selected_name", [](const Control &self) {
                return std::string(resolve<controls::ListBox>(self)->getSelectedRowName());
            }, "The selected row's name.")
        .def_prop_rw("multi_select",
            [](const Control &self) { return resolve<controls::ListBox>(self)->allowMultiSelect(); },
            [](const Control &self, bool multi) { resolve<controls::ListBox>(self)->setAllowMultiSelect(multi); },
            "Whether several rows can be selected.");
    bindEvent<decltype(listBox), controls::ListBox>(listBox, "on_row_selected", &controls::ListBox::onRowSelectedCaller,
        "A row was selected.");
    registerControl<ListBox, controls::ListBox>(listBox);

    // ProgressBar, Switch, Spinner, Chip
    auto progress=nb::class_<ProgressBar, Label>(m, "ProgressBar", "A bar filled to a fraction (controls::ProgressBar).")
        .def_prop_rw("progress",
            [](const Control &self) { return resolve<controls::ProgressBar>(self)->getProgress(); },
            [](const Control &self, float value) { resolve<controls::ProgressBar>(self)->setProgress(value); },
            "The fraction filled, 0..1.")
        .def_prop_rw("indeterminate",
            [](const Control &self) { return resolve<controls::ProgressBar>(self)->isIndeterminate(); },
            [](const Control &self, bool on) { resolve<controls::ProgressBar>(self)->setIndeterminate(on); },
            "A segment moving along it instead of the fill.")
        .def_prop_rw("horizontal",
            [](const Control &self) { return resolve<controls::ProgressBar>(self)->isHorizontal(); },
            [](const Control &self, bool horizontal) {
                controls::ProgressBar *bar=resolve<controls::ProgressBar>(self);

                if(horizontal)
                    bar->setHorizontal();
                else
                    bar->setVertical();
            }, "Filled along x (else along y).");
    progress.def("set_auto_label", [](const Control &self, bool on) {
            resolve<controls::ProgressBar>(self)->setAutoLabel(on);
        }, nb::arg("on"), "Whether its text is the percentage.");
    registerControl<ProgressBar, controls::ProgressBar>(progress);

    auto toggle=nb::class_<Switch, Button>(m, "Switch", "An on/off toggle with a sliding thumb (controls::Switch).")
        .def_prop_rw("on",
            [](const Control &self) { return resolve<controls::Switch>(self)->isOn(); },
            [](const Control &self, bool on) { inCall([&]() { resolve<controls::Switch>(self)->setOn(on); }); },
            "Whether it is on; setting it fires on_toggled and on_toggled_on/off.")
        .def_prop_rw("label_on_left",
            [](const Control &self) { return resolve<controls::Switch>(self)->isLabelOnLeft(); },
            [](const Control &self, bool left) { resolve<controls::Switch>(self)->setLabelOnLeft(left); },
            "Its text on the left of the track.");
    bindEvent<decltype(toggle), controls::Switch>(toggle, "on_toggled", &controls::Switch::onToggled,
        "It was turned on or off.");
    bindEvent<decltype(toggle), controls::Switch>(toggle, "on_toggled_on", &controls::Switch::onToggledOn,
        "It was turned on.");
    bindEvent<decltype(toggle), controls::Switch>(toggle, "on_toggled_off", &controls::Switch::onToggledOff,
        "It was turned off.");
    registerControl<Switch, controls::Switch>(toggle);

    auto spinner=nb::class_<Spinner, Control>(m, "Spinner", "A busy indicator, an arc turning round (controls::Spinner).")
        .def("start", [](const Control &self) { resolve<controls::Spinner>(self)->start(); }, "Turns it.")
        .def("stop", [](const Control &self) { resolve<controls::Spinner>(self)->stop(); },
            "Stops it (hidden, unless hide_when_stopped is False).")
        .def_prop_ro("spinning", [](const Control &self) { return resolve<controls::Spinner>(self)->isSpinning(); })
        .def_prop_rw("speed",
            [](const Control &self) { return resolve<controls::Spinner>(self)->getSpeed(); },
            [](const Control &self, float speed) { resolve<controls::Spinner>(self)->setSpeed(speed); },
            "Turns a second.")
        .def_prop_rw("diameter",
            [](const Control &self) { return resolve<controls::Spinner>(self)->getDiameter(); },
            [](const Control &self, int units) { resolve<controls::Spinner>(self)->setDiameter(units); },
            "Its diameter in units.")
        .def_prop_rw("hide_when_stopped",
            [](const Control &self) { return resolve<controls::Spinner>(self)->hidesWhenStopped(); },
            [](const Control &self, bool hide) { resolve<controls::Spinner>(self)->setHideWhenStopped(hide); },
            "Hidden while stopped (else a still ring).")
        .def_prop_rw("show_track",
            [](const Control &self) { return resolve<controls::Spinner>(self)->showsTrack(); },
            [](const Control &self, bool show) { resolve<controls::Spinner>(self)->setShowTrack(show); },
            "A ring under the arc.");
    registerControl<Spinner, controls::Spinner>(spinner);

    nb::enum_<controls::ChipTone>(m, "ChipTone", "A Chip's colour (ChipTone).")
        .value("NEUTRAL", controls::ChipTone::Neutral)
        .value("ACCENT", controls::ChipTone::Accent)
        .value("SUCCESS", controls::ChipTone::Success)
        .value("WARNING", controls::ChipTone::Warning)
        .value("DANGER", controls::ChipTone::Danger);

    auto chip=nb::class_<Chip, Button>(m, "Chip", "A toned label, clickable or removable (controls::Chip).")
        .def_prop_rw("tone",
            [](const Control &self) { return resolve<controls::Chip>(self)->getTone(); },
            [](const Control &self, controls::ChipTone tone) { resolve<controls::Chip>(self)->setTone(tone); })
        .def_prop_rw("palette_index",
            [](const Control &self) { return resolve<controls::Chip>(self)->getPaletteIndex(); },
            [](const Control &self, int index) { resolve<controls::Chip>(self)->setPaletteIndex(index); },
            "A palette colour (0..7) in place of the tone; -1 for the tone.")
        .def_prop_rw("clickable",
            [](const Control &self) { return resolve<controls::Chip>(self)->isClickable(); },
            [](const Control &self, bool on) { resolve<controls::Chip>(self)->setClickable(on); },
            "Whether it takes a press (on_press).")
        .def_prop_rw("removable",
            [](const Control &self) { return resolve<controls::Chip>(self)->isRemovable(); },
            [](const Control &self, bool on) { resolve<controls::Chip>(self)->setRemovable(on); },
            "Whether it has an x that fires on_remove (the app deletes it).");
    bindEvent<decltype(chip), controls::Chip>(chip, "on_remove", &controls::Chip::onRemove,
        "Its x was clicked; the chip deletes nothing itself (call delete_later()).");
    registerControl<Chip, controls::Chip>(chip);
}

}//namespace python
}//namespace gweni
