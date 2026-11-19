#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""gweni.App where no window can open (CTest's runs have no display; the window
itself is python_window_idle's, under Xvfb): it raises RuntimeError and leaves
nothing behind, and the one-instance rule holds between App and Headless."""

import os

import pytest

import gweni

pytestmark = pytest.mark.gpu


@pytest.fixture
def no_display(monkeypatch: pytest.MonkeyPatch) -> None:
    monkeypatch.delenv("DISPLAY", raising=False)
    monkeypatch.delenv("WAYLAND_DISPLAY", raising=False)


def test_no_display_raises_and_leaves_nothing(no_display: None) -> None:
    assert "DISPLAY" not in os.environ
    with pytest.raises(RuntimeError, match="gweni.App"):
        gweni.App(title="no display")
    with gweni.Headless() as h:  # the refused App holds no instance
        h.frame()


def test_an_app_is_refused_while_a_headless_lives(headless: gweni.Headless) -> None:
    with pytest.raises(RuntimeError, match="alive"):
        gweni.App()
    headless.frame()
