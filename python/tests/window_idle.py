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
     and is set again after it (set_wakeup_fd gives it back).

It prints "python_window_idle: passed" and exits 0, or names each failure and
exits 1; a run() that never returns is the test's timeout.
"""

import os
import resource
import signal
import sys
import threading
import time

import gweni

SETTLE_S = 1.0
IDLE_S = 2.0
CPU_LIMIT_S = 0.1
SIGINT_LIMIT_S = 0.5

failures: list[str] = []


def cpu_seconds() -> float:
    usage = resource.getrusage(resource.RUSAGE_SELF)
    return usage.ru_utime + usage.ru_stime


def main() -> int:
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

    app.close()
    for failure in failures:
        print(f"python_window_idle: FAIL: {failure}", file=sys.stderr)
    if failures:
        return 1
    print("python_window_idle: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
