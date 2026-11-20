#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Builds, repairs and checks the gweni wheel PyPI is given, as the PyPI
#  workflow (.github/workflows/pypi.yml) runs it: every step of the workflow's
#  build is here, so the YAML holds no build logic and a run can be repeated by
#  hand.
#
#  Inside the build container (it needs vcpkg and the image's Python), from any
#  directory:
#
#      ./tools/ci_wheel.sh                  # dist/gweni-<ver>-cp312-abi3-manylinux_2_35_x86_64.whl
#      ./tools/ci_wheel.sh --out /tmp/wheel
#      GITHUB_REF_NAME=v0.1.0 ./tools/ci_wheel.sh   # as a tag's run checks it
#
#  Options:
#    --out DIR   where the repaired wheel goes, alone (default dist/ in the
#                repo, git-ignored, and what the workflow uploads); the
#                gweni-*.whl already there are removed before step 1, so a
#                failed run leaves none, and nothing else is.
#    --help      this text.
#
#  In order, stopping at the first failure:
#    1. the tag against project(gweni VERSION ...) in CMakeLists.txt: the tag
#       is GITHUB_REF_NAME when it is set and GITHUB_REF_TYPE is not `branch`
#       (a workflow_dispatch run is a branch's, and has no tag to check), and
#       it must be v<project() version>; and, tag or not, the vcpkg port's
#       version-semver in ports/gweni/vcpkg.json must be that version too.
#       These v* tags are the port's as well, and tools/vcpkg_publish.sh
#       makes the same three-way check: a PyPI upload cannot be taken back, so
#       a forgotten port bump would leave a tag that can never release the
#       port. It comes first, so a wrong version fails before the build.
#    2. tools/build_wheel.sh, in its manifest mode (the persistent
#       build_linux_x64_release_wheel/, vcpkg's binary cache reused): the
#       linux_x86_64 wheel; its METADATA's Version must be the project()
#       version.
#    3. tools/repair_wheel.py: the manylinux_2_35_x86_64 repair with the glvnd
#       libraries left the system's, and its checks (the tag, nothing GL
#       vendored, GLFW's X11 loaded at run time, libstdc++ within the policy,
#       the licenses).
#    4. python/tests/wheelChecks.sh, python_wheel's checks, on the repaired
#       wheel: installed with pip and with uv into fresh 3.12 venvs and the
#       smoke set run from each (its one GL step under `mm run --lock render`
#       when mm is on PATH, unlocked otherwise, as on a GitHub runner, with one
#       line saying so), and Jammy's pip3 finding no wheel to install.
#    5. the repaired wheel copied into --out, its path printed last.
#
#  It takes no lock itself: building takes none. Every build runs at most
#  CMAKE_BUILD_PARALLEL_LEVEL jobs, nproc unless the environment sets it, as
#  tools/ci_site.sh does: an unlimited -j ran the 4-core, 16 GB GitHub runner
#  out of memory (#579).

set -euo pipefail

: "${CMAKE_BUILD_PARALLEL_LEVEL:=$(nproc)}"
export CMAKE_BUILD_PARALLEL_LEVEL

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
out="$repo/dist"
python="${GWENI_PYTHON:-/opt/python/venv/bin/python}"

usage()
{
    sed -n '7,/^$/{s/^# \{0,2\}//;p}' "${BASH_SOURCE[0]}"
}

while [ $# -gt 0 ]; do
    case "$1" in
        --out)
            [ $# -ge 2 ] || { echo "ci_wheel.sh: --out needs a directory" >&2; exit 1; }
            out="$(realpath -m "$2")"; shift 2 ;;
        --help|-h)
            usage; exit 0 ;;
        *)
            echo "ci_wheel.sh: unknown argument: $1" >&2
            usage >&2
            exit 1 ;;
    esac
done

fail() { echo "ci_wheel.sh: FAIL: $*" >&2; exit 1; }
step() { echo; echo "ci_wheel.sh: == $*"; }

[ -n "${VCPKG_ROOT:-}" ] || fail "VCPKG_ROOT is not set: run it inside the build container (./run_docker.sh ./tools/ci_wheel.sh)"
[ -x "$python" ] || fail "no Python at $python (GWENI_PYTHON); from a host, ./run_docker.sh -r rebuilds the image with it"

scratch="$(mktemp -d /tmp/gweni-ci-wheel.XXXXXX)"
trap 'rm -rf "$scratch"' EXIT

# an earlier run's wheel goes first, so a failed run leaves none in --out
mkdir -p "$out"
rm -f "$out"/gweni-*.whl

# 1. the tag and project()
version="$(sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt" | tr -d '\r' | head -n 1)"
[ -n "$version" ] || fail "CMakeLists.txt has no project(gweni VERSION ...)"
step "1. the version: project(gweni VERSION $version)"
port_version="$(sed -n 's/^ *"version-semver": *"\([^"]*\)".*/\1/p' "$repo/ports/gweni/vcpkg.json" 2>/dev/null \
    | tr -d '\r' | head -n 1 || true)"
[ -n "$port_version" ] || fail "ports/gweni/vcpkg.json has no version-semver"
[ "$port_version" = "$version" ] \
    || fail "version-semver $port_version in ports/gweni/vcpkg.json is not $version, project(gweni VERSION $version) in CMakeLists.txt: they must be one version"
echo "ci_wheel.sh: the vcpkg port's version-semver is project()'s version"
if [ -n "${GITHUB_REF_NAME:-}" ] && [ "${GITHUB_REF_TYPE:-tag}" != branch ]; then
    [ "$GITHUB_REF_NAME" = "v$version" ] \
        || fail "the tag $GITHUB_REF_NAME is not v$version, project(gweni VERSION $version) in CMakeLists.txt: they must be one version"
    echo "ci_wheel.sh: the tag $GITHUB_REF_NAME is project()'s version"
else
    echo "ci_wheel.sh: no tag to check (GITHUB_REF_NAME ${GITHUB_REF_NAME:-unset}, GITHUB_REF_TYPE ${GITHUB_REF_TYPE:-unset})"
fi

# 2. the build, and its metadata's version
step "2. tools/build_wheel.sh"
"$repo/tools/build_wheel.sh" --out "$scratch/dist" || fail "tools/build_wheel.sh failed"
mapfile -t built < <(ls "$scratch/dist")
[ "${#built[@]}" -eq 1 ] || fail "tools/build_wheel.sh made ${built[*]:-nothing}, not one wheel"
linux="$scratch/dist/${built[0]}"
metadata="$("$python" - "$linux" <<'PY'
import sys, zipfile
with zipfile.ZipFile(sys.argv[1]) as z:
    meta = next(n for n in z.namelist() if n.endswith(".dist-info/METADATA"))
    print(next(l.split(":", 1)[1].strip() for l in z.read(meta).decode().splitlines() if l.startswith("Version:")))
PY
)" || fail "${built[0]} has no Version in its METADATA"
[ "$metadata" = "$version" ] \
    || fail "${built[0]}'s METADATA says Version: $metadata, not project(gweni VERSION $version): they must be one version"
echo "ci_wheel.sh: ${built[0]}'s METADATA says Version: $metadata"

# 3. the repair and its checks
step "3. tools/repair_wheel.py"
"$python" "$repo/tools/repair_wheel.py" --out "$scratch/repaired" "$linux" || fail "tools/repair_wheel.py failed"
mapfile -t repaired < <(ls "$scratch/repaired")
[ "${#repaired[@]}" -eq 1 ] || fail "the repair made ${repaired[*]:-nothing}, not one wheel"
wheel="$scratch/repaired/${repaired[0]}"

# 4. python_wheel's checks on the repaired wheel
step "4. python/tests/wheelChecks.sh"
GWENI_PYTHON="$python" GWENI_CHECK_LABEL=ci_wheel.sh \
    bash "$repo/python/tests/wheelChecks.sh" "$wheel" "$scratch/check" || fail "the checks of ${repaired[0]} failed"

# 5. the wheel, alone in --out
step "5. the wheel into $out"
cp "$wheel" "$out/"
echo "ci_wheel.sh: passed"
echo "$out/${repaired[0]}"
