#!/usr/bin/env python3
"""Bakes the title logo (art/logo.png, transparent around it) into C as an ARGB image scaled to
WIDTH px across. Usage: logo.py OUT.c"""
import pathlib, sys
from PIL import Image

WIDTH = 300
root = pathlib.Path(__file__).resolve().parent.parent
im = Image.open(root / 'art' / 'logo.png').convert('RGBA')
im = im.crop(im.getbbox())
im = im.resize((WIDTH, round(im.height * WIDTH / im.width)), Image.LANCZOS)
raw = im.tobytes()
px = [raw[i + 3] << 24 | raw[i] << 16 | raw[i + 1] << 8 | raw[i + 2] for i in range(0, len(raw), 4)]
open(sys.argv[1], 'w').write(
    '/* made by tools/logo.py from art/logo.png: do not edit */\n#include "c4.h"\n'
    f'static uint32_t logo_px[] = {{' + ','.join(f'0x{p:08x}' for p in px) + '};\n'
    f'const Img logo = {{ {im.width}, {im.height}, logo_px }};\n')
