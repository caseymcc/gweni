#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""delete_later() never crashes: it deletes what add() made and the dialogs,
takes a TabButton out as close_tab() does and dismisses a Toast, and refuses
the canvas and every part an owner made and keeps a pointer to (a
CheckBoxWithLabel's box, a TabButton's page, a prompt's field, an option, an
item), the owner still working after the refusal. Each refused case below
segfaulted before delete_later() refused it."""

from typing import Callable

import pytest

import gweni

pytestmark = pytest.mark.gpu


def centre(control: gweni.Control) -> tuple[int, int]:
    x, y, w, h = control.canvas_bounds
    return x + w // 2, y + h // 2


def test_the_canvas_is_refused(headless: gweni.Headless) -> None:
    with pytest.raises(ValueError, match="canvas cannot be deleted"):
        headless.canvas.delete_later()
    headless.frame()
    assert headless.canvas.alive


def test_a_check_box_with_labels_box_is_refused(headless: gweni.Headless) -> None:
    row = headless.canvas.add(gweni.CheckBoxWithLabel, dock=gweni.Dock.TOP)
    headless.frame()
    with pytest.raises(ValueError, match="CheckBox #.* is a part its owner made"):
        row.check_box.delete_later()
    headless.frame()
    row.checked = True
    headless.frame()
    assert row.checked and row.check_box.alive


def test_a_tab_page_is_refused(headless: gweni.Headless) -> None:
    tabs = headless.canvas.add(gweni.TabControl, dock=gweni.Dock.CENTER)
    a = tabs.add_page("a")
    b = tabs.add_page("b")
    headless.frame()
    with pytest.raises(ValueError, match="is a part its owner made"):
        a.page.delete_later()
    headless.frame()
    headless.click(*centre(b))
    headless.frame()
    headless.click(*centre(a))
    headless.frame()
    assert tabs.current == a and a.page.alive


def test_a_prompts_field_is_refused(headless: gweni.Headless) -> None:
    results: list[tuple[gweni.DialogResult, str]] = []
    dialog = gweni.prompt(headless.canvas, "T", "Name?", "abc", lambda r, t: results.append((r, t)))
    headless.frame()
    with pytest.raises(ValueError, match="TextBox #.* is a part its owner made"):
        dialog.field.delete_later()
    headless.frame()
    headless.key("return")
    headless.frame()
    assert results == [(gweni.DialogResult.OK, "abc")]


def test_a_selected_radio_option_is_refused(headless: gweni.Headless) -> None:
    group = headless.canvas.add(gweni.RadioButtonController, dock=gweni.Dock.TOP)
    a = group.add_option("a")
    b = group.add_option("b")
    headless.frame()
    a.select()
    headless.frame()
    assert group.selected == a
    with pytest.raises(ValueError, match="LabeledRadioButton #.* is a part its owner made"):
        a.delete_later()
    headless.frame()
    x, y, w, h = b.canvas_bounds
    headless.click(x + 3, y + h // 2)
    headless.frame()
    assert group.selected == b


@pytest.mark.parametrize("part", ["dialog button", "combo item", "menu", "dialog content"])
def test_other_parts_are_refused(headless: gweni.Headless, part: str) -> None:
    canvas = headless.canvas
    pick: Callable[[], gweni.Control]
    if part == "dialog button":
        button = gweni.message(canvas, "T", "Hi").add_button("More")
        pick = lambda: button  # noqa: E731
    elif part == "combo item":
        combo = canvas.add(gweni.ComboBox, dock=gweni.Dock.TOP)
        item = combo.add_item("a")
        pick = lambda: item  # noqa: E731
    elif part == "menu":
        strip = canvas.add(gweni.MenuStrip)
        sub = strip.add_item("File").menu
        pick = lambda: sub  # noqa: E731
    else:
        dialog = gweni.message(canvas, "T", "Hi")
        pick = lambda: dialog.content  # noqa: E731
    headless.frame()
    control = pick()
    with pytest.raises(ValueError, match="is a part its owner made"):
        control.delete_later()
    headless.frame()
    headless.frame()
    assert control.alive


def test_the_selected_tab_is_closed_cleanly(headless: gweni.Headless) -> None:
    tabs = headless.canvas.add(gweni.TabControl, dock=gweni.Dock.CENTER)
    a = tabs.add_page("a")
    b = tabs.add_page("b")
    headless.frame()
    assert tabs.current == a
    page = a.page
    a.delete_later()
    headless.frame()
    assert tabs.tab_count == 1 and tabs.current == b
    headless.click(*centre(b))
    headless.frame()
    assert not a.alive and not page.alive and tabs.current == b


def test_a_toast_is_dismissed(headless: gweni.Headless) -> None:
    toast = headless.canvas.notify(gweni.ToastKind.INFO, "hello")
    headless.frame()
    toast.delete_later()
    assert toast.dismissed
    headless.frame()
    assert not toast.alive
    headless.canvas.notify(gweni.ToastKind.INFO, "again")
    headless.frame()


def test_a_dialog_and_what_add_made_are_deleted(headless: gweni.Headless) -> None:
    dialog = gweni.message(headless.canvas, "T", "Hi")
    label = dialog.content.add(gweni.Label, text="inside")
    headless.frame()
    label.delete_later()
    headless.frame()
    assert not label.alive and dialog.alive
    dialog.delete_later()
    headless.frame()
    assert not dialog.alive
    headless.key("escape")
    headless.click(10, 10)
    headless.frame()
