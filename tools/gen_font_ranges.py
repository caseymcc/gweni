#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Generate CSS unicode-range syntax from a TrueType/OpenType Unicode cmap,
#  without fonttools. Supports formats 4 (BMP) and 12 (full Unicode), used by
#  Droid Sans Fallback Full. Glyph 0, surrogates and variation-selector
#  subtables (format 14) are not coverage. Union the Unicode subtables.
#
#      tools/gen_font_ranges.py <tree>/resources-lazy/DroidSansFallbackFull.ttf \
#          --out resources-lazy/DroidSansFallbackFull-ranges.json
#
#  --check compares bytes with --out instead of writing it. The manifest
#  includes the face's SHA-256 so its declared ranges cannot silently drift.
#

import argparse
import hashlib
import json
from pathlib import Path
import struct
import sys


def u16(data, offset):
    return struct.unpack_from(">H", data, offset)[0]


def u32(data, offset):
    return struct.unpack_from(">I", data, offset)[0]


def format4(data):
    segments = u16(data, 6) // 2
    if not segments or 16 + 8 * segments > len(data):
        raise ValueError("invalid format 4 segment table")
    ends = 14
    starts = ends + 2 * segments + 2
    deltas = starts + 2 * segments
    offsets = deltas + 2 * segments
    codepoints = set()
    for i in range(segments):
        first, last = u16(data, starts + 2 * i), u16(data, ends + 2 * i)
        delta = u16(data, deltas + 2 * i)
        relative = u16(data, offsets + 2 * i)
        for cp in range(first, last + 1):
            if relative:
                # idRangeOffset is relative to this segment's offset word,
                # not to the start of the glyphIdArray.
                glyph = u16(data, offsets + 2 * i + relative + 2 * (cp - first))
                if glyph:
                    glyph = (glyph + delta) & 0xFFFF
            else:
                glyph = (cp + delta) & 0xFFFF
            if glyph:
                codepoints.add(cp)
    return codepoints


def format12(data):
    groups = u32(data, 12)
    if 16 + 12 * groups > len(data):
        raise ValueError("invalid format 12 group table")
    codepoints = set()
    for i in range(groups):
        first, last, glyph = struct.unpack_from(">III", data, 16 + 12 * i)
        if first > last or last > 0x10FFFF:
            raise ValueError("invalid format 12 codepoint range")
        # The first codepoint maps to glyph 0 only when startGlyphID is 0.
        codepoints.update(range(first + (glyph == 0), last + 1))
    return codepoints


def cmap_codepoints(data):
    if data[:4] not in (b"\x00\x01\x00\x00", b"OTTO"):
        raise ValueError("expected a TrueType/OpenType font (not a collection)")
    cmap = None
    for i in range(u16(data, 4)):
        tag, _, offset, length = struct.unpack_from(">4sIII", data, 12 + 16 * i)
        if tag == b"cmap":
            if offset + length > len(data):
                raise ValueError("truncated cmap table")
            cmap = data[offset:offset + length]
            break
    if cmap is None:
        raise ValueError("font has no cmap")

    codepoints = set()
    visited = set()
    for i in range(u16(cmap, 2)):
        platform, encoding, offset = struct.unpack_from(">HHI", cmap, 4 + 8 * i)
        if not (platform == 0 or (platform == 3 and encoding in (1, 10))):
            continue
        if offset in visited:
            continue
        visited.add(offset)
        fmt = u16(cmap, offset)
        if fmt == 14:  # variation sequences do not map standalone codepoints
            continue
        if fmt not in (4, 12):
            raise ValueError("unsupported Unicode cmap format %d" % fmt)
        length = u16(cmap, offset + 2) if fmt == 4 else u32(cmap, offset + 4)
        if offset + length > len(cmap):
            raise ValueError("truncated cmap subtable")
        subtable = cmap[offset:offset + length]
        codepoints.update(format4(subtable) if fmt == 4 else format12(subtable))
    return {cp for cp in codepoints if cp <= 0x10FFFF and not 0xD800 <= cp <= 0xDFFF}


def unicode_ranges(codepoints):
    ranges = []
    for cp in sorted(codepoints):
        if ranges and cp == ranges[-1][1] + 1:
            ranges[-1][1] = cp
        else:
            ranges.append([cp, cp])
    return ", ".join("U+%04X" % first if first == last else "U+%04X-%04X" % (first, last)
                     for first, last in ranges)


def manifest(font, data):
    return json.dumps({
        "font": Path(font).stem,
        "sha256": hashlib.sha256(data).hexdigest(),
        "unicodeRange": unicode_ranges(cmap_codepoints(data)),
    }, indent=4) + "\n"


def main():
    parser = argparse.ArgumentParser(description="Generate CSS unicode-range syntax from a font's cmap")
    parser.add_argument("font", type=Path)
    parser.add_argument("--out", type=Path, required=True, help="JSON manifest to write or check")
    parser.add_argument("--check", action="store_true", help="fail if --out differs; write nothing")
    args = parser.parse_args()
    try:
        output = manifest(args.font, args.font.read_bytes())
        if args.check:
            if args.out.read_bytes() != output.encode("utf-8"):
                sys.exit("gen_font_ranges.py: stale ranges: %s" % args.out)
        else:
            args.out.write_bytes(output.encode("utf-8"))
    except (OSError, ValueError, struct.error) as error:
        sys.exit("gen_font_ranges.py: %s" % error)


if __name__ == "__main__":
    main()
