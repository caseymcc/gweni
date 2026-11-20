#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""python_wheel's smoke set, run by an interpreter of a fresh venv the wheel
was installed into, from a directory outside the checkout (wheel.sh):

  1. gweni is the installed package (site-packages, not the checkout or a
     build tree) with its module, stub, py.typed, licenses and resources/;
  2. a Headless loads the skin and fonts from the package (degraded False)
     and draws the feature's example: a click on the button counts in the
     label, the capture is a PNG of the canvas's size that is not one colour;
  3. Headless(resources=<a copy of them elsewhere>) draws the same pixels,
     and one from a copy without the default font is degraded (so the
     directory given is the one read).

Prints "wheel_smoke: passed" or raises.
"""

import importlib.resources
import pathlib
import shutil
import sys
import tempfile

import gweni


def example(h: gweni.Headless) -> gweni.Label:
    # the feature's example as a user writes it: no position or size
    win = h.canvas.add(gweni.Window, title="Settings")
    label = win.add(gweni.Label, text="0 clicks", dock=gweni.Dock.TOP)
    btn = win.add(gweni.Button, text="Click me", dock=gweni.Dock.TOP)
    clicks = 0

    @btn.on_press
    def _(event: gweni.Event) -> None:
        nonlocal clicks
        clicks += 1
        label.text = f"{clicks} clicks"

    h.frame()
    x, y, w, hh = btn.canvas_bounds
    h.click(x + w // 2, y + hh // 2)
    h.frame(3)
    return label


def main() -> int:
    checkout = pathlib.Path(sys.argv[1]).resolve()
    package = pathlib.Path(gweni.__file__).resolve().parent
    assert "site-packages" in package.parts, package
    assert checkout not in package.parents, f"{package} is in the checkout {checkout}"
    for name in ("_gweni.abi3.so", "_gweni.pyi", "py.typed", "licenses/freetype.txt",
                 "resources/DefaultSkin.json", "resources/OpenSans.ttf", "resources/OpenSans-LICENSE.txt"):
        assert (package / name).is_file(), f"{name} is not in the installed package"
    resources = pathlib.Path(str(importlib.resources.files("gweni").joinpath("resources")))
    assert resources == package / "resources"

    with tempfile.TemporaryDirectory() as scratch:
        with gweni.Headless(size=(340, 240)) as h:
            assert not h.degraded, "the skin or a font did not load from the package"
            label = example(h)
            assert label.text == "1 clicks", label.text
            png = pathlib.Path(scratch) / "example.png"
            h.save_png(str(png))
            assert png.read_bytes()[:8] == b"\x89PNG\r\n\x1a\n"
            pixels = h.pixels()
            assert len(pixels) == 340 * 240 * 4
            colours = {pixels[i:i + 4] for i in range(0, len(pixels), 4 * 97)}
            assert len(colours) > 4, f"the capture is flat ({len(colours)} colours)"
            stats = h.stats()
            assert stats["frame"] > 0 and "prim_slots_written" in stats

        copy = pathlib.Path(scratch) / "my-resources"
        shutil.copytree(resources, copy)
        with gweni.Headless(size=(340, 240), resources=str(copy)) as h:
            assert not h.degraded
            example(h)
            assert h.pixels() == pixels, "resources= drew other pixels"

        (copy / "OpenSans.ttf").unlink()
        with gweni.Headless(size=(340, 240), resources=str(copy)) as h:
            assert h.degraded, "resources= without OpenSans.ttf loaded it anyway: the package's were read"

    print("wheel_smoke: passed")
    return 0


if __name__ == "__main__":
    sys.exit(main())
