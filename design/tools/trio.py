"""RAVEFORM: the upgrade Trio at the Break, and the Fusion orb (layout reference for M3 stage B).

Two frames at 2400x1080 (1.5 px per world unit), drawn with the game's own light, then tiled at half size.
The numbers are in design/screens.md ("The Trio"); this draws them.

Usage:  python design/tools/trio.py   -> design/previews/trio.png (+ screen_trio_break.png, screen_trio_fusion.png)
"""
import math
import os
import sys

from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import icons as ic  # noqa: E402
import screens as sc  # noqa: E402

W, H, PI = sc.W, sc.H, math.pi
PX = 1.5                      # pixels per world unit
RING = 170 * PX               # the Trio sits on a circle of 170 wu around the ship
ORB = 38 * PX                 # icon frame radius of a Mod orb; Riffs x1.14, Fusions x1.45


def backdrop(s, seed_shards=70):
    cv = s.cv
    cv.nebula([(520, 300, 760, s.hue[0], 0.022), (1900, 820, 760, s.hue[2], 0.02)])
    s.stars(200)
    grid = [(gx, 0, gx, H) for gx in range(0, W + 1, 45)] + [(0, gy, W, gy) for gy in range(0, H + 1, 45)]
    cv.lines(grid, s.lin(s.pal["grid"]), 0.07, 1.0)                    # the lattice at rest, dimmed for the Break
    for _ in range(9):                                                 # what is left of the fight, stepping back
        k = s.rng.randrange(3)
        gid = s.rng.choice(["drifter_2", "hound_2", "carom_2", "shoal_2"])
        a, d = s.rng.uniform(0, 2 * PI), s.rng.uniform(520, 1100)
        s.enemy(gid, W / 2 + math.cos(a) * d, 540 + math.sin(a) * d * 0.55, 21 * (0.55 if "shoal" in gid else 1.0), s.rng.uniform(0, 6.28), k, 0.22)


def shards_to(s, x, y, n):
    """The last Shards of the Drop still homing in: gold comets (look.md section 7)."""
    for _ in range(n):
        a, d = s.rng.uniform(0, 2 * PI), s.rng.uniform(70, 520)
        px, py = x + math.cos(a) * d, y + math.sin(a) * d * 0.7
        tx, ty = px + math.cos(a) * 26, py + math.sin(a) * 26 * 0.7
        s.cv.lines([(tx, ty, px, py)], ld.f_linear(tx, ty, px, py, [s.gold * 0.0, s.gold]), 1.1, 1.6)
        s.cv.discs([(px, py, 2.6)], s.gold, 1.6)


def choice_ring(s, x, y, left):
    """The circle the orbs sit on is also the clock: the lit arc is the Break that is left."""
    cv = s.cv
    n = 96
    lit = int(n * left)
    arc = lambda i0, i1: ld.polyline([(x + RING * math.sin(2 * PI * i / n), y - RING * math.cos(2 * PI * i / n)) for i in range(i0, i1 + 1)])
    cv.lines(arc(0, lit), ld.f_angular(x, y, [s.accent, s.white, s.accent, s.white]), 1.3, 2.0)
    cv.lines(arc(lit, n), s.white, 0.16, 1.4)
    tx, ty = x + RING * math.sin(2 * PI * lit / n), y - RING * math.cos(2 * PI * lit / n)
    cv.discs([(tx, ty, 5)], s.white, 2.0)
    cv.glow(tx, ty, 40, s.accent, 0.25)


def orb(s, iid, x, y, name, effect, stacks, side, state="idle", ship=None):
    """One choice. side: where its words go (above | below | left | right), always outside the circle.
    state: idle | near (the ship is closing on it)."""
    cv = s.cv
    icon = next(i for i in ic.ICONS if i[0] == iid)
    R = ORB * icon[4]
    near = state == "near"
    gold = icon[1] == "fusion"
    if near:
        cv.glow(x, y, R * 3.4, s.p1, 0.16)
        cv.lines(ld.transform(ld.polygon(64), x, y, R * 1.42), s.p1, 2.0, 2.6)
        cv.lines(ld.transform(ic.dashes(1.0, 24, 0.5), x, y, R * 1.62), s.white, 0.9, 1.4)
        if ship:                                               # a line of light from the ship: this is the one
            sx, sy = ship
            d = math.hypot(x - sx, y - sy)
            ux, uy = (x - sx) / d, (y - sy) / d
            a0, a1 = 34, d - R * 1.5
            cv.lines([(sx + ux * a0, sy + uy * a0, sx + ux * a1, sy + uy * a1)], ld.f_linear(sx, sy, x, y, [s.p1, s.white]), 1.2, 1.6)
    else:
        cv.lines(ld.transform(ld.polygon(64), x, y, R * 1.42), s.gold if gold else s.white, 0.55 if gold else 0.35, 1.4)
    ic.draw_layers(cv, ic.layers_of(icon), x, y, R * (1.12 if near else 1.0), s.sw)
    if gold:
        cv.flare(x, y, R * 3.0, s.gold, 1.4, 1.5, 4, PI / 4)
        cv.flare(x, y, R * 2.2, s.white, 1.6, 1.3, 2, 0.0)
    # words, always on the outside of the circle: name, what it does, the stacks this pick would make
    NAME, EFFECT = 30, 17
    width = lambda text, cap: (len(text) * 6 - 2) * cap / 6.0
    reach = R * 1.42 + 30
    if side == "above":
        top, anchor = y - reach - NAME - 14 - EFFECT - (34 if stacks else 0), 0
    elif side == "below":
        top, anchor = y + reach, 0
    else:
        top, anchor = y - 46, (-1 if side == "left" else 1)

    def put(text, yy, cap, colour, level, wd):
        if anchor == 0:
            cxx = x
        elif anchor < 0:
            cxx = x - reach - width(text, cap) / 2
        else:
            cxx = x + reach + width(text, cap) / 2
        s.centred(text, yy, cap, colour, level, wd, cxx)
        return cxx

    put(name, top, NAME, s.gold if gold else s.white, 1.9 if near else 1.3, 3.0)
    put(effect, top + NAME + 14, EFFECT, s.gold if gold else s.accent, 1.4 if near else 1.0, 1.8)
    if stacks:
        have, cap = stacks
        span = (cap - 1) * 26
        x0 = x - span / 2 if anchor == 0 else (x - reach - span if anchor < 0 else x + reach)
        for k in range(cap):
            px, py = x0 + k * 26 + (6 if anchor > 0 else (-6 if anchor < 0 else 0)), top + NAME + 14 + EFFECT + 24
            if k < have:
                cv.discs([(px, py, 6)], s.white, 1.3)
            elif k == have:
                cv.discs([(px, py, 6)], s.gold, 2.0)
                cv.lines(ld.transform(ld.polygon(20), px, py, 11), s.gold, 1.4, 1.6)
            else:
                cv.lines(ld.transform(ld.polygon(20), px, py, 6), s.white, 0.4, 1.3)


def at(x, y, deg, r=RING):
    a = math.radians(deg)
    return x + r * math.cos(a), y - r * math.sin(a)


def trio_break():
    s = sc.Screen("reef", 21)
    backdrop(s)
    cx, cy = W / 2, 560
    top, left, right = at(cx, cy, 90), at(cx, cy, 210), at(cx, cy, 330)
    # the ship has made up its mind: it is a third of the way to the right-hand orb
    sx, sy = cx + (right[0] - cx) * 0.36, cy + (right[1] - cy) * 0.36
    shards_to(s, sx, sy, 46)
    choice_ring(s, cx, cy, 0.62)
    orb(s, "split", top[0], top[1], "SPLIT", "+1 SHOT LANE", (1, 3), "above")
    orb(s, "magnet", left[0], left[1], "MAGNET", "WIDER SHARD PULL", (0, 3), "left")
    orb(s, "orbitals", right[0], right[1], "ORBITALS", "NEW RIFF: TWO ORBITERS", (0, 5), "right", "near", (sx, sy))
    s.ship(sx, sy, 21, math.atan2(right[1] - sy, right[0] - sx), cells=3)
    s.centred("FLY INTO ONE", 1012, 14, s.white, 0.7, 1.6)
    return s.save("trio_break")


def trio_fusion():
    s = sc.Screen("solar", 23)
    backdrop(s)
    cx, cy = W / 2, 600
    down, ur, ul = at(cx, cy, 270), at(cx, cy, 30), at(cx, cy, 150)
    fus = at(cx, cy, 90, 230 * PX)
    shards_to(s, cx, cy, 30)
    choice_ring(s, cx, cy, 0.85)
    # two thin gold lines run from the ingredients the player already owns up to the Fusion
    for src in (ul, ur):
        s.cv.lines([(src[0], src[1] - ORB * 1.5, fus[0] + (src[0] - fus[0]) * 0.28, fus[1] + (src[1] - fus[1]) * 0.28)],
                   ld.f_linear(src[0], src[1], fus[0], fus[1], [s.gold * 0.2, s.gold]), 0.9, 1.4)
    orb(s, "helix_array", fus[0], fus[1], "HELIX ARRAY", "SPLIT + ORBITALS FUSE", None, "above")
    orb(s, "pierce", ul[0], ul[1], "PIERCE", "PIERCES ONE MORE", (2, 3), "left")
    orb(s, "pulse_ring", ur[0], ur[1], "PULSE RING", "LEVEL 3: SECOND RING", (2, 5), "right")
    orb(s, "on_beat_crit", down[0], down[1], "ON-BEAT CRIT", "DOWNBEAT HITS DOUBLE", (1, 3), "below")
    s.ship(cx, cy, 21, -PI / 2, cells=2)
    return s.save("trio_fusion")


def main():
    shots = [trio_break(), trio_fusion()]
    sheet = Image.new("RGB", (W, H // 2), (0, 0, 0))
    for i, img in enumerate(shots):
        sheet.paste(img.resize((W // 2, H // 2), Image.LANCZOS), (i * (W // 2), 0))
    out = os.path.join(sc.DESIGN, "previews", "trio.png")
    sheet.save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
