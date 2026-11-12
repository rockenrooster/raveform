"""RAVEFORM boss 2, LONGWAVE: parts, numbers and patterns (edit here), export, preview.

Longwave is a serpent whose body is a travelling sine wave. A segment can be hurt only while it is the
crest of the wave, and the crest runs from head to tail one segment at a time, on the grid. What it
teaches: move with the music. Standing still you may hit a segment for one eighth note in a bar;
flying along with the crest you never stop hitting.

Usage:  python design/tools/boss_longwave.py   -> design/bosses/longwave.json, design/previews/boss_longwave.png
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

# --------------------------------------------------------------------------- the design (world units)

BPM = 126.0
BEAT = 60.0 / BPM
BAR = 4 * BEAT
SEGMENTS = 24
SPACING = 100.0                    # along the spine
SEG_RADIUS = 34.0
HEAD_RADIUS = 70.0
AMPLITUDE = 150.0                  # sideways swing of the wave
WAVELENGTH = 8                     # segments per wave: three crests on a whole body
PATH = (1500.0, 520.0)             # half width and half height of the figure of eight the head swims: long, gentle arcs
BULLET_SPEED = 220.0
BULLET_LIFE = 5.0
CAMERA_ZOOM = 0.75                 # the camera pulls back for this fight so the wave can be read
BOSS_ACCENT = (0.70, 258.0)        # OKLCH lightness and hue: electric blue, the complement of the Solar palette
ECHOES = [(0.125, 0.22), (0.25, 0.10)]   # (phase behind the body in cycles, line intensity): the wave's after-images

HULL = [(-30.0, 0.0), (-12.0, 21.0), (14.0, 21.0), (30.0, 0.0), (14.0, -21.0), (-12.0, -21.0)]


def fin(side, reach):
    """A swept blade on one flank; it opens as its segment crests."""
    s = side
    return [(-4.0, 21.0 * s), (-30.0, (21.0 + reach) * s), (12.0, 21.0 * s)]


def head_shapes(jaw):
    """Head-local shapes, x forward. jaw 0 closed .. 1 open."""
    spread = 16.0 + 26.0 * jaw
    upper = [(-40.0, 12.0), (-10.0, 44.0), (38.0, spread + 10.0), (70.0, spread), (30.0, 8.0 + 6.0 * jaw)]
    lower = [(x, -y) for x, y in upper]
    crown = [[(-34.0, 0.0), (-78.0, 0.0)], [(-30.0, 16.0), (-70.0, 40.0)], [(-30.0, -16.0), (-70.0, -40.0)]]
    return upper, lower, crown


NUMBERS = dict(
    segments=SEGMENTS, spacing=SPACING, segmentRadius=SEG_RADIUS, headRadius=HEAD_RADIUS,
    segmentHealth=60, headHealth=600,
    damageScale=dict(crest=1.0, elsewhere=0.0, headJawOpen=1.0, headJawOpenBare=3.0, headJawClosed=0.0),
    wave=dict(amplitude=AMPLITUDE, wavelengthSegments=WAVELENGTH, taperSegments=3,
              rule="sideways offset of live segment k = amplitude x min(1, k / taperSegments) x sin(2 pi (k / wavelength - steps / wavelength)), "
                   "steps = grid steps since the fight began. The segment where the sine is +1 is the crest; where it is -1, the trough. "
                   "The crest moves one segment toward the tail on every step.",
              crestWindow="a segment is the crest for the whole step in which the sine peaks on it"),
    path=dict(shape="figure of eight: x = halfWidth x cos(p), y = halfHeight x sin(2p) around the anchor",
              halfWidth=PATH[0], halfHeight=PATH[1],
              anchor="the centre of the players when it arrives, kept at least 1100 wu from every wall",
              segments="segment k sits on the same path, k x spacing behind the head along it, then swings sideways with the wave",
              continuity="p advances at the phase's rate; a phase change alters the rate, never p, so the body never jumps"),
    camera=dict(zoom=CAMERA_ZOOM, note="eased over the entrance bar; back to 1 over a bar when it dies"),
    bulletSpeed=BULLET_SPEED, bulletLifeSeconds=BULLET_LIFE,
    phases=[
        dict(name="Swell", segmentsAlive=[24, 17], crestStep="1/8", lapBars=16,
             crestSpeedWu=round(SPACING / (BEAT / 2)), note="slower than the ship: fly along with a crest",
             fire=dict(who="every trough", when="its step", bullets=2, pattern="one to each side, square to the body")),
        dict(name="Chop", segmentsAlive=[16, 9], crestStep="1/8 triplet", lapBars=12,
             crestSpeedWu=round(SPACING / (BEAT / 3)), note="just faster than the ship: each crest overtakes you slowly",
             fire=dict(who="every trough", when="its step", bullets=6, pattern="a fan of three to each side, 16 degrees apart")),
        dict(name="Whitecap", segmentsAlive=[8, 1], crestStep="1/16", lapBars=8,
             crestSpeedWu=round(SPACING / (BEAT / 4)), note="crests whip past; the body is short, so stay beside it",
             fire=dict(who="every trough", when="its step", bullets=2, pattern="one to each side"),
             lunge=dict(every="second bar", telegraphBeats=4, dashWu=700, dashBeats=1,
                        note="a dashed line from the head through where the nearest player stood at the start of the telegraph bar; "
                             "on the next downbeat the head dashes along it and its jaw stays open for two beats")),
        dict(name="Bare head", segmentsAlive=[0, 0], lapBars=0,
             lunge=dict(every="bar", telegraphBeats=3, dashWu=700, dashBeats=1),
             headBurst=dict(on="beats 2 and 4", count=16, note="alternate rings turned half a step")),
    ],
    whip=dict(when="the downbeat after each phase change", count=32, note="a ring from the tail tip; the tail cracks like a whip"),
    jaw=dict(open="the whole of beat 1 of every bar", note="the head is hurt only through the open jaw; a horn marks it"),
    music=dict(
        layer="a rubber-string run: the bass patch one octave up with decay x 2, one note on every crest step",
        rule="the note is the crest's place in its wave (0-7) as a degree of the Track's scale above the chord root: 0 2 3 5 7 8 10 12. "
             "The crest running down the body is heard as a scale running up; when the wave speeds up, so does the run",
        trough="the telegraph tick on every step a trough fires",
        jaw="the elite-arrives patch (a low fifth) on beat 1 while the jaw is open",
        whip="the impact sample, pitched up a fifth, on the whip's downbeat"),
    accent=dict(oklchLightness=BOSS_ACCENT[0], oklchHue=BOSS_ACCENT[1], chromaFraction=0.96,
                use="fins, crown, eyes and the spine's core line: a boss is the one enemy with a second colour family, "
                    "the complement of its Track's palette"),
    echoes=[dict(phaseBehindCycles=p, intensity=i, note="the spine line only, drawn where the body was that fraction of a wave ago") for p, i in ECHOES],
    entrance="on the Drop's downbeat the head breaks in from the nearest wall and the body follows it onto the figure of eight "
             "over two bars, already waving; the name card LONGWAVE holds for two bars",
    segmentDeath="the segment shatters along the wave's direction, its neighbours snap together with a white flash at the join, "
                 "and the crest skips on without missing a step",
    death="the jaw locks open, the clamped white flash, what is left of the body unravels tail-first one segment per sixteenth, "
          "each a medium kill, then the head as a boss break: three rings, 300+ sparks, 30 px distortion ring, 50 ms hit-stop, then 200 Shards",
)


def build():
    seg = lambda pts, layer, closed=True: [[round(v, 3) for v in s] + [layer] for s in ld.polyline(pts, closed=closed)]
    upper, lower, crown = head_shapes(0.0)
    upper_open, lower_open, _ = head_shapes(1.0)
    parts = dict(
        segment=dict(radius=SEG_RADIUS, glass=True,
                     segments=seg(HULL, 0) + seg([(-12.0, 12.0), (8.0, 0.0), (-12.0, -12.0)], 1, False) + seg([(-22.0, 0.0), (22.0, 0.0)], 2, False),
                     fin=dict(closedReach=8.0, crestReach=30.0,
                              note="two blades, one per flank: [(-4, 21), (-30, 21 + reach), (12, 21)] and its mirror; "
                                   "reach follows the wave: crestReach on the flank the segment is swinging toward at its crest, closedReach otherwise")),
        head=dict(radius=HEAD_RADIUS,
                  jawClosed=seg(upper, 0) + seg(lower, 0), jawOpen=seg(upper_open, 0) + seg(lower_open, 0),
                  crown=[[round(v, 3) for v in (a[0], a[1], b[0], b[1])] + [1] for a, b in crown],
                  eyes=[[18.0, 26.0], [18.0, -26.0]], seed=dict(x=34.0, radius=9.0, note="the light inside the jaw: the head's weak point")),
        tail=dict(blades=[[-10.0, 0.0, -64.0, 0.0], [-8.0, 6.0, -52.0, 30.0], [-8.0, -6.0, -52.0, -30.0]]),
        spine=dict(widthRefPx=3.4, coreWidthRefPx=1.2, note="one line through every live segment, head to tail"))
    return dict(
        note="Generated by design/tools/boss_longwave.py. World units. Shapes are local to their part, x forward along the body, "
             "as [x0,y0,x1,y1,layer] (0 hull, 1 inner mark, 2 core line). The body's colour runs along it: enemy hue 1 at the head, "
             "hue 2 in the middle, hue 3 at the tail, each toward its partner at the flanks; segments are drawn as elites. "
             "The crest segment is drawn white-hot.",
        bpm=BPM, parts=parts, numbers=NUMBERS)


# --------------------------------------------------------------------------- preview

class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == "solar")
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
        self.blue = ld.oklch(BOSS_ACCENT[0], BOSS_ACCENT[1], cf=0.96)[0].astype(np.float32)
        self.grid = lin(self.pal["grid"])
        self.bg0, self.bg1 = lin(self.pal["bg0"]), lin(self.pal["bg1"])


def body(p_head, steps, phase_shift=0.0, ks=None):
    """World positions, tangents and wave values at places ks along the body (0 = head, 1 = first
    segment, fractions in between for the spine line)."""
    ks = range(SEGMENTS + 1) if ks is None else ks
    ps = np.linspace(p_head - 2 * PI, p_head, 12000)
    xs, ys = PATH[0] * np.cos(ps), PATH[1] * np.sin(2 * ps)
    d = np.concatenate([[0.0], np.cumsum(np.hypot(np.diff(xs), np.diff(ys)))])
    total = d[-1]
    out = []
    for k in ks:
        i = int(np.searchsorted(d, total - k * SPACING))
        i = max(3, min(len(ps) - 4, i))
        tx, ty = xs[i + 3] - xs[i - 3], ys[i + 3] - ys[i - 3]
        l = math.hypot(tx, ty)
        tx, ty = tx / l, ty / l
        w = math.sin(2 * PI * (k / WAVELENGTH - steps / WAVELENGTH - phase_shift))
        off = AMPLITUDE * min(1.0, k / 3.0) * w
        out.append((xs[i] - ty * off, ys[i] + tx * off, tx, ty, w))
    return out


def preview(path):
    a = Art()
    W, H = 2400, 1080
    s = 1.5 * CAMERA_ZOOM
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.5, a.bg1 * 0.5, cx=0.5, cy=0.5)
    cv.nebula([(500, 280, 760, a.hue[0], 0.024), (1900, 800, 760, a.hue[2], 0.022)])
    step_px = 60 * s
    grid = [(gx, 0, gx, H) for gx in np.arange(W / 2 % step_px, W + 1, step_px)] + [(0, gy, W, gy) for gy in np.arange(H / 2 % step_px, H + 1, step_px)]
    cv.lines(grid, a.grid, 0.16, 1.0)
    view = (520.0, -120.0)          # where the camera is, in boss space
    to_px = lambda x, y: (W / 2 + (x - view[0]) * s, H / 2 + (y - view[1]) * s)

    steps = 4.0                     # crests on segments 6, 14, 22; troughs on 2, 10, 18
    p_head = 0.2
    pts = body(p_head, steps)
    fine_k = [i / 8.0 for i in range(SEGMENTS * 8 + 1)]

    # the wave's after-images: where the spine was an eighth and a quarter of a wave ago
    for shift, level in ECHOES:
        ghost = [to_px(x, y) for x, y, *_ in body(p_head, steps, shift, fine_k)]
        cv.lines(ld.polyline(ghost), a.blue, level * 2.2, 1.6)

    px = [to_px(x, y) for x, y, *_ in pts]
    stops = [a.hue[0], a.hue[0], a.hue[1], a.hue[2], a.mate[2]]
    colour_at = lambda k: ld.f_linear(0, 0, 1, 0, stops)(np.array([k / SEGMENTS], np.float32), np.array([0.0], np.float32))[0]
    # the spine: one smooth line through every segment
    fine = [to_px(x, y) for x, y, *_ in body(p_head, steps, 0.0, fine_k)]
    for i in range(len(fine) - 1):
        c0, c1 = colour_at(fine_k[i]), colour_at(fine_k[i + 1])
        seg = [(fine[i][0], fine[i][1], fine[i + 1][0], fine[i + 1][1])]
        cv.lines(seg, ld.f_linear(fine[i][0], fine[i][1], fine[i + 1][0], fine[i + 1][1], [c0, c1]), 2.2, 3.4)
        cv.lines(seg, a.blue * 0.5 + a.white * 0.5, 1.6, 1.2)

    bullets = []
    for k in range(1, SEGMENTS + 1):
        x, y, tx, ty, w = pts[k]
        cx, cy = px[k]
        ang = math.atan2(ty, tx)
        base, part = colour_at(k), a.mate[min(2, k * 3 // (SEGMENTS + 1))]
        crest, trough = w > 0.95, w < -0.95
        warm = max(0.0, w) ** 3
        field = ld.f_radial(cx, cy, SEG_RADIUS * s, [base * 0.6 + a.white * 0.4, base, part])
        cv.fill(ld.transform_points(HULL, cx, cy, s, ang), a.white if crest else base, 0.18 if crest else 0.09)
        cv.lines(ld.transform(ld.polyline(HULL, closed=True), cx, cy, s, ang), field, 2.6, 2.3)
        cv.lines(ld.transform(ld.polyline(HULL, closed=True), cx, cy, s, ang), a.white, 1.0 + 1.6 * warm, 0.9 + 0.8 * warm)
        cv.lines(ld.transform(ld.polyline([(-12.0, 12.0), (8.0, 0.0), (-12.0, -12.0)]), cx, cy, s, ang), a.accent * 0.5 + part * 0.5, 1.8, 1.4)
        for side in (1, -1):
            reach = 8.0 + 22.0 * max(0.0, w * side)
            blade = ld.transform(ld.polyline(fin(side, reach)), cx, cy, s, ang)
            cv.lines(blade, a.white if crest and side > 0 else a.blue, 2.6 if crest and side > 0 else 2.0, 1.7)
        if crest:
            cv.glow(cx, cy, 90 * s, a.white, 0.12)
            cv.flare(cx, cy, 130 * s, a.white, 1.8, 1.4, 2, ang + PI / 2)
            cv.flare(cx, cy, 60 * s, a.white, 1.4, 1.2, 2, ang)
        if trough:
            cv.glow(cx, cy, 60 * s, a.danger, 0.16)
            # what this trough and the troughs before it have fired: one bullet to each side on every step
            for j in range(0, 13):
                kk = k - j
                if kk < 1:
                    break
                bx, by, btx, bty, _ = pts[kk]
                dist = (j + 0.35) * (BEAT / 2) * BULLET_SPEED
                for side in (1, -1):
                    bullets.append(to_px(bx - bty * side * (SEG_RADIUS + dist), by + btx * side * (SEG_RADIUS + dist)))

    # tail whip
    x, y, tx, ty, _ = pts[SEGMENTS]
    ang = math.atan2(ty, tx)
    tcx, tcy = px[SEGMENTS]
    cv.lines(ld.transform([(-34.0, 0.0, -88.0, 0.0), (-32.0, 6.0, -76.0, 30.0), (-32.0, -6.0, -76.0, -30.0)], tcx, tcy, s, ang),
             ld.f_fade(ld.f_solid(a.blue), tcx, tcy, 100 * s, 1.2), 2.4, 2.0)

    # head: jaw half open on beat 1, the seed showing
    x, y, tx, ty, _ = pts[0]
    ang = math.atan2(ty, tx)
    hx, hy = px[0]
    upper, lower, crown = head_shapes(0.8)
    cv.glow(hx, hy, 260 * s, a.hue[0], 0.16)
    for jaw in (upper, lower):
        cv.fill(ld.transform_points(jaw, hx, hy, s, ang), a.hue[0], 0.12)
        cv.lines(ld.transform(ld.polyline(jaw, closed=True), hx, hy, s, ang), ld.f_radial(hx, hy, 80 * s, [a.white, a.hue[0], a.mate[0]]), 3.0, 2.6)
        cv.lines(ld.transform(ld.polyline(jaw, closed=True), hx, hy, s, ang), a.white, 1.4, 1.0)
    cv.lines(ld.transform([(c[0][0], c[0][1], c[1][0], c[1][1]) for c in crown], hx, hy, s, ang), ld.f_fade(ld.f_solid(a.blue), hx, hy, 90 * s, 1.0), 2.8, 2.2)
    for ex, ey in ((18.0, 26.0), (18.0, -26.0)):
        e = ld.transform_points([(ex, ey)], hx, hy, s, ang)[0]
        cv.lines(ld.transform(ld.star(4, 7.0, 2.6), e[0], e[1], s, ang), a.blue * 0.4 + a.white * 0.6, 3.0, 1.5)
    seed = ld.transform_points([(34.0, 0.0)], hx, hy, s, ang)[0]
    cv.discs([(seed[0], seed[1], 9 * s)], a.gold, 1.2)
    cv.glow(seed[0], seed[1], 70 * s, a.gold, 0.5)
    cv.flare(seed[0], seed[1], 90 * s, a.white, 1.6, 1.3, 2, ang)

    # the ship rides beside the first crest and pours fire into it
    x, y, tx, ty, w = pts[6]
    sx, sy = to_px(x - ty * 190, y + tx * 190)
    cx, cy = px[6]
    aim = math.atan2(cy - sy, cx - sx)
    hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
    r = 14 * s
    cv.glow(sx, sy, r * 6, a.p1, 0.13)
    cv.lines(ld.transform(hull, sx, sy, r, aim), ld.f_radial(sx, sy, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
    cv.lines(ld.transform(hull, sx, sy, r, aim), a.white, 2.0, 0.8)
    halo = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo, sx, sy, r, 0.4), ld.f_angular(sx, sy, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)
    d = math.hypot(cx - sx, cy - sy)
    for lane in (-10.0, 0.0, 10.0):
        ox, oy = -math.sin(aim) * lane * s, math.cos(aim) * lane * s
        for u in np.arange(0.2, 0.95, 0.26):
            hx2, hy2 = sx + (cx - sx) * u + ox, sy + (cy - sy) * u + oy
            tx2, ty2 = hx2 - math.cos(aim) * 31 * s, hy2 - math.sin(aim) * 31 * s
            cv.lines([(tx2, ty2, hx2, hy2)], ld.f_linear(tx2, ty2, hx2, hy2, [a.p1, a.white]), 4.6, 2.2)
    for i in range(9):                                      # sparks off the crest
        an = aim + PI + (i - 4) * 0.22
        cv.lines([(cx + math.cos(an) * 30, cy + math.sin(an) * 30, cx + math.cos(an) * (52 + 9 * (i % 3)), cy + math.sin(an) * (52 + 9 * (i % 3)))], a.white, 2.2, 1.3)

    # bullets: upright diamonds on the threat layer
    for bx, by in bullets:
        if -20 < bx < W + 20 and -20 < by < H + 20:
            cv.threat(ld.transform(ld.polygon(4), bx, by, 7.5), a.danger * 0.75 + a.white * 0.25, 2.1, 1.5)

    # notes
    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))
    note("LONGWAVE", 132, 60, 44, a.hue[0], 1.6)
    note("BOSS OF TRACK 2  SOLAR WIND  126 BPM", 132, 126, 14, a.accent, 1.0)
    note("THE CREST IS WHITE-HOT: THE ONLY PLACE IT CAN BE HURT", 132, 960, 13)
    note("IT RUNS HEAD TO TAIL, ONE SEGMENT EVERY EIGHTH NOTE: FLY WITH IT", 132, 986, 13)
    note("TROUGHS FIRE TO BOTH SIDES ON THE SAME STEPS", 132, 1012, 13, a.danger * 0.75 + a.white * 0.25)
    note("JAW OPENS ON BEAT 1", hx - 420, hy - 6, 13, a.gold)
    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "longwave.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "boss_longwave.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
