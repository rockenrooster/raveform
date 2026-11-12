"""Boss 5, Polyrhythm: two rotors, Three and Four, firing three against four.

Writes design/bosses/polyrhythm.json (numbers) and design/previews/boss_polyrhythm.png.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\boss_polyrhythm.py
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

BPM = 144.0
BEAT = 60.0 / BPM
BAR = 4 * BEAT
SPAN = 720.0                       # Three to Four along the link
ROTOR = 92.0                       # radius of each rotor's outer shape
RING_SPEED = 260.0
CAMERA_ZOOM = 0.75
# sides, pulses per bar, arc degrees (the rest of each side's share is the gap), turn per pulse
THREE = dict(sides=3, pulses=3, arcDeg=80.0, turnDeg=40.0)
FOUR = dict(sides=4, pulses=4, arcDeg=60.0, turnDeg=22.5)
WALL_LIFE = 2.6                    # seconds a wall lives: about a bar and a half

NUMBERS = dict(
    health={"three": 1400, "four": 1400},
    rotorRadius=ROTOR,
    span=SPAN,
    three=THREE,
    four=FOUR,
    rule=("each rotor fires a ring on each of its pulses: every corner throws a solid arc of light, a wall 10 wu thick that grows outward, and its flat faces throw nothing, "
          "so every ring has one gap in front of every face. These are walls, not bullets: there is no slipping between them, only through the gaps (or a Dash). A Nova erases every wall it touches. The faces are glass with a gold core behind: the only place a rotor can be hurt. "
          "Stand in a gap and the face in front of you is open to your shots"),
    turn=("a rotor turns turnDeg between pulses, so its gaps step round: Three's by 40 degrees three times a bar, Four's by 22.5 degrees four times a bar. "
          "Both come back to where they started every bar, and on beat 1 a face of each looks straight at the other: for that pulse both rings have their gap on the link"),
    theOne="beat 1 of every bar: both fire together and both of those rings have a gap along the link, so they pass either side of anyone standing on it, and each rotor's face is open down its own gap. Rings fired on the other pulses still cross the link: the One is a moment, not a shelter",
    link={
        "ticks": 12,
        "note": "a white line from Three to Four ruled into twelve: the bar in twelfths. A playhead runs along it once a bar. Three's ticks (0, 4, 8) are its colour, Four's (0, 3, 6, 9) are its colour, tick 0 is gold. The next tick to fire glows a beat ahead: the link is the score of the fight",
    },
    pivot="the midpoint of the link; it follows the players' centre at 50 wu/s, and the link turns round it once every 8 bars",
    ringSpeed=RING_SPEED,
    wallLifeSeconds=WALL_LIFE,
    wallThickness=10.0,
    camera={"zoom": CAMERA_ZOOM, "note": "eased over the entrance bar; both rotors in the frame"},
    phases=[
        {"name": "Three against four", "when": "both alive, above 60% of their joint health",
         "note": "the pattern as ruled. Learn the One, then learn to ride one rotor's gap round while stepping through the other's rings"},
        {"name": "Hemiola", "when": "both alive, 60% and below",
         "turn": "each rotor reverses its turn every bar",
         "linkBeam": {"every": "second bar", "telegraphBars": 1, "burnBeats": 2, "when": "beats 3 and 4",
                      "note": "the link shows danger dashes for a bar and then burns: the aisle is still safe on the One, but nobody can live in it"},
         "ringSpeed": "Four's walls fly at 300",
         "note": "the gaps now swing back and forth instead of circling"},
        {"name": "Solo", "when": "one rotor dead",
         "survivor": "fires both patterns from its own body: its own ring on its own pulses and the dead rotor's count on a second ring that turns the other way. The gaps of the two rings line up once a bar, on the One, in front of one face",
         "move": "it walks toward the nearest player at 70 wu/s",
         "damageScale": 1.25,
         "note": "kill them close together and this phase is short; kill one early and the other carries both parts for a long time"},
    ],
    damageScale={"face": 1.0, "corner": 0.0, "body": 0.0},
    contact="a rotor hurts; the link does not, except while it burns",
    music={
        "three": "the tom, tuned to the Track's root, on Three's pulses (twelfths 0, 4, 8)",
        "four": "the rim, tuned to the flat second, on Four's pulses (twelfths 0, 3, 6, 9)",
        "theOne": "both and the gong on beat 1",
        "pitch": "each drum rises a semitone for every quarter of its rotor's health lost",
        "solo": "the survivor plays both drums at its own pitch",
        "note": "three against four is audible from the first bar; a player can dodge with their eyes shut once they have the pattern",
    },
    look={
        "three": "a triangle in the palette's cyan with a smaller triangle turning the other way inside it; corners are bright nodes that glint the twelfth before they fire",
        "four": "a square in the palette's amber, built the same way",
        "faces": "each flat face is a pane of glass with a gold bar behind it: gold is the weak point",
        "rings": "walls are arcs in the danger colour on the priority layer, 3 px with the threat rim, like every threat; they thin to 2 px in their last half second; the gaps are plainly empty",
        "aisle": "on the One a white flare runs the length of the link and a faint white corridor shows through both sets of rings",
    },
    entrance="on the Drop's downbeat Three drops in on beat 1 and Four on beat 3, each landing with its drum; the link draws itself between them over the next bar, tick by tick; the name card POLYRHYTHM holds for two bars; firing starts on bar 3",
    death="the first rotor to die spins down over a bar and bursts on the One. When the second dies the link's playhead runs one last bar with every tick firing as a ring of sparks instead of bullets, then the boss break on the One: three rings, 300 sparks, the 30 px distortion ring, 50 ms hit-stop, 240 Shards",
    coop="health x (1 + 0.5 x (players - 1)); the pivot follows the players' centre; in Solo the survivor walks toward the nearest player",
)


def build():
    return {
        "note": "Generated by design/tools/boss_polyrhythm.py. World units. A rotor is a regular polygon of `sides` sides, radius rotorRadius, with an inner copy at 0.5 turning the other way; the numbers are the fight.",
        "bpm": BPM,
        "parts": {
            "rotor": {"radius": ROTOR, "innerScale": 0.5, "cornerNodeRadius": 9.0, "faceBar": {"lengthFraction": 0.5, "inset": 14.0}},
            "link": {"ticks": 12, "tickLength": 22.0, "oneTickLength": 44.0, "widthRefPx": 2.0},
        },
        "numbers": NUMBERS,
    }


class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == "furnace")
        self.glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
        self.font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
        lin = lambda e: np.asarray(e["linear"], np.float32)
        self.white = np.ones(3, np.float32)
        self.accent = lin(self.pal["accent"])
        self.danger = lin(self.pals["constants"]["danger"])
        p = self.pals["constants"]["players"][0]
        self.p1, self.p1_deep, self.p1_light = lin(p), lin(p["deep"]), lin(p["light"])
        self.gold = lin(self.pals["constants"]["pickup"])
        self.grid = lin(self.pal["grid"])
        self.bg0, self.bg1 = lin(self.pal["bg0"]), lin(self.pal["bg1"])
        self.cyan = ld.oklch(0.82, 205.0, cf=0.96)[0].astype(np.float32)
        self.cyan_deep = ld.oklch(0.58, 215.0, cf=0.96)[0].astype(np.float32)
        self.amber = ld.oklch(0.82, 70.0, cf=0.96)[0].astype(np.float32)
        self.amber_deep = ld.oklch(0.60, 45.0, cf=0.96)[0].astype(np.float32)


def preview(path):
    a = Art()
    W, H = 2400, 1080
    s = 1.5 * CAMERA_ZOOM
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.5, a.bg1 * 0.5, cx=0.55, cy=0.5)
    cv.nebula([(700, 300, 760, a.cyan, 0.020), (1900, 760, 760, a.amber, 0.022)])
    step_px = 60 * s
    grid = [(gx, 0, gx, H) for gx in np.arange(W / 2 % step_px, W + 1, step_px)] + [(0, gy, W, gy) for gy in np.arange(H / 2 % step_px, H + 1, step_px)]
    cv.lines(grid, a.grid, 0.15, 1.0)

    pivot = (W * 0.56, H * 0.5)
    link_ang = math.radians(-8.0)
    ux, uy = math.cos(link_ang), math.sin(link_ang)
    p3 = (pivot[0] - ux * SPAN / 2 * s, pivot[1] - uy * SPAN / 2 * s)
    p4 = (pivot[0] + ux * SPAN / 2 * s, pivot[1] + uy * SPAN / 2 * s)
    now = 0.06                         # bars since the One: both have just fired, the aisle is open

    def rings(pos, spec, facing, colour):
        """Every ring still alive: pulse i fired at i / pulses bars; the rotor had turned i * turnDeg by then."""
        out = []
        n, pulses = spec["sides"], spec["pulses"]
        for back in range(0, 3 * pulses):
            t_fire = -back / pulses                # in bars, relative to the last One
            age = (now - t_fire) * BAR
            if age <= 0 or age > WALL_LIFE:
                continue
            idx = (-back) % pulses
            turn = math.radians(spec["turnDeg"] * idx)
            radius = (ROTOR + age * RING_SPEED) * s
            share = 2 * PI / n
            for side in range(n):
                mid = facing + turn + side * share + share / 2      # a corner sits between two faces
                half = math.radians(spec["arcDeg"]) / 2
                out.append(([(pos[0] + radius * math.cos(t), pos[1] + radius * math.sin(t)) for t in np.linspace(mid - half, mid + half, 28)], age))
        return out

    bullets = rings(p3, THREE, link_ang, a.cyan) + rings(p4, FOUR, link_ang + PI, a.amber)

    # the aisle: two faint white rails either side of the link on the One
    for side in (1.0, -1.0):
        ox, oy = -uy * 46 * s * side, ux * 46 * s * side
        cv.lines([(p3[0] - ux * 900 + ox, p3[1] - uy * 900 + oy, p4[0] + ux * 900 + ox, p4[1] + uy * 900 + oy)], a.white, 0.32, 1.2)

    # the link: the bar ruled into twelfths, with a playhead
    cv.lines([(p3[0], p3[1], p4[0], p4[1])], a.white, 1.6, 2.0)
    nx, ny = -uy, ux
    for k in range(13):
        f = k / 12
        tx, ty = p3[0] + (p4[0] - p3[0]) * f, p3[1] + (p4[1] - p3[1]) * f
        is3, is4 = k % 4 == 0, k % 3 == 0
        if k in (0, 12):
            col, ln, lev = a.gold, 44.0, 3.0
        elif is3:
            col, ln, lev = a.cyan, 30.0, 2.4
        elif is4:
            col, ln, lev = a.amber, 30.0, 2.4
        else:
            col, ln, lev = a.white, 14.0, 0.8
        if is3 and k not in (0, 12):
            cv.lines([(tx, ty, tx + nx * ln * s, ty + ny * ln * s)], col, lev, 2.4)
        elif is4 and k not in (0, 12):
            cv.lines([(tx, ty, tx - nx * ln * s, ty - ny * ln * s)], col, lev, 2.4)
        else:
            cv.lines([(tx - nx * ln * s / 2, ty - ny * ln * s / 2, tx + nx * ln * s / 2, ty + ny * ln * s / 2)], col, lev, 2.0)
    hx, hy = p3[0] + (p4[0] - p3[0]) * now, p3[1] + (p4[1] - p3[1]) * now
    cv.discs([(hx, hy, 7 * s)], a.white, 2.4)
    cv.flare(hx, hy, 70 * s, a.white, 1.8, 1.3, 4, link_ang)

    def rotor(pos, spec, facing, col, deep):
        n = spec["sides"]
        x, y = pos
        rot = facing + PI / n            # a face looks along `facing`
        outer = ld.polygon(n, 1.0, rot)
        cv.glow(x, y, 260 * s, col, 0.20)
        pts = [(x + ROTOR * s * math.cos(rot + i * 2 * PI / n), y + ROTOR * s * math.sin(rot + i * 2 * PI / n)) for i in range(n)]
        cv.fill(pts, col, 0.10)
        cv.lines(ld.transform(outer, x, y, ROTOR * s), ld.f_radial(x, y, ROTOR * s, [a.white, col, deep]), 3.0, 3.0)
        cv.lines(ld.transform(outer, x, y, ROTOR * s), a.white, 1.6, 1.0)
        cv.lines(ld.transform(ld.polygon(n, 1.0, rot + PI / n + 0.3), x, y, ROTOR * 0.5 * s), deep * 0.4 + col * 0.6, 2.2, 2.0)
        cv.lines(ld.transform(ld.polygon(n * 2, 1.0, rot), x, y, ROTOR * 0.2 * s), a.white, 1.8, 1.4)
        for i in range(n):
            cxn, cyn = pts[i]
            cv.discs([(cxn, cyn, 9 * s)], a.white, 2.2)
            cv.flare(cxn, cyn, 60 * s, a.danger * 0.6 + a.white * 0.4, 1.6, 1.2, 4, rot + i * 2 * PI / n)
            # the face between corner i and i + 1: a gold bar behind the glass
            x1, y1 = pts[(i + 1) % n]
            mx, my = (cxn + x1) / 2, (cyn + y1) / 2
            ix, iy = x - mx, y - my
            il = math.hypot(ix, iy)
            bx, by = mx + ix / il * 14 * s, my + iy / il * 14 * s
            ex, ey = (x1 - cxn) * 0.25, (y1 - cyn) * 0.25
            cv.lines([(bx - ex, by - ey, bx + ex, by + ey)], a.gold, 3.0, 3.2)
        cv.glow(x, y, 80 * s, a.gold, 0.25)

    rotor(p3, THREE, link_ang, a.cyan, a.cyan_deep)
    rotor(p4, FOUR, link_ang + PI, a.amber, a.amber_deep)

    # the ship: on the link, in both gaps, firing into Three's open face
    sx, sy = p3[0] + (p4[0] - p3[0]) * 0.36, p3[1] + (p4[1] - p3[1]) * 0.36
    aim = math.atan2(p3[1] - sy, p3[0] - sx)
    hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
    r = 14 * s
    cv.glow(sx, sy, r * 6, a.p1, 0.15)
    cv.lines(ld.transform(hull, sx, sy, r, aim), ld.f_radial(sx, sy, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
    cv.lines(ld.transform(hull, sx, sy, r, aim), a.white, 2.0, 0.8)
    halo3 = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo3, sx, sy, r, 0.4), ld.f_angular(sx, sy, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)
    fx, fy = p3[0] + ux * ROTOR * 0.5 * s, p3[1] + uy * ROTOR * 0.5 * s
    for lane in (-10.0, 0.0, 10.0):
        ox, oy = -math.sin(aim) * lane * s, math.cos(aim) * lane * s
        for u in np.arange(0.2, 0.95, 0.24):
            hx2, hy2 = sx + (fx - sx) * u + ox, sy + (fy - sy) * u + oy
            tx2, ty2 = hx2 - math.cos(aim) * 31 * s, hy2 - math.sin(aim) * 31 * s
            cv.lines([(tx2, ty2, hx2, hy2)], ld.f_linear(tx2, ty2, hx2, hy2, [a.p1, a.white]), 4.6, 2.2)
    for i in range(9):
        an = aim + PI + (i - 4) * 0.24
        cv.lines([(fx + math.cos(an) * 10, fy + math.sin(an) * 10, fx + math.cos(an) * (34 + 9 * (i % 3)), fy + math.sin(an) * (34 + 9 * (i % 3)))], a.gold * 0.5 + a.white * 0.5, 2.2, 1.3)

    for pts, age in bullets:
        thin = age > WALL_LIFE - 0.5
        cv.threat(ld.polyline(pts), a.danger * 0.75 + a.white * 0.25, 2.0 if thin else 3.0, 1.5)

    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))
    note("POLYRHYTHM", 110, 60, 44, a.white, 1.6)
    note("BOSS OF TRACK 5  FURNACE  144 BPM", 110, 126, 14, a.accent, 1.0)
    note("THREE FIRES THREE TIMES A BAR", 110, 934, 13, a.cyan)
    note("FOUR FIRES FOUR TIMES A BAR", 110, 960, 13, a.amber)
    note("WALLS OF LIGHT, WITH A GAP IN FRONT OF EVERY FACE: THE FACES ARE THE WEAK POINTS", 110, 986, 13, a.gold)
    note("ON THE ONE BOTH GAPS LIE ON THE LINK", 110, 1012, 13)
    note("3", p3[0] - 16, p3[1] - ROTOR * s - 70, 36, a.cyan, 1.6)
    note("4", p4[0] - 16, p4[1] - ROTOR * s - 70, 36, a.amber, 1.6)
    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "polyrhythm.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "boss_polyrhythm.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
