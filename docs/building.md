# Building and testing gweni

Builds run inside the build container (`./run_docker.sh` from a host; see
`AGENTS.md` → "Building" for the full details) and are always configured through
`generate.sh`:

```bash
./generate.sh linux                       # build_linux_x64_debug/
./generate.sh linux release               # build_linux_x64_release/
./generate.sh linux asan                  # build_linux_x64_debug_asan/, AddressSanitizer
cmake --build build_linux_x64_debug -j
```

## Tests

The suite is CTest, under `tests/`, run from the repo root:

```bash
ctest --test-dir build_linux_x64_debug
ctest --test-dir build_linux_x64_debug_asan
```

The tests labelled `gpu` open a GL context through the examples' headless,
surfaceless-EGL path. On a shared host, hold the GPU lock around `ctest`;
`ctest -LE gpu` runs only the headless ones, which need no lock. The tests:

- `controltest_determinism`: two separate ControlTest processes must write
  byte-identical `--shot-all` PNGs and `--dump-bounds` listings.
  `controltest_determinism_scale150` is the same at `--scale 1.5`.
- `controltest_scale_bounds`: `--shot-all --dump-bounds` at `--scale 1` and 2.
  The listings (logical units) must be identical and every PNG exactly twice
  as wide and high (`tests/scaleBounds.cmake`).
- `controltest_teardown`: every demo builds, renders and tears down cleanly,
  and every frame lays out in one pass (`--require-one-pass`).
- `controltest_resize`: `ControlTest --scenario resize --require-one-pass`.
  Demo at 640x480, 1024x768 and 640x480 again comes back byte for byte, and
  matches a canvas built directly at 640x480.
- `controltest_resize_each`, `controltest_one_pass_each`: the same round trip
  for every other control in `ControlTest --list`, and the plain run of every
  control in one layout pass per frame (`tests/eachControl.cmake`).
- `controltest_interaction`: `ControlTest --scenario interaction`. Hover, press,
  disabled and a focus hand-off repaint the control, a hover touches only a few
  slots, an idle frame changes nothing, and collapsing a tree node moves the
  nodes below it.
- `controltest_text_size`: `ControlTest --scenario text-size`. A Label's font
  changed from 24 to 36 px at runtime, both drawn from the face's SDF atlas,
  uploads nothing to a font atlas and writes only that Label's glyph slots.
- `unittests`: doctest cases (`gweni_unittests`), e.g. the UTF-8 text-length
  guards in `tests/textLength.cpp`.
- `unit:<case name>`: the headless doctest cases (`gweni_unit_tests`, sources
  in `tests/unit/`), one CTest test per case, e.g. the id-bank checks across
  skin swaps in `tests/unit/skinSwap.cpp`. No GL context, no `gpu` label.

## ASAN tree

`./generate.sh linux asan` sets `GWENI_SANITIZE=address`. That instruments gweni,
the examples, the tests and the vendored `deps/freetype-gl` and `deps/imglib`,
but not the vcpkg dependencies. Its tests run with
`ASAN_OPTIONS=detect_leaks=0:abort_on_error=1`, so memory errors fail them and
leaks (Mesa/EGL's at exit) do not.

ASAN only sees accesses outside an allocation. An over-read that stays inside one
object goes unreported. ListBoxDemo's original UTF-8 over-read ran from a short
(SSO) string into the neighbouring `m_font` field of `controls::Text`, and neither
ASAN nor valgrind reported it. The deterministic trailing-bytes unit test and the
two-process determinism test cover that case.
