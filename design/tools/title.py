"""RAVEFORM title v2, "First Light": the main menu, its intro and its outro (the plan: design/title.md).

Every frame is one function of a state (St), so the rest frame and the twelve storyboard moments are the
same drawing with different numbers. Drawn at 2400x1080 with the game's own light (design/tools/lookdev.py).

Usage:  python -I -B design/tools/title.py [hero] [intro] [outro] [layout]      (no argument: all four)
  -> design/previews/title_v2.png          the title at rest
     design/previews/title_v2_intro.png    six moments of the intro (cold launch)
     design/previews/title_v2_outro.png    six moments of the drop (PLAY -> the run's first bar)
     design/previews/title_v2_layout.png   the rest frame with its measures
"""
import math
import os
import sys
from dataclasses import dataclass, replace

import numpy as np
from PIL import Image, ImageDraw, ImageFont

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import screens as sc  # noqa: E402

ld = sc.ld
W, H, M, PI = sc.W, sc.H, sc.M, math.pi
CX = W / 2.0

# ---- the measures of design/title.md (reference pixels at 2400x1080)
CAP = 240.0            # the wordmark's cap height
ADV = 5.25             # font units from one letter to the next (the font's own advance is 6): the wordmark's tracking
TOP = 138.0            # the caps' top
BASE = TOP + CAP
LW = (7 * ADV + 4) / 6.0 * CAP      # the word's width: 1630
X0 = CX - LW / 2.0
HORIZON = 0.56         # where the chrome turns, down a letter
TUBE = 14.0
CORE = 4.0
RULE = 432.0           # the rule: the line the word stands over
DEPTH = 6              # extrusion layers
DEPTH_STEP = 0.011     # each layer this much smaller about the rule's centre
REFL_SQUASH = 0.55
TAG = "S O U N D   B E C O M E S   L I G H T"
TAG_CAP = 22.0
PLAY_Y = 800.0
PLAY_W, PLAY_H = 640.0, 150.0
SIDE_DX, SIDE_W, SIDE_H = 560.0, 340.0, 104.0
LASERS = (-69.0, -46.0, -23.0, 0.0, 23.0, 46.0, 69.0)   # degrees from straight up


@dataclass
class St:
    bg: float = 1.0            # the background gradient, its clouds and stars
    lattice: float = 0.2       # the lattice (the run's light behind the title: a fifth)
    demo: float = 0.2          # the attract run
    ring: float = -1.0         # a ring in the lattice, its radius from (CX, ring_y); < 0: none
    ring_y: float = RULE
    rule_len: float = 1.0      # how much of the rule is drawn, from the centre outward
    rule_y: float = RULE
    rule_lv: float = 1.0
    wave: float = 1.0          # the rule's ends as the music
    letters: tuple = (1.0,) * 8    # each letter's light; 2 = the letter being struck
    squash: float = 0.0        # the word falling into the rule
    heat: float = 0.0          # the drop-in's wait: the tubes run hot
    lasers: float = 1.0        # the fan's opening
    laser_lv: float = 1.0
    refl: float = 1.0
    tag: float = 1.0
    motes: float = 1.0
    play: float = 1.0
    play_pressed: bool = False
    play_scale: float = 1.0
    play_y: float = PLAY_Y
    play_rings: tuple = ((16.0, 0.45), (40.0, 0.14))   # (how far out, how bright): the beat leaving PLAY
    count: int = -1            # dots lit in the wait's count; -1: the beat dots, beat 1 lit
    side: float = 1.0
    sink: float = 0.0
    foot: float = 1.0
    flash: float = 0.0
    burst: float = -1.0        # the downbeat's burst, 0..1 of its beat; < 0: none
    ship: float = 0.0          # the player's ship at the centre of the frame
    card: float = 0.0          # the Track name card
    caption: str = ""


def smooth(t):
    t = min(1.0, max(0.0, t))
    return t * t * (3.0 - 2.0 * t)


def pill_pts(cx, cy, w, h):
    r = h / 2.0
    pts = []
    for k in range(17):
        a = PI / 2 + PI * k / 16
        pts.append((cx - w / 2 + r + math.cos(a) * r, cy - math.sin(a) * r))
    for k in range(17):
        a = -PI / 2 + PI * k / 16
        pts.append((cx + w / 2 - r + math.cos(a) * r, cy - math.sin(a) * r))
    return pts


class Title(sc.Screen):
    def __init__(self, st):
        super().__init__("ultraviolet", 3)
        self.st = st
        self.cyan = self.p1
        self.violet = self.hue[0]
        self.deep = self.hue[1]
        self.magenta = self.hue[2]
        self.grid = self.lin(self.pal["grid"])
        self.grid_hot = self.lin(self.pal["gridHot"])
        self.cv.bg *= st.bg
        self.field_cols = np.asarray([self.cyan, self.white, self.violet, self.magenta], np.float32)

    # ---- colour
    def field(self, u):
        """The colour field across the screen or the word: P1 cyan -> white -> the palette's first hue -> its third."""
        t = np.clip(u, 0.0, 1.0) * 3.0
        i = np.minimum(t.astype(np.int32), 2)
        f = (t - i)[..., None]
        return self.field_cols[i] * (1.0 - f) + self.field_cols[i + 1] * f

    def chrome(self, top, cap):
        """The tube's colour by height in a letter: above the horizon the field with a little white at the top,
        below it the field turned toward the palette's third hue, deepening to the foot."""
        def f(xx, yy):
            col = self.field((xx - X0) / LW)
            y = np.clip((yy - top) / max(cap, 1e-3), 0.0, 1.0)
            a = (0.62 + 0.38 * np.clip(y / HORIZON, 0.0, 1.0))[..., None]
            upper = self.white * (1.0 - a) + col * a
            g = np.clip((y - HORIZON) / (1.0 - HORIZON), 0.0, 1.0)[..., None]
            lower = (col * 0.45 + self.magenta * 0.55) * (1.0 - 0.45 * g) + self.deep * (0.30 * g)
            return np.where((y < HORIZON)[..., None], upper * 0.95, lower * 1.1)
        return f

    # ---- the wordmark
    def letter(self, j):
        unit = CAP / 6.0
        lx = X0 + j * ADV * unit
        segs = []
        for pts in self.font["RAVEFORM"[j]]:
            px = [(lx + p[0] * unit, TOP + (6 - p[1]) * unit) for p in pts]
            segs += [(px[i][0], px[i][1], px[i + 1][0], px[i + 1][1]) for i in range(len(px) - 1)]
        return segs

    def fall(self, segs):
        """The word falls into the rule: every point toward the rule's line."""
        q = 1.0 - self.st.squash
        ry = self.st.rule_y
        return [(a, ry + (b - ry) * q, c, ry + (d - ry) * q) for a, b, c, d in segs]

    def logo(self):
        st, cv = self.st, self.cv
        q = 1.0 - st.squash
        top = st.rule_y + (TOP - st.rule_y) * q
        cap = CAP * q
        lit = [v for v in st.letters if v > 0]
        if not lit:
            return
        cv.glow(CX, top + cap * 0.5, 0.52 * LW, self.violet, 0.035 * min(1.0, sum(lit) / 8.0) * (1.0 + st.heat))
        for j in range(8):
            v = st.letters[j]
            if v <= 0:
                continue
            strike = v > 1.5
            lv = 1.0 if strike else v
            segs = self.fall(self.letter(j))
            # the extrusion: layers toward the rule's centre, each a little smaller and dimmer
            if q > 0.2:
                for d in range(DEPTH, 0, -1):
                    k = 1.0 - DEPTH_STEP * d
                    back = [(CX + (a - CX) * k, st.rule_y + (b - st.rule_y) * k, CX + (c - CX) * k, st.rule_y + (e - st.rule_y) * k) for a, b, c, e in segs]
                    cv.lines(back, self.deep * 0.6 + self.magenta * 0.4, 0.26 * lv * q * (1.0 - d / (DEPTH + 1.0)), TUBE)
            tube = TUBE * (1.0 + 0.25 * st.heat)
            cv.lines(segs, self.chrome(top, cap), (2.5 + 1.0 * st.heat) * lv, tube)
            cv.lines(segs, self.white, (2.5 + 2.0 * st.heat) * lv, CORE * (1.0 + 0.8 * st.heat))
            if strike:
                # a letter being struck: white-hot, and the rule leaps under it (the note that lights it)
                cv.lines(segs, self.white, 3.5, TUBE * 1.2)
                lx = X0 + (j * ADV + 2.0) * CAP / 6.0
                cv.glow(lx, TOP + CAP * 0.5, CAP * 0.9, self.white, 0.30)
                cv.lines([(lx - 46, st.rule_y, lx - 14, st.rule_y - 34), (lx - 14, st.rule_y - 34, lx, st.rule_y + 12),
                          (lx, st.rule_y + 12, lx + 14, st.rule_y - 34), (lx + 14, st.rule_y - 34, lx + 46, st.rule_y)], self.white, 3.0, 2.6)
        if min(st.letters) >= 1.0 and st.squash <= 0.0:
            # the glints: the R's top corner (the kick) and the M's right foot (the backbeat)
            cv.flare(X0 + 4, TOP, 96, self.white, 1.7, 1.4, 4, 0.2)
            cv.flare(X0 + LW - 2, BASE, 70, self.white, 1.2, 1.3, 4, 0.2)

    def reflection(self):
        st, cv = self.st, self.cv
        if st.refl <= 0:
            return
        q = 1.0 - st.squash
        ry = st.rule_y
        y_top = ry + (ry - BASE) * REFL_SQUASH * q
        y_bot = ry + (ry - TOP) * REFL_SQUASH * q
        segs = []
        for j in range(8):
            if st.letters[j] <= 0:
                continue
            for a, b, c, d in self.letter(j):
                for k in range(5):                                  # cut up so the ripple can bend it
                    u0, u1 = k / 5.0, (k + 1) / 5.0
                    p = []
                    for u in (u0, u1):
                        x, y = a + (c - a) * u, b + (d - b) * u
                        yr = ry + (ry - y) * REFL_SQUASH * q
                        depth = (yr - y_top) / max(1.0, y_bot - y_top)
                        p += [x + 9.0 * depth * math.sin(yr * 0.19 + 1.3), yr]
                    segs.append(tuple(p))

        def f(xx, yy):
            col = self.field((xx - X0) / LW) * 0.6 + self.deep * 0.4
            k = np.clip(1.0 - (yy - y_top) / max(1.0, y_bot - y_top), 0.0, 1.0) ** 2.0
            return col * k[..., None]
        cv.lines(segs, f, 0.44 * st.refl * q, TUBE * 0.8)

    # ---- the rule: a laser line under the word whose ends are the music
    def rule(self):
        st, cv = self.st, self.cv
        if st.rule_len <= 0 or st.rule_lv <= 0:
            return
        half = st.rule_len * W / 2.0
        tag_w = (len(TAG) * 6 - 2) * TAG_CAP / 6.0
        gap = (tag_w / 2.0 + 44.0) * smooth(st.tag * 1.5) if st.tag > 0 else 0.0
        runs, run, echo, erun = [], [], [], []
        for x in range(0, W + 1, 6):
            dx = abs(x - CX)
            if dx > half or dx < gap:
                if len(run) > 1:
                    runs.append(run)
                    echo.append(erun)
                run, erun = [], []
                continue
            env = smooth((dx - 0.52 * LW) / 300.0) * st.wave
            u = x / W
            v = 0.62 * math.sin(u * 83.0) * math.sin(u * 9.1 + 1.0) + 0.30 * math.sin(u * 211.0 + 0.6) + 0.16 * math.sin(u * 467.0)
            run.append((x, st.rule_y - env * 66.0 * v))
            erun.append((x, st.rule_y + env * 66.0 * v * 0.5))
        if len(run) > 1:
            runs.append(run)
            echo.append(erun)
        fld = lambda xx, yy: self.field(xx / W)
        for r, e in zip(runs, echo):
            cv.lines(ld.polyline(r), fld, 0.16 * st.rule_lv, 10.0)       # its glow
            cv.lines(ld.polyline(r), fld, 2.1 * st.rule_lv, 2.4)
            if st.wave > 0:
                cv.lines(ld.polyline(e), self.deep, 0.42 * st.rule_lv * st.wave, 1.3)
        # the straight part under the word carries a white core and a wide flare
        core = min(half, 0.53 * LW)
        for sgn in (-1.0, 1.0):
            a, b = CX + sgn * gap, CX + sgn * core
            if abs(b - CX) > abs(a - CX):
                cv.lines([(a, st.rule_y, b, st.rule_y)], self.white, 1.6 * st.rule_lv, 1.2)
        reach = min(half, 1150.0)
        for sgn in (-1.0, 1.0):                                      # the flare: two arms from the tagline's gap, fading outward
            if reach > gap + 4:
                arm = [(CX + sgn * gap, st.rule_y, CX + sgn * reach, st.rule_y)]
                cv.lines(arm, ld.f_fade(ld.f_solid(self.white), CX, st.rule_y, reach, 1.6), 0.75 * st.rule_lv, 5.0)
        if gap <= 0:
            cv.glow(CX, st.rule_y, 30.0 + 110.0 * st.rule_len, self.white * 0.5 + self.violet * 0.5, 0.06 * st.rule_lv)

    def tagline(self):
        st = self.st
        if st.tag <= 0:
            return
        self.centred(TAG, st.rule_y - TAG_CAP / 2.0, TAG_CAP, self.accent * 0.55 + self.white * 0.45, 2.1 * st.tag, 2.4)

    def lasers(self):
        st, cv = self.st, self.cv
        if st.lasers <= 0 or st.laser_lv <= 0:
            return
        for i, deg in enumerate(LASERS):
            a = math.radians(deg * st.lasers)
            col = self.field(np.asarray(i / (len(LASERS) - 1.0)))
            lv = st.laser_lv * (0.62 if i % 2 else 1.0)              # (every other beam is lit by the beat)
            for sgn, length, k in ((-1.0, 1040.0, 1.0), (1.0, 430.0, 0.3)):   # the beam, and its reflection under the rule
                sq = 1.0 if sgn < 0 else REFL_SQUASH
                bx, by = CX + math.sin(a) * 40.0, st.rule_y + sgn * math.cos(a) * 40.0 * sq      # (clear of the tagline)
                ex, ey = CX + math.sin(a) * length, st.rule_y + sgn * math.cos(a) * length * sq
                fade = ld.f_fade(ld.f_solid(col), CX, st.rule_y, length, 1.25)
                cv.lines([(bx, by, ex, ey)], fade, 0.028 * lv * k, 18.0)
                cv.lines([(bx, by, ex, ey)], fade, 0.40 * lv * k, 2.0)

    def motes(self):
        st, cv = self.st, self.cv
        if st.motes <= 0:
            return
        rng = np.random.default_rng(11)
        for _ in range(110):
            x = rng.uniform(0, W)
            h = rng.uniform(0.0, 1.0) ** 1.7
            col = self.field(np.asarray(x / W))
            cv.glow(x + 30.0 * h * math.sin(x), st.rule_y - 20.0 - 300.0 * h, rng.choice([1.8, 2.4, 3.2]), col, 1.5 * (1.0 - h) * st.motes)

    # ---- behind: the lattice and the attract run
    def lattice(self):
        st, cv = self.st, self.cv
        if st.lattice <= 0 and st.ring < 0:
            return
        cell = 40.0
        segs = [(x, 0, x, H) for x in np.arange(cell / 2, W, cell)] + [(0, y, W, y) for y in np.arange(cell / 2 - 10, H, cell)]
        if st.lattice > 0:
            cv.lines(segs, self.grid, 0.55 * st.lattice, 1.1)
        if st.ring >= 0:
            # a ring passing through the lattice lights it where it is
            r = st.ring

            def f(xx, yy):
                d = np.hypot(xx - CX, yy - st.ring_y)
                k = np.exp(-((d - r) / 70.0) ** 2) + 0.16 * np.clip(1.0 - d / max(r, 1.0), 0.0, 1.0)
                return (self.grid_hot * 0.7 + self.cyan * 0.3) * k[..., None]
            cv.lines(segs, f, 1.1, 1.3)
            pts = [(CX + r * math.cos(a), st.ring_y + r * math.sin(a)) for a in np.linspace(0, 2 * PI, 97)]
            cv.lines(ld.polyline(pts), self.cyan * 0.6 + self.white * 0.4, 1.3, 2.4)

    def demo(self):
        st = self.st
        if st.demo <= 0:
            return
        for _ in range(46):
            k = self.rng.randrange(3)
            gid = self.rng.choice(["drifter_2", "hound_2", "shoal_1", "shoal_2", "carom_2", "blink_2", "moth_2"])
            self.enemy(gid, self.rng.uniform(60, W - 60), self.rng.uniform(60, H - 60), 21 * (0.55 if "shoal" in gid else 1.0), self.rng.uniform(0, 6.28), k, st.demo)
        if st.ship <= 0:
            self.ship(CX, H / 2.0, 20, -0.5, lit=False)
            for k in range(5):
                x, y = CX + 60 + k * 74, H / 2.0 - 30 - k * 40
                self.cv.lines([(x, y, x + 26, y - 14)], self.cyan, 4.0 * st.demo, 3.0)

    # ---- the controls
    def play(self):
        st, cv = self.st, self.cv
        if st.play <= 0 or st.play_scale <= 0.02:
            return
        lv, k, cy = st.play, st.play_scale, st.play_y
        w, h = PLAY_W * k, PLAY_H * k
        pts = pill_pts(CX, cy, w, h)
        cv.fill(pts, self.cyan, (0.45 if st.play_pressed else 0.08) * lv)
        cv.glow(CX, cy, w * 0.55, self.cyan, (0.10 if st.play_pressed else 0.035) * lv)
        cv.lines(ld.polyline(pts, closed=True), self.cyan * 0.8 + self.white * 0.2, (2.6 if st.play_pressed else 1.8) * lv, (4.0 if st.play_pressed else 3.0) * max(k, 0.5))
        for out, level in st.play_rings:
            cv.lines(ld.polyline(pill_pts(CX, cy, w + 2 * out, h + 2 * out), closed=True), self.cyan, level * lv * 1.6, 2.0)
        if k > 0.6:
            cap = 60.0 * k
            self.centred("PLAY", cy - cap / 2.0 - 11.0 * k, cap, self.cyan, (1.5 if st.play_pressed else 1.1) * lv, 9.0 * k, CX)   # a tube: the colour
            self.centred("PLAY", cy - cap / 2.0 - 11.0 * k, cap, self.white, (2.4 if st.play_pressed else 1.9) * lv, 3.2 * k, CX)  # and its white core
            for b in range(4):
                on = (b < st.count) if st.count >= 0 else (b == 0)
                col = self.white if st.count >= 0 else (self.accent if b == 0 else self.white)
                cv.discs([(CX + (b - 1.5) * 32.0 * k, cy + 47.0 * k, 5.0 * k)], col, (1.9 if on else 0.32) * lv)

    def sides(self):
        st = self.st
        if st.side <= 0:
            return
        y = PLAY_Y + st.sink
        for sgn, label, sub in ((-1.0, "CO-OP", "2-4 PLAYERS   ONE WI-FI"), (1.0, "SETTINGS", "SOUND   CONTROLS   LOOK")):
            x = CX + sgn * SIDE_DX
            pts = pill_pts(x, y, SIDE_W, SIDE_H)
            self.cv.lines(ld.polyline(pts, closed=True), self.white, 0.9 * st.side, 2.0)
            self.centred(label, y - 15.0, 30.0, self.white, 1.15 * st.side, None, x)
            self.centred(sub, y + SIDE_H / 2.0 + 28.0, 12.0, self.white, 0.5 * st.side, 1.3, x)

    def footer(self):
        st, cv = self.st, self.cv
        if st.foot <= 0:
            return
        y = H - 60 + st.sink
        cv.text(self.font, "BEST 1,204,880   TRACK 05   X412", M, y, 14, self.gold, 1.0 * st.foot, 1.6)
        t = "HEADPHONES RECOMMENDED"
        cv.text(self.font, t, W - M - len(t) * 14, y - 14, 14, self.white, 0.5 * st.foot, 1.4)
        t = "V0.9.0   1699D25   2026-11-14 18:33"
        cv.text(self.font, t, W - M - len(t) * 11, y + 12, 11, self.white, 0.4 * st.foot, 1.2)

    # ---- the drop's far side
    def burst(self):
        st, cv = self.st, self.cv
        if st.burst < 0:
            return
        u = st.burst
        c = (CX, H / 2.0)
        cv.glow(c[0], c[1], 260.0 + 500.0 * u, self.white * 0.6 + self.cyan * 0.4, 0.5 * (1.0 - u) ** 2)
        for j in range(8):                                           # eight rays: one a letter, in its colour
            a = -PI / 2 + (j - 3.5) * (2 * PI / 8.0)
            col = self.field(np.asarray(j / 7.0))
            r0, r1 = 60.0 + 500.0 * u, 260.0 + 1100.0 * u
            seg = (c[0] + math.cos(a) * r0, c[1] + math.sin(a) * r0, c[0] + math.cos(a) * r1, c[1] + math.sin(a) * r1)
            cv.lines([seg], ld.f_fade(ld.f_solid(col), seg[0], seg[1], r1 - r0, 1.3), 2.6 * (1.0 - u), 5.0)
            cv.lines([seg], ld.f_fade(ld.f_solid(self.white), seg[0], seg[1], r1 - r0, 1.6), 2.2 * (1.0 - u), 1.6)

    def player(self):
        st = self.st
        if st.ship <= 0:
            return
        self.ship(CX, H / 2.0, 24, -PI / 2, cells=4 if st.ship >= 1.0 else 0, lit=True)

    def card(self):
        st = self.st
        if st.card <= 0:
            return
        self.centred("TRACK 1", 250, 26, self.accent, 1.4 * st.card, 2.4)
        x0 = CX - (11 * 6 - 2) * 92 / 6.0 / 2
        segs, end = ld.text_segments(self.font, "FIRST LIGHT", x0, 300, 92)
        self.cv.lines(segs, ld.f_linear(x0, 0, end, 0, [self.cyan, self.white, self.violet, self.magenta]), 2.2 * st.card, 6.0)
        self.cv.lines(segs, self.white, 2.2 * st.card, 1.7)

    def caption(self):
        if self.st.caption:
            self.cv.text(self.font, self.st.caption, 64, 44, 30, self.white, 1.1, 3.0)

    def draw(self):
        st, cv = self.st, self.cv
        if st.bg > 0:
            cv.nebula([(500, 300, 700, self.hue[0], 0.03 * st.bg), (1900, 800, 700, self.hue[2], 0.028 * st.bg), (1250, 500, 800, self.hue[1], 0.022 * st.bg)])
            n = int(360 * st.bg)
            if n:
                self.stars(n)
        self.lattice()
        self.demo()
        self.lasers()
        self.reflection()
        self.motes()
        self.rule()
        self.logo()
        self.tagline()
        self.play()
        self.sides()
        self.footer()
        self.burst()
        self.player()
        self.card()
        if st.flash > 0:
            cv.glow(CX, H / 2.0 if st.burst >= 0 else st.rule_y, 1000, self.white * 0.7 + self.violet * 0.3, 0.30 * st.flash)   # (clamped: a pool, never the whole frame)
        self.caption()
        return cv.render(bloom=0.95)


REST = St()
DARK = St(bg=0.0, lattice=0.0, demo=0.0, rule_len=0.0, wave=0.0, letters=(0.0,) * 8, lasers=0.0, refl=0.0, tag=0.0, motes=0.0,
          play=0.0, side=0.0, foot=0.0)

INTRO = [
    replace(DARK, rule_len=0.006, rule_lv=1.6, caption="1  BAR 1 BEAT 1   THE KICK IS A POINT OF LIGHT"),
    replace(DARK, rule_len=0.30, rule_lv=1.2, caption="2  BAR 1 BEAT 3   EVERY KICK THROWS THE LINE WIDER"),
    replace(DARK, bg=0.6, rule_len=1.0, wave=0.7, ring=560.0, motes=0.4, caption="3  BAR 1 BEAT 4   THE LINE REACHES THE EDGES AND WAKES AS THE MUSIC"),
    replace(DARK, bg=1.0, lattice=0.2, rule_len=1.0, wave=1.0, letters=(1, 1, 1, 2, 0, 0, 0, 0), lasers=1.0, laser_lv=1.3, motes=1.0, flash=0.16, refl=1.0,
            caption="4  BAR 2   THE DROP: THE WORD IS PLAYED, A LETTER A NOTE"),
    replace(DARK, bg=1.0, lattice=0.2, rule_len=1.0, wave=1.0, letters=(1.0,) * 8, lasers=1.0, motes=1.0, refl=1.0,
            caption="5  BAR 2 BEAT 4   EIGHT NOTES, EIGHT LETTERS"),
    replace(REST, demo=0.0, side=0.0, foot=0.0, play=0.6, play_y=PLAY_Y + 14.0, play_rings=(), tag=1.0,
            caption="6  BAR 3 BEAT 3   THE TAGLINE OPENS THE RULE, PLAY RISES AND TAKES A TAP"),
]

OUTRO = [
    replace(REST, play_pressed=True, count=4, play_rings=((26.0, 0.8), (64.0, 0.3)), caption="1  THE TAP   PLAY LIGHTS AND COUNTS THE BEATS TO THE BAR LINE"),
    replace(REST, play_pressed=True, count=2, play_rings=(), side=0.0, foot=0.0, demo=0.0, lattice=0.12, lasers=0.45, heat=0.6, motes=0.5,
            caption="2  THE WAIT   THE FAN CLOSES, THE TUBES RUN HOT, THE REST LEAVES"),
    replace(REST, play_pressed=True, count=1, play_rings=(), side=0.0, foot=0.0, demo=0.0, lattice=0.06, lasers=0.0, heat=1.0, motes=0.0,
            tag=0.0, wave=0.25, squash=0.62, rule_lv=1.6, caption="3  THE LAST BEAT   THE WORD FALLS INTO ITS LINE"),
    replace(REST, play_pressed=True, count=0, play_rings=(), side=0.0, foot=0.0, demo=0.0, lattice=0.03, lasers=0.0, motes=0.0, refl=0.0,
            tag=0.0, wave=0.0, letters=(0.0,) * 8, rule_len=0.26, rule_y=RULE + 70.0, rule_lv=2.2, play_scale=0.42, play_y=PLAY_Y - 150.0,
            caption="4  ITS LAST HALF   THE LINE AND PLAY GATHER INTO ONE POINT"),
    replace(DARK, bg=1.0, lattice=0.1, ring=420.0, ring_y=H / 2.0, burst=0.22, ship=0.5, flash=0.22,
            caption="5  THE DOWNBEAT   THE POINT BURSTS: EIGHT RAYS, THE LATTICE LIGHTS"),
    replace(DARK, bg=1.0, lattice=0.5, ring=1500.0, ring_y=H / 2.0, ship=1.0, card=1.0,
            caption="6  BEAT 2   THE LIGHT IS THE SHIP. TRACK 1 BEGINS"),
]


def sheet(frames, name):
    out = Image.new("RGB", (W, H * 3 // 2), (0, 0, 0))
    for i, st in enumerate(frames):
        img = Title(st).draw()
        out.paste(img.resize((W // 2, H // 2), Image.LANCZOS), ((i % 2) * (W // 2), (i // 2) * (H // 2)))
        print("  frame", i + 1)
    path = os.path.join(sc.DESIGN, "previews", name)
    out.save(path, optimize=True)
    print("wrote", path)


def layout(img):
    """The rest frame, dimmed, with its measures (reference pixels at 2400x1080)."""
    im = Image.eval(img, lambda v: int(v * 0.55))
    d = ImageDraw.Draw(im)
    try:
        font = ImageFont.load_default(size=22)
    except TypeError:
        font = ImageFont.load_default()
    line, text = (255, 214, 64), (255, 236, 150)

    def hline(y, label, x0=40, x1=W - 40):
        d.line([(x0, y), (x1, y)], fill=line, width=1)
        d.text((x0 + 6, y - 26), label, fill=text, font=font)

    def box(x0, y0, x1, y1, label, above=False):
        d.rectangle([x0, y0, x1, y1], outline=line, width=1)
        d.text((x0 + 6, y0 - 30 if above else y1 + 4), label, fill=text, font=font)

    hline(TOP, "y 138  caps' top (cap height 240, tube 14, core 4)")
    hline(BASE, "y 378  baseline")
    hline(RULE, "y 432  the rule", 40, 330)
    d.text((W - 800, RULE + 8), "tagline cap 22 set into the rule; fan origin (1200, 432)", fill=text, font=font)
    hline(RULE + (RULE - TOP) * REFL_SQUASH, "y 594  the reflection's foot (0.55 of the word's height, fading to nothing)")
    d.line([(X0, TOP - 40), (X0, BASE + 20)], fill=line, width=1)
    d.line([(X0 + LW, TOP - 40), (X0 + LW, BASE + 20)], fill=line, width=1)
    d.text((X0 + 8, TOP - 66), "x 385 .. 2015  the word: 1630 wide, 5.25 font units a letter", fill=text, font=font)
    box(CX - PLAY_W / 2, PLAY_Y - PLAY_H / 2, CX + PLAY_W / 2, PLAY_Y + PLAY_H / 2, "PLAY 640 x 150 at (1200, 800), label 60, beat dots 47 under centre", True)
    for sgn, t in ((-1, "CO-OP 340 x 104 at (640, 800), label 30, caption cap 12"), (1, "SETTINGS 340 x 104 at (1760, 800)")):
        x = CX + sgn * SIDE_DX
        box(x - SIDE_W / 2, PLAY_Y - SIDE_H / 2, x + SIDE_W / 2, PLAY_Y + SIDE_H / 2 + 44, t)
    hline(940, "y 940  notes (THE HOST LEFT, co-op messages), cap 18", 700, W - 700)
    hline(H - 46, "y 1020 / 1034  best (left, gold, cap 14); note and version (right)", 40, W - 40)
    d.line([(M, 0), (M, H)], fill=(120, 100, 40), width=1)
    d.line([(W - M, 0), (W - M, H)], fill=(120, 100, 40), width=1)
    d.text((M + 8, 12), "132 px side margins", fill=text, font=font)
    path = os.path.join(sc.DESIGN, "previews", "title_v2_layout.png")
    im.save(path, optimize=True)
    print("wrote", path)


def main():
    what = sys.argv[1:] or ["hero", "intro", "outro", "layout"]
    os.makedirs(os.path.join(sc.DESIGN, "previews"), exist_ok=True)
    hero = None
    if "hero" in what or "layout" in what:
        hero = Title(REST).draw()
    if "hero" in what:
        path = os.path.join(sc.DESIGN, "previews", "title_v2.png")
        hero.save(path, optimize=True)
        print("wrote", path)
    if "layout" in what:
        layout(hero)
    if "intro" in what:
        sheet(INTRO, "title_v2_intro.png")
    if "outro" in what:
        sheet(OUTRO, "title_v2_outro.png")


if __name__ == "__main__":
    main()
