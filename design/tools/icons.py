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
]
FRAME = dict(mod=frame_mod, riff=frame_riff, boost=frame_boost)


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

    mods = [i for i in ICONS if i[1] == "mod"]
    for k, icon in enumerate(mods):
        place(icon, 140 + k * 232, 205, 70)
    rest = [i for i in ICONS if i[1] != "mod"]
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
