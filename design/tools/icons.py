"""RAVEFORM upgrade icons v2: layered, multi-colour light emblems.

Each icon is a stack of layers in unit space (y up, frame radius 1.0): soft glow sprites, a faint glass
fill, gradient strokes, flares. The frame's shape names the category (hexagon = Mod, ring = Riff,
diamond = Boost) and its colours name the group; the emblem inside tells the story in several colours.
Sizes and glow grow with rarity: Mods are tight, Riffs carry an aura, Boosts blaze.

The same layer list is what the engine draws, so everything here uses only what the renderer has:
capsule lines with a two-or-more-colour gradient, additive fills, glow sprites and tapering flares.

Usage:  python design/tools/icons.py     -> design/icons.json, design/previews/icons.png
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

# --------------------------------------------------------------------------- swatches
# Nothing sits within 30 degrees of the danger hue (18): no reds, no hot pinks.
SWATCH_SPECS = dict(
    white=(1.0, 0, 0.0), gold=(0.95, 95, 0.07), ice=(0.93, 220, 0.05),
    sun=(0.90, 98, None), amber=(0.83, 76, None), orange=(0.77, 58, None),
    lime=(0.88, 130, None), green=(0.85, 150, None), mint=(0.87, 166, None), teal=(0.84, 186, None),
    cyan=(0.86, 200, None), sky=(0.80, 226, None), azure=(0.72, 246, None), blue=(0.64, 262, None),
    indigo=(0.58, 278, None), violet=(0.66, 298, None), purple=(0.68, 312, None), magenta=(0.70, 328, None),
)


def swatches():
    out = {}
    for name, (L, h, c) in SWATCH_SPECS.items():
        rgb = ld.oklch(L, h, c=c)[0] if c is not None else ld.oklch(L, h, cf=0.96)[0]
        out[name] = rgb.astype(np.float32)
    return out


# --------------------------------------------------------------------------- layer builders

def ln(segs, colours, grad=None, i=2.2, w=2.0):
    return dict(t="lines", segs=[[round(v, 4) for v in s[:4]] for s in segs],
                colours=colours if isinstance(colours, list) else [colours], grad=grad, i=i, w=w)


def fl(points, colours, grad=None, i=0.08):
    return dict(t="fill", pts=[[round(p[0], 4), round(p[1], 4)] for p in points],
                colours=colours if isinstance(colours, list) else [colours], grad=grad, i=i)


def gl(x, y, r, colour, i):
    return dict(t="glow", x=x, y=y, r=r, colour=colour, i=i)


def fr(x, y, length, colour, i=2.0, arms=4, rot=0.0, w=1.4):
    return dict(t="flare", x=x, y=y, len=length, colour=colour, i=i, arms=arms, rot=rot, w=w)


def P(*pts):
    return ld.polyline(list(pts))


def ring_pts(r, n=28, sy=1.0, rot=0.0):
    c, s = math.cos(rot), math.sin(rot)
    return [((r * math.cos(2 * PI * k / n)) * c - (r * sy * math.sin(2 * PI * k / n)) * s,
             (r * math.cos(2 * PI * k / n)) * s + (r * sy * math.sin(2 * PI * k / n)) * c) for k in range(n)]


def ring(r, n=28, sy=1.0, rot=0.0):
    return ld.polyline(ring_pts(r, n, sy, rot), closed=True)


def dashes(r, count, duty=0.55, rot=0.0, segs=2):
    out = []
    for k in range(count):
        a0 = rot + 2 * PI * k / count
        out += ld.arc(r, a0, a0 + 2 * PI / count * duty, segs)
    return out


def kite(x, y, r, rot=0.0):
    return ld.transform(ld.polyline([(1.0, 0.0), (0.0, 0.55), (-1.0, 0.0), (0.0, -0.55)], closed=True), x, y, r, rot)


def chevron(x, half, depth):
    return P((x, half), (x + depth, 0.0), (x, -half))


def magnet(scale=1.0, dx=0.0, dy=0.0):
    body = ld.arc(0.5, PI, 2 * PI, 10) + P((-0.5, 0.0), (-0.5, 0.38)) + P((0.5, 0.0), (0.5, 0.38))
    poles = P((-0.5, 0.38), (-0.5, 0.6)) + P((0.5, 0.38), (0.5, 0.6))
    return ld.transform(body, dx, dy, scale), ld.transform(poles, dx, dy, scale)


SHIP = [(1.15, 0.0), (0.10, 0.38), (-0.85, 0.95), (-0.45, 0.30), (-0.75, 0.0), (-0.45, -0.30), (-0.85, -0.95), (0.10, -0.38)]


def ship(x, y, r, rot):
    return ld.transform(ld.polyline(SHIP, closed=True), x, y, r, rot)


# --------------------------------------------------------------------------- frames

HEX = [(math.cos(PI / 6 + k * PI / 3), math.sin(PI / 6 + k * PI / 3)) for k in range(6)]


def frame_mod(a, b):
    """Hexagon. a and b are the group's two colours (top and bottom)."""
    inner = [(x * 0.84, y * 0.84) for x, y in HEX]
    return [
        gl(0, 0, 1.35, a, 0.10),
        fl(HEX, [a, b], ("rad", 0, 0.2, 1.1), 0.075),
        ln(ld.polyline(inner, closed=True), "white", None, 0.22, 1.1),
        ln(ld.polyline(HEX, closed=True), [a, "white", b], ("lin", 0, 1, 0, -1), 1.7, 2.3),
        gl(HEX[1][0], HEX[1][1], 0.2, "white", 0.55), gl(HEX[4][0], HEX[4][1], 0.2, b, 0.5),
    ]


def frame_riff(a, b):
    """Ring with a dial: Riffs are instruments."""
    return [
        gl(0, 0, 1.6, a, 0.15), gl(0.25, -0.2, 1.1, b, 0.08),
        fl(ring_pts(1.0), [a, b], ("rad", 0, 0, 1.0), 0.07),
        ln(dashes(0.84, 16, 0.5), "white", None, 0.3, 1.2),
        ln(ring(1.0), [a, b, a, b], ("ang", 0, 0, 0.6), 1.8, 2.5),
        ln(P((0, 1.06), (0, 1.2)) + P((0, -1.06), (0, -1.2)) + P((1.06, 0), (1.2, 0)) + P((-1.06, 0), (-1.2, 0)), "white", None, 0.9, 1.6),
    ]


def frame_boost(a, b):
    """Diamond that blazes: Boosts are the loud ones."""
    dia = [(1.08, 0.0), (0.0, 1.08), (-1.08, 0.0), (0.0, -1.08)]
    inner = [(x * 0.8, y * 0.8) for x, y in dia]
    return [
        gl(0, 0, 1.8, "gold", 0.12), gl(-0.3, 0.3, 1.3, a, 0.13), gl(0.35, -0.3, 1.3, b, 0.13),
        fr(0, 0, 1.7, "gold", 0.5, 2, PI / 4, 1.3), fr(0, 0, 1.6, "white", 0.8, 2, 0.0, 1.3),
        fl(dia, ["gold", a], ("rad", 0, 0, 1.1), 0.10),
        ln(ld.polyline(inner, closed=True), [a, b], ("lin", -1, 0, 1, 0), 0.6, 1.2),
        ln(ld.polyline(dia, closed=True), ["white", "gold", a, "gold", "white"], ("lin", 0, 1.1, 0, -1.1), 2.2, 2.6),
        fr(0, 1.08, 0.42, "white", 2.2, 2, 0.0, 1.2), fr(0, -1.08, 0.42, "white", 2.2, 2, 0.0, 1.2),
        fr(1.05, 0.72, 0.2, "white", 2.0, 2, 0.0, 1.1), fr(-0.92, -0.8, 0.16, "gold", 2.0, 2, 0.0, 1.1),
        fr(-1.12, 0.62, 0.13, a, 2.0, 2, 0.0, 1.0),
    ]


# --------------------------------------------------------------------------- emblems

def e_split():
    return [
        gl(0.3, 0, 0.75, "sun", 0.16),
        ln(P((-0.66, 0), (-0.14, 0)), ["white", "sun"], ("lin", -0.66, 0, -0.14, 0), 2.6, 2.6),
        ln(P((-0.14, 0), (0.56, 0.42)), ["sun", "amber"], ("lin", -0.14, 0, 0.56, 0.42), 2.4, 2.2),
        ln(P((-0.14, 0), (0.66, 0)), ["white", "sun"], ("lin", -0.14, 0, 0.66, 0), 2.8, 2.4),
        ln(P((-0.14, 0), (0.56, -0.42)), ["sun", "orange"], ("lin", -0.14, 0, 0.56, -0.42), 2.4, 2.2),
        gl(0.56, 0.42, 0.2, "amber", 1.1), gl(0.66, 0, 0.22, "white", 1.2), gl(0.56, -0.42, 0.2, "orange", 1.1),
        ln(kite(0.58, 0.43, 0.09, 0.54) + kite(0.68, 0, 0.1) + kite(0.58, -0.43, 0.09, -0.54), "white", None, 3.0, 1.5),
        gl(-0.14, 0, 0.16, "white", 0.9),
    ]


def e_double_time():
    streaks = P((-0.92, 0.27), (-0.66, 0.27)) + P((-0.98, 0.0), (-0.7, 0.0)) + P((-0.92, -0.27), (-0.66, -0.27))
    return [
        gl(0.1, 0, 0.8, "magenta", 0.14),
        ln(streaks, ["indigo", "violet"], ("lin", -1.0, 0, -0.66, 0), 1.3, 1.5),
        ln(chevron(-0.58, 0.46, 0.44), ["violet", "purple", "violet"], ("lin", 0, 0.46, 0, -0.46), 2.2, 2.4),
        ln(chevron(-0.06, 0.52, 0.5), ["magenta", "white", "magenta"], ("lin", 0, 0.52, 0, -0.52), 2.8, 2.8),
        gl(0.44, 0, 0.2, "white", 1.2),
        gl(-0.14, 0, 0.14, "purple", 0.8),
    ]


def e_pierce():
    sparks = P((0.05, 0.0), (0.26, 0.2)) + P((0.05, 0.0), (0.27, -0.18)) + P((0.05, 0.0), (-0.12, 0.2)) + P((0.05, 0.0), (-0.1, -0.2))
    return [
        gl(0.05, 0, 0.5, "cyan", 0.18),
        ln(P((0.05, 0.66), (0.05, 0.2)) + P((0.05, -0.2), (0.05, -0.66)), ["cyan", "blue", "cyan"], ("lin", 0, 0.66, 0, -0.66), 2.0, 2.6),
        ln(sparks, ["white", "cyan"], ("rad", 0.05, 0, 0.3), 2.2, 1.3),
        ln(P((-0.72, 0), (0.62, 0)), ["orange", "sun", "white"], ("lin", -0.72, 0, 0.62, 0), 2.8, 2.4),
        ln(P((0.38, 0.24), (0.68, 0), (0.38, -0.24)), "white", None, 3.0, 2.4),
        gl(0.68, 0, 0.2, "white", 1.2), gl(-0.6, 0, 0.3, "orange", 0.35),
    ]


def e_ricochet():
    return [
        ln(P((-0.55, 0.62), (0.55, 0.62)), ["cyan", "sky", "cyan"], ("lin", -0.55, 0, 0.55, 0), 1.9, 2.6),
        ln(P((-0.7, -0.52), (-0.06, 0.58)), ["orange", "sun"], ("lin", -0.7, -0.52, -0.06, 0.58), 2.4, 2.2),
        ln(P((-0.06, 0.58), (0.6, -0.34)), ["white", "sun", "amber"], ("lin", -0.06, 0.58, 0.6, -0.34), 2.8, 2.2),
        fr(-0.06, 0.58, 0.34, "white", 2.6, 4, PI / 8, 1.3), gl(-0.06, 0.58, 0.26, "sun", 0.9),
        ln(kite(0.62, -0.37, 0.1, -0.95), "white", None, 3.0, 1.5), gl(0.62, -0.37, 0.22, "amber", 1.1),
        ln(P((-0.84, -0.62), (-0.74, -0.56)), "orange", None, 1.0, 1.5),
    ]


def e_overcharge():
    bolt = P((0.24, 0.76), (-0.3, 0.06), (0.16, 0.06), (-0.22, -0.78))
    arcs = P((-0.55, 0.42), (-0.7, 0.2), (-0.52, 0.1), (-0.66, -0.08)) + P((0.52, -0.3), (0.68, -0.12), (0.5, 0.0), (0.64, 0.2))
    return [
        gl(0, 0, 0.85, "sun", 0.2), gl(0, 0, 0.45, "white", 0.25),
        ln(arcs, ["sky", "azure"], ("lin", 0, 0.5, 0, -0.4), 1.6, 1.4),
        ln(bolt, ["orange", "sun", "orange"], ("lin", 0, 0.76, 0, -0.78), 1.6, 4.6),
        ln(bolt, "white", None, 3.2, 2.0),
        gl(-0.22, -0.78, 0.16, "sun", 1.0), gl(0.24, 0.76, 0.14, "white", 0.8),
    ]


def e_extra_cell():
    return [
        gl(0, 0, 0.7, "cyan", 0.14),
        ln(ld.arc(0.68, math.radians(200), math.radians(290), 7) + ld.arc(0.68, math.radians(320), math.radians(410), 7), ["cyan", "sky"], ("rad", 0, 0, 0.7), 1.5, 2.2),
        ln(ld.arc(0.68, math.radians(80), math.radians(170), 7), ["white", "gold"], ("lin", -0.6, 0.5, 0.2, 0.7), 3.0, 2.8),
        gl(-0.4, 0.55, 0.3, "gold", 0.9),
        ln(ship(-0.02, -0.04, 0.3, PI / 2), "cyan", None, 2.2, 2.0), ln(ship(-0.02, -0.04, 0.3, PI / 2), "white", None, 1.6, 0.9),
        ln(P((0.42, 0.62), (0.7, 0.62)) + P((0.56, 0.48), (0.56, 0.76)), "gold", None, 2.8, 2.2),
    ]


def e_magnet():
    body, poles = magnet(0.95, 0.0, -0.12)
    field = dashes(0.3, 5, 0.55, 0.2)
    return [
        gl(0, 0.1, 0.8, "lime", 0.15),
        ln(body, ["lime", "mint", "teal"], ("lin", 0, 0.3, 0, -0.62), 2.4, 3.2),
        ln(poles, "white", None, 3.0, 3.2),
        gl(-0.475, 0.42, 0.2, "white", 0.8), gl(0.475, 0.42, 0.2, "white", 0.8),
        ln(ld.transform(field, 0.0, 0.3), "mint", None, 0.9, 1.2),
        ln(kite(0.0, 0.74, 0.11, PI / 2) + kite(-0.27, 0.6, 0.09, 1.9) + kite(0.27, 0.6, 0.09, 1.2), ["white", "gold"], ("rad", 0, 0.7, 0.4), 3.0, 1.5),
        gl(0.0, 0.7, 0.3, "gold", 0.7),
    ]


def e_on_beat_crit():
    long_rays, short_rays = [], []
    for k in range(4):
        a = k * PI / 2
        long_rays += P((0.14 * math.cos(a), 0.14 * math.sin(a)), (0.66 * math.cos(a), 0.66 * math.sin(a)))
        b = a + PI / 4
        short_rays += P((0.2 * math.cos(b), 0.2 * math.sin(b)), (0.46 * math.cos(b), 0.46 * math.sin(b)))
    return [
        gl(0, 0, 0.8, "purple", 0.2),
        ln(dashes(0.72, 12, 0.5), "indigo", None, 1.3, 1.4),
        ln(ring(0.5, 24), ["violet", "magenta", "violet", "magenta"], ("ang", 0, 0, 0.0), 1.8, 1.8),
        ln(short_rays, ["white", "violet"], ("rad", 0, 0, 0.46), 2.2, 1.6),
        ln(long_rays, ["white", "magenta", "purple"], ("rad", 0, 0, 0.66), 2.8, 2.4),
        gl(0, 0, 0.24, "white", 1.6),
        gl(0.66, 0, 0.12, "magenta", 0.9), gl(-0.66, 0, 0.12, "magenta", 0.9), gl(0, 0.66, 0.12, "magenta", 0.9), gl(0, -0.66, 0.12, "magenta", 0.9),
    ]


def e_orbitals():
    tilt = 0.42
    path = ring(0.66, 28, 0.5, tilt)
    pts = ring_pts(0.66, 28, 0.5, tilt)
    a, b = pts[3], pts[17]
    trail_a = ld.polyline(pts[0:4])
    trail_b = ld.polyline(pts[14:18])
    return [
        gl(0, 0, 0.5, "white", 0.3),
        ln(path, ["teal", "cyan", "teal", "mint"], ("ang", 0, 0, 0.0), 0.9, 1.3),
        ln(trail_a, ["teal", "white"], ("lin", pts[0][0], pts[0][1], a[0], a[1]), 2.6, 2.2),
        ln(trail_b, ["mint", "white"], ("lin", pts[14][0], pts[14][1], b[0], b[1]), 2.6, 2.2),
        ln(kite(a[0], a[1], 0.15, 1.9), "cyan", None, 3.0, 2.0), gl(a[0], a[1], 0.26, "cyan", 1.0),
        ln(kite(b[0], b[1], 0.15, 5.0), "lime", None, 3.0, 2.0), gl(b[0], b[1], 0.26, "lime", 1.0),
        ln(ld.transform(ld.polygon(4, 0.17), 0, 0), "white", None, 3.0, 2.0), gl(0, 0, 0.22, "white", 1.0),
    ]


def e_pulse_ring():
    return [
        gl(0, 0, 0.9, "sky", 0.2),
        ln(dashes(0.78, 14, 0.5), "azure", None, 1.2, 1.4),
        ln(ring(0.54, 26), ["cyan", "sky"], ("ang", 0, 0, 0.0), 2.0, 2.0),
        ln(ring(0.3, 20), ["white", "cyan"], ("rad", 0, 0, 0.3), 3.0, 2.8),
        gl(0, 0, 0.2, "white", 1.4),
        ln(P((0, 0.82), (0, 0.94)) + P((0, -0.82), (0, -0.94)) + P((0.82, 0), (0.94, 0)) + P((-0.82, 0), (-0.94, 0)), "white", None, 1.6, 1.5),
    ]


def e_sentinel():
    return [
        gl(-0.22, -0.2, 0.55, "amber", 0.2),
        ln(ld.transform(ld.polygon(4, 0.34), -0.22, -0.2), ["sun", "orange"], ("lin", -0.22, 0.14, -0.22, -0.54), 2.4, 2.4),
        ln(P((-0.56, -0.2), (-0.74, -0.2)) + P((-0.22, -0.54), (-0.22, -0.72)), "amber", None, 1.4, 1.8),
        gl(-0.22, -0.2, 0.14, "white", 1.5),
        ln(P((0.04, 0.06), (0.62, 0.64)), ["white", "sun", "amber"], ("lin", 0.04, 0.06, 0.62, 0.64), 2.8, 2.2),
        fr(0.04, 0.06, 0.24, "white", 2.4, 2, PI / 4, 1.2),
        fr(0.64, 0.66, 0.26, "sun", 2.4, 4, 0.0, 1.2), gl(0.64, 0.66, 0.2, "white", 1.0),
    ]


def e_prism():
    tri = [(0.46 * math.cos(PI / 2 + k * 2 * PI / 3) - 0.08, 0.46 * math.sin(PI / 2 + k * 2 * PI / 3) - 0.05) for k in range(3)]
    rays = [(0.44, "purple"), (0.24, "azure"), (0.04, "mint"), (-0.16, "sun"), (-0.36, "orange")]
    layers = [
        gl(0.4, 0, 0.9, "ice", 0.14),
        ln(P((-0.9, -0.04), (-0.3, -0.04)), ["ice", "white"], ("lin", -0.9, 0, -0.3, 0), 2.6, 2.6),
        fl(tri, ["white", "ice"], ("rad", -0.08, -0.05, 0.5), 0.2),
        ln(ld.polyline(tri, closed=True), ["white", "ice", "white"], ("lin", -0.5, 0.4, 0.3, -0.3), 2.6, 2.2),
        gl(0.14, 0.02, 0.2, "white", 1.0),
    ]
    for y, colour in rays:
        layers.append(ln(P((0.14, 0.02), (0.9, y)), ["white", colour, colour], ("lin", 0.14, 0.02, 0.9, y), 2.6, 2.3))
        layers.append(gl(0.9, y, 0.16, colour, 0.9))
    return layers


def e_hyper():
    streaks = P((-1.0, 0.36), (-0.6, 0.36)) + P((-1.05, 0.12), (-0.7, 0.12)) + P((-1.05, -0.12), (-0.7, -0.12)) + P((-1.0, -0.36), (-0.6, -0.36))
    return [
        gl(0.3, 0, 0.9, "sun", 0.2),
        ln(streaks, ["orange", "amber"], ("lin", -1.05, 0, -0.6, 0), 1.2, 1.5),
        ln(chevron(-0.72, 0.36, 0.36), "orange", None, 1.7, 2.0),
        ln(chevron(-0.3, 0.44, 0.42), ["amber", "sun", "amber"], ("lin", 0, 0.44, 0, -0.44), 2.3, 2.4),
        ln(chevron(0.14, 0.54, 0.5), ["sun", "white", "sun"], ("lin", 0, 0.54, 0, -0.54), 3.0, 3.0),
        gl(0.64, 0, 0.24, "white", 1.4), fr(0.64, 0, 0.36, "white", 2.2, 2, 0.0, 1.2),
    ]


def e_halo():
    return [
        gl(0, 0, 0.9, "cyan", 0.22),
        fl(ring_pts(0.68, 24), ["cyan", "blue"], ("rad", 0, 0, 0.68), 0.10),
        ln(ring(0.68, 28), ["cyan", "white", "sky", "white"], ("ang", 0, 0, 0.4), 2.6, 2.8),
        ln(dashes(0.84, 10, 0.4, 0.3), "ice", None, 1.2, 1.3),
        ln(ship(0, -0.04, 0.3, PI / 2), "cyan", None, 2.4, 2.0), ln(ship(0, -0.04, 0.3, PI / 2), "white", None, 1.8, 0.9),
        fr(0.48, 0.48, 0.22, "white", 2.4, 2, 0.0, 1.1), fr(-0.5, -0.46, 0.16, "ice", 2.0, 2, 0.0, 1.0),
    ]


def e_magnet_storm():
    body, poles = magnet(0.58, 0.0, -0.36)
    layers = [
        gl(0, 0.2, 0.9, "gold", 0.16), gl(0, -0.4, 0.6, "violet", 0.2),
        ln(body, ["purple", "violet", "indigo"], ("lin", 0, -0.1, 0, -0.66), 2.4, 3.0),
        ln(poles, "white", None, 3.0, 3.0),
    ]
    spiral = []
    for k in range(40):
        t = k / 39.0
        a = 1.2 + t * 4.4
        r = 0.86 - 0.62 * t
        spiral.append((r * math.cos(a), r * math.sin(a) * 0.82 + 0.16))
    layers.append(ln(ld.polyline(spiral), ["gold", "sun", "white"], ("lin", 0.5, 0.9, 0.0, 0.0), 0.9, 1.2))
    for k, idx in enumerate((2, 8, 14, 20, 26, 31, 36)):
        x, y = spiral[idx]
        size = 0.13 - 0.01 * k
        layers.append(ln(kite(x, y, size, 1.2 + idx * 0.11), ["white", "gold"], ("rad", x, y, size), 3.0, 1.5))
        layers.append(gl(x, y, 0.2, "gold", 0.6))
    return layers


# --------------------------------------------------------------------------- the rest of the catalogue
# More small helpers, then one emblem per upgrade. Same rules as above: a few strokes, two or three
# colours, hot points where the eye should land.

def at(segs, x=0.0, y=0.0, s=1.0, rot=0.0):
    return ld.transform(segs, x, y, s, rot)


def poly(n, r, x=0.0, y=0.0, rot=0.0):
    return ld.transform(ld.polygon(n, r, rot), x, y)


def arrow(x0, y0, x1, y1, head=0.2):
    a = math.atan2(y1 - y0, x1 - x0)
    return P((x0, y0), (x1, y1)) + P((x1 - head * math.cos(a - 0.5), y1 - head * math.sin(a - 0.5)), (x1, y1),
                                     (x1 - head * math.cos(a + 0.5), y1 - head * math.sin(a + 0.5)))


def bez(p0, p1, p2, n=12):
    return [((1 - t) ** 2 * p0[0] + 2 * (1 - t) * t * p1[0] + t * t * p2[0],
             (1 - t) ** 2 * p0[1] + 2 * (1 - t) * t * p1[1] + t * t * p2[1]) for t in [k / n for k in range(n + 1)]]


def sine(x0, x1, y, amp, cycles, n=28, phase=0.0):
    return [(x0 + (x1 - x0) * k / n, y + amp * math.sin(phase + 2 * PI * cycles * k / n)) for k in range(n + 1)]


def burst(x, y, r0, r1, n, rot=0.0):
    return [(x + r0 * math.cos(rot + 2 * PI * k / n), y + r0 * math.sin(rot + 2 * PI * k / n),
             x + r1 * math.cos(rot + 2 * PI * k / n), y + r1 * math.sin(rot + 2 * PI * k / n)) for k in range(n)]


def plus(x, y, r):
    return P((x - r, y), (x + r, y)) + P((x, y - r), (x, y + r))


def e_fork():
    return [gl(0.1, 0, 0.7, "sun", 0.14),
            ln(P((-0.75, 0), (-0.14, 0)), ["orange", "sun"], ("lin", -0.75, 0, -0.14, 0), 2.6, 2.4),
            ln(poly(6, 0.17, 0.0, 0.0), ["teal", "white"], ("rad", 0, 0, 0.17), 2.4, 1.8),
            ln(arrow(0.14, 0.09, 0.68, 0.46, 0.18), ["sun", "white"], ("lin", 0.14, 0.09, 0.68, 0.46), 2.7, 2.0),
            ln(arrow(0.14, -0.09, 0.68, -0.46, 0.18), ["sun", "white"], ("lin", 0.14, -0.09, 0.68, -0.46), 2.7, 2.0),
            gl(0.0, 0, 0.2, "white", 0.9), gl(0.68, 0.46, 0.14, "sun", 0.8), gl(0.68, -0.46, 0.14, "sun", 0.8)]


def e_seeker():
    pts = bez((-0.72, -0.52), (0.2, -0.62), (0.4, 0.2))
    return [gl(0.46, 0.34, 0.5, "teal", 0.2),
            ln(ld.polyline(pts), ["orange", "sun", "white"], ("lin", -0.72, -0.52, 0.4, 0.2), 2.7, 2.3),
            ln(P((0.2, 0.06), (0.4, 0.2), (0.44, -0.04)), "white", None, 3.0, 2.2),
            ln(at(ring(0.22, 18), 0.48, 0.38), ["teal", "mint"], ("ang", 0.48, 0.38, 0), 2.2, 1.8),
            ln(at(burst(0, 0, 0.28, 0.4, 4), 0.48, 0.38), "white", None, 1.8, 1.4), gl(0.48, 0.38, 0.1, "white", 1.2)]


def e_heavy_step():
    return [gl(0.44, 0, 0.6, "orange", 0.2),
            gl(-0.66, 0, 0.1, "sun", 1.4), gl(-0.36, 0, 0.1, "sun", 1.4), gl(-0.06, 0, 0.1, "sun", 1.4),
            ln(at(ring(0.26, 20), 0.44, 0), ["white", "sun", "orange"], ("rad", 0.44, 0, 0.26), 3.0, 2.6),
            ln(at(ld.arc(0.44, -0.9, 0.9, 8), 0.44, 0), "orange", None, 1.4, 1.5), gl(0.44, 0, 0.16, "white", 1.4)]


def e_accent():
    return [gl(0, 0.05, 0.8, "sun", 0.14),
            ln(P((-0.86, 0), (0.86, 0)), ["orange", "sun", "orange"], ("lin", -0.86, 0, 0.86, 0), 1.6, 5.0),
            ln(P((-0.86, 0), (0.86, 0)), "white", None, 3.2, 2.0),
            ln(P((-0.22, 0.62), (0.2, 0.44), (-0.22, 0.26)), ["sun", "white"], ("lin", -0.22, 0, 0.2, 0), 2.8, 2.4),
            ln(P((-0.45, -0.34), (-0.45, -0.58)), "white", None, 2.8, 2.4),
            ln(P((-0.15, -0.38), (-0.15, -0.54)) + P((0.15, -0.38), (0.15, -0.54)) + P((0.45, -0.38), (0.45, -0.54)), "orange", None, 1.2, 1.6),
            gl(-0.45, -0.46, 0.14, "sun", 0.9)]


def e_echo():
    return [gl(0.2, 0.2, 0.7, "sun", 0.14),
            ln(arrow(-0.74, -0.34, 0.14, -0.34, 0.16), "indigo", None, 0.9, 1.8),
            ln(arrow(-0.52, -0.04, 0.4, -0.04, 0.18), ["violet", "purple"], ("lin", -0.52, 0, 0.4, 0), 1.6, 2.0),
            ln(arrow(-0.26, 0.28, 0.7, 0.28, 0.2), ["orange", "sun", "white"], ("lin", -0.26, 0, 0.7, 0), 2.9, 2.4),
            gl(0.7, 0.28, 0.16, "white", 1.1)]


def e_wave():
    pts = sine(-0.78, 0.5, 0.0, 0.3, 1.5)
    return [gl(0, 0, 0.75, "sun", 0.13),
            ln(ld.polyline(pts), ["orange", "sun", "white"], ("lin", -0.78, 0, 0.5, 0), 2.8, 2.4),
            ln(P((0.44, -0.5), (0.72, -0.3), (0.4, -0.16)), "white", None, 3.0, 2.2), gl(0.7, -0.3, 0.14, "white", 1.0)]


def e_rear_guard():
    return [gl(0, 0, 0.6, "sun", 0.13),
            ln(ship(0.0, 0.0, 0.3, 0.0), ["white", "cyan"], ("rad", 0, 0, 0.36), 2.6, 2.0),
            ln(P((0.46, 0.1), (0.78, 0.1)) + P((0.46, -0.1), (0.78, -0.1)), ["sun", "white"], ("lin", 0.46, 0, 0.78, 0), 3.0, 2.2),
            ln(P((-0.46, 0.1), (-0.78, 0.1)) + P((-0.46, -0.1), (-0.78, -0.1)), ["orange", "sun"], ("lin", -0.46, 0, -0.78, 0), 2.2, 2.0),
            gl(0.78, 0, 0.14, "white", 0.9), gl(-0.78, 0, 0.14, "orange", 0.8)]


def e_arc():
    nodes = [(-0.56, -0.32), (0.0, 0.42), (0.56, -0.22)]
    zap1 = P(nodes[0], (-0.4, 0.0), (-0.22, -0.06), nodes[1])
    zap2 = P(nodes[1], (0.2, 0.12), (0.3, -0.04), nodes[2])
    layers = [gl(0, 0, 0.75, "sky", 0.16), ln(zap1 + zap2, ["white", "sky", "white"], ("lin", -0.56, 0, 0.56, 0), 2.8, 2.0)]
    for x, y in nodes:
        layers += [ln(poly(4, 0.13, x, y), "teal", None, 2.6, 1.8), gl(x, y, 0.2, "white", 0.9)]
    return layers


def e_burn():
    flame = [(0.0, -0.7), (-0.42, -0.22), (-0.3, 0.22), (-0.12, 0.02), (0.02, 0.76), (0.22, 0.18), (0.42, -0.18)]
    inner = [(0.0, -0.48), (-0.18, -0.2), (0.0, 0.26), (0.16, -0.16)]
    return [gl(0, -0.1, 0.7, "orange", 0.24),
            fl(flame, ["sun", "orange"], ("lin", 0, -0.7, 0, 0.76), 0.16),
            ln(ld.polyline(flame, closed=True), ["white", "sun", "orange"], ("lin", 0, -0.7, 0, 0.76), 2.7, 2.3),
            ln(ld.polyline(inner, closed=True), ["white", "sun"], ("lin", 0, -0.48, 0, 0.26), 2.6, 1.7), gl(0, -0.3, 0.22, "white", 1.0)]


def e_shatter():
    layers = [gl(0, 0, 0.7, "teal", 0.14), ln(burst(0, 0, 0.08, 0.26, 8, 0.2), ["white", "sun"], ("rad", 0, 0, 0.26), 2.6, 1.5), gl(0, 0, 0.16, "white", 1.2)]
    for k in range(6):
        a = k * PI / 3 + 0.3
        cx, cy = 0.56 * math.cos(a), 0.56 * math.sin(a)
        seg = at([(-0.2, 0.0, 0.2, 0.0)], cx, cy, 1.0, a + PI / 2 + 0.5 * (-1) ** k)
        layers.append(ln(seg, ["teal", "white"], ("rad", 0, 0, 0.8), 2.5, 2.0))
    return layers


def e_gravity_rounds():
    layers = [gl(0, 0, 0.6, "orange", 0.18), ln(ring(0.2, 16), ["white", "sun"], ("rad", 0, 0, 0.2), 2.8, 2.0),
              ln(ld.arc(0.42, 0.4, 2.6, 8) + ld.arc(0.42, 3.6, 5.8, 8), "orange", None, 1.5, 1.5), gl(0, 0, 0.14, "white", 1.4)]
    for k in range(4):
        a = PI / 4 + k * PI / 2
        layers.append(ln(arrow(0.8 * math.cos(a), 0.8 * math.sin(a), 0.5 * math.cos(a), 0.5 * math.sin(a), 0.14), "sun", None, 2.4, 1.8))
    return layers


def e_regen():
    return [gl(0, 0, 0.7, "cyan", 0.14),
            ln(ld.arc(0.62, math.radians(110), math.radians(400), 16), ["blue", "cyan", "white"], ("ang", 0, 0, 1.9), 2.5, 2.4),
            ln(P((0.56, 0.62), (0.4, 0.48), (0.62, 0.36)), "white", None, 3.0, 2.2),
            ln(plus(0, 0, 0.24), ["white", "gold"], ("rad", 0, 0, 0.24), 3.0, 2.6), gl(0, 0, 0.26, "gold", 0.7)]


def e_afterimage():
    return [gl(0.3, 0.2, 0.6, "cyan", 0.14),
            ln(ship(-0.5, -0.36, 0.22, 0.6), "blue", None, 0.9, 1.5),
            ln(ship(-0.1, -0.08, 0.26, 0.6), ["sky", "blue"], ("rad", -0.1, -0.08, 0.3), 1.6, 1.8),
            ln(ship(0.36, 0.24, 0.3, 0.6), ["white", "cyan"], ("rad", 0.36, 0.24, 0.36), 2.8, 2.2),
            ln(ship(0.36, 0.24, 0.3, 0.6), "white", None, 1.8, 0.9)]


def e_reserve():
    return [gl(0.1, 0, 0.7, "cyan", 0.13),
            ln(arrow(-0.6, 0.24, 0.5, 0.24, 0.2), ["blue", "cyan", "white"], ("lin", -0.6, 0, 0.5, 0), 2.7, 2.4),
            ln(arrow(-0.6, -0.24, 0.5, -0.24, 0.2), ["blue", "cyan", "white"], ("lin", -0.6, 0, 0.5, 0), 2.7, 2.4),
            ln(plus(0.72, 0.56, 0.14), "gold", None, 2.8, 2.2)]


def e_shockstep():
    return [gl(-0.38, 0, 0.6, "cyan", 0.2),
            ln(at(ring(0.3, 20), -0.38, 0), ["white", "cyan"], ("rad", -0.38, 0, 0.3), 2.6, 2.2),
            ln(at(burst(0, 0, 0.38, 0.52, 8, 0.39), -0.38, 0), "sky", None, 1.6, 1.5),
            ln(arrow(-0.06, 0, 0.76, 0, 0.22), ["cyan", "white"], ("lin", -0.06, 0, 0.76, 0), 2.9, 2.6), gl(0.76, 0, 0.16, "white", 1.0)]


def e_aegis():
    return [gl(0.3, 0, 0.7, "cyan", 0.16),
            ln(ship(-0.2, 0.0, 0.28, 0.0), ["white", "cyan"], ("rad", -0.2, 0, 0.34), 2.4, 1.9),
            ln(at(ld.arc(0.62, -0.95, 0.95, 12), -0.2, 0), ["sky", "white", "sky"], ("lin", 0, -0.5, 0, 0.5), 2.9, 3.4),
            ln(P((0.86, 0.5), (0.5, 0.2), (0.8, -0.1)), ["amber", "sun"], ("lin", 0.86, 0.5, 0.8, -0.1), 1.8, 1.5),
            gl(0.48, 0.2, 0.16, "white", 1.0)]


def e_last_light():
    return [gl(0, 0, 0.75, "gold", 0.18),
            ln(ld.arc(0.66, 0.5, 1.5, 5) + ld.arc(0.66, 2.6, 3.6, 5) + ld.arc(0.66, 4.7, 5.7, 5), ["cyan", "blue"], ("rad", 0, 0, 0.7), 1.8, 2.0),
            ln(ld.star(4, 0.46, 0.16, 0.0), ["white", "gold"], ("rad", 0, 0, 0.46), 3.0, 2.2),
            fr(0, 0, 0.7, "white", 1.6, 2, PI / 4, 1.2), gl(0, 0, 0.2, "white", 1.5)]


def e_slipstream():
    streaks = P((-0.8, 0.3), (-0.2, 0.3)) + P((-0.9, 0.1), (-0.3, 0.1)) + P((-0.9, -0.1), (-0.3, -0.1)) + P((-0.8, -0.3), (-0.2, -0.3))
    return [gl(0.3, 0, 0.6, "cyan", 0.15),
            ln(streaks, ["blue", "cyan"], ("lin", -0.9, 0, -0.2, 0), 1.5, 1.6),
            ln(ship(0.34, 0.0, 0.34, 0.0), ["white", "cyan"], ("rad", 0.34, 0, 0.4), 2.8, 2.2),
            ln(ship(0.34, 0.0, 0.34, 0.0), "white", None, 1.8, 0.9)]


def e_prospector():
    return [gl(-0.1, 0, 0.7, "gold", 0.18),
            ln(kite(-0.16, -0.04, 0.5, PI / 2), ["white", "gold", "amber"], ("lin", -0.16, 0.46, -0.16, -0.54), 2.8, 2.3),
            ln(kite(0.44, -0.3, 0.22, PI / 2), ["white", "gold"], ("lin", 0.44, -0.08, 0.44, -0.52), 2.6, 1.8),
            ln(plus(0.5, 0.42, 0.16), ["lime", "white"], ("rad", 0.5, 0.42, 0.16), 2.8, 2.2), gl(-0.16, 0, 0.2, "white", 0.8)]


def e_nova_stock():
    return [gl(-0.08, -0.04, 0.8, "cyan", 0.18),
            ln(at(ld.star(4, 0.6, 0.2, PI / 4), -0.08, -0.04), ["white", "cyan", "sky"], ("rad", -0.08, -0.04, 0.6), 2.9, 2.4),
            fr(-0.08, -0.04, 0.8, "white", 1.2, 2, 0.0, 1.2), gl(-0.08, -0.04, 0.2, "white", 1.5),
            ln(plus(0.56, 0.5, 0.16), "gold", None, 2.8, 2.2)]


def e_encore():
    return [gl(0, 0, 0.7, "lime", 0.14),
            ln(poly(4, 0.3), ["white", "gold"], ("rad", 0, 0, 0.3), 2.8, 2.2),
            ln(ld.arc(0.62, math.radians(60), math.radians(340), 16), ["teal", "lime", "white"], ("ang", 0, 0, 1.0), 2.4, 2.2),
            ln(P((0.44, -0.4), (0.6, -0.2), (0.72, -0.44)), "white", None, 3.0, 2.0), gl(0, 0, 0.16, "gold", 1.0)]


def e_lucky_break():
    layers = [gl(0.4, 0.0, 0.5, "gold", 0.22)]
    for k, x in enumerate((-0.6, -0.2, 0.2)):
        layers.append(ln(at(ring(0.15, 14), x, -0.1), "teal", None, 1.5, 1.6))
    layers += [ln(at(ring(0.19, 16), 0.62, -0.1), ["white", "gold"], ("rad", 0.62, -0.1, 0.19), 3.0, 2.2), gl(0.62, -0.1, 0.14, "white", 1.0),
               ln(plus(0.62, 0.36, 0.14), "lime", None, 2.8, 2.2)]
    return layers


def e_collector():
    layers = [gl(0.36, 0, 0.6, "gold", 0.2), ln(poly(4, 0.3, 0.4, 0.0), ["white", "gold", "amber"], ("rad", 0.4, 0, 0.3), 2.9, 2.3)]
    for y in (0.46, 0.0, -0.46):
        layers.append(ln(kite(-0.66, y, 0.11, 0.0), ["white", "gold"], ("rad", -0.66, y, 0.11), 2.6, 1.5))
        layers.append(ln(arrow(-0.48, y * 0.8, -0.06, y * 0.25, 0.12), ["teal", "lime"], ("lin", -0.48, 0, -0.06, 0), 1.9, 1.5))
    return layers


def e_syncopation():
    layers = [gl(0, 0, 0.75, "purple", 0.16), ln(P((-0.8, -0.3), (0.8, -0.3)), "indigo", None, 1.2, 1.4)]
    for k in range(4):
        x = -0.6 + k * 0.4
        layers.append(ln(P((x, -0.3), (x, -0.06)), "violet", None, 1.4, 1.8))
    for k in range(4):
        x = -0.4 + k * 0.4
        layers += [ln(poly(4, 0.13, x, 0.26), ["white", "magenta"], ("rad", x, 0.26, 0.13), 2.9, 2.0), gl(x, 0.26, 0.16, "magenta", 0.8)]
    return layers


def e_bar_line():
    return [gl(0, 0, 0.8, "magenta", 0.18),
            ln(P((-0.84, 0), (0.84, 0)) + P((0, -0.84), (0, 0.84)), ["magenta", "purple", "magenta"], ("rad", 0, 0, 0.84), 1.6, 4.6),
            ln(P((-0.84, 0), (0.84, 0)) + P((0, -0.84), (0, 0.84)), "white", None, 3.0, 1.8),
            ln(P((-0.14, 0.5), (-0.14, -0.5)), "violet", None, 1.6, 1.4),
            gl(0, 0, 0.22, "white", 1.6), gl(0.84, 0, 0.1, "magenta", 0.9), gl(-0.84, 0, 0.1, "magenta", 0.9), gl(0, 0.84, 0.1, "magenta", 0.9), gl(0, -0.84, 0.1, "magenta", 0.9)]


def e_crescendo():
    layers = [gl(0.3, 0, 0.7, "magenta", 0.16),
              ln(P((0.78, 0.46), (-0.78, 0.0), (0.78, -0.46)), ["indigo", "magenta", "white"], ("lin", -0.78, 0, 0.78, 0), 2.6, 2.3)]
    for k in range(5):
        x = -0.36 + k * 0.24
        h = 0.05 + 0.065 * k
        layers.append(ln(P((x, -h), (x, h)), ["violet", "white"], ("lin", -0.4, 0, 0.7, 0), 1.4 + 0.4 * k, 2.2))
    return layers


def e_kick_pulse():
    return [gl(0, 0, 0.8, "purple", 0.2),
            ln(ring(0.3, 20), ["white", "magenta"], ("rad", 0, 0, 0.3), 3.0, 2.8), ln(ring(0.12, 12), "white", None, 2.6, 1.8),
            ln(ld.arc(0.52, -0.7, 0.7, 8) + ld.arc(0.52, PI - 0.7, PI + 0.7, 8), ["magenta", "violet"], ("rad", 0, 0, 0.52), 2.0, 2.0),
            ln(ld.arc(0.74, -0.5, 0.5, 6) + ld.arc(0.74, PI - 0.5, PI + 0.5, 6), "indigo", None, 1.3, 1.6), gl(0, 0, 0.18, "white", 1.4)]


def e_afterparty():
    return [gl(0, 0.1, 0.8, "magenta", 0.18),
            ln(ship(0.0, -0.36, 0.26, PI / 2), ["white", "cyan"], ("rad", 0, -0.36, 0.32), 2.5, 1.9),
            ln(P((-0.34, 0.14), (0.0, 0.42), (0.34, 0.14)), ["purple", "magenta", "purple"], ("lin", -0.34, 0, 0.34, 0), 2.4, 2.4),
            ln(P((-0.4, 0.44), (0.0, 0.76), (0.4, 0.44)), ["magenta", "white", "magenta"], ("lin", -0.4, 0, 0.4, 0), 2.9, 2.6),
            fr(-0.6, 0.5, 0.16, "gold", 2.2, 2, 0.0, 1.1), fr(0.62, 0.3, 0.2, "white", 2.2, 2, 0.0, 1.1), fr(0.5, -0.5, 0.13, "gold", 2.0, 2, 0.0, 1.0)]


# ---- Riffs

def e_lancer():
    return [gl(0.2, 0.2, 0.8, "violet", 0.2),
            gl(-0.66, -0.66, 0.08, "purple", 1.2), gl(-0.52, -0.52, 0.1, "purple", 1.4), gl(-0.36, -0.36, 0.12, "white", 1.4),
            ln(P((-0.24, -0.24), (0.66, 0.66)), ["purple", "violet", "purple"], ("lin", -0.24, -0.24, 0.66, 0.66), 1.7, 6.0),
            ln(P((-0.24, -0.24), (0.66, 0.66)), "white", None, 3.2, 2.2),
            fr(0.66, 0.66, 0.3, "white", 2.4, 2, 0.0, 1.2), gl(0.66, 0.66, 0.2, "white", 1.2)]


def e_minelayer():
    layers = [gl(0.5, -0.05, 0.5, "sun", 0.24)]
    for x in (-0.52, 0.0):
        layers += [ln(at(ring(0.15, 14), x, -0.05), ["amber", "orange"], ("rad", x, -0.05, 0.15), 2.4, 2.0),
                   ln(at(burst(0, 0, 0.19, 0.27, 4, PI / 4), x, -0.05), "amber", None, 1.8, 1.5), gl(x, -0.05, 0.07, "white", 1.2)]
    layers += [ln(at(burst(0, 0, 0.1, 0.4, 10, 0.2), 0.52, -0.05), ["white", "sun", "orange"], ("rad", 0.52, -0.05, 0.4), 2.8, 1.8),
               gl(0.52, -0.05, 0.16, "white", 1.5),
               ln(P((-0.7, -0.5), (0.7, -0.5)), "orange", None, 1.0, 1.3),
               ln(P((0.52, -0.5), (0.52, -0.62)), "white", None, 2.4, 2.0)]
    return layers


def e_storm_coil():
    coil = P((0.0, -0.7), (-0.16, -0.5), (0.16, -0.3), (-0.16, -0.1), (0.16, 0.1), (-0.16, 0.3), (0.0, 0.5))
    zaps = P((0.0, 0.5), (0.3, 0.44), (0.4, 0.62), (0.74, 0.5)) + P((0.0, 0.5), (-0.3, 0.6), (-0.42, 0.42), (-0.74, 0.56)) + P((0.0, 0.5), (0.06, 0.66), (-0.06, 0.74), (0.02, 0.86))
    return [gl(0, 0.4, 0.7, "sky", 0.22),
            ln(coil, ["azure", "sky", "white"], ("lin", 0, -0.7, 0, 0.5), 2.5, 2.4),
            ln(zaps, ["white", "sky"], ("rad", 0, 0.5, 0.8), 2.8, 1.7), gl(0, 0.5, 0.2, "white", 1.4)]


def e_swarm():
    layers = [gl(0.2, 0.2, 0.8, "lime", 0.15)]
    for k in range(5):
        a = -0.5 + k * 0.38
        x, y = -0.1 + 0.62 * math.cos(a), -0.3 + 0.78 * math.sin(a + 0.5)
        tx, ty = -0.6, -0.6
        layers.append(ln(P((tx + (x - tx) * 0.25, ty + (y - ty) * 0.25), (x, y)), ["teal", "lime"], ("lin", tx, ty, x, y), 1.3, 1.3))
        layers.append(ln(kite(x, y, 0.12, math.atan2(y - ty, x - tx)), ["white", "lime"], ("rad", x, y, 0.12), 2.9, 1.7))
        layers.append(gl(x, y, 0.14, "mint", 0.8))
    return layers


def e_singularity():
    spiral = [((0.72 - 0.6 * k / 40) * math.cos(0.4 + k * 0.31), (0.72 - 0.6 * k / 40) * math.sin(0.4 + k * 0.31)) for k in range(41)]
    return [gl(0, 0, 0.85, "violet", 0.2),
            ln(ld.polyline(spiral), ["orange", "purple", "violet", "white"], ("rad", 0, 0, 0.74), 2.3, 2.0),
            ln(ring(0.14, 14), "white", None, 3.0, 2.0),
            ln(arrow(0.86, 0.3, 0.6, 0.2, 0.12) + arrow(-0.7, 0.56, -0.5, 0.38, 0.12) + arrow(-0.1, -0.88, -0.08, -0.62, 0.12), "orange", None, 2.0, 1.5)]


def e_mirror():
    return [gl(-0.36, 0, 0.5, "cyan", 0.2),
            ln([(0.0, -0.76 + 0.2 * k, 0.0, -0.66 + 0.2 * k) for k in range(8)], "white", None, 1.4, 1.4),
            ln(ship(-0.4, 0.0, 0.32, PI / 2), ["white", "cyan"], ("rad", -0.4, 0, 0.38), 2.8, 2.1), ln(ship(-0.4, 0.0, 0.32, PI / 2), "white", None, 1.8, 0.9),
            ln(ship(0.4, 0.0, 0.32, PI / 2), ["sky", "azure"], ("rad", 0.4, 0, 0.38), 1.7, 1.8)]


def e_comet():
    path = [(0.62 * math.cos(a), 0.4 * math.sin(a) - 0.05) for a in [0.6 + k * 0.26 for k in range(20)]]
    x, y = path[-1]
    return [gl(x, y, 0.4, "teal", 0.24),
            ln(ld.polyline(path), ["blue", "teal", "white"], ("lin", path[0][0], path[0][1], x, y), 2.2, 2.0),
            ln(at(ring(0.17, 14), x, y), ["white", "cyan"], ("rad", x, y, 0.17), 3.0, 2.4), gl(x, y, 0.1, "white", 1.4)]


def e_sweeper():
    a = 0.7
    tip = (0.72 * math.cos(a), 0.72 * math.sin(a))
    return [gl(0, 0, 0.8, "magenta", 0.18),
            ln(ld.arc(0.72, a + 0.1, a + 2.2, 14), ["purple", "indigo"], ("ang", 0, 0, a), 1.5, 1.8),
            ln(ld.arc(0.5, a + 0.1, a + 1.5, 10), "indigo", None, 1.0, 1.4),
            ln(P((0, 0), tip), ["magenta", "purple"], ("lin", 0, 0, tip[0], tip[1]), 1.7, 5.4),
            ln(P((0, 0), tip), "white", None, 3.2, 2.0), gl(0, 0, 0.16, "white", 1.4), gl(tip[0], tip[1], 0.14, "magenta", 1.0)]


def e_flak():
    layers = [gl(0, 0, 0.8, "orange", 0.18), ln(burst(0, 0, 0.16, 0.5, 8, PI / 8), ["white", "sun", "orange"], ("rad", 0, 0, 0.5), 2.6, 2.0), gl(0, 0, 0.18, "white", 1.4)]
    for k in range(8):
        a = PI / 8 + k * PI / 4
        layers.append(gl(0.66 * math.cos(a), 0.66 * math.sin(a), 0.1, "sun", 1.5))
    return layers


# ---- Boosts

def e_supernova():
    return [gl(0.3, 0, 0.9, "orange", 0.24),
            ln(P((-0.86, 0), (-0.2, 0)), ["orange", "sun", "white"], ("lin", -0.86, 0, -0.2, 0), 2.7, 2.6),
            ln(at(burst(0, 0, 0.14, 0.62, 6, 0.0), 0.3, 0), ["white", "sun", "orange"], ("rad", 0.3, 0, 0.62), 3.0, 2.4),
            ln(at(burst(0, 0, 0.16, 0.4, 6, PI / 6), 0.3, 0), ["white", "sun"], ("rad", 0.3, 0, 0.4), 2.6, 1.8),
            ln(at(ring(0.26, 18), 0.3, 0), "sun", None, 1.6, 1.5), gl(0.3, 0, 0.26, "white", 1.8)]


def e_colossus():
    return [gl(0, 0, 0.95, "violet", 0.2),
            fl(ld.transform_points(SHIP, 0.0, -0.08, 0.66, PI / 2), ["gold", "violet"], ("lin", 0, 0.7, 0, -0.7), 0.14),
            ln(ship(0.0, -0.08, 0.66, PI / 2), ["white", "gold", "violet"], ("lin", 0, 0.7, 0, -0.7), 2.9, 3.0),
            ln(ship(0.0, -0.08, 0.66, PI / 2), "white", None, 2.0, 1.1),
            ln(ship(0.0, -0.2, 0.16, PI / 2), ["white", "cyan"], ("rad", 0, -0.2, 0.2), 2.6, 1.6),
            fr(0.0, 0.68, 0.34, "white", 2.2, 2, 0.0, 1.2)]


def e_time_stretch():
    tight = sine(-0.86, -0.3, -0.62, 0.1, 3.0, 24)
    loose = sine(-0.3, 0.86, -0.62, 0.1, 1.5, 24)
    return [gl(0, 0.1, 0.8, "cyan", 0.2),
            ln(at(ring(0.46, 24), 0, 0.14), ["cyan", "white", "ice", "white"], ("ang", 0, 0.14, 0), 2.7, 2.6),
            ln(P((0, 0.14), (0, 0.46)), "white", None, 3.0, 2.2), ln(P((0, 0.14), (0.24, 0.02)), "white", None, 3.0, 2.2),
            ln(at(burst(0, 0, 0.52, 0.6, 12), 0, 0.14), "sky", None, 1.4, 1.4),
            ln(ld.polyline(tight), "sky", None, 1.8, 1.6), ln(ld.polyline(loose), ["sky", "white"], ("lin", -0.3, 0, 0.86, 0), 2.6, 2.0), gl(0, 0.14, 0.1, "white", 1.2)]


def e_freeze_frame():
    layers = [gl(0, 0, 0.9, "ice", 0.2), ln(poly(6, 0.16), "white", None, 3.0, 1.8), gl(0, 0, 0.16, "white", 1.4)]
    for k in range(6):
        a = k * PI / 3 + PI / 2
        arm = [(0.16, 0.0, 0.72, 0.0), (0.42, 0.0, 0.56, 0.14), (0.42, 0.0, 0.56, -0.14), (0.58, 0.0, 0.68, 0.1), (0.58, 0.0, 0.68, -0.1)]
        layers.append(ln(at(arm, 0, 0, 1.0, a), ["white", "ice", "sky"], ("rad", 0, 0, 0.72), 2.7, 2.0))
    return layers


def e_black_hole():
    layers = [gl(0, 0, 1.0, "violet", 0.22), gl(0.3, 0.1, 0.5, "orange", 0.16),
              ln(ring(0.7, 30, 0.36, 0.35), ["orange", "sun", "purple", "violet"], ("ang", 0, 0, 0.3), 2.8, 2.8),
              ln(ring(0.5, 26, 0.36, 0.35), ["violet", "indigo"], ("ang", 0, 0, 0.3), 1.6, 1.6),
              ln(ring(0.2, 16), "white", None, 3.0, 1.8)]
    for k in range(7):
        a = k * 0.9 + 0.2
        layers.append(ln(P((0.9 * math.cos(a), 0.62 * math.sin(a)), (0.66 * math.cos(a + 0.2), 0.44 * math.sin(a + 0.2))), ["purple", "white"], ("rad", 0, 0, 0.9), 2.0, 1.3))
    return layers


def e_chain_storm():
    nodes = [(0.6 * math.cos(PI / 2 + k * 2 * PI / 5), 0.6 * math.sin(PI / 2 + k * 2 * PI / 5)) for k in range(5)]
    layers = [gl(0, 0, 0.9, "sky", 0.2)]
    for k in range(5):
        a, b = nodes[k], nodes[(k + 2) % 5]
        mx, my = (a[0] + b[0]) / 2, (a[1] + b[1]) / 2
        layers.append(ln(P(a, (mx + 0.08 * (-1) ** k, my + 0.08), (mx - 0.06, my - 0.08 * (-1) ** k), b), ["white", "sun", "sky"], ("rad", 0, 0, 0.6), 2.6, 1.7))
    for x, y in nodes:
        layers += [ln(poly(4, 0.1, x, y), "white", None, 3.0, 1.6), gl(x, y, 0.18, "sun", 0.9)]
    layers.append(gl(0, 0, 0.2, "white", 1.2))
    return layers


def e_chorus():
    return [gl(0, 0, 0.9, "cyan", 0.18),
            ln(ship(-0.48, -0.26, 0.24, PI / 2), ["white", "lime"], ("rad", -0.48, -0.26, 0.3), 2.3, 1.8),
            ln(ship(0.48, -0.26, 0.24, PI / 2), ["white", "violet"], ("rad", 0.48, -0.26, 0.3), 2.3, 1.8),
            ln(ship(0.0, 0.16, 0.34, PI / 2), ["white", "cyan"], ("rad", 0, 0.16, 0.4), 2.9, 2.3), ln(ship(0.0, 0.16, 0.34, PI / 2), "white", None, 1.9, 0.9),
            ln(P((-0.48, 0.1), (-0.48, 0.36)) + P((0.48, 0.1), (0.48, 0.36)) + P((0.0, 0.6), (0.0, 0.84)), "white", None, 2.6, 1.8)]


def e_beam():
    return [gl(0.2, 0, 0.9, "cyan", 0.22),
            ln(ship(-0.62, 0.0, 0.2, 0.0), ["white", "cyan"], ("rad", -0.62, 0, 0.24), 2.6, 1.7),
            ln(P((-0.34, 0), (0.9, 0)), ["cyan", "sky", "cyan"], ("lin", -0.34, 0, 0.9, 0), 1.5, 9.0),
            ln(P((-0.34, 0), (0.9, 0)), "white", None, 3.4, 3.0),
            ln(ld.arc(0.5, 0.35, 0.9, 5) + ld.arc(0.5, -0.9, -0.35, 5), "sky", None, 1.4, 1.5), gl(-0.34, 0, 0.2, "white", 1.4)]


def e_ricochet_fever():
    box = [(-0.62, -0.5), (0.62, -0.5), (0.62, 0.5), (-0.62, 0.5)]
    path = [(-0.56, -0.3), (-0.2, 0.5), (0.1, -0.5), (0.36, 0.5), (0.62, -0.1), (0.2, -0.5)]
    layers = [gl(0, 0, 0.9, "sun", 0.18), ln(ld.polyline(box, closed=True), ["cyan", "sky"], ("lin", -0.62, 0, 0.62, 0), 1.9, 2.0),
              ln(ld.polyline(path), ["orange", "sun", "white"], ("lin", -0.56, 0, 0.62, 0), 2.8, 2.0)]
    for x, y in path[1:-1]:
        layers += [fr(x, y, 0.16, "white", 2.2, 2, PI / 4, 1.0), gl(x, y, 0.1, "sun", 0.9)]
    return layers


def e_confetti():
    colours = ["cyan", "lime", "sun", "orange", "magenta", "violet", "sky", "mint", "purple", "amber"]
    layers = [gl(0, 0, 0.9, "magenta", 0.14), gl(0, 0, 0.5, "sun", 0.14), ln(burst(0, 0, 0.06, 0.2, 8), "white", None, 2.8, 1.5), gl(0, 0, 0.16, "white", 1.4)]
    for k in range(16):
        a = k * 2.399963
        r = 0.3 + 0.5 * ((k * 7) % 10) / 10.0
        x, y = r * math.cos(a), r * math.sin(a)
        c = colours[k % len(colours)]
        layers += [ln(kite(x, y, 0.09, a * 1.7), ["white", c], ("rad", x, y, 0.09), 2.8, 1.5), gl(x, y, 0.1, c, 0.7)]
    return layers


def e_fractal():
    layers = [gl(0.1, 0, 0.9, "purple", 0.18), ln(P((-0.8, 0), (-0.34, 0)), ["purple", "white"], ("lin", -0.8, 0, -0.34, 0), 2.8, 2.6)]
    for sy in (1, -1):
        layers.append(ln(P((-0.34, 0), (0.1, 0.36 * sy)), ["white", "mint"], ("lin", -0.34, 0, 0.1, 0), 2.6, 2.2))
        for tip in ((0.62, 0.62 * sy), (0.66, 0.16 * sy)):
            layers.append(ln(P((0.1, 0.36 * sy), tip), ["mint", "sun"], ("lin", 0.1, 0, 0.66, 0), 2.4, 1.8))
            layers.append(gl(tip[0], tip[1], 0.12, "sun", 1.2))
    layers.append(gl(-0.34, 0, 0.14, "white", 1.0))
    return layers


def e_carousel():
    layers = [gl(0, 0, 0.9, "amber", 0.18), ln(dashes(0.52, 18, 0.5), "magenta", None, 1.2, 1.4), gl(0, 0, 0.14, "white", 1.4), ln(poly(4, 0.12), "white", None, 2.8, 1.6)]
    for k in range(6):
        a = k * PI / 3 + 0.2
        r = 0.52 if k else 0.86
        x, y = r * math.cos(a), r * math.sin(a)
        layers += [ln(at(ring(0.11, 12), x, y), ["white", "amber"] if k else ["white", "sun"], ("rad", x, y, 0.11), 2.8, 2.0), gl(x, y, 0.12, "amber", 0.9)]
    layers.append(ln(P((0.56 * math.cos(0.2), 0.56 * math.sin(0.2)), (0.74 * math.cos(0.2), 0.74 * math.sin(0.2))), ["magenta", "white"], ("rad", 0, 0, 0.8), 2.4, 1.6))
    return layers


# ---- Fusions

def frame_fusion(a, b):
    """Eight-point star in every colour: a Fusion is two upgrades become one, and it should look like an event."""
    star = [((1.12 if k % 2 == 0 else 0.9) * math.cos(PI / 8 * k + PI / 2), (1.12 if k % 2 == 0 else 0.9) * math.sin(PI / 8 * k + PI / 2)) for k in range(16)]
    rainbow = ["cyan", "lime", "sun", "orange", "magenta", "violet", "azure"]
    return [
        gl(0, 0, 2.0, "gold", 0.14), gl(-0.4, 0.3, 1.3, a, 0.14), gl(0.4, -0.3, 1.3, b, 0.14),
        fr(0, 0, 1.9, "white", 0.9, 4, PI / 8, 1.3),
        fl(star, ["white", a, b], ("rad", 0, 0, 1.1), 0.09),
        ln(dashes(0.82, 24, 0.5), "white", None, 0.35, 1.2),
        ln(ld.polyline(star, closed=True), rainbow, ("ang", 0, 0, 0.0), 2.4, 2.8),
        ln(ld.polyline(star, closed=True), "white", None, 1.4, 1.0),
        fr(0, 1.12, 0.3, "white", 2.2, 2, 0.0, 1.1), fr(0, -1.12, 0.3, "white", 2.2, 2, 0.0, 1.1),
        fr(1.12, 0, 0.3, "white", 2.2, 2, 0.0, 1.1), fr(-1.12, 0, 0.3, "white", 2.2, 2, 0.0, 1.1),
    ]


def e_helix():
    one, two = sine(-0.72, 0.6, 0.0, 0.34, 1.5), sine(-0.72, 0.6, 0.0, 0.34, 1.5, phase=PI)
    return [gl(0, 0, 0.8, "sun", 0.16),
            ln(ld.polyline(two), ["blue", "cyan", "white"], ("lin", -0.72, 0, 0.6, 0), 2.4, 2.2),
            ln(ld.polyline(one), ["orange", "sun", "white"], ("lin", -0.72, 0, 0.6, 0), 2.8, 2.4),
            ln([(-0.72 + 1.32 * k / 9.0, -0.2, -0.72 + 1.32 * k / 9.0, 0.2) for k in (2, 4, 5, 7)], "white", None, 0.9, 1.2),
            gl(0.62, -0.34, 0.14, "sun", 1.2), gl(0.62, 0.34, 0.14, "cyan", 1.2)]


def e_standing_wave():
    top, bot = sine(-0.78, 0.78, 0.0, 0.42, 1.5), sine(-0.78, 0.78, 0.0, -0.42, 1.5)
    return [gl(0, 0, 0.85, "magenta", 0.18),
            ln(ld.polyline(top), ["violet", "magenta", "violet"], ("lin", -0.78, 0, 0.78, 0), 2.3, 2.0),
            ln(ld.polyline(bot), ["indigo", "purple", "indigo"], ("lin", -0.78, 0, 0.78, 0), 1.8, 1.8),
            ln(P((-0.9, 0), (0.9, 0)), ["orange", "sun", "orange"], ("lin", -0.9, 0, 0.9, 0), 1.5, 4.6), ln(P((-0.9, 0), (0.9, 0)), "white", None, 3.2, 1.8),
            gl(-0.26, 0, 0.1, "white", 1.4), gl(0.26, 0, 0.1, "white", 1.4)]


def e_tesla_bloom():
    pts = [(0.62 * math.cos(PI / 2 + k * 4 * PI / 5), 0.62 * math.sin(PI / 2 + k * 4 * PI / 5)) for k in range(6)]
    zig = []
    for k in range(5):
        a, b = pts[k], pts[k + 1]
        zig += [a, ((a[0] * 2 + b[0]) / 3 + 0.05, (a[1] * 2 + b[1]) / 3 - 0.05), ((a[0] + b[0] * 2) / 3 - 0.05, (a[1] + b[1] * 2) / 3 + 0.05)]
    zig.append(pts[5])
    return [gl(0, 0, 0.9, "sky", 0.2),
            ln(ring(0.66, 28), ["sky", "white", "azure", "white"], ("ang", 0, 0, 0.0), 2.6, 2.6),
            ln(ld.polyline(zig), ["white", "sun", "sky"], ("rad", 0, 0, 0.62), 2.6, 1.7), gl(0, 0, 0.2, "white", 1.4)]


def e_shard_halo():
    layers = [gl(0, 0, 0.9, "teal", 0.18), ln(ring(0.44, 22), ["teal", "cyan", "lime", "cyan"], ("ang", 0, 0, 0), 2.4, 2.2), gl(0, 0, 0.14, "white", 1.2)]
    for k in range(6):
        a = k * PI / 3
        x0, y0 = 0.44 * math.cos(a), 0.44 * math.sin(a)
        x1, y1 = x0 - 0.42 * math.sin(a), y0 + 0.42 * math.cos(a)
        layers += [ln(P((x0, y0), (x1, y1)), ["teal", "white"], ("lin", x0, y0, x1, y1), 1.6, 1.4),
                   ln(kite(x1, y1, 0.11, a + PI / 2), ["white", "gold"], ("rad", x1, y1, 0.11), 2.9, 1.6), gl(x1, y1, 0.1, "gold", 0.8)]
    return layers


def e_downbeat_cannon():
    return [gl(-0.5, 0, 0.7, "sun", 0.22),
            ln(P((-0.5, 0), (0.92, 0)), ["magenta", "violet", "magenta"], ("lin", -0.5, 0, 0.92, 0), 1.6, 10.0),
            ln(P((-0.5, 0), (0.92, 0)), "white", None, 3.4, 3.4),
            ln(at(burst(0, 0, 0.12, 0.46, 8, PI / 8), -0.5, 0), ["white", "sun"], ("rad", -0.5, 0, 0.46), 2.9, 2.0), gl(-0.5, 0, 0.22, "white", 1.8),
            ln(P((0.2, 0.74), (0.2, 0.42)) , "white", None, 3.0, 2.6), gl(0.2, 0.84, 0.08, "sun", 1.4)]


def e_collapse_field():
    layers = [gl(0, 0, 0.9, "violet", 0.2), ln(ring(0.16, 14), "white", None, 3.0, 2.0), gl(0, 0, 0.24, "white", 1.4),
              ln(ld.arc(0.36, 0.3, 2.4, 8) + ld.arc(0.36, 3.5, 5.6, 8), "purple", None, 1.4, 1.5)]
    for k in range(4):
        a = PI / 4 + k * PI / 2
        x, y = 0.68 * math.cos(a), 0.68 * math.sin(a)
        layers += [ln(at(ring(0.12, 12), x, y), ["amber", "orange"], ("rad", x, y, 0.12), 2.5, 1.9),
                   ln(arrow(x * 0.78, y * 0.78, x * 0.42, y * 0.42, 0.12), ["amber", "white"], ("rad", 0, 0, 0.7), 2.3, 1.6)]
    return layers


def e_murmuration():
    layers = [gl(0, 0, 0.9, "lime", 0.16)]
    for k in range(18):
        u = k / 17.0
        x = -0.74 + 1.48 * u
        y = 0.34 * math.sin(u * 5.2 + 0.6) + 0.16 * math.sin(k * 2.4)
        heading = math.atan2(0.34 * 5.2 * math.cos(u * 5.2 + 0.6), 1.48)
        c = ["lime", "mint", "teal", "sun"][k % 4]
        layers += [ln(kite(x, y, 0.07 + 0.03 * math.sin(u * PI), heading), ["white", c], ("rad", x, y, 0.1), 2.8, 1.4)]
    layers.append(gl(0.74, 0.34 * math.sin(5.8), 0.2, "white", 1.0))
    return layers


def e_quartet():
    spots = [((0.0, 0.48), "cyan"), ((-0.5, 0.0), "lime"), ((0.5, 0.0), "violet"), ((0.0, -0.48), "sun")]
    layers = [gl(0, 0, 0.9, "cyan", 0.16), ln(ld.polyline([s[0] for s in spots[:2]] + [spots[3][0], spots[2][0]], closed=True), "white", None, 0.5, 1.2)]
    for (x, y), c in spots:
        layers += [ln(ship(x, y, 0.24, PI / 2), ["white", c], ("rad", x, y, 0.3), 2.8, 1.9), gl(x, y, 0.2, c, 0.7)]
    return layers


WEAPON, DEFENCE, ECONOMY, RHYTHM = ("sun", "orange"), ("cyan", "blue"), ("lime", "teal"), ("magenta", "indigo")

ICONS = [
    # id, category, group colours, display name, size relative to a Mod, emblem
    ("split", "mod", WEAPON, "Split", 1.0, e_split),
    ("double_time", "mod", RHYTHM, "Double-Time", 1.0, e_double_time),
    ("pierce", "mod", WEAPON, "Pierce", 1.0, e_pierce),
    ("ricochet", "mod", WEAPON, "Ricochet", 1.0, e_ricochet),
    ("overcharge", "mod", WEAPON, "Overcharge", 1.0, e_overcharge),
    ("extra_cell", "mod", DEFENCE, "Extra Cell", 1.0, e_extra_cell),
    ("magnet", "mod", ECONOMY, "Magnet", 1.0, e_magnet),
    ("on_beat_crit", "mod", RHYTHM, "On-Beat Crit", 1.0, e_on_beat_crit),
    ("orbitals", "riff", ("teal", "lime"), "Orbitals", 1.14, e_orbitals),
    ("pulse_ring", "riff", ("sky", "azure"), "Pulse Ring", 1.14, e_pulse_ring),
    ("sentinel", "riff", ("amber", "orange"), "Sentinel", 1.14, e_sentinel),
    ("prism", "boost", ("purple", "mint"), "Prism", 1.3, e_prism),
    ("hyper", "boost", ("sun", "orange"), "Hyper", 1.3, e_hyper),
    ("halo", "boost", ("cyan", "sky"), "Halo", 1.3, e_halo),
    ("magnet_storm", "boost", ("violet", "gold"), "Magnet Storm", 1.3, e_magnet_storm),
    # ---- the rest of the catalogue (the fifteen above are the first playable slice)
    ("fork", "mod", WEAPON, "Fork", 1.0, e_fork),
    ("seeker", "mod", WEAPON, "Seeker", 1.0, e_seeker),
    ("heavy_step", "mod", WEAPON, "Heavy Step", 1.0, e_heavy_step),
    ("accent", "mod", WEAPON, "Accent", 1.0, e_accent),
    ("echo", "mod", WEAPON, "Echo", 1.0, e_echo),
    ("wave", "mod", WEAPON, "Wave", 1.0, e_wave),
    ("rear_guard", "mod", WEAPON, "Rear Guard", 1.0, e_rear_guard),
    ("arc", "mod", WEAPON, "Arc", 1.0, e_arc),
    ("burn", "mod", WEAPON, "Burn", 1.0, e_burn),
    ("shatter", "mod", WEAPON, "Shatter", 1.0, e_shatter),
    ("gravity_rounds", "mod", WEAPON, "Gravity Rounds", 1.0, e_gravity_rounds),
    ("regen", "mod", DEFENCE, "Regen", 1.0, e_regen),
    ("afterimage", "mod", DEFENCE, "Afterimage", 1.0, e_afterimage),
    ("reserve", "mod", DEFENCE, "Reserve", 1.0, e_reserve),
    ("shockstep", "mod", DEFENCE, "Shockstep", 1.0, e_shockstep),
    ("aegis", "mod", DEFENCE, "Aegis", 1.0, e_aegis),
    ("last_light", "mod", DEFENCE, "Last Light", 1.0, e_last_light),
    ("slipstream", "mod", DEFENCE, "Slipstream", 1.0, e_slipstream),
    ("prospector", "mod", ECONOMY, "Prospector", 1.0, e_prospector),
    ("nova_stock", "mod", ECONOMY, "Nova Stock", 1.0, e_nova_stock),
    ("encore", "mod", ECONOMY, "Encore", 1.0, e_encore),
    ("lucky_break", "mod", ECONOMY, "Lucky Break", 1.0, e_lucky_break),
    ("collector", "mod", ECONOMY, "Collector", 1.0, e_collector),
    ("syncopation", "mod", RHYTHM, "Syncopation", 1.0, e_syncopation),
    ("bar_line", "mod", RHYTHM, "Bar Line", 1.0, e_bar_line),
    ("crescendo", "mod", RHYTHM, "Crescendo", 1.0, e_crescendo),
    ("kick_pulse", "mod", RHYTHM, "Kick Pulse", 1.0, e_kick_pulse),
    ("afterparty", "mod", RHYTHM, "Afterparty", 1.0, e_afterparty),
    ("lancer", "riff", ("violet", "purple"), "Lancer", 1.14, e_lancer),
    ("minelayer", "riff", ("amber", "sun"), "Minelayer", 1.14, e_minelayer),
    ("storm_coil", "riff", ("sky", "ice"), "Storm Coil", 1.14, e_storm_coil),
    ("swarm", "riff", ("lime", "mint"), "Swarm", 1.14, e_swarm),
    ("singularity", "riff", ("violet", "orange"), "Singularity", 1.14, e_singularity),
    ("mirror", "riff", ("cyan", "sky"), "Mirror", 1.14, e_mirror),
    ("comet", "riff", ("teal", "cyan"), "Comet", 1.14, e_comet),
    ("sweeper", "riff", ("magenta", "purple"), "Sweeper", 1.14, e_sweeper),
    ("flak", "riff", ("orange", "sun"), "Flak", 1.14, e_flak),
    ("supernova_rounds", "boost", ("orange", "sun"), "Supernova Rounds", 1.3, e_supernova),
    ("colossus", "boost", ("violet", "gold"), "Colossus", 1.3, e_colossus),
    ("time_stretch", "boost", ("cyan", "ice"), "Time Stretch", 1.3, e_time_stretch),
    ("freeze_frame", "boost", ("ice", "sky"), "Freeze Frame", 1.3, e_freeze_frame),
    ("black_hole", "boost", ("violet", "orange"), "Black Hole", 1.3, e_black_hole),
    ("chain_storm", "boost", ("sun", "sky"), "Chain Storm", 1.3, e_chain_storm),
    ("chorus", "boost", ("cyan", "lime"), "Chorus", 1.3, e_chorus),
    ("beam", "boost", ("cyan", "ice"), "Beam", 1.3, e_beam),
    ("ricochet_fever", "boost", ("sun", "cyan"), "Ricochet Fever", 1.3, e_ricochet_fever),
    ("confetti", "boost", ("magenta", "lime"), "Confetti", 1.3, e_confetti),
    ("fractal", "boost", ("purple", "mint"), "Fractal", 1.3, e_fractal),
    ("carousel", "boost", ("amber", "magenta"), "Carousel", 1.3, e_carousel),
    ("helix_array", "fusion", ("sun", "cyan"), "Helix Array", 1.45, e_helix),
    ("standing_wave", "fusion", ("magenta", "sun"), "Standing Wave", 1.45, e_standing_wave),
    ("tesla_bloom", "fusion", ("sky", "sun"), "Tesla Bloom", 1.45, e_tesla_bloom),
    ("shard_halo", "fusion", ("teal", "gold"), "Shard Halo", 1.45, e_shard_halo),
    ("downbeat_cannon", "fusion", ("magenta", "sun"), "Downbeat Cannon", 1.45, e_downbeat_cannon),
    ("collapse_field", "fusion", ("violet", "amber"), "Collapse Field", 1.45, e_collapse_field),
    ("murmuration", "fusion", ("lime", "mint"), "Murmuration", 1.45, e_murmuration),
    ("quartet", "fusion", ("cyan", "violet"), "Quartet", 1.45, e_quartet),
]
FIRST_SLICE = 15
FRAME = dict(mod=frame_mod, riff=frame_riff, boost=frame_boost, fusion=frame_fusion)


def sheet_all(sw, font, pal):
    """Every upgrade on one sheet, by category: design/previews/icons_all.png."""
    white = np.ones(3, np.float32)
    bg0, bg1 = np.asarray(pal["bg0"]["linear"], np.float32), np.asarray(pal["bg1"]["linear"], np.float32)
    rows = [("MODS  36", "mod", 9, 56, 196), ("RIFFS  12", "riff", 6, 62, 232), ("BOOSTS  16", "boost", 8, 62, 250), ("FUSIONS  8", "fusion", 8, 62, 266)]
    W = 2400
    H = 90 + sum(60 + row_h * ((len([i for i in ICONS if i[1] == cat]) + per - 1) // per) for _, cat, per, _, row_h in rows)
    cv = ld.Canvas(W, H)
    cv.radial_bg(bg0 * 0.45, bg1 * 0.3, cx=0.5, cy=0.4)
    cv.nebula([(400, 300, 700, sw["indigo"], 0.03), (2000, 600, 760, sw["violet"], 0.026), (1100, H - 500, 800, sw["azure"], 0.026),
               (300, H - 300, 600, sw["purple"], 0.022), (2100, H - 200, 600, sw["teal"], 0.02)])
    cv.lines(ld.text_segments(font, "RAVEFORM  UPGRADES", 40, 26, 24)[0], ld.f_linear(40, 0, 700, 0, [sw["cyan"], white, sw["magenta"]]), 1.6, 2.4)
    y = 90
    for title, cat, per, base, row_h in rows:
        items = [i for i in ICONS if i[1] == cat]
        cv.text(font, title, 40, y + 6, 14, white, 0.9, 1.6)
        y += 60
        cell = (W - 80) / per
        for k, icon in enumerate(items):
            cx = 40 + cell * (k % per + 0.5)
            cy = y + (k // per) * row_h + row_h * 0.42
            R = base * icon[4]
            draw_layers(cv, layers_of(icon), cx, cy, R, sw)
            name = icon[3].upper()
            cv.text(font, name, cx - len(name) * 6 * 1.5 / 2, cy + R * 1.2 + 12, 9, white, 0.8, 1.2)
        y += row_h * ((len(items) + per - 1) // per)
    out = os.path.join(DESIGN, "previews", "icons_all.png")
    cv.render(bloom=0.95).save(out, optimize=True)
    return out


def layers_of(icon):
    iid, cat, (a, b), name, size, emblem = icon
    return FRAME[cat](a, b) + emblem()


# --------------------------------------------------------------------------- drawing

def draw_layers(cv, layers, cx, cy, R, sw):
    def px(x, y):
        return (cx + x * R, cy - y * R)

    def colour_of(layer):
        cols = [sw[c] for c in layer["colours"]]
        g = layer.get("grad")
        if not g or len(cols) == 1:
            return cols[0]
        if g[0] == "lin":
            a, b = px(g[1], g[2]), px(g[3], g[4])
            return ld.f_linear(a[0], a[1], b[0], b[1], cols)
        if g[0] == "rad":
            c = px(g[1], g[2])
            return ld.f_radial(c[0], c[1], g[3] * R, cols)
        c = px(g[1], g[2])
        return ld.f_angular(c[0], c[1], cols, -g[3])

    k = R / 60.0
    for layer in layers:
        t = layer["t"]
        if t == "lines":
            segs = [px(s[0], s[1]) + px(s[2], s[3]) for s in layer["segs"]]
            cv.lines(segs, colour_of(layer), layer["i"], max(1.1, layer["w"] * k))
        elif t == "fill":
            cv.fill([px(p[0], p[1]) for p in layer["pts"]], colour_of(layer), layer["i"])
        elif t == "glow":
            x, y = px(layer["x"], layer["y"])
            cv.glow(x, y, layer["r"] * R, sw[layer["colour"]], layer["i"])
        elif t == "flare":
            x, y = px(layer["x"], layer["y"])
            cv.flare(x, y, layer["len"] * R, sw[layer["colour"]], layer["i"], max(1.0, layer["w"] * k), layer["arms"], -layer["rot"])


def build():
    sw = swatches()
    return dict(
        note="Generated by design/tools/icons.py. Unit space, y up, frame radius 1.0. Each icon is a list of "
             "layers drawn in order, all additive: glow {x,y,r,colour,i}; fill {pts,colours,grad,i}; "
             "lines {segs,colours,grad,i (HDR intensity),w (width in px at a 60 px icon radius)}; "
             "flare {x,y,len,colour,i,arms,rot,w}. grad is null (solid, first colour), [lin,x0,y0,x1,y1], "
             "[rad,cx,cy,r] or [ang,cx,cy,rot]; colours are evenly spaced stops. size scales the whole icon.",
        swatches={k: dict(linear=[round(float(v), 5) for v in rgb], hex=ld.hexstr(rgb)) for k, rgb in sw.items()},
        icons=[dict(id=i[0], category=i[1], name=i[3], size=i[4], layers=layers_of(i)) for i in ICONS])


def main():
    data = build()
    with open(os.path.join(DESIGN, "icons.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"))
    sw = swatches()
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    pal = pals["palettes"][0]
    white = np.ones(3, np.float32)
    bg0, bg1 = np.asarray(pal["bg0"]["linear"], np.float32), np.asarray(pal["bg1"]["linear"], np.float32)

    W, H = 1900, 1180
    cv = ld.Canvas(W, H)
    cv.radial_bg(bg0 * 0.45, bg1 * 0.35, cx=0.5, cy=0.45)
    cv.nebula([(300, 250, 520, sw["indigo"], 0.035), (1500, 300, 560, sw["violet"], 0.03), (950, 900, 600, sw["azure"], 0.03),
               (1650, 950, 420, sw["teal"], 0.025), (200, 980, 420, sw["purple"], 0.025)])
    cv.lines(ld.text_segments(font, "UPGRADES", 40, 28, 22)[0], ld.f_linear(40, 0, 320, 0, [sw["cyan"], white, sw["magenta"]]), 1.6, 2.2)
    cv.text(font, "HEXAGON MOD   RING RIFF   DIAMOND BOOST", 360, 34, 12, white, 0.7, 1.4)

    def place(icon, cx, cy, base):
        R = base * icon[4]
        draw_layers(cv, layers_of(icon), cx, cy, R, sw)
        name = icon[3].upper()
        cv.text(font, name, cx - len(name) * 6 * 1.9 / 2, cy + R * 1.32 + 10, 11, white, 0.85, 1.4)
        draw_layers(cv, layers_of(icon), cx + R * 1.5, cy + R * 0.95, 21 * icon[4], sw)       # in-game size

    print("wrote", sheet_all(sw, font, pal))
    first = ICONS[:FIRST_SLICE]
    mods = [i for i in first if i[1] == "mod"]
    for k, icon in enumerate(mods):
        place(icon, 140 + k * 232, 205, 70)
    rest = [i for i in first if i[1] != "mod"]
    for k, icon in enumerate(rest):
        place(icon, 150 + k * 250 if k < 3 else 930 + (k - 3) * 272, 520, 70)

    # ---- the upgrade moment: a Break, the ship, three orbs to choose from
    by = 900
    cv.text(font, "THE BREAK: FLY INTO ONE", 40, by - 150, 14, sw["cyan"], 1.1, 1.6)
    grid = []
    for gx in range(0, W + 1, 44):
        grid.append((gx, by - 110, gx, H))
    for gy in range(by - 110, H + 1, 44):
        grid.append((0, gy, W, gy))
    cv.lines(grid, np.asarray(pal["grid"]["linear"], np.float32), 0.16, 1.0)
    sx, sy = 950.0, by + 170.0
    p1 = np.asarray(pals["constants"]["players"][0]["linear"], np.float32)
    cv.glow(sx, sy, 150, p1, 0.10)
    hull = ld.transform([(s[0], -s[1], s[2], -s[3]) for s in ld.polyline(SHIP, closed=True)], sx, sy, 30, -PI / 2)
    cv.lines(hull, ld.f_linear(sx, sy - 34, sx, sy + 30, [white, p1, sw["blue"]]), 2.8, 2.6)
    cv.lines(hull, white, 2.0, 0.9)
    for k in range(3):
        a0 = -PI / 2 + k * 2 * PI / 3 + 0.25
        cv.lines(ld.transform(ld.arc(1.0, a0, a0 + 1.55, 8), sx, sy, 48), p1, 1.4, 1.6)
    trail = [(sx, sy + 34 + 12 * k, sx, sy + 46 + 12 * k) for k in range(9)]
    cv.lines(trail, ld.f_fade(ld.f_solid(p1), sx, sy + 30, 130, 1.2), 2.0, 2.6)
    orbs = [("ricochet", sx - 380, by + 30, "BOUNCES OFF WALLS +1", "sun"),
            ("orbitals", sx, by - 70, "NEW RIFF: TWO ORBITING BLADES", "teal"),
            ("prism", sx + 380, by + 30, "RAINBOW FAN FOR 4 BARS", "gold")]
    by_id = {i[0]: i for i in ICONS}
    for iid, ox, oy, blurb, tint in orbs:
        icon = by_id[iid]
        R = 50 * icon[4]
        cv.lines(ld.transform(dashes(1.0, 24, 0.5), ox, oy, R * 1.62), white, 0.35, 1.2)
        draw_layers(cv, layers_of(icon), ox, oy, R, sw)
        name = icon[3].upper()
        cv.text(font, name, ox - len(name) * 6 * 2.2 / 2, oy + R * 1.85, 13, white, 1.1, 1.6)
        cv.text(font, blurb, ox - len(blurb) * 6 * 1.5 / 2, oy + R * 1.85 + 28, 9, sw[tint], 0.9, 1.2)
        link = [(sx + (ox - sx) * t, sy + (oy - sy) * t, sx + (ox - sx) * (t + 0.04), sy + (oy - sy) * (t + 0.04)) for t in (0.2, 0.3, 0.4)]
        cv.lines(link, white, 0.4, 1.2)

    out = os.path.join(DESIGN, "previews", "icons.png")
    cv.render(bloom=0.95).save(out, optimize=True)
    print("icons:", len(ICONS), "layers:", sum(len(layers_of(i)) for i in ICONS), "wrote", out)


if __name__ == "__main__":
    main()
