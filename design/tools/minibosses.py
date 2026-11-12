"""Mini-bosses, batch 1: Alpha Hound, The Hive, Battering Ram, Carom King.

Writes design/bosses/minibosses.json (numbers) and design/previews/minibosses_1.png (four panels).
Run: . .\\scripts\\env.ps1; python -I design\\tools\\minibosses.py
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

PEN_SIDE = 1400.0

COMMON = dict(
    arrives="on the downbeat of the middle cycle's Drop from Track 2 on, in place of the mega-wave; a small name card holds for one bar",
    pick="the run's seed draws one whose family the run has already met, never the one met in the Track before. Batch 1: Track 2 draws from Alpha Hound and The Hive, Track 3 from Battering Ram and Carom King",
    drop="the Drop holds while the mini-boss lives, for at most 8 bars past its 16; then it warps out and leaves nothing",
    healthByTrack={"2": 600, "3": 1100, "4": 1700, "5": 2400, "6": 3200, "loopFactorPerTrack": 1.25,
                   "note": "times the mini-boss's own healthFactor; in co-op x (1 + 0.5 x (players - 1)), as GAME_DESIGN 8 has it. Sized for 25 to 35 seconds of fire with the build of that Track's middle cycle: tune with the bot"},
    crowd={"sectionScale": 0.75, "note": "fodder only, on top of what the mini-boss calls itself"},
    reward={"boostOrbs": 1, "shards": 60, "note": "for each player; the kill also scores 25 times a tier-5 body of its family"},
    body="its family's tier-5 glyph enlarged, with one more ring of ornament and the white equipment marks; off screen it gets the boss's double chevron",
    healthBar="a thin bar under the section bar with its name, as the boss's but half the height",
    pen={"side": PEN_SIDE, "riseBeats": 1,
         "note": ("a square of white light walls raised round the players' centre on the downbeat, for mini-bosses that need walls (the arena's own are too far apart). "
                  "Walls stop ships and bodies without hurting; shots pass through them; fodder caught outside dissolves; the crowd warps in inside. It falls when the fight ends. It is a structure of M5's list")},
)

MINIBOSSES = [
    dict(
        id="alpha_hound", name="Alpha Hound", family="hound", glyph="hound_5", radius=64.0, healthFactor=1.0, pen=False,
        idea="it does not chase: it circles and sends the pack",
        move="circles the nearest player at 420 wu, 170 wu/s, turning to keep them in front",
        pattern=[
            "beat 1 of every bar: the howl. Six Hounds of the Track's tier warp in as a chevron 500 wu from the player on the Alpha's side, with a danger line from the chevron's tip through the player's position at the howl",
            "beat 3: the pack charges along that line at 520 wu/s for two beats, through and past; afterwards they are ordinary Hounds",
            "every fourth bar the Alpha lunges itself: a danger line on beats 3 and 4, then 900 wu/s for one beat through the player's position at the lock. It is winded for the two beats after",
        ],
        weakPoint="winded: damage x 2 for two beats after each lunge. At other times x 1 anywhere on the body",
        half="two packs of five a bar, from opposite sides: a pincer. The lunge comes every third bar",
        death="the pack scatters: every Hound alive turns tail for two bars and drops double Shards",
        sound="the howl is a saw note gliding up a fifth on beat 1; the pack's charge is a snare roll into beat 3; the lunge is the sub drop",
        cap="at most 30 called Hounds alive; a howl that would pass it calls fewer",
    ),
    dict(
        id="the_hive", name="The Hive", family="shoal", glyph="shoal_5", radius=18.0, healthFactor=0.8, pen=False,
        idea="a cloud that becomes shapes; the Queen is the only part with health",
        move="120 darts and one gold Queen. A dart dies to one shot and stops it, so the cloud is her armour. She hatches 12 darts a bar while she lives, up to 160",
        pattern=[
            "the shape changes every two bars, on the bar line, with one beat of every dart flashing white as the tell: Ring, Spear, Wall, then again",
            "Ring: a circle of radius 300 round the player closing to 120 over the two bars, with one gap of 60 degrees that turns half a turn a bar. The Queen rides the ring opposite the gap",
            "Spear: a tight column that charges through the player's position on beats 1 and 3 (danger line a beat ahead), 700 wu/s. The Queen is the last dart of the column",
            "Wall: a line 900 wu long crossing the frame at 160 wu/s with two gaps of 90 wu. The Queen flies behind its middle",
        ],
        weakPoint="the Queen, whenever a shape leaves her open. Ring: she is in plain view on the far side from the gap, so every shot at her is time not spent leaving. Spear: from behind, after it passes. Wall: from behind, once through a gap",
        half="the cloud divides into two hives of 60 that take different shapes; one carries the Queen, the other a white decoy",
        death="every dart falls at once as Shards: the biggest single Shard drop of the Track",
        sound="the cloud is the shaker; a change of shape is a reverse cymbal into the bar line; the Ring is closed hats that tighten as it closes, the Spear two toms, the Wall a noise sweep",
        cap="160 darts, counted against the enemy cap; the crowd gives up that many places while the Hive lives",
    ),
    dict(
        id="battering_ram", name="Battering Ram", family="ram", glyph="ram_5", radius=80.0, healthFactor=1.2, pen=True,
        idea="a bullfight: make it hit the wall",
        move="stands and turns between charges; its front 120 degrees sheds every shot, sides and back take x 1",
        pattern=[
            "two bars a charge. Bar one: beats 1 and 2 it turns to face the nearest player; beat 3 it locks, and a danger line runs from its horns through the player to the wall; beat 4 it holds",
            "bar two, beat 1: it charges along the line at 1,400 wu/s to the player's locked position and skids to a stop there",
            "if a wall of the pen is within 300 wu behind that position it cannot stop: it hits the wall, the wall rings (16 bullets fan out from the point of impact) and it is stunned",
        ],
        weakPoint="stunned for three beats after hitting a wall: horns down, damage x 2 from every side",
        half="after a skid it locks again in one beat and charges a second time; the stun is two beats; its charge leaves a wake in the danger colour that lasts a bar",
        death="it charges once more with no one aiming it, breaks through the pen and the walls fall outward as Shards",
        sound="the lock is a tom roll rising through beats 3 and 4; the charge is the sub drop and a driven kick; the wall is the gong; while it is stunned the bed is low-passed",
        cap="one body, at most 16 bullets and one wake",
    ),
    dict(
        id="carom_king", name="Carom King", family="carom", glyph="carom_5", radius=80.0, healthFactor=1.0, pen=True,
        idea="it plays the pen like an instrument, and every quarter of its health is another player in the band",
        move="straight lines at about 300 wu/s, reflecting off the pen. Its speed on each leg is trimmed so that every bounce lands on a beat; each bounce is 8% faster, up to x 1.6",
        pattern=[
            "every bounce sheds five bullets from the point of impact, fanned 40 degrees either side of its new heading",
            "at 75%, 50% and 25% of its health it sheds its outer square as a Prince: radius 40, 12% of the King's full health, 1.3 times its speed, bouncing on the half beats. The King is left a size smaller (80, 68, 56, 44) and 15% faster",
            "a dead Prince breaks into two ordinary Caroms",
        ],
        weakPoint="for one beat after each bounce its core is gold and it takes x 2: hit it as it comes off the wall",
        half="see the pattern: the splits are the phases",
        death="the last square bursts on a beat and every Prince alive loses its speed and drifts",
        sound=("each wall is a note of the Track's chord (left the root, top the third, right the fifth, bottom the seventh) played as a rim shot, so the King's bounces are an arpeggio that speeds up. "
               "Princes play the same notes an octave up on the half beats"),
        cap="one King, three Princes, six Caroms and at most 60 bullets",
    ),
]


def build():
    return {
        "note": "Generated by design/tools/minibosses.py. World units. Rules common to every mini-boss, then one entry each. Batch 1 of 3 (GAME_DESIGN 7.5 and 13).",
        "common": COMMON,
        "minibosses": MINIBOSSES,
    }


class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
        self.font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
        self.lin = lambda e: np.asarray(e["linear"], np.float32)
        self.white = np.ones(3, np.float32)
        self.danger = self.lin(self.pals["constants"]["danger"])
        p = self.pals["constants"]["players"][0]
        self.p1, self.p1_deep, self.p1_light = self.lin(p), self.lin(p["deep"]), self.lin(p["light"])
        self.gold = self.lin(self.pals["constants"]["pickup"])

    def palette(self, pid):
        pal = next(p for p in self.pals["palettes"] if p["id"] == pid)
        cands = [pal["enemyCandidates"][i] for i in pal["soloEnemies"]]
        return dict(bg0=self.lin(pal["bg0"]), bg1=self.lin(pal["bg1"]), grid=self.lin(pal["grid"]), accent=self.lin(pal["accent"]),
                    enemies=[(self.lin(c), self.lin(c["partners"][c["soloPartner"]])) for c in cands])

    def segs(self, gid, layers=(0, 1, 2)):
        return [(g[0], -g[1], g[2], -g[3]) for g in self.glyphs[gid]["segments"] if g[4] in layers]


def preview(path):
    a = Art()
    W, H = 2400, 1080
    PW, PH = W // 2, H // 2
    cv = ld.Canvas(W, H)
    solar, reef = a.palette("solar"), a.palette("reef")
    cv.radial_bg(solar["bg0"] * 0.5, solar["bg1"] * 0.45, cx=0.5, cy=0.5)
    cv.nebula([(600, 270, 620, solar["accent"], 0.012), (1800, 270, 620, solar["accent"], 0.012),
               (600, 810, 620, reef["accent"], 0.016), (1800, 810, 620, reef["accent"], 0.016)])
    cv.lines([(PW, 20, PW, H - 20), (20, PH, W - 20, PH)], a.white, 0.35, 1.2)
    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))

    def grid(x0, y0, pal, step):
        gs = [(gx, y0 + 8, gx, y0 + PH - 8) for gx in np.arange(x0 + step / 2, x0 + PW, step)] + [(x0 + 8, gy, x0 + PW - 8, gy) for gy in np.arange(y0 + step / 2, y0 + PH, step)]
        cv.lines(gs, pal["grid"], 0.12, 1.0)

    def body(gid, x, y, r, rot, col, partner, level=3.0):
        cv.glow(x, y, r * 3.2, col, 0.16)
        cv.lines(ld.transform(a.segs(gid, (0,)), x, y, r, rot), ld.f_radial(x, y, r * 1.2, [a.white, col, partner]), level, 2.6)
        cv.lines(ld.transform(a.segs(gid, (0,)), x, y, r, rot), a.white, level * 0.5, 0.9)
        cv.lines(ld.transform(a.segs(gid, (1, 2)), x, y, r, rot), partner * 0.5 + a.white * 0.5, level * 0.8, 1.8)

    def small(gid, x, y, r, rot, col, level=2.2, width=1.6):
        cv.lines(ld.transform(a.segs(gid, (0,)), x, y, r, rot), col, level, width)

    def ship(x, y, s, aim):
        r = 14 * s
        hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
        cv.glow(x, y, r * 6, a.p1, 0.15)
        cv.lines(ld.transform(hull, x, y, r, aim), ld.f_radial(x, y, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
        cv.lines(ld.transform(hull, x, y, r, aim), a.white, 2.0, 0.8)
        halo = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
        cv.lines(ld.transform(halo, x, y, r, 0.4), ld.f_angular(x, y, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)

    def danger_line(x0, y0, x1, y1, dash=18.0, gap=12.0):
        ln = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / ln, (y1 - y0) / ln
        segs = [(x0 + ux * d, y0 + uy * d, x0 + ux * min(d + dash, ln), y0 + uy * min(d + dash, ln)) for d in np.arange(0, ln, dash + gap)]
        cv.threat(segs, a.danger * 0.8 + a.white * 0.2, 2.2, 1.4)

    def bullet(x, y, r):
        cv.threat(ld.transform(ld.polygon(4, 1.0, 0.0), x, y, r), a.danger * 0.75 + a.white * 0.25, 2.0, 1.4)

    def pen(cx, cy, s):
        h = PEN_SIDE / 2 * s
        box = ld.polyline([(cx - h, cy - h), (cx + h, cy - h), (cx + h, cy + h), (cx - h, cy + h)], closed=True)
        cv.lines(box, a.white, 2.4, 3.0)
        cv.lines(ld.polyline([(cx - h - 7, cy - h - 7), (cx + h + 7, cy - h - 7), (cx + h + 7, cy + h + 7), (cx - h - 7, cy + h + 7)], closed=True), a.white, 0.5, 1.2)
        return h

    def title(x0, y0, name, line1, line2, colour):
        note(name, x0 + 44, y0 + 34, 28, a.white, 1.5)
        note(line1, x0 + 44, y0 + PH - 62, 12, colour)
        note(line2, x0 + 44, y0 + PH - 38, 12, a.gold)

    # ---- 1: Alpha Hound (Solar) -------------------------------------------------------------
    x0, y0, s = 0, 0, 0.62
    grid(x0, y0, solar, 60 * s * 1.5)
    col, par = solar["enemies"][0]
    sx, sy = x0 + 400, y0 + 300
    ax, ay = sx + 420 * s * 1.5 * 0.94, sy - 420 * s * 1.5 * 0.34
    body("hound_5", ax, ay, 64 * s * 1.5, math.atan2(sy - ay, sx - ax), col, par)
    for k, lev in ((1, 1.2), (2, 0.7), (3, 0.35)):      # the howl
        cv.lines(ld.transform(ld.polygon(48, 1.0, 0.0), ax, ay, (64 + 46 * k) * s * 1.5), a.white, lev, 1.6)
    tipx, tipy = sx + 500 * s * 1.5 * 0.80, sy + 500 * s * 1.5 * 0.10
    head = math.atan2(sy - tipy, sx - tipx)
    danger_line(tipx, tipy, sx - math.cos(head) * -260, sy - math.sin(head) * -260)
    for row, lat in ((0, 0.0), (1, 1.0), (1, -1.0), (2, 2.0), (2, -2.0), (3, 0.0)):
        hx_ = tipx - math.cos(head) * row * 34 - math.sin(head) * lat * 30
        hy_ = tipy - math.sin(head) * row * 34 + math.cos(head) * lat * 30
        small("hound_2", hx_, hy_, 14 * s * 1.5, head, col * 0.7 + a.white * 0.3, 2.6, 1.8)
    ship(sx, sy, s * 1.5, math.atan2(ay - sy, ax - sx))
    title(x0, y0, "ALPHA HOUND", "IT HOWLS ON BEAT 1 AND THE PACK CHARGES ON BEAT 3", "WINDED FOR TWO BEATS AFTER ITS OWN LUNGE: X2", col)

    # ---- 2: The Hive (Solar), the Ring -------------------------------------------------------
    x0, y0, s = PW, 0, 0.62
    grid(x0, y0, solar, 60 * s * 1.5)
    col, par = solar["enemies"][2]
    sx, sy = x0 + PW / 2, y0 + PH / 2 - 6
    ring = 230 * s * 1.5
    gap_mid, gap_half = math.radians(205.0), math.radians(30.0)
    rng = np.random.default_rng(7)
    for i in range(118):
        t = rng.uniform(0, 2 * PI)
        d = (t - gap_mid + PI) % (2 * PI) - PI
        if abs(d) < gap_half:
            continue
        rr = ring + rng.normal(0, 13)
        small("shoal_1", sx + math.cos(t) * rr, sy + math.sin(t) * rr, 9 * s * 1.5, t + PI / 2, col * 0.75 + a.white * 0.25, 2.0, 1.4)
    qa = gap_mid + PI
    qx, qy = sx + math.cos(qa) * ring, sy + math.sin(qa) * ring
    cv.glow(qx, qy, 70, a.gold, 0.4)
    cv.lines(ld.transform(a.segs("shoal_5"), qx, qy, 18 * s * 1.5 * 1.3, qa + PI / 2), a.gold * 0.6 + a.white * 0.4, 3.4, 2.4)
    cv.flare(qx, qy, 60, a.gold, 1.6, 1.2, 4, PI / 4)
    for sgn in (-1.0, 1.0):                         # the gap's edges
        ga = gap_mid + sgn * gap_half
        cv.lines([(sx + math.cos(ga) * (ring - 30), sy + math.sin(ga) * (ring - 30), sx + math.cos(ga) * (ring + 30), sy + math.sin(ga) * (ring + 30))], a.white, 1.4, 1.6)
    aim = qa
    ship(sx - 40, sy + 10, s * 1.5, aim)
    for u in (0.3, 0.55, 0.8):
        px, py = sx - 40 + math.cos(aim) * ring * u, sy + 10 + math.sin(aim) * ring * u
        cv.lines([(px - math.cos(aim) * 26, py - math.sin(aim) * 26, px, py)], ld.f_linear(px - math.cos(aim) * 26, py - math.sin(aim) * 26, px, py, [a.p1, a.white]), 4.6, 2.2)
    title(x0, y0, "THE HIVE", "RING, SPEAR, WALL: A NEW SHAPE EVERY TWO BARS", "ONLY THE GOLD QUEEN HAS HEALTH. THE CLOUD IS HER ARMOUR", col)

    # ---- 3: Battering Ram (Reef), locked -----------------------------------------------------
    x0, y0, s = 0, PH, 0.27
    grid(x0, y0, reef, 60 * s * 1.5 * 2)
    col, par = reef["enemies"][0]
    cx, cy = x0 + PW / 2 + 150, y0 + 250
    h = pen(cx, cy, s)
    rx, ry = cx - h * 0.55, cy - h * 0.35
    sx, sy = cx + h * 0.72, cy + h * 0.30
    head = math.atan2(sy - ry, sx - rx)
    wx = cx + h
    wy = sy + math.tan(head) * (wx - sx)
    danger_line(rx, ry, wx, wy, 16.0, 10.0)
    body("ram_5", rx, ry, 80 * s * 2.0, head, col, par)
    cv.lines(ld.polyline([(rx + math.cos(head + t) * 80 * s * 2.6, ry + math.sin(head + t) * 80 * s * 2.6) for t in np.linspace(-PI / 3, PI / 3, 18)]), a.white, 2.2, 2.4)
    cv.flare(wx, wy, 70, a.white, 1.6, 1.3, 4, 0.0)
    ship(sx, sy, 1.25, math.atan2(ry - sy, rx - sx))
    cv.lines([(sx, sy + 26, sx, sy + 120)], a.p1, 0.9, 1.4)
    cv.lines([(sx - 12, sy + 100, sx, sy + 120), (sx + 12, sy + 100, sx, sy + 120)], a.p1, 0.9, 1.4)
    title(x0, y0, "BATTERING RAM", "IT LOCKS ON BEAT 3 AND CHARGES ON THE NEXT BEAT 1", "STAND BY A WALL AND STEP ASIDE: STUNNED FOR THREE BEATS, X2", col)

    # ---- 4: Carom King (Reef) ---------------------------------------------------------------
    x0, y0, s = PW, PH, 0.27
    grid(x0, y0, reef, 60 * s * 1.5 * 2)
    col, par = reef["enemies"][1]
    cx, cy = x0 + PW / 2 + 150, y0 + 250
    h = pen(cx, cy, s)
    # its path: three legs, the last bounce just behind it on the right wall
    p0 = (cx - h * 0.2, cy - h)
    p1 = (cx + h, cy - h * 0.1)
    kx, ky = cx + h * 0.35, cy + h * 0.42
    cv.lines([(cx - h, cy + h * 0.15, p0[0], p0[1]), (p0[0], p0[1], p1[0], p1[1]), (p1[0], p1[1], kx, ky)], col, 0.5, 1.4)
    new_head = math.atan2(ky - p1[1], kx - p1[0])
    for i in range(5):
        an = new_head + math.radians(-40 + 20 * i)
        bullet(p1[0] + math.cos(an) * 110, p1[1] + math.sin(an) * 110, 7)
    cv.flare(p1[0], p1[1], 60, a.white, 1.6, 1.3, 4, 0.0)
    body("carom_5", kx, ky, 68 * s * 2.0, 0.3, col, par)
    cv.discs([(kx, ky, 9)], a.gold * 0.7 + a.white * 0.3, 2.6)
    cv.glow(kx, ky, 70, a.gold, 0.45)
    for (qx, qy, rot) in ((cx - h * 0.5, cy - h * 0.45, 0.6), (cx - h * 0.15, cy + h * 0.62, 1.1)):
        body("carom_3", qx, qy, 40 * s * 2.0, rot, col * 0.8 + a.white * 0.2, par, 2.4)
    for i, (wx_, wy_, lab) in enumerate(((cx - h - 30, cy, "1"), (cx, cy - h - 26, "3"), (cx + h + 16, cy, "5"), (cx, cy + h + 12, "7"))):
        note(lab, wx_, wy_ - 8, 14, a.gold, 1.2)
    ship(cx - h * 0.1, cy + h * 0.1, 1.25, math.atan2(ky - (cy + h * 0.1), kx - (cx - h * 0.1)))
    title(x0, y0, "CAROM KING", "EVERY BOUNCE LANDS ON A BEAT: EACH WALL IS A NOTE OF THE CHORD", "GOLD FOR ONE BEAT AFTER A BOUNCE: X2. EVERY QUARTER SHEDS A PRINCE", col)

    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "minibosses.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "minibosses_1.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
