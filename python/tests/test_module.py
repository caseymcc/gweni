#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The module without a context: what it exports, the errors it raises, and an
App or Headless refused before it opens (a bad size or scale)."""

import pytest

import gweni


def test_errors_are_python_exception_types() -> None:
    assert issubclass(gweni.ControlDeletedError, ReferenceError)
    assert issubclass(gweni.ThreadError, RuntimeError)


def test_handles_are_not_made_from_python() -> None:
    # a handle comes from the tree (add(), canvas, children), never from a
    # constructor that could name a control id by hand
    with pytest.raises(TypeError):
        gweni.Control()
    with pytest.raises(TypeError):
        gweni.Label()


def test_bound_classes_derive_from_control() -> None:
    for cls in (gweni.Canvas, gweni.Panel, gweni.Label, gweni.Button):
        assert issubclass(cls, gweni.Control)
    assert issubclass(gweni.Button, gweni.Label)


def test_enums() -> None:
    assert {d.name for d in gweni.Dock} == {"NONE", "LEFT", "RIGHT", "TOP", "BOTTOM", "CENTER"}
    assert {d.name for d in gweni.Layout} == {"NONE", "VERTICAL", "HORIZONTAL"}


@pytest.mark.parametrize("kind", ["Headless", "App"])
@pytest.mark.parametrize(
    "size, scale, match",
    [((0, 0), 1.0, "size must be positive"), ((-1, 100), 1.0, "size must be positive"),
     ((640, 480), 0.0, "scale must be a positive number"), ((640, 480), float("nan"), "scale must be a positive number"),
     ((640, 480), -2.0, "scale must be a positive number")])
def test_a_bad_size_or_scale_is_refused_before_opening(kind: str, size: tuple[int, int], scale: float, match: str) -> None:
    # refused up front with a ValueError, no context made (so no display or
    # render lock needed), rather than a device that does not open
    with pytest.raises(ValueError, match=match):
        getattr(gweni, kind)(size=size, scale=scale)
