#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""gweni.Headless: the stepped clock, captures, stats, the one-instance rule
and the thread rule. The first and last tests build the same tree in two
Headless instances with the others between them, and must draw and count the
same."""

import threading
from collections.abc import Callable
from typing import Any

import pytest

import gweni

pytestmark = pytest.mark.gpu

_first: dict[str, Any] = {}


def _build_and_capture(path: str) -> tuple[bytes, dict[str, Any], bytes]:
    with gweni.Headless(size=(320, 200)) as h:
        panel = h.canvas.add(gweni.Panel, dock=gweni.Dock.CENTER)
        panel.layout = gweni.Layout.VERTICAL
        panel.add(gweni.Label, text="order independence")
        panel.add(gweni.Button, text="Press")
        h.frame(3)
        h.move(40, 30)
        h.frame()
        h.save_png(path)
        with open(path, "rb") as f:
            png = f.read()
        return png, h.stats(), h.pixels()


def test_a_first_instance(tmp_path) -> None:
    _first["png"], _first["stats"], _first["pixels"] = _build_and_capture(str(tmp_path / "a.png"))


def test_clock_steps_from_zero(headless: gweni.Headless) -> None:
    assert headless.seconds == 0.0
    headless.frame(6)
    assert headless.frame_count == 6
    assert headless.seconds == pytest.approx(0.1)
    headless.press(1, 1)
    assert headless.seconds == pytest.approx(1.1)
    headless.release(1, 1)


def test_capture_and_stats(headless: gweni.Headless, tmp_path) -> None:
    assert headless.stats() == {}
    with pytest.raises(RuntimeError):
        headless.save_png(str(tmp_path / "early.png"))
    headless.canvas.add(gweni.Label, text="stats")
    headless.frame()
    stats = headless.stats()
    assert list(stats)[:2] == ["frame", "control"]
    assert stats["frame"] == 0 and stats["control"] == "Headless"
    assert isinstance(stats["text_slots_written"], int) and stats["text_slots_written"] > 0
    headless.frame()
    assert headless.stats()["text_slots_written"] == 0  # settled
    w, h = headless.pixel_size
    assert (w, h) == (640, 480)
    assert len(headless.pixels()) == w * h * 4
    headless.save_png(str(tmp_path / "x.png"))
    assert (tmp_path / "x.png").read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"


def test_scale_sizes_the_target() -> None:
    with gweni.Headless(size=(100, 50), scale=2.0) as h:
        assert h.pixel_size == (200, 100)


def test_input_steps() -> None:
    with gweni.Headless() as h:
        h.frame()
        h.move(10, 10)
        h.click(10, 10)
        h.key("ctrl+a")
        h.key("return")
        h.type("héllo")
        assert h.wheel(-60) is False  # nothing scrolls on an empty canvas
        with pytest.raises(ValueError):
            h.key("ctrl+nonsense")
        h.frame()


def test_one_instance_at_a_time(headless: gweni.Headless) -> None:
    with pytest.raises(RuntimeError, match="alive"):
        gweni.Headless()
    headless.frame()  # the refused one took nothing from it


def test_one_after_another() -> None:
    for _ in range(2):
        h = gweni.Headless()
        h.frame()
        h.close()
        assert h.closed


def _off_thread(fn: Callable[[], object]) -> BaseException | None:
    caught: list[BaseException] = []

    def run() -> None:
        try:
            fn()
        except BaseException as e:  # noqa: BLE001 - handed back to the test
            caught.append(e)

    t = threading.Thread(target=run)
    t.start()
    t.join()
    return caught[0] if caught else None


def test_thread_rule(headless: gweni.Headless) -> None:
    label = headless.canvas.add(gweni.Label, text="mine")
    for fn in (
        lambda: label.text,
        lambda: setattr(label, "text", "theirs"),
        lambda: label.add(gweni.Label),
        lambda: label.alive,
        lambda: headless.frame(),
        lambda: headless.canvas,
        lambda: headless.stats(),
    ):
        assert isinstance(_off_thread(fn), gweni.ThreadError)
    assert label.text == "mine"
    assert "Label" in repr(label)
    # off the thread a live control is not called deleted: its repr says
    # whose thread it is on
    off = _off_thread_repr(label)
    assert "#" in off and "deleted" not in off and "thread" in off, off


def _off_thread_repr(handle: gweni.Control) -> str:
    out: list[str] = []
    t = threading.Thread(target=lambda: out.append(repr(handle)))
    t.start()
    t.join()
    return out[0]


def test_z_second_instance_draws_and_counts_as_the_first(tmp_path) -> None:
    png, stats, pixels = _build_and_capture(str(tmp_path / "z.png"))
    assert pixels == _first["pixels"]
    assert png == _first["png"]
    assert stats == _first["stats"]
