#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  python_wheel (python/tests/tests.cmake, the release tree only, no gpu
#  label): the wheel built, installed and used as an app would, offline.
#
#      wheel.sh <release tree>
#
#  Its venvs and the directory the smoke set runs in are a mktemp -d outside
#  the checkout, removed when it passes and kept (its path printed) when not.
#
#  1. tools/build_wheel.sh in the persistent build_linux_x64_release_wheel_installed/
#     (a rerun is incremental), with the release tree's vcpkg_installed and no
#     manifest install (vcpkg builds and fetches nothing) and uv --offline;
#  2. one wheel, gweni-<project() version>-cp312-abi3-linux_x86_64.whl, whose
#     METADATA says Requires-Python: >=3.12;
#  3. installed into a fresh venv on the image's CPython 3.12 with pip, and
#     into another with uv pip install, both --no-index from the wheelhouse
#     (GWENI_WHEELHOUSE) and the wheel's directory;
#  4. wheel_smoke.py run by each venv's interpreter from a directory outside
#     the checkout: its one GL step, under `mm run --lock render` when mm is on
#     PATH (unlocked otherwise, saying so);
#  5. Jammy's pip3 (Python 3.10) finds no wheel it can install.
#
#  GWENI_PYTHON names the 3.12 interpreter (the image's venv by default).
#
set -euo pipefail

tree="$(realpath "$1")"
scratch="$(mktemp -d /tmp/gweni-python-wheel.XXXXXX)"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"
wheelhouse="${GWENI_WHEELHOUSE:-$(dirname "$(dirname "$(dirname "$python")")")/wheelhouse}"

fail() { echo "python_wheel: FAIL: $* (kept $scratch)" >&2; exit 1; }

[ -x "$python" ] || fail "no Python at $python (GWENI_PYTHON); ./run_docker.sh -r rebuilds the image with it"
[ -d "$wheelhouse" ] || fail "no wheelhouse at $wheelhouse (GWENI_WHEELHOUSE)"
[ -d "$tree/vcpkg_installed" ] || fail "no $tree/vcpkg_installed: configure and build the release tree first"
command -v uv >/dev/null || fail "no uv on PATH"

mkdir -p "$scratch/dist" "$scratch/elsewhere"

# 1. the build
"$repo/tools/build_wheel.sh" --vcpkg-installed "$tree/vcpkg_installed" --offline --out "$scratch/dist" \
    > "$scratch/build.log" 2>&1 || { tail -40 "$scratch/build.log"; fail "tools/build_wheel.sh failed ($scratch/build.log)"; }

# 2. the wheel
version="$(sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt" | tr -d '\r')"
wheel="$scratch/dist/gweni-$version-cp312-abi3-linux_x86_64.whl"
[ -f "$wheel" ] || fail "no $(basename "$wheel"): $(ls "$scratch/dist")"
[ "$(ls "$scratch/dist" | wc -l)" -eq 1 ] || fail "more than one file in dist: $(ls "$scratch/dist")"
"$python" - "$wheel" <<'PY' || fail "the wheel's METADATA does not say Requires-Python: >=3.12"
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    meta = next(n for n in z.namelist() if n.endswith(".dist-info/METADATA"))
    assert "Requires-Python: >=3.12" in z.read(meta).decode().splitlines()
PY

# 3. two fresh venvs on the image's 3.12, one per installer
base="$("$python" -c 'import sys, pathlib; print(pathlib.Path(sys.base_prefix) / "bin" / "python3.12")')"
[ -x "$base" ] || fail "no base interpreter $base"
"$base" -m venv "$scratch/pip-venv"
"$scratch/pip-venv/bin/python" -m pip install --quiet --no-index --find-links "$wheelhouse" \
    --find-links "$scratch/dist" gweni || fail "pip install of the wheel failed"
uv venv --quiet --offline --python "$base" "$scratch/uv-venv"
uv pip install --quiet --offline --python "$scratch/uv-venv/bin/python" --no-index \
    --find-links "$wheelhouse" --find-links "$scratch/dist" gweni || fail "uv pip install of the wheel failed"

# 4. the smoke set from the installed packages, from outside the checkout
smoke() {
    cd "$scratch/elsewhere"
    for venv in pip-venv uv-venv; do
        env -u PYTHONPATH PYTHONDONTWRITEBYTECODE=1 \
            "$scratch/$venv/bin/python" "$repo/python/tests/wheel_smoke.py" "$repo" \
            || { echo "python_wheel: FAIL: the smoke set failed in $venv" >&2; return 1; }
        echo "python_wheel: smoke passed in $venv"
    done
}
if command -v mm >/dev/null; then
    export -f smoke
    export scratch repo
    mm run --lock render -- bash -c smoke || fail "the smoke set failed"
else
    echo "python_wheel: no mm on PATH: the GL step runs unlocked"
    smoke || fail "the smoke set failed"
fi

# 5. Jammy's pip (3.10) has no wheel to install
if /usr/bin/python3 -m pip download --quiet --no-deps --no-index --find-links "$scratch/dist" \
        -d "$scratch/py310" gweni > "$scratch/py310.log" 2>&1; then
    fail "Python 3.10's pip took the wheel"
fi
grep -q "No matching distribution" "$scratch/py310.log" || { cat "$scratch/py310.log"; fail "pip3 failed for another reason"; }
echo "python_wheel: Python 3.10's pip finds no installable wheel"

rm -rf "$scratch"
echo "python_wheel: passed"
