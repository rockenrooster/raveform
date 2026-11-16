"""RAVEFORM modes: layout references for design/modes (a plan, 2026-11-15; nothing here is built).

Drawn at 2400x1080 with the game's own light, as design/tools/screens.py draws the screens:

  design/previews/modes_screen.png    the MODES screen (design/modes/screens.md): TRACERS at the centre, Mirrorball chosen
  design/previews/modes_tracers.png   TRACERS on the Stage (design/modes/tracers.md): two players' floor, a tracer about
                                      to close with its hint, a ring locked round what it caught
  design/previews/modes_songline.png  SONGLINE (design/modes/songline.md): the Spark, its string of gates waving with the
                                      tune, the ship threading it in time

each with a .small.png (1600 px wide). Mirrorball's palette is not in palettes.json yet (design/modes/README.md 11): its
recipe is resolved here with the same OKLCH maths.

Usage:  python design/tools/modes.py [screen] [tracers] [songline]      (no names = all three)
"""
import math
import os
import random
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import screens as sc  # noqa: E402

W, H, M, PI = sc.W, sc.H, sc.M, math.pi
PREVIEWS = os.path.join(sc.DESIGN, "previews")


def col(L, h, c=None, cf=None):
    return np.asarray(ld.oklch(L, h, c=c, cf=cf)[0], np.float32)


# design/modes/README.md 10: each mode's two colours.
MODE_COLOURS = {
    "THE SET": (col(0.86, 200, cf=0.95), col(0.70, 300, cf=0.95)),
    "HEADLINERS": (col(0.82, 70, cf=0.95), col(0.72, 318, cf=0.95)),
    "FULL BLAST": (col(0.85, 230, cf=0.95), col(0.72, 295, cf=0.95)),
    "TRACERS": (col(0.90, 140, cf=0.95), col(0.86, 190, cf=0.95)),
    "SONGLINE": (col(0.86, 150, cf=0.95), col(0.70, 295, cf=0.95)),
    "JAM": (col(0.80, 55, cf=0.95), col(0.82, 245, cf=0.95)),
}
# design/modes/README.md 11: the two bonus palettes' accents (their chips).
BONUS_ACCENT = {11: col(0.96, 280, c=0.03), 12: col(0.90, 160, cf=0.60)}


class Mirrorball(sc.Screen):
    """A Screen in Mirrorball's palette (the recipe of design/modes/README.md 11)."""

    def __init__(self, seed=1, bg=0.5):
        super().__init__("zenith", seed)
        self.accent = col(0.96, 280, c=0.03)
        self.grid = col(0.50, 285, c=0.045)
        self.hue = [col(0.84, 150, cf=0.97), col(0.68, 258, cf=0.97), col(0.72, 318, cf=0.95)]
        self.mate = [col(0.89, 190, cf=0.95), col(0.73, 218, cf=0.95), col(0.77, 278, cf=0.95)]
        self.cv.radial_bg(col(0.08, 275, c=0.030) * bg, col(0.17, 290, c=0.060) * bg, cx=0.5, cy=0.5)


def small(img, name):
    img.resize((1600, 720), Image.LANCZOS).save(os.path.join(PREVIEWS, name + ".small.png"), optimize=True)


def round_rect(cx, cy, w, h, r):
    pts = []
    for qx, qy, a0 in ((cx + w / 2 - r, cy - h / 2 + r, -PI / 2), (cx + w / 2 - r, cy + h / 2 - r, 0.0),
                       (cx - w / 2 + r, cy + h / 2 - r, PI / 2), (cx - w / 2 + r, cy - h / 2 + r, PI)):
        for k in range(9):
            a = a0 + PI / 2 * k / 8
            pts.append((qx + math.cos(a) * r, qy + math.sin(a) * r))
    return pts


def tube(s, text, cx, top, cap, c0, c1, level=2.3):
    x = cx - (len(text) * 6 - 2) * cap / 6.0 / 2
    segs, end = ld.text_segments(s.font, text, x, top, cap)
    s.cv.lines(segs, ld.f_linear(x, 0, end, 0, [c0, c0 * 0.5 + s.white * 0.5, c1]), level, max(2.6, cap * 0.06))
    s.cv.lines(segs, s.white, level * 0.9, max(1.0, cap * 0.016))


def ring_pts(cx, cy, rx, ry, n=64, a0=0.0, a1=2 * PI):
    return [(cx + rx * math.cos(a0 + (a1 - a0) * k / n), cy + ry * math.sin(a0 + (a1 - a0) * k / n)) for k in range(n + 1)]


# --------------------------------------------------------------------------- emblems (README.md 10)

def emblem_tracers(s, cx, cy, k, level=1.0):
    c0, c1 = MODE_COLOURS["TRACERS"]
    cv = s.cv
    for i in range(3):                                           # the nine tiles inside the ring, lit
        for j in range(3):
            x, y = cx - 54 * k + i * 36 * k, cy - 54 * k + j * 36 * k
            cv.lines(ld.polyline([(x, y), (x + 36 * k, y), (x + 36 * k, y + 36 * k), (x, y + 36 * k)], closed=True), c1, 0.7 * level, 1.3)
    pts = ring_pts(cx, cy, 120 * k, 82 * k, 48, -0.35, 2 * PI - 0.75)   # the ring, nearly closed, the ship at its head
    cv.lines(ld.polyline(pts), ld.f_radial(pts[-1][0], pts[-1][1], 260 * k, [s.white, c0, c1]), 2.4 * level, 2.6)
    for p in pts[::6]:
        cv.glow(p[0], p[1], 7, s.white, 0.6 * level)
    a = -0.75 + PI / 2
    s.ship(pts[-1][0], pts[-1][1], 17 * k, a, 0, 0, level > 0.7)


def emblem_blast(s, cx, cy, k, level=1.0):
    cv = s.cv
    sx, sy = cx - 150 * k, cy
    hues = [300, 240, 190, 140, 95, 60, 330]
    for i in range(7):
        a = (i - 3) * 0.16
        x1, y1 = sx + math.cos(a) * 300 * k, sy + math.sin(a) * 300 * k
        c = col(0.70, hues[i], cf=0.97)
        cv.lines([(sx + math.cos(a) * 34 * k, sy + math.sin(a) * 34 * k, x1, y1)], ld.f_linear(sx, sy, x1, y1, [c, c, s.white]), 1.5 * level, 2.0)
        cv.glow(x1, y1, 8, s.white, 0.6 * level)
    s.ship(sx, sy, 17 * k, 0.0, 0, 0, level > 0.7)


def emblem_songline(s, cx, cy, k, level=1.0):
    c0, c1 = MODE_COLOURS["SONGLINE"]
    cv = s.cv
    notes = [0, 3, 7, 12, 7, 3, 7, 10]
    for i, n in enumerate(notes):
        x, y = cx - 190 * k + i * 54 * k, cy + 34 * k - n * 6.5 * k
        lit = i < 5
        cv.lines(ld.polyline(ring_pts(x, y, 20 * k, 20 * k, 24), closed=True), c0 * (1 - i / 7.0) + c1 * (i / 7.0), (1.9 if lit else 0.6) * level, 1.7)
        if i == 4:                                               # the spark, at the fifth gate
            cv.lines(ld.transform(ld.star(4, 1.0, 0.22), x, y, 30 * k, PI / 4), s.white, 3.0 * level, 1.8)
            cv.glow(x, y, 60 * k, c0, 0.25 * level)


# --------------------------------------------------------------------------- the MODES screen

def modes_screen():
    s = Mirrorball(11, bg=0.5)
    cv = s.cv
    cv.nebula([(500, 320, 700, s.hue[1], 0.022), (1900, 780, 700, s.hue[2], 0.020)])
    s.stars(260)
    for _ in range(26):                                          # the demo run behind, at a fifth
        k = s.rng.randrange(3)
        gid = s.rng.choice(["drifter_2", "hound_2", "shoal_1", "carom_2", "moth_2"])
        s.enemy(gid, s.rng.uniform(60, W - 60), s.rng.uniform(200, H - 60), 21 * (0.55 if "shoal" in gid else 1.0), s.rng.uniform(0, 6.28), k, 0.2)
    field = [s.p1, s.white, s.hue[0], s.hue[2]]

    # the rule, moved up from the title, its ends the music
    pts = []
    for i in range(0, W + 1, 6):
        u = i / W
        end = max(0.0, 1.0 - min(u, 1.0 - u) * W / 300.0)
        pts.append((i, 150 + end * (33 * math.sin(u * 61.0) * math.sin(u * 7.3 + 1.0) + 12 * math.sin(u * 173.0 + 0.6))))
    cv.lines(ld.polyline(pts), ld.f_linear(0, 0, W, 0, field), 2.1, 2.4)
    cv.lines(ld.polyline(pts), ld.f_linear(0, 0, W, 0, field), 0.16, 10.0)
    tube(s, "MODES", W / 2, 60, 44, s.accent, s.accent, 1.6)

    # the cards: FULL BLAST | TRACERS | SONGLINE
    for name, cx, emblem in (("FULL BLAST", 500, emblem_blast), ("SONGLINE", 1900, emblem_songline)):
        c0, c1 = MODE_COLOURS[name]
        cv.lines(ld.polyline(round_rect(cx, 440, 520, 340, 40), closed=True), ld.f_linear(cx - 260, 0, cx + 260, 0, [c0, c1]), 0.8 * 0.6, 2.0)
        emblem(s, cx, 400, 0.62, 0.6)
        tube(s, name, cx, 504, 36, c0, c1, 1.2)
    c0, c1 = MODE_COLOURS["TRACERS"]
    card = round_rect(1200, 440, 720, 440, 40)
    cv.fill(card, ld.f_linear(840, 0, 1560, 0, [c0, c1]), 0.085)
    cv.lines(ld.polyline(card, closed=True), ld.f_linear(840, 0, 1560, 0, [c0, c1]), 2.2, 3.4)
    cv.glow(960, 300, 300, c0, 0.05)
    cv.glow(1440, 580, 300, c1, 0.05)
    emblem_tracers(s, 1200, 330, 1.0)
    tube(s, "TRACERS", 1200, 428, 64, c0, c1)
    s.centred("DRAW RINGS OF LIGHT. NO AIMING", 522, 18, s.white, 1.2, 1.8)
    s.centred("1-4 PLAYERS   TEAM OR VERSUS   ABOUT 2 MINUTES", 567, 13, s.accent, 1.0, 1.5)
    s.centred("BEST 412,300", 608, 14, s.gold, 1.2, 1.6)

    for k in range(7):                                           # which card is at the centre
        cv.discs([(W / 2 - 108 + k * 36, 700, 5)], c0 if k == 4 else s.white, 1.9 if k == 4 else 0.32)

    # the Track line and strip: Mirrorball (a bonus Track) chosen
    s.centred("BONUS   MIRRORBALL   122 BPM", 734, 18, s.accent, 1.4, 1.9)
    for k in range(12):
        t = k + 1
        x = 672 + 96 * k
        if t <= 10:
            acc = s.lin(next(p for p in s.pals["palettes"] if p["track"] == t)["accent"])
        else:
            acc = BONUS_ACCENT[t]
        chosen = t == 11
        r = 42 if chosen else 30
        ring = ld.polyline(ring_pts(x, 820, r, r, 40), closed=True)
        if chosen:
            cv.discs([(x, 820, r)], acc, 0.16)
            cv.lines(ring, s.p1, 2.0, 3.0)
            cv.glow(x, 820, 110, s.p1, 0.10)
        else:
            cv.lines(ring, acc, 0.9, 2.0)
        s.centred(str(t), 820 - (11 if chosen else 8), 22 if chosen else 16, s.white if chosen else acc, 1.6 if chosen else 1.1, None, x)
        if t > 10:
            cv.lines(ld.transform(ld.star(4, 1.0, 0.3), x + r * 0.78, 820 - r * 0.78, 9, PI / 4), s.white, 1.6, 1.2)

    s.pill(282, 968, 300, 90, "BACK", s.white, 26)
    s.pill(W / 2, 968, 600, 130, "", s.p1, 52, pressed=False)
    cv.fill(round_rect(W / 2, 968, 600, 130, 65), s.p1, 0.08)
    cv.glow(W / 2, 968, 350, s.p1, 0.08)
    tube(s, "PLAY", W / 2, 968 - 26 - 11, 52, s.p1, s.p1, 1.8)
    for k in range(4):
        cv.discs([(W / 2 - 54 + k * 36, 968 + 44, 5)], s.accent if k == 0 else s.white, 1.9 if k == 0 else 0.32)
    img = cv.render(bloom=0.95)
    img.save(os.path.join(PREVIEWS, "modes_screen.png"), optimize=True)
    small(img, "modes_screen")


# --------------------------------------------------------------------------- TRACERS on the Stage

STAGE_W, STAGE_H, TILE = 1720.0, 840.0, 40.0
TX, TY = 43, 21
K = min(1600.0 / (STAGE_W + 80.0), 720.0 / (STAGE_H + 80.0)) * 1.5      # px per wu at the Stage's zoom on a 20:9 phone


def px(x, y):
    return W / 2 + x * K, H / 2 + y * K


def hud(s, lines, score="48,210", chain="X86"):
    cv = s.cv
    cv.text(s.font, score, M, 44, 34, ld.f_linear(0, 44, 0, 78, [s.white, s.accent]), 1.6, 3.0)
    cv.text(s.font, chain, M, 94, 22, s.gold, 1.4, 2.2)
    top = lines[0]
    s.centred(top, 34, 12, s.accent, 1.0, 1.4)
    x0, widths = W / 2 - 260, [8, 4, 8, 4, 8, 4, 12]
    x = x0
    for i, bars in enumerate(widths):                            # the run bar: the 48-bar form, the playhead in its last Drop
        w = 520.0 * bars / 48.0
        cv.lines([(x + 3, 110, x + w - 3, 110)], s.white, 0.6 if i in (2, 6) else 0.3, 3.0)
        x += w
    cv.discs([(x0 + 520.0 * 40.5 / 48.0, 110, 6)], s.white, 2.0)
    cols = [(s.hue[1], 8), (s.accent, 4), (s.hue[2], 8), (s.grid, 4)]
    x = x0
    for i, (c, bars) in enumerate(cols):                         # the phrase bar
        w = 520.0 * bars / 24.0
        cv.lines([(x + 4, 60, x + w - 4, 60)], c, 1.9 if i == 2 else 0.6, 6.0)
        x += w
    c0, c1 = lines[2]
    xg = W / 2 - (len(lines[1]) * 6 - 2) * 14 / 6.0 / 2
    segs, end = ld.text_segments(s.font, lines[1], xg, 76, 14)
    cv.lines(segs, ld.f_linear(xg, 0, end, 0, [c0, s.white, c1]), 1.5, 1.7)
    for k in range(3):                                           # the Nova stock, the pause ring
        cv.lines(ld.transform(ld.star(4, 1.0, 0.3), W - M - 150 + k * 44, 60, 15, PI / 4), s.p1, 2.0, 1.6)
    cv.lines(ld.polyline(ring_pts(W - M - 16, 60, 28, 28, 32), closed=True), s.white, 0.9, 2.0)
    cv.lines([(W - M - 22, 50, W - M - 22, 70), (W - M - 10, 50, W - M - 10, 70)], s.white, 0.9, 2.4)


def tracers_frame():
    s = Mirrorball(4, bg=0.42)
    cv = s.cv
    rng = s.rng
    s.stars(120)
    x0, y0 = -STAGE_W / 2, -STAGE_H / 2

    # the floor: who has lit which tile (0 dark, 1 P1, 2 P2), from the rings flown so far
    owner = np.zeros((TY, TX), np.int32)
    rings = [(1, -470, -110, 300, 230), (1, -150, 250, 330, 130), (2, 470, 170, 270, 200), (2, 560, -230, 230, 150), (1, 60, -250, 190, 120)]
    for who, cx, cy, rx, ry in rings:
        for j in range(TY):
            for i in range(TX):
                tx, ty = x0 + (i + 0.5) * TILE, y0 + (j + 0.5) * TILE
                if ((tx - cx) / rx) ** 2 + ((ty - cy) / ry) ** 2 <= 1.0:
                    owner[j, i] = who
    lit = int((owner > 0).sum())

    # the lattice at rest, 20 wu
    base = []
    for i in range(int(STAGE_W / 20) + 1):
        a, b = px(x0 + i * 20, y0), px(x0 + i * 20, -y0)
        base.append((a[0], a[1], b[0], b[1]))
    for j in range(int(STAGE_H / 20) + 1):
        a, b = px(x0, y0 + j * 20), px(-x0, y0 + j * 20)
        base.append((a[0], a[1], b[0], b[1]))
    cv.lines(base, s.grid, 0.30, 1.1)
    # lit tiles: the lattice's lines inside them in their owner's colour; the regions' edges
    for who in (1, 2):
        colour, deep, light = s.players[who - 1]
        inner, edge = [], []
        for j in range(TY):
            for i in range(TX):
                if owner[j, i] != who:
                    continue
                tx, ty = x0 + i * TILE, y0 + j * TILE
                for d in (0.0, 20.0):
                    a, b = px(tx + d, ty), px(tx + d, ty + TILE)
                    inner.append((a[0], a[1], b[0], b[1]))
                    a, b = px(tx, ty + d), px(tx + TILE, ty + d)
                    inner.append((a[0], a[1], b[0], b[1]))
                for di, dj, p, q in ((1, 0, (TILE, 0), (TILE, TILE)), (-1, 0, (0, 0), (0, TILE)), (0, 1, (0, TILE), (TILE, TILE)), (0, -1, (0, 0), (TILE, 0))):
                    ni, nj = i + di, j + dj
                    if ni < 0 or nj < 0 or ni >= TX or nj >= TY or owner[nj, ni] != who:
                        a, b = px(tx + p[0], ty + p[1]), px(tx + q[0], ty + q[1])
                        edge.append((a[0], a[1], b[0], b[1]))
        cv.lines(inner, ld.f_radial(W / 2, H / 2, 1300, [colour, colour * 0.7 + light * 0.3]), 0.36, 1.2)
        cv.lines(edge, colour, 1.4, 1.5)
        cv.lines(edge, colour, 0.07, 14.0)                       # the edge's glow
    # the walls and the rig
    a, b = px(x0, y0), px(-x0, -y0)
    wall = [(a[0], a[1]), (b[0], a[1]), (b[0], b[1]), (a[0], b[1])]
    cv.lines(ld.polyline(wall, closed=True), ld.f_linear(a[0], 0, b[0], 0, [s.accent, s.hue[1], s.accent]), 1.6, 2.4)
    for k in range(8):
        fx, fy = px(-665 + 190 * k, y0 + 14)
        cv.lines([(fx - 22, fy, fx + 22, fy), (fx - 22, fy, fx - 22, fy + 9), (fx + 22, fy, fx + 22, fy + 9)], s.grid, 0.8, 1.6)

    # the crowd (contact only: no bullets anywhere)
    def body(gid, x, y, k, gain=0.75, colour=None):
        p = px(x, y)
        r = 16.5 * (0.6 if "shoal" in gid else 1.0)
        if colour is None:
            s.enemy(gid, p[0], p[1], r, rng.uniform(0, 6.28), k, gain)
        else:
            g = s.glyphs[gid]
            hull = ld.transform([(q[0], -q[1], q[2], -q[3]) for q in g["segments"] if q[4] == 0], p[0], p[1], r, rng.uniform(0, 6.28))
            cv.lines(hull, colour, 1.5, 1.8)
            cv.discs([(p[0], p[1], 3.2)], s.white, 2.0)

    for _ in range(54):
        gid = rng.choice(["drifter_2", "drifter_2", "hound_2", "shoal_1", "shoal_1", "carom_2", "moth_2"])
        body(gid, rng.uniform(x0 + 40, -x0 - 40), rng.uniform(y0 + 40, -y0 - 40), 0 if gid in ("drifter_2", "shoal_1", "carom_2") else 1)

    # P2's ring, locked: what it caught stands still in P2's colour with a white dot; it pops on the next eighth
    c2, d2, l2 = s.players[1]
    rc = (300.0, -40.0, 150.0, 120.0)
    rp = [px(rc[0] + rc[2] * math.cos(a), rc[1] + rc[3] * math.sin(a)) for a in np.linspace(0, 2 * PI, 60)]
    cv.lines(ld.polyline(rp, closed=True), ld.f_linear(rp[0][0] - 300, 0, rp[0][0], 0, [c2, s.white]), 2.8, 2.6)
    q = px(rc[0], rc[1])
    cv.glow(q[0], q[1], 190, c2, 0.16)
    for _ in range(13):
        a, u = rng.uniform(0, 6.28), math.sqrt(rng.uniform(0.02, 0.8))
        body(rng.choice(["drifter_2", "hound_2", "shoal_1", "carom_2"]), rc[0] + rc[2] * u * math.cos(a), rc[1] + rc[3] * u * math.sin(a), 0, colour=c2)
    cv.text(s.font, "RING X3", q[0] - 62, q[1] - rc[3] * K - 44, 20, c2, 1.8, 2.0)
    sp = px(455, 60)
    s.ship(sp[0], sp[1], 16.5, 0.6, 1, 4)
    t2 = [px(455 + 6 * k, 60 + 9 * k + 0.07 * k * k) for k in range(0, 22)]
    cv.lines(ld.polyline(t2), ld.f_radial(sp[0], sp[1], 260, [s.white, c2, d2]), 2.4, 2.2)

    # P1's tracer: a ring nearly closed round a pack, with the hint to where it will close
    c1, d1, l1 = s.players[0]
    oc = (-250.0, 30.0, 250.0, 170.0)
    for _ in range(16):
        a, u = rng.uniform(0, 6.28), math.sqrt(rng.uniform(0.02, 0.7))
        body(rng.choice(["drifter_2", "hound_2", "shoal_1", "shoal_1", "moth_2"]), oc[0] + oc[2] * u * math.cos(a), oc[1] + oc[3] * u * math.sin(a), rng.randrange(2), 0.9)
    arc = [px(oc[0] + oc[2] * math.cos(a), oc[1] + oc[3] * math.sin(a)) for a in np.linspace(0.9, 0.9 + 2 * PI * 0.88, 96)]
    head = arc[-1]
    cv.lines(ld.polyline(arc[14:]), ld.f_radial(head[0], head[1], 900, [s.white, c1, d1]), 2.6, 2.2)
    for k in range(0, 14, 2):                                    # its last two beats: dashed, about to go out
        cv.lines([(arc[k][0], arc[k][1], arc[k + 1][0], arc[k + 1][1])], d1, 1.2, 2.0)
    for p in arc[14::7]:                                         # a node a sixteenth: each a note of the lead
        cv.glow(p[0], p[1], 9, s.white, 0.7)
    s.ship(head[0], head[1], 16.5, 0.9 + 2 * PI * 0.88 + PI / 2, 0, 4)
    target = arc[16]
    n = 9
    for k in range(n):                                           # the hint: where it will close
        if k % 2 == 0:
            ax, ay = head[0] + (target[0] - head[0]) * k / n, head[1] + (target[1] - head[1]) * k / n
            bx, by = head[0] + (target[0] - head[0]) * (k + 1) / n, head[1] + (target[1] - head[1]) * (k + 1) / n
            cv.lines([(ax, ay, bx, by)], l1, 0.7, 1.2)
    cv.lines(ld.polyline(ring_pts(target[0], target[1], 13, 13, 20), closed=True), l1, 1.2, 1.4)

    share = int(round(100.0 * lit / (TX * TY)))
    hud(s, ["TRACERS   MIRRORBALL   122 BPM", "FLOOR %d%%   HOUSE 1" % share, MODE_COLOURS["TRACERS"]])
    cv.text(s.font, "TRACER 11 BEATS", M, 136, 12, s.gold, 1.0, 1.4)
    cv.lines([(M, 164, M + 220, 164)], s.white, 0.3, 5.0)
    cv.lines([(M, 164, M + 150, 164)], s.gold, 1.4, 5.0)
    img = cv.render(bloom=0.95)
    img.save(os.path.join(PREVIEWS, "modes_tracers.png"), optimize=True)
    small(img, "modes_tracers")
    print("modes_tracers: %d of %d tiles lit (%d%%), px per wu %.3f" % (lit, TX * TY, share, K))


# --------------------------------------------------------------------------- SONGLINE

def songline_frame():
    s = sc.Screen("aurora", 7)
    cv = s.cv
    rng = s.rng
    cv.nebula([(700, 300, 800, s.hue[0], 0.03), (1800, 800, 700, s.hue[1], 0.026)])
    s.stars(200)
    k = 1.5 * 0.85                                               # px per wu: the camera holds 0.85 near the Spark
    lat = []
    for i in range(-40, 41):
        lat.append((W / 2 + i * 20 * k, 0, W / 2 + i * 20 * k, H))
    for j in range(-20, 21):
        lat.append((0, H / 2 + j * 20 * k, W, H / 2 + j * 20 * k))
    cv.lines(lat, s.lin(s.pal["grid"]), 0.26, 1.1)

    # the line: a smooth curve from left to right; a gate every eighth; each gate's place across it is its pitch
    def line(u):
        return 140 + u * 2000, 600 - 250 * math.sin(u * 2.1 + 0.2) + 60 * math.sin(u * 5.3)

    # A Groove lays a gate every eighth: the even sixteenths of Aurora's E minor bar (design/music/track04.json), its tune
    # E G F# G, E G A G; the bar's middle (80.4) is pulled up by the pedal note on the odd sixteenths, so the tune rides
    # one side of the line and a Drop's string (every sixteenth) zigzags across it.
    arp = [76, 79, 78, 79, 76, 79, 81, 79]
    mid = 80.4
    gates = []
    n = 11
    for g in range(n):
        u = 0.02 + g * 0.068                                     # about 113 wu apart: 416 wu/s for an eighth at 110 BPM
        x, y = line(u)
        x2, y2 = line(u + 0.004)
        tx, ty = x2 - x, y2 - y
        ln = math.hypot(tx, ty)
        nx, ny = -ty / ln, tx / ln
        off = max(-56.0, min(56.0, (arp[g % 8] - mid) * 7.0)) * k
        gates.append((x + nx * off, y + ny * off, arp[g % 8]))
    spark_u = 0.02 + (n - 0.5) * 0.068
    spark = line(spark_u)
    c0, c1 = MODE_COLOURS["SONGLINE"]
    ship_at = 3
    for g, (x, y, note) in enumerate(gates):
        hue_c = s.accent * 0.6 + (s.hue[0] if note > 78.5 else s.hue[1]) * 0.4
        taken = 1 <= g < ship_at
        if g < 1:                                                # missed, long ago: fading grey
            cv.lines(ld.polyline(ring_pts(x, y, 34 * k, 34 * k, 28), closed=True), s.white, 0.25, 1.3)
        elif taken:                                              # threaded: gone, a few sparks left in the player's colour
            for _ in range(3):
                a = rng.uniform(0, 6.28)
                cv.lines([(x, y, x + math.cos(a) * 16, y + math.sin(a) * 16)], s.p1, 0.9, 1.2)
        else:
            cv.lines(ld.polyline(ring_pts(x, y, 34 * k, 34 * k, 32), closed=True), hue_c, 1.8, 1.7)
            cv.lines(ld.polyline(ring_pts(x, y, 22 * k, 22 * k, 24), closed=True), hue_c, 0.6, 1.2)
    # the guide: the line a bar ahead of the Spark, dotted
    for d in range(1, 20):
        gx, gy = line(spark_u + d * 0.012)
        cv.discs([(gx, gy, 2.2)], s.accent, 0.35 + 0.5 * (1 - d / 20.0))
    # the Spark: a four-point star with its tail
    tail = [line(spark_u - d * 0.004) for d in range(0, 16)]
    cv.lines(ld.polyline(tail), ld.f_linear(tail[0][0], tail[0][1], tail[-1][0], tail[-1][1], [s.white, s.accent, s.accent * 0.2]), 2.6, 2.4)
    cv.glow(spark[0], spark[1], 150, s.accent, 0.22)
    cv.lines(ld.transform(ld.star(4, 1.0, 0.16), spark[0], spark[1], 60 * k, PI / 4), s.white, 3.5, 2.2)
    # the ship, a bar behind the Spark, threading its way up the string
    gx, gy, _ = gates[ship_at]
    ahead = gates[ship_at + 1]
    heading = math.atan2(ahead[1] - gy, ahead[0] - gx)
    s.ship(gx, gy, 14 * k, heading + PI / 2, 0, 4)
    for r in (27, 32, 37):                                       # three rings of streak: Prism is on
        cv.lines(ld.polyline(ring_pts(gx, gy, r * k, r * k, 40), closed=True), ld.f_angular(gx, gy, [c0, c1, c0, c1]), 1.2, 1.0)
    crowd = line(0.93)
    aim = math.atan2(crowd[1] - 130 - gy, crowd[0] - gx)
    for i, hdeg in enumerate((300, 250, 200, 150, 100)):         # the five-way fan, at the crowd coming down the line
        a = aim + (i - 2) * 0.10
        c = s.p1 if i == 2 else col(0.70, hdeg, cf=0.97)
        for d in (110, 290, 470, 650):
            ax, ay = gx + math.cos(a) * d, gy + math.sin(a) * d
            cv.lines([(ax, ay, ax + math.cos(a) * 40, ay + math.sin(a) * 40)], ld.f_linear(ax, ay, ax + math.cos(a) * 40, ay + math.sin(a) * 40, [c, s.white]), 2.6, 2.4)
    # the crowd, coming along the line ahead
    for _ in range(60):
        u = rng.uniform(spark_u + 0.06, 1.0)
        x, y = line(u)
        gid = rng.choice(["drifter_2", "hound_2", "shoal_1", "shoal_1", "carom_2", "moth_2"])
        s.enemy(gid, x + rng.gauss(0, 150), y + rng.gauss(0, 150), 21 * k / 1.5 * (0.55 if "shoal" in gid else 1.0), rng.uniform(0, 6.28), rng.randrange(3), 0.8)
    s.accent = s.lin(s.pal["accent"])
    s.grid = s.lin(s.pal["grid"])
    hud(s, ["SONGLINE   AURORA   125 BPM", "GATES 148   STREAK 72", MODE_COLOURS["SONGLINE"]], "126,480", "X212")
    img = cv.render(bloom=0.95)
    img.save(os.path.join(PREVIEWS, "modes_songline.png"), optimize=True)
    small(img, "modes_songline")


def main():
    want = sys.argv[1:] or ["screen", "tracers", "songline"]
    if "screen" in want:
        modes_screen()
        print("modes_screen written", flush=True)
    if "tracers" in want:
        tracers_frame()
    if "songline" in want:
        songline_frame()
        print("modes_songline written", flush=True)


if __name__ == "__main__":
    main()
