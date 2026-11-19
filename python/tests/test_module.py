#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""The module without a context: what it exports and the errors it raises."""

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
