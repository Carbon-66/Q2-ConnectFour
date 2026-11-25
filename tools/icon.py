#!/usr/bin/env python3
"""The card's icons, drawn: a corner of the blue board with red and yellow discs in a rounded
square. card/icon.png (52 px, Library's Games row) and card/icon-40.png (40 px). Usage: tools/icon.py"""
import pathlib
from PIL import Image, ImageDraw

def icon(size):
    S = size * 8  # drawn large, then scaled down smooth
    im = Image.new('RGBA', (S, S), (0, 0, 0, 0))
    bg = Image.new('RGB', (S, S), (33, 96, 221))
    d = ImageDraw.Draw(bg)
    for y in range(S):  # lighter at the top
        t = y / S
        d.line([0, y, S, y], fill=(int(56 - 23 * t), int(124 - 28 * t), int(244 - 23 * t)))
    discs = [[None, None, None], [None, 'y', None], ['r', 'y', 'r']]
    cell = S / 3
    for r in range(3):
        for c in range(3):
            cx, cy, rad = c * cell + cell / 2, r * cell + cell / 2, cell * 0.38
            col = {'r': (230, 59, 54), 'y': (246, 194, 44), None: (16, 24, 54)}[discs[r][c]]
            d.ellipse([cx - rad, cy - rad, cx + rad, cy + rad], fill=col)
            if discs[r][c]:
                lite = {'r': (255, 138, 120), 'y': (255, 240, 160)}[discs[r][c]]
                d.ellipse([cx - rad * 0.62, cy - rad * 0.62, cx + rad * 0.62, cy + rad * 0.62], outline=lite, width=int(S * 0.012))
    mask = Image.new('L', (S, S), 0)
    ImageDraw.Draw(mask).rounded_rectangle([0, 0, S - 1, S - 1], radius=int(S * 0.22), fill=255)
    im.paste(bg, (0, 0), mask)
    return im.resize((size, size), Image.LANCZOS)

root = pathlib.Path(__file__).resolve().parent.parent
icon(52).save(root / 'card' / 'icon.png')
icon(40).save(root / 'card' / 'icon-40.png')
