"""Boss 4, Event Horizon: a well that eats the crowd and erupts with what it ate.

Writes design/bosses/horizon.json (numbers) and design/previews/boss_horizon.png.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\boss_horizon.py
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

BPM = 138.0
BEAT = 60.0 / BPM
HORIZON = 120.0                    # the black disc: anything that crosses it is eaten
PHOTON = 136.0                     # the thin white ring round it
DISC = [(190.0, 0.9), (250.0, -0.6), (320.0, 0.4)]      # accretion rings: radius, turns per bar (sign = direction)
PULL_RANGE = 1400.0
PULL_SHIP_MAX = 260.0              # wu/s at the photon ring: half the ship's speed, so flight always wins
BULLET_SPEED = 200.0
CAMERA_ZOOM = 0.8
MEAL_CELLS = 12                    # cells on the mass meter

NUMBERS = dict(
    health=3200,
    horizonRadius=HORIZON,
    photonRadius=PHOTON,
    disc=[{"radius": r, "turnsPerBar": t} for r, t in DISC],
    damageScale={"starved": 1.0, "fed": 0.15},
    contact="the horizon takes a Shield cell and throws the ship 300 wu outward; nothing else about the Well touches",
    pull={
        "range": PULL_RANGE,
        "rule": "everything inside the range accelerates toward the Well: speed toward it = max x (photonRadius / distance) ^ 1.2, added to its own motion",
        "max": {"ship": PULL_SHIP_MAX, "fodder": 340.0, "enemyBullets": 180.0, "playerShots": 220.0, "shards": 300.0},
        "note": "the ship flies at 520, so it can always leave; a Dash ignores the pull. Player shots bend: aim ahead of the curve. The lattice is pulled with a standing impulse of 400 at the horizon, falling off the same way",
    },
    feeding={
        "lanes": "spiral arms that start 1,300 wu out and wind 270 degrees into the Well; the director's whole crowd for this fight arrives along them, fodder only",
        "eaten": "a body that crosses the horizon is eaten: no Shards, no score, one cell on the meal meter. Five uncollected Shards that cross it count as one body",
        "meter": {"cells": MEAL_CELLS, "note": "an arc of cells round the photon ring, filling clockwise in the danger colour as it eats; emptied on every bar line"},
    },
    barLine={
        "rule": "on every bar line the Well looks at its meal. Fed (the meal reached the phase's hunger): it erupts and its shield holds for the bar. Starved (it did not): it gasps, and for the whole next bar it takes full damage",
        "eruption": {"bullets": "8 + 4 per body eaten, at most 96", "pattern": "spiral arms: as many arms as there are lanes, the bullets leaving one per sixteenth along each arm, each arm turning 12 degrees per bullet",
                     "bulletSpeed": BULLET_SPEED, "bulletLifeSeconds": 6.0, "note": "its bullets are pulled too: slow ones curl back and are eaten, without filling the meter"},
        "gasp": {"note": "the rings thin to a third, the horizon's rim turns gold, the core shows: a gold four-point star, the weak point"},
    },
    phases=[
        {"name": "Accretion", "health": [1.0, 0.66], "lanes": 2, "hunger": 5, "streamPerBar": 10,
         "note": "two lanes, opposite each other: one player can hold both by circling"},
        {"name": "Tidal", "health": [0.66, 0.33], "lanes": 3, "hunger": 7, "streamPerBar": 18, "pullScale": 1.25,
         "wave": {"when": "beat 3 of every bar", "note": "a ring leaves the Well at 600 wu/s and shoves ships and Shards 140 wu outward as it passes; it does no damage, it breaks your position"},
         "note": "three lanes: no place covers them all, so the meal is decided by where you stand when the bar ends"},
        {"name": "Collapse", "health": [0.33, 0.0], "lanes": 4, "hunger": 9, "streamPerBar": 26, "pullScale": 1.5, "driftWu": 90.0,
         "jets": {"when": "a fed bar line", "telegraphBars": 1, "burnBeats": 2, "note": "two opposite beams along the Well's axis, shown as danger dashes for the bar before; the axis turns 45 degrees every bar"},
         "note": "four lanes and the Well comes for you; starving it now takes the whole build"},
    ],
    camera={"zoom": CAMERA_ZOOM, "note": "eased over the entrance bar; the Well is kept in the frame as for every boss"},
    music={
        "drone": "a sub sine on the Track's root that sinks a semitone for every three cells on the meter and springs back on the bar line",
        "eaten": "each body eaten plays the next note of the Track's arp an octave down, bending down a fifth: you hear the meal being counted",
        "eruption": "the impact and the gong on the bar line, the louder the bigger the meal",
        "gasp": "the pads and the bass cut out for the first beat of a starved bar under a reversed cymbal: that silence is the signal to attack",
        "tidalWave": "the riser's last half-beat into beat 3",
    },
    look={
        "black": "the disc inside the horizon is drawn last and is true black: no bloom, no afterglow, no grid. It is the only black thing in the game",
        "rings": "three rings of dashes in the Track's three enemy hues, each turning at its own rate; the side turning toward the camera's right is 1.8x brighter than the other, so the disc looks tilted and alive",
        "photon": "one white line round the horizon, 2 px; gold while starved",
        "meter": "twelve cells just outside the photon ring in the danger colour; the cell being filled flashes white",
        "lens": "the lattice bends into the Well; lines that cross the horizon stop at it",
        "trails": "everything on a lane leaves a trail in its own hue, so the lanes read as spiral arms of light",
    },
    entrance="on the Drop's downbeat every light on screen leans toward one point and the grid pinches there over two beats; the disc opens black on beat 3 and the rings spin up over the rest of the bar; the name card EVENT HORIZON holds for two bars",
    death="it implodes: for one bar everything left alive, every bullet, every spark and the lattice itself spiral into a point and the music thins to the drone alone; one beat of black and silence; then the clamped white flash on the downbeat, three rings, 400 sparks in the Track's hues, the 30 px distortion ring, 50 ms hit-stop, and everything it ate comes back as Shards: 300 in a fountain",
    coop="health x (1 + 0.5 x (players - 1)); hunger +2 per extra player; lanes are spread evenly whatever the player count, so a team covers them by splitting up",
)


def build():
    return {
        "note": "Generated by design/tools/boss_horizon.py. World units. The Well has no hull: it is a black disc, a photon ring, three rings of dashes and a meter; everything else is rules.",
        "bpm": BPM,
        "parts": {
            "horizon": {"radius": HORIZON},
            "photonRing": {"radius": PHOTON, "widthRefPx": 2.0},
            "disc": [{"radius": r, "dashes": int(round(r / 9.0)), "dashFraction": 0.6, "turnsPerBar": t} for r, t in DISC],
            "meter": {"radius": 152.0, "cells": MEAL_CELLS, "cellFraction": 0.72, "widthRefPx": 4.0},
            "core": {"star": {"points": 4, "outer": 46.0, "inner": 14.0}, "note": "gold; drawn only while starved"},
        },
        "numbers": NUMBERS,
    }


class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == "aurora")
        self.glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
        self.font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
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
        self.grid = lin(self.pal["grid"])
        self.bg0, self.bg1 = lin(self.pal["bg0"]), lin(self.pal["bg1"])


def preview(path):
    a = Art()
    W, H = 2400, 1080
    s = 1.5 * CAMERA_ZOOM
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.5, a.bg1 * 0.5, cx=0.56, cy=0.5)
    cv.nebula([(480, 260, 780, a.hue[1], 0.022), (2000, 820, 780, a.hue[0], 0.022)])
    cx, cy = W * 0.56, H * 0.5

    def lens(x, y):
        dx, dy = x - cx, y - cy
        r = math.hypot(dx, dy)
        k = 1.0 - 0.62 / (1.0 + (r / (300.0 * s)) ** 2)
        return cx + dx * k, cy + dy * k

    # the lattice, pulled into the Well
    step_px = 60 * s
    for gx in np.arange(cx % step_px - step_px * 6, W + step_px * 6, step_px):
        cv.lines(ld.polyline([lens(gx, y) for y in np.linspace(-200, H + 200, 90)]), a.grid, 0.20, 1.0)
    for gy in np.arange(cy % step_px - step_px * 6, H + step_px * 6, step_px):
        cv.lines(ld.polyline([lens(x, gy) for x in np.linspace(-300, W + 300, 150)]), a.grid, 0.20, 1.0)

    # three lanes (the Tidal phase): spiral arms of fodder winding 270 degrees into the Well
    lanes = 3
    spin = 0.85

    def lane_point(arm, u):
        """u = 0 at the mouth of the lane (1,300 wu out), 1 at the horizon."""
        r = (1300.0 * (1.0 - u) ** 1.35 + HORIZON) * s
        t = spin + arm * 2 * PI / lanes + u * 1.5 * PI
        return cx + r * math.cos(t), cy + r * math.sin(t) * 0.999

    hexa = ld.polygon(6)
    tri = ld.polygon(3, 0.45)
    for arm in range(lanes):
        col, mate = a.hue[arm % 3], a.mate[arm % 3]
        spine = [lane_point(arm, u) for u in np.linspace(0.0, 1.0, 120)]
        for i in range(len(spine) - 1):
            u = i / (len(spine) - 1)
            cv.lines([(spine[i][0], spine[i][1], spine[i + 1][0], spine[i + 1][1])], col, 0.25 + 0.9 * u, 1.2 + 2.2 * u)
        cut = (arm == 0)               # the lane the ship is cutting: bodies only on its outer part
        n = 26
        for j in range(n):
            u = 0.04 + 0.95 * (j / n) + 0.012 * math.sin(j * 2.4)
            if cut and 0.42 < u < 0.80:
                continue
            x, y = lane_point(arm, u)
            x2, y2 = lane_point(arm, min(1.0, u + 0.01))
            heading = math.atan2(y2 - y, x2 - x)
            jit = 16 * s * math.sin(j * 1.7 + arm)
            x, y = x - math.sin(heading) * jit, y + math.cos(heading) * jit
            size = (13.0 - 5.0 * u) * s
            cv.lines(ld.transform(hexa, x, y, size, heading), ld.f_radial(x, y, size, [col * 0.6 + a.white * 0.4, col, mate]), 2.2, 1.7)
            cv.lines(ld.transform(tri, x, y, size, heading), mate, 1.6, 1.2)
            tx, ty = lane_point(arm, max(0.0, u - 0.035))
            cv.lines([(tx, ty, x, y)], ld.f_linear(tx, ty, x, y, [col * 0.0, col]), 1.2, 1.4)

    # the accretion rings: dashes, brighter on the side turning toward the right
    for i, (r, turn) in enumerate(DISC):
        n = int(round(r / 9.0))
        col = a.hue[i % 3]
        for j in range(n):
            t0 = (j + 0.2 * i) * 2 * PI / n + turn * 0.7
            t1 = t0 + 0.6 * 2 * PI / n
            side = 0.5 + 0.5 * math.cos(t0 - PI * 0.15 * (1 if turn > 0 else -1))
            level = 1.0 + 1.9 * side
            seg = ld.polyline([(cx + r * s * math.cos(t), cy + r * s * math.sin(t)) for t in np.linspace(t0, t1, 4)])
            cv.lines(seg, col * (0.55 + 0.45 * side) + a.white * 0.25 * side, level, 2.4 - 0.3 * i)
    cv.glow(cx, cy, 520 * s, a.hue[1], 0.10)
    cv.glow(cx, cy, 300 * s, a.hue[0], 0.14)

    # the meal meter: seven of twelve cells eaten, the eighth flashing
    mr = 152.0 * s
    for c in range(MEAL_CELLS):
        t0 = -PI / 2 + c * 2 * PI / MEAL_CELLS
        t1 = t0 + 0.72 * 2 * PI / MEAL_CELLS
        seg = ld.polyline([(cx + mr * math.cos(t), cy + mr * math.sin(t)) for t in np.linspace(t0, t1, 6)])
        if c < 7:
            cv.lines(seg, a.danger * 0.8 + a.white * 0.2, 3.2, 5.0)
            cv.threat(seg, a.danger * 0.8 + a.white * 0.2, 4.0, 1.6)
        elif c == 7:
            cv.lines(seg, a.white, 3.0, 4.0)
        else:
            cv.lines(seg, a.danger, 0.35, 3.0)

    # last bar's eruption: three spiral arms of bullets, already curling back
    bullets = []
    for arm in range(lanes):
        for j in range(12):
            age = (j + 1.5) * BEAT / 4 + 0.35
            r = (PHOTON + age * BULLET_SPEED * (1.0 - 0.10 * age)) * s
            t = 1.4 + arm * 2 * PI / lanes + math.radians(12.0 * (11 - j)) + 0.25 * age
            bullets.append((cx + r * math.cos(t), cy + r * math.sin(t)))

    # the ship: outside the rings, cutting the second lane; its shots bend toward the Well
    lx, ly = lane_point(0, 0.50)
    out = math.hypot(lx - cx, ly - cy)
    sx, sy = lx + (lx - cx) / out * 190, ly + (ly - cy) / out * 190
    target = lane_point(0, 0.47)
    aim = math.atan2(target[1] - sy, target[0] - sx)
    hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
    r = 14 * s
    cv.glow(sx, sy, r * 6, a.p1, 0.13)
    cv.lines(ld.transform(hull, sx, sy, r, aim), ld.f_radial(sx, sy, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
    cv.lines(ld.transform(hull, sx, sy, r, aim), a.white, 2.0, 0.8)
    halo3 = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo3, sx, sy, r, 0.4), ld.f_angular(sx, sy, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)
    for lane in (-12.0, 0.0, 12.0):
        px, py = sx + math.cos(aim) * 30 * s, sy + math.sin(aim) * 30 * s
        vx, vy = math.cos(aim + math.radians(lane * 0.5)), math.sin(aim + math.radians(lane * 0.5))
        path_pts = []
        for i in range(60):
            dx, dy = cx - px, cy - py
            d = math.hypot(dx, dy)
            g = 0.020 * (PHOTON * s / d) ** 1.2 * 22
            vx, vy = vx + dx / d * g * 0.08, vy + dy / d * g * 0.08
            n = math.hypot(vx, vy)
            vx, vy = vx / n, vy / n
            px, py = px + vx * 9 * s, py + vy * 9 * s
            path_pts.append((px, py))
        for i0 in (6, 24, 42):
            seg = path_pts[i0:i0 + 5]
            cv.lines(ld.polyline(seg), ld.f_linear(seg[0][0], seg[0][1], seg[-1][0], seg[-1][1], [a.p1, a.white]), 4.4, 2.2)
        cv.lines(ld.polyline(path_pts[::3]), a.p1, 0.22, 1.0)
    # kills on the cut lane: bursts and gold Shards drifting toward the Well
    for u in (0.45, 0.52, 0.60):
        kx, ky = lane_point(0, u)
        cv.flare(kx, ky, 54 * s, a.white, 1.8, 1.2, 4, u * 9)
        cv.lines(ld.transform(ld.polygon(12), kx, ky, 26 * s), a.hue[0], 1.2, 1.3)
        for i in range(4):
            gx, gy = kx + 30 * s * math.cos(i * 1.9 + u * 20), ky + 30 * s * math.sin(i * 1.9 + u * 20)
            cv.lines(ld.transform(ld.polygon(4), gx, gy, 5.5 * s, 0.3 * i), a.gold, 2.4, 1.3)

    # the black disc and its photon ring, last
    disc = [(cx + HORIZON * s * math.cos(t), cy + HORIZON * s * math.sin(t)) for t in np.linspace(0, 2 * PI, 72, endpoint=False)]
    cv.lines(ld.transform(ld.polygon(96), cx, cy, PHOTON * s), a.white, 2.6, 2.0)
    for bx, by in bullets:
        if math.hypot(bx - cx, by - cy) > PHOTON * s + 8:
            cv.threat(ld.transform(ld.polygon(4), bx, by, 7.5), a.danger * 0.75 + a.white * 0.25, 2.1, 1.5)

    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))
    note("EVENT HORIZON", 110, 60, 44, a.hue[0], 1.6)
    note("BOSS OF TRACK 4  AURORA  138 BPM", 110, 126, 14, a.accent, 1.0)
    note("IT EATS WHAT YOU LEAVE ALIVE", 110, 934, 13)
    note("FED: IT ERUPTS ON THE BAR LINE WITH EVERYTHING IT ATE", 110, 960, 13, a.danger * 0.75 + a.white * 0.25)
    note("STARVED: IT GASPS, AND FOR ONE BAR IT CAN BE HURT", 110, 986, 13, a.gold)
    note("CUT THE LANES. YOUR SHOTS BEND", 110, 1012, 13)
    note("MEAL  7 OF 12", cx + 180 * s, cy - 210 * s, 13, a.danger * 0.75 + a.white * 0.25)
    img = cv.render(bloom=0.95)
    # true black inside the horizon: nothing bleeds in
    from PIL import Image, ImageDraw
    mask = Image.new("L", (W * 2, H * 2), 0)
    ImageDraw.Draw(mask).ellipse([(cx - HORIZON * s) * 2, (cy - HORIZON * s) * 2, (cx + HORIZON * s) * 2, (cy + HORIZON * s) * 2], fill=255)
    mask = mask.resize((W, H), Image.LANCZOS)
    img = img.convert("RGB")
    img.paste(Image.new("RGB", (W, H), (0, 0, 0)), (0, 0), mask)
    del disc
    img.save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "horizon.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "boss_horizon.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
