#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""ButtonDemo from Python: the tree examples/demo/button.cpp builds, control
for control and property for property, so it draws what
``ControlTest --control ButtonDemo`` draws (python_parity checks it byte for
byte).

    python button_demo.py out.png    # a headless capture
"""

import sys

import gweni


def build(parent: gweni.Control) -> gweni.Panel:
    """Builds ButtonDemo's buttons in a host docked in the centre of parent."""
    host = parent.add(
        gweni.Panel,
        dock=gweni.Dock.CENTER,
        size_flags=(gweni.SizeFlag.FIXED, gweni.SizeFlag.FIXED),
        size=(100, 100),
        layout=gweni.Layout.VERTICAL,
    )
    host.add(gweni.Button, text="Event Tester")
    host.add(gweni.Button, text="Text")
    host.add(gweni.Button, text="Image Button")
    host.add(
        gweni.Button,
        text="",
        size_flags=(gweni.SizeFlag.FIXED, gweni.SizeFlag.FIXED),
        size=(20, 20),
    )
    host.add(gweni.Button, text="Toggle Me", is_toggle=True, tooltip="Stays down until clicked again")
    host.add(gweni.Button, text="Disabled :D", enabled=False)
    host.add(gweni.Button, text="With Tooltip", tooltip="This is a tooltip!")
    return host


def main(argv: list[str]) -> int:
    if len(argv) != 2:
        print(f"usage: {argv[0]} <out.png>", file=sys.stderr)
        return 2
    with gweni.Headless() as h:
        build(h.canvas)
        h.frame(3)
        h.save_png(argv[1])
    return 0


if __name__ == "__main__":
    sys.exit(main(sys.argv))
