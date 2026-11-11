"""RAVEFORM screens outside the run: title, game over, pause, co-op lobby (layout reference for M3 / M4).

Each screen is drawn at 2400x1080 with the game's own light (stroke font, capsule lines, pools, glass),
then the four are tiled at half size. Layout numbers are in design/screens.md; this draws them.

Usage:  python design/tools/screens.py   -> design/previews/screens.png (+ screen_<name>.png at full size)
"""
import json
import math
import os
import random
import sys

import numpy as np
from PIL import Image

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import lookdev as ld  # noqa: E402
import icons as ic  # noqa: E402

ROOT = os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__))))
DESIGN = os.path.join(ROOT, "design")
W, H = 2400, 1080
PI = math.pi
M = 132   # side margin kept clear of the camera cut-out


class Screen:
    def __init__(self, palette_id, seed=1):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == palette_id)
        self.glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
        self.font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
        self.sw = ic.swatches()
        lin = lambda e: np.asarray(e["linear"], np.float32)
        self.lin = lin
        cands = [self.pal["enemyCandidates"][i] for i in self.pal["soloEnemies"]]
        self.hue = [lin(c) for c in cands]
        self.mate = [lin(c["partners"][c["soloPartner"]]) for c in cands]
        self.white = np.ones(3, np.float32)
        self.accent = lin(self.pal["accent"])
        self.gold = lin(self.pals["constants"]["pickup"])
        self.danger = lin(self.pals["constants"]["danger"])
        self.players = [(lin(p), lin(p["deep"]), lin(p["light"])) for p in self.pals["constants"]["players"]]
        self.p1 = self.players[0][0]
        self.rng = random.Random(seed)
        self.cv = ld.Canvas(W, H)
        self.cv.radial_bg(lin(self.pal["bg0"]) * 0.5, lin(self.pal["bg1"]) * 0.5, cx=0.5, cy=0.5)

    # ---- pieces
    def stars(self, n=300):
        for _ in range(n):
            c = self.rng.choice([self.white, self.white, self.white, self.hue[0], self.hue[2], self.mate[1]])
            self.cv.glow(self.rng.uniform(0, W), self.rng.uniform(0, H), self.rng.choice([1.6, 1.6, 2.2, 3.0]), c, self.rng.uniform(0.15, 0.7))

    def centred(self, text, y, cap, colour, intensity=1.2, width=None, cx=W / 2):
        x = cx - (len(text) * 6 - 2) * cap / 6.0 / 2
        segs, end = ld.text_segments(self.font, text, x, y, cap)
        self.cv.lines(segs, colour, intensity, width if width else max(1.2, 0.55 * cap / 6.0))
        return x, end

    def pill(self, cx, cy, w, h, label, colour, cap, pressed=False, dim=False):
        """A control: idle = outline and label; pressed = glass fill, thicker brighter outline, brighter label."""
        r = h / 2
        pts = []
        for k in range(13):
            a = PI / 2 + PI * k / 12
            pts.append((cx - w / 2 + r + math.cos(a) * r, cy - math.sin(a) * r))
        for k in range(13):
            a = -PI / 2 + PI * k / 12
            pts.append((cx + w / 2 - r + math.cos(a) * r, cy - math.sin(a) * r))
        if pressed:
            self.cv.fill(pts, colour, 0.45)
            self.cv.glow(cx, cy, w * 0.6, colour, 0.12)
        self.cv.lines(ld.polyline(pts, closed=True), colour, 2.0 if pressed else (0.3 if dim else 0.9), 3.0 if pressed else 2.0)
        self.centred(label, cy - cap / 2, cap, self.white, 1.8 if pressed else (0.4 if dim else 1.1), None, cx)

    def ship(self, x, y, r, rot, player=0, cells=3, lit=True):
        colour, deep, light = self.players[player]
        g = 1.0 if lit else 0.35
        hull = [(s[0], -s[1], s[2], -s[3]) for s in self.glyphs["ship"]["segments"]]
        px = ld.transform(hull, x, y, r, rot)
        if lit:
            self.cv.glow(x, y, r * 6, colour, 0.13)
        self.cv.lines(px, ld.f_radial(x, y, r * 1.3, [self.white, colour, deep]), 2.8 * g, max(1.6, r * 0.1))
        self.cv.lines(px, self.white, 2.0 * g, max(0.8, r * 0.035))
        if cells:
            halo = [(s[0], -s[1], s[2], -s[3]) for s in self.glyphs["halo_%d" % cells]["segments"]]
            self.cv.lines(ld.transform(halo, x, y, r, 0.35), ld.f_angular(x, y, [colour, self.white, colour, light]), 1.5 * g, max(1.4, r * 0.07))

    def enemy(self, gid, x, y, r, rot, k, gain=1.0):
        g = self.glyphs[gid]
        hull = ld.transform([(s[0], -s[1], s[2], -s[3]) for s in g["segments"] if s[4] == 0], x, y, r, rot)
        ext = r * g["extent"]
        self.cv.lines(hull, ld.f_radial(x, y, ext, [self.mate[k], self.hue[k], self.hue[k] * 0.75 + self.white * 0.25]), 2.0 * gain, 1.8)

    def save(self, name):
        img = self.cv.render(bloom=0.95)
        img.save(os.path.join(DESIGN, "previews", "screen_%s.png" % name), optimize=True)
        return img


def title():
    s = Screen("ultraviolet", 3)
    cv = s.cv
    cv.nebula([(500, 300, 700, s.hue[0], 0.03), (1900, 800, 700, s.hue[2], 0.028), (1250, 500, 800, s.hue[1], 0.022)])
    s.stars(360)
    # the attract run plays behind everything at a third of its light
    for _ in range(46):
        k = s.rng.randrange(3)
        gid = s.rng.choice(["drifter_2", "hound_2", "shoal_1", "shoal_2", "carom_2", "blink_2", "moth_2"])
        s.enemy(gid, s.rng.uniform(60, W - 60), s.rng.uniform(60, H - 60), 21 * (0.55 if "shoal" in gid else 1.0), s.rng.uniform(0, 6.28), k, 0.3)
    # the music as a line of light across the screen: the logo stands on its own waveform
    pts = []
    for i in range(0, W + 1, 6):
        u = i / W
        env = 0.25 + 0.75 * math.exp(-((u - 0.5) / 0.26) ** 2)
        y = 520 + env * (58 * math.sin(u * 61.0) * math.sin(u * 7.3 + 1.0) + 26 * math.sin(u * 173.0 + 0.6))
        pts.append((i, y))
    wave = ld.polyline(pts)
    cv.lines(wave, ld.f_linear(0, 0, W, 0, [s.p1, s.white, s.hue[0], s.sw["magenta"]]), 1.7, 2.0)
    cv.lines([(a[0], 520 - (a[1] - 520) * 0.45, a[2], 520 - (a[3] - 520) * 0.45) for a in wave], s.hue[1], 0.45, 1.2)   # its echo, mirrored
    # logo: thick tubes with a white core, colour running across the word
    cap = 190.0
    x0 = W / 2 - (8 * 6 - 2) * cap / 6 / 2
    segs, end = ld.text_segments(s.font, "RAVEFORM", x0, 216, cap)
    field = ld.f_linear(x0, 0, end, 0, [s.p1, s.white, s.hue[0], s.sw["magenta"]])
    cv.glow(W / 2, 310, 760, s.hue[0], 0.10)
    cv.lines(segs, field, 2.6, 11.0)
    cv.lines(segs, s.white, 2.4, 3.2)
    for fx, fy in ((x0 + 6, 216), (end - 70, 216 + cap)):
        cv.flare(fx, fy, 130, s.white, 1.8, 1.5, 4, 0.2)
    s.centred("SOUND  BECOMES  LIGHT", 446, 22, s.accent, 1.3, 2.0)
    for k in range(4):                                         # the beat, under the tagline
        cv.discs([(W / 2 - 54 + k * 36, 498, 6)], s.accent if k == 0 else s.white, 1.6 if k == 0 else 0.35)
    # controls: PLAY is the largest thing a thumb can find; it is shown pressed
    s.pill(W / 2, 760, 600, 140, "PLAY", s.p1, 52, pressed=True)
    s.pill(W / 2 - 520, 760, 330, 104, "CO-OP", s.white, 30)
    s.pill(W / 2 + 520, 760, 330, 104, "SETTINGS", s.white, 30)
    s.ship(W / 2, 930, 26, -PI / 2)
    cv.text(s.font, "BEST 1,204,880   TRACK 05   X412", M, H - 60, 14, s.gold, 1.0, 1.6)
    t = "HEADPHONES RECOMMENDED"
    cv.text(s.font, t, W - M - len(t) * 14, H - 60, 14, s.white, 0.5, 1.4)
    return s.save("title")


def game_over():
    s = Screen("furnace", 5)
    cv = s.cv
    cv.nebula([(600, 300, 700, s.hue[0], 0.03), (1800, 760, 700, s.hue[2], 0.026)])
    s.stars(240)
    s.centred("RUN OVER", 56, 44, s.accent, 1.3, 3.2)
    # the score is the headline; a new best wears a flare
    cap = 150.0
    x, end = s.centred("1,204,880", 150, cap, s.white, 0.01)
    segs, _ = ld.text_segments(s.font, "1,204,880", x, 150, cap)
    cv.glow(W / 2, 225, 620, s.gold, 0.10)
    cv.lines(segs, ld.f_linear(0, 150, 0, 150 + cap, [s.white, s.gold, s.sw["amber"]]), 2.4, 9.0)
    cv.lines(segs, s.white, 2.2, 2.6)
    cv.flare(end + 40, 150, 120, s.gold, 2.0, 1.5, 4, 0.3)
    cv.text(s.font, "NEW BEST", end - 250, 322, 20, s.gold, 1.8, 2.2)
    # four numbers, no more
    stats = [("TIME", "12:44"), ("TRACK", "05"), ("BEST CHAIN", "X412"), ("KILLS", "8,412")]
    for k, (name, value) in enumerate(stats):
        cx = W / 2 - 660 + k * 440
        s.centred(name, 392, 14, s.accent, 0.9, 1.6, cx)
        s.centred(value, 424, 40, s.white, 1.5, 3.0, cx)
    # the set list: the run drawn as the song it was
    bx0, bx1, by = M + 60, W - M - 60, 610
    cv.text(s.font, "SET LIST", bx0, by - 62, 14, s.accent, 0.9, 1.6)
    tracks = [("FIRST LIGHT", 3.2, "ultraviolet"), ("SOLAR WIND", 4.5, "solar"), ("REEF", 4.3, "reef"), ("AURORA", 4.1, "aurora"), ("FURNACE", 1.6, "furnace")]
    total = sum(t[1] for t in tracks)
    x = bx0
    for name, minutes, pid in tracks:
        w = (bx1 - bx0) * minutes / total
        colour = s.lin(next(p for p in s.pals["palettes"] if p["id"] == pid)["accent"])
        cv.lines([(x + 6, by, x + w - 6, by)], colour, 2.0, 7.0)
        cv.glow(x + w / 2, by, w * 0.45, colour, 0.06)
        cv.text(s.font, name, x + 8, by + 22, 11, colour, 1.0, 1.4)
        if name != "FURNACE":
            cv.lines(ld.transform(ld.star(4, 1.0, 0.4), x + w - 6, by, 13, PI / 4), s.white, 2.2, 1.6)      # boss beaten
        x += w
    for u in (0.09, 0.2, 0.3, 0.41, 0.5, 0.58, 0.69, 0.78, 0.88, 0.95):                                    # upgrades taken
        px = bx0 + (bx1 - bx0) * u
        cv.lines(ld.polyline([(px - 7, by - 22), (px, by - 34), (px + 7, by - 22)], closed=True), s.gold, 1.8, 1.4)
    for u in (0.47, 0.83, 0.985):                                                                           # hits taken
        px = bx0 + (bx1 - bx0) * u
        cv.lines([(px - 7, by - 34, px + 7, by - 20), (px - 7, by - 20, px + 7, by - 34)], s.danger, 2.2, 1.8)
    cv.text(s.font, "STAR = BOSS BEATEN   GOLD = UPGRADE   RED = HIT TAKEN", bx0, by + 62, 9, s.white, 0.6, 1.2)
    # what the run was built from
    cv.text(s.font, "YOUR BAND", bx0, 760, 14, s.accent, 0.9, 1.6)
    by_id = {i[0]: i for i in ic.ICONS}
    for k, iid in enumerate(["orbitals", "pulse_ring", "sentinel", "split", "double_time", "ricochet", "on_beat_crit", "magnet"]):
        ic.draw_layers(cv, ic.layers_of(by_id[iid]), bx0 + 60 + k * 118, 846, 36 * by_id[iid][4] * 0.9, s.sw)
    s.pill(W - M - 330, 846, 600, 140, "RETRY", s.p1, 52, pressed=False)
    cv.glow(W - M - 330, 846, 300, s.p1, 0.10)
    s.pill(W - M - 330, 990, 330, 90, "HOME", s.white, 26)
    return s.save("game_over")


def pause():
    s = Screen("reef", 7)
    cv = s.cv
    grid = [(gx, 0, gx, H) for gx in range(0, W + 1, 45)] + [(0, gy, W, gy) for gy in range(0, H + 1, 45)]
    cv.lines(grid, s.lin(s.pal["grid"]), 0.10, 1.0)
    for _ in range(60):                                        # the run, frozen and dimmed to a fifth
        k = s.rng.randrange(3)
        gid = s.rng.choice(["drifter_3", "hound_2", "shoal_2", "carom_2", "moth_3", "sentry_3"])
        s.enemy(gid, s.rng.uniform(60, W - 60), s.rng.uniform(60, H - 60), 21 * (0.55 if "shoal" in gid else 1.0), s.rng.uniform(0, 6.28), k, 0.2)
    s.ship(W / 2 - 40, 560, 21, -0.4, lit=False)
    s.centred("PAUSED", 120, 96, s.white, 0.01)
    x, end = W / 2 - (6 * 6 - 2) * 96 / 6 / 2, 0
    segs, end = ld.text_segments(s.font, "PAUSED", x, 120, 96)
    cv.lines(segs, ld.f_linear(x, 0, end, 0, [s.accent, s.white, s.accent]), 2.2, 6.4)
    cv.lines(segs, s.white, 2.0, 2.0)
    s.pill(W / 2, 400, 600, 130, "RESUME", s.p1, 48)
    s.pill(W / 2 - 500, 400, 320, 100, "RESTART", s.white, 26)
    s.pill(W / 2 + 500, 400, 320, 100, "SETTINGS", s.white, 26)
    s.pill(W / 2, 900, 320, 90, "QUIT RUN", s.white, 24, dim=True)
    # resuming is musical: the run starts again on a bar line after a one-bar count
    s.centred("RESUME COUNTS YOU IN: FOUR BEATS, THEN THE BAR STARTS", 560, 14, s.accent, 1.0, 1.6)
    for k in range(4):
        cx = W / 2 - 150 + k * 100
        cv.lines(ld.transform(ld.polygon(28), cx, 650, 26), s.white, 0.6, 1.8)
        cv.text(s.font, str(k + 1), cx - 8, 638, 20, s.white, 0.7, 2.0)
    s.centred("TRACK 03 REEF   132 BPM   SCORE 412,880   X96", 780, 14, s.gold, 0.9, 1.6)
    return s.save("pause")


def lobby():
    s = Screen("ultraviolet", 9)
    cv = s.cv
    cv.nebula([(400, 800, 700, s.hue[0], 0.03), (2000, 300, 700, s.hue[2], 0.028)])
    s.stars(260)
    s.centred("CO-OP", 56, 60, s.accent, 1.4, 4.2)
    s.centred("ROOM CODE", 176, 16, s.white, 0.8, 1.8)
    cap = 120.0
    x = W / 2 - (4 * 6 - 2) * cap / 6 / 2
    segs, end = ld.text_segments(s.font, "K7QM", x, 210, cap)
    cv.glow(W / 2, 270, 420, s.gold, 0.10)
    cv.lines(segs, ld.f_linear(x, 0, end, 0, [s.gold, s.white, s.gold]), 2.4, 8.0)
    cv.lines(segs, s.white, 2.2, 2.4)
    s.centred("SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT", 356, 13, s.white, 0.7, 1.5)
    # four seats: the band
    names = [("P1  HOST", "READY", True, 3), ("P2", "READY", True, 3), ("P3", "JOINING", False, 0), ("OPEN SEAT", "", None, 0)]
    for k, (name, state, ready, cells) in enumerate(names):
        cx = W / 2 - 720 + k * 480
        cy = 610
        colour = s.players[k][0]
        if ready is None:
            cv.lines(ld.transform(ic.dashes(1.0, 24, 0.5), cx, cy, 130), s.white, 0.3, 1.6)
            s.centred(name, cy - 8, 16, s.white, 0.4, 1.8, cx)
            continue
        cv.lines(ld.transform(ld.polygon(72), cx, cy, 130), ld.f_angular(cx, cy, [colour, s.white, colour, s.players[k][2]]), 2.0 if ready else 0.8, 2.6)
        if ready:
            cv.fill([(cx + 130 * math.cos(2 * PI * i / 48), cy + 130 * math.sin(2 * PI * i / 48)) for i in range(48)], colour, 0.07)
            cv.glow(cx, cy, 240, colour, 0.10)
        s.ship(cx, cy, 44, -PI / 2, player=k, cells=cells, lit=bool(ready))
        s.centred(name, cy + 156, 18, colour, 1.4, 2.0, cx)
        s.centred(state, cy + 190, 13, s.white if ready else s.accent, 1.0 if ready else 0.6, 1.5, cx)
    s.centred("EACH PHONE IS AN INSTRUMENT: SET THEM DOWN TOGETHER AND THE ROOM IS THE BAND", 868, 12, s.accent, 0.9, 1.4)
    s.pill(W / 2, 970, 600, 130, "START", s.p1, 48)
    s.pill(M + 170, 970, 300, 90, "LEAVE", s.white, 24)
    return s.save("lobby")


def round_rect(x0, y0, x1, y1, r, n=8):
    pts = []
    for cx, cy, a0 in ((x1 - r, y0 + r, -PI / 2), (x1 - r, y1 - r, 0.0), (x0 + r, y1 - r, PI / 2), (x0 + r, y0 + r, PI)):
        for k in range(n + 1):
            a = a0 + PI / 2 * k / n
            pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return pts


def sound_wave(kind, x0, x1, cy, amp, n=240):
    """The signature line of a sound variant: the same four beats, drawn the way each one sounds."""
    pts = []
    for i in range(n + 1):
        u = i / n
        beat = (u * 4) % 1.0
        if kind == "club":        # round and heavy: a fat kick hump on every beat, a warm body between
            y = math.exp(-beat * 5.0) * math.sin(beat * 15.0) * 1.0 + 0.22 * math.sin(u * 2 * PI * 8)
        elif kind == "arcade":    # crisp: hard steps, a short bright tick on every beat
            y = round(math.sin(u * 2 * PI * 4 + 0.3) * 3.0) / 3.0 * 0.62 + math.exp(-beat * 28.0) * 0.55
        else:                     # velvet: slow and wide, nothing sudden
            y = 0.8 * math.sin(u * 2 * PI * 2) * (0.75 + 0.25 * math.sin(u * 2 * PI * 0.5 + 1.0))
        pts.append((x0 + (x1 - x0) * u, cy - y * amp))
    return pts


def settings():
    s = Screen("aurora", 11)
    cv = s.cv
    sw = s.sw
    cv.nebula([(420, 260, 720, s.hue[0], 0.026), (2000, 820, 720, s.hue[1], 0.024)])
    s.stars(220)
    s.centred("SETTINGS", 44, 44, s.accent, 1.3, 3.2)
    # three groups, never more: SOUND is open
    s.pill(W / 2 - 420, 168, 360, 84, "SOUND", s.p1, 28, pressed=True)
    s.pill(W / 2, 168, 360, 84, "CONTROLS", s.white, 28)
    s.pill(W / 2 + 420, 168, 360, 84, "LOOK", s.white, 28)
    s.centred("THE SAME MUSIC, THREE WAYS.  TAP ONE: IT CHANGES ON THE NEXT BAR", 244, 13, s.white, 0.7, 1.5)

    # the three sounds. Each card is drawn the way it sounds; the playing one is glass and thick
    cards = [
        ("club", "CLUB", "WARM   ROUND   DEEP", sw["amber"], sw["magenta"], "playing"),
        ("arcade", "ARCADE", "CRISP   BRIGHT   DRY", sw["cyan"], sw["lime"], "queued"),
        ("velvet", "VELVET", "SOFT   WIDE   LONG", sw["violet"], sw["azure"], ""),
    ]
    cw, ch, gap, top = 620, 300, 48, 286
    left = (W - (3 * cw + 2 * gap)) / 2
    for k, (kind, name, words, a, b, state) in enumerate(cards):
        x0, x1, y0, y1 = left + k * (cw + gap), left + k * (cw + gap) + cw, top, top + ch
        cx = (x0 + x1) / 2
        frame = round_rect(x0, y0, x1, y1, 40)
        edge = ld.f_linear(x0, y0, x1, y1, [a, s.white, b])
        playing, queued = state == "playing", state == "queued"
        if playing:
            cv.fill(frame, ld.f_linear(x0, y0, x1, y1, [a, b]), 0.085)
            cv.glow(cx, (y0 + y1) / 2, 520, a, 0.10)
            cv.glow(cx + 120, y1 - 60, 380, b, 0.07)
        cv.lines(ld.polyline(frame, closed=True), edge, 2.2 if playing else (1.5 if queued else 0.8), 3.4 if playing else 2.0)
        # name as a tube in the card's two colours
        segs, end = ld.text_segments(s.font, name, x0 + 44, y0 + 36, 46)
        cv.lines(segs, ld.f_linear(x0 + 44, 0, end, 0, [a, s.white, b]), 2.3 if playing else 1.5, 4.6)
        cv.lines(segs, s.white, 2.0 if playing else 1.1, 1.3)
        cv.text(s.font, words, x0 + 46, y0 + 104, 12, s.white, 0.9 if playing else 0.6, 1.4)
        # its signature line
        wy, amp = y0 + 212, 54
        pts = sound_wave(kind, x0 + 44, x1 - 44, wy, amp)
        wave = ld.polyline(pts)
        field = ld.f_linear(x0 + 44, 0, x1 - 44, 0, [a, s.white, b, a])
        g = 1.0 if playing else (0.8 if queued else 0.6)
        if kind == "club":
            cv.lines(wave, field, 1.9 * g, 5.0)
            cv.lines(wave, s.white, 1.6 * g, 1.4)
            for beat in range(4):
                cv.glow(x0 + 44 + (cw - 88) * (beat + 0.06) / 4, wy, 70, a, 0.16 * g)
        elif kind == "arcade":
            cv.lines(wave, field, 2.4 * g, 2.2)
            for beat in range(4):
                bx = x0 + 44 + (cw - 88) * beat / 4
                cv.flare(bx + 2, wy - amp * 0.76, 34, s.white, 1.6 * g, 1.2, 2, PI / 4)
        else:
            for off, lvl, wd in ((0, 1.5, 3.4), (16, 0.55, 2.4), (32, 0.25, 2.0)):      # the echo trails behind it
                cv.lines([(p[0] + off, p[1] + off * 0.35, q[0] + off, q[1] + off * 0.35) for p, q in zip(pts[:-1], pts[1:]) if q[0] + off < x1 - 30],
                         field, lvl * g, wd)
            cv.glow(cx, wy, 300, b, 0.07 * g)
        # state, top right of the card
        if playing:
            t = "PLAYING"
            cv.text(s.font, t, x1 - 44 - len(t) * 13, y0 + 44, 13, s.white, 1.6, 1.6)
            for bar in range(4):                                # a little level meter that moves with the music
                hgt = (14, 26, 18, 30)[bar]
                bx = x1 - 44 - len(t) * 13 - 64 + bar * 12
                cv.lines([(bx, y0 + 57, bx, y0 + 57 - hgt)], ld.f_linear(0, y0 + 57, 0, y0 + 27, [a, s.white]), 2.0, 5.0)
        elif queued:
            t = "NEXT BAR"
            cv.text(s.font, t, x1 - 44 - len(t) * 13, y0 + 44, 13, a, 1.6, 1.6)
            for beat in range(4):                               # counts the bar out: three beats gone, one to go
                cv.discs([(x1 - 44 - len(t) * 13 + 8 + beat * 30, y0 + 80, 6)], a if beat < 3 else s.white, 1.8 if beat < 3 else 0.3)

    # faders that are also meters: the lit part of the track is the live level of that bus
    fy = 690
    for k, (name, value, level) in enumerate((("MASTER", 0.8, 0.62), ("MUSIC", 0.7, 0.55), ("EFFECTS", 0.9, 0.38))):
        x0 = left + k * (cw + gap) + 20
        x1 = x0 + cw - 40
        cv.text(s.font, name, x0, fy - 62, 14, s.accent, 0.9, 1.6)
        t = "%d" % round(value * 100)
        cv.text(s.font, t, x1 - len(t) * 22, fy - 70, 22, s.white, 1.3, 2.2)
        nseg = 40
        for i in range(nseg):
            u = (i + 0.5) / nseg
            bx = x0 + (x1 - x0) * u
            lit = u <= level * value / 0.8
            hot = u > 0.82
            colour = (sw["amber"] if hot else s.p1 * (1 - u) + s.accent * u) if lit else s.white
            cv.lines([(bx, fy - 13, bx, fy + 13)], colour, 1.7 if lit else (0.22 if u <= value else 0.1), 4.0)
        tx = x0 + (x1 - x0) * value
        cv.discs([(tx, fy, 24)], s.p1, 0.45)
        cv.lines(ld.transform(ld.polygon(36), tx, fy, 24), s.p1, 2.0, 2.6)
        cv.glow(tx, fy, 90, s.p1, 0.10)

    # bottom row: sync, haptics, done
    by = 900
    s.pill(left + 180, by, 360, 100, "SYNC", s.white, 28)
    s.centred("+35 MS", by + 66, 13, s.gold, 1.0, 1.5, left + 180)
    hx = W / 2
    s.centred("HAPTICS", by - 84, 14, s.accent, 0.9, 1.6, hx)
    seg_w = 190
    for i, (label, on) in enumerate((("OFF", False), ("SOFT", True), ("STRONG", False))):
        s.pill(hx - seg_w - 10 + i * (seg_w + 10), by, seg_w, 88, label, s.p1 if on else s.white, 20, pressed=on, dim=not on)
    s.pill(W - left - 230, by, 460, 124, "DONE", s.p1, 44)
    cv.glow(W - left - 230, by, 260, s.p1, 0.08)
    return s.save("settings")


def sync():
    s = Screen("solar", 13)
    cv = s.cv
    cv.nebula([(1200, 480, 900, s.hue[0], 0.022)])
    s.stars(160)
    s.centred("SYNC", 60, 34, s.accent, 1.4, 3.0)
    s.centred("MAKE THE FLASH AND THE CLICK HAPPEN TOGETHER", 128, 16, s.white, 1.2, 1.8)
    # the ring, caught 60 ms after a beat: disc fading, outline hot, a ring on its way out
    cx, cy, r = W / 2, 400, 150
    cv.discs([(cx, cy, r)], s.white, 0.27)
    cv.glow(cx, cy, 520, s.accent, 0.10)
    cv.lines(ld.transform(ld.polygon(72), cx, cy, r), s.white, 2.6, 3.0)
    cv.lines(ld.transform(ld.polygon(72), cx, cy, 183), ld.f_angular(cx, cy, [s.accent, s.white, s.accent, s.hue[1]]), 0.9, 2.0)
    for k in range(4):
        cv.discs([(cx - 54 + k * 36, 628, 7)], s.accent if k == 0 else s.white, 1.9 if k == 0 else 0.3)
    # slider: -150 .. +350 ms, zero ticked, thumb at +35
    sx0, sx1, sy = W / 2 - 550, W / 2 + 550, 742
    at = lambda ms: sx0 + (sx1 - sx0) * (ms + 150) / 500.0
    cv.lines([(sx0, sy, sx1, sy)], s.white, 0.5, 2.0)
    cv.lines([(at(0), sy - 18, at(0), sy + 18)], s.white, 1.0, 2.0)
    for ms in range(-150, 351, 50):
        cv.lines([(at(ms), sy - 7, at(ms), sy + 7)], s.white, 0.35, 1.4)
    cv.lines([(at(0), sy, at(35), sy)], s.p1, 1.8, 4.0)
    cv.discs([(at(35), sy, 22)], s.p1, 0.5)
    cv.lines(ld.transform(ld.polygon(36), at(35), sy, 22), s.p1, 2.2, 2.6)
    cv.glow(at(35), sy, 90, s.p1, 0.12)
    s.centred("+35 MS", sy + 46, 22, s.white, 1.4, 2.2, at(35) + 40)
    for sign, x in ((-1, sx0 - 110), (1, sx1 + 110)):
        pressed = sign > 0
        if pressed:
            cv.discs([(x, sy, 55)], s.p1, 0.45)
            cv.glow(x, sy, 150, s.p1, 0.10)
        cv.lines(ld.transform(ld.polygon(48), x, sy, 55), s.p1 if pressed else s.white, 2.0 if pressed else 0.9, 3.0 if pressed else 2.0)
        arms = [(x - 20, sy, x + 20, sy)] + ([(x, sy - 20, x, sy + 20)] if sign > 0 else [])
        cv.lines(arms, s.white, 1.8 if pressed else 1.1, 3.0)
    s.pill(M + 190, 960, 330, 100, "AUTO", s.white, 28)
    s.pill(W - M - 230, 960, 460, 124, "DONE", s.p1, 44)
    return s.save("sync")


def main():
    os.makedirs(os.path.join(DESIGN, "previews"), exist_ok=True)
    shots = [title(), settings(), sync(), game_over(), pause(), lobby()]
    sheet = Image.new("RGB", (W, H * 2 // 3), (0, 0, 0))
    for i, img in enumerate(shots):
        sheet.paste(img.resize((W // 3, H // 3), Image.LANCZOS), ((i % 3) * (W // 3), (i // 3) * (H // 3)))
    out = os.path.join(DESIGN, "previews", "screens.png")
    sheet.save(out, optimize=True)
    print("wrote", out)


if __name__ == "__main__":
    main()
