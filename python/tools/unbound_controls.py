#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""Lists the control types gweni/controls.h includes that the Python module
does not bind (information only: it always exits 0 once it has run).

    python unbound_controls.py [--repo <checkout>]

Run it with the module importable (the staged package on PYTHONPATH, or an
installed wheel); it makes no context. A type is a GWENI_CONTROL or
GWENI_CONTROL_INLINE declaration in one of controls.h's headers; one inside a
namespace internal is listed apart, as a part of another control.
"""

import argparse
import pathlib
import re
import sys

import gweni._gweni as _gweni

INCLUDE = re.compile(r'#include\s*<(gweni/controls/[^>]+)>')
CONTROL = re.compile(r'GWENI_CONTROL(?:_INLINE)?\s*\(\s*(\w+)')
NAMESPACE = re.compile(r'\bnamespace\s+(\w+)\s*(\{)?')


def declared(header: pathlib.Path) -> list[tuple[str, bool]]:
    """(type, inside namespace internal) for each control in header."""
    found: list[tuple[str, bool]] = []
    stack: list[str] = []  # what each open brace opened: a namespace name or ""
    pending: str | None = None
    for line in header.read_text(errors="replace").splitlines():
        code = line.split("//", 1)[0]
        for match in CONTROL.finditer(code):
            found.append((match.group(1), "internal" in stack))
        ns = NAMESPACE.search(code)
        if ns:
            pending = ns.group(1)
        for char in code:
            if char == "{":
                stack.append(pending or "")
                pending = None
            elif char == "}" and stack:
                stack.pop()
    return found


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__.splitlines()[0])
    parser.add_argument("--repo", default=str(pathlib.Path(__file__).resolve().parents[2]))
    args = parser.parse_args()
    include = pathlib.Path(args.repo) / "include"
    umbrella = (include / "gweni" / "controls.h").read_text()
    bound = set(_gweni._bound_type_names())

    public: dict[str, str] = {}
    internal: dict[str, str] = {}
    for rel in INCLUDE.findall(umbrella):
        header = include / rel
        if not header.exists():
            continue
        for name, is_internal in declared(header):
            (internal if is_internal else public).setdefault(name, rel)

    unbound = sorted(name for name in public if name not in bound)
    print(f"bound: {len(bound)} control types; controls.h declares {len(public)} "
          f"(and {len(internal)} internal parts)")
    print(f"not bound ({len(unbound)}):")
    for name in unbound:
        print(f"  {name:28} {public[name]}")
    parts = sorted(name for name in internal if name not in bound)
    print(f"internal parts not bound ({len(parts)}): {', '.join(parts)}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
