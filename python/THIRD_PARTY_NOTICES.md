# Third-party notices for the gweni Python package

gweni itself is MIT licensed (`LICENSE`). The extension module
`gweni/_gweni.abi3.so` links the following libraries statically; the full text
of each license ships in the installed package, in `gweni/licenses/<name>.txt`
(the vcpkg ports' copyright files and the vendored sources' `LICENSE`).

| Component | License | In `gweni/licenses/` |
|---|---|---|
| FreeType | FreeType License (FTL); portions of this software are copyright © The FreeType Project (www.freetype.org). All rights reserved. | `freetype.txt` |
| libpng | libpng license (PNG Reference Library License v2) | `libpng.txt` |
| zlib | zlib license | `zlib.txt` |
| bzip2 | bzip2 license (BSD-style) | `bzip2.txt` |
| Brotli | MIT | `brotli.txt` |
| libjpeg-turbo | IJG license and Modified (3-clause) BSD license; this software is based in part on the work of the Independent JPEG Group | `libjpeg-turbo.txt` |
| GLFW | zlib/libpng license | `glfw3.txt` |
| GLEW | Modified BSD, MIT and Khronos (MIT) licenses | `glew.txt` |
| md4c | MIT | `md4c.txt` |
| RapidJSON | MIT | `rapidjson.txt` |
| GLM | MIT (Happy Bunny License) | `glm.txt` |
| freetype-gl (vendored, modified) | BSD 2-clause | `freetype-gl.txt` |
| imglib (vendored) | MIT | `imglib.txt` |
| nanobind | BSD 3-clause | `nanobind.txt` |

The fonts in `gweni/resources/` keep their own licenses, beside each face and
in the wheel's license files:

| Face | License | File |
|---|---|---|
| Open Sans (regular, bold, italic, bold italic) | Apache License 2.0 | `resources/OpenSans-LICENSE.txt` |
| Liberation Sans | SIL Open Font License 1.1 | `resources/LiberationSans-LICENSE.txt` |
| DejaVu Sans Mono | Bitstream Vera Fonts license, DejaVu changes public domain | `resources/DejaVuSansMono-LICENSE.txt` |

The system's OpenGL, EGL and X11 libraries (glvnd's `libOpenGL`, `libGLX`,
`libEGL`, `libX11`) are not part of the package: they are loaded from the
machine it runs on.
