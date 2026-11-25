#!/usr/bin/env python3
"""Bakes the game's three type sizes from Poppins (SIL Open Font License 1.1) into C: grey-level
glyphs for ' ' to '~', one atlas row a size. Usage: font.py OUT.c
POPPINS_DIR: the folder with Poppins-Medium.ttf and Poppins-Bold.ttf (default: the system's)."""
import os, sys
from PIL import Image, ImageDraw, ImageFont

DIR = os.environ.get('POPPINS_DIR', '/usr/share/fonts/truetype/google-fonts')
SIZES = [('font_s', 'Poppins-Medium.ttf', 14), ('font_m', 'Poppins-Bold.ttf', 19), ('font_l', 'Poppins-Bold.ttf', 34)]

out = ['/* made by tools/font.py from Poppins (SIL Open Font License 1.1): do not edit */',
       '#include "c4.h"']
for name, file, size in SIZES:
    f = ImageFont.truetype(os.path.join(DIR, file), size)
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
