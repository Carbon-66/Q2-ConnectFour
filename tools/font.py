#!/usr/bin/env python3
"""Bakes the game's four type sizes, all in ITC Avant Garde Gothic Bold, into C: grey-level glyphs
for ' ' to '~', one atlas row a size. Usage: font.py OUT.c
FONT_BOLD: the font file. The default is TeX Gyre Adventor Bold (GUST Font License), the free
clone of ITC Avant Garde Gothic; point FONT_BOLD at a licensed ITC Avant Garde Bold to use that."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

BOLD = os.environ.get('FONT_BOLD', '/usr/share/texmf/fonts/opentype/public/tex-gyre/texgyreadventor-bold.otf')
SIZES = [('font_s', 14), ('font_m', 20), ('font_menu', 24), ('font_l', 34)]

out = [f'/* made by tools/font.py from {os.path.basename(BOLD)}: do not edit */',
       '#include "c4.h"']
for name, size in SIZES:
    f = ImageFont.truetype(BOLD, size)
    asc, desc = f.getmetrics()
    h = asc + desc
    glyphs = []
    for code in range(32, 127):
        ch = chr(code)
        adv = round(f.getlength(ch))
        l, t, r, b = f.getbbox(ch) if ch != ' ' else (0, 0, 0, 0)
        w = max(0, r - l)
        im = Image.new('L', (max(1, w), h), 0)
        if w:
            ImageDraw.Draw(im).text((-l, 0), ch, font=f, fill=255)
        glyphs.append((im, w, l, adv))
    atlas_w = sum(max(1, g[1]) for g in glyphs)
    atlas = Image.new('L', (atlas_w, h), 0)
    xs, x = [], 0
    for im, w, l, adv in glyphs:
        atlas.paste(im, (x, 0)); xs.append(x); x += max(1, w)
    data = atlas.tobytes()
    out.append(f'static const uint8_t {name}_px[{len(data)}] = {{' + ','.join(map(str, data)) + '};')
    arr = lambda v: '{' + ','.join(map(str, v)) + '}'
    out.append(f'const Font {name} = {{ {h}, {asc}, {name}_px, {atlas_w}, {arr(xs)}, {arr([g[1] for g in glyphs])}, '
               f'{arr([g[2] for g in glyphs])}, {arr([g[3] for g in glyphs])} }};')
open(sys.argv[1], 'w').write('\n'.join(out) + '\n')
