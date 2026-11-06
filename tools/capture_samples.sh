#!/usr/bin/env bash
#
#  Regenerates every UI sample PNG in docs/images/, which README.md embeds.
#
#  Run from the repo root, inside the build container:
#
#      ./tools/capture_samples.sh
#
#  The binaries come from the normal build (./generate.sh linux then
#  cmake --build build_linux_x64_debug -j). Captures need no display: they are
#  run under xvfb-run when it is installed, and otherwise take the examples'
#  own surfaceless-EGL path (Mesa llvmpipe in the container).
#
#  Editing SAMPLES below is how the sample set changes.

set -euo pipefail

# hero: the whole demo window, one page selected. ColorPickerDemo would be the
# obvious pick but draws nothing but its Result swatch (issue #82).
HERO_DEMO=CollapsibleListDemo
# OpenGLCore has no --size: its window is fixed at 1024x768
# (examples/openglcore/openglcore.cpp), so HERO_SIZE only labels the output.
HERO_SIZE=1024x768
HERO_OUT=demo.png

# The gallery: one ControlTest demo per line, captured at 640x480. README.md
# embeds each of these, so a change here needs a matching change there.
#
# Left out because they do not look right today:
#   ImagePanelDemo - an empty canvas (issue #69)
#   ComboBoxDemo - three bare round dots in the corner, no combo boxes (issue #69)
#   LabelMultilineDemo - the wrapped lines stack now, but the right column runs
#     past the canvas edge (issue #207) and its SplitterBars draw nothing
#     (idea #191)
#   ProgressBarDemo - the fill is not drawn, only the empty track
#   CheckBoxDemo - the first CheckBox sticks out above the canvas top
#   GroupBoxDemo - the caption sits on the frame's top-left corner
#   TextBoxDemo, NumericDemo - the text sits on the box's left border, outside
#     its padding (perhaps issue #206)
#   ColorPickerDemo - draws only its Result swatch and one slider (issue #82)
#   CollapsibleListDemo - already the hero page
SAMPLES=(
    ButtonDemo
    RadioButtonDemo
    TabControlDemo
    TreeControlDemo
    PropertiesDemo
    PageControlDemo
    ListBoxDemo
    SliderDemo
    TextDemo
)

CONTROL_SIZE=640x480

BUILD_DIR=build_linux_x64_debug
BIN_DIR="$BUILD_DIR/examples/openglcore"
OUT_DIR=docs/images

if [[ ! -f resources/DefaultSkin.json ]]; then
    echo "capture_samples.sh: run me from the repo root (resources/ not found here)" >&2
    exit 1
fi

for bin in OpenGLCore ControlTest; do
    if [[ ! -x "$BIN_DIR/$bin" ]]; then
        echo "capture_samples.sh: $BIN_DIR/$bin is missing; build it first:" >&2
        echo "    ./generate.sh linux" >&2
        echo "    cmake --build $BUILD_DIR -j" >&2
        exit 1
    fi
done

# Keep every capture on one screen size and colour depth so re-running on an
# unchanged tree produces identical images.
XVFB=()
if command -v xvfb-run >/dev/null 2>&1; then
    XVFB=(xvfb-run -s "-screen 0 1280x1024x24")
fi

mkdir -p "$OUT_DIR"

log=$(mktemp)
trap 'rm -f "$log"' EXIT

# The examples print a line per control update (base.h logging), so each run's
# stdout is kept in a file and only shown when the capture fails.
capture() {
    local out="$1"; shift

    rm -f "$out"

    if ! "${XVFB[@]}" "$@" >"$log" 2>&1; then
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
    "$BIN_DIR/OpenGLCore" --demo "$HERO_DEMO" --screenshot "$OUT_DIR/$HERO_OUT"

echo "controls at $CONTROL_SIZE:"
for demo in "${SAMPLES[@]}"; do
    capture "$OUT_DIR/$demo.png" \
        "$BIN_DIR/ControlTest" --headless --control "$demo" \
        --size "$CONTROL_SIZE" --shot "$OUT_DIR/$demo.png"
done

echo "capture_samples.sh: ${#SAMPLES[@]} control samples + hero written to $OUT_DIR"
