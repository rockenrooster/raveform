"""RAVEFORM engine acceptance scenes (creative tooling, not game code).

The look-dev canvas records everything it draws as flat engine primitives. This tool writes them as
`.rfscene` files the engine replays, and renders next to each one the picture a correct engine must
produce from it (`design/previews/<name>_ref.png`). Any difference between an engine screenshot and
its reference is then the renderer's, never the content's. Format and rules: design/look.md §9.

Usage:
  python design/tools/scenes.py                      -> design/scenes/*.rfscene, *.probes.json, previews/*_ref.png
  python design/tools/scenes.py compare <engine.png> <scene> [--nobloom] [--crop x,y,w,h]
                                                     -> numbers, and <engine>.cmp.png for looking at
  python design/tools/scenes.py light <png> ...      -> dark share and mean luminance (design/scenes.md §5)
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import icons as ic  # noqa: E402
import frame_mock as fm  # noqa: E402

DESIGN = fm.DESIGN
SCENES = os.path.join(DESIGN, "scenes")
PREVIEWS = os.path.join(DESIGN, "previews")
W, H = fm.W, fm.H
PI = math.pi
LOOK_BLOOM = 0.95      # the reference look (design/look.md §3)


# --------------------------------------------------------------------------- the look card

def look_card():
    """A calibration chart: every light level, line weight, colour rule and layer of the look, laid
    out so each can be measured. Returns (canvas, probes)."""
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    pal = next(p for p in pals["palettes"] if p["id"] == "reef")
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    sw = ic.swatches()
    rng = random.Random(9)

    def lin(e):
        return np.asarray(e["linear"], np.float32)

    white = np.ones(3, np.float32)
    cands = [pal["enemyCandidates"][i] for i in pal["soloEnemies"]]
    hue = [lin(c) for c in cands]
    mate = [fm.partner_of(c) for c in cands]
    p1 = lin(pals["constants"]["players"][0])
    danger = lin(pals["constants"]["danger"])
    gold = lin(pals["constants"]["pickup"])
    accent = lin(pal["accent"])
    grid_c, grid_h = lin(pal["grid"]), lin(pal["gridHot"])
    R = 14.0 * fm.WU

    cv = ld.Canvas(W, H)
    cv.radial_bg(lin(pal["bg0"]) * 0.5, lin(pal["bg1"]) * 0.5, cx=0.5, cy=0.5)
    probes = []

    def probe(name, x, y, half=6):
        probes.append(dict(name=name, x=int(x - half), y=int(y - half), w=2 * half, h=2 * half))

    def label(s, x, y, cap=9, level=0.55):
        cv.text(font, s, x, y, cap, white, level)

    def unit(gid):
        return [(s[0], -s[1], s[2], -s[3]) for s in glyphs[gid]["segments"]]

    # ---- A. tone ramp: flat discs from 1/8 to 32 in four colours (tone map, white bleed, bloom spill)
    levels = [0.125, 0.25, 0.5, 1, 2, 4, 8, 16, 32]
    for k, lv in enumerate(levels):
        label("%g" % lv, 290 + k * 76 - 16, 40)
    for r, (name, colour) in enumerate((("WHITE", white), ("P1", p1), ("DANGER", danger), ("ENEMY", hue[0]))):
        cy = 96 + r * 70
        label(name, 150, cy - 5, 10)
        for k, lv in enumerate(levels):
            cx = 290 + k * 76
            cv.discs([(cx, cy, 26)], colour, lv)
            probe("tone_%s_%g" % (name.lower(), lv), cx, cy)

    # ---- B. line weights at the four working light levels, axis-aligned and at 7 degrees
    widths = [0.8, 1.1, 1.3, 1.45, 1.7, 2.0, 2.3, 3.0]
    for k, wd in enumerate(widths):
        label("%g" % wd, 300 + k * 84, 384)
    styles = (("GRID .30", grid_c, 0.30), ("WHITE 1", white, 1.0), ("HULL 2", hue[0], 2.0), ("SHOT 4.5", p1 * 0.45 + white * 0.55, 4.5))
    for r, (name, colour, lv) in enumerate(styles):
        y = 424 + r * 46
        label(name, 150, y - 4)
        for k, wd in enumerate(widths):
            x = 300 + k * 84
            cv.lines([(x, y - 8, x + 60, y - 8)], colour, lv, wd)
            cv.lines([(x, y + 13.5, x + 60, y + 13.5 - 60 * math.tan(math.radians(7.0)))], colour, lv, wd)

    # ---- C. glyphs: the colour rule at 2.5x and at game size, then a tier ladder
    cast = (("drifter_3", 0, 0.10, False, 0.0, None), ("hound_3", 1, 0.0, False, 0.0, None),
            ("ram_4", 1, 0.22, True, 0.07, "mark_c"), ("blink_3", 2, 0.12, False, 0.0, None),
            ("sentry_3", 2, 0.14, False, 0.0, "mark_b_spin"), ("moth_3", 1, 0.08, False, 0.0, None),
            ("carom_2", 2, 0.0, False, 0.0, None), ("wyrm_head_3", 1, 0.10, False, 0.0, None))
    label("GLYPHS  X2.5 AND GAME SIZE", 1040, 40)
    for k, (gid, h_, aura, core, glass, mark) in enumerate(cast):
        x = 1100 + k * 160
        rot = -0.35 if glyphs[gid]["facesHeading"] else 0.3
        for y, radius, width in ((160, R * 2.5, 2.4), (300, R, 1.8)):
            fm.draw_glyph(cv, glyphs[gid], x, y, radius, rot, hue[h_], mate[h_], accent, 2.2 if not core else 2.8, width, aura, core, glass)
            if mark:
                cv.lines(ld.transform(unit(mark), x, y, radius, rot), white, 1.8, 1.5)
        if gid == "blink_3":
            cv.flare(x, 160, R * 2.5 * 2.2, mate[h_], 0.9, 1.1, 2, PI / 4)
            cv.flare(x, 300, R * 2.2, mate[h_], 0.9, 1.1, 2, PI / 4)
    label("TIERS 1-5", 1040, 352)
    for t in range(5):
        fm.draw_glyph(cv, glyphs["drifter_%d" % (t + 1)], 1100 + t * 64, 400, R, 0.3, hue[0], mate[0], accent, 1.8 + 0.2 * t, 1.8, 0.10 if t >= 2 else 0.0)
        fm.draw_glyph(cv, glyphs["hound_%d" % (t + 1)], 1480 + t * 64, 400, R, -0.35, hue[1], mate[1], accent, 1.8 + 0.2 * t)
    for t in range(3):
        fm.draw_glyph(cv, glyphs["shoal_%d" % (t + 1)], 1860 + t * 48, 400, R * 0.55, -0.35, hue[0], mate[0], accent, 2.0, 1.5)

    # ---- D. the player: two-tone hull, halo, trail, shots, Orbitals
    label("PLAYER", 1040, 470)
    for x, y, radius in ((1110, 590, R * 2.5), (1320, 590, R)):
        k = radius / R
        aim = math.radians(-24.0)
        cv.glow(x, y, 150 * min(k, 1.4), p1, 0.13)
        hull = ld.transform(unit("ship"), x, y, radius, aim)
        cv.lines(hull, ld.f_radial(x, y, radius * 1.3, [white, p1, sw["blue"]]), 2.8, 2.3 if k == 1 else 3.0)
        cv.lines(hull, white, 2.0, 0.8 if k == 1 else 1.1)
        cv.lines(ld.transform(unit("halo_3"), x, y, radius, 0.35), ld.f_angular(x, y, [p1, white, p1, sw["sky"]]), 1.5, 1.6)
        bx, by = -math.cos(aim + 0.5), -math.sin(aim + 0.5)
        trail = [(x + bx * (18 + 10 * j) * k, y + by * (18 + 10 * j) * k, x + bx * (28 + 10 * j) * k, y + by * (28 + 10 * j) * k) for j in range(13)]
        cv.lines(trail, ld.f_fade(ld.f_linear(x, y, x + bx * 150 * k, y + by * 150 * k, [white, p1, sw["blue"]]), x + bx * 18 * k, y + by * 18 * k, 150 * k, 1.1), 2.4, 2.6)
        if k == 1:
            for lane in (-0.16, 0.0, 0.16):
                for j in range(3):
                    d = 44 + j * 88
                    a = aim + lane
                    sx, sy = x + math.cos(a) * d, y + math.sin(a) * d
                    hx, hy = sx + math.cos(a) * 24, sy + math.sin(a) * 24
                    cv.lines([(sx, sy, hx, hy)], ld.f_linear(sx, sy, hx, hy, [p1, white]), 4.6, 2.2)
                    cv.glow(hx, hy, 9, white, 0.7)
            for j in range(3):
                a = 0.8 + j * 2 * PI / 3
                ox, oy = x + math.cos(a) * 62, y + math.sin(a) * 62
                cv.lines(ld.transform(ld.arc(1.0, a - 0.9, a, 8), x, y, 62), ld.f_fade(ld.f_solid(p1), ox, oy, 60, 1.0), 2.0, 2.0)
                cv.lines(ld.transform(unit("shard"), ox, oy, 8.5, a), white * 0.5 + p1 * 0.5, 3.4, 1.7)
                cv.glow(ox, oy, 14, p1, 0.7)

    # ---- E. soft light: pools and sparkles at their working sizes, embers, Shards, star dust
    label("POOLS 16 50 150 270", 150, 612)
    for name, x, y, radius, colour, lv in (("pool_16", 200, 668, 16, white, 0.7), ("pool_50", 300, 690, 50, white, 0.5),
                                           ("pool_150", 480, 770, 150, p1, 0.13), ("pool_270", 780, 770, 270, hue[0], 0.08)):
        cv.glow(x, y, radius, colour, lv)
        probe(name, x, y)
    for _ in range(10):
        a, d = rng.uniform(0, 2 * PI), rng.uniform(4, 46)
        cv.glow(200 + math.cos(a) * d, 820 + math.sin(a) * d, 4.0, mate[0], 1.2)
    for j in range(6):
        x, y = 290 + j * 26, 870 + 10 * math.sin(j * 1.7)
        cv.lines(ld.transform(unit("shard"), x, y, 5.2, 0.5 * j), gold * 0.6 + white * 0.4, 3.2, 1.3)
        cv.glow(x, y, 9.0, gold, 0.5)
    for _ in range(70):
        cv.glow(rng.uniform(150, 950), rng.uniform(900, 950), rng.choice([1.6, 1.6, 2.2, 3.0]),
                rng.choice([white, white, white, hue[0], hue[2], mate[1]]), rng.uniform(0.15, 0.7))
    probe("haze_dark", 2330, 1040)
    probe("haze_centre", 1200, 700)

    # ---- F. one large kill, frozen at a third of its life
    bx, by, age = 1800.0, 600.0, 0.35
    label("KILL AND THREAT LAYER", 1600, 470)
    cv.glow(bx, by, 150 + 120 * age, hue[0], 0.22 * (1.2 - age))
    cv.glow(bx, by, 50, white, 0.5 * (1.1 - age))
    for _ in range(int(86 * (1.2 - 0.5 * age))):
        a = rng.uniform(0, 2 * PI)
        ra = rng.uniform(20 + 90 * age, 70 + 160 * age)
        ln_ = rng.uniform(10, 40)
        u, v = rng.random(), rng.random() * 0.4
        colour = (hue[0] * (1 - u) + mate[0] * u) * (1 - v) + white * v
        seg = (bx + math.cos(a) * ra, by + math.sin(a) * ra, bx + math.cos(a) * (ra + ln_), by + math.sin(a) * (ra + ln_))
        cv.lines([seg], ld.f_linear(seg[0], seg[1], seg[2], seg[3], [colour * 0.5, colour * 0.6 + white * 0.4]), rng.choice([2.4, 3.4, 4.6, 5.6]), 1.4)
    for _ in range(10):
        a, d = rng.uniform(0, 2 * PI), rng.uniform(10, 60 + 150 * age)
        cv.glow(bx + math.cos(a) * d, by + math.sin(a) * d, 4.0, mate[0], 1.2)
    cv.lines(ld.transform(ld.polygon(56), bx, by, 60 + 150 * age), ld.f_angular(bx, by, [white, hue[0], white, mate[0]]), 1.3, 1.6)
    for s in glyphs["drifter_5"]["segments"]:
        mx, my = (s[0] + s[2]) * 0.5, -(s[1] + s[3]) * 0.5
        out = 2.6 + rng.random()
        seg = ld.transform([(s[0] - mx, -s[1] - my, s[2] - mx, -s[3] - my)], bx + mx * R * out, by + my * R * out, R, rng.uniform(-0.9, 0.9))
        cv.lines(seg, hue[0] * 0.5 + white * 0.5, 2.8, 1.7)
    probe("kill_core", bx, by)

    # ---- G. danger over the brightest thing on screen: it must still read
    dias = []
    for j in range(14):
        a = j * 2 * PI / 14
        dias += ld.transform(ld.polygon(4), bx + math.cos(a) * 96, by + math.sin(a) * 96, 7.5)
        cv.glow(bx + math.cos(a) * 96, by + math.sin(a) * 96, 13, danger, 0.35)
    for j in range(5):
        dias += ld.transform(ld.polygon(4), bx - 44 + j * 22, by + 4 * math.sin(j), 7.5)
    cv.lines(dias, danger, 2.4, 2.2)
    cv.threat(dias, danger + white * 0.25, 2.1, 1.5)
    tele = []
    rot = math.atan2(430 - 770, 2060 - 1560)
    for j in range(1, 9):
        tele += ld.transform(ld.polyline([(-0.5, 0.7), (0.5, 0.0), (-0.5, -0.7)]), 1560 + j * 62 * math.cos(rot), 770 + j * 62 * math.sin(rot), 13, rot)
    cv.lines(tele, danger, 1.3, 1.8)
    cv.threat(tele, danger * 0.9, 1.6, 1.0)

    # ---- H. text at the HUD's sizes
    cv.lines(ld.text_segments(font, "RAVEFORM 0123456789", 1640, 836, 34)[0], ld.f_linear(1640, 836, 1640, 870, [white, white, accent]), 1.6, 3.1)
    cv.lines(ld.text_segments(font, "X412  CHAIN  DROP IN 4.5", 1640, 890, 22)[0], ld.f_linear(1640, 890, 1640, 912, [white, gold, sw["amber"]]), 2.0, 2.0)
    cv.text(font, "THE QUICK BROWN FOX JUMPS OVER 13 LAZY DOGS", 1640, 930, 14, white, 1.2)
    cv.text(font, "TRACK 03  132 BPM  REEF  .,:;!?+-/()%", 1640, 960, 12, accent, 1.0, 1.5)
    cv.text(font, "SMALLEST HUD TEXT 0123456789", 1640, 986, 11, sw["amber"], 1.0, 1.4)

    # ---- I. lattice: at rest, warm and hot around one shock front, tinted by the player's light
    gx0, gy0, gx1, gy1 = 1040, 740, 1560, 1040
    boom = ((gx0 + gx1) / 2 + 40, (gy0 + gy1) / 2, 0.5)

    def warp(x, y):
        dx, dy = x - boom[0], y - boom[1]
        d = math.hypot(dx, dy) + 1e-3
        push = 34.0 * (1.0 - 0.5 * boom[2]) * math.exp(-((d - (60.0 + 150.0 * boom[2])) / 60.0) ** 2)
        return x + dx / d * push, y + dy / d * push, push

    cold, warm, hot = [], [], []

    def bucket(a, b):
        heat = (a[2] + b[2]) * 0.5
        (hot if heat > 20.0 else warm if heat > 8.0 else cold).append((a[0], a[1], b[0], b[1]))

    for gx in range(gx0, gx1 + 1, 30):
        prev = None
        for gy in range(gy0, gy1 + 1, 15):
            cur = warp(gx, gy)
            if prev:
                bucket(prev, cur)
            prev = cur
    for gy in range(gy0, gy1 + 1, 30):
        prev = None
        for gx in range(gx0, gx1 + 1, 15):
            cur = warp(gx, gy)
            if prev:
                bucket(prev, cur)
            prev = cur
    cv.lines(cold, ld.f_radial(gx0, gy1, 700.0, [grid_c * 0.55 + p1 * 0.45, grid_c, grid_c * 0.7 + hue[2] * 0.3]), 0.30, 1.1)
    cv.lines(warm, grid_c * 0.6 + grid_h * 0.4, 0.42, 1.15)
    cv.lines(hot, grid_h, 0.85, 1.3)
    label("LATTICE", gx0, 716)

    # ---- J. every palette's line colours under the same light: grid, hot grid, accent, hues and partners
    for i, p in enumerate(pals["palettes"]):
        x0 = 150 + i * 136
        label(p["name"].upper(), x0, 1046, 8)
        bars = [(lin(p["grid"]), 0.6), (lin(p["gridHot"]), 0.85), (lin(p["accent"]), 1.6)]
        for j in p["soloEnemies"]:
            bars += [(lin(p["enemyCandidates"][j]), 2.0), (fm.partner_of(p["enemyCandidates"][j]), 2.0)]
        for j, (colour, lv) in enumerate(bars):
            cv.lines([(x0 + 4 + j * 13, 986, x0 + 4 + j * 13, 1030)], colour, lv, 4.0)
    return cv, probes


# --------------------------------------------------------------------------- export

def export(name, cv, probes=None, nobloom=False):
    os.makedirs(SCENES, exist_ok=True)
    counts = cv.dump(os.path.join(SCENES, name + ".rfscene"), bloom=LOOK_BLOOM)
    hdr, rims, cores = cv.engine_hdr()
    cv._composite(hdr, rims, cores, bloom=LOOK_BLOOM).save(os.path.join(PREVIEWS, name + "_ref.png"), optimize=True)
    if nobloom:
        cv._composite(hdr, rims, cores, bloom=0.0).save(os.path.join(PREVIEWS, name + "_nobloom_ref.png"), optimize=True)
    if probes:
        with open(os.path.join(SCENES, name + ".probes.json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(probes, f, indent=1)
    size = os.path.getsize(os.path.join(SCENES, name + ".rfscene"))
    print("%-11s %s  (%d KB)" % (name, " ".join("%s=%d" % kv for kv in sorted(counts.items())), size // 1024))


# --------------------------------------------------------------------------- comparison

def _decode(v):
    v = v / 255.0
    return np.where(v <= 0.04045, v / 12.92, ((v + 0.055) / 1.055) ** 2.4)


def compare(engine_png, name, nobloom=False, crop=None):
    ref = np.asarray(Image.open(os.path.join(PREVIEWS, name + ("_nobloom_ref.png" if nobloom else "_ref.png"))).convert("RGB"), np.float32)
    eng = np.asarray(Image.open(engine_png).convert("RGB"), np.float32)
    if eng.shape != ref.shape:
        print("note: engine image is %dx%d, reference %dx%d - comparing the common centre" % (eng.shape[1], eng.shape[0], ref.shape[1], ref.shape[0]))
        h, w = min(eng.shape[0], ref.shape[0]), min(eng.shape[1], ref.shape[1])

        def centre(a):
            y0, x0 = (a.shape[0] - h) // 2, (a.shape[1] - w) // 2
            return a[y0:y0 + h, x0:x0 + w]
        eng, ref = centre(eng), centre(ref)
    d = eng - ref
    ad = np.abs(d)
    lum = np.array([0.2126, 0.7152, 0.0722], np.float32)
    le, lr = (_decode(eng) * lum).sum(-1), (_decode(ref) * lum).sum(-1)
    print("mean abs error %.2f /255   bias R %+.2f G %+.2f B %+.2f   p95 %.0f  p99 %.0f  max %.0f" % (
        ad.mean(), d[..., 0].mean(), d[..., 1].mean(), d[..., 2].mean(), np.percentile(ad, 95), np.percentile(ad, 99), ad.max()))
    print("linear luminance: engine %.5f  reference %.5f  ratio %.3f" % (le.mean(), lr.mean(), le.mean() / max(lr.mean(), 1e-9)))
    # soft light only (16 px block means): bloom, pools and background, without anti-aliasing noise
    bh, bw = eng.shape[0] // 16, eng.shape[1] // 16

    def blocks(a):
        return a[:bh * 16, :bw * 16].reshape(bh, 16, bw, 16).mean(axis=(1, 3))
    be, br = blocks(le), blocks(lr)
    print("soft light (16 px blocks): mean abs %.5f  ratio p10 %.2f  p50 %.2f  p90 %.2f" % (
        np.abs(be - br).mean(), *np.percentile((be + 1e-4) / (br + 1e-4), [10, 50, 90])))
    probes_path = os.path.join(SCENES, name + ".probes.json")
    if os.path.exists(probes_path) and eng.shape[1] == W and eng.shape[0] == H:
        print("%-22s %-16s %-16s %s" % ("probe", "engine sRGB", "reference sRGB", "delta"))
        for p in json.load(open(probes_path, encoding="utf-8")):
            a = eng[p["y"]:p["y"] + p["h"], p["x"]:p["x"] + p["w"]].mean(axis=(0, 1))
            b = ref[p["y"]:p["y"] + p["h"], p["x"]:p["x"] + p["w"]].mean(axis=(0, 1))
            worst = np.abs(a - b).max()
            print("%-22s %-16s %-16s %+.0f %+.0f %+.0f %s" % (p["name"], "%3.0f %3.0f %3.0f" % tuple(a), "%3.0f %3.0f %3.0f" % tuple(b),
                                                           a[0] - b[0], a[1] - b[1], a[2] - b[2], "  <<<" if worst > 12 else ""))
    out = os.path.splitext(engine_png)[0] + ".cmp.png"
    if crop:
        x, y, w, h = crop
        tiles = [eng[y:y + h, x:x + w], ref[y:y + h, x:x + w], np.clip(ad[y:y + h, x:x + w] * 4.0, 0, 255)]
        sheet = np.concatenate(tiles, axis=1)
    else:
        sheet = np.concatenate([eng, ref, np.clip(ad * 4.0, 0, 255)], axis=0)
    img = Image.fromarray(sheet.astype(np.uint8), "RGB")
    if not crop and img.width > 1600:
        img = img.resize((1600, round(img.height * 1600 / img.width)), Image.LANCZOS)
    img.save(out, optimize=True)
    print("wrote %s  (%s: engine, reference, difference x4)" % (out, "left to right" if crop else "top to bottom"))


def light_numbers(image):
    """How much of a picture is dark and how bright it is overall (design/scenes.md §5).
    Returns (share of pixels whose brightest channel is under 48/255, the same under 96, the share at
    200 and above, mean linear luminance)."""
    a = np.asarray(image.convert("RGB"), np.float32)
    peak = a.max(axis=2)
    lum = (_decode(a) * np.array([0.2126, 0.7152, 0.0722], np.float32)).sum(-1)
    return float((peak < 48).mean()), float((peak < 96).mean()), float((peak >= 200).mean()), float(lum.mean())


def light(paths):
    print("%-44s %8s %8s %8s %10s" % ("picture", "dark<48", "dark<96", ">=200", "luminance"))
    for path in paths:
        d48, d96, hot, lum = light_numbers(Image.open(path))
        print("%-44s %7.1f%% %7.1f%% %7.1f%% %10.4f" % (os.path.basename(path), d48 * 100, d96 * 100, hot * 100, lum))


def main():
    if len(sys.argv) > 1 and sys.argv[1] == "light":
        light(sys.argv[2:])
        return
    if len(sys.argv) > 1 and sys.argv[1] == "compare":
        args = [a for a in sys.argv[2:] if not a.startswith("--")]
        crop = None
        if "--crop" in sys.argv:
            crop = [int(v) for v in sys.argv[sys.argv.index("--crop") + 1].split(",")]
            args = [a for a in args if a != sys.argv[sys.argv.index("--crop") + 1]]
        compare(args[0], args[1], "--nobloom" in sys.argv, crop)
        return
    export("frame_reef", fm.build("reef"))
    card, probes = look_card()
    export("look_card", card, probes, nobloom=True)
    card.render(bloom=LOOK_BLOOM).save(os.path.join(PREVIEWS, "look_card.png"), optimize=True)


if __name__ == "__main__":
    main()
