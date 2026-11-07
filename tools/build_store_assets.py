#!/usr/bin/env python3
"""Generate Hearth's app store icons: the face in miniature, its digits
filled with the Doom fire in the Classic palette.

    python3 tools/build_store_assets.py

Outputs store/icon-80.png (iconSmall) and store/icon-144.png (iconLarge).
The fire is a Python copy of src/c/fire.c's spreadFire (same rule, same
offset distribution), seeded so the output is reproducible.
"""

import os
import random
import sys

from PIL import Image, ImageDraw, ImageFont

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
FONT = os.path.join(ROOT, "tools", "fonts", "Rubik-Black.ttf")
OUT = os.path.join(ROOT, "store")

MAX_HEAT = 36
# Classic theme stops, as in src/c/flames.c.
STOPS = [(0, 0x000000), (8, 0x550000), (15, 0xAA0000), (23, 0xFF5500),
         (30, 0xFFAA00), (34, 0xFFFF55), (36, 0xFFFFFF)]
RAND_IDX = [0, 1, 1, 2, 2, 3]  # Math.round(Math.random() * 3)


def palette():
    pal = [None] * (MAX_HEAT + 1)
    for (a, ca), (b, cb) in zip(STOPS, STOPS[1:]):
        for h in range(a, b + 1):
            t = (h - a) / (b - a)
            rgb = []
            for shift in (16, 8, 0):
                x, y = (ca >> shift) & 255, (cb >> shift) & 255
                v = x + (y - x) * t
                rgb.append(int((v + 42) // 85 * 85))  # Pebble-exact levels
            pal[h] = tuple(rgb)
    return pal


def doom_fire(width, rows, frames, seed):
    """Doom's spreadFire, column by column, edges wrapping within the row."""
    rng = random.Random(seed)
    grid = [[0] * width for _ in range(rows)] + [[MAX_HEAT] * width]
    for _ in range(frames):
        for x in range(width):
            for y in range(1, rows + 1):
                pixel = grid[y][x]
                if pixel == 0:
                    grid[y - 1][x] = 0
                    continue
                r = RAND_IDX[rng.randrange(6)]
                grid[y - 1][(x - r + 1) % width] = pixel - (r & 1)
    return grid[:rows]


def icon(size, text_rows, seed=7):
    pal = palette()
    font = ImageFont.truetype(FONT, int(size * (0.52 if len(text_rows) > 1 else 0.62)))
    # Lay the rows out like the face: centered, with a small gap between them.
    mask = Image.new("L", (size, size), 0)
    draw = ImageDraw.Draw(mask)
    boxes = [draw.textbbox((0, 0), t, font=font) for t in text_rows]
    heights = [b[3] - b[1] for b in boxes]
    gap = int(size * 0.05)
    total = sum(heights) + gap * (len(text_rows) - 1)
    y = (size - total) // 2
    top = y
    for t, b, h in zip(text_rows, boxes, heights):
        w = b[2] - b[0]
        draw.text(((size - w) // 2 - b[0], y - b[1]), t, font=font, fill=255)
        y += h + gap
    bottom = y - gap
    # One fire row per pixel row of the digit block (about 2 px per cell
    # across), with the top reaching tip heat ~9 like the face's default.
    cell = 2 if size >= 100 else 1
    cols = (size + cell - 1) // cell
    rows = max(8, (bottom - top + cell - 1) // cell)
    grid = doom_fire(cols, rows, frames=rows * 4, seed=seed)
    # Doom loses ~0.43 levels per row; stretch so the tips land near heat 9.
    img = Image.new("RGB", (size, size), (0, 0, 0))
    px, mk = img.load(), mask.load()
    for yy in range(size):
        for xx in range(size):
            if mk[xx, yy] >= 128:
                fy = min(rows - 1, max(0, (yy - top) * rows // max(1, bottom - top)))
                px[xx, yy] = pal[grid[fy][xx // cell]]
    return img


def main():
    os.makedirs(OUT, exist_ok=True)
    # The face itself in miniature: HH over MM, filled with fire.
    icon(144, ["10", "09"]).save(os.path.join(OUT, "icon-144.png"), optimize=True)
    icon(80, ["10", "09"]).save(os.path.join(OUT, "icon-80.png"), optimize=True)
    return 0


if __name__ == "__main__":
    sys.exit(main())
