#!/usr/bin/env python3
"""Generate demo app icons (128x128 RGBA PNG) for Markup demos."""

from __future__ import annotations

import math
import struct
import zlib
from pathlib import Path

SIZE = 128
OUT = Path(__file__).resolve().parent.parent / "apps" / "demo_minimal" / "assets"


def write_png(path: Path, w: int, h: int, rgba: bytes) -> None:
    raw = b""
    stride = w * 4
    for y in range(h):
        raw += b"\x00" + rgba[y * stride : (y + 1) * stride]

    def chunk(tag: bytes, data: bytes) -> bytes:
        crc = zlib.crc32(tag + data) & 0xFFFFFFFF
        return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", crc)

    ihdr = struct.pack(">IIBBBBB", w, h, 8, 6, 0, 0, 0)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(b"\x89PNG\r\n\x1a\n")
        f.write(chunk(b"IHDR", ihdr))
        f.write(chunk(b"IDAT", zlib.compress(raw, 9)))
        f.write(chunk(b"IEND", b""))


def blank() -> bytearray:
    return bytearray(SIZE * SIZE * 4)


def set_px(buf: bytearray, x: int, y: int, r: int, g: int, b: int, a: int = 255) -> None:
    if x < 0 or y < 0 or x >= SIZE or y >= SIZE:
        return
    i = (y * SIZE + x) * 4
    if a == 0:
        buf[i : i + 4] = bytes([0, 0, 0, 0])
        return
    # Simple alpha over existing
    oa = buf[i + 3] / 255.0
    na = a / 255.0
    out_a = na + oa * (1 - na)
    if out_a <= 0:
        return
    for c, nc in zip(range(i, i + 3), (r, g, b)):
        oc = buf[c] / 255.0
        buf[c] = int(round((nc * na + oc * oa * (1 - na)) / out_a))
    buf[i + 3] = int(round(out_a * 255))


def fill_circle(buf: bytearray, cx: float, cy: float, rad: float, rgb: tuple[int, int, int], a: int = 255) -> None:
    r, g, b = rgb
    r0 = int(cx - rad - 2)
    r1 = int(cx + rad + 2)
    c0 = int(cy - rad - 2)
    c1 = int(cy + rad + 2)
    for y in range(max(0, c0), min(SIZE, c1 + 1)):
        for x in range(max(0, r0), min(SIZE, r1 + 1)):
            d = math.hypot(x + 0.5 - cx, y + 0.5 - cy)
            if d <= rad:
                set_px(buf, x, y, r, g, b, a)
            elif d < rad + 1:
                t = 1 - (d - rad)
                set_px(buf, x, y, r, g, b, int(a * t))


def fill_round_rect(
    buf: bytearray,
    x0: float,
    y0: float,
    x1: float,
    y1: float,
    radius: float,
    rgb: tuple[int, int, int],
    a: int = 255,
) -> None:
    r, g, b = rgb
    radius = min(radius, (x1 - x0) * 0.5, (y1 - y0) * 0.5)
    for y in range(int(y0), int(math.ceil(y1))):
        for x in range(int(x0), int(math.ceil(x1))):
            px = x + 0.5
            py = y + 0.5
            inside = True
            if px < x0 + radius and py < y0 + radius:
                inside = math.hypot(px - (x0 + radius), py - (y0 + radius)) <= radius
            elif px > x1 - radius and py < y0 + radius:
                inside = math.hypot(px - (x1 - radius), py - (y0 + radius)) <= radius
            elif px < x0 + radius and py > y1 - radius:
                inside = math.hypot(px - (x0 + radius), py - (y1 - radius)) <= radius
            elif px > x1 - radius and py > y1 - radius:
                inside = math.hypot(px - (x1 - radius), py - (y1 - radius)) <= radius
            elif px < x0 or px > x1 or py < y0 or py > y1:
                inside = False
            if inside:
                set_px(buf, x, y, r, g, b, a)


def fill_poly(buf: bytearray, pts: list[tuple[float, float]], rgb: tuple[int, int, int], a: int = 255) -> None:
    r, g, b = rgb
    ys = [p[1] for p in pts]
    y_min = max(0, int(min(ys)))
    y_max = min(SIZE - 1, int(max(ys)))
    for y in range(y_min, y_max + 1):
        py = y + 0.5
        xs: list[float] = []
        n = len(pts)
        for i in range(n):
            x1, y1 = pts[i]
            x2, y2 = pts[(i + 1) % n]
            if y1 == y2:
                continue
            if (y1 <= py < y2) or (y2 <= py < y1):
                t = (py - y1) / (y2 - y1)
                xs.append(x1 + t * (x2 - x1))
        xs.sort()
        for i in range(0, len(xs), 2):
            if i + 1 >= len(xs):
                break
            x_start = int(math.floor(xs[i]))
            x_end = int(math.ceil(xs[i + 1]))
            for x in range(max(0, x_start), min(SIZE, x_end + 1)):
                set_px(buf, x, y, r, g, b, a)


def draw_files(buf: bytearray, accent: tuple[int, int, int]) -> None:
    light = tuple(min(255, c + 40) for c in accent)
    fill_round_rect(buf, 28, 46, 100, 102, 10, accent)
    fill_round_rect(buf, 34, 36, 72, 54, 6, light)
    fill_round_rect(buf, 28, 50, 100, 102, 10, accent)


def draw_browser(buf: bytearray, accent: tuple[int, int, int]) -> None:
    fill_round_rect(buf, 24, 30, 104, 98, 12, (32, 36, 46))
    fill_round_rect(buf, 24, 30, 104, 48, 12, accent)
    fill_circle(buf, 36, 39, 4, (20, 24, 32))
    fill_circle(buf, 50, 39, 4, (20, 24, 32))
    fill_circle(buf, 64, 39, 4, (20, 24, 32))
    fill_circle(buf, 64, 72, 22, accent)
    fill_circle(buf, 64, 72, 16, (32, 36, 46))
    for angle in range(0, 360, 45):
        rad = math.radians(angle)
        x0 = 64 + math.cos(rad) * 8
        y0 = 72 + math.sin(rad) * 8
        x1 = 64 + math.cos(rad) * 20
        y1 = 72 + math.sin(rad) * 20
        fill_poly(buf, [(x0, y0), (x1, y1), (x1 + 1, y1 + 1), (x0 + 1, y0 + 1)], accent)


def draw_settings(buf: bytearray, accent: tuple[int, int, int]) -> None:
    cx, cy = 64, 64
    for i in range(8):
        a0 = math.radians(i * 45 - 22.5)
        a1 = math.radians(i * 45 + 22.5)
        outer = 38
        inner = 26
        pts = [
            (cx + math.cos(a0) * inner, cy + math.sin(a0) * inner),
            (cx + math.cos(a0) * outer, cy + math.sin(a0) * outer),
            (cx + math.cos(a1) * outer, cy + math.sin(a1) * outer),
            (cx + math.cos(a1) * inner, cy + math.sin(a1) * inner),
        ]
        fill_poly(buf, pts, accent)
    fill_circle(buf, cx, cy, 18, accent)
    fill_circle(buf, cx, cy, 10, (20, 24, 32))


def draw_terminal(buf: bytearray, accent: tuple[int, int, int]) -> None:
    fill_round_rect(buf, 24, 28, 104, 100, 12, (32, 36, 46))
    fill_round_rect(buf, 24, 28, 104, 46, 12, accent)
    fill_circle(buf, 38, 37, 4, (20, 24, 32))
    fill_circle(buf, 52, 37, 4, (20, 24, 32))
    fill_round_rect(buf, 38, 60, 52, 72, 3, accent)
    fill_round_rect(buf, 58, 60, 88, 72, 3, (148, 163, 184))
    fill_round_rect(buf, 38, 78, 78, 90, 3, (148, 163, 184))


ICONS = [
    ("icon_files.png", (45, 212, 191), draw_files),
    ("icon_browser.png", (74, 222, 128), draw_browser),
    ("icon_settings.png", (251, 146, 60), draw_settings),
    ("icon_terminal.png", (148, 163, 184), draw_terminal),
]


def make_icon(accent: tuple[int, int, int], draw_fn) -> bytes:
    buf = blank()
    draw_fn(buf, accent)
    return bytes(buf)


def make_sheet(cells: list[bytes]) -> bytes:
    w = SIZE * len(cells)
    h = SIZE
    out = bytearray(w * h * 4)
    for i, cell in enumerate(cells):
        for y in range(SIZE):
            src = y * SIZE * 4
            dst = (y * w + i * SIZE) * 4
            out[dst : dst + SIZE * 4] = cell[src : src + SIZE * 4]
    return bytes(out)


def main() -> None:
    cells = []
    for name, accent, draw_fn in ICONS:
        data = make_icon(accent, draw_fn)
        write_png(OUT / name, SIZE, SIZE, data)
        cells.append(data)
        print(f"wrote {OUT / name}")

    sheet = make_sheet(cells)
    write_png(OUT / "app_icons_sheet.png", SIZE * len(cells), SIZE, sheet)
    print(f"wrote {OUT / 'app_icons_sheet.png'}")

    # Keep legacy names as copies for older references
    legacy = [
        ("demo_icon_teal.png", 0),
        ("demo_icon_green.png", 1),
        ("demo_icon_orange.png", 2),
        ("demo_icon_gray.png", 3),
    ]
    for legacy_name, idx in legacy:
        write_png(OUT / legacy_name, SIZE, SIZE, cells[idx])
        print(f"wrote {OUT / legacy_name}")


if __name__ == "__main__":
    main()
