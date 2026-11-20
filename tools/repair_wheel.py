#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
"""Makes the wheel tools/build_wheel.sh builds one PyPI takes, and checks it.

    repair_wheel.py [--out DIR] <gweni-...-linux_x86_64.whl>
    repair_wheel.py --check <gweni-...-manylinux_2_35_x86_64.whl>

Run it with the image's CPython 3.12 venv (GWENI_PYTHON), which holds
auditwheel and patchelf; the venv's bin/ is put on PATH for auditwheel.

The repair is `auditwheel repair --plat manylinux_2_35_x86_64` with the glvnd
libraries the module links excluded (libEGL.so.1, libOpenGL.so.0,
libGLX.so.0, libGLdispatch.so.0, and libGL.so.1 should it appear): all but
libGL.so.1 are on no manylinux whitelist, and a vendored glvnd breaks the
dispatch to the machine's driver, so they stay the system's and the wheel needs a glvnd GL
driver, as the container-built exes do. The repaired wheel goes into --out
(default: the input's directory) and its path is printed last.

Then (and with --check, on a wheel alone) it fails, one line per check, unless:
  1. the wheel's name and its WHEEL file's one Tag are
     cp312-abi3-manylinux_2_35_x86_64 (the repair passes --only-plat, so the
     name has that one platform tag), and the symbol versions `auditwheel show
     --json` gives (sym_tag) are manylinux_2_35_x86_64's or an older one's;
  2. nothing GL is vendored: the wheel holds one shared object,
     gweni/_gweni.abi3.so (no gweni.libs/), and the only libraries keeping it
     from manylinux_2_35 in `auditwheel show` (libs_to_eliminate) are the
     excluded glvnd ones;
  3. GLFW's X11 and Wayland libraries are loaded at run time: no libX*,
     libxcb*, libwayland*, libxkbcommon* or libdecor* is NEEDED by the module
     (readelf -d), which names libX11.so.6 for GLFW's dlopen;
  4. libstdc++ is the system's, within the policy: libstdc++.so.6 is NEEDED,
     not vendored, on manylinux_2_35's whitelist, and every GLIBCXX and CXXABI
     version the module asks of it is one the policy allows (the container's
     GCC 11 needs GLIBCXX_3.4.29, the policy's highest is 3.4.30);
  5. the licenses: the wheel's .dist-info/licenses/ holds gweni's LICENSE, the
     third-party notice (python/THIRD_PARTY_NOTICES.md) and the fonts'
     resources/*-LICENSE.txt, every gweni/licenses/<name>.txt the notice's
     table names is in the package, and each font's license is beside it in
     gweni/resources/.
"""

import argparse
import json
import os
import re
import shutil
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

GLIBC = (2, 35)
POLICY = f"manylinux_{GLIBC[0]}_{GLIBC[1]}"
PLAT = POLICY + "_x86_64"
LEGACY = {"manylinux1": (2, 5), "manylinux2010": (2, 12), "manylinux2014": (2, 17)}
TAG = "cp312-abi3-" + PLAT
GLVND = ["libEGL.so.1", "libOpenGL.so.0", "libGLX.so.0", "libGLdispatch.so.0", "libGL.so.1"]
MODULE = "gweni/_gweni.abi3.so"
RUNTIME_LOADED = re.compile(r"^(libX|libxcb|libwayland|libxkbcommon|libdecor)")
FONTS = ["OpenSans", "LiberationSans", "DejaVuSansMono"]

failures = []


def fail(message):
    failures.append(message)
    print(f"repair_wheel: FAIL: {message}", file=sys.stderr)


def glibc_of(tag):
    """The glibc version of a manylinux x86_64 platform tag, or None."""
    m = re.fullmatch(r"manylinux_(\d+)_(\d+)_x86_64", tag or "")
    if m:
        return (int(m.group(1)), int(m.group(2)))
    m = re.fullmatch(r"(manylinux1|manylinux2010|manylinux2014)_x86_64", tag or "")
    return LEGACY[m.group(1)] if m else None


def tool_env():
    # auditwheel finds patchelf on PATH: the venv's bin/ holds both
    env = dict(os.environ)
    env["PATH"] = str(Path(sys.executable).parent) + os.pathsep + env.get("PATH", "")
    return env


def auditwheel(*args):
    return subprocess.run([str(Path(sys.executable).parent / "auditwheel"), *args],
                          env=tool_env(), capture_output=True, text=True)


def repair(wheel, out):
    out.mkdir(parents=True, exist_ok=True)
    scratch = Path(tempfile.mkdtemp(prefix="gweni-repair-"))
    try:
        excludes = [arg for lib in GLVND for arg in ("--exclude", lib)]
        result = auditwheel("repair", "--plat", PLAT, "--only-plat", *excludes, "-w", str(scratch), str(wheel))
        if result.returncode != 0:
            sys.stderr.write(result.stdout + result.stderr)
            sys.exit(f"repair_wheel: FAIL: auditwheel repair exited {result.returncode}")
        made = list(scratch.glob("*.whl"))
        if len(made) != 1:
            sys.exit(f"repair_wheel: FAIL: auditwheel repair made {[p.name for p in made]}, not one wheel")
        target = out / made[0].name
        shutil.move(str(made[0]), target)
        return target
    finally:
        shutil.rmtree(scratch, ignore_errors=True)


def load_policy():
    import auditwheel as package  # noqa: the venv's, for its policy file
    with open(Path(package.__file__).parent / "policy" / "manylinux-policy.json") as f:
        return next(p for p in json.load(f) if p["name"] == POLICY)


def needed(module):
    result = subprocess.run(["readelf", "-d", str(module)], capture_output=True, text=True, check=True)
    return re.findall(r"\(NEEDED\)\s+Shared library: \[([^\]]+)\]", result.stdout)


def check(wheel):
    # 1. the tag
    if not wheel.name.endswith(f"-{TAG}.whl"):
        fail(f"the wheel's name {wheel.name} does not end in -{TAG}.whl")
    with zipfile.ZipFile(wheel) as z:
        names = z.namelist()
        dist_info = next((n.split("/")[0] for n in names if n.endswith(".dist-info/WHEEL")), None)
        if dist_info is None:
            fail("the wheel has no .dist-info/WHEEL")
            return
        tags = [line.split(":", 1)[1].strip() for line in z.read(f"{dist_info}/WHEEL").decode().splitlines()
                if line.startswith("Tag:")]
        if tags != [TAG]:
            fail(f"the WHEEL file's tags are {tags}, not [{TAG!r}]")
        scratch = Path(tempfile.mkdtemp(prefix="gweni-wheel-check-"))
        try:
            z.extractall(scratch)
            check_contents(wheel, names, dist_info, scratch)
        finally:
            shutil.rmtree(scratch, ignore_errors=True)


def check_contents(wheel, names, dist_info, root):
    show = auditwheel("show", "--json", str(wheel))
    if show.returncode != 0:
        sys.stderr.write(show.stdout + show.stderr)
        fail(f"auditwheel show exited {show.returncode}")
        return
    report = json.loads(show.stdout)
    sym = glibc_of(report.get("sym_tag"))
    if sym is None or sym > GLIBC:
        fail(f"auditwheel show gives the symbol versions the tag {report.get('sym_tag')}, not {PLAT} or older")

    # 2. nothing GL vendored, and only glvnd outside the policy
    objects = [n for n in names if re.search(r"\.so(\.|$)", n)]
    if objects != [MODULE]:
        fail(f"the wheel holds the shared objects {objects}, not [{MODULE!r}] alone (something was vendored)")
    if any(n.startswith("gweni.libs/") for n in names):
        fail("the wheel has a gweni.libs/ directory (something was vendored)")
    eliminate = report.get("policy_upgrades", {}).get(PLAT, {}).get("libs_to_eliminate", [])
    outside = sorted(set(eliminate) - set(GLVND))
    if outside:
        fail(f"auditwheel show finds libraries outside {PLAT} that are not glvnd's: {outside}")

    module = root / MODULE
    if not module.is_file():
        fail(f"no {MODULE} in the wheel")
        return
    libs = needed(module)

    # 3. GLFW's window-system libraries are dlopen()ed, not NEEDED
    linked = [lib for lib in libs if RUNTIME_LOADED.match(lib)]
    if linked:
        fail(f"the module NEEDs window-system libraries GLFW should load at run time: {linked}")
    if b"libX11.so.6\0" not in module.read_bytes():
        fail("the module does not name libX11.so.6 (GLFW's X11 dlopen)")

    # 4. libstdc++ the system's, within the policy
    policy = load_policy()
    if "libstdc++.so.6" not in libs:
        fail(f"the module does not NEED the system's libstdc++.so.6 (NEEDED: {libs})")
    if "libstdc++.so.6" not in policy["lib_whitelist"]:
        fail(f"libstdc++.so.6 is not on {POLICY}'s whitelist")
    allowed = policy["symbol_versions"]["x86_64"]
    for version in report.get("versioned_symbols", {}).get("libstdc++.so.6", []):
        name, _, number = version.partition("_")
        if number not in allowed.get(name, []):
            fail(f"the module asks libstdc++ for {version}, which {POLICY} does not allow")
    unknown = sorted(set(libs) - set(policy["lib_whitelist"]) - set(GLVND) - {"ld-linux-x86-64.so.2"})
    if unknown:
        fail(f"the module NEEDs libraries outside {POLICY}'s whitelist and glvnd: {unknown}")

    # 5. the licenses
    licenses = f"{dist_info}/licenses/"
    for path in ["LICENSE", "python/THIRD_PARTY_NOTICES.md"] + [f"resources/{f}-LICENSE.txt" for f in FONTS]:
        if licenses + path not in names:
            fail(f"the wheel has no {licenses}{path}")
        if path.startswith("resources/") and f"gweni/{path}" not in names:
            fail(f"the wheel has no gweni/{path} beside the face")
    notice = root / licenses / "python/THIRD_PARTY_NOTICES.md"
    if notice.is_file():
        named = re.findall(r"^\|[^\n]*\| `([a-z0-9-]+\.txt)` \|$", notice.read_text(), re.M)
        if not named:
            fail("the third-party notice's table names no gweni/licenses/ file")
        for name in named:
            if f"gweni/licenses/{name}" not in names:
                fail(f"the third-party notice names gweni/licenses/{name}, which the wheel does not hold")


def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n")[0])
    parser.add_argument("wheel", type=Path)
    parser.add_argument("--out", type=Path, help="where the repaired wheel goes (default: beside the input)")
    parser.add_argument("--check", action="store_true", help="check a repaired wheel, repair nothing")
    args = parser.parse_args()
    if not (Path(sys.executable).parent / "auditwheel").is_file():
        # the shebang's python3 is Jammy's 3.10 in the image, which has none
        sys.exit(f"repair_wheel: FAIL: no auditwheel beside {sys.executable}: run it with GWENI_PYTHON "
                 "(\"$GWENI_PYTHON\" tools/repair_wheel.py ...)")
    wheel = args.wheel.resolve()
    if not wheel.is_file():
        sys.exit(f"repair_wheel: FAIL: no wheel {wheel}")
    if not args.check:
        wheel = repair(wheel, (args.out or wheel.parent).resolve())
    check(wheel)
    if failures:
        sys.exit(f"repair_wheel: FAIL: {len(failures)} check(s) failed on {wheel.name}")
    print(f"repair_wheel: {wheel.name} passes: the tag, nothing GL vendored, X11 loaded at run time, "
          "libstdc++ within the policy, the licenses")
    print(wheel)


if __name__ == "__main__":
    main()
