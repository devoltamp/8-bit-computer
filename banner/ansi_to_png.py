#!/usr/bin/env python3
"""
Convert ANSI block-art (truecolor escape codes) into a transparent PNG.

Usage:
    python ansi_to_png.py art.py  out.png          # runs art.py, captures its output
    python ansi_to_png.py art.txt out.png          # reads a raw ANSI text file
    python ansi_to_png.py art.py  out.png --scale 2 --cell 8x16

Requires: pip install pillow
"""
import argparse
import os
import re
import subprocess
import sys

from PIL import Image, ImageDraw

DEFAULT_FG = (204, 204, 204)

# one token = SGR escape | newline | single visible character
TOKEN = re.compile(r"\x1b\[([0-9;]*)m|(\n)|([^\x1b\n])")

# standard 8 + bright 8 colours (used only for 30-37 / 90-97 / 40-47 / 100-107)
BASIC = [
    (0, 0, 0), (170, 0, 0), (0, 170, 0), (170, 85, 0),
    (0, 0, 170), (170, 0, 170), (0, 170, 170), (170, 170, 170),
    (85, 85, 85), (255, 85, 85), (85, 255, 85), (255, 255, 85),
    (85, 85, 255), (255, 85, 255), (85, 255, 255), (255, 255, 255),
]


def apply_sgr(params, fg, bg):
    codes = [int(p) if p else 0 for p in params.split(";")] if params else [0]
    i = 0
    while i < len(codes):
        c = codes[i]
        if c == 0:
            fg, bg = None, None
        elif c == 39:
            fg = None
        elif c == 49:
            bg = None
        elif 30 <= c <= 37:
            fg = BASIC[c - 30]
        elif 90 <= c <= 97:
            fg = BASIC[c - 90 + 8]
        elif 40 <= c <= 47:
            bg = BASIC[c - 40]
        elif 100 <= c <= 107:
            bg = BASIC[c - 100 + 8]
        elif c in (38, 48):
            if i + 1 < len(codes) and codes[i + 1] == 2 and i + 4 < len(codes):
                colour = tuple(codes[i + 2:i + 5])
                i += 4
            elif i + 1 < len(codes) and codes[i + 1] == 5:
                colour = None  # 256-colour palette not handled
                i += 2
            else:
                colour = None
            if c == 38:
                fg = colour
            else:
                bg = colour
        i += 1
    return fg, bg


def parse(text):
    """Return a list of rows; each row is a list of (char, fg, bg)."""
    rows, row = [], []
    fg = bg = None
    for m in TOKEN.finditer(text):
        sgr, newline, ch = m.groups()
        if sgr is not None:
            fg, bg = apply_sgr(sgr, fg, bg)
        elif newline:
            rows.append(row)
            row = []
        else:
            row.append((ch, fg, bg))
    if row:
        rows.append(row)
    return rows


def render(rows, cw, ch_):
    width = max((len(r) for r in rows), default=0)
    img = Image.new("RGBA", (width * cw, len(rows) * ch_), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    hw, hh = cw // 2, ch_ // 2
    shades = {"░": 0.25, "▒": 0.50, "▓": 0.75}

    for y, row in enumerate(rows):
        for x, (c, fg, bg) in enumerate(row):
            x0, y0 = x * cw, y * ch_
            x1, y1 = x0 + cw - 1, y0 + ch_ - 1
            f = (fg or DEFAULT_FG) + (255,)

            if bg:  # background colour fills the whole cell first
                d.rectangle([x0, y0, x1, y1], fill=bg + (255,))

            if c == "█":
                d.rectangle([x0, y0, x1, y1], fill=f)
            elif c == "▀":
                d.rectangle([x0, y0, x1, y0 + hh - 1], fill=f)
            elif c == "▄":
                d.rectangle([x0, y0 + hh, x1, y1], fill=f)
            elif c == "▌":
                d.rectangle([x0, y0, x0 + hw - 1, y1], fill=f)
            elif c == "▐":
                d.rectangle([x0 + hw, y0, x1, y1], fill=f)
            elif c in shades:
                a = shades[c]
                if bg:  # blend fg over the cell's bg colour
                    mixed = tuple(round(f[k] * a + bg[k] * (1 - a)) for k in range(3))
                    d.rectangle([x0, y0, x1, y1], fill=mixed + (255,))
                else:   # no bg -> keep it transparent-friendly via alpha
                    d.rectangle([x0, y0, x1, y1], fill=f[:3] + (round(255 * a),))
            # spaces / unknown chars: only the bg (if any) is drawn
    return img


def main():
    ap = argparse.ArgumentParser(description="ANSI art -> transparent PNG")
    ap.add_argument("input", help=".py script that prints the art, or a raw ANSI text file")
    ap.add_argument("output", nargs="?", default="output.png")
    ap.add_argument("--cell", default="10x20", help="pixel size of one character cell, WxH (default 10x20)")
    ap.add_argument("--scale", type=int, default=1, help="integer upscale factor (nearest-neighbour)")
    ap.add_argument("--no-crop", action="store_true", help="keep the empty transparent margins")
    args = ap.parse_args()

    cw, chh = (int(v) for v in args.cell.lower().split("x"))

    if args.input.endswith(".py"):
        # Force the child process to print UTF-8. On Windows its stdout defaults
        # to cp1252, which can't encode the block characters and crashes.
        env = dict(os.environ, PYTHONIOENCODING="utf-8", PYTHONUTF8="1")
        proc = subprocess.run(
            [sys.executable, args.input], capture_output=True, text=True,
            encoding="utf-8", env=env,
        )
        if proc.returncode != 0:
            sys.exit(f"{args.input} failed:\n{proc.stderr}")
        text = proc.stdout
    else:
        with open(args.input, encoding="utf-8") as fh:
            text = fh.read()

    img = render(parse(text), cw, chh)

    if not args.no_crop:
        bbox = img.getchannel("A").getbbox()
        if bbox:
            img = img.crop(bbox)
    if args.scale > 1:
        img = img.resize((img.width * args.scale, img.height * args.scale), Image.NEAREST)

    img.save(args.output)
    print(f"Saved {args.output} ({img.width}x{img.height}, transparent background)")


if __name__ == "__main__":
    main()
