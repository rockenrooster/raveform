"""RAVEFORM target frame: a full 2400x1080 mock of minute ten, HUD and touch controls included.

This is the look target for milestone M1 and the HUD layout reference for M3. It is assembled from the
real data files (palettes.json, glyphs.json, vector_font.txt) with the look-dev renderer.

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

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")
W, H = 2400, 1080
WU = 1.5            # pixels per world unit at this resolution (view is 1600 x 720 wu)
PI = math.pi


def main():
    pal_id = sys.argv[1] if len(sys.argv) > 1 else "reef"
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    pal = next(p for p in pals["palettes"] if p["id"] == pal_id)
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    rng = random.Random(4)

    def lin(e):
        return np.asarray(e["linear"], np.float32)

    white = np.ones(3, np.float32)
    hue = [lin(pal["enemyCandidates"][i]) for i in pal["soloEnemies"]]
    p1 = lin(pals["constants"]["players"][0])
    danger = lin(pals["constants"]["danger"])
    gold = lin(pals["constants"]["pickup"])
    accent = lin(pal["accent"])

    cv = ld.Canvas(W, H)
    cv.radial_bg(lin(pal["bg0"]) * 0.55, lin(pal["bg1"]) * 0.55, cx=0.42, cy=0.52)

    ship = (980.0, 570.0)
    aim = math.radians(-24.0)
    booms = [(1500.0, 330.0, 1.0), (1330.0, 690.0, 0.35), (640.0, 300.0, 0.75)]
    well = (700.0, 860.0)

    # ---------------------------------------------------------------- grid
    def warp(x, y):
        amount = 0.0
        for bx, by, age in booms:
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

    # Heat is graded, not binary: only the shock front itself goes white-hot.
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
    grid_c, grid_h = lin(pal["grid"]), lin(pal["gridHot"])
    cv.lines(cold, grid_c, 0.30, 1.1)
    cv.lines(warm, grid_c * 0.6 + grid_h * 0.4, 0.42, 1.15)
    cv.lines(hot, grid_h, 0.85, 1.3)

    # ---------------------------------------------------------------- batching helpers
    batches = {}

    def add(colour_key, colour, intensity, width, segs):
        batches.setdefault((colour_key, round(intensity, 2), round(width, 2)), (colour, []))[1].extend(segs)

    def glyph(gid, x, y, radius, rot, colour_key, colour, intensity=2.0, width=1.8):
        g = glyphs[gid]
        for layer, k, wk in ((0, 1.0, 1.0), (1, 0.72, 0.85), (2, 1.15, 0.85)):
            segs = [(s[0], -s[1], s[2], -s[3]) for s in g["segments"] if s[4] == layer]
            if segs:
                add(colour_key, colour, intensity * k, width * wk, ld.transform(segs, x, y, radius, rot))

    def far_from_ship(x, y, d=110.0):
        in_hud = (x < 560 and y < 150) or (abs(x - W / 2) < 330 and y < 130) or (x > W - 420 and y < 110)
        return math.hypot(x - ship[0], y - ship[1]) > d and not in_hud

    R = 14.0 * WU   # standard enemy radius in pixels

    # ---------------------------------------------------------------- enemies
    for i in range(18):                                       # drifters
        x, y = rng.uniform(60, W - 60), rng.uniform(90, H - 60)
        if far_from_ship(x, y, 150):
            glyph("drifter_%d" % rng.choice([1, 1, 2, 2, 3]), x, y, R, rng.uniform(0, PI), 0, hue[0])
    for i in range(64):                                       # shoal streaming in a curve from the top right
        u = i / 64.0 + rng.uniform(-0.01, 0.01)
        cx = 2300 - 1150 * u + 90 * math.sin(u * 9.0) + rng.uniform(-38, 38)
        cy = 60 + 420 * u * u + 70 * math.sin(u * 6.0 + 1.0) + rng.uniform(-38, 38)
        glyph("shoal_%d" % rng.choice([1, 2, 3]), cx, cy, R * 0.55, PI - 0.5 + 0.9 * u + rng.uniform(-0.25, 0.25), 0, hue[0], 2.0, 1.5)
    for i in range(16):                                       # hounds closing in
        a = rng.uniform(PI * 0.55, PI * 1.6)
        d = rng.uniform(210, 620)
        x, y = ship[0] + math.cos(a) * d, ship[1] - math.sin(a) * d * 0.7
        if 40 < x < W - 40 and 80 < y < H - 40:
            glyph("hound_%d" % rng.choice([1, 2, 2, 3, 4]), x, y, R, math.atan2(ship[1] - y, ship[0] - x), 1, hue[1], 2.2)
    for i in range(9):                                        # moths spiralling around the player
        a = i * 2 * PI / 9 + 0.3
        d = 330 + 14 * i
        glyph("moth_%d" % (2 if i % 3 else 3), ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d * 0.72, R * 1.05, a, 1, hue[1], 2.1)
    for i in range(7):                                        # caroms with motion streaks
        x, y = rng.uniform(100, W - 100), rng.uniform(120, H - 80)
        if far_from_ship(x, y, 160):
            rot = rng.uniform(0, PI)
            glyph("carom_%d" % rng.choice([1, 2, 3]), x, y, R, rot, 2, hue[2], 2.2)
            ang = rng.uniform(0, 2 * PI)
            add(2, hue[2], 0.7, 2.0, [(x - math.cos(ang) * 26, y - math.sin(ang) * 26, x - math.cos(ang) * 78, y - math.sin(ang) * 78)])
    for x, y in ((420, 880), (2130, 240), (330, 190), (1700, 980)):   # blinks
        glyph("blink_%d" % rng.choice([1, 2, 3]), x, y, R * 1.05, rng.uniform(0, 1), 2, hue[2], 2.3)

    wyrm = [(1560 + 46 * k, 930 - 70 * math.sin(k * 0.62) - 6 * k) for k in range(10)]   # serpent
    for k, (x, y) in enumerate(wyrm):
        nx, ny = wyrm[max(0, k - 1)]
        rot = math.atan2(ny - y, nx - x) if k else math.atan2(wyrm[0][1] - wyrm[1][1], wyrm[0][0] - wyrm[1][0])
        glyph("wyrm_head_3" if k == 0 else "wyrm_body_3", x, y, R * (1.1 if k == 0 else 1.0), rot, 1, hue[1], 2.2)

    # sentries with rotating bullet patterns (danger)
    bullets = []
    for (sx, sy, phase) in ((300, 620, 0.2), (2060, 520, 1.4)):
        glyph("sentry_3", sx, sy, R * 1.15, phase, 2, hue[2], 2.2)
        glyph("mark_b_heading", sx, sy, R * 1.15, phase, "w", white, 1.8, 1.5)
        for arm in range(3):
            for k in range(1, 9):
                a = phase + arm * 2 * PI / 3 + k * 0.17
                r = 34 + k * 30
                bullets.append((sx + math.cos(a) * r, sy + math.sin(a) * r))
    # an elite Ram charging: white core copy, crown ticks, danger chevrons along its path
    ex, ey = 1960.0, 300.0
    erot = math.atan2(ship[1] - ey, ship[0] - ex)
    glyph("ram_4", ex, ey, R * 1.25 * 1.8, erot, 1, hue[1], 2.8, 2.2)
    glyph("ram_4", ex, ey, R * 1.25 * 1.8, erot, "w", white, 1.0, 0.9)
    glyph("mark_c", ex, ey, R * 1.25 * 1.8, erot, "w", white, 1.6, 1.5)
    telegraph = []
    for k in range(3, 11):
        tx, ty = ex + math.cos(erot) * k * 78, ey + math.sin(erot) * k * 78
        telegraph += ld.transform(ld.polyline([(-0.5, 0.7), (0.5, 0.0), (-0.5, -0.7)]), tx, ty, 13, erot)

    # ---------------------------------------------------------------- effects
    def burst(x, y, colour, n, r0, r1, key):
        for i in range(n):
            a = rng.uniform(0, 2 * PI)
            ra = rng.uniform(r0, r1)
            ln = rng.uniform(10, 38)
            mix = 0.35 * rng.random()
            add(key + "s%d" % (i % 4), colour * (1 - mix) + white * mix, rng.choice([2.2, 3.2, 4.4, 5.5]), 1.4,
                [(x + math.cos(a) * ra, y + math.sin(a) * ra, x + math.cos(a) * (ra + ln), y + math.sin(a) * (ra + ln))])

    for (bx, by, age), colour, key in zip(booms, (hue[0], hue[1], hue[2]), ("b0", "b1", "b2")):
        burst(bx, by, colour, int(70 * (1.2 - 0.5 * age)), 20 + 90 * age, 70 + 160 * age, key)
        if age < 0.5:      # shock rings are brief: only the newest explosion still shows one
            add("ring", white, 1.1, 1.4, ld.transform(ld.polygon(48), bx, by, 60 + 150 * age))
    # shatter debris from the big kill: the enemy's own segments tumbling outward
    for s in glyphs["drifter_5"]["segments"]:
        mx, my = (s[0] + s[2]) * 0.5, -(s[1] + s[3]) * 0.5
        out = 2.6 + rng.random()
        spin = rng.uniform(-0.9, 0.9)
        seg = ld.transform([(s[0] - mx, -s[1] - my, s[2] - mx, -s[3] - my)], booms[0][0] + mx * R * out, booms[0][1] + my * R * out, R, spin)
        add("deb", hue[0] * 0.6 + white * 0.4, 2.6, 1.6, seg)

    # shards streaming to the player
    for i in range(46):
        bx, by, _ = booms[i % 3]
        t = rng.random()
        x = bx + (ship[0] - bx) * t + rng.uniform(-60, 60) * (1 - t)
        y = by + (ship[1] - by) * t + rng.uniform(-60, 60) * (1 - t)
        glyph("shard", x, y, 5.0, rng.uniform(0, PI), "g", gold, 3.0, 1.3)

    # gravity well
    for k, rr in enumerate((62, 44, 28, 14)):
        add("well", hue[2], 1.2 + 0.35 * k, 1.5, ld.transform(ld.arc(1.0, 0.5 * k, 0.5 * k + 5.3, 30), well[0], well[1], rr))

    # ---------------------------------------------------------------- player
    glyph("ship", ship[0], ship[1], R, aim, "p", p1, 2.6, 2.2)
    glyph("ship", ship[0], ship[1], R, aim, "w", white, 2.0, 0.8)
    glyph("halo_3", ship[0], ship[1], R, 0.35, "p", p1, 1.3, 1.5)
    for k in range(12):                                       # engine trail
        t0, t1 = k / 12.0, (k + 1) / 12.0
        bx, by = -math.cos(aim + 0.5), -math.sin(aim + 0.5)
        add("trail%d" % k, p1, 1.7 * (1 - t0) ** 1.5, 2.4,
            [(ship[0] + bx * (18 + 120 * t0), ship[1] + by * (18 + 120 * t0), ship[0] + bx * (18 + 120 * t1), ship[1] + by * (18 + 120 * t1))])
    shot = p1 * 0.45 + white * 0.55
    for lane in (-0.16, 0.0, 0.16):                           # Split x2: three lanes on the 1/16 grid
        for k in range(8):
            d = 44 + k * 88
            a = aim + lane
            x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d
            add("shot", shot, 4.5, 2.2, [(x, y, x + math.cos(a) * 22, y + math.sin(a) * 22)])
    for k in range(3):                                        # Orbitals
        a = 0.8 + k * 2 * PI / 3
        glyph("shard", ship[0] + math.cos(a) * 62, ship[1] + math.sin(a) * 62, 8.0, a, "p", p1, 3.2, 1.6)
    add("pulse", p1, 2.0, 2.6, ld.transform(ld.polygon(64), ship[0], ship[1], 168))                        # Pulse Ring: the
    add("pulse2", p1, 0.7, 1.4, ld.transform(ld.polygon(64), ship[0], ship[1], 150))                       # player's colour, with an echo
    sx, sy = ship[0] - 70, ship[1] - 78                       # Sentinel drone and its off-beat shot
    add("p2", p1, 2.6, 1.6, ld.transform(ld.polygon(4), sx, sy, 9, 0.2))
    add("shot", shot, 3.6, 1.8, [(sx - 40, sy - 52, sx - 62, sy - 80)])

    # Boost orb waiting to be picked up
    ox, oy = 1240.0, 880.0
    add("g2", gold, 2.4, 1.8, ld.transform(ld.polygon(24), ox, oy, 24))
    add("g2", gold, 2.8, 1.6, ld.transform(ld.polygon(3, 1.0, -PI / 2), ox, oy, 12))

    # edge indicators: elite (white) to the right, event (gold) at the top
    add("w", white, 1.6, 1.8, ld.transform(ld.polyline([(-0.6, 0.8), (0.6, 0.0), (-0.6, -0.8)]), W - 46, 250, 16))
    add("g2", gold, 1.8, 1.8, ld.transform(ld.polyline([(-0.6, 0.8), (0.6, 0.0), (-0.6, -0.8)]), 1700, 118, 16, -PI / 2))

    for (colour, segs), (key, intensity, width) in ((v, k) for k, v in batches.items()):
        cv.lines(segs, colour, intensity, width)

    # danger: bullets and the charge telegraph, in the scene and on the threat layer
    dias = []
    for bx, by in bullets:
        if 0 < bx < W and 0 < by < H:
            dias += ld.transform(ld.polygon(4), bx, by, 7.5)
    cv.lines(dias, danger, 2.4, 2.2)
    cv.threat(dias, danger + white * 0.25, 2.1, 1.5)
    cv.lines(telegraph, danger, 1.3, 1.8)
    cv.threat(telegraph, danger * 0.9, 1.6, 1.0)

    # ---------------------------------------------------------------- HUD
    m = 132                                                    # side margin keeps clear of the camera cut-out
    cv.text(font, "1,204,880", m, 44, 34, white, 1.5)
    cv.text(font, "X412", m, 96, 22, gold, 1.9)
    cv.text(font, "TRACK 03  132 BPM", W / 2 - 17 * 6 * 2.0 / 2, 40, 12, accent, 1.0, 1.5)
    # phrase bar: Groove / Build / Drop / Break with a playhead and beat ticks
    bar_w, bx0, by0 = 520.0, W / 2 - 260.0, 78.0
    parts = [("G", 16), ("B", 8), ("D", 16), ("K", 8)]
    x = bx0
    for name, bars in parts:
        wseg = bar_w * bars / 48.0
        active = name == "B"
        cv.lines([(x + 3, by0, x + wseg - 3, by0)], accent if active else white, 1.6 if active else 0.45, 3.0 if active else 2.0)
        x += wseg
    cv.lines([(bx0 + bar_w * 19.5 / 48.0, by0 - 9, bx0 + bar_w * 19.5 / 48.0, by0 + 9)], white, 2.2, 2.2)
    cv.text(font, "DROP IN 4.5", W / 2 - 11 * 6 * 1.8 / 2, 98, 11, white, 0.8, 1.4)
    for k in range(3):                                        # Nova stock, top right
        cx = W - m - 150 - k * 50
        cv.lines(ld.transform(ld.star(4, 1.0, 0.4), cx, 62, 16, PI / 4), p1 if k < 2 else white, 1.6 if k < 2 else 0.3, 1.8)
    cv.lines(ld.transform(ld.polygon(32), W - m - 30, 62, 28), white, 0.6, 1.8)       # pause button
    cv.lines([(W - m - 38, 50, W - m - 38, 74), (W - m - 22, 50, W - m - 22, 74)], white, 0.9, 3.0)
    for k in range(4):                                        # Riff slots with level pips, bottom centre
        cx = W / 2 - 120 + k * 80
        filled = k < 3
        cv.lines(ld.transform(ld.polygon(6, 1.0, PI / 6), cx, H - 58, 22), p1 if filled else white, 1.0 if filled else 0.25, 1.6)
        for pip in range(5):
            cv.lines([(cx - 16 + pip * 8, H - 26, cx - 12 + pip * 8, H - 26)], p1, 1.2 if filled and pip < (3, 2, 4, 0)[k] else 0.15, 2.0)
    cv.text(font, "PRISM", 1170, H - 122, 11, gold, 1.2, 1.4)   # active Boost with bars remaining
    for k in range(4):
        cv.lines([(1170 + k * 22, H - 100, 1186 + k * 22, H - 100)], gold, 1.4 if k < 3 else 0.2, 3.0)

    # touch controls: sticks are faint rings; the pressed button is filled and brighter
    def dashed_ring(cx, cy, radius, dashes=20):
        segs = []
        for k in range(dashes):
            a0 = 2 * PI * k / dashes
            segs += ld.transform(ld.arc(1.0, a0, a0 + 2 * PI / dashes * 0.55, 3), cx, cy, radius)
        return segs

    # Stick rings are dashed so they can never be mistaken for a shock ring in the world.
    left = (m + 210, H - 250)
    cv.lines(dashed_ring(left[0], left[1], 150), white, 0.34, 2.0)
    cv.discs([(left[0] + 62, left[1] - 40, 44)], white, 0.16)
    cv.lines(ld.transform(ld.polygon(40), left[0] + 62, left[1] - 40, 44), white, 0.7, 2.0)
    right = (W - m - 210, H - 250)
    cv.lines(dashed_ring(right[0], right[1], 150), white, 0.34, 2.0)
    cv.discs([(right[0] + 80, right[1] - 36, 44)], p1, 0.22)
    cv.lines(ld.transform(ld.polygon(40), right[0] + 80, right[1] - 36, 44), p1, 0.9, 2.0)
    nova = (W - m - 430, H - 330)                              # idle button: outline only
    cv.lines(ld.transform(ld.polygon(48), nova[0], nova[1], 74), white, 0.5, 2.2)
    cv.text(font, "NOVA", nova[0] - 4 * 6 * 2.0 / 2 + 2, nova[1] - 6, 12, white, 0.9, 1.6)
    dash = (W - m - 300, H - 520)                              # pressed button: filled, brighter, larger
    cv.discs([(dash[0], dash[1], 66)], p1, 0.45)
    cv.lines(ld.transform(ld.polygon(48), dash[0], dash[1], 66), p1, 2.0, 3.0)
    cv.text(font, "DASH", dash[0] - 4 * 6 * 2.0 / 2 + 2, dash[1] - 6, 12, white, 1.8, 1.8)

    out = os.path.join(DESIGN, "previews", "frame_%s.png" % pal_id)
    cv.render(bloom=0.9).save(out, optimize=True)
    print("wrote", out, "batches:", len(batches))


if __name__ == "__main__":
    main()
