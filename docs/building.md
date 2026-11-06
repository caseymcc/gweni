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

Every test opens a GL context through the examples' headless, surfaceless-EGL
path and is labelled `gpu` (skip them with `-LE gpu`). On a shared host, hold
the GPU lock around `ctest`. The tests:

- `controltest_determinism`: two separate ControlTest processes must write
  byte-identical `--shot-all` PNGs and `--dump-bounds` listings.
- `controltest_teardown`: every demo builds, renders and tears down cleanly.
- `unittests`: doctest cases (`gweni_unittests`), e.g. the UTF-8 text-length
  guards in `tests/textLength.cpp`.

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
