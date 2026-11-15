Droid Sans Fallback Full is downloaded at configure time by
`cmake/LazyFont.cmake` into `<build-tree>/resources-lazy/`, outside the
`resources/` directory preloaded by the web pages. The font is unmodified;
its binary is not checked in. Configuration also copies its license and
range manifest beside it. Library-only builds do not fetch example resources.

Source: AOSP `platform/frameworks/base`, revision
`82454e029f05bf1795ac8704fb828ce9ed76767a`, file
`data/fonts/DroidSansFallbackFull.ttf` (4,033,576 bytes).
The download uses the AOSP GitHub mirror. SHA-256:
`2392015530438bafc48edfc4aee6d9de2387f627a6134d8ab3dfcc99d21c8240`.
`DroidSansFallbackFull-LICENSE.txt` is the same revision's `data/fonts/NOTICE`,
including the upstream attribution and Apache 2.0 license.

An existing font with that hash is reused without a network request. For
offline configuration, pass
`-DGWENI_CJK_FONT_FILE=/absolute/path/to/DroidSansFallbackFull.ttf` to
`generate.sh`; that file is verified against the same hash before copying.
A corrupt cached download fails configuration with the path and both hashes.

Regenerate the declared coverage with Python's standard library:

```sh
tools/gen_font_ranges.py build_linux_x64_debug/resources-lazy/DroidSansFallbackFull.ttf \
    --out resources-lazy/DroidSansFallbackFull-ranges.json
```

Add `--check` to check the manifest without rewriting it. The generator reads
the font's Unicode cmap, excludes glyph 0 and non-scalar codepoints, merges
contiguous coverage, and emits CSS `unicode-range` syntax. It needs neither
fonttools nor a system font. `tests/fontRanges.py` independently checks its
coverage against FreeType, checks the manifest's exact bytes, and requires
the default and mono chains in `resources/DefaultSkin.json` to declare that
same `unicodeRange`.

Native examples, the bench and unit tests mirror the build-tree directory
beside their executable. The default and mono glyph fallback chains declare
the face's ranges and file there. A first miss within those ranges reads its
bytes once per renderer; captures read all chain bytes before laying out text.
Ordinary `--resources` overrides leave this separate directory beside the
executable. The web's separate lazy package follows in task #605's web step.
