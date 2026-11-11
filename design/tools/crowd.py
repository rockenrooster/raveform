"""RAVEFORM crowd study: the look at endgame density.

The target frame (frame_mock.py) holds about 130 enemies, 50 bullets and three kills. The endgame holds
700 enemies, 1,500 bullets and a hundred kills a second (design/scenes.md §4), and light that reads well
at the first density washes the screen out at the second. This tool builds a still of the endgame from
the same glyphs and recipes, with every kind of light on its own layer, so the crowd rules of
design/look.md §3 ("Light budget") can be tuned on a picture instead of argued.

Usage:  python design/tools/crowd.py                 -> design/previews/frame_endgame_ref.png and
                                                        design/previews/crowd_rules.png (before / after)
        python design/tools/crowd.py study <folder> [name ...]
                                                     -> the variants in STUDY (also the afterglow, as it
                                                        looks when the whole horde drifts), with numbers
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image, ImageDraw

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import frame_mock as fm  # noqa: E402
import scenes as sc  # noqa: E402

W, H, WU, PI = fm.W, fm.H, fm.WU, math.pi
R = 14.0 * WU                      # standard enemy radius in pixels
BEAT = 60.0 / 132.0

# Light classes. "Crowd light" is everything the horde makes; the rest never dims.
CROWD = ("enemy", "bullet", "fx", "flash", "pool", "aura", "warp", "shard", "heat")
FIXED = ("grid", "elite", "player")

# The recipes of look.md v2 (as the engine first drew them) and of v2.1.
OLD = dict(tierAuras=True, warpFixedWu=90.0, smallRing=True, smallFlash="pool", maxRings=999, priorityShip=False,
           flashS=None)
NEW = dict(tierAuras=False, warpFixedWu=None, smallRing=False, smallFlash="sparkle", maxRings=6, priorityShip=True,
           flashS=0.15)


class Rec(ld.Canvas):
    """A canvas that only records primitives; engine_hdr() draws them the way the engine does."""

    def _mask(self, segs, width):
        return None

    def glow(self, x, y, radius, colour, intensity=1.0):
        self.rec.append(("glow", float(x), float(y), float(radius), tuple(float(v) for v in np.asarray(colour, np.float32) * intensity)))

    def fill(self, points, colour, intensity=0.1):
        points = list(points)
        cols = np.broadcast_to(np.asarray(colour, np.float32) * intensity, (len(points), 3))
        self.rec.append(("fill", [(float(p[0]), float(p[1])) for p in points], [tuple(float(v) for v in c) for c in cols]))


def build(rules, seed=1, lancer=False):
    """The endgame scene of design/scenes.md §4 at a quiet instant (no Drop burst), one canvas per light
    class. Returns (layers, base) where base carries the background."""
    pals = json.load(open(os.path.join(fm.DESIGN, "palettes.json"), encoding="utf-8"))
    pal = next(p for p in pals["palettes"] if p["id"] == "reef")
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(fm.DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    rng = random.Random(seed)

    def lin(e):
        return np.asarray(e["linear"], np.float32)

    white = np.ones(3, np.float32)
    cands = [pal["enemyCandidates"][i] for i in pal["soloEnemies"]]
    hue = [lin(c) for c in cands]
    mate = [fm.partner_of(c) for c in cands]
    player = pals["constants"]["players"][0]
    p1, deep, light = lin(player), lin(player["deep"]), lin(player["light"])
    danger, gold, accent = lin(pals["constants"]["danger"]), lin(pals["constants"]["pickup"]), lin(pal["accent"])
    grid_c, grid_h = lin(pal["grid"]), lin(pal["gridHot"])

    L = {c: Rec(W, H) for c in CROWD + FIXED + ("heat_rest",)}
    base = L["grid"]
    base.radial_bg(lin(pal["bg0"]) * 0.5, lin(pal["bg1"]) * 0.5, cx=0.46, cy=0.52)
    base.nebula([(420, 300, 620, hue[2], 0.030), (1900, 260, 700, hue[0], 0.026), (1250, 900, 760, mate[1], 0.022),
                 (2200, 900, 520, hue[2], 0.024), (120, 950, 520, hue[0], 0.020), (1100, 120, 520, accent, 0.016)])
    for _ in range(420):
        c = rng.choice([white, white, white, hue[0], hue[2], mate[1]])
        base.glow(rng.uniform(0, W), rng.uniform(0, H), rng.choice([1.6, 1.6, 2.2, 3.0]), c, rng.uniform(0.15, 0.7))

    ship = (1200.0, 560.0)
    aim = math.radians(-22.0)

    def in_view(x, y, pad=60.0):
        return -pad < x < W + pad and -pad < y < H + pad

    # ------------------------------------------------------------ kills alive at this instant
    def kill_place():
        if rng.random() < 0.6:                                   # where the guns point
            a = aim + rng.uniform(-0.7, 0.7)
            d = rng.uniform(120, 1000)
            return ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d
        return rng.uniform(40, W - 40), rng.uniform(40, H - 40)

    kills = []                                                    # (x, y, size, age s, hue index)
    for size, per_second, life in (("small", 40 / BEAT, 0.7), ("medium", 6 / BEAT, 0.9), ("large", 2 / (4 * BEAT), 1.2)):
        for _ in range(int(round(per_second * life))):
            x, y = kill_place()
            if in_view(x, y):
                kills.append((x, y, size, rng.uniform(0, life), rng.choice([0, 0, 1, 1, 2])))
    wells = [(ship[0] - 430, ship[1] + 250), (ship[0] + 520, ship[1] - 330), (ship[0] + 150, ship[1] + 400)]

    # ------------------------------------------------------------ lattice
    amp = {"small": 6.8, "medium": 15.9, "large": 34.0}

    def warp(x, y):
        amount = 0.0
        for kx, ky, size, age, _ in kills:
            dx, dy = x - kx, y - ky
            if abs(dx) > 320 or abs(dy) > 320:
                continue
            d = math.hypot(dx, dy) + 1e-3
            radius = 30.0 + 190.0 * age
            push = amp[size] * max(0.0, 1.0 - 0.8 * age) * math.exp(-((d - radius) / 50.0) ** 2)
            x, y = x + dx / d * push, y + dy / d * push
            amount += push
        for wx, wy in wells:
            dx, dy = x - wx, y - wy
            d = math.hypot(dx, dy) + 1e-3
            pull = min(d * 0.7, 1500.0 / (d + 20.0))
            x, y = x - dx / d * pull, y - dy / d * pull
            amount += max(0.0, pull - 6.0)
        dx, dy = x - ship[0], y - ship[1]
        d = math.hypot(dx, dy) + 1e-3
        ripple = 7.0 * math.exp(-((d - 250.0) / 40.0) ** 2)
        return x + dx / d * ripple, y + dy / d * ripple, amount + ripple

    cold, warm, hot = [], [], []
    step, sub = 30, 15

    def bucket(a, b):
        heat = (a[2] + b[2]) * 0.5
        (hot if heat > 19.5 else warm if heat > 7.5 else cold).append((a[0], a[1], b[0], b[1]))

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
    tint = ld.f_radial(ship[0], ship[1], 1300.0, [grid_c * 0.55 + p1 * 0.45, grid_c, grid_c * 0.7 + hue[2] * 0.3])
    base.lines(cold, tint, 0.30, 1.1)
    L["heat"].lines(warm, grid_c * 0.6 + grid_h * 0.4, 0.42, 1.15)
    L["heat"].lines(hot, grid_h, 0.85, 1.3)
    L["heat_rest"].lines(warm + hot, tint, 0.30, 1.1)           # the same lines as they are at rest
    grid_stats = (len(cold), len(warm), len(hot))

    # ------------------------------------------------------------ enemies
    count = {"enemies": 0, "elites": 0, "bullets": 0}
    aura_tier = [0.0, 0.0, 0.08, 0.10, 0.14]

    def enemy(family, tier, x, y, rot, k, elite=False, scale=1.0, streak=None):
        if not in_view(x, y, 40):
            return
        g = glyphs["%s_%d" % (family, tier)]
        radius = R * g["size"] * scale
        ext = radius * g["extent"]
        count["enemies"] += 1
        if elite:
            count["elites"] += 1
            L["elite"].glow(x, y, ext * 2.3 * 1.5, hue[k], 0.22)
            fm.draw_glyph(L["elite"], g, x, y, radius * 1.5, rot, hue[k], mate[k], accent, 1.8 + 0.2 * (tier - 1) + 0.6, 2.3,
                          core=True, glass=0.07)
            return
        if rules["tierAuras"] and aura_tier[tier - 1] > 0:
            L["aura"].glow(x, y, ext * 2.3, hue[k], aura_tier[tier - 1])
        fm.draw_glyph(L["enemy"], g, x, y, radius, rot, hue[k], mate[k], accent, 1.8 + 0.2 * (tier - 1), 1.8)
        if streak is not None:
            tail = (x - math.cos(streak) * 26, y - math.sin(streak) * 26, x - math.cos(streak) * 96, y - math.sin(streak) * 96)
            L["enemy"].lines([tail], ld.f_fade(ld.f_linear(tail[0], tail[1], tail[2], tail[3], [hue[k], mate[k]]), tail[0], tail[1], 80, 1.0), 1.6, 2.0)

    elite_roll = lambda p: rng.random() < p                        # noqa: E731

    for s in range(3):                                            # Shoal: three streams of 87
        ph, ax, ay = rng.uniform(0, 6.28), rng.uniform(700, 1000), rng.uniform(260, 400)
        for i in range(87):
            u = i * 27.0 / 2400.0 * 2 * PI + ph
            x = ship[0] + ax * math.sin(u * 0.5 + s * 2.1) + rng.uniform(-30, 30)
            y = ship[1] + ay * math.sin(u + s * 1.3) + rng.uniform(-30, 30)
            enemy("shoal", rng.choice([2, 3, 4]), x, y, math.atan2(ay * math.cos(u + s * 1.3), 0.5 * ax * math.cos(u * 0.5 + s * 2.1)), 0)
    for pack in range(2):                                         # Hounds: two packs charging the ship
        a0 = rng.uniform(0, 2 * PI)
        for i in range(55):
            a = a0 + rng.gauss(0, 0.35)
            d = rng.uniform(170, 820)
            x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d * 0.8
            enemy("hound", rng.choice([2, 3, 4]), x, y, math.atan2(ship[1] - y, ship[0] - x), 1, elite_roll(0.05))
    for i in range(90):                                           # Drifters
        enemy("drifter", rng.choice([2, 3, 3, 4, 5]), rng.uniform(40, W - 40), rng.uniform(40, H - 40), rng.uniform(0, PI), 0, elite_roll(0.05))
    for c in range(6):                                            # Wyrms: six chains of twelve
        x0, y0, hd = rng.uniform(200, W - 200), rng.uniform(150, H - 150), rng.uniform(0, 2 * PI)
        pts = []
        for k in range(12):
            s_ = k * 44.0
            off = 60.0 * math.sin(k * 0.62 + c)
            pts.append((x0 + math.cos(hd) * s_ - math.sin(hd) * off, y0 + math.sin(hd) * s_ + math.cos(hd) * off))
        for k, (x, y) in enumerate(pts):
            nx, ny = pts[max(0, k - 1)] if k else (2 * pts[0][0] - pts[1][0], 2 * pts[0][1] - pts[1][1])
            enemy("wyrm_head" if k == 0 else "wyrm_body", 3, x, y, math.atan2(ny - y, nx - x), 1)
    moths = []
    for ring in range(4):                                         # Moths: four rings of nine around the ship
        d = (300 + 20 * ring) * WU
        for i in range(9):
            a = i * 2 * PI / 9 + ring * 0.35
            x, y = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d
            moths.append((x, y, a))
            enemy("moth", rng.choice([2, 3, 4]), x, y, a, 1, elite_roll(0.06))
    for i in range(30):                                           # Caroms with streaks
        enemy("carom", rng.choice([2, 3, 4]), rng.uniform(80, W - 80), rng.uniform(80, H - 80), rng.uniform(0, PI), 2, elite_roll(0.06),
              streak=rng.uniform(0, 2 * PI))
    lethal = []                                                   # beams and trails: scene and threat layer
    for i in range(12):                                           # Tethers: pairs joined by a lethal beam
        x, y, a = rng.uniform(100, W - 100), rng.uniform(100, H - 100), rng.uniform(0, PI)
        ax_, ay_ = math.cos(a) * 120, math.sin(a) * 120
        enemy("tether", 3, x - ax_, y - ay_, a, 2)
        enemy("tether", 3, x + ax_, y + ay_, a + PI, 2)
        lethal.append((x - ax_ * 0.8, y - ay_ * 0.8, x + ax_ * 0.8, y + ay_ * 0.8))
    for i in range(16):                                           # Blinks on a five-point star
        a = i * 4 * PI / 5 + 0.2 + (i // 5) * 0.4
        x, y = ship[0] - 500 + math.cos(a) * (300 - 30 * (i // 5)), ship[1] - 120 + math.sin(a) * (300 - 30 * (i // 5))
        enemy("blink", rng.choice([3, 4]), x, y, rng.uniform(0, 1), 2)
        if in_view(x, y):
            L["enemy"].flare(x, y, R * 2.2, mate[2], 0.9, 1.1, 2, PI / 4)
    for i in range(12):                                           # Weavers and their lethal trails
        x, y, hd = rng.uniform(150, W - 150), rng.uniform(120, H - 120), rng.uniform(0, 2 * PI)
        enemy("weaver", 3, x, y, hd, 2)
        px, py = x, y
        for k in range(40):
            hd2 = hd + PI + 0.9 * math.sin(k * 0.21 + i)
            qx, qy = px + math.cos(hd2) * 16, py + math.sin(hd2) * 16
            lethal.append((px, py, qx, qy))
            px, py = qx, qy
    bullets = []
    for i in range(10):                                           # Sentries with three arms of twenty bullets
        a = i * 2 * PI / 10 + 0.2
        d = rng.uniform(480, 560) * WU
        sx, sy = ship[0] + math.cos(a) * d, ship[1] + math.sin(a) * d * 0.62
        phase = rng.uniform(0, 2)
        enemy("sentry", rng.choice([3, 4]), sx, sy, phase, 2)
        for arm in range(3):
            for k in range(1, 21):
                b = phase + arm * 2 * PI / 3 + k * 0.17
                bullets.append((sx + math.cos(b) * (34 + k * 30), sy + math.sin(b) * (34 + k * 30)))
    bx0, by0 = ship[0] + 760, ship[1] - 230                       # boss stand-in: a wheel of eight arms
    for arm in range(8):
        a = arm * PI / 4 + 0.3
        for k in range(6):
            d = 60 + k * 50
            x, y = bx0 + math.cos(a) * d, by0 + math.sin(a) * d
            if in_view(x, y, 40):
                count["enemies"] += 1
                fm.draw_glyph(L["elite"], glyphs["carom_3"], x, y, R * (1.5 - 0.15 * k), a, hue[2], mate[2], accent, 2.2, 1.8)
        for ring in range(8):                                     # fans of seven from each arm
            for f in range(-3, 4):
                b = a + f * 0.06 + ring * 0.11
                bullets.append((bx0 + math.cos(b) * (330 + ring * 68), by0 + math.sin(b) * (330 + ring * 68)))
    for x, y, a in moths:                                         # Moth petals
        for k in range(1, 6):
            bullets.append((x + math.cos(a + 0.5 * k) * 40 * k, y + math.sin(a + 0.5 * k) * 40 * k))
    for i in range(40):                                           # Hound bursts of three
        a, d = rng.uniform(0, 2 * PI), rng.uniform(150, 700)
        for k in range(3):
            bullets.append((ship[0] + math.cos(a) * (d + 26 * k), ship[1] + math.sin(a) * (d + 26 * k) * 0.8))
    for i in range(150):                                          # loose spirals
        a = i * 0.41
        bullets.append((ship[0] + math.cos(a) * (180 + 5.2 * i), ship[1] + math.sin(a) * (180 + 5.2 * i) * 0.7))

    dias = []
    for bx_, by_ in bullets:
        if in_view(bx_, by_, 10):
            count["bullets"] += 1
            dias += ld.transform(ld.polygon(4), bx_, by_, 7.5)
            L["bullet"].glow(bx_, by_, 13, danger, 0.35)
    L["bullet"].lines(dias, danger, 2.4, 2.2)
    L["bullet"].threat(dias, danger + white * 0.25, 2.1, 1.5)
    L["bullet"].lines(lethal, danger, 2.4, 2.2)
    L["bullet"].threat(lethal, danger + white * 0.25, 2.1, 1.5)

    # ------------------------------------------------------------ kills: sparks, rings, flashes, pools
    spark_count, rings = 0, []
    recipe = {"small": ((14, 20), (180, 520), (0.35, 0.7)), "medium": ((36, 48), (200, 640), (0.4, 0.9)),
              "large": ((90, 120), (150, 900), (0.4, 1.2))}
    for kx, ky, size, age, k in kills:
        (n0, n1), (v0, v1), (l0, l1) = recipe[size]
        for _ in range(rng.randint(n0, n1)):
            life = rng.uniform(l0, l1)
            if age >= life:
                continue
            a, v = rng.uniform(0, 2 * PI), rng.uniform(v0, v1) * WU
            dist = v / 3.2 * (1.0 - math.exp(-3.2 * age))
            ln_ = max(2.0, min(v * math.exp(-3.2 * age) * 0.035, 26 * WU))
            u, w_ = rng.random(), rng.random() * 0.4
            colour = (hue[k] * (1 - u) + mate[k] * u) * (1 - w_) + white * w_
            seg = (kx + math.cos(a) * dist, ky + math.sin(a) * dist, kx + math.cos(a) * (dist + ln_), ky + math.sin(a) * (dist + ln_))
            L["fx"].lines([seg], ld.f_linear(seg[0], seg[1], seg[2], seg[3], [colour * 0.5, colour * 0.6 + white * 0.4]),
                          rng.choice([2.4, 3.4, 4.6, 5.6]) * (1.0 - age / life) ** 1.6, 1.4)
            spark_count += 1
        life = l1
        if size == "small":
            if rules["smallFlash"] == "pool":
                L["flash"].glow(kx, ky, 33 * WU, white, 0.55 + (0.05 - 0.55) * age / life)
            elif age < 0.09:
                L["flash"].glow(kx, ky, 14.0, white, 1.4 * (1.0 - age / 0.09))
            if rules["smallRing"] and age < 0.18:
                rings.append((age, kx, ky, 48 * WU * age / 0.18, k))
        else:
            big = size == "large"
            flash_s = rules["flashS"]
            if flash_s is None:
                L["flash"].glow(kx, ky, 33 * WU, white, 0.55 + (0.05 - 0.55) * age / life)
            elif age < flash_s:
                L["flash"].glow(kx, ky, 33 * WU * (1.5 if big else 1.0), white, 0.55 * (1.0 - age / flash_s) ** 2)
            t = age / life
            r0, r1, i0, i1 = (150, 250, 0.32, 0.05) if big else (100, 180, 0.26, 0.04)
            L["pool"].glow(kx, ky, (r0 + (r1 - r0) * t) * WU, hue[k], i0 + (i1 - i0) * t)
            for j in range(3 if big else 2):
                ra = age - 0.06 * j
                if 0 <= ra < 0.2:
                    rings.append((ra, kx, ky, (150 if big else 100) * WU * ra / 0.2, k))
            for _ in range(10):
                if age < 1.2:
                    a, d = rng.uniform(0, 2 * PI), rng.uniform(10, 60 + 150 * age)
                    L["fx"].glow(kx + math.cos(a) * d, ky + math.sin(a) * d, 4.0, mate[k], 1.2 * (1 - age / 1.2))
    rings.sort()
    for age, kx, ky, radius, k in rings[:rules["maxRings"]]:
        if radius > 4:
            ring = ld.transform(ld.polygon(max(24, int(radius / 3))), kx, ky, radius)
            L["fx"].lines(ring, ld.f_angular(kx, ky, [white, hue[k], white, mate[k]]), 1.3, 1.6)

    # warp-ins: every kill is replaced, each new enemy arriving over two beats
    warps = 0
    for _ in range(int(round(46 * 2))):
        x, y = rng.uniform(40, W - 40), rng.uniform(40, H - 40)
        small = rng.random() < 40 / 46.0
        k = rng.choice([0, 0, 1, 2])
        if rules["warpFixedWu"]:
            start = rules["warpFixedWu"] * WU
        else:
            start = 2.5 * (R * 0.55 * 1.2 if small else R * 1.25)
        radius = start * (1.0 - rng.random())
        if radius > 3:
            L["warp"].lines(ld.transform(ld.polygon(max(16, int(radius / 3))), x, y, radius), hue[k], 1.3, 1.6)
            warps += 1

    # Shards in flight to the ship
    shard = [(s[0], -s[1], s[2], -s[3]) for s in glyphs["shard"]["segments"]]
    shards = 0
    for i in range(600):
        kx, ky = kills[i % len(kills)][:2]
        t = rng.random()
        x = kx + (ship[0] - kx) * t + rng.uniform(-60, 60) * (1 - t)
        y = ky + (ship[1] - ky) * t + rng.uniform(-60, 60) * (1 - t)
        if in_view(x, y, 0):
            L["shard"].lines(ld.transform(shard, x, y, 5.2, rng.uniform(0, PI)), gold * 0.6 + white * 0.4, 3.2, 1.3)
            L["shard"].glow(x, y, 9.0, gold, 0.5)
            shards += 1

    for wx, wy in wells:                                          # gravity wells
        L["fx"].glow(wx, wy, 150, hue[2], 0.16)
        for k, rr in enumerate((62, 44, 28, 14)):
            L["fx"].lines(ld.transform(ld.arc(1.0, 0.5 * k, 0.5 * k + 5.3, 30), wx, wy, rr),
                          ld.f_angular(wx, wy, [hue[2], mate[2], white, mate[2]], 0.7 * k), 1.3 + 0.4 * k, 1.5)
        L["fx"].glow(wx, wy, 12, white, 1.6)

    # ------------------------------------------------------------ the player, full build
    P = L["player"]
    P.glow(ship[0], ship[1], 150, p1, 0.13)
    hull = [(s[0], -s[1], s[2], -s[3]) for s in glyphs["ship"]["segments"]]
    hull_px = ld.transform(hull, ship[0], ship[1], R, aim)
    P.lines(hull_px, ld.f_radial(ship[0], ship[1], R * 1.3, [white, p1, deep]), 2.8, 2.3)
    P.lines(hull_px, white, 2.0, 0.8)
    halo = ld.transform([(s[0], -s[1], s[2], -s[3]) for s in glyphs["halo_3"]["segments"]], ship[0], ship[1], R, 0.35)
    P.lines(halo, ld.f_angular(ship[0], ship[1], [p1, white, p1, light]), 1.5, 1.6)
    if rules["priorityShip"]:
        P.threat(halo, p1 * 0.75 + white * 0.25, 1.6, 1.2)
        P.threat(hull_px, light * 0.55 + white * 0.45, 2.3, 2.0)
    bx, by = -math.cos(aim + 0.5), -math.sin(aim + 0.5)
    trail = [(ship[0] + bx * (18 + 10 * k), ship[1] + by * (18 + 10 * k), ship[0] + bx * (28 + 10 * k), ship[1] + by * (28 + 10 * k)) for k in range(13)]
    P.lines(trail, ld.f_fade(ld.f_linear(ship[0], ship[1], ship[0] + bx * 150, ship[1] + by * 150, [white, p1, deep]), ship[0] + bx * 18, ship[1] + by * 18, 150, 1.1), 2.4, 2.6)
    shots = 0
    for origin in ((0.0, 0.0), (-math.sin(aim) * 90, math.cos(aim) * 90)):       # the ship and its wingman
        for lane in (-0.24, -0.12, 0.0, 0.12, 0.24):
            for k in range(16):
                d = 44 + k * 44
                a = aim + lane
                x, y = ship[0] + origin[0] + math.cos(a) * d, ship[1] + origin[1] + math.sin(a) * d
                hx, hy = x + math.cos(a) * 24, y + math.sin(a) * 24
                P.lines([(x, y, hx, hy)], ld.f_linear(x, y, hx, hy, [p1, white]), 4.6, 2.2)
                P.glow(hx, hy, 9, white, 0.7)
                shots += 1
    for k in range(5):                                            # Orbitals
        a = 0.8 + k * 2 * PI / 5
        ox, oy = ship[0] + math.cos(a) * 62, ship[1] + math.sin(a) * 62
        P.lines(ld.transform(ld.arc(1.0, a - 0.9, a, 8), ship[0], ship[1], 62), ld.f_fade(ld.f_solid(p1), ox, oy, 60, 1.0), 2.0, 2.0)
        P.lines(ld.transform(shard, ox, oy, 8.5, a), white * 0.5 + p1 * 0.5, 3.4, 1.7)
        P.glow(ox, oy, 14, p1, 0.7)
    for radius in (150, 330):                                     # double Pulse Ring
        P.lines(ld.transform(ld.polygon(96), ship[0], ship[1], radius), ld.f_angular(ship[0], ship[1], [p1, white, p1, light, p1, white]), 2.2, 2.6)
        P.lines(ld.transform(ld.polygon(96), ship[0], ship[1], radius - 18), p1, 0.7, 1.4)
    dx_, dy_ = ship[0] - 70, ship[1] - 78                         # Sentinel and its burst of three
    P.lines(ld.transform(ld.polygon(4), dx_, dy_, 9, 0.2), ld.f_radial(dx_, dy_, 9, [white, p1]), 2.8, 1.7)
    P.glow(dx_, dy_, 16, p1, 0.6)
    for k in range(3):
        sx, sy = dx_ - 40 - 60 * k, dy_ - 52 - 78 * k
        P.lines([(sx, sy, sx - 22, sy - 28)], ld.f_linear(sx, sy, sx - 22, sy - 28, [p1, white]), 3.8, 1.9)
    sa = aim + 2.2                                                # Sweeper arm, 600 wu
    sweep = [(ship[0] + math.cos(sa) * (40 + 86 * k), ship[1] + math.sin(sa) * (40 + 86 * k),
              ship[0] + math.cos(sa) * (40 + 86 * (k + 1)), ship[1] + math.sin(sa) * (40 + 86 * (k + 1))) for k in range(10)]
    P.lines(sweep, ld.f_linear(ship[0], ship[1], ship[0] + math.cos(sa) * 900, ship[1] + math.sin(sa) * 900, [white, p1, deep]), 2.2, 2.6)
    if lancer:                                                    # Lancer beam: on screen 150 ms in every two beats
        la = aim + 0.4
        P.lines([(ship[0] - math.cos(la) * 3000, ship[1] - math.sin(la) * 3000, ship[0] + math.cos(la) * 3000, ship[1] + math.sin(la) * 3000)],
                p1 * 0.5 + white * 0.5, 4.0, 6.0)

    info = dict(count, kills=len(kills), sparks=spark_count, rings=min(len(rings), rules["maxRings"]), warps=warps, shards=shards,
                shots=shots, grid=grid_stats)
    return L, base, info


def rasterise(L):
    """Engine picture of every layer: {class: hdr}, plus the priority layer (rims, cores)."""
    hdr, rims, cores = {}, [], []
    for name in sorted(L, key=lambda n: n != "player"):          # ship cores first, threat cores over them
        h, r, c = L[name].engine_hdr()
        hdr[name] = h
        rims += r
        cores += c
    return hdr, rims, cores


def load_of(hdr):
    """Mean linear luminance of each layer's light: what 'light load' is made of."""
    lum = np.array([0.2126, 0.7152, 0.0722], np.float32)
    return {name: float((h @ lum).mean()) for name, h in hdr.items()}


def compose(base, hdr, rims, cores, gain=1.0, gains=None, **look):
    """Scene = fixed light + crowd light x gain. `gains` overrides the gain of single classes."""
    gains = gains or {}
    total = np.zeros_like(hdr["grid"])
    for name, h in hdr.items():
        if name == "heat_rest":
            continue
        g = gains.get(name, gain if name in CROWD else 1.0)
        total += h * g
        if name == "heat":
            total += hdr["heat_rest"] * (1.0 - g)               # a line that is not heated is at rest
    return base._composite(total, rims, cores, **look), total


def crowd_gain(enemies, bullets):
    """The light budget of design/look.md §3: (crowd gain, swarm gain) for what is in view."""
    gain = min(1.0, max(0.4, (150.0 / max(enemies + bullets / 4.0, 1.0)) ** 0.4))
    return gain, gain ** 1.5


def stats(im):
    d48, d96, hot, lum = sc.light_numbers(im)
    return "dark<48 %4.1f%%  dark<96 %4.1f%%  >=200 %4.1f%%  luminance %.4f" % (d48 * 100, d96 * 100, hot * 100, lum)


def blur(img, sigma):
    """Gaussian blur of an HDR image (what one bloom level holds)."""
    pad = int(sigma * 3) + 2
    p = np.pad(img, ((pad, pad), (pad, pad), (0, 0)), mode="edge")
    hh, ww = p.shape[:2]
    g = np.exp(-2.0 * (math.pi ** 2) * (sigma * sigma) * (np.fft.rfftfreq(ww)[None, :] ** 2 + np.fft.fftfreq(hh)[:, None] ** 2))
    out = np.stack([np.fft.irfft2(np.fft.rfft2(p[..., c]) * g, s=(hh, ww)) for c in range(3)], axis=-1)
    return np.maximum(out[pad:-pad, pad:-pad], 0.0).astype(np.float32)


def afterglow(moving, velocity=(4, -3), tau_ms=110.0, strength=0.6, sigma=1.5, knee=0.0, frames=48, hz=60.0):
    """What the afterglow adds when everything in `moving` drifts across the screen at `velocity`
    pixels per frame (the bench scenes: formations ride with the ship over a world-locked lattice).
    history = max(source, history x decay); the picture gains strength x max(history x decay - source, 0).
    source = the frame blurred to `sigma`, less `knee` (only light above the knee persists)."""
    src = blur(moving, sigma)
    if knee > 0:
        m = src.max(axis=2, keepdims=True)
        src = src * np.maximum(m - knee, 0.0) / np.maximum(m, 1e-6)
    decay = math.exp(-1000.0 / hz / tau_ms)
    hist = np.zeros_like(src)
    for k in range(frames, 0, -1):                                # oldest frame first
        old = np.roll(src, (k * velocity[1], k * velocity[0]), axis=(0, 1))   # where the picture was k frames ago
        hist = np.maximum(old, hist) * decay
    return strength * np.maximum(hist - src, 0.0)


SOFT = dict(tau_ms=110, strength=0.7, sigma=6.0, knee=0.25)       # the afterglow of look.md §7
STUDY = [  # name, rules, light budget on, afterglow settings or None
    ("v2", OLD, False, None),
    ("v2_1_rules_only", NEW, False, None),
    ("v2_1", NEW, True, None),
    ("v2_1_afterglow_first_version", NEW, True, dict(tau_ms=110, strength=0.6, sigma=1.5, knee=0.0)),
    ("v2_1_afterglow", NEW, True, SOFT),
]
CROP = (760, 250, 1720, 850)


def picture(base, hdr, rims, cores, info, budget, glow=None):
    """One composite: with the light budget for what is in view, and with the afterglow the scene
    would carry if everything but the lattice drifted at 300 px/s."""
    gain, swarm = crowd_gain(info["enemies"], info["bullets"]) if budget else (1.0, 1.0)
    im, total = compose(base, hdr, rims, cores, gain, {"bullet": swarm, "shard": swarm}, bloom=fm.BLOOM)
    if glow:
        still = hdr["grid"] + hdr["heat"] * gain + hdr["heat_rest"] * (1.0 - gain)
        bg = base.bg
        base.bg = bg + afterglow(total - still, **glow)       # added after the bloom, like the background
        im = base._composite(total, rims, cores, bloom=fm.BLOOM)
        base.bg = bg
    return im, gain, swarm


def study(folder, only=None):
    os.makedirs(folder, exist_ok=True)
    cache = {}
    for name, rules, budget, glow in STUDY:
        if only and name not in only:
            continue
        if id(rules) not in cache:
            L, base, info = build(rules)
            cache[id(rules)] = (base,) + rasterise(L) + (info,)
            print(name, info)
            load = load_of(cache[id(rules)][1])
            print("   load: " + "  ".join("%s %.4f" % (k, v) for k, v in sorted(load.items(), key=lambda kv: -kv[1])))
        im, gain, swarm = picture(*cache[id(rules)], budget, glow)
        im.save(os.path.join(folder, name + ".png"))
        im.resize((1600, 720), Image.LANCZOS).save(os.path.join(folder, name + ".jpg"), quality=88)
        im.crop(CROP).save(os.path.join(folder, name + ".crop.jpg"), quality=90)
        print("%-30s gain %.2f swarm %.2f  %s" % (name, gain, swarm, stats(im)))


def main():
    if len(sys.argv) > 2 and sys.argv[1] == "study":
        study(sys.argv[2], sys.argv[3:])
        return
    previews = os.path.join(fm.DESIGN, "previews")
    rows = []
    for label, rules, budget in (("LOOK V2: the numbers tuned on 130 enemies, at endgame density", OLD, False),
                                 ("LOOK V2.1: light budget, ship on the priority layer, kill and warp-in recipes for crowds", NEW, True)):
        L, base, info = build(rules)
        hdr, rims, cores = rasterise(L)
        im, gain, swarm = picture(base, hdr, rims, cores, info, budget)
        print(label)
        print("   %s" % info)
        print("   crowd gain %.2f  swarm gain %.2f  %s" % (gain, swarm, stats(im)))
        d48, _, _, lum = sc.light_numbers(im)
        rows.append(("%s   -   %d enemies, %d bullets, %d kills in flight   -   %.0f%% dark, luminance %.3f" % (
            label, info["enemies"], info["bullets"], info["kills"], d48 * 100, lum), im))
        if budget:
            im.save(os.path.join(previews, "frame_endgame_ref.png"), optimize=True)
            print("wrote", os.path.join(previews, "frame_endgame_ref.png"))
    w, h, strip = 1600, 720, 30
    sheet = Image.new("RGB", (w, (h + strip) * len(rows)), (0, 0, 0))
    draw = ImageDraw.Draw(sheet)
    for i, (label, im) in enumerate(rows):
        draw.text((10, i * (h + strip) + 9), label, fill=(215, 220, 235))
        sheet.paste(im.resize((w, h), Image.LANCZOS), (0, i * (h + strip) + strip))
    sheet.save(os.path.join(previews, "crowd_rules.png"), optimize=True)
    print("wrote", os.path.join(previews, "crowd_rules.png"))


if __name__ == "__main__":
    main()
