#!/usr/bin/env python3
# Gweni
# Copyright (c) 2010 Facepunch Studios
# MIT License, see LICENSE
#
# Writes the two images MarkdownDemo delivers to its reference document a few
# frames late (resources/MarkdownDemoPipeline.png, MarkdownDemoDusk.jpg), the
# bytes an app would have fetched. Run from anywhere; needs Pillow. The output
# is checked in, so this runs only to change them.
import os
from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..')
RES = os.path.join(ROOT, 'resources')

# Tokyo Night
BG = (26, 27, 38)
BOX = (41, 46, 66)
LINE = (122, 162, 247)
TEXT = (192, 202, 245)
ACCENT = (187, 154, 247)


def pipeline():
    # wider than the demo's view, so the document scales it down to its width
    w, h = 1600, 120
    image = Image.new('RGB', (w, h), BG)
    draw = ImageDraw.Draw(image)
    font = ImageFont.truetype(os.path.join(RES, 'OpenSans.ttf'), 30)
    steps = ['setMarkdown', 'parse', 'diff', 'blocks', 'layout', 'draw']
    box_w, gap, top = 224, 44, 20
    x = (w - (len(steps) * box_w + (len(steps) - 1) * gap)) // 2
    for i, step in enumerate(steps):
        draw.rounded_rectangle((x, top, x + box_w, h - top), radius=14, fill=BOX,
                               outline=ACCENT if i == 2 else LINE, width=4)
        tw = draw.textlength(step, font=font)
        draw.text((x + (box_w - tw) / 2, h / 2), step, font=font, fill=TEXT, anchor='lm')
        if i + 1 < len(steps):
            ax = x + box_w + 10
            draw.line((ax, h / 2, ax + gap - 24, h / 2), fill=LINE, width=4)
            draw.polygon([(ax + gap - 20, h / 2), (ax + gap - 34, h / 2 - 10), (ax + gap - 34, h / 2 + 10)], fill=LINE)
        x += box_w + gap
    image.save(os.path.join(RES, 'MarkdownDemoPipeline.png'), optimize=True)


def dusk():
    w, h = 240, 80
    image = Image.new('RGB', (w, h))
    pixels = image.load()
    for y in range(h):
        t = y / (h - 1)
        sky = tuple(int(a + (b - a) * t) for a, b in zip((36, 40, 89), (247, 118, 142)))
        for x in range(w):
            pixels[x, y] = sky
    draw = ImageDraw.Draw(image)
    draw.ellipse((150, 44, 190, 84), fill=(255, 158, 100))
    hills = [(0, h)] + [(x, int(58 + 10 * ((x * 37) % 23) / 23 - 8 * ((x // 40) % 2))) for x in range(0, w + 1, 20)] + [(w, h)]
    draw.polygon(hills, fill=(52, 59, 88))
    image.save(os.path.join(RES, 'MarkdownDemoDusk.jpg'), quality=85)


pipeline()
dusk()
