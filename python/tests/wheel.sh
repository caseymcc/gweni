#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  python_wheel (python/tests/tests.cmake, the release tree only, no gpu
#  label): the wheel built, repaired, installed and used as an app would,
#  offline.
#
#      wheel.sh <release tree>
#
#  Its venvs and the directory the smoke set runs in are a mktemp -d outside
#  the checkout, removed when it passes and kept (its path printed) when not.
#
#  1. tools/build_wheel.sh in the persistent build_linux_x64_release_wheel_installed/
#     (a rerun is incremental), with the release tree's vcpkg_installed and no
#     manifest install (vcpkg builds and fetches nothing) and uv --offline:
#     the wheel it makes (its name and tag are checked on the repaired one);
#  2. tools/repair_wheel.py: auditwheel's manylinux_2_35_x86_64 repair with the
#     glvnd libraries left the system's, and its checks (its header: the tag,
#     nothing GL vendored, GLFW's X11 loaded at run time, libstdc++ within the
#     policy, the licenses);
#  3. wheelChecks.sh on the repaired wheel (its header: the name and
#     Requires-Python, pip and uv installs into fresh 3.12 venvs, wheel_smoke.py
#     from each under `mm run --lock render`, Jammy's pip3 refusing it).
#
#  GWENI_PYTHON names the 3.12 interpreter (the image's venv by default).
#
set -euo pipefail

tree="$(realpath "$1")"
scratch="$(mktemp -d /tmp/gweni-python-wheel.XXXXXX)"
repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"

fail() { echo "python_wheel: FAIL: $* (kept $scratch)" >&2; exit 1; }

[ -x "$python" ] || fail "no Python at $python (GWENI_PYTHON); ./run_docker.sh -r rebuilds the image with it"
[ -d "$tree/vcpkg_installed" ] || fail "no $tree/vcpkg_installed: configure and build the release tree first"
command -v uv >/dev/null || fail "no uv on PATH"

mkdir -p "$scratch/dist"

# 1. the build
"$repo/tools/build_wheel.sh" --vcpkg-installed "$tree/vcpkg_installed" --offline --out "$scratch/dist" \
    > "$scratch/build.log" 2>&1 || { tail -40 "$scratch/build.log"; fail "tools/build_wheel.sh failed ($scratch/build.log)"; }

# 2. the repair and its checks
linux="$(ls "$scratch/dist"/*.whl 2>/dev/null || true)"
[ "$(echo "$linux" | wc -w)" -eq 1 ] || fail "tools/build_wheel.sh made $(ls "$scratch/dist"), not one wheel"
"$python" "$repo/tools/repair_wheel.py" --out "$scratch/repaired" "$linux" \
    || fail "tools/repair_wheel.py failed on $(basename "$linux")"

# 3. the repaired wheel installed and used
bash "$repo/python/tests/wheelChecks.sh" "$scratch/repaired"/*.whl "$scratch/check" || fail "wheelChecks.sh failed"

rm -rf "$scratch"
echo "python_wheel: passed"
