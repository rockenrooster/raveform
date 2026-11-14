"""RAVEFORM glyph shapes: recipes (edit here), export to design/glyphs.json, contact sheets.

Every shape is a list of line segments in unit space (+x is forward, outer extent about 1.0), each tagged
with a layer: 0 hull, 1 inner ornament (rotates/pulses on its own), 2 core mark. The engine bakes these
into static meshes; nothing here is drawn from textures.

Usage:  python design/tools/glyphs.py     -> design/glyphs.json, design/previews/glyphs_*.png
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


def tag(segs, layer):
    return [(s[0], s[1], s[2], s[3], layer) for s in segs]


def mirror_y(segs):
    return [(s[0], -s[1], s[2], -s[3]) for s in segs]


def both(segs):
    """A list of segments plus its mirror image across the x axis."""
    return list(segs) + mirror_y(segs)


def line(x0, y0, x1, y1):
    return [(x0, y0, x1, y1)]


def ticks(angles_deg, r0, r1):
    out = []
    for a in angles_deg:
        c, s = math.cos(math.radians(a)), math.sin(math.radians(a))
        out.append((r0 * c, r0 * s, r1 * c, r1 * s))
    return out


def ring_arcs(r, count, span_deg, segs_per_arc, rot_deg=0.0):
    out = []
    for i in range(count):
        a0 = math.radians(rot_deg + i * 360.0 / count - span_deg / 2)
        out += ld.arc(r, a0, a0 + math.radians(span_deg), segs_per_arc)
    return out


def scaled(segs, sx, sy=None, dx=0.0, dy=0.0):
    sy = sx if sy is None else sy
    return [(s[0] * sx + dx, s[1] * sy + dy, s[2] * sx + dx, s[3] * sy + dy) for s in segs]


# --------------------------------------------------------------------------- families

def drifter(t):
    g = tag(ld.polygon(6, 1.0, PI / 6), 0)
    if t == 2:
        g += tag(ld.polygon(3, 0.50, PI / 2), 1)
    elif t == 3:
        g += tag(ld.polygon(6, 0.55, 0.0), 1)
    elif t == 4:
        g += tag(ld.polygon(6, 0.76, PI / 6), 0) + tag(ld.polygon(3, 0.46, PI / 2), 1)
    elif t == 5:
        g += tag(ld.polygon(3, 0.80, PI / 2), 1) + tag(ld.polygon(3, 0.80, -PI / 2), 1) + tag(ld.polygon(6, 0.28, 0.0), 2)
    return g


HOUND_HULL = ld.polyline([(1.0, 0.0), (-0.8, 0.85), (-0.3, 0.0), (-0.8, -0.85)], closed=True)


def hound(t):
    g = tag(HOUND_HULL, 0)
    if t >= 2 and t != 4 and t != 5:
        g += tag(ld.polyline([(-0.45, 0.42), (0.35, 0.0), (-0.45, -0.42)]), 1)
    if t >= 3:
        g += tag(both(line(-0.10, 0.48, -0.60, 1.12)), 0)
    if t >= 4:
        g += tag(scaled(HOUND_HULL, 0.5, dx=0.02), 1) + tag(line(-0.3, 0.0, -1.15, 0.0), 0)
    if t >= 5:
        g += tag(both(line(-0.45, 0.66, -0.95, 1.22)), 0) + tag(line(1.0, 0.0, 1.4, 0.0), 0)
    return g


def crescent(scale=1.0, dx=0.0, outer_n=6, inner_n=4):
    outer = ld.arc(1.0, math.radians(-115), math.radians(115), outer_n)
    inner = [(s[0] - 0.6, s[1], s[2] - 0.6, s[3]) for s in ld.arc(0.9235, math.radians(-78.9), math.radians(78.9), inner_n)]
    return scaled(outer + inner, scale, dx=dx)


def flanker(t):
    g = tag(crescent(), 0)
    if t >= 2:
        g += tag(line(0.52, 0.0, 0.80, 0.0), 2)
    if t >= 3:
        g += tag([(s[0] - 0.2, s[1], s[2] - 0.2, s[3]) for s in ld.arc(0.80, math.radians(-58), math.radians(58), 3)], 1)
    if t >= 4:
        g += tag(both(line(-0.4226, 0.9063, -0.95, 1.22)), 0)
    if t >= 5:
        g += tag(crescent(0.5, dx=-0.42, outer_n=4, inner_n=3), 1)
    return g


SKITTER_KITE = ld.polyline([(0.9, 0.0), (0.0, 0.5), (-0.5, 0.0), (0.0, -0.5)], closed=True)


def skitter(t):
    g = tag(SKITTER_KITE, 0)
    if t == 1:
        g += tag(line(-0.5, 0.0, -1.2, 0.0), 0)
    if t >= 2:
        g += tag(both(line(-0.5, 0.0, -1.15, 0.38)), 0)
    if t >= 3:
        g += tag(ld.transform(ld.polygon(4, 0.2), 0.1, 0.0), 1)
    if t >= 4:
        g += tag(both(line(0.0, 0.5, -0.5, 0.95)), 0)
    if t >= 5:
        g += tag(line(-0.5, 0.0, -1.3, 0.0), 0) + tag(line(0.9, 0.0, 1.3, 0.0), 0)
    return g


RAM_HULL = ld.polyline([(0.7, 0.55), (-0.8, 0.9), (-0.45, 0.0), (-0.8, -0.9), (0.7, -0.55)], closed=True)


def ram(t):
    g = tag(RAM_HULL, 0) + tag(both(line(0.7, 0.55, 1.2, 0.9)), 0)
    if t >= 2:
        g += tag(line(0.92, -0.38, 0.92, 0.38), 0)
    if t >= 3:
        g += tag(scaled(RAM_HULL, 0.5, dx=0.05), 1)
    if t >= 4:
        g += tag(both(line(0.45, 0.82, -0.95, 1.15)), 0)
    if t >= 5:
        g += tag(both(line(1.2, 0.9, 1.02, 1.28)), 0) + tag(line(-0.45, 0.0, -1.2, 0.0), 0)
    return g


def petal(a, half, r_in=0.40, r_mid=0.72, r_out=1.0):
    pts = [(r_in * math.cos(a), r_in * math.sin(a)),
           (r_mid * math.cos(a - half), r_mid * math.sin(a - half)),
           (r_out * math.cos(a), r_out * math.sin(a)),
           (r_mid * math.cos(a + half), r_mid * math.sin(a + half))]
    return ld.polyline(pts, closed=True)


def moth(t):
    """A ring of separate petals around an empty centre: a flower, not a star and not a pinwheel."""
    n = {1: 4, 2: 4, 3: 5, 4: 6, 5: 6}[t]
    half = math.radians({4: 23, 5: 19, 6: 16}[n])
    g = []
    for i in range(n):
        g += tag(petal(PI / 2 + 2 * PI * i / n, half), 0)
    if t == 2:
        g += tag(ld.polygon(4, 0.2, 0.0), 1)
    elif t == 3:
        g += tag(ld.polygon(5, 0.24, -PI / 2), 1)
    elif t == 4:
        g += tag(ld.polygon(6, 0.26, 0.0), 1)
    elif t == 5:
        g += tag(ld.polygon(6, 0.30, 0.0), 1) + tag(ticks([0, 60, 120, 180, 240, 300], 0.50, 0.74), 1) + tag(ld.polygon(3, 0.12, PI / 2), 2)
    return g


SHOAL_DART = ld.polyline([(1.0, 0.0), (-0.7, 0.55), (-0.25, 0.0), (-0.7, -0.55)], closed=True)


def shoal(t):
    if t == 1:
        return tag(ld.polyline([(0.9, 0.0), (-0.7, 0.6), (-0.7, -0.6)], closed=True), 0)
    if t == 2:
        return tag(ld.polyline([(0.9, 0.0), (-0.7, 0.6), (-0.7, -0.6)], closed=True), 0) + tag(line(-0.7, 0.0, -1.3, 0.0), 0)
    g = tag(SHOAL_DART, 0)
    if t == 4:
        g += tag(line(-0.25, 0.0, -1.2, 0.0), 0)
    if t == 5:
        g += tag(line(0.5, 0.0, -0.25, 0.0), 1) + tag(both(line(-0.25, 0.0, -1.1, 0.3)), 0)
    return g


WYRM_HEAD = ld.polyline([(1.1, 0.0), (0.3, 0.7), (-0.7, 0.55), (-0.9, 0.0), (-0.7, -0.55), (0.3, -0.7)], closed=True)
WYRM_BODY = ld.polyline([(0.9, 0.0), (0.45, 0.6), (-0.45, 0.6), (-0.9, 0.0), (-0.45, -0.6), (0.45, -0.6)], closed=True)


def wyrm_head(t):
    g = tag(WYRM_HEAD, 0) + tag(both(line(0.3, 0.7, 1.0, 0.82)), 0)
    if t >= 2:
        g += tag(line(0.2, 0.0, 0.62, 0.0), 2)
    if t >= 3:
        g += tag(both(line(-0.2, 0.62, -0.52, 1.05)), 0)
    if t >= 4:
        g += tag(scaled(WYRM_HEAD, 0.45, dx=-0.05), 1)
    if t >= 5:
        g += tag(both(line(1.0, 0.82, 1.32, 0.6)), 0) + tag(both(line(-0.5, 0.58, -0.85, 0.98)), 0)
    return g


def wyrm_body(t):
    g = tag(WYRM_BODY, 0)
    if t in (2, 3):
        g += tag(line(-0.9, 0.0, 0.9, 0.0), 1)
    if t >= 3:
        g += tag(both(line(0.0, 0.6, -0.3, 1.0)), 0)
    if t >= 4:
        g += tag(ld.polygon(4, 0.3, 0.0), 1)
    if t >= 5:
        g += tag(both(line(0.35, 0.6, 0.1, 0.98)), 0)
    return g


def carom(t):
    g = []
    depth = {1: 1, 2: 2, 3: 3, 4: 3, 5: 4}[t]
    for k in range(depth):
        g += tag(ld.polygon(4, 0.7071 ** k, PI / 4 + k * PI / 4), 0 if k == 0 else 1)
    if t >= 4:
        g += tag(ticks([45, 135, 225, 315], 1.0, 1.3), 0)
    return g


def blink(t):
    g = tag(ld.star(4, 1.0, 0.36, 0.0), 0)
    if t in (2, 3):
        g += tag(ld.polygon(4, 0.26, PI / 4), 1)
    if t == 3:
        g += tag(ticks([45, 135, 225, 315], 0.52, 0.84), 0)
    if t >= 4:
        g += tag(ld.star(4, 0.68, 0.30, PI / 4), 1) + tag(ld.polygon(4, 0.16, 0.0), 2)
    if t >= 5:
        g += tag(ticks([0, 90, 180, 270], 1.08, 1.38), 0)
    return g


def sentry(t):
    g = tag(ld.polygon(6, 0.68, PI / 6), 0)
    teeth = [60, 180, 300] if t == 1 else [60, 120, 180, 240, 300]
    g += tag(ticks(teeth, 0.68, 1.0), 0)
    if t <= 3:
        g += tag(line(0.62, 0.0, 1.25, 0.0), 0)
    else:
        g += tag(both(line(0.60, 0.18, 1.25, 0.18)), 0)
    if t >= 3:
        g += tag(ld.polygon(3, 0.32, 0.0), 1)
    if t >= 5:
        g += tag(ld.arc(1.22, math.radians(100), math.radians(170), 3) + ld.arc(1.22, math.radians(190), math.radians(260), 3), 1)
    return g


BROOD_CELLS = {
    1: [(0.38, 0.0), (-0.38, 0.0)],
    2: [(0.44, 0.0), (-0.25, 0.28), (-0.25, -0.28)],
    3: [(0.46, 0.0), (-0.46, 0.0), (0.0, 0.32), (0.0, -0.32)],
    4: [(0.5, 0.0), (-0.5, 0.0), (0.0, 0.36), (0.0, -0.36), (0.0, 0.0)],
    5: [(0.0, 0.0), (0.52, 0.0), (-0.52, 0.0), (0.26, 0.36), (-0.26, 0.36), (0.26, -0.36), (-0.26, -0.36)],
}


def brood(t):
    g = tag(scaled(ld.polygon(8, 1.0, PI / 8), 1.0, 0.74), 0)
    for i, (x, y) in enumerate(BROOD_CELLS[t]):
        g += tag(ld.transform(ld.polygon(3, 0.17, PI / 2 if i % 2 else -PI / 2), x, y), 1)
    return g


def tether(t):
    g = tag(ld.polygon(3, 1.0, 0.0), 0)
    if t == 1:
        g += tag(ld.polygon(3, 0.45, PI), 1)
    elif t == 2:
        g += tag(ld.polygon(6, 0.34, 0.0), 1)
    else:
        g += tag(ld.polygon(3, 0.5, PI), 1) + tag(ld.polygon(6, 0.2, 0.0), 2)
    if t >= 4:
        g += tag(ticks([0, 120, 240], 1.02, 1.32), 0)
    if t >= 5:
        g += tag(ld.polygon(3, 0.78, 0.0), 0)
    return g


def trefoil(n, scale=1.0, rot=0.0):
    pts = []
    for i in range(n):
        th = 2 * PI * i / n
        r = (0.55 + 0.45 * math.cos(3 * th)) * scale
        pts.append((r * math.cos(th + rot), r * math.sin(th + rot)))
    return ld.polyline(pts, closed=True)


def weaver(t):
    g = tag(trefoil(15), 0)
    if t == 2:
        g += tag(ld.polygon(3, 0.2, PI), 2)
    if t >= 3:
        g += tag(trefoil(9, 0.5, PI / 3), 1)
    if t >= 4:
        g += tag(ld.polygon(3, 0.16, 0.0), 2)
    if t >= 5:
        g += tag(ring_arcs(1.06, 3, 34, 2, 60), 1)
    return g


def well(t):
    if t == 1:
        return tag(ring_arcs(1.0, 3, 90, 3, 0), 0) + tag(ld.polygon(6, 0.42, 0.0), 1)
    if t == 2:
        return tag(ring_arcs(1.0, 3, 90, 3, 0), 0) + tag(ring_arcs(0.62, 2, 140, 3, 90), 1) + tag(ld.polygon(4, 0.15, 0.0), 2)
    if t == 3:
        return tag(ring_arcs(1.0, 3, 90, 3, 0), 0) + tag(ring_arcs(0.68, 3, 80, 2, 60), 1) + tag(ld.polygon(6, 0.36, 0.0), 2)
    if t == 4:
        return (tag(ring_arcs(1.0, 3, 90, 3, 0), 0) + tag(ring_arcs(0.68, 3, 80, 2, 60), 1) + tag(ld.polygon(6, 0.36, 0.0), 2)
                + tag(ticks([60, 180, 300], 0.86, 1.16), 0))
    return (tag(ring_arcs(1.0, 4, 68, 3, 0), 0) + tag(ring_arcs(0.72, 3, 84, 2, 45), 1)
            + tag(ring_arcs(0.46, 2, 130, 3, 0), 1) + tag(ld.polygon(6, 0.2, 0.0), 2))


def lemniscate(n, scale=1.0, rot=0.0, fat=1.7):
    pts = []
    for i in range(n):
        tt = 2 * PI * i / n
        d = 1 + math.sin(tt) ** 2
        x, y = math.cos(tt) / d, fat * math.sin(tt) * math.cos(tt) / d
        c, s = math.cos(rot), math.sin(rot)
        pts.append((scale * (x * c - y * s), scale * (x * s + y * c)))
    return ld.polyline(pts, closed=True)


def mimic(t):
    g = tag(lemniscate(16 if t < 4 else 14), 0)
    if t == 2:
        g += tag(ld.polygon(4, 0.14, 0.0), 2)
    if t in (3, 5):
        g += tag(ld.transform(ld.polygon(4, 0.12), 0.56, 0.0) + ld.transform(ld.polygon(4, 0.12), -0.56, 0.0), 2)
    if t >= 4:
        g += tag(lemniscate(10, 0.6, PI / 2), 1)
    return g


# --------------------------------------------------------------------------- player, pickups, marks

SHIP_HULL = ld.polyline([(1.15, 0.0), (0.10, 0.38), (-0.85, 0.95), (-0.45, 0.30), (-0.75, 0.0),
                         (-0.45, -0.30), (-0.85, -0.95), (0.10, -0.38)], closed=True)
SHIP_CORE = ld.polyline([(0.45, 0.0), (0.05, 0.16), (-0.30, 0.0), (0.05, -0.16)], closed=True)


def ship():
    return tag(SHIP_HULL, 0) + tag(SHIP_CORE, 2)


def halo(cells):
    """The ship's halo is the health display: one arc per Shield cell."""
    span = 360.0 / cells - 26.0 if cells > 1 else 320.0
    segs = max(2, int(round(span / 15.0)))
    return tag(ring_arcs(1.55, cells, span, segs, 90.0), 1)


def shard():
    return tag(ld.polyline([(0.0, 1.0), (0.55, 0.0), (0.0, -1.0), (-0.55, 0.0)], closed=True), 0)


def enemy_bullet():
    return tag(ld.polygon(4, 1.0, 0.0), 0)


def mark_b_heading():   # ranged, for families that face their heading: twin barrels beside the nose
    return tag(both(line(0.45, 0.42, 0.98, 0.42)), 2)


def mark_b_spin():      # ranged, for spinning families: three outward spikes
    return tag(ticks([90, 210, 330], 1.10, 1.40), 2)


def mark_c():           # special: two bracket arcs
    return tag(ld.arc(1.30, math.radians(50), math.radians(130), 4) + ld.arc(1.30, math.radians(230), math.radians(310), 4), 2)


FAMILIES = [
    # id, generator, faces heading?, relative size, note
    ("drifter", drifter, False, 1.00, "hexagon cell"),
    ("hound", hound, True, 1.00, "arrowhead"),
    ("flanker", flanker, True, 1.05, "crescent"),
    ("skitter", skitter, True, 0.80, "kite with tails"),
    ("ram", ram, True, 1.25, "horned wedge"),
    ("moth", moth, False, 1.05, "flower"),
    ("shoal", shoal, True, 0.55, "tiny dart"),
    ("wyrm_head", wyrm_head, True, 1.10, "serpent head"),
    ("wyrm_body", wyrm_body, True, 1.00, "serpent link"),
    ("carom", carom, False, 1.00, "nested squares"),
    ("blink", blink, False, 1.05, "sharp star"),
    ("sentry", sentry, True, 1.15, "gear turret"),
    ("brood", brood, False, 1.40, "egg sac"),
    ("tether", tether, False, 1.00, "triangle node"),
    ("weaver", weaver, False, 1.10, "trefoil"),
    ("well", well, False, 1.30, "broken rings"),
    ("mimic", mimic, True, 1.15, "figure eight"),
]


def extent(segs):
    return max(max(math.hypot(s[0], s[1]), math.hypot(s[2], s[3])) for s in segs)


def build():
    glyphs = []

    def add(gid, segs, **extra):
        entry = dict(id=gid, segmentCount=len(segs), extent=round(extent(segs), 3),
                     segments=[[round(v, 4) for v in s[:4]] + [s[4]] for s in segs])
        entry.update(extra)
        glyphs.append(entry)

    add("ship", ship(), kind="player", facesHeading=True, size=1.0)
    for n in range(1, 9):
        add("halo_%d" % n, halo(n), kind="player", facesHeading=False, size=1.0)
    add("shard", shard(), kind="pickup", facesHeading=False, size=0.3)
    add("enemy_bullet", enemy_bullet(), kind="threat", facesHeading=False, size=0.5)  # 0.38 until 2026-11-14 (the owner: "too small")
    add("mark_b_heading", mark_b_heading(), kind="mark", facesHeading=True, size=1.0)
    add("mark_b_spin", mark_b_spin(), kind="mark", facesHeading=False, size=1.0)
    add("mark_c", mark_c(), kind="mark", facesHeading=False, size=1.0)
    for fid, fn, heading, size, note in FAMILIES:
        for t in range(1, 6):
            add("%s_%d" % (fid, t), fn(t), kind="enemy", family=fid, tier=t, facesHeading=heading, size=size, note=note)
    return dict(
        note="Generated by design/tools/glyphs.py. Unit space, +x forward, y up. segments = [x0,y0,x1,y1,layer]; "
             "layer 0 hull, 1 inner ornament (independent rotation/pulse), 2 core mark. size = visual radius "
             "relative to the standard enemy radius. Low-detail LOD: drop layers 1 and 2.",
        glyphs=glyphs)


# --------------------------------------------------------------------------- preview

def draw_glyph(cv, segs, x, y, r, colour, intensity=2.0, width=1.7, rot=0.0):
    # y is flipped: glyph space is y up, canvas is y down
    flipped = [(s[0], -s[1], s[2], -s[3]) for s in segs]
    by_layer = {}
    for s, f in zip(segs, flipped):
        by_layer.setdefault(s[4], []).append(f)
    for layer, ss in by_layer.items():
        k = {0: 1.0, 1: 0.72, 2: 1.15}.get(layer, 1.0)
        cv.lines(ld.transform(ss, x, y, r, rot), colour, intensity * k, width * (1.0 if layer == 0 else 0.85))


def sheet(data, pal, font, rows, title, out_path):
    by_id = {g["id"]: g for g in data["glyphs"]}
    colours = [np.asarray(pal["enemyCandidates"][i]["linear"], np.float32) for i in pal["soloEnemies"]]
    white = np.ones(3, np.float32)
    col_w, row_h, left = 150, 132, 250
    w = left + col_w * 7
    h = 70 + row_h * len(rows)
    cv = ld.Canvas(w, h)
    cv.radial_bg(np.asarray(pal["bg0"]["linear"]) * 0.5, np.asarray(pal["bg1"]["linear"]) * 0.4)
    cv.text(font, title, 16, 16, 18, np.asarray(pal["accent"]["linear"], np.float32), 1.4)
    for i, label in enumerate(["I", "II", "III", "IV", "V", "III+B", "III+C"]):
        cv.text(font, label, left + i * col_w + 44, 44, 11, white, 0.8, 1.3)
    for r_i, (fid, heading, size) in enumerate(rows):
        cy = 70 + r_i * row_h + 54
        colour = colours[r_i % len(colours)]
        cv.text(font, fid.upper().replace("_", " "), 14, cy - 18, 12, white, 0.9, 1.4)
        for t in range(1, 6):
            g = by_id["%s_%d" % (fid, t)]
            cx = left + (t - 1) * col_w + 50
            draw_glyph(cv, g["segments"], cx, cy - 8, 34 * min(size, 1.15), colour)
            draw_glyph(cv, g["segments"], cx + 62, cy + 38, 13 * size, colour, 2.0, 1.5)   # in-game size
            cv.text(font, str(g["segmentCount"]), cx - 46, cy + 44, 7, white, 0.5, 1.0)
        g3 = by_id["%s_3" % fid]
        mark_b = by_id["mark_b_heading" if heading else "mark_b_spin"]
        for k, mark in enumerate((mark_b, by_id["mark_c"])):
            cx = left + (5 + k) * col_w + 50
            draw_glyph(cv, g3["segments"], cx, cy - 8, 34 * min(size, 1.15), colour)
            draw_glyph(cv, mark["segments"], cx, cy - 8, 34 * min(size, 1.15), white, 1.8, 1.5)
            draw_glyph(cv, g3["segments"], cx + 62, cy + 38, 13 * size, colour, 2.0, 1.5)
            draw_glyph(cv, mark["segments"], cx + 62, cy + 38, 13 * size, white, 1.8, 1.3)
    cv.render(bloom=0.85).save(out_path, optimize=True)


def player_sheet(data, pals, font, out_path):
    by_id = {g["id"]: g for g in data["glyphs"]}
    const = pals["constants"]
    pal = pals["palettes"][0]
    w, h = 1300, 330
    cv = ld.Canvas(w, h)
    cv.radial_bg(np.asarray(pal["bg0"]["linear"]) * 0.5, np.asarray(pal["bg1"]["linear"]) * 0.4)
    white = np.ones(3, np.float32)
    cv.text(font, "PLAYER SHIP  HALO ARCS = SHIELD CELLS", 16, 14, 16, white, 1.1)
    for i, p in enumerate(const["players"]):
        colour = np.asarray(p["linear"], np.float32)
        cx = 120 + i * 190
        cells = [3, 4, 2, 6][i]
        draw_glyph(cv, by_id["ship"]["segments"], cx, 150, 44, colour, 2.6, 2.2, rot=-0.5)
        draw_glyph(cv, by_id["ship"]["segments"], cx, 150, 44, white, 2.0, 0.8, rot=-0.5)
        draw_glyph(cv, by_id["halo_%d" % cells]["segments"], cx, 150, 44, colour, 1.3, 1.4, rot=0.3 * i)
        draw_glyph(cv, by_id["ship"]["segments"], cx - 30, 270, 15, colour, 2.6, 1.8, rot=0.4)
        draw_glyph(cv, by_id["ship"]["segments"], cx - 30, 270, 15, white, 2.0, 0.7, rot=0.4)
        draw_glyph(cv, by_id["halo_%d" % cells]["segments"], cx - 30, 270, 15, colour, 1.3, 1.1)
        cv.text(font, "P%d  %d CELLS" % (i + 1, cells), cx - 10, 262, 9, white, 0.7, 1.2)
    gold = np.asarray(const["pickup"]["linear"], np.float32)
    danger = np.asarray(const["danger"]["linear"], np.float32)
    for k in range(6):
        draw_glyph(cv, by_id["shard"]["segments"], 900 + k * 26, 120 + (k % 3) * 14, 6, gold, 3.0, 1.3, rot=0.3 * k)
    cv.text(font, "SHARDS", 890, 170, 9, white, 0.7, 1.2)
    for k in range(6):
        dia = [(s[0], -s[1], s[2], -s[3]) for s in by_id["enemy_bullet"]["segments"]]
        segs = ld.transform(dia, 1090 + k * 30, 120 + (k % 2) * 16, 6.5)
        cv.lines(segs, danger, 2.4, 2.0)
        cv.threat(segs, danger + white * 0.25, 1.9, 1.3)
    cv.text(font, "ENEMY BULLETS", 1070, 170, 9, white, 0.7, 1.2)
    cv.render(bloom=0.9).save(out_path, optimize=True)


def main():
    data = build()
    with open(os.path.join(DESIGN, "glyphs.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, separators=(",", ":"))
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    os.makedirs(os.path.join(DESIGN, "previews"), exist_ok=True)
    rows = [(fid, heading, size) for fid, fn, heading, size, note in FAMILIES]
    sheet(data, pals["palettes"][0], font, rows[:9], "ENEMY FAMILIES 1  TIERS I-V", os.path.join(DESIGN, "previews", "glyphs_1.png"))
    sheet(data, pals["palettes"][2], font, rows[9:], "ENEMY FAMILIES 2  TIERS I-V", os.path.join(DESIGN, "previews", "glyphs_2.png"))
    player_sheet(data, pals, font, os.path.join(DESIGN, "previews", "glyphs_player.png"))
    enemy = [g for g in data["glyphs"] if g.get("kind") == "enemy"]
    print("glyphs:", len(data["glyphs"]), "enemy glyphs:", len(enemy),
          "segments min/avg/max: %d / %.1f / %d" % (min(g["segmentCount"] for g in enemy),
                                                     sum(g["segmentCount"] for g in enemy) / len(enemy),
                                                     max(g["segmentCount"] for g in enemy)))


if __name__ == "__main__":
    main()
