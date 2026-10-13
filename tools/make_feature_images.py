"""Render illustrated feature images (1280x720) into package/art/features/.

Usage: python make_feature_images.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter

from make_preview import FONTS, background, font, keycap, pause_icon  # noqa: F401

OUT = os.path.join(os.path.dirname(__file__), "..", "package", "art", "features")
W, H = 1280, 720
GOLD = (255, 200, 70)
WHITE = (240, 243, 250)
DIM = (170, 185, 215)
PANEL = (14, 18, 34)

TITLE = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 64)
SUB = font(["segoeui.ttf", "arial.ttf"], 32)
LABEL = font(["segoeuisb.ttf", "segoeui.ttf", "arial.ttf"], 26)
SMALL = font(["segoeui.ttf", "arial.ttf"], 22)
KEY = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 40)


def canvas(seed, title, subtitle):
    img = background(W, H, seed=seed)
    d = ImageDraw.Draw(img, "RGBA")
    d.text((70, 48), title, font=TITLE, fill=WHITE)
    d.text((72, 128), subtitle, font=SUB, fill=DIM)
    return img, d


def planet(img, cx, cy, r, base, name=None, owner=None):
    layer = Image.new("RGBA", img.size, (0, 0, 0, 0))
    g = ImageDraw.Draw(layer)
    g.ellipse([cx - r * 1.6, cy - r * 1.6, cx + r * 1.6, cy + r * 1.6], fill=base + (60,))
    layer = layer.filter(ImageFilter.GaussianBlur(r * 0.5))
    img.paste(Image.alpha_composite(img.convert("RGBA"), layer).convert("RGB"))
    d = ImageDraw.Draw(img, "RGBA")
    d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=base)
    hi = tuple(min(255, c + 70) for c in base)
    d.ellipse([cx - r * 0.75, cy - r * 0.8, cx + r * 0.2, cy + r * 0.1], fill=hi + (110,))
    shade = Image.new("L", img.size, 0)
    ImageDraw.Draw(shade).ellipse([cx - r * 0.1, cy - r * 0.1, cx + r * 1.15, cy + r * 1.15], fill=90)
    disk = Image.new("L", img.size, 0)
    ImageDraw.Draw(disk).ellipse([cx - r, cy - r, cx + r, cy + r], fill=255)
    from PIL import ImageChops
    img.paste((0, 0, 0), mask=ImageChops.multiply(shade.filter(ImageFilter.GaussianBlur(r * 0.25)), disk))
    d = ImageDraw.Draw(img, "RGBA")
    if owner:
        d.ellipse([cx - r - 7, cy - r - 7, cx + r + 7, cy + r + 7], outline=owner, width=4)
    if name:
        tw = d.textlength(name, font=LABEL)
        d.text((cx - tw / 2, cy + r + 14), name, font=LABEL, fill=WHITE)
    return d


def fleet_marker(d, x, y, color, count):
    pts = [(x, y - 18), (x + 26, y), (x, y + 18), (x + 8, y)]
    d.polygon(pts, fill=color)
    d.polygon([(p[0] - 14, p[1] + 10) for p in pts], fill=color + (150,) if len(color) == 3 else color)
    d.text((x + 34, y - 15), f"x{count}", font=LABEL, fill=WHITE)


def dashed(d, a, b, color, width=4, dash=18, gap=12):
    (x1, y1), (x2, y2) = a, b
    length = math.hypot(x2 - x1, y2 - y1)
    ux, uy = (x2 - x1) / length, (y2 - y1) / length
    pos = 0.0
    while pos < length:
        e = min(pos + dash, length)
        d.line([(x1 + ux * pos, y1 + uy * pos), (x1 + ux * e, y1 + uy * e)], fill=color, width=width)
        pos += dash + gap


def arrow_head(d, tip, direction, color, size=22):
    ang = math.atan2(direction[1], direction[0])
    pts = [tip,
           (tip[0] - size * math.cos(ang - 0.45), tip[1] - size * math.sin(ang - 0.45)),
           (tip[0] - size * math.cos(ang + 0.45), tip[1] - size * math.sin(ang + 0.45))]
    d.polygon(pts, fill=color)


def paused_badge(d, x, y):
    d.rounded_rectangle([x, y, x + 300, y + 64], radius=14, fill=(20, 16, 6, 220), outline=GOLD, width=3)
    pause_icon(d, x + 38, y + 32, 20)
    d.text((x + 72, y + 14), "GAME PAUSED", font=LABEL, fill=GOLD)


def footer(d, text):
    d.rounded_rectangle([70, H - 92, W - 70, H - 36], radius=14, fill=(255, 255, 255, 18))
    d.text((96, H - 80), text, font=SMALL, fill=WHITE)


def img_pause():
    img, d = canvas(21, "Pause with one key", "Press SPACE on the galactic map. Press it again to resume.")
    keycap(d, 120, 300, 480, 120, "SPACE", KEY)
    d.line([(615, 360), (690, 360)], fill=GOLD, width=6)
    arrow_head(d, (705, 360), (1, 0), GOLD, 26)
    # mock command bar time controls
    x0, y0 = 720, 270
    d.rounded_rectangle([x0, y0, x0 + 420, y0 + 190], radius=22, fill=PANEL + (235,), outline=(90, 110, 160), width=2)
    d.text((x0 + 28, y0 + 18), "Galactic time", font=LABEL, fill=DIM)
    pause_icon(d, x0 + 110, y0 + 115, 48)
    d.ellipse([x0 + 230 - 48, y0 + 115 - 48, x0 + 230 + 48, y0 + 115 + 48], outline=(110, 120, 150), width=4)
    d.polygon([(x0 + 215, y0 + 90), (x0 + 250, y0 + 115), (x0 + 215, y0 + 140)], fill=(110, 120, 150))
    d.polygon([(x0 + 300, y0 + 95), (x0 + 330, y0 + 115), (x0 + 300, y0 + 135)], fill=(110, 120, 150))
    d.polygon([(x0 + 330, y0 + 95), (x0 + 360, y0 + 115), (x0 + 330, y0 + 135)], fill=(110, 120, 150))
    paused_badge(d, 790, 500)
    footer(d, "The on-screen Pause/Play button stays in sync. Tactical battles keep SPACE as Camera Center.")
    return img


def img_fleets():
    img, d = canvas(33, "Move fleets while paused", "Plan your moves with the clock stopped. Fleets set off when you unpause.")
    d = planet(img, 260, 430, 70, (70, 120, 200), "Home world", owner=(80, 170, 255))
    d = planet(img, 1000, 340, 58, (190, 110, 70), "Frontier", owner=None)
    d = planet(img, 700, 500, 40, (110, 170, 110), "Outpost", owner=(80, 170, 255))
    dashed(d, (350, 410), (925, 350), GOLD, width=5)
    arrow_head(d, (930, 349), (575, -60), GOLD)
    dashed(d, (330, 460), (650, 492), (120, 190, 255), width=4)
    arrow_head(d, (655, 493), (320, 32), (120, 190, 255))
    fleet_marker(d, 360, 330, (120, 200, 255), 6)
    fleet_marker(d, 600, 360, GOLD, 4)
    paused_badge(d, 900, 180)
    footer(d, "Move orders, routes and destinations are accepted while you are paused. Nothing travels until time resumes.")
    return img


def img_production():
    img, d = canvas(47, "Queue production while paused", "Fill every build queue without losing a day.")
    d = planet(img, 230, 420, 110, (80, 140, 190), "Shipyard world", owner=(80, 170, 255))
    x0, y0 = 430, 230
    d.rounded_rectangle([x0, y0, x0 + 760, y0 + 330], radius=22, fill=PANEL + (235,), outline=(90, 110, 160), width=2)
    d.text((x0 + 28, y0 + 18), "Build queue", font=LABEL, fill=DIM)
    items = [("Frigate", 1.0, (120, 200, 255)), ("Frigate", 1.0, (120, 200, 255)), ("Cruiser", 1.0, GOLD),
             ("Infantry", 1.0, (140, 220, 140)), ("Defenses", 1.0, (220, 140, 140))]
    for i, (name, _, col) in enumerate(items):
        bx = x0 + 28 + i * 146
        by = y0 + 70
        d.rounded_rectangle([bx, by, bx + 130, by + 150], radius=14, fill=(255, 255, 255, 22), outline=col, width=3)
        # simple ship/unit glyph
        cx, cy = bx + 65, by + 62
        if name == "Infantry":
            for k in range(3):
                d.ellipse([cx - 32 + k * 24, cy - 26, cx - 16 + k * 24, cy - 10], fill=col)
                d.rounded_rectangle([cx - 34 + k * 24, cy - 6, cx - 14 + k * 24, cy + 26], radius=6, fill=col)
        elif name == "Defenses":
            d.ellipse([cx - 30, cy - 30, cx + 30, cy + 30], outline=col, width=6)
            d.ellipse([cx - 10, cy - 10, cx + 10, cy + 10], fill=col)
        else:
            s = 1.3 if name == "Cruiser" else 1.0
            d.polygon([(cx - 40 * s, cy + 14 * s), (cx + 40 * s, cy), (cx - 40 * s, cy - 14 * s), (cx - 26 * s, cy)], fill=col)
        tw = d.textlength(name, font=SMALL)
        d.text((cx - tw / 2, by + 112), name, font=SMALL, fill=WHITE)
        d.text((bx + 8, by + 6), str(i + 1), font=SMALL, fill=DIM)
    d.rounded_rectangle([x0 + 28, y0 + 245, x0 + 732, y0 + 265], radius=10, fill=(255, 255, 255, 30))
    d.rounded_rectangle([x0 + 28, y0 + 245, x0 + 180, y0 + 265], radius=10, fill=GOLD + (200,))
    d.text((x0 + 28, y0 + 278), "Progress resumes when you unpause", font=SMALL, fill=DIM)
    paused_badge(d, 890, 120)
    footer(d, "Production orders go through while paused. Build time only counts while the galaxy is running.")
    return img


def img_orbit():
    img, d = canvas(59, "Surface and orbit transfers while paused", "Shuttle troops and heroes between the ground and your fleet.")
    cx, cy = 430, 430
    d = planet(img, cx, cy, 130, (150, 120, 80), None, owner=(80, 170, 255))
    d.ellipse([cx - 260, cy - 70, cx + 260, cy + 70], outline=(120, 190, 255, 160), width=3)
    fleet_marker(d, cx + 170, cy - 95, (120, 200, 255), 3)
    # ground units
    for k in range(3):
        gx = cx - 50 + k * 40
        d.ellipse([gx - 10, cy + 40, gx + 10, cy + 60], fill=(140, 220, 140))
        d.rounded_rectangle([gx - 12, cy + 64, gx + 12, cy + 100], radius=6, fill=(140, 220, 140))
    # up / down arrows
    d.line([(cx + 70, cy + 20), (cx + 150, cy - 70)], fill=GOLD, width=6)
    arrow_head(d, (cx + 155, cy - 76), (80, -90), GOLD, 26)
    d.line([(cx + 215, cy - 60), (cx + 140, cy + 30)], fill=(120, 200, 255), width=6)
    arrow_head(d, (cx + 135, cy + 36), (-75, 90), (120, 200, 255), 26)
    x0, y0 = 780, 250
    d.rounded_rectangle([x0, y0, x0 + 420, y0 + 230], radius=22, fill=PANEL + (235,), outline=(90, 110, 160), width=2)
    rows = [("Orbit", "Fleet  x3", (120, 200, 255)), ("Surface", "Troops  x3", (140, 220, 140))]
    for i, (where, what, col) in enumerate(rows):
        ry = y0 + 30 + i * 95
        d.text((x0 + 28, ry), where, font=LABEL, fill=DIM)
        d.rounded_rectangle([x0 + 150, ry - 4, x0 + 392, ry + 50], radius=12, fill=(255, 255, 255, 22), outline=col, width=3)
        d.text((x0 + 170, ry + 8), what, font=LABEL, fill=WHITE)
    paused_badge(d, 900, 520)
    footer(d, "Launch troops into orbit or land them on your own planets while the galactic clock is stopped.")
    return img


def img_install():
    img, d = canvas(71, "Easy install, easy removal", "No game files are changed. Works alongside Workshop mods like AotR.")
    steps = [("1", "Extract the zip", "or subscribe on the\nSteam Workshop"),
             ("2", "Run Install.bat", "finds your Steam\ngame folder for you"),
             ("3", "Play", "press SPACE on the\ngalactic map")]
    for i, (n, head, body) in enumerate(steps):
        x = 90 + i * 380
        d.rounded_rectangle([x, 240, x + 340, 540], radius=24, fill=PANEL + (235,), outline=(90, 110, 160), width=2)
        d.ellipse([x + 30, 268, x + 100, 338], fill=GOLD)
        tw = d.textlength(n, font=KEY)
        d.text((x + 65 - tw / 2, 278), n, font=KEY, fill=(30, 32, 44))
        d.text((x + 30, 360), head, font=font(["segoeuisb.ttf", "segoeui.ttf"], 34), fill=WHITE)
        d.multiline_text((x + 30, 416), body, font=SMALL, fill=DIM, spacing=8)
        if i < 2:
            arrow_head(d, (x + 372, 390), (1, 0), GOLD, 24)
    footer(d, "Uninstall.bat removes it. Open source (MIT) on GitHub. Turns itself off safely if the game is updated.")
    return img


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    for name, fn in [("1_pause.png", img_pause), ("2_fleets.png", img_fleets), ("3_production.png", img_production),
                     ("4_orbit.png", img_orbit), ("5_install.png", img_install)]:
        im = fn()
        if im.mode != "RGB":
            im = im.convert("RGB")
        path = os.path.join(OUT, name)
        im.save(path, optimize=True)
        print(name, os.path.getsize(path))
