# Building and testing gweni

Builds run inside the build container (`./run_docker.sh` from a host; see
`AGENTS.md` → "Building" for the full details) and are always configured through
`generate.sh`:

```bash
./generate.sh linux                       # build_linux_x64_debug/
./generate.sh linux release               # build_linux_x64_release/
./generate.sh linux asan                  # build_linux_x64_debug_asan/, AddressSanitizer
./generate.sh linux profile               # build_linux_x64_profile/, for profiling
./generate.sh linux gles                  # build_linux_x64_debug_gles/, GLES 3.0 (see "GLES tree")
./generate.sh linux vulkan                # build_linux_x64_debug_vulkan/, no GL (see "Vulkan-only tree")
cmake --build build_linux_x64_debug -j
```

Every tree `generate.sh` configures has the Vulkan build on
(`GWENI_RENDER_VULKAN=ON`, with the vcpkg manifest feature `vulkan`: the Vulkan
headers, the volk loader, glslang's `glslangValidator`, which compiles the
shaders to SPIR-V at build time, and the Khronos validation layer, which only
the tests need). CMake's own default is off, so a plain CMake build needs none
of it. For a GL-only tree:

```bash
./generate.sh linux -DGWENI_RENDER_VULKAN=OFF
```

A `-DVCPKG_MANIFEST_FEATURES=...` given to `generate.sh` (e.g.
`boost-filesystem`) is merged with `vulkan`. The build image has the Vulkan
loader, Mesa's lavapipe CPU driver (Vulkan 1.3), which the Vulkan tests run on,
and Xvfb; an older container lacks them (`ls /usr/share/vulkan/icd.d` tells,
and `vulkan_smoke` fails saying so).

## Choosing the renderer

`ControlTest`, `OpenGLCore` and `gweni_bench` draw with `OpenGLCore` by default.
`--renderer vulkan` (or `GWENI_RENDERER=vulkan`; the flag wins) draws with
`renderer::Vulkan` on a Vulkan device instead, with no GL or EGL context at
all; a capture is always headless:

```bash
mm run --lock render -- build_linux_x64_debug/examples/openglcore/ControlTest \
    --renderer vulkan --headless --control ButtonDemo --shot /tmp/button.png
mm run --lock render -- build_linux_x64_debug/examples/openglcore/OpenGLCore \
    --renderer vulkan --headless --screenshot /tmp/demo.png
mm run --lock render -- build_linux_x64_release/bench/gweni_bench \
    --renderer vulkan --scene hover-sweep --n 1000
```

On Vulkan a capture (`--shot`, `--shot-all`, `--screenshot`, `--frames`,
`--stats`, the scenarios, `--check-teardown`) is the same headless run with or
without `--headless`. Without one, both examples open their window as on GL,
with a Vulkan swapchain (see "Vulkan in a window" below). `--renderer vulkan`
in a tree built without Vulkan prints one line and exits 1.
`GWENI_VULKAN_VALIDATION=1` turns the validation layer on (any warning or error
aborts the run); the Vulkan tests set it, with the layer's path
(`VK_LAYER_PATH=<tree>/vulkan/explicit_layer.d`) and lavapipe's ICD
(`VK_ICD_FILENAMES=/usr/share/vulkan/icd.d/lvp_icd.x86_64.json`).

### Idle windows

Both examples' windows, GL or Vulkan, are idle by default (`--loop idle`): they
draw a frame only when the canvas has work and otherwise sleep until an event,
a post from another thread, a timer or the next deadline (a caret blink, a
tooltip's delay). `--loop continuous` draws every frame, as a game loop does.
`--verify-idle` (ControlTest, or `GWENI_IDLE_VERIFY=1` in either window) draws
the frames the idle loop would skip as well and fails on one that writes
anything, which is how a change that forgot to mark its canvas shows
(`docs/architecture.md` -> "Threads, posts and the idle loop"). Captures and
`gweni_bench` step their own clock and draw every frame, as before.

### Vulkan in a window

`OpenGLCore --renderer vulkan` and `ControlTest --renderer vulkan` with no
capture open a GLFW window with no GL context and draw into it through a FIFO
swapchain. They resize, follow the content scale and stop drawing while
minimised as the GL window does (`docs/architecture.md` -> "The Vulkan
window"). The build container has no display, so there it is tried under
Xvfb (`window_vulkan` does that for you, see "Tests"):

```bash
xvfb-run -a -s "-screen 0 1280x1024x24" \
    build_linux_x64_debug/examples/openglcore/ControlTest --renderer vulkan --control ButtonDemo
```

To try it on a host with a real Vulkan driver (not part of any check; on a
HiDPI display the window is drawn at its content scale):

```bash
./run.sh opengl --renderer vulkan
./run.sh controltest --renderer vulkan --control ButtonDemo
GWENI_VULKAN_VALIDATION=1 VK_LAYER_PATH=$PWD/build_linux_x64_debug/vulkan/explicit_layer.d \
    ./run.sh opengl --renderer vulkan      # the same, every validation message aborting
```

The host needs, besides what the GL exes need (see "Running the examples"
in `AGENTS.md`), the Vulkan loader (`libvulkan.so.1`, e.g. Ubuntu's
`libvulkan1`) and a Vulkan 1.3 driver (its ICD manifest under
`/usr/share/vulkan/icd.d/`: Mesa's `mesa-vulkan-drivers` for AMD and Intel,
NVIDIA's own driver), on X11 or XWayland. `vulkaninfo --summary`
(`vulkan-tools`) lists what the loader finds. The exes take the first device
with Vulkan 1.3, dynamic rendering and a graphics queue; one that cannot present
to the window prints one line and exits 1.

## Vulkan-only tree

`renderer::Vulkan` does not need the GL renderer: it derives from the slot core
(`SlotRenderer`, freetype-gl, glm, FreeType, the fallback font, `DepthView`),
which is built with either renderer and holds nothing GL. `./generate.sh linux
vulkan` configures `build_linux_x64_debug_vulkan/` with
`-DGWENI_RENDER_OPENGL_CORE=OFF` and Vulkan on; it combines with `release`
(`build_linux_x64_release_vulkan/`) and `asan`
(`build_linux_x64_debug_vulkan_asan/`), and refuses `gles`, `web` and `profile`
with one error line:

```bash
./generate.sh linux vulkan
cmake --build build_linux_x64_debug_vulkan -j
ctest --test-dir build_linux_x64_debug_vulkan -LE gpu
mm run --lock render -- ctest --test-dir build_linux_x64_debug_vulkan -L gpu
```

Nothing in that tree finds or links GL, GLEW or EGL. It builds the library, the
headless unit tests, the header checks, `vulkan_smoke` and `vulkan_frame` (one
frame drawn with the textured skin on a validated device), and two checks that
need no device: `vulkan_frame_no_gl` (no GL library in `vulkan_frame`'s
`NEEDED`, no undefined `gl*`/`egl*`/`glew*` symbol) and `vulkan_cache_no_gl` (no
`OPENGL_*`, `GLEW_*` or EGL entry in the tree's `CMakeCache.txt`). ControlTest,
OpenGLCore, `gweni_unittests` and the bench are GL's (their capture code makes a
GL context), so they are not built there; `unittests_vulkan` in the normal tree
covers `renderer::Vulkan`. vcpkg still installs glew and glfw3 there, from
gweni's own manifest; an app's manifest is its own (below).

`gweni_gl_free_header_check`, part of `all` in every tree with either renderer,
compiles `slotRenderer.h`, `depthView.h` and `vulkan.h` with an include
directory of `#error` files in place of every GL-family header (`GL/`, `GLES*/`,
`EGL/`, `KHR/`, `OpenGL/`, `glad/`, `GLFW/`) searched first, so a GL include
reaching one of them fails the build naming the header.

## gweni in a Vulkan app

An app that draws with Vulkan alone takes gweni as a subproject:

```cmake
set(GWENI_RENDER_OPENGL_CORE OFF)
set(GWENI_RENDER_VULKAN ON)
add_subdirectory(external/gweni gweni)
target_link_libraries(app PRIVATE gweni::gweni)
```

(or, without a checkout, an installed gweni: "gweni as an installed package"
below; the target is the same `gweni::gweni`).

- A plain `set()` before the `add_subdirectory`, as above, sets an option, and
  so does a cache entry (`-D` or `set(... CACHE BOOL "")`): gweni's minimum
  CMake is 3.16, so policy CMP0077 is NEW and its `option()` takes the variable
  the parent set.
- `GWENI_BUILD_EXAMPLES` and `GWENI_BUILD_TESTS` default to OFF when gweni is
  not the top-level project, so this builds the library alone. With
  `GWENI_BUILD_TESTS` on, the tests need the validation layer, `doctest` and
  `glfw3`'s headers too.
- `GWENI_RENDER_OPENGL_CORE` is ON by default; turn it off, or GL, GLEW and
  GLFW are found and linked.
- `GWENI_SANITIZE`, `GWENI_FRAME_POINTERS` and `GWENI_DEBUG_LOGGING` are as in
  gweni's own trees, off by default.
- Configuring writes nothing under gweni's source tree. `<gweni/version.h>`
  (`GWENI_VERSION`, `GWENI_VERSION_STR`) is generated into the build tree and is
  on `gweni`'s include path; `cmake --install` installs it with the headers.
- `gweni` is a static library, `libgweni.a`, holding the vendored freetype-gl's
  and imglib's objects too (`libgwenid.a` in a tree with
  `CMAKE_DEBUG_POSTFIX=d`, as generate.sh's debug trees). Every textured-skin
  renderer is linked into an app that makes a `TexturedSkin`: its constructor
  calls `registerTexturedRenderers()`, the anchor that pulls the renderers'
  object, which registers them from static initializers, out of the archive.

The dependencies come from vcpkg, through the app's own manifest:
`rapidjson`, `glm`, `freetype`, `libpng`, `libjpeg-turbo`, `md4c` (the
CommonMark parser behind `MarkdownDocument`, always linked), `vulkan-headers`,
`volk` and `glslang` with its `tools` feature (`glslangValidator` compiles the
shaders to SPIR-V at build time; `cmake/Vulkan.cmake` takes it from
`${VCPKG_INSTALLED_DIR}/${VCPKG_TARGET_TRIPLET}/tools/glslang/`). It needs
neither `glew`, `glfw3` nor `vulkan-validationlayers` (a heavy port, for gweni's
tests alone).

```json
"dependencies": [
    "rapidjson", "glm", "freetype", "libpng", "libjpeg-turbo", "md4c",
    "vulkan-headers", "volk",
    { "name": "glslang", "features": [ "tools" ] }
]
```

volk loads `libvulkan.so.1` at run time; nothing links libvulkan. gweni links
vcpkg's static `volk::volk`, so an app that loads volk itself (`volkInitialize`,
`volkLoadInstance`, `volkLoadDevice`) and links the same port shares one set of
function pointers with it. `createVulkanDevice` makes a device for a test or a
tool; an app with its own fills a `renderer::VulkanDevice` (Vulkan 1.3,
`dynamicRendering` and `shaderDemoteToHelperInvocation` on, `dualSrcBlend`
where the device has it) and has `renderer::Vulkan` draw into a `VulkanTarget`
(`docs/architecture.md` -> "renderer::Vulkan (headless)").

The textured skin loads its files from the `ResourcePaths` the renderer is
given (`platform::CurrentPath("resources")`, `RelativeToExecutablePaths`, ...),
so ship gweni's `resources/` with the app: `DefaultSkin.json` (the atlas
description and the theme) and `DefaultSkin.png` (the atlas), `OpenSans.ttf`
(the default face) and `DejaVuSansMono.ttf` (the mono face and the glyph
fallback chain, loaded when used), with their `*-LICENSE.txt`. A missing skin
config draws every surface magenta and a missing face draws in the built-in
OpenSans, each with one line on stderr (`docs/architecture.md` -> "Missing
resources"). `gwen.png`, `test16.png` and `LiberationSans-Regular.ttf` are the
demos' alone.

The shipped `DefaultSkin.json` also names a lazy CJK face at the end of both
glyph fallback chains, `DroidSansFallbackFull.ttf` (Apache 2.0, 4 MB), which is
not in `resources/`: gweni's own trees download it at configure time
(`cmake/LazyFont.cmake`, only with `GWENI_BUILD_EXAMPLES` or `GWENI_BUILD_TESTS`)
into `<tree>/resources-lazy/`. An app that wants CJK drawn ships that directory's
`DroidSansFallbackFull.ttf` and `DroidSansFallbackFull-LICENSE.txt` as
`resources-lazy/` beside its executable (`ResourcePaths::Type::LazyFont` looks
there, whatever the resources directory is); a web page serves it beside the
page. Without it, CJK draws as the missing glyph, as before the face was added,
and the first CJK miss prints one line on stderr naming the path tried; text
without CJK never looks for it (`docs/architecture.md` -> "The lazy CJK face").

## gweni as an installed package

`cmake --install <tree> --prefix <prefix>` makes a package any CMake project
takes with `find_package`, with no gweni checkout:

```cmake
find_package(gweni CONFIG REQUIRED)       # CMAKE_PREFIX_PATH=<prefix>
add_executable(app main.cpp)
target_link_libraries(app PRIVATE gweni::gweni)
gweni_add_resources(app)
```

The prefix holds:

- `lib/libgweni.a`, the one library: gweni with the vendored freetype-gl's and
  imglib's objects;
- `include/gweni/**`, the public headers, those of the renderers the tree built
  only (no `vulkan.h` in a tree without Vulkan, no `glApi.h`/`openglCore.h`
  without the GL renderer), and under `include/gweni/third_party/` the vendored
  freetype-gl and imglib headers gweni's own headers include;
- `share/gweni/`: `gweniConfig.cmake` (with its version file: a 0.x minor
  version is an incompatible one), the exported targets, `gweniResources.cmake`
  and `mirrorDirectory.cmake`;
- `share/gweni/resources/`, the textured skin's files: `DefaultSkin.json` and
  `DefaultSkin.png`, the four OpenSans styles, DejaVu Sans Mono and their
  licenses. Not the demos' images and faces, and not the lazy CJK face
  (`resources-lazy/`, below).

No installed file names a path of the machine it was built on, so the prefix
can be moved. `find_package(gweni)` finds the dependencies the tree was built
with (RapidJSON, glm, md4c, libpng, libjpeg and FreeType always; GL and GLEW,
GLES 3 and EGL, or nothing on the web; Vulkan's headers and volk with Vulkan),
from the app's own vcpkg (or other) packages, so the app's manifest lists
them: `rapidjson`, `glm`, `freetype`, `libpng`, `libjpeg-turbo`, `md4c`, and
`glew` for desktop GL, `vulkan-headers` and `volk` for Vulkan. It sets
`gweni_RENDER_OPENGL_CORE`, `gweni_RENDER_VULKAN` and `gweni_GL_ES` to what the
package was built with, and `GWENI_RESOURCE_DIR` to its `resources/`.

`gweni::gweni` carries what an app compiles and links against it with: the
include directories, the compile definitions (`GWENI_USE_FILESYSTEM`,
`GWENI_HAS_VULKAN`, `GWENI_GL_ES`, ...) and, on the web, the link options every
page needs, `-sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2` (OpenGLCore's GLES 3.0
calls), `-fexceptions` (without it the app's link turns catching off, and a
throw gweni catches itself aborts the page) and `-Wl,--export-if-defined=malloc`
(gweni's JavaScript hands strings back through malloc). An app's own settings
stay its own, the stack and heap in particular: give a page at least
`-sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4MB` (gweni's layout and render walks
recurse once per level of the control tree; Emscripten's default stack is
64 KB). gweni does not export them because Emscripten takes the last `-s` of a
name, so an exported one would override an app asking for more.

`gweni_add_resources(<target>)` puts the skin's files where
`RelativeToExecutablePaths("resources")` finds them:

- natively it mirrors them to `resources/` in the target's output directory
  on every build (the rules of `cmake/mirrorDirectory.cmake`: a file the
  package no longer has is removed from the copy). The directory is where
  CMake puts the executable: the target's `RUNTIME_OUTPUT_DIRECTORY` (which
  `CMAKE_RUNTIME_OUTPUT_DIRECTORY` sets on a target made after it), else its
  binary directory, with the configuration's directory under a multi-config
  generator unless the property names it in a generator expression. It is read
  when the function is called, so set the property first;
- on the web it preloads them into the page's `.data` at `/resources`, the
  license texts left out, as link dependencies (an update relinks).

On the web, configure the app with the vcpkg toolchain chainloading emsdk's
`Emscripten.cmake` (`-DVCPKG_TARGET_TRIPLET=wasm32-emscripten
-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`).
Emscripten's toolchain finds packages only inside its sysroot and the paths it
is given as roots (vcpkg's installed tree among them), so a plain install prefix
needs `-Dgweni_DIR=<prefix>/share/gweni` (or the prefix on
`CMAKE_FIND_ROOT_PATH`); a gweni installed by vcpkg is found as any port is.

Limits and caveats:

- static only: `GWENI_EXPORT` is empty, there is no shared library;
- Linux x64 and wasm32 are what is built and tested;
- the vendored freetype-gl is gweni's patched copy, built into `libgweni.a`
  under freetype-gl's own C names and included by gweni's headers as
  `<freetype-gl/...>` from `include/gweni/third_party/`, so an app cannot link
  gweni and vcpkg's `freetype-gl` port together: it gets duplicate symbols.
  Both can be installed into one prefix (one `vcpkg_installed/<triplet>/`): no
  file of one is the other's. But then the app's include order decides which
  `<freetype-gl/...>` gweni's headers get. An installed gweni lists
  `include/gweni/third_party/` before `include/`, so with `gweni::gweni` first
  in the app's `target_link_libraries` its own copy wins; any target before
  it that brings the prefix's `include/` (`glm::glm`, any vcpkg port), or an
  include directory of the app's own holding the port's headers, puts
  `include/freetype-gl/` first, and gweni's headers then compile, silently,
  against the unpatched port (#868). gweni's own build puts its copy first
  whatever it links;
- the lazy CJK face is not in the package: CJK text draws as the missing glyph,
  after one stderr line, unless the app ships `resources-lazy/` beside its
  executable (above);
- an app using `input::GLFW3` or `glfwLoop.h` (header-only) links GLFW itself:
  vcpkg's `glfw3` natively, `--use-port=contrib.glfw3` on the web.

`package_consumer` (every native tree) and `package_consumer_web` (both web
trees) install the tree into a scratch prefix and build and run an app of this
kind against it (`tests/packageConsumer.cmake`, `tests/package/`).

## gweni from vcpkg

`ports/gweni/` is a vcpkg port of gweni (its `vcpkg.json`, `portfile.cmake`
and `usage`). It is published in a vcpkg registry of its own, the orphan branch
`vcpkg-registry` of caseymcc/gweni, which holds only `ports/gweni/` and
`versions/`. A project takes it by naming that registry in its
`vcpkg-configuration.json` and gweni in its `vcpkg.json`:

```json
// vcpkg-configuration.json
{"default-registry": {"kind": "builtin", "baseline": "<vcpkg sha>"},
 "registries": [{"kind": "git", "repository": "https://github.com/caseymcc/gweni",
   "reference": "vcpkg-registry", "baseline": "<sha>", "packages": ["gweni"]}]}
// vcpkg.json
"dependencies": ["gweni"]          // or {"name": "gweni", "features": ["vulkan"]} (native only)
```

`<sha>` is a commit of `vcpkg-registry`, the baseline a release prints (below).
vcpkg refuses a configuration with registries unless the default registry has
a baseline too, so `<vcpkg sha>` is a microsoft/vcpkg commit (the
`git rev-parse HEAD` of the app's `VCPKG_ROOT`; the dependencies' versions
come from it); a `"builtin-baseline"` in the app's `vcpkg.json` does the same
in place of the `default-registry` entry, and so does a `default-registry` of
`"kind": "git"` with that baseline for a `VCPKG_ROOT` that is no git clone.
Until the first version is published the branch does not exist, and the
registry cannot be used yet.

Without the registry, the port works as an overlay (`--overlay-ports=<dir>`
on `vcpkg install`, `VCPKG_OVERLAY_PORTS`, or the CMake toolchain's
`VCPKG_OVERLAY_PORTS`): `<dir>` the `ports/` of a checkout of
`vcpkg-registry` for a release (its portfile has the release's `SHA512`), in
manifest or classic mode. `<gweni>/ports` of a gweni checkout works only in
classic mode, `vcpkg install gweni --head --overlay-ports=<gweni>/ports`,
which builds GitHub's `main`: the copy on `main` keeps a placeholder
`SHA512`, which only `--head` skips, and `--head` is a classic-mode option (a
manifest or the CMake toolchain cannot pass it). Either way the source is
downloaded from GitHub, never the checkout's own files.

The app's CMake is that of any installed gweni ("gweni as an installed
package"):

```cmake
find_package(gweni CONFIG REQUIRED)
target_link_libraries(app PRIVATE gweni::gweni)
gweni_add_resources(app)           # the skin's files where RelativeToExecutablePaths("resources") finds them
```

The features:

- `opengl`, the default: `renderer::OpenGLCore` (`GWENI_RENDER_OPENGL_CORE`),
  desktop GL through glew natively, WebGL 2 on the web;
- `vulkan`, native only: `renderer::Vulkan` (`GWENI_RENDER_VULKAN`), with
  `vulkan-headers` and `volk`, its shaders compiled by the host's
  `glslang[tools]` (a host dependency, so cross builds use the build machine's
  `glslangValidator`);
- `"default-features": false` and neither: the controls, skins and layout with
  no renderer, for an app with a `renderer::Base` of its own.

The port always takes `rapidjson`, `glm`, `freetype` (vcpkg's, for its SDF
module), `libpng`, `libjpeg-turbo` and `md4c`, and builds gweni without its
examples and tests (nothing is downloaded at configure time). It takes no
`glfw3`: `input/glfw3.h` and `glfwLoop.h` are header-only, and an app using
them adds vcpkg's `glfw3` itself natively, or links with
`--use-port=contrib.glfw3` on the web.

On Linux, vcpkg's `glew` and gweni's GL renderer link the system's GL through
glvnd, so the machine needs its development packages: on Ubuntu or Debian
`libgl1-mesa-dev`, `libglu1-mesa-dev` and `libopengl-dev` (the build image's
`Dockerfile` installs them, with the X11 ones vcpkg's `glfw3` asks for).

On the web, configure the app as for any vcpkg Emscripten project:
`-DVCPKG_TARGET_TRIPLET=wasm32-emscripten
-DVCPKG_CHAINLOAD_TOOLCHAIN_FILE=$EMSDK/upstream/emscripten/cmake/Modules/Platform/Emscripten.cmake`.
vcpkg builds the port itself with its triplet's own toolchain (which
chainloads vcpkg's `scripts/toolchains/emscripten.cmake`, around the same
`Emscripten.cmake`); `EMSCRIPTEN` is set either way, and only the image's
emsdk 6.0.13 is tested. Then:

- the stack and heap are the app's: link with at least
  `-sALLOW_MEMORY_GROWTH=1 -sSTACK_SIZE=4MB` (gweni's walks recurse once per
  level of the control tree; Emscripten's default stack is 64 KB);
- `gweni::gweni` brings WebGL 2, `-fexceptions` and the malloc export
  ("gweni as an installed package"); `-fwasm-exceptions` is not supported, as
  for vcpkg's own ports, which are built with JavaScript exceptions and
  setjmp/longjmp;
- do not link Emscripten's own `-sUSE_FREETYPE=1` or `-sUSE_LIBPNG=1`: gweni
  uses vcpkg's FreeType and libpng, and two copies of one library collide.

Limits:

- x64-linux and wasm32-emscripten (`"supports": "(linux & x64) | emscripten"`;
  other targets may build with `--allow-unsupported`, untested);
- static only (`vcpkg_check_linkage(ONLY_STATIC_LIBRARY)`);
- the vendored freetype-gl is in `libgweni.a` under its own C names, so an app
  cannot link vcpkg's `freetype-gl` port beside gweni; both can be installed
  in one `vcpkg_installed`, but then gweni's headers find its own copy only
  with `gweni::gweni` first in the app's `target_link_libraries` ("gweni as
  an installed package", #868);
- the lazy CJK face is not in the port: CJK text draws as the missing glyph,
  after one stderr line, unless the app ships `resources-lazy/` beside its
  executable ("gweni in a Vulkan app").

### Publishing a version (the owner's steps)

A version is a tag `v<major>.<minor>.<patch>` whose `project(gweni VERSION ...)`
and `ports/gweni/vcpkg.json` `version-semver` are that version (bump both in the
commit to tag). Agents never push or tag; the owner:

1. tags `main` and pushes the tag: `git tag v0.1.0 && git push origin v0.1.0`
   (GitHub's archive of the tag is what the port downloads);
2. runs `tools/vcpkg_publish.sh v0.1.0` in the build container (from the
   host, `./run_docker.sh tools/vcpkg_publish.sh v0.1.0`: it needs vcpkg): it refuses
   unless the three versions agree, downloads GitHub's archive of the tag for
   its SHA512 (`--archive FILE` takes a local one), and in a scratch repository
   under `/tmp` writes the tag's `ports/gweni/` (its `REF` the tag, its `SHA512`
   the archive's) onto `vcpkg-registry` (the repository's branch, else
   `origin/vcpkg-registry` — `git fetch origin` first — else a new orphan
   branch), commits, runs `vcpkg x-add-version` and commits `versions/`;
3. runs the two commands it prints: the `git -C <repo> fetch` of the branch
   into the repository in the container, where the scratch repository is
   (from the host, prefixed with `./run_docker.sh`; its container persists,
   and with it `/tmp`), and then `git push origin vcpkg-registry` on the host,
   in the checkout (the same files, mounted), since only the host has the
   GitHub credentials;
4. gives consumers the `baseline:` it printed, in the
   `vcpkg-configuration.json` it prints (whose `default-registry` baseline is
   the vcpkg commit it ran with; any other works).

`vcpkg_port` (the native release tree) and `vcpkg_port_web` (the release web
tree) install the port through an overlay whose source is the checkout and
build and run `tests/package/` against it (`tests/vcpkgPort.cmake`);
`vcpkg_publish` (every native tree) runs the script on a scratch repository
and resolves gweni from what it wrote (`tests/vcpkgPublish.sh`).

## gweni as a HUD: HudExample

`HudExample` (`examples/hud/hud.cpp`) draws gweni over the app's own GL scene: a
spinning cube with its own shader and depth, a translucent panel with buttons and
a label, a tag over the cube and the stats overlay (F10 or `--stats-overlay`). It
is built with the examples and the GL renderer (not in the web tree), into
`build_linux_x64_<type>/examples/hud/` with its own `resources/` and `resources-lazy/` copies:

```bash
./build_linux_x64_debug/examples/hud/HudExample                 # the window: drag the bare canvas to orbit
mm run --lock render -- ./build_linux_x64_debug/examples/hud/HudExample \
    --headless --stats-overlay --screenshot /tmp/hud.png         # in the container
```

The frame is the app's scene, `SlotRenderer::clearDepth()`, then `renderCanvas()`
over a canvas with no background, and every input the UI did not take (each
`Canvas::input*` and `input::GLFW3::process*` returning false) goes to the
scene; `docs/architecture.md` → "gweni over an app's scene" has the rules.
`--renderer vulkan` prints one line and exits 1. Its test is `hud_example`
(label `gpu`): two captures byte for byte the same, and the scene, a Button, the
tag and the panel's blend at the probe points it prints.

## GLES tree

`./generate.sh linux gles` configures `build_linux_x64_debug_gles/` (with
`release`: `build_linux_x64_release_gles/`; with `asan`:
`build_linux_x64_debug_gles_asan/`; `profile` does not combine with it), the
GL renderer, the examples, the tests and the bench built against GLES 3.0
(`-DGWENI_GL_ES=ON`):

```bash
./generate.sh linux gles
cmake --build build_linux_x64_debug_gles -j
mm run --lock render -- ctest --test-dir build_linux_x64_debug_gles -L gpu
ctest --test-dir build_linux_x64_debug_gles -LE gpu
```

It compiles against `<GLES3/gl3.h>` and links `libGLESv2` (and `libEGL`), with
no glew, so a GL call newer than GLES 3.0 does not compile there, and the
examples and tests run on a GLES 3.0 context: the surfaceless EGL one is
`EGL_OPENGL_ES_API` 3.0, and a window asks GLFW for a GLES 3.0 context (there is
no display in the container, so only the EGL path is exercised there). It is
the browser's API without the browser: every test passes as in the desktop
trees, `docs_samples` byte for byte included, and `gweni_bench`'s counters are
the desktop tree's. Mesa's llvmpipe GLES context has
`GL_EXT_multi_draw_arrays` and `GL_EXT_blend_func_extended`, so the text draws
and LCD text work as on desktop GL. Without dual-source blending (WebGL 2, or
`GWENI_GL_NO_DUAL_SOURCE=1` in any tree) LCD text (`GWENI_TEXT_AA=rgb|bgr`)
falls back to grayscale with one warning.

## Web tree

`./generate.sh web` configures `build_web_wasm32_debug/` (`web release`:
`build_web_wasm32_release/`), a WebAssembly build made by Emscripten:

```bash
./generate.sh web
cmake --build build_web_wasm32_debug -j
cmake --build build_linux_x64_debug_gles -j   # the native reference (./generate.sh linux gles)
ctest --test-dir build_web_wasm32_debug -L web
```

It builds the `gweni` library with `OpenGLCore` on WebGL 2, the textured skin,
the headless unit tests (`gweni_unit_tests.js`/`.wasm`, run under node) and
three pages, each an `.html` with its `.js`, `.wasm` and a `.data` holding
`resources/` (all of it but the fonts' license texts, which nothing loads):

- `examples/openglcore/ControlTest.html` and `OpenGLCore.html`, the examples;
- `tests/gweni_unittests.html`, the GPU unit tests.

A page runs what the native exe runs. A capture (`--headless` with `--shot`,
`--shot-all`, `--screenshot`, ...) renders into an offscreen framebuffer and
reads it back in one blocking call. Without one, the page is the interactive
window: ControlTest's control or the OpenGLCore demo, drawn in a browser
animation frame when it has work (an idle page skips the frame), with the mouse (both buttons and the wheel) and the keyboard
(keys, modifiers and typed text) as on a desktop. The canvas fills the page above
a 96 px output panel and follows its size, and it draws at the browser's
`devicePixelRatio` (a HiDPI display, or the page zoom) times `--scale`, as a
desktop window draws at its monitor's content scale.

What the browser does not have degrades, each said once on stderr (and in the
output panel):

- the file dialogs say they are unavailable and return cancelled;
- LCD text needs dual-source blending, which WebGL 2 lacks, so text is grayscale.

The mouse cursor is the canvas's CSS cursor (a text beam, a pointer, a resize
arrow, ...), as the control under the mouse asks for it.

The clipboard is the browser's: Ctrl+C, Ctrl+X and Ctrl+V in a text box copy to it
and paste from it, and a Copy button (`platform::setClipboardText` from its click)
writes it. A desktop window's clipboard is the system's in the same way. A capture,
a headless page and the tests keep an in-process clipboard, so what they paste never
depends on the machine's.

The dependencies come from vcpkg's `wasm32-emscripten` triplet, with emsdk's own
`Emscripten.cmake` chainloaded. `glew` and `glfw3` are left out on that
platform; GLFW comes from Emscripten's `contrib.glfw3` port instead. Everything
is built with `-fexceptions`, so C++ exceptions are caught as natively.
The release web tree is built with `-Oz` instead of CMake's `-O3`, because it is
what the hosted site sends to every visitor (see "The hosted site").
`gweni_bench` is not built. `asan` and `profile` do not combine with `web`. The
vcpkg binary cache starts cold for this triplet: the first configure builds its
dependencies, about 30 s on a 32-core host.

### Running a page by hand

`tests/web/run.mjs` runs one page in the image's headless Chromium, as you would
run the executable natively:

```bash
node tests/web/run.mjs --out /tmp/shots build_web_wasm32_debug/examples/openglcore/ControlTest.html \
    -- --headless --control ButtonDemo --shot /out/ButtonDemo.png
```

Everything after `--` is the program's argv. The page writes to an in-memory
file system, and the runner copies whatever the run wrote under `/out` to the
`--out` directory. `--mount <dir>:<path>` copies a host directory into the page's
file system before `main` runs, e.g. a `resources/` to load with
`--resources <path>`. The runner prints the run's stdout and stderr and exits
with its exit status. It fails with exit 1 and one `run.mjs: FAIL:` line if:

- the page logs a console error or throws;
- the run aborts or times out (`--timeout <s>`, default 600);
- the run made no WebGL 2 context on ANGLE's SwiftShader, or one with fewer than
  24 depth bits.

In a browser of your own, serve the build directory over HTTP (a page cannot
load its `.wasm` and `.data` from `file://`). The build tree is in the repo, so
on a host:

```bash
python3 -m http.server -d build_web_wasm32_debug/examples/openglcore 8000
```

and open `http://localhost:8000/ControlTest.html?control=ButtonDemo` or
`http://localhost:8000/OpenGLCore.html`. The arguments are the URL query, in
order and URL-decoded: `key=value` is `--key value`, a bare `key` is `--key`,
and `argv=<text>` is one argument as it is. So
`ControlTest.html?control=ButtonDemo&scale=2` runs
`ControlTest --control ButtonDemo --scale 2`, the window at twice the display's
scale, and `ControlTest.html?headless&control=ButtonDemo&shot=/out/x.png` a
capture. An argument the program does not take ends the run as it does natively,
with its usage in the output panel under the canvas, where the page shows
everything the program prints.

The build image has what it needs, pinned in the `Dockerfile`, about 2.1 GB
more image in all:

- emsdk 6.0.13 at `/opt/emsdk` (`EMSDK`), with its bundled node 24, which runs
  the wasm tests and Playwright;
- Playwright 1.65.0 at `/opt/playwright` (`NODE_PATH`), with the Chromium 158
  headless shell it pins (`PLAYWRIGHT_BROWSERS_PATH`) and that shell's apt
  packages.

Without emsdk (an older container: `echo $EMSDK` is empty), `./generate.sh web`
prints one line saying so and exits 1. From a host, `./run_docker.sh -r`
rebuilds the image.

Every test in the web tree has the label `web` and needs no GPU lock:

- each `unit:` case, run by node;
- `web_probe`, which opens a blank page in the headless Chromium
  (`tests/web/probe.mjs`) and fails unless it gets a WebGL 2 context on
  ANGLE's SwiftShader with at least 24 depth bits;
- `web_probe_fails_without_webgl2` and `web_run_fails_without_webgl2`, which
  check that the probe and the runner fail when the browser has no WebGL 2;
- `web_unittests`, the GPU unit tests in the browser. It leaves out the LCD
  text cases (WebGL 2 has no dual-source blending, so LCD text is grayscale
  there) and the one that compares the text draws' `glDrawElements` loop with a
  multi-draw the web does not have. The list and its reasons are in
  `tests/CMakeLists.txt`. Doctest counts the cases left out as skipped;
- `web_controltest_shot` and `web_openglcore_screenshot`: a capture writes a
  PNG of the canvas's size that is not one flat colour (`tests/web/shot.mjs`);
- `web_controltest_missing_resources*` and `web_openglcore_missing_resources*`:
  `tests/missingResources.cmake`'s cases, with the incomplete `resources/` copy
  mounted into the page;
- `web_input`: the interactive windows of ControlTest and the OpenGLCore demo,
  driven with real mouse and keyboard input (`tests/web/interactive.mjs`, whose
  header lists the checks): a button press and release, typing "abcé", a wheel
  notch, the CSS cursor, the canvas's size at `devicePixelRatio` 2, after a
  resize and with `?scale=1.5` (its layout the same as a capture's at that size
  and scale), an unknown argument, a click in the demo list, the shell's Stats
  button closing and opening the stats overlay, and its Depth button opening
  the depth view, which Escape closes, the button following;
- `web_controltest_determinism`: two page loads of `ControlTest --shot-all
  --dump-bounds` write the same bytes;
- `web_bounds_native*`, `web_stats_native` and `web_pixels_native*`, which hold
  the browser to the native GLES tree: every `--dump-bounds` listing and every
  `--stats` counter identical, every PNG within 2 per channel with at most 0.1 %
  of its pixels more than 1 off (`tests/web/native.cmake` has the measured
  values). They run the native tree's exes themselves, so build it first:

  ```bash
  ./generate.sh linux gles && cmake --build build_linux_x64_debug_gles -j
  ```

  Without it they fail and say so. `./generate.sh web` points them at the GLES
  tree of the same build type (`GWENI_NATIVE_REFERENCE_DIR`). A PNG over the
  bound is written beside its native one and the difference, as
  `<test dir>/diff/<name>.diff.png`.

- `web_site_smoke`, in the release web tree only: the hosted site, staged and
  loaded page by page (see "The hosted site").

The browser tests take no lock because the runner keeps Chromium on its own
SwiftShader. The image pins the Vulkan loader to Mesa's lavapipe
(`VK_DRIVER_FILES`) for gweni's Vulkan tests, and with that pin ANGLE would
render on lavapipe instead. `probe.mjs` launches Chromium without the pin, and
both scripts fail if the renderer is not SwiftShader.

### The hosted site

`tools/build_site.sh` builds the release web tree (configuring it with
`./generate.sh web release` if it is not yet) and stages the live demo as one
static directory, `site/` in the repo (git-ignored), that GitHub Pages or any
static server serves as it is:

```bash
./tools/build_site.sh                    # stage site/
./tools/build_site.sh --out /tmp/site    # stage it elsewhere
./tools/build_site.sh --build-dir DIR    # take the pages from another web tree
python3 -m http.server -d site 8000      # then http://localhost:8000/
```

It holds three pages, their files and nothing else:

- `index.html`, the landing page: what gweni is, links to the demo, the gallery
  and the GitHub repo, and the hero sample (`docs/images/demo.png`) as a preview;
- `demo.html`, the OpenGLCore demo's window, with the stats overlay
  (`gweni::tools::StatsOverlay`, `docs/architecture.md` → "The stats overlay")
  open in its bottom-right corner: the page sets `Module.gweniDefaultArgs` to
  `['--stats-overlay']`, which `web/pre.js` puts before the URL's arguments, so
  `demo.html?stats-overlay=off` leaves it closed and F10 toggles it.
  `demo.html?depth-view` opens the depth view (`gweni::tools::DepthView`,
  `docs/architecture.md` → "The depth view") as well, once the first frame is
  drawn; the shell's **Depth** button, left of Stats in its status bar, opens it
  as F9 does, and since the view takes the mouse, Escape or F9 closes it again,
  the button following. A **Z spacing** slider in the header is the open
  view's spacing factor on a log scale, x0.1 to x20 of its reset spacing
  (`gweni_web_depth_view_spacing(factor)`, in both apps: above 0 sets, else
  reads), disabled while the view is closed and following +/-, Alt+wheel, R
  and the open (`Module.gweniDepthViewChanged`,
  `Module.gweniDepthViewSpacingChanged`). The overlay's times
  are the browser's `performance.now()`, which Chromium coarsens to about
  100 µs on a page without COOP/COEP headers, so a settled frame's phases read
  0.0 to 0.1 ms;
- `controltest.html`, ControlTest's window with a control picker. Its entries
  are `ControlTest --list`, written in when the site is staged. The URL query is
  ControlTest's argv, so `controltest.html?control=ButtonDemo&scale=2` is a link
  to one control at 2x, and choosing a control loads its page with every other
  parameter kept. Its **Depth view** checkbox opens and closes the depth view
  live (ControlTest's `gweni_web_depth_view(on)` export: 1 opens, 0 closes,
  anything else only reads); it starts as `?depth-view` says, follows F9 and
  Escape in the view (ControlTest calls the page's
  `Module.gweniDepthViewChanged`), and keeps `depth-view` in the URL as it is,
  so a control picked next opens with the view as it was. The **Z spacing**
  slider beside it is the one on `demo.html`.

Beside them is `resources-lazy/`, the lazy package: the CJK face (Droid Sans
Fallback Full, `docs/architecture.md` → "The lazy CJK face"), in no page's
`.data`. A window fetches it once it shows text that misses it, so a visitor who
sees no CJK downloads none of it; its license is in `licenses/` with the others.

They are written in `tools/site/` (the pages, `site.css`), dark like the Tokyo
Night skin, with no framework and nothing loaded from elsewhere. Every URL is
relative, so the site works under `https://<user>.github.io/gweni/` as from a
local server. It needs no COOP/COEP headers, since nothing in the build uses
threads or SharedArrayBuffer. A non-empty `--out` directory is cleared only if
it holds a site staged by the script (its `.gweni-site` marker).

The script then prints what each page sends, every file raw, gzip and brotli,
with nothing taken to be cached (`tools/site/sizes.mjs`). It exits 1 if the
gallery page's gzip total is over its `BUDGET_BYTES`, 8 MB. Measured with DejaVu
Sans Mono in `resources/` (#604), which adds its 341 kB (0.20 MB gzip) to each
page's `.data`, and OpenSans Bold, Italic and BoldItalic (#590), which add 651 kB
(0.35 MB gzip) more. The lazy package is a row of its own, in no page's total (#605,
which adds about 6 kB gzip to the pages themselves):

| page | raw | gzip | brotli |
|---|---|---|---|
| `controltest.html` (`ControlTest.wasm` 4572 kB, `.data` 1709 kB, `.js` 161 kB) | 6.46 MB | 2.58 MB | 1.83 MB |
| `demo.html` (`OpenGLCore.wasm` 3391 kB, `.data` 1709 kB, `.js` 160 kB) | 5.27 MB | 2.26 MB | 1.58 MB |
| `index.html` (with the 144 kB preview) | 0.15 MB | 0.14 MB | 0.14 MB |
| lazy package: `resources-lazy/DroidSansFallbackFull.ttf`, fetched on the first CJK shown | 4.03 MB | 2.15 MB | 1.70 MB |

The release web tree is `-Oz` for this: at CMake's `-O3`, `ControlTest.wasm` is
3041 kB (1080 kB gzip), against `-Oz`'s 2566 kB (975 kB gzip), and a
browser run (`--check-teardown`, Demo's resize scenario) is no slower within
3 %. `-Os` lies between them (2854 kB).

`web_site_smoke` (`tests/web/site.cmake`, release web tree) runs the script into
the build tree and then `tests/web/site.mjs`, which serves the staged site and
loads every page in the headless Chromium: the landing page with its image, the
demo (its argv starting with the default `--stats-overlay` and OpenGLCore saying
on stdout that the overlay is open) and `demo.html?stats-overlay=off` (without
that line), and the gallery at ButtonDemo, TextBoxDemo and ShowcaseButtonsDemo at
`?scale=2`, where it also picks SliderDemo and must get
`controltest.html?control=SliderDemo&scale=2`. Then `demo.html?depth-view` must
open with the overlay, the depth view and the Depth button on (read through
OpenGLCore's `gweni_web_depth_view`), and `controltest.html?control=ButtonDemo&depth-view`
with the view open and the checkbox checked; unchecking and checking it must
close and open the view, `depth-view` leaving the URL and coming back, and
picking SliderDemo must load `?control=SliderDemo&depth-view` with the view open
again, none of them writing to stderr. On both, the Z spacing slider must be
enabled at x1 with the view open and disabled while it is closed, set the app's
factor (read back through the export), move by exactly one step for a `+` and
for one Alt+wheel notch over the canvas with the wheel's default prevented
(`defaultPrevented`: the page neither scrolls nor zooms), and go back to x1 on R. It fails on a console error, an
uncaught exception, a failed request or an HTTP error (a file missing from the
site), a page without a WebGL 2 context on SwiftShader with 24 depth bits, a
window that draws no frames or whose canvas is one colour after 5 of them, a
picker not showing its control, a page without its built-from stamp (see
"Publishing the site"), a link into the site that is not there, and, through
the script, a gallery page over the budget. `node tests/web/site.mjs
--url http://localhost:8000/` checks a site some other server serves.

### Publishing the site

The site is live at <https://caseymcc.github.io/gweni/>. The GitHub Actions
workflow `.github/workflows/pages.yml` (the repo's only CI) publishes it on
every push to `main`, and on demand from *Actions → pages → Run workflow*. Its
`build` job:

1. builds the build image from the repo's `Dockerfile`, with the runner's
   UID/GID as `run_docker.sh` passes the host's;
2. runs `tools/ci_site.sh` in a container from it, with the checkout mounted at
   `/home/developer/gweni` and the vcpkg cache at `/vcpkg-cache`, as
   `run_docker.sh` mounts them;
3. uploads the staged `site/` as the Pages artifact.

Then its `deploy` job deploys that artifact to the `github-pages` environment.
If any step fails, a web test included, nothing is deployed and the site stays
as the last good run left it.

`tools/ci_site.sh` is every build step, so the YAML holds none and a run can be
repeated by hand in the container:

```bash
./tools/ci_site.sh                 # in the container; ./run_docker.sh ./tools/ci_site.sh from a host
./tools/ci_site.sh --out /tmp/site # stage the site elsewhere
```

It stops at the first step that fails:

1. the native GLES release tree, `build_linux_x64_release_gles/`, with Vulkan
   off (`-DGWENI_RENDER_VULKAN=OFF`): only its `ControlTest` and `OpenGLCore`,
   which the `web_*_native` tests compare with. Nothing here uses Vulkan, and
   its vcpkg ports (glslang, the validation layers) would double a cold
   dependency build. A tree of that name configured with Vulkan on is
   reconfigured without it;
2. the whole release web tree, `build_web_wasm32_release/`;
3. `ctest -L web -LE package` in it, `web_site_smoke` and the comparisons with
   native included (`vcpkg_port_web`, a port test, left out). On a runner, without `mm`, their native runs take no lock and say
   so;
4. `tools/build_site.sh`, the site and its size against the budget;
5. `tools/check_site_links.py README.md site/`, which fails unless:
   - every link in `README.md` to `https://caseymcc.github.io/gweni/` is a page
     of the staged site, and any `control=`/`demo=` in it is one of the
     gallery's picker entries;
   - the README links to the site itself;
   - every `docs/images/` image is a link to its live page: the hero to
     `demo.html`, a gallery image `<Name>.png` to
     `controltest.html?control=<Name>`;
   - the gallery images are `tools/capture_samples.sh`'s `SAMPLES`, in order.

   So a sample added to `SAMPLES` needs its linked image in the README, and a
   page renamed in `tools/site/` needs the README's links changed with it.

Two caches keep later runs short. Both live in GitHub's Actions cache (10 GB
per repository; the oldest entries are evicted beyond that):

- the image's layers, through buildx's `gha` cache, so a run rebuilds from the
  first `Dockerfile` layer that changed;
- vcpkg's binary cache and downloads, keyed on the image's vcpkg commit and on
  `vcpkg.json`. The `Dockerfile` clones vcpkg unpinned, so the commit is the one
  in the cached layer. A new key starts from the newest older entry. The cache
  is saved even when a later step fails, so a failed run still keeps the
  dependencies it built.

The builds run at most `CMAKE_BUILD_PARALLEL_LEVEL` compile jobs at a time,
`nproc` (4 on the runner) unless the environment sets it: `tools/ci_site.sh`
and `tools/build_site.sh` export it and give `cmake --build` no `-j`, which on
the Makefile trees means unlimited jobs. Unlimited jobs ran the runner's 16 GB
out of memory (exit 137 or 143, or the runner lost) in 7 of 40 runs (#579). If
4 jobs are still too many, pass `-e CMAKE_BUILD_PARALLEL_LEVEL=3` to the
`docker run` in `pages.yml`, or add a step before it that makes a swap file.

Every page says which commit it was built from, `built from <short sha>
(<commit date>)` linked to the commit: the landing page in its footer, the demo
and the gallery in their header, on its one row, so the canvas keeps its
height. There it takes only the room the row has spare, so the header wraps at
the widths it did without it: a narrower window drops the words "built from",
then the date, then the sha. `tools/build_site.sh` takes the sha from
`GITHUB_SHA`, which the workflow passes into the container, or else from the
checkout's `HEAD`, with `-dirty` when `git status --porcelain` shows changes
(ignored files, the build trees and `site/`, do not count, nor does an `--out`
inside the checkout), and says `unknown` without git. The date is
the commit's, not the build's, so two builds of one commit stage the same
bytes. `site.mjs` fails a page without the stamp.

A failed run says why without its log, which only a token can read. While
`tools/ci_site.sh` runs, the build step logs the runner's memory once a minute;
a step that runs only after a failure prints `free -m`, `df -h /` and the
kernel's out-of-memory lines (`dmesg`), and sums them up in one `::error::`
annotation, which the public check-runs API shows. A VM shut down (the runner
"lost communication") runs no later step, so there the per-minute memory lines
in the build step's log are what is left.

Every action is pinned to a full commit SHA, its version beside it. Each job
gets only the permissions it needs: `contents: read` to build, and `pages:
write` and `id-token: write` to deploy. Runs take turns (the `pages`
concurrency group), and a run in progress is never cancelled half-deployed.

Done once, by the repository's owner (no agent changes repository settings or
pushes):

- *Settings → Pages → Build and deployment → Source: **GitHub Actions***.
- Expect the first run to be slow: a cold image build (emsdk, Playwright and
  Chromium) and cold vcpkg builds for both triplets, `x64-linux` and
  `wasm32-emscripten`, on a 4-core runner. Later runs reuse both caches.

`node tests/web/site.mjs --url https://caseymcc.github.io/gweni/`, in the
container, runs the `web_site_smoke` checks against the deployed site.

## Tests

The suite is CTest, under `tests/`, run from the repo root:

```bash
ctest --test-dir build_linux_x64_debug
ctest --test-dir build_linux_x64_debug_asan
```

The tests labelled `gpu` open a GL context through the examples' headless,
surfaceless-EGL path, or a Vulkan device: Mesa's CPU rasterisers, llvmpipe and
lavapipe, not a hardware GPU. On a shared host run them under the `render` lock,
and the headless rest on their own with no lock; never hold a lock around a
whole `ctest`, and never around a build (`./generate.sh`, `cmake --build`):

```bash
mm run --lock render -- ctest --test-dir build_linux_x64_debug -L gpu
ctest --test-dir build_linux_x64_debug -LE gpu
```

The Vulkan tests are also labelled `vulkan`, so `ctest -LE vulkan` runs the
GL-only suite. Each whole-app check below (the `controltest_*` and
`openglcore_missing_resources*` tests, but `controltest_verify_layout_*`,
`controltest_demo_labels*` and `controltest_any_cwd`, which check layout and
paths, not drawing) and `bench_smoke` is registered twice: on GL, and as
`<name>_vulkan` (labels `gpu`, `vulkan`), the same command with
`GWENI_RENDERER=vulkan`, validated. `docs_samples` stays GL only. The tests:

- `controltest_determinism`: two separate ControlTest processes must write
  byte-identical `--shot-all` PNGs and `--dump-bounds` listings.
  `controltest_determinism_scale150` is the same at `--scale 1.5`.
- `controltest_scale_bounds`: `--shot-all --dump-bounds` at `--scale 1` and 2.
  The listings (logical units) must be identical and every PNG exactly twice
  as wide and high (`tests/scaleBounds.cmake`).
- `controltest_scale_roundtrip`: `ControlTest --scenario scale --require-one-pass`
  for every control in `--list`, Demo included. `Canvas::setScale` 1 -> 2 -> 1
  comes back byte for byte (listing, renderer mirror, pixels), and the 2x frame
  matches a canvas built directly at scale 2 (`tests/eachControl.cmake`).
- `controltest_teardown`: every demo builds, renders and tears down cleanly,
  and every frame lays out in one pass (`--require-one-pass`).
- `controltest_resize`: `ControlTest --scenario resize --require-one-pass`.
  Demo at 640x480, 1024x768 and 640x480 again comes back byte for byte, and
  matches a canvas built directly at 640x480.
- `controltest_resize_each`, `controltest_one_pass_each`: the same round trip
  for every other control in `ControlTest --list`, and the plain run of every
  control in one layout pass per frame (`tests/eachControl.cmake`).
- `docs_samples`: `tools/capture_samples.sh --check`. The README samples in
  `docs/images/` must match a fresh capture byte for byte, so a change that
  alters one regenerates them (`./tools/capture_samples.sh`) in the same
  commit. Only meaningful inside the build container.
- `controltest_interaction`: `ControlTest --scenario interaction`. Hover, press,
  disabled and a focus hand-off repaint the control, a hover touches only a few
  slots, an idle frame changes nothing, and collapsing a tree node moves the
  nodes below it. In CheckBoxDemo a box lights when it or its label is hovered,
  a click checks it and a second unchecks it, and the disabled box looks
  muted and ignores hover and clicks.
- `controltest_text_size`: `ControlTest --scenario text-size`. A Label's font
  changed from 24 to 36 px at runtime, both drawn from the face's SDF atlas,
  uploads nothing to a font atlas and writes only that Label's glyph slots.
- `controltest_script_first_frame`: a ControlTest script that starts with
  input (`--move 30,5 --shot`) is settled first and renders a frame before the
  shot, so its PNG matches `--frames 3 --move 30,5 --frames 1 --shot`, and
  `--frames 0 --shot` exits 1 and writes nothing (`tests/scriptFirstFrame.cmake`).
- `unittests`: doctest cases (`gweni_unittests`), e.g. the UTF-8 text-length
  guards in `tests/textLength.cpp`.
- `unittests_vulkan` (labels `gpu`, `vulkan`): the same `gweni_unittests` on
  the Vulkan backend (`GWENI_RENDERER=vulkan`: a validated Vulkan device and
  `renderer::Vulkan`, no GL context), so its cases check the same analytic
  models on both: every case but the `gl_only` suite (what only GL has: the
  GLSL dialects, `glMultiDrawElements`, the GL objects a renderer deletes).
- `vulkan_smoke` (labels `gpu`, `vulkan`): the Vulkan plumbing. It loads the
  loader through volk, creates an instance with `VK_LAYER_KHRONOS_validation`
  (any validation warning or error aborts it), finds a Vulkan 1.3 device with
  `dynamicRendering` and `dualSrcBlend`, and builds a pipeline from SPIR-V
  compiled at build time. Like every Vulkan test it runs with only the vcpkg
  validation layer on the loader's layer path and the device pinned to
  lavapipe (`gweni_vulkan_test_environment` in `cmake/Vulkan.cmake`).
- `vulkan_frame` (labels `gpu`, `vulkan`): one frame as a Vulkan-only app draws
  it, with `gweni` alone linked: a validated device, a `VulkanTarget`, the
  textured skin from `resources/` and a few controls; it fails if the frame
  read back is one flat colour. In the Vulkan-only tree `vulkan_frame_no_gl`
  and `vulkan_cache_no_gl` (no label, no device) check its ELF and the tree's
  cache for GL (see "Vulkan-only tree").
- `window_vulkan` (labels `gpu`, `vulkan`): the Vulkan window, under an Xvfb
  it starts itself and a minimal test window manager (`gweni_test_wm`):
  `ControlTest --renderer vulkan --window-test` presents ButtonDemo at
  640x480, resizes the window to 800x600, 512x384 and back, iconifies and
  restores it, and reads back each image it presented; every one must be byte
  for byte a `--headless` capture at its size, every resize must recreate the
  swapchain and nothing may be drawn or presented while iconified
  (`tests/windowVulkan.cmake`). It fails, not skips, without Xvfb.
- `window_idle` (label `gpu`) and `window_idle_vulkan` (labels `gpu`,
  `vulkan`): `ControlTest --idle-test` on ButtonDemo under an Xvfb it starts
  itself. Once settled the window idles 2 s, which must draw no frame, swap or
  present nothing and take under 0.1 s of CPU; then a post from another thread
  must wake it and draw exactly one frame (`tests/windowIdle.cmake`). It fails,
  not skips, without Xvfb. `window_idle_demo` (label `gpu`) is the same check
  on the whole demo shell (`--control Demo`), where every hidden page must
  leave the loop idle too.
- `controltest_verify_idle_<scenario>`: the interaction, tooltip, context-menu,
  depth-view and stats-overlay scenarios with `--verify-idle`. Every frame an
  idle loop would have skipped is drawn anyway and must write no slot, upload
  nothing and change no pixel.
- `backend_counters` (labels `gpu`, `vulkan`): every `gweni_bench` scene, every
  `ControlTest --stats` control and `OpenGLCore --stats` run on `--renderer gl`
  and `--renderer vulkan`; every counter of every frame must be the same (the
  times and the bench's allocation counts left out), and a settled Vulkan frame
  must write and upload nothing (`tests/backends.mjs`, run on emsdk's node).
- `backend_pixels` and `backend_pixels_scale200` (labels `gpu`, `vulkan`):
  `ControlTest --shot-all` and the hero on both backends, at scale 1 and 2; each
  Vulkan PNG within 1 level per channel of GL's, with at most 0.01 % of its pixels
  differing. A failure keeps its scratch dir under `/tmp` with a
  `diff/<name>.diff.png` (GL, Vulkan, the difference) per PNG over the bound
  (`docs/architecture.md` -> "Vulkan's counters and pixels against GL's").
- `unit:<case name>`: the headless doctest cases (`gweni_unit_tests`, sources
  in `tests/unit/`), one CTest test per case, e.g. the id-bank checks across
  skin swaps in `tests/unit/skinSwap.cpp`. No GL context, no `gpu` label.

Three checks of the public headers run in the build itself, not in `ctest`.
Every header under `include/gweni/` is compiled as the only include of its own
translation unit (the target `gweni_header_check`, part of `all`; a backend's
headers only in a tree that builds that backend), so a header that does not
include what it uses fails `cmake --build`, naming its
`tests/header_check/gweni/<rel>.cpp`. And `gweni/controls.h` includes every
header under `include/gweni/controls/`: a new control header must be added to
it, or configuring fails with `controls.h does not include gweni/controls/<x>.h`.
And `gweni_gl_free_header_check` compiles the headers a Vulkan app includes with
every GL header poisoned (see "Vulkan-only tree").

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

## Profile tree

`./generate.sh linux profile` configures `build_linux_x64_profile/`: an
optimised build with debug info (`RelWithDebInfo`) and frame pointers kept
(`GWENI_FRAME_POINTERS=ON`, `-fno-omit-frame-pointer` on everything, `deps/`
included), for profiling and for comparing revisions. It does not combine with
`release` or `asan`. Its tests pass like every other tree's. The build image has
the profiling tools, `valgrind` (callgrind), `graphviz` (`dot`) and `gprof2dot`;
an older container lacks them (`which valgrind` tells).

## Benchmarks

`gweni_bench` (`bench/`) runs a fixed scene on the full path, gweni plus
`OpenGLCore` on surfaceless EGL (or `renderer::Vulkan` with `--renderer vulkan`),
and prints per measured frame the wall time of
each phase (`input`, `update`, `render`, `flush`, `gpu`) and the work counters:
controls rendered and changed (`visited`, the queued controls the render
phase rendered, 0 on a settled frame, and `render_carried`, the entries it
left queued for a later frame), slots written, uploads and buffer regrows
(`buffer_regrows`), draw calls, layout passes
and the controls they entered (`layout_visited`), the children their loops
went over, entered or not (`layout_children_scanned`), text measures
(`measure_calls`, and `measure_calls_<phase>` for the input, update and
render phases), z renumbering, live ids and C++ allocations. The same
arguments give the same counters every run. Times are only meaningful from a release tree, and `gpu` is
Mesa llvmpipe (or lavapipe) on the CPU inside the container; the summary's
`renderer=` and `device=` say which:

```bash
build_linux_x64_release/bench/gweni_bench --list
build_linux_x64_release/bench/gweni_bench --scene hover-sweep --n 10000
```

The scenes cover a cold first frame, idle frames, hover, typing, scrolling, a
dragged window, a tree node toggled, controls added, removed and brought to the
front, a canvas resized and rescaled, and animations. `--list` marks each
`scaling=local` (its per-frame counters must not grow with N) or `scaling=linear`
(it touches every control by design). `--help` lists the options and every
output key. `bench_smoke` (label `gpu`) only checks that every scene runs, and
`bench_smoke_vulkan` that it runs on Vulkan.
`bench_scaling` (label `gpu`) is the one test on the bench's numbers: it fails
if a `scaling=local` scene's per-frame work counters (never its times) are
larger at N=10000 than at N=1000. See `AGENTS.md` → "Benchmarks and profiling".

## Profiling

`tools/profile.sh` runs one scene from the profile tree under callgrind,
collecting only the measured frames (and the `build` scene's construction), not
setup, EGL init, resource loading, warm-up or teardown:

```bash
tools/profile.sh hover-sweep --n 1000     # into /tmp/gweni-profile-hover-sweep-1000/
tools/profile.sh build --n 10000 --out /tmp/build10k
```

It writes `callgrind.out` (open it with kcachegrind or qcachegrind on a host),
`phases.txt` (the instructions of each phase, `phase=<name> ir=<n>`, and their
`total`), `annotate.txt` (the functions by inclusive cost and every line of
gweni's sources with its cost) and `callgraph.svg` (the call graph, hot paths
red). It runs a copy of the bench from a fixed directory in a fixed
environment, so the same binary gives the same counts every run, to the
instruction; two revisions lay the heap out differently, which alone can move
`update` by up to 0.4 % and the other phases by under 0.1 %
(`--layout-pad K` samples that). Inside the container nearly all of a
small scene's instructions are Mesa llvmpipe rasterising, in the `flush` phase
under callgrind; Mesa's functions and its JIT code show as anonymous
`???:0x...` functions. See
`AGENTS.md` → "Profiling a scene".

## Comparing two revisions

`tools/bench.sh compare` builds two revisions in their profile trees, runs every
scene on both and prints a markdown report (also written to
`/tmp/gweni-bench-<base>-<rev>.md`) of how the work counters, the instructions
of each phase under callgrind and, with `--time`, the wall time moved. It flags
regressions and never fails because of one: it is a report to accept or act on,
not a gate, and the baseline is always the base revision.

```bash
tools/bench.sh compare origin/main            # origin/main vs the working tree
tools/bench.sh compare origin/main --keep     # keep origin/main's build for next time
tools/bench.sh compare origin/main --time     # wall time too, alternating runs
```

Before finishing a change to `source/` or `include/`, run it against
`origin/main` without `--time` and look at the flags. The flags: any counter
that changed; a phase more than 1 % more instructions; a counter of a local
scene that now grows from N=1000 to N=10000 (a new O(N)); with `--time`, a phase
more than 5 % and more than twice the spread slower. A base commit is exported
with `git archive` to `/tmp` and built there, so the first compare against it
configures a new tree (slow without the vcpkg binary cache); `--keep` keeps it.
A default compare takes about 40 minutes, nearly all of it callgrind;
`--no-ir` about 2, and `--scenes` limits it to the scenes a change touches.
Each timed run holds the `render` and `quiet` locks (no other locked job runs
alongside it), but builds take no lock, so on a shared host a wall-time flag can
still be noise (a compare of a commit with itself flagged 2 of 78 phases): run
it again before acting on it. Callgrind runs take no lock. See `AGENTS.md` → "Comparing two
revisions".
