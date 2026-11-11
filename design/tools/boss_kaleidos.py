"""RAVEFORM boss 1, KALEIDOS: parts, numbers and patterns (edit here), export, preview.

Kaleidos is a turning kaleidoscope wheel: six arms in three mirrored pairs around a shuttered core.
Everything it does sits on the bar grid, and what it teaches is the game's whole rhythm language:
attacks land on the beat, the thing that is not firing is the thing to hit, and a phrase has a shape.

Usage:  python design/tools/boss_kaleidos.py   -> design/bosses/kaleidos.json, design/previews/boss_kaleidos.png
"""
import json
import math
import os
import sys

import numpy as np

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")
PI = math.pi

# --------------------------------------------------------------------------- the design (world units)

BPM = 120.0
BEAT = 60.0 / BPM
BAR = 4 * BEAT
FACETS = [(104.0, 52.0, 22.0), (157.0, 44.0, 18.0), (202.0, 36.0, 15.0), (240.0, 30.0, 12.0)]   # centre radius, length, half width
TIP_RADIUS = 264.0
LENS_RADIUS, LENS_HALF_ANGLE = 66.0, 26.0
CORE_RINGS = [(44.0, 0.0), (31.0, 30.0), (18.0, 0.0)]            # radius, phase in degrees
BULLET_SPEED = 200.0
BULLET_LIFE = 5.0
BOSS_ACCENT = (0.83, 76.0)   # OKLCH lightness and hue: amber-gold, the complement of the Ultraviolet palette
# The kaleidoscope: faint copies of the arms behind the wheel (scale, turn in degrees, line intensity).
REFLECTIONS = [(1.6, 30.0, 0.22), (2.4, 0.0, 0.10)]

NUMBERS = dict(
    radius=270.0, contactRadiusCore=46.0,
    pairHealth=400, coreHealth=600,
    damageScale=dict(firing=1.0 / 3.0, resting=3.0, lens=0.0, coreExposed=1.0, coreDownbeat=3.0),
    drift=dict(speed=30.0, keepDistance=[450.0, 650.0]),
    bulletSpeed=BULLET_SPEED, bulletLifeSeconds=BULLET_LIFE,
    phases=[
        dict(name="Bloom", pairsAlive=3, turnBars=8, direction=1,
             schedule="each bar one pair rests in turn (A, B, C); the other two fire",
             fire=dict(step="1/8", fan=3, spreadDeg=14.0)),
        dict(name="Refraction", pairsAlive=2, turnBars=4, direction=-1,
             schedule="the two pairs alternate bars; the pair that rested opens its bar with a beam",
             fire=dict(step="1/8", fan=5, spreadDeg=14.0),
             beam=dict(telegraphBeats=4, fireBeats=1, widthWu=10.0, note="along the arm, out to the arena wall; it sweeps as the wheel turns")),
        dict(name="Spiral", pairsAlive=1, turnBars=2, direction=1,
             schedule="fires for three beats, rests on beat 4 of every bar",
             fire=dict(step="1/16", fan=1, spreadDeg=0.0),
             coreBurst=dict(on="beat 1 of every bar", count=24)),
        dict(name="Naked core", pairsAlive=0, turnBars=2, direction=-1,
             schedule="the core fires on every beat; from the ninth bar on every eighth",
             coreBurst=dict(on="every beat, alternate bursts turned half a step", count=16)),
    ],
    music=dict(
        layer="glass bells (the P3 bell patch), one voice per pair: A plays the chord root, B the third, C the fifth",
        rule="a pair's bell sounds on every step it fires, so the fans are heard as an ostinato and the resting pair is a hole in it",
        onPairDeath="that voice leaves the chord for good; a low growl (the elite-arrives patch) enters one octave down",
        coreDownbeat="a deep gong on beat 1 while the iris is open"),
    accent=dict(oklchLightness=BOSS_ACCENT[0], oklchHue=BOSS_ACCENT[1], chromaFraction=0.96,
                use="core rings, lens plates, arm tips and spines: a boss is the one enemy with a second colour family, "
                    "the complement of its Track's palette"),
    reflections=[dict(scale=sc, turnDeg=tu, intensity=it, note="arm hulls only, behind the wheel, never on the priority layer")
                 for sc, tu, it in REFLECTIONS],
    entrance="on the Drop's downbeat six facets streak in along the mirror axes and lock together over one bar, inner to outer; the name card KALEIDOS holds for two bars",
    death="iris wide open, clamped white flash, every facet tumbles outward in six-fold symmetry, three rings, 300+ sparks, 30 px distortion ring, 50 ms hit-stop, then 200 Shards",
)


def kite(length, half_width):
    return [(-length / 2, 0.0), (-length / 2 + 0.38 * length, half_width), (length / 2, 0.0), (-length / 2 + 0.38 * length, -half_width)]


def facet_layers(length, half_width):
    """Arm-local shapes (x outward). 0 hull, 1 mirrored inner chevron, 2 spine."""
    k = kite(length, half_width)
    hull = ld.polyline(k, closed=True)
    inner = ld.polyline([(k[1][0] * 0.55 + 0.1 * length, k[1][1] * 0.55), (k[0][0] * 0.55, 0.0), (k[3][0] * 0.55 + 0.1 * length, k[3][1] * 0.55)])
    spine = [(-length * 0.18, 0.0, length * 0.34, 0.0)]
    return hull, inner, spine, k


def build():
    parts = {}
    for j, (r, length, hw) in enumerate(FACETS):
        hull, inner, spine, _ = facet_layers(length, hw)
        segs = [[round(v, 3) for v in s] + [0] for s in hull] + [[round(v, 3) for v in s] + [1] for s in inner] + [[round(v, 3) for v in s] + [2] for s in spine]
        parts["facet_%d" % j] = dict(armRadius=r, length=length, halfWidth=hw, glass=True, segments=segs)
    parts["tip"] = dict(armRadius=TIP_RADIUS, segments=[[round(v, 3) for v in s] + [0] for s in ld.star(4, 10.0, 3.8)])
    parts["lens"] = dict(radius=LENS_RADIUS, halfAngleDeg=LENS_HALF_ANGLE, widthRefPx=3.0, innerRadius=59.0,
                         note="two plates per pair, centred on its arms; they stop shots; they shatter with the pair")
    parts["core"] = dict(rings=[dict(radius=r, phaseDeg=p, sides=6) for r, p in CORE_RINGS],
                         iris=dict(shutters=6, closedRadius=4.0, openRadius=13.0, note="open for the whole of beat 1 of every bar"),
                         healthRing=dict(radius=52.0, note="an arc that shortens clockwise from the top as the core loses health"))
    return dict(
        note="Generated by design/tools/boss_kaleidos.py. World units, boss space. Arms at k x 60 degrees, k = 0..5; "
             "pair = k mod 3 (A, B, C); the two arms of a pair share one health pool and shatter together. "
             "Facet segments are arm-local (x outward along the arm) as [x0,y0,x1,y1,layer]. Colours: pair A, B, C "
             "take the run's enemy hues 1, 2, 3 with their partners, drawn as elites (white copy, glass, flares); "
             "the resting pair is drawn white-hot.",
        bpm=BPM, arms=6, parts=parts, numbers=NUMBERS)


# --------------------------------------------------------------------------- preview

def load():
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    pal = next(p for p in pals["palettes"] if p["id"] == "ultraviolet")
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    return pals, pal, glyphs, font


class Art:
    def __init__(self):
        self.pals, self.pal, self.glyphs, self.font = load()
        lin = lambda e: np.asarray(e["linear"], np.float32)
        cands = [self.pal["enemyCandidates"][i] for i in self.pal["soloEnemies"]]
        self.hue = [lin(c) for c in cands]
        self.mate = [lin(c["partners"][c["soloPartner"]]) for c in cands]
        self.white = np.ones(3, np.float32)
        self.accent = lin(self.pal["accent"])
        self.danger = lin(self.pals["constants"]["danger"])
        p = self.pals["constants"]["players"][0]
        self.p1, self.p1_deep, self.p1_light = lin(p), lin(p["deep"]), lin(p["light"])
        self.gold = lin(self.pals["constants"]["pickup"])
        self.amber = ld.oklch(BOSS_ACCENT[0], BOSS_ACCENT[1], cf=0.96)[0].astype(np.float32)   # the boss's own colour family
        self.grid = lin(self.pal["grid"])
        self.bg0, self.bg1 = lin(self.pal["bg0"]), lin(self.pal["bg1"])


def arm_xy(cx, cy, r, ang, s):
    return cx + math.cos(ang) * r * s, cy + math.sin(ang) * r * s


def draw_boss(cv, a, cx, cy, s, phi, alive, resting, gain=1.0, ghost=False, iris_open=0.0, core_health=1.0):
    """s = pixels per world unit. phi = wheel angle in radians (screen space, y down)."""
    for k in range(6):
        pair = k % 3
        if not alive[pair]:
            continue
        ang = phi + k * PI / 3
        base, part = a.hue[pair], a.mate[pair]
        soft = (resting == pair) and not ghost
        tipx, tipy = arm_xy(cx, cy, TIP_RADIUS, ang, s)
        innerx, innery = arm_xy(cx, cy, FACETS[0][0] - FACETS[0][1] / 2, ang, s)
        field = ld.f_linear(innerx, innery, tipx, tipy, [base * 0.6 + a.white * 0.4, base, part])
        for r, length, hw in FACETS:
            hull, inner, spine, k_pts = facet_layers(length, hw)
            x, y = arm_xy(cx, cy, r, ang, s)
            if not ghost:
                cv.fill(ld.transform_points(k_pts, x, y, s, ang), a.white if soft else base, 0.16 if soft else 0.10)
            cv.lines(ld.transform(hull, x, y, s, ang), field, 2.6 * gain, 2.3 if not ghost else 1.3)
            if ghost:
                continue
            cv.lines(ld.transform(inner, x, y, s, ang), a.accent * 0.6 + part * 0.4, 2.0, 1.5)
            cv.lines(ld.transform(spine, x, y, s, ang), a.amber * 0.6 + a.white * 0.4, 2.8, 1.5)
            cv.lines(ld.transform(hull, x, y, s, ang), a.white, 2.4 if soft else 1.0, 1.6 if soft else 0.9)
        if ghost:
            continue
        cv.lines(ld.transform(ld.star(4, 10.0, 3.8), tipx, tipy, s, ang), a.white if soft else a.amber, 3.2, 1.7)
        cv.glow(tipx, tipy, 18 * s, a.white if soft else a.amber, 0.9 if soft else 0.6)
        if soft:
            cv.flare(tipx, tipy, 70 * s, a.white, 1.4, 1.3, 2, ang)
        # lens plates
        a0 = ang - math.radians(LENS_HALF_ANGLE)
        plate = ld.transform(ld.arc(LENS_RADIUS, a0, a0 + 2 * math.radians(LENS_HALF_ANGLE), 10), cx, cy, s)
        cv.lines(plate, ld.f_angular(cx, cy, [a.amber, a.white, a.amber, base], phi), 2.6, 3.0)
        cv.lines(ld.transform(ld.arc(59.0, a0 + 0.05, a0 + 2 * math.radians(LENS_HALF_ANGLE) - 0.05, 8), cx, cy, s), a.amber, 1.2, 1.2)
    if ghost:
        return
    # core: three counter-turning hexagons, shutters, seed, health arc
    wheel = [a.amber, a.white, a.hue[0], a.white, a.amber, a.hue[2]]
    for i, (r, phase) in enumerate(CORE_RINGS):
        rot = math.radians(phase) + phi * (-2.0, 3.0, -4.0)[i]
        cv.lines(ld.transform(ld.polygon(6, r, rot), cx, cy, s), ld.f_angular(cx, cy, wheel, rot), 2.4 + 0.4 * i, 2.2 - 0.2 * i)
    cv.fill(ld.transform_points([(CORE_RINGS[0][0] * math.cos(i * PI / 3), CORE_RINGS[0][0] * math.sin(i * PI / 3)) for i in range(6)], cx, cy, s, phi * -2.0), a.amber, 0.08)
    reach = 4.0 + (13.0 - 4.0) * iris_open
    rot = phi * -4.0
    shutters = [(18.0 * math.cos(rot + i * PI / 3), 18.0 * math.sin(rot + i * PI / 3), reach * math.cos(rot + i * PI / 3 + 0.5), reach * math.sin(rot + i * PI / 3 + 0.5)) for i in range(6)]
    cv.lines(ld.transform(shutters, cx, cy, s), a.white, 2.2, 1.4)
    cv.glow(cx, cy, (10 + 22 * iris_open) * s, a.white, 1.2 + 2.2 * iris_open)
    cv.glow(cx, cy, 330 * s, a.hue[0] * 0.5 + a.hue[2] * 0.5, 0.12)
    if core_health > 0:
        cv.lines(ld.transform(ld.arc(52.0, -PI / 2, -PI / 2 + 2 * PI * core_health, int(48 * core_health) + 2), cx, cy, s), a.gold, 1.8, 1.5)


def reflections(cv, a, cx, cy, s, phi, alive):
    """The kaleidoscope: two faint, larger, turned copies of the wheel behind it."""
    draw_boss(cv, a, cx, cy, s * 1.6, phi + PI / 6, alive, None, gain=REFLECTIONS[0][2] / 2.6, ghost=True)
    draw_boss(cv, a, cx, cy, s * 2.4, phi, alive, None, gain=REFLECTIONS[1][2] / 2.6, ghost=True)


def clip_to(x0, y0, x1, y1, left, top, right, bottom):
    """Shorten a segment that starts inside a rectangle so that it ends on the rectangle's edge."""
    t = 1.0
    if x1 > right:
        t = min(t, (right - x0) / (x1 - x0))
    if x1 < left:
        t = min(t, (left - x0) / (x1 - x0))
    if y1 > bottom:
        t = min(t, (bottom - y0) / (y1 - y0))
    if y1 < top:
        t = min(t, (top - y0) / (y1 - y0))
    return (x0, y0, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t)


def volley(phi_now, turn_bars, direction, schedule, fan, spread, step_beats, now_beats, cx, cy, s):
    """Positions of the bullets alive at `now_beats`. schedule(pair, bar, beat_in_bar) -> fires?"""
    out = []
    steps = int(BULLET_LIFE / (step_beats * BEAT))
    omega = direction * 2 * PI / (turn_bars * BAR)
    for i in range(1, steps + 1):
        t_beats = now_beats - i * step_beats
        if t_beats < 0:
            break
        age = i * step_beats * BEAT
        bar, beat = int(t_beats // 4), t_beats % 4
        phi = phi_now - omega * age
        for k in range(6):
            if not schedule(k % 3, bar, beat):
                continue
            ang = phi + k * PI / 3
            tx, ty = arm_xy(cx, cy, TIP_RADIUS, ang, s)
            for f in range(fan):
                d = ang + math.radians(spread) * (f - (fan - 1) / 2.0)
                out.append((tx + math.cos(d) * BULLET_SPEED * age * s, ty + math.sin(d) * BULLET_SPEED * age * s))
    return out


def draw_bullets(cv, a, pts, size, w, h):
    dias = []
    for x, y in pts:
        if -20 < x < w + 20 and -20 < y < h + 20:
            dias += ld.transform(ld.polygon(4), x, y, size)
            cv.glow(x, y, size * 1.8, a.danger, 0.35)
    cv.lines(dias, a.danger, 2.4, 2.2)
    cv.threat(dias, a.danger + a.white * 0.25, 2.1, 1.5)


def draw_ship(cv, a, glyphs, x, y, r, aim):
    hull = [(g[0], -g[1], g[2], -g[3]) for g in glyphs["ship"]["segments"]]
    halo = [(g[0], -g[1], g[2], -g[3]) for g in glyphs["halo_3"]["segments"]]
    cv.glow(x, y, r * 7, a.p1, 0.13)
    px = ld.transform(hull, x, y, r, aim)
    cv.lines(px, ld.f_radial(x, y, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.3)
    cv.lines(px, a.white, 2.0, 0.8)
    cv.lines(ld.transform(halo, x, y, r, 0.35), ld.f_angular(x, y, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.6)


def label(cv, a, text, x, y, size=11, colour=None, to=None):
    cv.text(a.font, text, x, y, size, a.white if colour is None else colour, 1.0, 1.4)
    if to:
        cv.lines([(x - 8, y + size * 0.5, to[0], to[1])], a.white, 0.45, 1.1)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "kaleidos.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)

    a = Art()
    W, H = 2400, 1500
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.5, a.bg1 * 0.5, cx=0.56, cy=0.36)
    cv.nebula([(1380, 540, 900, a.hue[0], 0.030), (500, 300, 600, a.hue[1], 0.022), (2100, 900, 600, a.hue[2], 0.022)])

    # ---------------- main picture: phase 1, bar 3 (pair C rests), sixth eighth of the bar, zoom 0.85
    s = 1.5 * 0.85
    cx, cy = 1380.0, 540.0
    grid = []
    for gx in range(0, W + 1, 38):
        grid.append((gx, 0, gx, 1080))
    for gy in range(0, 1081, 38):
        grid.append((0, gy, W, gy))
    cv.lines(grid, a.grid, 0.22, 1.0)
    now = 2 * 4 + 2.5                                   # beats: bar index 2, 2.5 beats in
    phi = -(2 * PI / (8 * BAR)) * now * BEAT * -1.0     # clockwise on screen
    alive = [True, True, True]
    rest = lambda pair, bar: bar % 3 == pair
    bullets = volley(phi, 8, 1, lambda pair, bar, beat: not rest(pair, bar), 3, 14.0, 0.5, now, cx, cy, s)
    reflections(cv, a, cx, cy, s, phi, alive)
    draw_boss(cv, a, cx, cy, s, phi, alive, resting=2, iris_open=0.0, core_health=1.0)
    draw_bullets(cv, a, bullets, 7.0, W, 1080)
    # the ship sits in the lane of the resting pair and pours fire into it
    c_ang = phi + 2 * PI / 3 + PI                        # the far arm of pair C points down-left
    sx, sy = arm_xy(cx, cy, 455.0, c_ang, s)
    aim = c_ang + PI
    draw_ship(cv, a, a.glyphs, sx, sy, 21 * 0.85, aim)
    for lane in (-0.05, 0.0, 0.05):
        for k in range(7):
            d = 40 + k * 56
            x0, y0 = sx + math.cos(aim + lane) * d, sy + math.sin(aim + lane) * d
            x1, y1 = x0 + math.cos(aim + lane) * 20, y0 + math.sin(aim + lane) * 20
            cv.lines([(x0, y0, x1, y1)], ld.f_linear(x0, y0, x1, y1, [a.p1, a.white]), 4.6, 2.2)
            cv.glow(x1, y1, 9, a.white, 0.7)
    hx, hy = arm_xy(cx, cy, TIP_RADIUS - 6, c_ang, s)
    cv.glow(hx, hy, 60, a.white, 0.8)
    cv.flare(hx, hy, 90, a.white, 2.2, 1.4, 4, 0.4)
    cv.text(a.font, "X3", hx - 70, hy + 40, 16, a.gold, 2.0, 2.0)

    name, end = ld.text_segments(a.font, "KALEIDOS", 60, 46, 54)
    cv.lines(name, ld.f_linear(60, 0, end, 0, [a.hue[1], a.white, a.hue[2]]), 2.0, 4.6)
    cv.text(a.font, "BOSS 1   TRACK 1 FIRST LIGHT   120 BPM", 62, 122, 13, a.accent, 1.0, 1.5)
    cv.text(a.font, "PHASE 1  BLOOM", 62, 156, 16, a.white, 1.2, 1.8)
    tipA = arm_xy(cx, cy, TIP_RADIUS, phi, s)
    label(cv, a, "FIRING PAIRS: FAN OF 3 ON EVERY 1/8  ARMOURED X1/3", 1500, 60, 11, a.hue[0], (tipA[0] + 10, tipA[1] - 10))
    label(cv, a, "RESTING PAIR GLOWS WHITE: SOFT, TRIPLE DAMAGE", hx + 150, hy - 6, 11, a.white, (hx + 30, hy))
    plate_xy = arm_xy(cx, cy, LENS_RADIUS + 6, phi + PI / 3, s)
    label(cv, a, "LENS PLATES SHIELD THE CORE UNTIL THEIR PAIR DIES", plate_xy[0] + 330, plate_xy[1] - 60, 11, a.amber, plate_xy)
    label(cv, a, "SAFE LANES TURN WITH THE WHEEL: ONE TURN IN 8 BARS", 60, 240, 11, a.accent, None)
    label(cv, a, "MIRRORED PAIRS SHARE ONE HEALTH POOL", 60, 268, 11, a.accent, None)

    # ---------------- phrase diagram: who fires when (phase 1)
    ox, oy, cw, ch = 60, 320, 30, 30
    cv.text(a.font, "PHASE 1 PHRASE  3 BARS", ox, oy - 34, 11, a.white, 0.9, 1.4)
    for row, nm in enumerate("ABC"):
        cv.text(a.font, nm, ox - 4, oy + row * (ch + 10) + 6, 14, a.hue[row], 1.6, 1.8)
        for bar in range(3):
            for e in range(8):
                x = ox + 40 + (bar * 8 + e) * cw + bar * 14
                y = oy + row * (ch + 10)
                box = [(x, y), (x + cw - 6, y), (x + cw - 6, y + ch - 6), (x, y + ch - 6)]
                if bar % 3 == row:
                    cv.lines(ld.polyline(box, closed=True), a.white, 0.9, 1.2)
                else:
                    cv.fill(box, a.hue[row], 0.5)
                    cv.lines(ld.polyline(box, closed=True), a.hue[row], 1.6, 1.3)
    cv.text(a.font, "FILLED = FIRES   OUTLINE = RESTS, SOFT", ox + 40, oy + 3 * (ch + 10) + 6, 9, a.white, 0.7, 1.2)

    # ---------------- three inset panels: phases 2, 3 and the naked core
    py, pw, ph = 1100, 760, 380
    insets = [("PHASE 2  REFRACTION", "TWO PAIRS ALTERNATE BARS. FANS OF 5. THE RESTED PAIR OPENS WITH A BEAM"),
              ("PHASE 3  SPIRAL", "LAST PAIR: STREAMS ON 1/16, RESTS ON BEAT 4. CORE BURST ON BEAT 1"),
              ("NAKED CORE", "IRIS OPEN ON BEAT 1: TRIPLE DAMAGE. BURSTS OF 16 ON EVERY BEAT")]
    for i, (title, blurb) in enumerate(insets):
        x0 = 20 + i * (pw + 30)
        ccx, ccy, ss = x0 + pw / 2 + 40, py + ph / 2 + 10, 0.52
        cv.lines(ld.polyline([(x0, py), (x0 + pw, py), (x0 + pw, py + ph), (x0, py + ph)], closed=True), a.grid, 0.5, 1.2)
        cv.text(a.font, title, x0 + 16, py + 14, 14, a.white, 1.2, 1.6)
        cv.text(a.font, blurb, x0 + 16, py + ph - 26, 8, a.accent, 0.9, 1.2)
        if i == 0:
            nowb = 5 * 4 + 1.0
            ph2 = (2 * PI / (4 * BAR)) * nowb * BEAT
            al = [True, True, False]
            firing = lambda pair, bar, beat: al[pair] and (bar % 2 == pair)
            pts = volley(ph2, 4, -1, firing, 5, 14.0, 0.5, nowb, ccx, ccy, ss)
            draw_boss(cv, a, ccx, ccy, ss, ph2, al, resting=0, core_health=0.8)
            for k in (1, 4):                                    # pair B just rested: its beam
                ang = ph2 + k * PI / 3
                bx0, by0 = arm_xy(ccx, ccy, TIP_RADIUS, ang, ss)
                bx1, by1 = arm_xy(ccx, ccy, 900.0, ang, ss)
                beam = [clip_to(bx0, by0, bx1, by1, x0 + 6, py + 40, x0 + pw - 6, py + ph - 36)]
                cv.lines(beam, a.danger, 3.4, 5.0)
                cv.lines(beam, a.white, 2.0, 1.6)
                cv.threat(beam, a.danger + a.white * 0.25, 3.0, 1.5)
        elif i == 1:
            nowb = 3 * 4 + 2.75
            ph3 = -(2 * PI / (2 * BAR)) * nowb * BEAT * -1.0
            al = [True, False, False]
            firing = lambda pair, bar, beat: al[pair] and beat < 3.0
            pts = volley(ph3, 2, 1, firing, 1, 0.0, 0.25, nowb, ccx, ccy, ss)
            for k in range(24):                                 # the core burst of beat 1, 2.75 beats old
                d = k * 2 * PI / 24
                age = 2.75 * BEAT
                pts.append((ccx + math.cos(d) * BULLET_SPEED * age * ss, ccy + math.sin(d) * BULLET_SPEED * age * ss))
            draw_boss(cv, a, ccx, ccy, ss, ph3, al, resting=None, core_health=0.55)
        else:
            al = [False, False, False]
            pts = []
            for b in range(1, 5):
                age = (b - 0.9) * BEAT
                for k in range(16):
                    d = (k + 0.5 * (b % 2)) * 2 * PI / 16
                    pts.append((ccx + math.cos(d) * BULLET_SPEED * age * ss, ccy + math.sin(d) * BULLET_SPEED * age * ss))
            draw_boss(cv, a, ccx, ccy, ss * 1.6, 0.3, al, resting=None, iris_open=1.0, core_health=0.3)
            cv.flare(ccx, ccy, 150, a.white, 1.6, 1.4, 6, 0.3)
        pts = [(x, y) for x, y in pts if x0 + 8 < x < x0 + pw - 8 and py + 40 < y < py + ph - 36]
        draw_bullets(cv, a, pts, 4.6, W, H)

    out = os.path.join(DESIGN, "previews", "boss_kaleidos.png")
    cv.render(bloom=0.95).save(out, optimize=True)
    print("wrote", out, "| bullets in main picture:", len(bullets))


if __name__ == "__main__":
    main()
