#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""Events: callbacks added through event methods (as decorators too), the
Event they get, errors raised after the event, the re-entry rule, callables
going with their control, canvas.post() from any thread, and the atexit hook."""

import gc
import os
import subprocess
import sys
import threading
import weakref

import pytest

import gweni

pytestmark = pytest.mark.gpu


def _button(h: gweni.Headless, **props: object) -> tuple[gweni.Button, tuple[int, int]]:
    button = h.canvas.add(gweni.Button, dock=gweni.Dock.TOP, **props)
    h.frame()
    x, y, w, hh = button.bounds
    assert w > 0 and hh > 0
    return button, (x + w // 2, y + hh // 2)


def test_press_calls_back_with_an_event(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")
    events: list[gweni.Event] = []

    @button.on_press
    def pressed(event: gweni.Event) -> None:
        events.append(event)

    assert callable(pressed)  # the decorator gives the function back
    headless.click(x, y)
    headless.frame()
    assert len(events) == 1
    assert events[0].control == button
    assert type(events[0].control) is gweni.Button
    assert "Event" in repr(events[0])


def test_method_form_and_several_callbacks(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")
    calls: list[str] = []
    button.on_press(lambda e: calls.append("a"))
    button.on_press(lambda e: calls.append("b"))
    headless.click(x, y)
    assert calls == ["a", "b"]


def test_toggle_events_from_a_setter(headless: gweni.Headless) -> None:
    button, _ = _button(headless, text="Toggle", is_toggle=True)
    calls: list[str] = []
    button.on_toggle(lambda e: calls.append("toggle"))
    button.on_toggle_on(lambda e: calls.append("on"))
    button.on_toggle_off(lambda e: calls.append("off"))
    button.toggled = True
    button.toggled = False
    assert calls == ["toggle", "on", "toggle", "off"]


def test_hover_events(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Hover")
    calls: list[str] = []
    button.on_hover_enter(lambda e: calls.append("enter"))
    button.on_hover_leave(lambda e: calls.append("leave"))
    headless.move(x, y)
    headless.frame()
    assert calls == ["enter"]
    headless.move(x, 470)
    headless.frame()
    assert calls == ["enter", "leave"]


class Boom(Exception):
    pass


def test_an_error_in_a_callback_is_raised_after_the_event(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")
    after: list[str] = []

    def fail(event: gweni.Event) -> None:
        raise Boom("first")

    button.on_press(fail)
    button.on_press(lambda e: after.append("ran"))  # the event goes on past it
    with pytest.raises(Boom, match="first"):
        headless.click(x, y)
    assert after == ["ran"]
    # the UI is usable: the next frame and press work, and raise again
    headless.frame()
    with pytest.raises(Boom):
        headless.click(x, y)
    assert after == ["ran", "ran"]


def test_later_errors_are_chained_to_the_first(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")

    def first(event: gweni.Event) -> None:
        raise Boom("first")

    def second(event: gweni.Event) -> None:
        raise KeyError("second")

    button.on_press(first)
    button.on_press(second)
    with pytest.raises(Boom) as raised:
        headless.click(x, y)
    assert isinstance(raised.value.__context__, KeyError)


def test_an_error_whose_context_is_the_first_closes_no_loop(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")
    first_error = Boom("first")

    def first(event: gweni.Event) -> None:
        raise first_error

    def second(event: gweni.Event) -> None:
        # an error that already hangs off the first: linking it after the
        # first would close the chain into a loop
        error = KeyError("second")
        error.__context__ = first_error
        raise error

    def third(event: gweni.Event) -> None:
        raise ValueError("third")

    for callback in (first, second, third):
        button.on_press(callback)
    with pytest.raises(Boom) as raised:
        headless.click(x, y)
    chain: list[BaseException] = []
    error: BaseException | None = raised.value
    while error is not None:
        assert error not in chain, f"the __context__ chain loops: {chain!r}"
        chain.append(error)
        error = error.__context__
    assert [type(e) for e in chain] == [Boom, KeyError, ValueError]


def test_an_error_from_a_setters_event(headless: gweni.Headless) -> None:
    button, _ = _button(headless, text="Toggle", is_toggle=True)

    def fail(event: gweni.Event) -> None:
        raise Boom("from the setter")

    button.on_toggle(fail)
    with pytest.raises(Boom):
        button.toggled = True
    assert button.toggled  # the set itself happened


def test_reentry_is_refused(headless: gweni.Headless) -> None:
    button, (x, y) = _button(headless, text="Press")
    for drive in (lambda: headless.frame(), lambda: headless.click(1, 1), lambda: headless.close()):
        callbacks_done: list[bool] = []

        def reenter(event: gweni.Event, drive=drive, done=callbacks_done) -> None:
            drive()
            done.append(True)

        button.on_press(reenter)
        with pytest.raises(RuntimeError, match="inside a callback"):
            headless.click(x, y)
        assert callbacks_done == []
    assert not headless.closed
    # setters and the tree stay open to a callback
    button.on_press(lambda e: setattr(button, "text", "renamed"))
    with pytest.raises(RuntimeError):  # the earlier callbacks still refuse
        headless.click(x, y)
    assert button.text == "renamed"


class Counter:
    def __init__(self) -> None:
        self.calls = 0

    def __call__(self, event: gweni.Event) -> None:
        self.calls += 1


def test_a_callback_goes_with_its_control(headless: gweni.Headless) -> None:
    button, _ = _button(headless, text="Press")
    counter = Counter()
    button.on_press(counter)
    gone = weakref.ref(counter)
    del counter
    gc.collect()
    assert gone() is not None  # the control holds it
    button.delete_later()
    headless.frame()
    gc.collect()
    assert gone() is None


def test_post_from_another_thread(headless: gweni.Headless) -> None:
    ran_on: list[int] = []
    canvas = headless.canvas

    def post() -> None:
        assert canvas.post(lambda: ran_on.append(threading.get_ident()))

    t = threading.Thread(target=post)
    t.start()
    t.join()
    assert ran_on == []  # at the top of the next update
    headless.frame()
    assert ran_on == [threading.get_ident()]


def test_a_posted_error_is_raised_by_frame(headless: gweni.Headless) -> None:
    def fail() -> None:
        raise Boom("posted")

    headless.canvas.post(fail)
    with pytest.raises(Boom, match="posted"):
        headless.frame()
    headless.frame()


def test_post_after_close_is_dropped() -> None:
    with gweni.Headless() as h:
        canvas = h.canvas
    assert canvas.post(lambda: None) is False


_EXIT_SCRIPT = """
import gweni
h = gweni.Headless()
b = h.canvas.add(gweni.Button, text="x")
b.on_press(lambda e: None)
h.canvas.post(lambda: None)
h.frame()
print("open at exit")
"""


def test_a_session_left_open_is_closed_at_exit() -> None:
    result = subprocess.run([sys.executable, "-c", _EXIT_SCRIPT], env=os.environ.copy(),
                            capture_output=True, text=True, timeout=120)
    assert result.returncode == 0, result.stderr
    assert result.stdout.strip() == "open at exit"
    assert "leak" not in result.stderr, result.stderr
