#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  The checks of a built wheel as an app would use it, offline: python_wheel's
#  (wheel.sh) on the repaired wheel, and tools/ci_wheel.sh's.
#
#      wheelChecks.sh <gweni-...-cp312-abi3-manylinux_2_35_x86_64.whl> <scratch dir>
#
#  The venvs and the directory the smoke set runs in go into <scratch dir>,
#  which the caller removes. Each failure is one `<label>: FAIL: ...` line
#  (GWENI_CHECK_LABEL, python_wheel by default) and exit 1.
#
#  1. the wheel is the only file in its directory, named
#     gweni-<project() version>-cp312-abi3-manylinux_2_35_x86_64.whl, and its
#     METADATA says Requires-Python: >=3.12;
#  2. installed into a fresh venv on the image's CPython 3.12 with pip, and
#     into another with uv pip install, both --no-index from the wheelhouse
#     (GWENI_WHEELHOUSE) and the wheel's directory;
#  3. wheel_smoke.py run by each venv's interpreter from a directory outside
#     the checkout: its one GL step, under `mm run --lock render` when mm is on
#     PATH (unlocked otherwise, saying so);
#  4. Jammy's pip3 (Python 3.10) finds no wheel it can install.
#
#  GWENI_PYTHON names the 3.12 interpreter (the image's venv by default).
#
set -euo pipefail

wheel="$(realpath "$1")"
scratch="$(realpath -m "$2")"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"
wheelhouse="${GWENI_WHEELHOUSE:-$(dirname "$(dirname "$(dirname "$python")")")/wheelhouse}"
label="${GWENI_CHECK_LABEL:-python_wheel}"
dist="$(dirname "$wheel")"

fail() { echo "$label: FAIL: $*" >&2; exit 1; }

[ -x "$python" ] || fail "no Python at $python (GWENI_PYTHON); ./run_docker.sh -r rebuilds the image with it"
[ -d "$wheelhouse" ] || fail "no wheelhouse at $wheelhouse (GWENI_WHEELHOUSE)"
command -v uv >/dev/null || fail "no uv on PATH"
mkdir -p "$scratch/elsewhere"

# 1. the wheel
version="$(sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt" | tr -d '\r')"
[ "$(basename "$wheel")" = "gweni-$version-cp312-abi3-manylinux_2_35_x86_64.whl" ] \
    || fail "the wheel is $(basename "$wheel"), not gweni-$version-cp312-abi3-manylinux_2_35_x86_64.whl"
[ "$(ls "$dist" | wc -l)" -eq 1 ] || fail "more than one file beside the wheel: $(ls "$dist")"
"$python" - "$wheel" <<'PY' || fail "the wheel's METADATA does not say Requires-Python: >=3.12"
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    meta = next(n for n in z.namelist() if n.endswith(".dist-info/METADATA"))
    assert "Requires-Python: >=3.12" in z.read(meta).decode().splitlines()
PY

# 2. two fresh venvs on the image's 3.12, one per installer
base="$("$python" -c 'import sys, pathlib; print(pathlib.Path(sys.base_prefix) / "bin" / "python3.12")')"
[ -x "$base" ] || fail "no base interpreter $base"
"$base" -m venv "$scratch/pip-venv"
"$scratch/pip-venv/bin/python" -m pip install --quiet --no-index --find-links "$wheelhouse" \
    --find-links "$dist" gweni || fail "pip install of the wheel failed"
uv venv --quiet --offline --python "$base" "$scratch/uv-venv"
uv pip install --quiet --offline --python "$scratch/uv-venv/bin/python" --no-index \
    --find-links "$wheelhouse" --find-links "$dist" gweni || fail "uv pip install of the wheel failed"

# 3. the smoke set from the installed packages, from outside the checkout
smoke() {
    cd "$scratch/elsewhere"
    for venv in pip-venv uv-venv; do
        env -u PYTHONPATH PYTHONDONTWRITEBYTECODE=1 \
            "$scratch/$venv/bin/python" "$repo/python/tests/wheel_smoke.py" "$repo" \
            || { echo "$label: FAIL: the smoke set failed in $venv" >&2; return 1; }
        echo "$label: smoke passed in $venv"
    done
}
if command -v mm >/dev/null; then
    export -f smoke
    export scratch repo label
    mm run --lock render -- bash -c smoke || fail "the smoke set failed"
else
    echo "$label: no mm on PATH: the GL step runs unlocked"
    smoke || fail "the smoke set failed"
fi

# 4. Jammy's pip (3.10) has no wheel to install
if /usr/bin/python3 -m pip download --quiet --no-deps --no-index --find-links "$dist" \
        -d "$scratch/py310" gweni > "$scratch/py310.log" 2>&1; then
    fail "Python 3.10's pip took the wheel"
fi
grep -q "No matching distribution" "$scratch/py310.log" || { cat "$scratch/py310.log"; fail "pip3 failed for another reason"; }
echo "$label: Python 3.10's pip finds no installable wheel"
