#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""Control handles on a Headless: made by add(), found by id, stale once their
control is deleted, and never the owner of their control."""

import gc

import pytest

import gweni

pytestmark = pytest.mark.gpu


def test_add_sets_props_and_returns_its_class(headless: gweni.Headless) -> None:
    canvas = headless.canvas
    assert isinstance(canvas, gweni.Canvas)
    label = canvas.add(gweni.Label, text="hello", dock=gweni.Dock.TOP)
    assert type(label) is gweni.Label
    assert label.text == "hello"
    assert label.dock == gweni.Dock.TOP
    assert label.type_name == "Label"
    assert label.parent == canvas
    label.text = "again"
    assert label.text == "again"


def test_children_come_back_as_their_bound_classes(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    panel.add(gweni.Label, text="a")
    panel.add(gweni.Button, text="b")
    kinds = [type(child) for child in panel.children]
    assert kinds == [gweni.Label, gweni.Button]
    assert [child.text for child in panel.children] == ["a", "b"]
    assert type(panel.parent.children[0]) is gweni.Panel


def test_handles_compare_by_control(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    a = panel.add(gweni.Label)
    (b,) = panel.children
    assert a == b and hash(a) == hash(b) and a is not b
    assert a != panel
    assert f"#{a.id} Label" in repr(a)


def test_delete_later_makes_the_handle_stale(headless: gweni.Headless) -> None:
    label = headless.canvas.add(gweni.Label, text="bye")
    headless.frame()
    label.delete_later()
    assert label.alive  # deleted in the next frame's update
    headless.frame()
    assert not label.alive
    with pytest.raises(gweni.ControlDeletedError):
        label.text
    with pytest.raises(gweni.ControlDeletedError):
        label.text = "x"
    assert "deleted" in repr(label)


def test_garbage_collection_deletes_no_control(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    for i in range(3):
        panel.add(gweni.Label, text=str(i))
    gc.collect()
    headless.frame()
    assert [child.text for child in panel.children] == ["0", "1", "2"]


def test_add_refuses_an_unknown_property_and_makes_nothing(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    with pytest.raises(AttributeError):
        panel.add(gweni.Label, colour="red")
    headless.frame()
    assert panel.children == []


@pytest.mark.parametrize("name", ["visible", "on_press", "delete_later"])
def test_add_refuses_a_read_only_property_or_a_method_and_makes_nothing(headless: gweni.Headless, name: str) -> None:
    panel = headless.canvas.add(gweni.Panel)
    with pytest.raises(AttributeError, match=name):
        panel.add(gweni.Button, **{name: True})
    # refused before the child is made: none even until the next frame
    assert panel.children == []


def test_add_refuses_a_property_of_the_wrong_type_and_makes_nothing(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    with pytest.raises(TypeError):
        panel.add(gweni.Label, text=3)
    headless.frame()
    assert panel.children == []


def test_add_refuses_what_is_no_control_type(headless: gweni.Headless) -> None:
    with pytest.raises(TypeError):
        headless.canvas.add(int)
    with pytest.raises(TypeError):
        headless.canvas.add(gweni.Canvas)


def test_layout_round_trip(headless: gweni.Headless) -> None:
    panel = headless.canvas.add(gweni.Panel)
    assert panel.layout == gweni.Layout.NONE
    panel.layout = gweni.Layout.VERTICAL
    assert panel.layout == gweni.Layout.VERTICAL
    panel.layout = gweni.Layout.HORIZONTAL
    assert panel.layout == gweni.Layout.HORIZONTAL
    with pytest.raises(ValueError):
        panel.layout = gweni.Layout.NONE


def test_handles_go_stale_when_the_headless_closes() -> None:
    with gweni.Headless() as h:
        label = h.canvas.add(gweni.Label, text="x")
        canvas = h.canvas
    with pytest.raises(gweni.ControlDeletedError):
        label.text
    with pytest.raises(gweni.ControlDeletedError):
        canvas.children
    with pytest.raises(RuntimeError, match="closed"):
        h.frame()
