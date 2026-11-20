#!/usr/bin/env bash
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Installs the published gweni wheel from a package index into fresh venvs,
#  one per CPython version, and runs the smoke set on each, as the PyPI
#  workflow's testpypi job (.github/workflows/pypi.yml) runs it after the
#  upload: every step of that check is here, so the YAML holds none of it.
#
#      ./tools/ci_wheel_install.sh --index-url https://test.pypi.org/simple/ \
#          --expect dist/gweni-0.1.0-cp312-abi3-manylinux_2_35_x86_64.whl 3.12 3.13 3.14
#      ./tools/ci_wheel_install.sh --find-links /tmp/wheel 3.13      # a local try
#
#  Options:
#    --index-url URL   the index to install from (with no other index, so
#                      nothing else can stand in for gweni)
#    --find-links DIR  a directory of wheels instead of an index
#    --expect WHEEL    fail unless the installed module is byte for byte the
#                      one in WHEEL (the workflow's artifact): an upload
#                      skipped as existing (skip-existing) leaves the index's
#                      older file of that version, which is then not tested
#                      as if it were this build's
#    --version V       the version to install (default project(gweni VERSION
#                      ...) in CMakeLists.txt)
#    --timeout S       how long to keep retrying the install while the index
#                      catches up with the upload (default 600)
#    --help            this text
#
#  For each version given (CPython 3.12 and newer, the wheel being cp312-abi3),
#  stopping at the first failure:
#    1. `uv python install` of that CPython, the GIL build (abi3 does not cover
#       the free-threaded one), into a scratch directory;
#    2. a fresh venv on it, and `uv pip install gweni==V` from the index alone,
#       retried every 20 s with uv's cache bypassed until it installs or
#       --timeout passes;
#    3. the installed distribution is version V and, with --expect, its module
#       is the artifact's;
#    4. python/tests/wheel_smoke.py run by the venv's interpreter from a
#       directory outside the checkout, its one GL step under
#       `mm run --lock render` when mm is on PATH (unlocked otherwise, as on a
#       GitHub runner, which needs Mesa's EGL and llvmpipe installed: the
#       workflow's apt step).
#  Each failure is one `ci_wheel_install.sh: FAIL: ...` line on stderr and exit 1.
#  It needs uv and the network (the index and uv's Python downloads).

set -euo pipefail

repo="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
index=()
expect=""
version=""
timeout=600
pythons=()

usage()
{
    sed -n '7,/^$/{s/^# \{0,2\}//;p}' "${BASH_SOURCE[0]}"
}

fail() { echo "ci_wheel_install.sh: FAIL: $*" >&2; exit 1; }

while [ $# -gt 0 ]; do
    case "$1" in
        --index-url) [ $# -ge 2 ] || fail "--index-url needs a URL"; index=(--index-url "$2"); shift 2 ;;
        --find-links) [ $# -ge 2 ] || fail "--find-links needs a directory"
            index=(--no-index --find-links "$(realpath "$2")"); shift 2 ;;
        --expect) [ $# -ge 2 ] || fail "--expect needs a wheel"; expect="$(realpath "$2")"; shift 2 ;;
        --version) [ $# -ge 2 ] || fail "--version needs a version"; version="$2"; shift 2 ;;
        --timeout) [ $# -ge 2 ] || fail "--timeout needs seconds"; timeout="$2"; shift 2 ;;
        --help|-h) usage; exit 0 ;;
        -*) echo "ci_wheel_install.sh: unknown argument: $1" >&2; usage >&2; exit 1 ;;
        *) pythons+=("$1"); shift ;;
    esac
done

[ "${#index[@]}" -gt 0 ] || fail "no --index-url or --find-links"
[ "${#pythons[@]}" -gt 0 ] || fail "no CPython version given (3.12 3.13 3.14, say)"
command -v uv >/dev/null || fail "no uv on PATH"
[ -z "$expect" ] || [ -f "$expect" ] || fail "no wheel $expect"
if [ -z "$version" ]; then
    version="$(sed -n 's/^project(gweni VERSION \([0-9.]*\).*/\1/p' "$repo/CMakeLists.txt" | tr -d '\r' | head -n 1)"
    [ -n "$version" ] || fail "CMakeLists.txt has no project(gweni VERSION ...)"
fi

scratch="$(mktemp -d /tmp/gweni-ci-wheel-install.XXXXXX)"
trap 'rm -rf "$scratch"' EXIT
export UV_PYTHON_INSTALL_DIR="$scratch/pythons"
mkdir -p "$scratch/elsewhere"

smoke()
{
    cd "$scratch/elsewhere"
    env -u PYTHONPATH PYTHONDONTWRITEBYTECODE=1 "$venv/bin/python" "$repo/python/tests/wheel_smoke.py" "$repo"
}

for py in "${pythons[@]}"; do
    echo
    echo "ci_wheel_install.sh: == CPython $py"
    # 1. the interpreter: the GIL build, uv's own
    uv python install --quiet "cpython-$py" || fail "uv python install $py failed"
    venv="$scratch/venv-$py"
    uv venv --quiet --managed-python --python "cpython-$py" "$venv" || fail "uv venv on CPython $py failed"
    "$venv/bin/python" -c 'import sys, sysconfig
assert not sysconfig.get_config_var("Py_GIL_DISABLED"), "free-threaded"
print("ci_wheel_install.sh: " + sys.version.split()[0] + " at " + sys.executable)' \
        || fail "the venv's CPython $py is not a GIL build"

    # 2. the install, retried while the index catches up
    deadline=$((SECONDS + timeout))
    until uv pip install --quiet --no-cache --python "$venv/bin/python" "${index[@]}" "gweni==$version" \
            2> "$scratch/install.log"; do
        if [ "$SECONDS" -ge "$deadline" ]; then
            cat "$scratch/install.log" >&2
            fail "gweni==$version did not install on CPython $py from ${index[*]} within ${timeout} s"
        fi
        echo "ci_wheel_install.sh: gweni==$version not installable yet ($(tail -n 1 "$scratch/install.log")); again in 20 s"
        sleep 20
    done

    # 3. the version, and the artifact's module
    "$venv/bin/python" - "$version" "$expect" <<'PY' || fail "the installed gweni on CPython $py is not the expected one"
import hashlib, importlib.metadata, importlib.util, pathlib, sys, zipfile
version, expect = sys.argv[1], sys.argv[2]
got = importlib.metadata.version("gweni")
if got != version:
    sys.exit(f"ci_wheel_install.sh: the installed gweni is {got}, not {version}")
module = pathlib.Path(importlib.util.find_spec("gweni._gweni").origin)
if expect:
    with zipfile.ZipFile(expect) as z:
        want = hashlib.sha256(z.read("gweni/_gweni.abi3.so")).hexdigest()
    have = hashlib.sha256(module.read_bytes()).hexdigest()
    if have != want:
        sys.exit(f"ci_wheel_install.sh: the index's gweni {version} is another build than {pathlib.Path(expect).name} "
                 f"(module sha256 {have[:12]}, the artifact's {want[:12]}): a version already uploaded is not replaced")
print(f"ci_wheel_install.sh: gweni {got} installed, {module}" + (", the artifact's module" if expect else ""))
PY

    # 4. the smoke set
    if command -v mm >/dev/null; then
        export -f smoke
        export scratch repo venv
        mm run --lock render -- bash -c smoke || fail "the smoke set failed on CPython $py"
    else
        echo "ci_wheel_install.sh: no mm on PATH: the GL step runs unlocked"
        smoke || fail "the smoke set failed on CPython $py"
    fi
    echo "ci_wheel_install.sh: CPython $py passed"
done
echo "ci_wheel_install.sh: passed on CPython ${pythons[*]}"
