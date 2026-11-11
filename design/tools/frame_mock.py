"""RAVEFORM target frame v2 (the spectacle pass): a full 2400x1080 mock of minute ten.

This is the look target for milestone M1 and the HUD layout reference for M3. It is assembled from the
real data files (palettes.json, glyphs.json, icons.json recipes, vector_font.txt) with the look-dev
renderer. Everything uses only what the engine has: gradient capsule lines, glow sprites, faint fills,
flares, bloom.

Usage:  python design/tools/frame_mock.py [palette_id]   -> design/previews/frame_<palette>.png
"""
import json
import math
import os
import random
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import icons as ic  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")
W, H = 2400, 1080
WU = 1.5            # pixels per world unit at this resolution (view is 1600 x 720 wu)
PI = math.pi
DANGER_HUE, P1_HUE = 18.0, 200.0


def partner_of(entry):
    """The second colour of an enemy's gradient in a solo run (palettes.json, partnerRule)."""
    return np.asarray(entry["partners"][entry["soloPartner"]]["linear"], np.float32)


def draw_glyph(cv, g, x, y, radius, rot, base, part, accent, intensity=2.0, width=1.8, aura=0.0, core=False, glass=0.0):
    """The enemy colour rule (design/look.md §2). Hull: a gradient from the hue to its partner, nose to
    tail on shapes that face their heading, centre to rim on shapes that spin. Ornament: the palette
    accent mixed with the partner. Core mark: white. `core` adds the elite's thin white hull copy."""
    white = np.ones(3, np.float32)
    layers = {0: [], 1: [], 2: []}
    for s in g["segments"]:
        layers[s[4]].append((s[0], -s[1], s[2], -s[3]))
    ext = radius * g["extent"]
    if aura > 0:
        cv.glow(x, y, ext * 2.3, base, aura)
    hull = ld.transform(layers[0], x, y, radius, rot)
    if g["facesHeading"]:
        c, s_ = math.cos(rot), math.sin(rot)
        field = ld.f_linear(x + c * ext, y + s_ * ext, x - c * ext, y - s_ * ext, [base * 0.6 + white * 0.4, base, part])
    else:
        field = ld.f_radial(x, y, ext, [part, base, base * 0.75 + white * 0.25])
    if glass > 0:                                          # fill the first closed loop of the hull
        pts = []
        for sg in hull:
            pts.append((sg[0], sg[1]))
            if len(pts) > 2 and abs(sg[2] - hull[0][0]) < 1e-3 and abs(sg[3] - hull[0][1]) < 1e-3:
                break
        cv.fill(pts, base, glass)
    cv.lines(hull, field, intensity, width)
    if layers[1]:
        cv.lines(ld.transform(layers[1], x, y, radius, rot), accent * 0.6 + part * 0.4, intensity * 0.8, width * 0.85)
    if layers[2]:
        cv.lines(ld.transform(layers[2], x, y, radius, rot), white, intensity * 1.1, width * 0.85)
    if core:
        cv.lines(hull, white, 1.0, 0.9)


BLOOM = 0.95        # the target frame's bloom strength


def build(pal_id="reef"):
    """Compose the target frame on a look-dev canvas and return the canvas."""
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    pal = next(p for p in pals["palettes"] if p["id"] == pal_id)
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    sw = ic.swatches()
    icon_by_id = {i[0]: i for i in ic.ICONS}
    rng = random.Random(4)

    def lin(e):
        return np.asarray(e["linear"], np.float32)

    white = np.ones(3, np.float32)
    cands = [pal["enemyCandidates"][i] for i in pal["soloEnemies"]]
    hue = [lin(c) for c in cands]
    mate = [partner_of(c) for c in cands]
    p1 = lin(pals["constants"]["players"][0])
    danger = lin(pals["constants"]["danger"])
    gold = lin(pals["constants"]["pickup"])
    accent = lin(pal["accent"])
    grid_c, grid_h = lin(pal["grid"]), lin(pal["gridHot"])

    cv = ld.Canvas(W, H)
    cv.radial_bg(lin(pal["bg0"]) * 0.5, lin(pal["bg1"]) * 0.5, cx=0.42, cy=0.52)
    # nebula: soft colour clouds in the palette's own hues, far below the scene's brightness
    cv.nebula([(420, 300, 620, hue[2], 0.030), (1900, 260, 700, hue[0], 0.026), (1250, 900, 760, mate[1], 0.022),
               (2200, 900, 520, hue[2], 0.024), (120, 950, 520, hue[0], 0.020), (1100, 120, 520, accent, 0.016)])
    for _ in range(420):                                       # star dust, a few of them coloured
        x, y = rng.uniform(0, W), rng.uniform(0, H)
        c = rng.choice([white, white, white, hue[0], hue[2], mate[1]])
        cv.glow(x, y, rng.choice([1.6, 1.6, 2.2, 3.0]), c, rng.uniform(0.15, 0.7))

    ship = (980.0, 570.0)
    aim = math.radians(-24.0)
    booms = [(1500.0, 330.0, 1.0, 0), (1330.0, 690.0, 0.35, 1), (640.0, 300.0, 0.75, 2)]
    well = (700.0, 860.0)

    # ---------------------------------------------------------------- grid
    def warp(x, y):
        amount = 0.0
        for bx, by, age, _ in booms:
            dx, dy = x - bx, y - by
            d = math.hypot(dx, dy) + 1e-3
            radius = 60.0 + 150.0 * age
            push = 34.0 * (1.0 - 0.5 * age) * math.exp(-((d - radius) / 60.0) ** 2)
            x, y = x + dx / d * push, y + dy / d * push
            amount += push
        dx, dy = x - well[0], y - well[1]
        d = math.hypot(dx, dy) + 1e-3
        pull = min(d * 0.7, 1500.0 / (d + 20.0))
        x, y = x - dx / d * pull, y - dy / d * pull
        amount += max(0.0, pull - 6.0)
        dx, dy = x - ship[0], y - ship[1]
        d = math.hypot(dx, dy) + 1e-3
        ripple = 7.0 * math.exp(-((d - 250.0) / 40.0) ** 2)      # the kick's ring around the player
        return x + dx / d * ripple, y + dy / d * ripple, amount + ripple

    cold, warm, hot = [], [], []
    step, sub = 30, 15

    def bucket(a, b):
        heat = (a[2] + b[2]) * 0.5
        (hot if heat > 20.0 else warm if heat > 8.0 else cold).append((a[0], a[1], b[0], b[1]))

    for gx in range(0, W + 1, step):
        prev = None
        for gy in range(-sub, H + sub + 1, sub):
            cur = warp(gx, gy)
            if prev:
                bucket(prev, cur)
            prev = cur
    for gy in range(0, H + 1, step):
        prev = None
        for gx in range(-sub, W + sub + 1, sub):
            cur = warp(gx, gy)
            if prev:
                bucket(prev, cur)
            prev = cur
    # the lattice carries colour: the player's light tints it nearby, the palette's far away
    tint = ld.f_radial(ship[0], ship[1], 1300.0, [grid_c * 0.55 + p1 * 0.45, grid_c, grid_c * 0.7 + hue[2] * 0.3])
    cv.lines(cold, tint, 0.30, 1.1)
    cv.lines(warm, grid_c * 0.6 + grid_h * 0.4, 0.42, 1.15)
    cv.lines(hot, grid_h, 0.85, 1.3)

    # ---------------------------------------------------------------- glyph drawing
    R = 14.0 * WU   # standard enemy radius in pixels

    def glyph(gid, x, y, radius, rot, k, intensity=2.0, width=1.8, aura=0.0, core=False, glass=0.0):
        """k indexes the enemy hue."""
        draw_glyph(cv, glyphs[gid], x, y, radius, rot, hue[k], mate[k], accent, intensity, width, aura, core, glass)

    def mark(gid, x, y, radius, rot):
        segs = [(s[0], -s[1], s[2], -s[3]) for s in glyphs[gid]["segments"]]
        cv.lines(ld.transform(segs, x, y, radius, rot), white, 1.8, 1.5)

    def far_from_ship(x, y, d=110.0):
        in_hud = (x < 560 and y < 150) or (abs(x - W / 2) < 330 and y < 130) or (x > W - 420 and y < 110)
        return math.hypot(x - ship[0], y - ship[1]) > d and not in_hud

    # ---------------------------------------------------------------- enemies
    for i in range(18):                                       # drifters
        x, y = rng.uniform(60, W - 60), rng.uniform(90, H - 60)
        if far_from_ship(x, y, 150):
            tier = rng.choice([1, 1, 2, 2, 3, 4])
            glyph("drifter_%d" % tier, x, y, R, rng.uniform(0, PI), 0, 1.8 + 0.2 * tier, aura=0.10 if tier >= 3 else 0.0)
    for i in range(64):                                       # shoal streaming in a curve from the top right
        u = i / 64.0 + rng.uniform(-0.01, 0.01)
        cx = 2300 - 1150 * u + 90 * math.sin(u * 9.0) + rng.uniform(-38, 38)
        cy = 60 + 420 * u * u + 70 * math.sin(u * 6.0 + 1.0) + rng.uniform(-38, 38)
        glyph("shoal_%d" % rng.choice([1, 2, 3]), cx, cy, R * 0.55, PI - 0.5 + 0.9 * u + rng.uniform(-0.25, 0.25), 0, 2.0, 1.5)
    for i in range(16):                                       # hounds closing in
        a = rng.uniform(PI * 0.55, PI * 1.6)
        d = rng.uniform(210, 620)
        x, y = ship[0] + math.cos(a) * d, ship[1] - math.sin(a) * d * 0.7
        if 40 < x < W - 40 and 80 < y < H - 40:
            glyph("hound_%d" % rng.choice([1, 2, 2, 3, 4]), x, y, R, math.atan2(ship[1] - y, ship[0] - x), 1, 2.2)
    for i in range(9):                                        # moths spiralling around the player
        a = i * 2 * PI / 9 + 0.3
        d = 330 + 14 * i
        glyph("moth_%d" % (2 if i % 3 else 3), ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d * 0.72, R * 1.05, a, 1, 2.1, aura=0.08)
    for i in range(7):                                        # caroms with motion streaks
        x, y = rng.uniform(100, W - 100), rng.uniform(120, H - 80)
        if far_from_ship(x, y, 160):
            glyph("carom_%d" % rng.choice([1, 2, 3]), x, y, R, rng.uniform(0, PI), 2, 2.2)
            ang = rng.uniform(0, 2 * PI)
            tail = (x - math.cos(ang) * 26, y - math.sin(ang) * 26, x - math.cos(ang) * 96, y - math.sin(ang) * 96)
            cv.lines([tail], ld.f_fade(ld.f_linear(tail[0], tail[1], tail[2], tail[3], [hue[2], mate[2]]), tail[0], tail[1], 80, 1.0), 1.6, 2.0)
    for x, y in ((420, 880), (2130, 240), (330, 190), (1700, 980)):   # blinks
        glyph("blink_%d" % rng.choice([2, 3, 4]), x, y, R * 1.05, rng.uniform(0, 1), 2, 2.3, aura=0.12)
        cv.flare(x, y, R * 2.2, mate[2], 0.9, 1.1, 2, PI / 4)

    wyrm = [(1560 + 46 * k, 930 - 70 * math.sin(k * 0.62) - 6 * k) for k in range(10)]   # serpent
    for k, (x, y) in enumerate(wyrm):
        nx, ny = wyrm[max(0, k - 1)]
        rot = math.atan2(ny - y, nx - x) if k else math.atan2(wyrm[0][1] - wyrm[1][1], wyrm[0][0] - wyrm[1][0])
        glyph("wyrm_head_3" if k == 0 else "wyrm_body_3", x, y, R * (1.1 if k == 0 else 1.0), rot, 1, 2.2, aura=0.10 if k == 0 else 0.0)

    bullets = []
    for (sx, sy, phase) in ((300, 620, 0.2), (2060, 520, 1.4)):   # sentries with rotating bullet arms
        glyph("sentry_3", sx, sy, R * 1.15, phase, 2, 2.2, aura=0.14)
        mark("mark_b_heading", sx, sy, R * 1.15, phase)
        for arm in range(3):
            for k in range(1, 9):
                a = phase + arm * 2 * PI / 3 + k * 0.17
                r = 34 + k * 30
                bullets.append((sx + math.cos(a) * r, sy + math.sin(a) * r))

    ex, ey = 1960.0, 300.0                                    # an elite Ram mid-charge
    erot = math.atan2(ship[1] - ey, ship[0] - ex)
    glyph("ram_4", ex, ey, R * 1.25 * 1.8, erot, 1, 2.8, 2.3, aura=0.22, core=True, glass=0.07)
    mark("mark_c", ex, ey, R * 1.25 * 1.8, erot)
    cv.flare(ex, ey, 120, white, 0.9, 1.2, 2, erot)
    telegraph = []
    for k in range(3, 11):
        tx, ty = ex + math.cos(erot) * k * 78, ey + math.sin(erot) * k * 78
        telegraph += ld.transform(ld.polyline([(-0.5, 0.7), (0.5, 0.0), (-0.5, -0.7)]), tx, ty, 13, erot)

    # ---------------------------------------------------------------- explosions: many colours per burst
    for bx, by, age, k in booms:
        cv.glow(bx, by, 150 + 120 * age, hue[k], 0.22 * (1.2 - age))
        cv.glow(bx, by, 50, white, 0.5 * (1.1 - age))
        for i in range(int(86 * (1.2 - 0.5 * age))):
            a = rng.uniform(0, 2 * PI)
            ra = rng.uniform(20 + 90 * age, 70 + 160 * age)
            ln_ = rng.uniform(10, 40)
            u, v = rng.random(), rng.random() * 0.4
            colour = (hue[k] * (1 - u) + mate[k] * u) * (1 - v) + white * v
            seg = (bx + math.cos(a) * ra, by + math.sin(a) * ra, bx + math.cos(a) * (ra + ln_), by + math.sin(a) * (ra + ln_))
            cv.lines([seg], ld.f_linear(seg[0], seg[1], seg[2], seg[3], [colour * 0.5, colour * 0.6 + white * 0.4]), rng.choice([2.4, 3.4, 4.6, 5.6]), 1.4)
        for i in range(10):                                    # slow embers
            a, d = rng.uniform(0, 2 * PI), rng.uniform(10, 60 + 150 * age)
            cv.glow(bx + math.cos(a) * d, by + math.sin(a) * d, 4.0, mate[k], 1.2)
        if age < 0.5:
            ring = ld.transform(ld.polygon(56), bx, by, 60 + 150 * age)
            cv.lines(ring, ld.f_angular(bx, by, [white, hue[k], white, mate[k]]), 1.3, 1.6)
    for s in glyphs["drifter_5"]["segments"]:                  # shatter debris from the big kill
        mx, my = (s[0] + s[2]) * 0.5, -(s[1] + s[3]) * 0.5
        out = 2.6 + rng.random()
        seg = ld.transform([(s[0] - mx, -s[1] - my, s[2] - mx, -s[3] - my)], booms[0][0] + mx * R * out, booms[0][1] + my * R * out, R, rng.uniform(-0.9, 0.9))
        cv.lines(seg, hue[0] * 0.5 + white * 0.5, 2.8, 1.7)

    # Shards streaming to the player
    shard = [(s[0], -s[1], s[2], -s[3]) for s in glyphs["shard"]["segments"]]
    for i in range(46):
        bx, by = booms[i % 3][0], booms[i % 3][1]
        t = rng.random()
        x = bx + (ship[0] - bx) * t + rng.uniform(-60, 60) * (1 - t)
        y = by + (ship[1] - by) * t + rng.uniform(-60, 60) * (1 - t)
        cv.lines(ld.transform(shard, x, y, 5.2, rng.uniform(0, PI)), gold * 0.6 + white * 0.4, 3.2, 1.3)
        cv.glow(x, y, 9.0, gold, 0.5)

    # gravity well: rings of shifting colour around a bright seed
    cv.glow(well[0], well[1], 150, hue[2], 0.16)
    for k, rr in enumerate((62, 44, 28, 14)):
        arc_ = ld.transform(ld.arc(1.0, 0.5 * k, 0.5 * k + 5.3, 30), well[0], well[1], rr)
        cv.lines(arc_, ld.f_angular(well[0], well[1], [hue[2], mate[2], white, mate[2]], 0.7 * k), 1.3 + 0.4 * k, 1.5)
    cv.glow(well[0], well[1], 12, white, 1.6)

    # ---------------------------------------------------------------- player
    cv.glow(ship[0], ship[1], 150, p1, 0.13)
    hull = [(s[0], -s[1], s[2], -s[3]) for s in glyphs["ship"]["segments"]]
    hull_px = ld.transform(hull, ship[0], ship[1], R, aim)
    cv.lines(hull_px, ld.f_radial(ship[0], ship[1], R * 1.3, [white, p1, sw["blue"]]), 2.8, 2.3)
    cv.lines(hull_px, white, 2.0, 0.8)
    halo = [(s[0], -s[1], s[2], -s[3]) for s in glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo, ship[0], ship[1], R, 0.35), ld.f_angular(ship[0], ship[1], [p1, white, p1, sw["sky"]]), 1.5, 1.6)
    bx, by = -math.cos(aim + 0.5), -math.sin(aim + 0.5)
    trail = [(ship[0] + bx * (18 + 10 * k), ship[1] + by * (18 + 10 * k), ship[0] + bx * (28 + 10 * k), ship[1] + by * (28 + 10 * k)) for k in range(13)]
    cv.lines(trail, ld.f_fade(ld.f_linear(ship[0], ship[1], ship[0] + bx * 150, ship[1] + by * 150, [white, p1, sw["blue"]]), ship[0] + bx * 18, ship[1] + by * 18, 150, 1.1), 2.4, 2.6)
    for lane in (-0.16, 0.0, 0.16):                           # Split x2: three lanes on the 1/16 grid
        for k in range(8):
            d = 44 + k * 88
            a = aim + lane
            x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d
            hx, hy = x + math.cos(a) * 24, y + math.sin(a) * 24
            cv.lines([(x, y, hx, hy)], ld.f_linear(x, y, hx, hy, [p1, white]), 4.6, 2.2)
            cv.glow(hx, hy, 9, white, 0.7)
    for k in range(3):                                        # Orbitals, each with a short arc of trail
        a = 0.8 + k * 2 * PI / 3
        ox, oy = ship[0] + math.cos(a) * 62, ship[1] + math.sin(a) * 62
        cv.lines(ld.transform(ld.arc(1.0, a - 0.9, a, 8), ship[0], ship[1], 62), ld.f_fade(ld.f_solid(p1), ox, oy, 60, 1.0), 2.0, 2.0)
        cv.lines(ld.transform(shard, ox, oy, 8.5, a), white * 0.5 + p1 * 0.5, 3.4, 1.7)
        cv.glow(ox, oy, 14, p1, 0.7)
    pulse = ld.transform(ld.polygon(72), ship[0], ship[1], 168)   # Pulse Ring in the player's colour
    cv.lines(pulse, ld.f_angular(ship[0], ship[1], [p1, white, p1, sw["sky"], p1, white]), 2.2, 2.6)
    cv.lines(ld.transform(ld.polygon(72), ship[0], ship[1], 150), p1, 0.7, 1.4)
    dx_, dy_ = ship[0] - 70, ship[1] - 78                     # Sentinel drone and its off-beat shot
    cv.lines(ld.transform(ld.polygon(4), dx_, dy_, 9, 0.2), ld.f_radial(dx_, dy_, 9, [white, p1]), 2.8, 1.7)
    cv.glow(dx_, dy_, 16, p1, 0.6)
    cv.lines([(dx_ - 40, dy_ - 52, dx_ - 62, dy_ - 80)], ld.f_linear(dx_ - 40, dy_ - 52, dx_ - 62, dy_ - 80, [p1, white]), 3.8, 1.9)

    # a Boost waiting to be picked up: the real Prism icon inside its dashed ring
    ox, oy = 1240.0, 880.0
    cv.lines(ld.transform(ic.dashes(1.0, 20, 0.5), ox, oy, 54), white, 0.4, 1.2)
    ic.draw_layers(cv, ic.layers_of(icon_by_id["prism"]), ox, oy, 30, sw)

    # edge indicators: elite (white) to the right, event (gold) at the top
    cv.lines(ld.transform(ld.polyline([(-0.6, 0.8), (0.6, 0.0), (-0.6, -0.8)]), W - 46, 250, 16), white, 1.8, 1.9)
    cv.glow(W - 46, 250, 22, white, 0.3)
    cv.lines(ld.transform(ld.polyline([(-0.6, 0.8), (0.6, 0.0), (-0.6, -0.8)]), 1700, 118, 16, -PI / 2), gold, 2.0, 1.9)
    cv.glow(1700, 118, 22, gold, 0.3)

    # danger: bullets and the charge telegraph, in the scene and on the threat layer
    dias = []
    for bx_, by_ in bullets:
        if 0 < bx_ < W and 0 < by_ < H:
            dias += ld.transform(ld.polygon(4), bx_, by_, 7.5)
            cv.glow(bx_, by_, 13, danger, 0.35)
    cv.lines(dias, danger, 2.4, 2.2)
    cv.threat(dias, danger + white * 0.25, 2.1, 1.5)
    cv.lines(telegraph, danger, 1.3, 1.8)
    cv.threat(telegraph, danger * 0.9, 1.6, 1.0)

    # ---------------------------------------------------------------- HUD
    m = 132                                                    # side margin keeps clear of the camera cut-out
    score_segs, end = ld.text_segments(font, "1,204,880", m, 44, 34)
    cv.lines(score_segs, ld.f_linear(m, 44, m, 78, [white, white, accent]), 1.6, 3.1)
    chain_segs, _ = ld.text_segments(font, "X412", m, 96, 22)
    cv.glow(m + 44, 107, 60, gold, 0.10)
    cv.lines(chain_segs, ld.f_linear(m, 96, m, 118, [white, gold, sw["amber"]]), 2.0, 2.0)
    cv.text(font, "TRACK 03  132 BPM", W / 2 - 17 * 6 * 2.0 / 2, 40, 12, accent, 1.0, 1.5)
    # phrase bar: each section has its colour; the current one is lit
    bar_w, bx0, by0 = 520.0, W / 2 - 260.0, 78.0
    parts = [("G", 16, accent), ("B", 8, sw["amber"]), ("D", 16, white), ("K", 8, sw["sky"])]
    x = bx0
    for name, bars, colour in parts:
        wseg = bar_w * bars / 48.0
        active = name == "B"
        cv.lines([(x + 3, by0, x + wseg - 3, by0)], colour, 2.0 if active else 0.5, 3.4 if active else 2.0)
        if active:
            cv.glow(x + wseg / 2, by0, 40, colour, 0.14)
        x += wseg
    play_x = bx0 + bar_w * 19.5 / 48.0
    cv.lines([(play_x, by0 - 10, play_x, by0 + 10)], white, 2.6, 2.4)
    cv.glow(play_x, by0, 12, white, 0.6)
    cv.text(font, "DROP IN 4.5", W / 2 - 11 * 6 * 1.8 / 2, 98, 11, sw["amber"], 1.0, 1.4)
    for k in range(3):                                        # Nova stock, top right
        cx = W - m - 150 - k * 50
        lit = k < 2
        cv.lines(ld.transform(ld.star(4, 1.0, 0.4), cx, 62, 16, PI / 4), ld.f_radial(cx, 62, 16, [white, p1]) if lit else white, 2.0 if lit else 0.3, 1.8)
        if lit:
            cv.glow(cx, 62, 26, p1, 0.25)
    cv.lines(ld.transform(ld.polygon(32), W - m - 30, 62, 28), white, 0.6, 1.8)       # pause button
    cv.lines([(W - m - 38, 50, W - m - 38, 74), (W - m - 22, 50, W - m - 22, 74)], white, 0.9, 3.0)
    slots = ["orbitals", "pulse_ring", "sentinel", None]      # Riff slots show the real icons
    for k, iid in enumerate(slots):
        cx = W / 2 - 138 + k * 92
        if iid:
            ic.draw_layers(cv, ic.layers_of(icon_by_id[iid]), cx, H - 62, 26, sw)
            for pip in range(5):
                on = pip < (3, 2, 4)[k]
                cv.lines([(cx - 18 + pip * 9, H - 22, cx - 13 + pip * 9, H - 22)], p1 if on else white, 1.4 if on else 0.15, 2.2)
        else:
            cv.lines(ld.transform(ic.dashes(1.0, 14, 0.5), cx, H - 62, 26), white, 0.3, 1.3)
    ic.draw_layers(cv, ic.layers_of(icon_by_id["prism"]), 1206, H - 152, 20, sw)      # active Boost
    cv.text(font, "PRISM", 1240, H - 162, 11, gold, 1.2, 1.4)
    for k in range(4):
        cv.lines([(1240 + k * 22, H - 140, 1256 + k * 22, H - 140)], gold, 1.5 if k < 3 else 0.2, 3.0)

    # touch controls: dashed stick rings, a filled thumb pad; the pressed button is filled and brighter
    def dashed_ring(cx, cy, radius, count=20):
        return ld.transform(ic.dashes(1.0, count, 0.55, 0.0, 3), cx, cy, radius)

    left = (m + 210, H - 250)
    cv.lines(dashed_ring(left[0], left[1], 150), white, 0.34, 2.0)
    cv.discs([(left[0] + 62, left[1] - 40, 44)], white, 0.16)
    cv.lines(ld.transform(ld.polygon(40), left[0] + 62, left[1] - 40, 44), white, 0.7, 2.0)
    right = (W - m - 210, H - 250)
    cv.lines(dashed_ring(right[0], right[1], 150), white, 0.34, 2.0)
    cv.discs([(right[0] + 80, right[1] - 36, 44)], p1, 0.22)
    cv.lines(ld.transform(ld.polygon(40), right[0] + 80, right[1] - 36, 44), p1, 0.9, 2.0)
    nova = (W - m - 430, H - 330)                              # idle button: outline and label
    cv.lines(ld.transform(ld.polygon(48), nova[0], nova[1], 74), white, 0.5, 2.2)
    cv.text(font, "NOVA", nova[0] - 4 * 6 * 2.0 / 2 + 2, nova[1] - 6, 12, white, 0.9, 1.6)
    dash = (W - m - 300, H - 520)                              # pressed button: filled, brighter, larger
    cv.discs([(dash[0], dash[1], 66)], p1, 0.45)
    cv.lines(ld.transform(ld.polygon(48), dash[0], dash[1], 66), p1, 2.0, 3.0)
    cv.text(font, "DASH", dash[0] - 4 * 6 * 2.0 / 2 + 2, dash[1] - 6, 12, white, 1.8, 1.8)

    return cv


def main():
    pal_id = sys.argv[1] if len(sys.argv) > 1 else "reef"
    out = os.path.join(DESIGN, "previews", "frame_%s.png" % pal_id)
    build(pal_id).render(bloom=BLOOM).save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
