#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""python_parity: ButtonDemo built from Python (python/examples/button_demo.py)
against ``ControlTest --headless --control ButtonDemo`` on the same script.

    parity.py <ControlTest> <scratch dir>

It fails, naming each difference, unless
  1. the settled capture (three frames) and the capture after a move onto
     "Toggle Me" (one frame more) are byte-identical PNGs;
  2. on --stats --stats-clock step --frames 3 --move 29,80 --frames 2 (the
     Headless clock steps every frame, as step does), every counter of every
     frame is ControlTest's, but for the differences of the host, listed
     below; in particular the settled frames (1, 2, 4) write no primitive or
     glyph slot and lay nothing out, and the hover frame (3) writes the slots
     ControlTest's does;
  3. the host costs exactly one primitive id and its mirror bytes in every
     frame.

Like with like: ControlTest's ButtonDemo is a control type of its own that
draws through GWENI_DEMO_TRANSPARENT (no primitive), whose constructor builds
the buttons before it is attached, so their first changes are not counted on
any canvas. From Python the host is a Panel (a plain Base, whose textured
Control holds one hidden primitive slot, drawing nothing), filled once it is
on the canvas. So prim_ids and mirror_bytes are one slot more in every frame
(checked to be exactly one), and frame 0's change counts (background, text,
invalidations) and its upload_bytes (the slot more) are left out; the build
frame's slot writes, layout passes and the pixels are not.
"""

import os
import pathlib
import subprocess
import sys

HERE = pathlib.Path(__file__).resolve().parent
sys.path.insert(0, str(HERE.parent / "examples"))

import button_demo  # noqa: E402

import gweni  # noqa: E402

TOGGLE_ME = (29, 80)  # "Toggle Me" is at 0,73 58x15 (controltest.cpp)
SCRIPT = ["--frames", "3", "--move", f"{TOGGLE_ME[0]},{TOGGLE_ME[1]}", "--frames", "2"]
# the counters the host's construction moves, frame 0 only (see above)
BUILD_ONLY = {"background", "text", "invalidations", "upload_bytes"}
# the counters the host's one slot moves, every frame
HOST_SLOT = {"prim_ids", "mirror_bytes"}


def controltest(exe: str, *args: str) -> str:
    result = subprocess.run([exe, "--headless", "--control", "ButtonDemo", *args],
                            capture_output=True, text=True, timeout=300)
    if result.returncode != 0:
        sys.exit(f"python_parity: ControlTest {' '.join(args)} exited {result.returncode}:\n{result.stderr}")
    return result.stdout


def main(argv: list[str]) -> int:
    if len(argv) != 3:
        print(__doc__, file=sys.stderr)
        return 2
    exe, scratch = argv[1], pathlib.Path(argv[2])
    scratch.mkdir(parents=True, exist_ok=True)
    failures: list[str] = []

    # 1. the captures
    controltest(exe, "--shot", str(scratch / "ct_settled.png"))
    controltest(exe, "--frames", "3", "--move", f"{TOGGLE_ME[0]},{TOGGLE_ME[1]}", "--shot",
                str(scratch / "ct_hover.png"))
    with gweni.Headless() as h:
        button_demo.build(h.canvas)
        h.frame(3)
        h.save_png(str(scratch / "py_settled.png"))
        h.move(*TOGGLE_ME)
        h.frame()
        h.save_png(str(scratch / "py_hover.png"))
    for name in ("settled", "hover"):
        a = (scratch / f"ct_{name}.png").read_bytes()
        b = (scratch / f"py_{name}.png").read_bytes()
        if a != b:
            failures.append(f"the {name} captures differ: {scratch}/ct_{name}.png, py_{name}.png")
    if (scratch / "ct_settled.png").read_bytes() == (scratch / "ct_hover.png").read_bytes():
        failures.append("the hover capture is the settled one: the move hovered nothing")

    # 2. and 3. the counters
    lines = [line for line in controltest(exe, "--stats", "--stats-clock", "step", *SCRIPT).splitlines()
             if line.startswith("frame=")]
    expected = [dict(field.split("=", 1) for field in line.split()) for line in lines]
    got: list[dict[str, object]] = []
    with gweni.Headless() as h:
        button_demo.build(h.canvas)
        for _ in range(3):
            h.frame()
            got.append(h.stats())
        h.move(*TOGGLE_ME)
        for _ in range(2):
            h.frame()
            got.append(h.stats())
    if len(expected) != len(got):
        failures.append(f"ControlTest printed {len(expected)} frames, Python drew {len(got)}")
    slot_bytes = None
    for frame, (ct, py) in enumerate(zip(expected, got)):
        for key, value in ct.items():
            if key == "control" or key in HOST_SLOT or (frame == 0 and key in BUILD_ONLY):
                continue
            if str(py.get(key)) != value:
                failures.append(f"frame {frame}: {key}={py.get(key)}, ControlTest's {value}")
        if int(str(py["prim_ids"])) != int(ct["prim_ids"]) + 1:
            failures.append(f"frame {frame}: prim_ids={py['prim_ids']}, not ControlTest's {ct['prim_ids']} + the host's 1")
        delta = int(str(py["mirror_bytes"])) - int(ct["mirror_bytes"])
        if slot_bytes is None:
            slot_bytes = delta
        if delta <= 0 or delta != slot_bytes:
            failures.append(f"frame {frame}: mirror_bytes {py['mirror_bytes']} against {ct['mirror_bytes']}: "
                            f"not one slot more ({slot_bytes} bytes in frame 0)")
    for frame in (1, 2, 4):
        if frame < len(got):
            py = got[frame]
            for key in ("prim_slots_written", "text_slots_written", "layout_passes", "layout_visited"):
                if py[key] != 0:
                    failures.append(f"settled frame {frame}: {key}={py[key]}")
    if len(got) > 3 and got[3]["prim_slots_written"] == 0:
        failures.append("the hover frame wrote no primitive slot: the move hovered nothing")

    for failure in failures:
        print(f"python_parity: {failure}", file=sys.stderr)
    if failures:
        return 1
    print(f"python_parity: ButtonDemo from Python matches ControlTest ({len(got)} frames; host slot {slot_bytes} bytes)")
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
