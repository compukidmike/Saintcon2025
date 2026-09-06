#!/usr/bin/env python3
"""Compose a Saintcon 2025 badge attract-screen preview from 2025-only assets.

Badge display (sdkconfig): portrait ST7789, CONFIG_LCD_WIDTH=240,
CONFIG_LCD_HEIGHT=320 → LV_HOR_RES=240, LV_VER_RES=320.
"""

from __future__ import annotations

import math
import shutil
import subprocess
import tempfile
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont

ROOT = Path(__file__).resolve().parent
ASSETS = ROOT / "assets"
OUT_MP4 = ROOT / "saintcon2025_attract.mp4"
OUT_GIF = ROOT / "saintcon2025_attract_badge.gif"
FACTION_SPIN = ASSETS / "Faction-Sc25-Spin.mp4"

W, H = 240, 320
FPS = 12
DURATION_S = 60
TOTAL = FPS * DURATION_S

MENU_OPTIONS = ["Game", "Settings", "Code", "Credits"]
FACTION_COLORS = [
    (0xDA, 0x38, 0x32),  # Ember Claw / HQ 24
    (0x00, 0xAA, 0xE9),  # Aether Watch / HQ 20
    (0x00, 0xA3, 0x59),  # Verdant Pact / HQ 21
    (0xEA, 0x98, 0x3E),  # Iron Howl / HQ 23
    (0x86, 0x2F, 0x8B),  # Dream Coil / HQ 25
    (0xFF, 0xF3, 0x4A),  # Dawn Accord / HQ 22
]

# Port uplinks from Firmware/badge/components/badge_game/nodes.c (node_id -> peers)
NODE_LINKS: dict[int, list[int]] = {
    1: [2, 3, 4, 5, 6, 7],
    2: [1, 3, 7, 8, 9, 19],
    3: [1, 2, 4, 9, 10, 11],
    4: [1, 3, 5, 11, 12, 13],
    5: [1, 4, 6, 13, 14, 15],
    6: [1, 5, 7, 15, 16, 17],
    7: [1, 2, 6, 17, 18, 19],
    8: [2, 9, 19, 20, 21, 26],
    9: [2, 3, 8, 10, 21],
    10: [3, 9, 11, 21, 22, 27],
    11: [3, 4, 10, 12, 22],
    12: [4, 11, 13, 22, 23, 28],
    13: [4, 5, 12, 14, 23],
    14: [5, 13, 15, 23, 24, 29],
    15: [5, 6, 14, 16, 24],
    16: [6, 15, 17, 24, 25, 30],
    17: [6, 7, 16, 18, 25],
    18: [7, 17, 19, 20, 25, 31],
    19: [2, 7, 8, 18, 20],
    20: [8, 18, 19],
    21: [8, 9, 10],
    22: [10, 11, 12],
    23: [12, 13, 14],
    24: [14, 15, 16],
    25: [16, 17, 18],
    26: [8],
    27: [10],
    28: [12],
    29: [14],
    30: [16],
    31: [18],
}

# HQ node_id → faction color index (from factions.h hq_node mapping)
HQ_FACTION = {20: 1, 21: 2, 22: 5, 23: 3, 24: 0, 25: 4}


def load_rgb(name: str) -> Image.Image:
    return Image.open(ASSETS / name).convert("RGBA")


def font(size: int):
    for path in (
        "/System/Library/Fonts/Supplemental/Courier New Bold.ttf",
        "/System/Library/Fonts/Supplemental/Courier New.ttf",
        "/Library/Fonts/Courier New.ttf",
        "/System/Library/Fonts/Menlo.ttc",
    ):
        try:
            return ImageFont.truetype(path, size)
        except OSError:
            continue
    return ImageFont.load_default()


F1, F2, F3 = font(11), font(14), font(20)
F_TITLE = font(16)
F_SMALL = font(10)


def paste(base: Image.Image, overlay: Image.Image, xy=(0, 0)):
    base.alpha_composite(overlay, xy)


def text_center(draw: ImageDraw.ImageDraw, text: str, y: int, f, fill=(255, 255, 255, 255)):
    bbox = draw.textbbox((0, 0), text, font=f)
    tw = bbox[2] - bbox[0]
    draw.text(((W - tw) // 2, y), text, font=f, fill=fill)


def round_rect(draw, box, radius, fill):
    draw.rounded_rectangle(box, radius=radius, fill=fill)


def fit_contain(img: Image.Image, box_w: int, box_h: int) -> Image.Image:
    img = img.copy()
    img.thumbnail((box_w, box_h), Image.NEAREST)
    return img


def fit_cover(img: Image.Image, box_w: int, box_h: int) -> Image.Image:
    src = img.convert("RGBA")
    scale = max(box_w / src.width, box_h / src.height)
    nw, nh = max(1, int(src.width * scale)), max(1, int(src.height * scale))
    resized = src.resize((nw, nh), Image.NEAREST)
    x0 = (nw - box_w) // 2
    y0 = (nh - box_h) // 2
    return resized.crop((x0, y0, x0 + box_w, y0 + box_h))


def extract_spin_frames() -> list[Image.Image]:
    tmp = Path(tempfile.mkdtemp(prefix="spin25_"))
    try:
        subprocess.check_call(
            [
                "ffmpeg",
                "-y",
                "-i",
                str(FACTION_SPIN),
                "-vf",
                f"scale={W}:{H}:force_original_aspect_ratio=increase,crop={W}:{H}",
                "-r",
                str(FPS),
                str(tmp / "s%04d.png"),
            ],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
        paths = sorted(tmp.glob("s*.png"))
        if not paths:
            raise RuntimeError("No frames extracted from Faction-Sc25-Spin.mp4")
        return [Image.open(p).convert("RGBA") for p in paths]
    finally:
        shutil.rmtree(tmp)


def unique_edges() -> list[tuple[int, int]]:
    seen: set[tuple[int, int]] = set()
    edges: list[tuple[int, int]] = []
    for a, peers in NODE_LINKS.items():
        for b in peers:
            if b <= 0:
                continue
            key = (min(a, b), max(a, b))
            if key not in seen:
                seen.add(key)
                edges.append(key)
    # Core-outward order: smaller max-distance from core first
    def dist(n: int) -> int:
        if n == 1:
            return 0
        if 2 <= n <= 7:
            return 1
        if 8 <= n <= 19:
            return 2
        return 3

    edges.sort(key=lambda e: (min(dist(e[0]), dist(e[1])), max(dist(e[0]), dist(e[1])), e[0], e[1]))
    return edges


EDGES = unique_edges()


def node_pos(node_id: int, cx: int = W // 2, cy: int = H // 2 + 8, spacing: float = 30.0) -> tuple[int, int]:
    """Match Firmware game.c get_node_position layout (scaled for full 240x320)."""
    r_1 = spacing * 1.00
    r_2 = spacing * 2.00
    r_tower = spacing * 2.95
    r_hq = spacing * 3.30
    hq_angles = [120.0, 60.0, 0.0, -60.0, -120.0, 180.0]
    tower_angles = [90.0, 30.0, -30.0, -90.0, -150.0, 150.0]

    if node_id == 1:
        return cx, cy
    if 2 <= node_id <= 7:
        angle = (90.0 - 60.0 * (node_id - 2)) * math.pi / 180.0
        radius = r_1
    elif 8 <= node_id <= 19:
        angle = (90.0 - 30.0 * (node_id - 8)) * math.pi / 180.0
        radius = r_2
    elif 20 <= node_id <= 25:
        angle = hq_angles[node_id - 20] * math.pi / 180.0
        radius = r_hq
    elif 26 <= node_id <= 31:
        angle = tower_angles[node_id - 26] * math.pi / 180.0
        radius = r_tower
    else:
        return cx, cy
    return cx + int(round(radius * math.cos(angle))), cy - int(round(radius * math.sin(angle)))


def scene_splash(t: float, bg: Image.Image, logo: Image.Image) -> Image.Image:
    frame = bg.copy()
    fade = min(1.0, t / 1.2)
    lg = logo.copy()
    if fade < 1:
        lg.putalpha(lg.split()[-1].point(lambda a: int(a * fade)))
    paste(frame, lg, ((W - lg.width) // 2, (H - lg.height) // 2 - 10))
    draw = ImageDraw.Draw(frame)
    if fade >= 1.0 and int(t * 2) % 2 == 0:
        text_center(draw, "PRESS CENTER TO START", 260, F2, (255, 230, 80, 255))
    return frame


def scene_menu(t: float, bg: Image.Image) -> Image.Image:
    frame = fit_cover(bg, W, H)
    shade = Image.new("RGBA", (W, H), (20, 20, 30, 140))
    paste(frame, shade)
    draw = ImageDraw.Draw(frame)
    text_center(draw, "Main Menu", 28, F_TITLE, (255, 255, 255, 255))

    selected = int(t * 1.4) % len(MENU_OPTIONS)
    box_w, row_h = 170, 36
    x0 = (W - box_w) // 2
    y0 = 80
    round_rect(draw, (x0 - 6, y0 - 6, x0 + box_w + 6, y0 + row_h * len(MENU_OPTIONS) + 6), 6, (40, 40, 50, 210))
    for i, opt in enumerate(MENU_OPTIONS):
        y = y0 + i * row_h
        if i == selected:
            round_rect(draw, (x0, y, x0 + box_w, y + row_h - 4), 4, (0, 140, 180, 255))
        draw.text((x0 + 18, y + 8), opt, font=F2, fill=(220, 230, 210, 255))
    text_center(draw, "CENTER: select", 295, F1, (160, 180, 200, 255))
    return frame


def scene_faction_spin(t: float, spin_frames: list[Image.Image]) -> Image.Image:
    idx = int(t * FPS) % len(spin_frames)
    frame = spin_frames[idx].copy()
    plate = Image.new("RGBA", (W, 28), (0, 0, 0, 150))
    paste(frame, plate, (0, 0))
    draw = ImageDraw.Draw(frame)
    text_center(draw, "Choose Your Faction", 7, F2, (255, 240, 120, 255))
    return frame


def scene_map_pan(t: float, map_img: Image.Image) -> Image.Image:
    frame = Image.new("RGBA", (W, H), (10, 12, 18, 255))
    src = map_img.convert("RGBA")
    scale = W / src.width
    nw, nh = W, max(H, int(src.height * scale))
    resized = src.resize((nw, nh), Image.NEAREST)
    max_off = max(0, nh - H)
    y_off = int((0.5 + 0.5 * math.sin(t * 0.7)) * max_off) if max_off else 0
    paste(frame, resized.crop((0, y_off, W, y_off + H)))
    bar = Image.new("RGBA", (W, 18), (0, 0, 0, 160))
    paste(frame, bar, (0, 0))
    draw = ImageDraw.Draw(frame)
    text_center(draw, "OVERVIEW MAP", 3, F1, (180, 220, 255, 255))
    if int(t * 2) % 2 == 0:
        text_center(draw, "FIND THE NODES", H - 24, F2, (255, 220, 80, 255))
    return frame


def scene_node_map(t: float, duration: float = 18.0) -> Image.Image:
    """Animate the badge game node graph with connections lighting up."""
    frame = Image.new("RGBA", (W, H), (12, 14, 22, 255))
    draw = ImageDraw.Draw(frame)
    text_center(draw, "NODE MAP", 8, F2, (255, 240, 120, 255))

    positions = {nid: node_pos(nid) for nid in NODE_LINKS}
    # Fill links across most of the scene, leave a beat at 100%
    lit = min(len(EDGES), int((t / max(0.1, duration * 0.85)) * len(EDGES)))

    for a, b in EDGES:
        xa, ya = positions[a]
        xb, yb = positions[b]
        draw.line([(xa, ya), (xb, yb)], fill=(70, 75, 90, 90), width=1)

    for i, (a, b) in enumerate(EDGES):
        if i >= lit:
            break
        xa, ya = positions[a]
        xb, yb = positions[b]
        pulse = 1.0 if i < lit - 1 else 0.55 + 0.45 * abs(math.sin(t * 8))
        green = int(255 * pulse)
        draw.line([(xa, ya), (xb, yb)], fill=(0, green, 80, 230), width=2)

    for nid, (x, y) in positions.items():
        if nid == 1:
            r, fill, border = 9, (240, 240, 245, 255), (255, 220, 80, 255)
        elif 20 <= nid <= 25:
            color = FACTION_COLORS[HQ_FACTION[nid]]
            r, fill, border = 8, (*color, 255), (255, 255, 255, 200)
        elif 26 <= nid <= 31:
            r, fill, border = 7, (180, 100, 255, 255), (255, 255, 255, 180)
        else:
            r, fill, border = 6, (120, 130, 150, 255), (30, 30, 40, 255)
        draw.ellipse((x - r, y - r, x + r, y + r), fill=fill, outline=border, width=1)

    live = min(100, int(100 * lit / max(1, len(EDGES))))
    text_center(draw, f"LINKS ONLINE  {live}%", H - 22, F1, (180, 255, 200, 255))
    return frame


def draw_giant_nut(draw: ImageDraw.ImageDraw, cx: int, cy: int, size: int, angle_deg: float):
    """Hex nut with circular hole — rotates with the wrench."""
    outer = []
    inner_r = size * 0.38
    for i in range(6):
        ang = math.radians(angle_deg + i * 60)
        outer.append((cx + size * math.cos(ang), cy + size * math.sin(ang)))
    # metal body
    draw.polygon(outer, fill=(160, 165, 170, 255), outline=(90, 95, 100, 255))
    # inner bevel ring
    draw.ellipse(
        (cx - inner_r - 3, cy - inner_r - 3, cx + inner_r + 3, cy + inner_r + 3),
        fill=(110, 115, 120, 255),
    )
    draw.ellipse(
        (cx - inner_r, cy - inner_r, cx + inner_r, cy + inner_r),
        fill=(25, 28, 32, 255),
    )


def scene_wrench(t: float, badge: Image.Image) -> Image.Image:
    frame = Image.new("RGBA", (W, H), (25, 28, 32, 255))
    for y in range(H):
        v = 25 + int(18 * math.sin(y / 40 + t))
        ImageDraw.Draw(frame).line([(0, y), (W, y)], fill=(v, v + 4, v + 8, 255))

    draw = ImageDraw.Draw(frame)
    # comically large nut under/near the wrench jaws
    nut_angle = t * 80  # degrees — turns with the gag
    draw_giant_nut(draw, W // 2 + 40, 175, 52, nut_angle)

    # wrench photo rocks as if turning the nut
    img = fit_contain(badge, 200, 140)
    rocked = img.rotate(12 * math.sin(t * 2.5), expand=True, resample=Image.NEAREST)
    paste(frame, rocked, ((W - rocked.width) // 2 - 20, 55))

    draw = ImageDraw.Draw(frame)
    text_center(draw, "50mm wrench * NFC Tip", 18, F2, (255, 220, 120, 255))
    text_center(draw, "& 10mm lanyard hole", 38, F2, (255, 220, 120, 255))
    return frame


def scene_finale(t: float, bg: Image.Image, logo: Image.Image) -> Image.Image:
    frame = fit_cover(bg, W, H)
    y = int((t * 70) % H)
    paste(frame, Image.new("RGBA", (W, 3), (255, 255, 255, 45)), (0, y))
    paste(frame, Image.new("RGBA", (W, 70), (0, 0, 0, 180)), (0, 120))
    paste(frame, logo, ((W - logo.width) // 2, 40))
    draw = ImageDraw.Draw(frame)
    text_center(draw, "INSERT BADGE", 135, F3, (255, 240, 100, 255))
    if int(t * 2.5) % 2 == 0:
        text_center(draw, "TO CONTINUE", 165, F2, (200, 220, 255, 255))
    text_center(draw, "SAINTCON 2025", 280, F2, (180, 220, 255, 255))
    return frame


def build():
    bg = load_rgb("sc25_bg.png")
    logo = load_rgb("sc25_logo.png")
    badge = load_rgb("badge_photo.png")
    map_small = load_rgb("map_small.png")

    print("Extracting Faction-Sc25-Spin frames...")
    spin_frames = extract_spin_frames()
    print(f"  {len(spin_frames)} frames @ {FPS}fps from spin reel")

    frames: list[Image.Image] = []
    # 0-5 splash, 5-11 menu, 11-25 faction spin, 25-30 map overview,
    # 30-48 node map, 48-56 wrench+nut, 56-60 finale
    for i in range(TOTAL):
        t = i / FPS
        if t < 5:
            fr = scene_splash(t, bg, logo)
        elif t < 11:
            fr = scene_menu(t - 5, bg)
        elif t < 25:
            fr = scene_faction_spin(t - 11, spin_frames)
        elif t < 30:
            fr = scene_map_pan(t - 25, map_small)
        elif t < 48:
            fr = scene_node_map(t - 30, duration=18.0)
        elif t < 56:
            fr = scene_wrench(t - 48, badge)
        else:
            fr = scene_finale(t - 56, bg, logo)
        frames.append(fr.convert("RGB"))

    print(f"Writing {OUT_GIF} ({len(frames)} frames @ {FPS}fps, {W}x{H})...")
    frames[0].save(
        OUT_GIF,
        save_all=True,
        append_images=frames[1:],
        duration=int(1000 / FPS),
        loop=0,
        optimize=False,
    )

    print(f"Encoding {OUT_MP4} at badge resolution {W}x{H}...")
    tmp = Path(tempfile.mkdtemp(prefix="attract25_"))
    try:
        for i, fr in enumerate(frames):
            fr.save(tmp / f"f{i:04d}.png")
        subprocess.check_call(
            [
                "ffmpeg",
                "-y",
                "-framerate",
                str(FPS),
                "-i",
                str(tmp / "f%04d.png"),
                "-c:v",
                "libx264",
                "-pix_fmt",
                "yuv420p",
                "-crf",
                "18",
                "-movflags",
                "+faststart",
                str(OUT_MP4),
            ]
        )
    finally:
        shutil.rmtree(tmp)

    print("Done:")
    for p in (OUT_MP4, OUT_GIF):
        print(f"  {p}  ({p.stat().st_size / 1024:.0f} KB)")


if __name__ == "__main__":
    build()
