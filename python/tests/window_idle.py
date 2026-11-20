#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""python_window_idle: gweni.App's run() in a real window (run under an X server
of its own, python/tests/window_idle.cmake). On the feature's example window
(a label counting a button's clicks), once it has settled:

  1. run() from another thread raises RuntimeError;
  2. a 2 s idle draws no frame and takes under 0.1 s of the process's CPU
     (getrusage, every thread);
  3. a post from another thread wakes the loop and draws exactly one frame;
  4. SIGINT to the idle run() raises KeyboardInterrupt within 0.5 s;
  5. the wakeup fd set before run() got the signal's byte (run() passed it on)
     and is set again after it (set_wakeup_fd gives it back);
  6. with a Spinner turning (a frame every iteration, so run() never waits),
     a worker thread still runs: its canvas.post lands within 0.5 s of the
     post (run() holds the GIL only to run Python).

Before the example's window, two Apps of their own:

  7. one made and run on another thread: run() raises RuntimeError for being
     off the main thread (the thread rule passes there, the App being that
     thread's);
  8. App(resources=...) reads the directory given: a copy of the package's
     resources/ loads complete, an empty directory leaves the App degraded.

It prints "python_window_idle: passed" and exits 0, or names each failure and
exits 1; a run() that never returns is the test's timeout.
"""

import importlib.resources
import os
import resource
import shutil
import signal
import sys
import tempfile
import threading
import time

import gweni

SETTLE_S = 1.0
IDLE_S = 2.0
CPU_LIMIT_S = 0.1
SIGINT_LIMIT_S = 0.5
BUSY_POST_LIMIT_S = 0.5
BUSY_SLEEP_S = 0.3
BUSY_GIVE_UP_S = 4.0

failures: list[str] = []


def cpu_seconds() -> float:
    usage = resource.getrusage(resource.RUSAGE_SELF)
    return usage.ru_utime + usage.ru_stime


def own_apps() -> None:
    # 7. an App of another thread's: run() refused off the main thread
    caught: list[BaseException] = []

    def other_thread() -> None:
        app = gweni.App(title="Other thread", size=(320, 240))
        try:
            app.run()
        except BaseException as e:  # noqa: BLE001 - checked below
            caught.append(e)
        app.close()

    t = threading.Thread(target=other_thread)
    t.start()
    t.join()
    if not caught or type(caught[0]) is not RuntimeError or "main thread" not in str(caught[0]):
        failures.append(f"run() on the thread that made the App raised {caught!r}, "
                        "not RuntimeError for the main thread")

    # 8. App(resources=...)
    with tempfile.TemporaryDirectory() as scratch:
        copy = os.path.join(scratch, "resources")
        shutil.copytree(str(importlib.resources.files("gweni").joinpath("resources")), copy)
        app = gweni.App(title="Resources", size=(320, 240), resources=copy)
        if app.degraded:
            failures.append(f"App(resources={copy!r}) is degraded: the copy did not load")
        app.close()
        empty = os.path.join(scratch, "empty")
        os.mkdir(empty)
        app = gweni.App(title="No resources", size=(320, 240), resources=empty)
        if not app.degraded:
            failures.append(f"App(resources={empty!r}) is not degraded: the directory given was not read")
        app.close()


def main() -> int:
    own_apps()
    app = gweni.App(title="Hello", size=(800, 600))
    win = app.canvas.add(gweni.Window, title="Settings")
    label = win.add(gweni.Label, text="0 clicks", dock=gweni.Dock.TOP)
    btn = win.add(gweni.Button, text="Click me", dock=gweni.Dock.TOP)
    clicks = 0

    @btn.on_press
    def _(event: gweni.Event) -> None:
        nonlocal clicks
        clicks += 1
        label.text = f"{clicks} clicks"

    # 1. run() off the main thread
    caught: list[BaseException] = []

    def off_main() -> None:
        try:
            app.run()
        except BaseException as e:  # noqa: BLE001 - checked below
            caught.append(e)

    t = threading.Thread(target=off_main)
    t.start()
    t.join()
    # (the thread rule refuses it first, as ThreadError, a RuntimeError)
    if not caught or not isinstance(caught[0], RuntimeError):
        failures.append(f"run() from another thread raised {caught!r}, not RuntimeError")

    # 5. an earlier wakeup fd
    earlier_r, earlier_w = os.pipe()
    os.set_blocking(earlier_w, False)
    signal.set_wakeup_fd(earlier_w)

    measures: dict[str, float] = {}
    posted = threading.Event()

    def poster() -> None:
        try:
            time.sleep(SETTLE_S)
            frames0, cpu0 = app.frame_count, cpu_seconds()
            time.sleep(IDLE_S)
            measures["idle_frames"] = app.frame_count - frames0
            measures["idle_cpu"] = cpu_seconds() - cpu0

            frames1 = app.frame_count
            ok = canvas.post(lambda: setattr(label, "text", "posted"))
            measures["post_ok"] = float(ok)
            time.sleep(0.5)
            measures["post_frames"] = app.frame_count - frames1
            time.sleep(0.5)
            measures["after_post_frames"] = app.frame_count - frames1
            posted.set()

            measures["sigint_at"] = time.monotonic()
            os.kill(os.getpid(), signal.SIGINT)
            # a loop the signal did not wake is woken here, so the latency is
            # measured and reported rather than the run hanging
            time.sleep(2 * SIGINT_LIMIT_S + 1.0)
            canvas.post(lambda: None)
        except BaseException as e:  # noqa: BLE001
            failures.append(f"the driver thread raised {e!r}")
            posted.set()
            os.kill(os.getpid(), signal.SIGINT)

    canvas = app.canvas
    threading.Thread(target=poster, daemon=True).start()
    interrupted_at = None
    try:
        app.run()
        failures.append("run() returned without KeyboardInterrupt")
    except KeyboardInterrupt:
        interrupted_at = time.monotonic()

    restored = signal.set_wakeup_fd(-1)
    os.set_blocking(earlier_r, False)
    try:
        earlier_bytes = os.read(earlier_r, 64)
    except BlockingIOError:
        earlier_bytes = b""

    print(f"python_window_idle: idle frames={measures.get('idle_frames')} cpu_s={measures.get('idle_cpu')}")
    print(f"python_window_idle: after post frames={measures.get('post_frames')} "
          f"then {measures.get('after_post_frames')}")
    if measures.get("idle_frames") != 0:
        failures.append(f"the {IDLE_S} s idle drew {measures.get('idle_frames')} frames")
    if measures.get("idle_cpu", 1.0) >= CPU_LIMIT_S:
        failures.append(f"the {IDLE_S} s idle took {measures.get('idle_cpu')} s of CPU (limit {CPU_LIMIT_S})")
    if measures.get("post_ok") != 1.0:
        failures.append("canvas.post() from another thread returned False")
    if measures.get("post_frames") != 1 or measures.get("after_post_frames") != 1:
        failures.append(f"the post drew {measures.get('post_frames')} frames "
                        f"({measures.get('after_post_frames')} a second later), not exactly 1")
    if label.text != "posted":
        failures.append(f"the posted function did not run: the label reads {label.text!r}")
    if interrupted_at is not None and "sigint_at" in measures:
        latency = interrupted_at - measures["sigint_at"]
        print(f"python_window_idle: KeyboardInterrupt after {latency:.3f} s")
        if latency >= SIGINT_LIMIT_S:
            failures.append(f"KeyboardInterrupt came {latency:.3f} s after SIGINT (limit {SIGINT_LIMIT_S})")
    if restored != earlier_w:
        failures.append(f"the wakeup fd after run() is {restored}, not the earlier {earlier_w}")
    if bytes([signal.SIGINT]) not in earlier_bytes:
        failures.append(f"the earlier wakeup fd got {earlier_bytes!r}, not SIGINT's byte")

    # 6. a worker thread while the loop draws every iteration
    spinner = win.add(gweni.Spinner, dock=gweni.Dock.TOP)
    spinner.start()
    busy: dict[str, float] = {}

    def landed() -> None:
        busy["landed_at"] = time.monotonic()
        app.quit()

    def worker() -> None:
        time.sleep(BUSY_SLEEP_S)
        busy["posted_at"] = time.monotonic()
        canvas.post(landed)

    def give_up(signum: int, frame: object) -> None:
        raise TimeoutError()

    # a worker the loop starves never posts: SIGALRM ends that run instead
    signal.signal(signal.SIGALRM, give_up)
    started = time.monotonic()
    threading.Thread(target=worker, daemon=True).start()
    signal.setitimer(signal.ITIMER_REAL, BUSY_GIVE_UP_S)
    frames_before = app.frame_count
    try:
        app.run()
    except TimeoutError:
        failures.append(f"with a Spinner turning, run() was still running after {BUSY_GIVE_UP_S} s "
                        f"(the worker posted {'at ' + format(busy['posted_at'] - started, '.3f') + ' s' if 'posted_at' in busy else 'never'})")
    finally:
        signal.setitimer(signal.ITIMER_REAL, 0)
        signal.signal(signal.SIGALRM, signal.SIG_DFL)
    busy_frames = app.frame_count - frames_before
    if "landed_at" in busy:
        latency = busy["landed_at"] - busy["posted_at"]
        print(f"python_window_idle: busy loop: worker posted at {busy['posted_at'] - started:.3f} s, "
              f"post landed {latency:.3f} s later, {busy_frames} frames")
        if busy["posted_at"] - started >= BUSY_SLEEP_S + BUSY_POST_LIMIT_S:
            failures.append(f"with a Spinner turning, the worker's {BUSY_SLEEP_S} s sleep took "
                            f"{busy['posted_at'] - started:.3f} s")
        if latency >= BUSY_POST_LIMIT_S:
            failures.append(f"with a Spinner turning, the post landed {latency:.3f} s after it "
                            f"(limit {BUSY_POST_LIMIT_S})")
    if busy_frames < 2:
        failures.append(f"the Spinner's run drew {busy_frames} frames: it was not turning")

    app.close()
    for failure in failures:
        print(f"python_window_idle: FAIL: {failure}", file=sys.stderr)
    if failures:
        return 1
    print("python_window_idle: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
