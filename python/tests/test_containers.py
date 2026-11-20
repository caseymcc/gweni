#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The containers and the rest of v1 on a Headless: ScrollControl, GroupBox,
TabControl, Window, MenuStrip/Menu/MenuItem, the standard dialogs with their
result callback, toasts and Flow. A closed Dialog's and a dismissed Toast's
handles go stale (ControlDeletedError), never crash."""

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


def test_scroll_control(headless: gweni.Headless) -> None:
    scroll = headless.canvas.add(gweni.ScrollControl, position=(0, 0), size=(200, 100))
    scroll.set_scroll(False, True)
    for i in range(20):
        scroll.add(gweni.Button, text=f"row {i}", dock=gweni.Dock.TOP)
    headless.frame(2)
    assert scroll.max_scroll_offset[1] > 0
    headless.move(100, 50)
    assert headless.wheel(-60)
    headless.frame()
    assert scroll.scroll_offset[1] > 0
    scroll.scroll_to_top()
    headless.frame()
    assert scroll.scroll_offset == (0, 0)
    scroll.scroll_offset = (0, 30)
    headless.frame()
    assert scroll.scroll_offset == (0, 30)


def test_group_box(headless: gweni.Headless) -> None:
    group = headless.canvas.add(gweni.GroupBox, text="Options", size=(200, 100))
    inside = group.add(gweni.Label, text="inside", dock=gweni.Dock.TOP)
    headless.frame()
    assert group.text == "Options" and inside.parent is not None
    assert inside.canvas_bounds[1] > group.canvas_bounds[1]


def test_tab_control(headless: gweni.Headless) -> None:
    tabs = headless.canvas.add(gweni.TabControl, dock=gweni.Dock.CENTER)
    added: list[str] = []
    tabs.on_add_tab(lambda e: added.append("tab"))
    first = tabs.add_page("First")
    second = tabs.add_page("Second")
    assert type(first) is gweni.TabButton and tabs.tab_count == 2
    first.page.add(gweni.Label, text="one", dock=gweni.Dock.TOP)
    label2 = second.page.add(gweni.Label, text="two", dock=gweni.Dock.TOP)
    headless.frame()
    assert tabs.current == first and not label2.visible
    click(headless, second)
    assert tabs.current == second and second.active and label2.visible
    first.select()
    assert tabs.current == first
    assert tabs.tab(1) == second
    with pytest.raises(IndexError):
        tabs.tab(5)
    assert added == ["tab", "tab"]

    second.closable = True
    closing: list[gweni.Control] = []

    @tabs.on_close
    def _(event: gweni.Event) -> None:
        closing.append(event.control)
        tabs.veto_close()

    tabs.close_tab(second)  # closes without asking
    headless.frame()
    assert tabs.tab_count == 1 and not second.alive
    assert closing == []


def test_window(headless: gweni.Headless) -> None:
    win = headless.canvas.add(gweni.Window, title="Settings", position=(50, 50), size=(300, 200))
    assert win.title == "Settings"
    label = win.add(gweni.Label, text="0 clicks", dock=gweni.Dock.TOP)
    btn = win.add(gweni.Button, text="Click me", dock=gweni.Dock.TOP)
    clicks = 0

    @btn.on_press
    def _(event: gweni.Event) -> None:
        nonlocal clicks
        clicks += 1
        label.text = f"{clicks} clicks"

    headless.frame()
    click(headless, btn)
    click(headless, btn)
    assert label.text == "2 clicks"
    assert label.canvas_bounds[1] > 50  # below the title bar, inside the window
    win.make_modal()
    assert win.modal
    win.destroy_modal()
    assert not win.modal
    closed: list[str] = []
    win.on_closed(lambda e: closed.append("closed"))
    headless.key("escape")  # not modal: Escape does not close it
    headless.frame()
    assert closed == [] and win.visible


def test_menu_strip(headless: gweni.Headless) -> None:
    strip = headless.canvas.add(gweni.MenuStrip, dock=gweni.Dock.TOP)
    file_item = strip.add_item("File")
    assert type(file_item) is gweni.MenuItem
    menu = file_item.menu
    assert isinstance(menu, gweni.Menu)
    open_item = menu.add_item("Open", "Ctrl+O")
    menu.add_divider()
    wrap_item = menu.add_item("Wrap")
    wrap_item.checkable = True
    chosen: list[str] = []
    open_item.on_selected(lambda e: chosen.append("open"))
    changes: list[bool] = []
    wrap_item.on_check_change(lambda e: changes.append(wrap_item.checked))
    headless.frame()
    click(headless, file_item)
    assert menu.is_open
    click(headless, open_item)
    assert chosen == ["open"] and not menu.is_open
    wrap_item.checked = True
    assert changes == [True]


def test_canvas_menu_by_keys(headless: gweni.Headless) -> None:
    menu = headless.canvas.add(gweni.Menu)
    first = menu.add_item("First")
    menu.add_item("Second")
    picked: list[str] = []
    first.on_selected(lambda e: picked.append(e.control.text))
    headless.frame()
    menu.open_at((100, 100))
    headless.frame()
    assert menu.is_open
    headless.key("return")
    headless.frame()
    assert picked == ["First"]


def test_a_canvas_menu_add_makes_is_closed_until_opened(headless: gweni.Headless) -> None:
    headless.frame()
    blank = headless.pixels()
    menu = headless.canvas.add(gweni.Menu)
    menu.add_item("First")
    menu.add_item("Second")
    headless.frame()
    assert not menu.is_open and menu.hidden
    assert headless.pixels() == blank  # nothing drawn at (0, 0)
    menu.open_at((40, 40))
    headless.frame()
    assert menu.is_open and headless.pixels() != blank
    menu.close()
    headless.frame()
    assert headless.pixels() == blank
    # a MenuStrip is a bar on its parent: shown as made
    strip = headless.canvas.add(gweni.MenuStrip, dock=gweni.Dock.TOP)
    assert not strip.hidden


@pytest.mark.parametrize("kind", ["Window", "GroupBox", "ScrollControl", "Panel"])
def test_layout_set_after_a_layout_lays_the_children_out(headless: gweni.Headless, kind: str) -> None:
    def build(late: bool) -> list[tuple[int, int, int, int]]:
        panel = headless.canvas.add(getattr(gweni, kind), size=(300, 300), position=(10, 10))
        if not late:
            panel.layout = gweni.Layout.VERTICAL
        labels = [panel.add(gweni.Label, text=f"label {i}") for i in range(3)]
        headless.frame()
        if late:
            panel.layout = gweni.Layout.VERTICAL
        headless.frame()
        bounds = [label.bounds for label in labels]
        panel.delete_later()
        headless.frame()
        return bounds

    first = build(False)
    assert len({y for _, y, _, _ in first}) == 3  # stacked
    assert build(True) == first


def test_combo_box_items_are_menu_items(headless: gweni.Headless) -> None:
    combo = headless.canvas.add(gweni.ComboBox)
    assert type(combo.add_item("Apple")) is gweni.MenuItem


def test_message_dialog_closes_and_goes_stale(headless: gweni.Headless) -> None:
    results: list[tuple[gweni.DialogResult, str]] = []
    dialog = gweni.message(headless.canvas, "Saved", "The file was saved.",
                           on_result=lambda r, t: results.append((r, t)))
    assert type(dialog) is gweni.Dialog and dialog.is_open
    assert dialog.title == "Saved"
    headless.frame()
    headless.key("return")  # OK, the default button
    headless.frame()
    assert results == [(gweni.DialogResult.OK, "")]
    headless.frame()
    with pytest.raises(gweni.ControlDeletedError):
        dialog.is_open
    with pytest.raises(gweni.ControlDeletedError):
        dialog.close()


def test_confirm_dialog_by_click(headless: gweni.Headless) -> None:
    results: list[gweni.DialogResult] = []
    dialog = gweni.confirm(headless.canvas, "Delete", "Delete it?", gweni.DialogButtons.YES_NO,
                           on_result=lambda r, t: results.append(r))
    headless.frame()
    no = _footer_button(dialog, "No")
    click(headless, no)
    headless.frame()
    assert results == [gweni.DialogResult.NO]
    assert not dialog.alive


def _footer_button(dialog: gweni.Control, text: str) -> gweni.Button:
    stack = list(dialog.children)
    while stack:
        control = stack.pop()
        if isinstance(control, gweni.Button) and control.text == text:
            return control
        stack.extend(control.children)
    raise AssertionError(f"no {text!r} button in {dialog!r}")


def test_prompt_dialog(headless: gweni.Headless) -> None:
    results: list[tuple[gweni.DialogResult, str]] = []
    dialog = gweni.prompt(headless.canvas, "Name", "Your name?", "x",
                          on_result=lambda r, t: results.append((r, t)))
    assert type(dialog) is gweni.PromptDialog
    assert type(dialog.field) is gweni.TextBox and dialog.field.text == "x"
    headless.frame()
    headless.type("Ada")  # replaces the selected initial text
    headless.key("return")
    headless.frame()
    assert results == [(gweni.DialogResult.OK, "Ada")]


def test_dialog_close_from_python_and_an_error_in_its_handler(headless: gweni.Headless) -> None:
    def fail(result: gweni.DialogResult, text: str) -> None:
        raise ValueError("handler")

    dialog = gweni.message(headless.canvas, "t", "x", on_result=fail)
    headless.frame()
    with pytest.raises(ValueError, match="handler"):
        dialog.close()
    headless.frame()
    assert not dialog.alive


def test_toast_dismissed_goes_stale(headless: gweni.Headless) -> None:
    toast = headless.canvas.notify(gweni.ToastKind.POSITIVE, "Saved", title="Done")
    assert type(toast) is gweni.Toast
    assert toast.kind == gweni.ToastKind.POSITIVE and toast.text == "Saved" and toast.title == "Done"
    headless.frame()
    toast.dismiss()
    assert toast.dismissed
    headless.frame(2)
    assert not toast.alive
    with pytest.raises(gweni.ControlDeletedError):
        toast.text


def test_toast_times_out(headless: gweni.Headless) -> None:
    toast = headless.canvas.notify(gweni.ToastKind.INFO, "brief", timeout=0.5)
    headless.frame(20)
    assert toast.alive
    headless.frame(30)  # 50 frames: past 0.5 s on the stepped clock
    assert not toast.alive


def test_flow(headless: gweni.Headless) -> None:
    flow = headless.canvas.add(gweni.Flow, gap=8, min_card_width=150, max_columns=4)
    assert (flow.gap, flow.min_card_width, flow.max_columns) == (8, 150, 4)
    cards = [flow.add(gweni.GroupBox, text=f"card {i}") for i in range(6)]
    headless.frame(2)
    assert flow.columns == 4  # 640 wide: four 150-unit columns fit
    flow.set_breakpoints([(1000, 2)])
    headless.frame(2)
    assert flow.columns == 2
    assert cards[0].canvas_bounds[1] == cards[1].canvas_bounds[1] != cards[2].canvas_bounds[1]
