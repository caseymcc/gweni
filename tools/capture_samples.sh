#!/usr/bin/env bash
#
#  Regenerates, or checks, every UI sample PNG in docs/images/, which README.md
#  embeds.
#
#  Inside the build container, after a build:
#
#      ./tools/capture_samples.sh                 # rewrite docs/images/
#      ./tools/capture_samples.sh --check         # compare, write nothing there
#      ./tools/capture_samples.sh --bin-dir DIR   # take the exes from DIR
#
#  Options:
#    --check        capture into a scratch dir (mktemp -d) and compare each
#                   sample byte for byte with docs/images/. Prints one line per
#                   problem (differs / missing from docs/images / not in the
#                   sample set) and exits 1 if there is any, keeping the
#                   scratch dir and printing its path; exits 0 and removes it if
#                   everything matches. CTest runs this as docs_samples.
#    --bin-dir DIR  where OpenGLCore and ControlTest live; default
#                   build_linux_x64_debug/examples/openglcore (the tree of
#                   ./generate.sh linux then cmake --build build_linux_x64_debug -j).
#    --help         this text.
#
#  Both modes capture through the same capture() below, so the check compares
#  exactly what regenerating would write. Every capture is --headless (the
#  examples' surfaceless-EGL path, Mesa llvmpipe in the container), so no
#  display is needed and a display being present changes nothing. The skin,
#  fonts and textures are loaded from the repo's resources/, not the copy next
#  to the exes. Re-running on an unchanged tree produces byte-identical PNGs, but
#  only inside the pinned build container: another Mesa or compiler renders
#  other bytes, so a --check failure on a host means nothing.
#
#  On a shared host (an agent run) it renders on llvmpipe, Mesa's CPU
#  rasteriser, so it takes the render lock, not the gpu one:
#
#      mm run --lock render -- ./tools/capture_samples.sh --check
#
#  It can be run from any directory. Editing SAMPLES below is how the sample set
#  changes.

set -euo pipefail

# hero: the whole demo window with a showcase page selected, filling the
# centre pane (examples/demo/showcase.h).
HERO_DEMO=ShowcaseContainersDemo
# OpenGLCore has no --size: its window is fixed at 1024x768
# (examples/openglcore/openglcore.cpp), so HERO_SIZE only labels the output.
HERO_SIZE=1024x768
HERO_OUT=demo.png

# The gallery: one ControlTest demo per line. README.md embeds each of these,
# in this order, each a link to its live page, so a change here needs a
# matching change there (tools/check_site_links.py, run by tools/ci_site.sh,
# checks both). The showcase pages (examples/demo/showcase*.cpp) hold many
# controls each; TextDemo is the one page of a single control, the text
# sizes side by side.
#
# Every other page of one control is left out (ControlTest --control still
# shows each). Left off the showcase pages because they do not look right on
# one today:
#   ImagePanel (ImagePanelDemo) - its VerticalLayout stacks the two images
#     down the left edge, not side by side where they ask to be
#   a checkable MenuItem (MenuStripDemo) - its check is not drawn (#414)
#   Popup (PopupDemo) - the canvas shows one popup at a time, and the Windows &
#     menus page's capture opens its context menu
SAMPLES=(
    ShowcaseButtonsDemo
    ShowcaseInputDemo
    ShowcaseDisplayDemo
    ShowcaseWindowsDemo
    TextDemo
)

# Each sample's canvas size: a showcase page at the size of the hero's centre
# pane (DemoCenter, from ControlTest --control Demo --size 1024x768
# --dump-bounds), so it lays out as it does in the shell; a page of one
# control at its own size.
PANE_SIZE=865x545
declare -A SAMPLE_SIZE=(
    [TextDemo]=640x480
)

# Scripted input a sample is captured after, in place of the plain capture's
# frames: ControlTest steps, run in order before the --shot.
# ShowcaseWindowsDemo is shown with its context menu open, right-clicked in
# the "Context menu" box, and the mouse on its "Paste" item.
declare -A SAMPLE_STEPS=(
    [ShowcaseWindowsDemo]="--frames 3 --move 70,280 --frames 2 --click-right 70,280 --frames 3 --move 110,340 --frames 3"
)

# The samples are grayscale text whatever the caller's environment says: LCD
# subpixel text is opt-in, and checked by tests/textAA.cpp rather than here.
export GWENI_TEXT_AA=gray

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR=build_linux_x64_debug
BIN_DIR="$BUILD_DIR/examples/openglcore"
SAMPLE_DIR=docs/images

usage() {
    echo "Usage: tools/capture_samples.sh [--check] [--bin-dir DIR]"
    echo
    echo "  (no option)    capture every sample into $SAMPLE_DIR/"
    echo "  --check        capture into a scratch dir and compare with $SAMPLE_DIR/;"
    echo "                 exit 1 and keep the scratch dir if anything differs"
    echo "  --bin-dir DIR  where OpenGLCore and ControlTest live"
    echo "                 (default $BIN_DIR)"
    echo "  --help         show this text"
}

check=0
binDirGiven=0

while [[ $# -gt 0 ]]; do
    case "$1" in
        --check)
            check=1
            ;;
        --bin-dir)
            if [[ $# -lt 2 ]]; then
                echo "capture_samples.sh: --bin-dir needs a directory" >&2
                exit 2
            fi
            BIN_DIR="$2"
            binDirGiven=1
            shift
            ;;
        -h|--help)
            usage
            exit 0
            ;;
        *)
            echo "capture_samples.sh: unexpected argument $1" >&2
            usage >&2
            exit 2
            ;;
    esac
    shift
done

# A relative --bin-dir means the caller's cwd; the default means the repo root.
if [[ $binDirGiven -eq 0 ]]; then
    BIN_DIR="$ROOT/$BIN_DIR"
fi
BIN_DIR="$(cd "$BIN_DIR" 2>/dev/null && pwd || echo "$BIN_DIR")"

cd "$ROOT"

for bin in OpenGLCore ControlTest; do
    if [[ ! -x "$BIN_DIR/$bin" ]]; then
        echo "capture_samples.sh: $BIN_DIR/$bin is missing; build it first:" >&2
        if [[ $binDirGiven -eq 0 ]]; then
            echo "    ./generate.sh linux" >&2
            echo "    cmake --build $BUILD_DIR -j" >&2
        else
            echo "    cmake --build <the build tree holding $BIN_DIR> -j" >&2
        fi
        exit 1
    fi
done

if [[ $check -eq 1 ]]; then
    OUT_DIR=$(mktemp -d "${TMPDIR:-/tmp}/capture_samples.XXXXXX")
else
    OUT_DIR="$SAMPLE_DIR"
    mkdir -p "$OUT_DIR"
fi

log=$(mktemp)
keepOut=0
cleanup() {
    rm -f "$log"
    if [[ $check -eq 1 && $keepOut -eq 0 ]]; then
        rm -rf "$OUT_DIR"
    fi
}
trap cleanup EXIT

# The examples print a line per control update (base.h logging), so each run's
# stdout is kept in a file and only shown when the capture fails.
capture() {
    local out="$1"; shift

    rm -f "$out"

    if ! "$@" >"$log" 2>&1; then
        echo "capture_samples.sh: FAILED: $*" >&2
        cat "$log" >&2
        exit 1
    fi

    if [[ ! -s "$out" ]]; then
        echo "capture_samples.sh: $* wrote no image to $out" >&2
        cat "$log" >&2
        exit 1
    fi

    echo "  $out"
}

echo "hero: $HERO_DEMO at $HERO_SIZE"
capture "$OUT_DIR/$HERO_OUT" \
    "$BIN_DIR/OpenGLCore" --headless --resources resources \
    --demo "$HERO_DEMO" --screenshot "$OUT_DIR/$HERO_OUT"

echo "controls:"
for demo in "${SAMPLES[@]}"; do
    size=${SAMPLE_SIZE[$demo]:-$PANE_SIZE}
    # word-split on purpose: one ControlTest argument per word
    # shellcheck disable=SC2206
    steps=(${SAMPLE_STEPS[$demo]:-})
    capture "$OUT_DIR/$demo.png" \
        "$BIN_DIR/ControlTest" --headless --resources resources \
        --control "$demo" --size "$size" "${steps[@]}" --shot "$OUT_DIR/$demo.png"
done

if [[ $check -eq 0 ]]; then
    echo "capture_samples.sh: ${#SAMPLES[@]} control samples + hero written to $OUT_DIR"
    exit 0
fi

expected=("$HERO_OUT")
for demo in "${SAMPLES[@]}"; do
    expected+=("$demo.png")
done

problems=0

for name in "${expected[@]}"; do
    if [[ ! -f "$SAMPLE_DIR/$name" ]]; then
        echo "missing from $SAMPLE_DIR: $name"
        problems=$((problems+1))
    elif ! cmp -s "$OUT_DIR/$name" "$SAMPLE_DIR/$name"; then
        echo "differs: $name"
        problems=$((problems+1))
    fi
done

shopt -s nullglob
for path in "$SAMPLE_DIR"/*.png; do
    name=$(basename "$path")
    known=0
    for want in "${expected[@]}"; do
        [[ "$name" == "$want" ]] && known=1 && break
    done
    if [[ $known -eq 0 ]]; then
        echo "not in the sample set: $name"
        problems=$((problems+1))
    fi
done

if [[ $problems -gt 0 ]]; then
    keepOut=1
    echo "capture_samples.sh: $problems problem(s) in $SAMPLE_DIR; the new captures are in $OUT_DIR"
    echo "  (regenerate with ./tools/capture_samples.sh, look at the images, and commit them with the change)"
    exit 1
fi

echo "capture_samples.sh: all ${#expected[@]} samples match $SAMPLE_DIR"
