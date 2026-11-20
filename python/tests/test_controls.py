#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The Control properties and the simple controls on a Headless: for each
family a frame, a property round trip and an event, through the input
ControlTest's flags give where there is one (a click, typing, a key)."""

import pytest

import gweni

pytestmark = pytest.mark.gpu


def centre(control: gweni.Control) -> tuple[int, int]:
    x, y, w, h = control.canvas_bounds
    assert w > 0 and h > 0, f"{control!r} has no size"
    return x + w // 2, y + h // 2


def click(h: gweni.Headless, control: gweni.Control) -> None:
    h.click(*centre(control))
    h.frame()


def top(h: gweni.Headless, cls: type, **props: object) -> gweni.Control:
    control = h.canvas.add(cls, dock=gweni.Dock.TOP, **props)
    h.frame()
    return control


def test_control_properties(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel, position=(10, 20), size=(120, 40))
    assert panel.position == (10, 20) and panel.size == (120, 40)
    panel.margin = (1, 2, 3, 4)
    assert panel.margin == (1, 2, 3, 4)
    panel.padding = (5, 6, 7, 8)
    assert panel.padding == (5, 6, 7, 8)
    child = panel.add(gweni.Label, text="inside")
    headless.frame()
    assert panel.canvas_bounds[:2] == (10, 20)

    assert not panel.hidden and child.visible
    panel.hide()
    assert panel.hidden and not child.visible and not child.hidden
    panel.show()
    assert child.visible
    panel.hidden = True
    assert panel.hidden
    panel.hidden = False

    assert panel.opacity is None
    panel.opacity = 0.5
    assert panel.opacity == pytest.approx(0.5)
    panel.set_opacity(0.25, 1.0)
    assert panel.opacity == pytest.approx(0.25)
    panel.opacity = None
    assert panel.opacity is None

    assert panel.enabled
    panel.enabled = False
    assert not panel.enabled
    panel.enabled = True
    panel.tooltip = "tip"
    assert panel.tooltip == "tip"
    headless.frame()


def test_focus(headless: gweni.Headless) -> None:
    box = top(headless, gweni.TextBox)
    assert not box.focused
    box.focus()
    assert box.focused
    box.unfocus()
    assert not box.focused


def test_check_box(headless: gweni.Headless) -> None:
    box = top(headless, gweni.CheckBox)
    calls: list[str] = []
    box.on_check_changed(lambda e: calls.append("changed"))
    box.on_checked(lambda e: calls.append("checked"))
    box.on_unchecked(lambda e: calls.append("unchecked"))
    click(headless, box)
    assert box.checked and calls == ["checked", "changed"]
    box.checked = False
    assert not box.checked and calls[-2:] == ["unchecked", "changed"]


def test_check_box_with_label(headless: gweni.Headless) -> None:
    row = top(headless, gweni.CheckBoxWithLabel, text="Remember me")
    assert row.text == "Remember me"
    events: list[gweni.Event] = []
    row.on_check_changed(events.append)
    x, y, w, h = row.canvas_bounds
    headless.click(x + w - 10, y + h // 2)  # on the label, right of the box
    headless.frame()
    assert row.checked and len(events) == 1
    assert events[0].control == row
    assert type(row.check_box) is gweni.CheckBox
    row.label_on_left = True
    assert row.label_on_left


def test_radio_buttons(headless: gweni.Headless) -> None:
    group = top(headless, gweni.RadioButtonController, size=(200, 80))
    a = group.add_option("Alpha", "a")
    b = group.add_option("Beta", "b")
    assert type(a) is gweni.RadioButton and a.text == "Alpha"
    headless.frame()
    changes: list[str] = []
    group.on_selection_change(lambda e: changes.append(group.selected_name))
    b.select()
    assert b.selected and not a.selected and group.selected == b
    click(headless, a)
    assert group.selected_name == "a"
    assert changes == ["b", "a"]


def test_text_box_typing(headless: gweni.Headless) -> None:
    box = top(headless, gweni.TextBox, placeholder="name")
    assert box.placeholder == "name"
    changes: list[str] = []
    returns: list[str] = []
    box.on_text_changed(lambda e: changes.append(box.text))
    box.on_return_pressed(lambda e: returns.append(box.text))
    click(headless, box)
    headless.type("abc")
    headless.key("ctrl+a")
    assert box.selection == "abc"
    box.cursor = 1
    assert box.cursor == 1
    headless.key("return")
    headless.frame()
    assert box.text == "abc"
    assert changes == ["a", "ab", "abc"]
    assert returns == ["abc"]

    box.editable = False
    headless.type("x")
    assert box.text == "abc" and not box.editable


def test_text_box_error(headless: gweni.Headless) -> None:
    box = top(headless, gweni.TextBox)
    errors: list[str] = []
    box.on_error_changed(lambda e: errors.append(box.error))
    box.error = "required"
    assert box.error == "required"
    box.error = ""
    assert errors == ["required", ""]
    box.helper_text = "letters only"
    assert box.helper_text == "letters only"
    box.max_length = 3
    assert box.max_length == 3


def test_text_box_kinds(headless: gweni.Headless) -> None:
    multi = top(headless, gweni.TextBoxMultiline, size=(200, 60))
    click(headless, multi)
    headless.type("a")
    headless.key("return")
    headless.type("b")
    assert multi.text == "a\nb"

    numeric = top(headless, gweni.TextBoxNumeric, text="")
    click(headless, numeric)
    headless.type("12x3")
    assert numeric.text == "123" and numeric.value == 123.0

    password = top(headless, gweni.PasswordTextBox)
    click(headless, password)
    headless.type("pw")
    assert password.text == "pw"
    password.mask = "•"
    assert password.mask == "•"
    with pytest.raises(ValueError):
        password.mask = "ab"
    headless.frame()


def test_sliders(headless: gweni.Headless) -> None:
    slider = top(headless, gweni.HorizontalSlider, size=(200, 20), range=(0, 10))
    assert slider.range == (0, 10)
    values: list[float] = []
    slider.on_value_changed(lambda e: values.append(slider.value))
    slider.value = 5
    assert slider.value == pytest.approx(5) and values == [pytest.approx(5)]
    slider.notch_count = 10
    slider.clamp_to_notches = True
    assert slider.notch_count == 10 and slider.clamp_to_notches
    slider.value_format = "%.1f"
    assert slider.value_format == "%.1f"
    with pytest.raises(ValueError):
        slider.value_format = "%s"
    vertical = headless.canvas.add(gweni.VerticalSlider, dock=gweni.Dock.LEFT)
    assert isinstance(vertical, gweni.Slider)
    with pytest.raises(TypeError):
        headless.canvas.add(gweni.Slider)  # only its two kinds
    headless.frame()


def test_combo_box(headless: gweni.Headless) -> None:
    combo = top(headless, gweni.ComboBox)
    combo.add_item("Apple", "a")
    combo.add_item("Banana", "b")
    chosen: list[str] = []
    combo.on_selection(lambda e: chosen.append(combo.selected_name))
    combo.select("b")
    assert combo.selected_text == "Banana" and chosen == ["b"]
    headless.frame()
    click(headless, combo)
    assert combo.is_open
    headless.click(600, 470)  # outside it and its list
    headless.frame()
    assert not combo.is_open
    combo.clear_items()
    assert combo.selected_text == ""


def test_list_box(headless: gweni.Headless) -> None:
    lst = top(headless, gweni.ListBox, size=(200, 100))
    rows = [lst.add_item(text, text.lower()) for text in ("One", "Two", "Three")]
    headless.frame()
    selected: list[str] = []
    lst.on_row_selected(lambda e: selected.append(lst.selected_name))
    lst.select("Two")
    assert lst.selected_name == "two"
    click(headless, rows[2])
    assert selected == ["two", "three"]
    lst.multi_select = True
    assert lst.multi_select
    lst.clear()
    headless.frame()
    assert not rows[0].alive


def test_progress_bar(headless: gweni.Headless) -> None:
    bar = top(headless, gweni.ProgressBar, progress=0.25)
    assert bar.progress == pytest.approx(0.25)
    bar.indeterminate = True
    assert bar.indeterminate
    headless.frame(3)
    assert headless.stats()["anims_run"] > 0
    bar.indeterminate = False
    bar.horizontal = False
    assert not bar.horizontal
    bar.set_auto_label(True)
    headless.frame()


def test_switch(headless: gweni.Headless) -> None:
    switch = top(headless, gweni.Switch, text="Wi-Fi")
    calls: list[str] = []
    switch.on_toggled(lambda e: calls.append("toggled"))
    switch.on_toggled_on(lambda e: calls.append("on"))
    switch.on_toggled_off(lambda e: calls.append("off"))
    click(headless, switch)
    assert switch.on and calls == ["toggled", "on"]
    switch.on = False
    assert calls[-2:] == ["toggled", "off"]
    switch.label_on_left = True
    assert switch.label_on_left


def test_spinner(headless: gweni.Headless) -> None:
    spinner = top(headless, gweni.Spinner, diameter=32, show_track=True)
    assert spinner.diameter == 32 and spinner.show_track
    spinner.stop()
    headless.frame()
    assert not spinner.spinning and spinner.hidden == spinner.hide_when_stopped
    assert headless.stats()["anims_run"] == 0
    spinner.start()
    headless.frame(2)
    assert spinner.spinning and not spinner.hidden and headless.stats()["anims_run"] == 1
    spinner.speed = 2.0
    assert spinner.speed == pytest.approx(2.0)


def test_chip_removed_from_its_handler(headless: gweni.Headless) -> None:
    chip = top(headless, gweni.Chip, text="python", removable=True, tone=gweni.ChipTone.ACCENT)
    assert chip.tone == gweni.ChipTone.ACCENT and chip.removable
    chip.palette_index = 3
    assert chip.palette_index == 3
    chip.palette_index = 11
    assert chip.palette_index == 3
    chip.palette_index = -1
    assert chip.palette_index == -1 and chip.tone == gweni.ChipTone.ACCENT
    removed: list[gweni.Control] = []

    @chip.on_remove
    def _(event: gweni.Event) -> None:
        removed.append(event.control)
        chip.delete_later()

    # the x is the chip's last child, at its right
    (x_button,) = [c for c in chip.children if c.type_name == "ChipRemoveButton"]
    click(headless, x_button)
    headless.frame()
    assert len(removed) == 1
    assert not chip.alive
    with pytest.raises(gweni.ControlDeletedError):
        chip.text


@pytest.mark.parametrize("bad", [(5, 5), (10, 0), (float("nan"), 1)])
def test_slider_range_refuses_an_empty_or_reversed_range(headless: gweni.Headless, bad: tuple[float, float]) -> None:
    slider = top(headless, gweni.HorizontalSlider, size=(200, 20), range=(0, 10))
    slider.value = 4
    with pytest.raises(ValueError, match="minimum must be below the maximum"):
        slider.range = bad
    # left as it was, its value a number
    assert slider.range == (0, 10) and slider.value == pytest.approx(4)



def test_slider_value_refuses_nan(headless: gweni.Headless) -> None:
    slider = top(headless, gweni.HorizontalSlider, size=(200, 20), range=(0, 10))
    slider.value = 4
    with pytest.raises(ValueError, match="not a number"):
        slider.value = float("nan")
    assert slider.value == pytest.approx(4)
