"""Render the store/Workshop preview images into package/art/.

Usage: python make_preview.py
"""
import math
import os
import random

from PIL import Image, ImageDraw, ImageFilter, ImageFont

OUT = os.path.join(os.path.dirname(__file__), "..", "package", "art")
FONTS = r"C:\Windows\Fonts"


def font(names, size):
    for n in names:
        p = os.path.join(FONTS, n)
        if os.path.exists(p):
            return ImageFont.truetype(p, size)
    return ImageFont.load_default()


def background(w, h, seed=7):
    rnd = random.Random(seed)
    img = Image.new("RGB", (w, h), (6, 8, 18))
    glow = Image.new("RGB", (w, h), (0, 0, 0))
    g = ImageDraw.Draw(glow)
    # nebula blobs
    for cx, cy, r, col in [(0.72, 0.35, 0.45, (60, 30, 110)), (0.25, 0.75, 0.40, (20, 60, 110)),
                           (0.85, 0.85, 0.30, (110, 40, 40))]:
        g.ellipse([w * cx - w * r, h * cy - w * r * 0.6, w * cx + w * r, h * cy + w * r * 0.6], fill=col)
    glow = glow.filter(ImageFilter.GaussianBlur(w / 9))
    img = Image.blend(img, glow, 0.55)
    d = ImageDraw.Draw(img)
    for _ in range(int(w * h / 900)):
        x, y = rnd.uniform(0, w), rnd.uniform(0, h)
        b = int(rnd.uniform(90, 255))
        s = rnd.choice([0.6, 0.8, 1.0, 1.0, 1.6])
        d.ellipse([x - s, y - s, x + s, y + s], fill=(b, b, min(255, b + 20)))
    return img


def keycap(d, x, y, w, h, label, f):
    d.rounded_rectangle([x, y + 8, x + w, y + h + 8], radius=18, fill=(20, 22, 30))
    d.rounded_rectangle([x, y, x + w, y + h], radius=18, fill=(228, 230, 236), outline=(255, 255, 255), width=3)
    d.rounded_rectangle([x + 14, y + 10, x + w - 14, y + h - 18], radius=12, fill=(242, 243, 247))
    tw = d.textlength(label, font=f)
    d.text((x + (w - tw) / 2, y + h / 2 - f.size * 0.62), label, font=f, fill=(30, 32, 44))


def pause_icon(d, cx, cy, r):
    d.ellipse([cx - r, cy - r, cx + r, cy + r], outline=(255, 200, 70), width=max(3, r // 9))
    bw, bh = r * 0.22, r * 0.9
    for dx in (-r * 0.25, r * 0.25):
        d.rounded_rectangle([cx + dx - bw / 2, cy - bh / 2, cx + dx + bw / 2, cy + bh / 2], radius=bw / 3,
                            fill=(255, 200, 70))


def banner(w=1280, h=720):
    img = background(w, h)
    d = ImageDraw.Draw(img)
    title = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 86)
    sub = font(["segoeui.ttf", "arial.ttf"], 38)
    small = font(["segoeuisb.ttf", "segoeui.ttf", "arial.ttf"], 34)
    keyf = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 46)
    d.text((80, 78), "GALACTIC ACTIVE PAUSE", font=title, fill=(255, 255, 255))
    d.text((84, 190), "Galactic conquest mod for Empire at War: Forces of Corruption", font=sub, fill=(185, 200, 230))
    keycap(d, 80, 300, 520, 120, "PAUSE", keyf)
    pause_icon(d, 720, 360, 62)
    bullets = ["One-key pause, rebindable in Options",
               "Move fleets while paused",
               "Queue planet production while paused",
               "Shuttle troops between surface and orbit"]
    for i, t in enumerate(bullets):
        y = 480 + i * 52
        d.ellipse([88, y + 14, 102, y + 28], fill=(255, 200, 70))
        d.text((120, y), t, font=small, fill=(235, 238, 245))
    return img


def square(s=512):
    img = background(s, s, seed=11)
    d = ImageDraw.Draw(img)
    title = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 54)
    sub = font(["segoeuisb.ttf", "segoeui.ttf", "arial.ttf"], 26)
    keyf = font(["bahnschrift.ttf", "segoeuib.ttf", "arialbd.ttf"], 40)
    for i, line in enumerate(["GALACTIC", "ACTIVE PAUSE"]):
        tw = d.textlength(line, font=title)
        d.text(((s - tw) / 2, 48 + i * 64), line, font=title, fill=(255, 255, 255))
    keycap(d, 56, 220, 400, 100, "PAUSE", keyf)
    pause_icon(d, s / 2, 395, 40)
    line = "+ give orders while paused"
    tw = d.textlength(line, font=sub)
    d.text(((s - tw) / 2, 452), line, font=sub, fill=(255, 210, 110))
    return img


if __name__ == "__main__":
    os.makedirs(OUT, exist_ok=True)
    banner().save(os.path.join(OUT, "banner_1280x720.png"), optimize=True)
    square().save(os.path.join(OUT, "preview_512.png"), optimize=True)
    for n in os.listdir(OUT):
        print(n, os.path.getsize(os.path.join(OUT, n)))
