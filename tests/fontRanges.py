#!/usr/bin/env python3
#
#  Gweni
#  Copyright (c) 2010 Facepunch Studios
#  MIT License, see LICENSE
#
#  Pins the range generator's glyph-0 handling, format-4 offsets and wrapped
#  deltas, Unicode charmap filtering, format-12 supplementary coverage, and
#  contiguous CSS ranges. The real downloaded face must reproduce its
#  checked-in manifest exactly and cover the demo's CJK line.
#

import ctypes
import ctypes.util
import importlib.util
import json
from pathlib import Path
import struct
import sys
import unittest

ROOT = Path(__file__).resolve().parents[1]
spec = importlib.util.spec_from_file_location("font_ranges", ROOT / "tools/gen_font_ranges.py")
generator = importlib.util.module_from_spec(spec)
spec.loader.exec_module(generator)


def freetype_codepoints(font):
    # The host's FreeType selects the full Unicode charmap independently of
    # the parser. This resource-tool test runs on the container host in every
    # tree, including wasm; it does not create a GL/Vulkan context.
    library_name = ctypes.util.find_library("freetype")
    if not library_name:
        raise RuntimeError("font_ranges needs the host FreeType library")
    ft = ctypes.CDLL(library_name)
    pointer, uint, ulong = ctypes.c_void_p, ctypes.c_uint, ctypes.c_ulong
    ft.FT_Init_FreeType.argtypes = [ctypes.POINTER(pointer)]
    ft.FT_New_Face.argtypes = [pointer, ctypes.c_char_p, ctypes.c_long, ctypes.POINTER(pointer)]
    ft.FT_Get_First_Char.argtypes = [pointer, ctypes.POINTER(uint)]
    ft.FT_Get_First_Char.restype = ulong
    ft.FT_Get_Next_Char.argtypes = [pointer, ulong, ctypes.POINTER(uint)]
    ft.FT_Get_Next_Char.restype = ulong
    ft.FT_Done_Face.argtypes = [pointer]
    ft.FT_Done_FreeType.argtypes = [pointer]
    library, face = pointer(), pointer()
    if ft.FT_Init_FreeType(ctypes.byref(library)):
        raise RuntimeError("could not initialize FreeType")
    try:
        if ft.FT_New_Face(library, str(font).encode(), 0, ctypes.byref(face)):
            raise RuntimeError("FreeType could not load %s" % font)
        try:
            codepoints, glyph = set(), uint()
            cp = ft.FT_Get_First_Char(face, ctypes.byref(glyph))
            while glyph.value:
                codepoints.add(cp)
                cp = ft.FT_Get_Next_Char(face, cp, ctypes.byref(glyph))
            return codepoints
        finally:
            ft.FT_Done_Face(face)
    finally:
        ft.FT_Done_FreeType(library)


def full_cmap(groups):
    return struct.pack(">HHIII", 12, 0, 16 + 12 * len(groups), 0, len(groups)) + b"".join(
        struct.pack(">III", *group) for group in groups)


def font_fixture():
    # A uses glyphIdArray + delta; B's raw glyph 0 must stay missing; C's
    # nonzero raw glyph wraps to 0. FFFD wraps directly to 0, FFFE to 1.
    segments = [
        [0x43, 0xFFFE, 0xFFFF],  # endCode
        [0x41, 0xFFFD, 0xFFFF],  # startCode
        [1, 3, 1],              # idDelta
        [6, 0, 0],              # idRangeOffset
    ]
    bmp = struct.pack(">7H", 4, 46, 0, 6, 0, 0, 0)
    for i, array in enumerate(segments):
        bmp += struct.pack(">3H", *array)
        if i == 0:
            bmp += b"\0\0"  # reservedPad
    bmp += struct.pack(">3H", 2, 0, 0xFFFF)
    subtables = [
        (3, 1, bmp),
        (3, 10, full_cmap([(0x4E00, 0x4E01, 1), (0x10000, 0x10001, 0),
                           (0xD800, 0xD800, 1)])),
        (3, 0, full_cmap([(0x2603, 0x2603, 1)])),  # symbol, not Unicode
        (0, 5, struct.pack(">HII", 14, 10, 0)),    # variation sequences
    ]
    records, bodies = b"", b""
    offset = 4 + 8 * len(subtables)
    for platform, encoding, subtable in subtables:
        records += struct.pack(">HHI", platform, encoding, offset + len(bodies))
        bodies += subtable
    cmap = struct.pack(">HH", 0, len(subtables)) + records + bodies
    return (struct.pack(">I4H", 0x10000, 1, 0, 0, 0)
            + struct.pack(">4sIII", b"cmap", 0, 28, len(cmap)) + cmap)


class FontRanges(unittest.TestCase):
    def test_unicode_cmaps_exclude_missing_glyphs_and_merge_ranges(self):
        codepoints = generator.cmap_codepoints(font_fixture())
        self.assertEqual(codepoints, {0x41, 0x4E00, 0x4E01, 0xFFFE, 0x10001})
        self.assertEqual(generator.unicode_ranges(codepoints),
                         "U+0041, U+4E00-4E01, U+FFFE, U+10001")

    def test_droid_manifest_matches_downloaded_font(self):
        data = FONT.read_bytes()
        codepoints = generator.cmap_codepoints(data)
        self.assertEqual(codepoints, freetype_codepoints(FONT))
        expected = (ROOT / "resources-lazy/DroidSansFallbackFull-ranges.json").read_text()
        self.assertEqual(generator.manifest(FONT, data), expected)
        self.assertEqual(json.loads(expected)["sha256"],
                         "2392015530438bafc48edfc4aee6d9de2387f627a6134d8ab3dfcc99d21c8240")
        self.assertTrue({ord(cp) for cp in "单元测试あア"} <= codepoints)
        self.assertNotIn(0x10FFFF, codepoints)
        self.assertNotIn(ord("A"), codepoints)


if __name__ == "__main__":
    if len(sys.argv) < 2:
        sys.exit("usage: fontRanges.py FONT [unittest selectors/options]")
    FONT = Path(sys.argv.pop(1))
    unittest.main()
