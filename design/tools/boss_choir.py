"""Boss 3, The Choir: eight Voices on a ring around the players, singing call and response.

Writes design/bosses/choir.json (shapes and numbers) and design/previews/boss_choir.png.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\boss_choir.py
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

BPM = 132.0
BEAT = 60.0 / BPM
VOICES = 8
RING = 380.0                       # the Round: radius of the ring the Voices stand on
RING_FUGUE = 300.0
VOICE_RADIUS = 66.0
BULLET_SPEED = 220.0
CAMERA_ZOOM = 0.75
NOTES = ["D4", "E4", "F4", "G4", "A4", "B4", "C5", "D5"]      # D dorian up the ring: opposite Voices are a fifth apart
MIDI = [62, 64, 65, 67, 69, 71, 72, 74]
PEARL = (0.93, 300.0, 0.22)        # OKLCH lightness, hue, chroma fraction: halos and strings at rest

# A Voice, local units = world units, +x toward the ring's centre (the way it sings).
K = 1.3                            # the Voice is drawn 1.3 times the first sketch: it has to read from across the Round
HOOD = [(K * x, K * y) for x, y in [(30.0, 14.0), (22.0, 34.0), (-6.0, 42.0), (-38.0, 26.0), (-62.0, 0.0), (-38.0, -26.0), (-6.0, -42.0), (22.0, -34.0), (30.0, -14.0)]]
RIB = [[(K * x, K * y) for x, y in rib] for rib in [[(-50.0, 0.0), (-20.0, 0.0)], [(-30.0, 20.0), (-8.0, 12.0)], [(-30.0, -20.0), (-8.0, -12.0)]]]
FACE_X, FACE_R, CORE_R = 4.0 * K, 16.0 * K, 7.0 * K
HALO_R, HALO_SPAN = 76.0 * K, (112.0, 248.0)                     # degrees, behind the hood


def horn(opening):
    """The mouth: two lips from the hood's front. opening 0 = closed, 1 = singing."""
    reach, flare = K * (40.0 + 14.0 * opening), K * (4.0 + 24.0 * opening)
    return [[(30.0 * K, 14.0 * K), (reach, flare)], [(30.0 * K, -14.0 * K), (reach, -flare)]]


NUMBERS = dict(
    voices=VOICES,
    voiceRadius=VOICE_RADIUS,
    voiceHealth=260,
    notes=NOTES,
    midi=MIDI,
    damageScale={"singing": 1.0, "silent": 0.0, "lastVoice": 1.5},
    singing="a Voice's mouth is open from the step it fires until one beat later; shots ring off a closed hood",
    ring={
        "radius": RING,
        "radiusFugue": RING_FUGUE,
        "shrinkBars": 2,
        "turn": "one revolution every 16 bars; the direction reverses whenever a Voice dies",
        "centre": "the players' centroid when it arrives; it follows the centroid at 60 wu/s, kept 700 wu from every wall",
        "order": "Voice k stands at k x 45 degrees and owns note k of the scale, so the ring is the scale and opposite Voices are a fifth apart",
    },
    camera={"zoom": CAMERA_ZOOM, "note": "eased over the entrance bar, so the whole Round is in the frame from inside it; back to 1 over a bar at the end"},
    contact={"hoodHurts": "only while it sings",
             "note": ("a hood hurts by touch only while its mouth is open (from the step it fires until one beat later: the same window in which it can be hurt). "
                      "A silent hood is drawn at 0.6 of its level and a ship flies through it unharmed. The strings never hurt, except while the cage burns. "
                      "Ruled 2026-11-13: with every hood hurting, a ship that crosses the Round lost a cell eleven times a fight and the bot that does not dodge lost half its runs here; "
                      "the Choir's danger is its song, not its furniture")},
    bulletSpeed=BULLET_SPEED,
    bulletLifeSeconds=5.0,
    phases=[
        {"name": "Unison", "voicesAlive": [8, 7],
         "call": {"who": "the caller", "when": "beats 1 and 3", "bullets": 5, "pattern": "a fan at the nearest player, 11 degrees apart"},
         "response": {"who": "the Voice opposite the caller", "when": "beats 2 and 4", "bullets": 5, "pattern": "the same fan"},
         "caller": "moves one Voice clockwise every bar",
         "note": "fire comes from in front, then from behind: turn with the song"},
        {"name": "Canon", "voicesAlive": [6, 5],
         "call": {"who": "the caller", "when": "beat 1", "bullets": 5, "pattern": "a fan at the nearest player, 11 degrees apart"},
         "response": {"who": "every other live Voice, clockwise from the caller", "when": "one eighth after the Voice before it", "bullets": 3, "pattern": "a fan at the nearest player, 14 degrees apart"},
         "caller": "moves one live Voice clockwise every bar",
         "cage": {"every": "fourth bar"},
         "note": "the answer runs round the ring like a wave: move with it, not against it"},
        {"name": "Fugue", "voicesAlive": [4, 3],
         "call": {"who": "two callers, as far apart as the live Voices allow", "when": "every eighth of the bar", "bullets": 1, "pattern": "a stream that sweeps 80 degrees across the nearest player over the bar; the second caller sweeps the other way"},
         "response": {"who": "the other live Voices", "when": "the next bar", "bullets": 1, "pattern": "the same streams, mirrored"},
         "caller": "callers and answerers swap every bar",
         "cage": {"every": "second bar"},
         "note": "the Round has shrunk; two sweeps cross once a bar and the crossing is the gap"},
        {"name": "Solo", "voicesAlive": [2, 1],
         "orbit": {"radius": 280.0, "lapBars": 4, "note": "the ring is gone: the last Voices circle the nearest player"},
         "call": {"who": "each live Voice", "when": "beats 2 and 4", "bullets": 16, "pattern": "a ring; alternate rings turned half a step"},
         "heldNote": {"every": "fourth bar", "telegraphBars": 1, "sweepDegrees": 120, "sweepBeats": 2,
                      "note": "a dashed line for one bar, then a beam from the open mouth that sweeps on beats 1 and 2 of the next"},
         "note": "one voice left: it is hurt at 1.5x while it sings"},
    ],
    cage={
        "telegraphBars": 1,
        "burnBeats": 2,
        "rule": "the strings between neighbouring live Voices show danger dashes for one bar, then burn as beams for two beats from the next downbeat. A string needs both its Voices: every dead Voice opens two gaps in the cage",
    },
    deadVoice="its beat becomes a rest: nothing fires, nothing sounds. A dead caller is skipped; a dead answerer leaves the call unanswered",
    strings={
        "neighbour": {"widthRefPx": 1.6, "idleAmplitude": 5.0, "note": "the octagon: a thin line between neighbouring live Voices, coloured from one Voice's hue to the other's"},
        "call": {"widthRefPx": 2.6, "amplitude": 44.0, "antinodes": 3, "decayBeats": 1.0,
                 "note": "between a singing Voice and the Voice that will answer it: a standing wave drawn as its two envelopes, the singer's hue at one end, white in the middle, the answerer's hue at the other. Light only: it never hurts"},
    },
    music={
        "patch": "the pad patch as a voice: attack 20 ms, release one beat, reverb send 0.5; one note per sung step",
        "rule": "a Voice sings its own note whenever it fires, so the fight plays the scale: in Unison each bar is root, fifth, root, fifth, one degree higher every bar; in Canon the answer runs up the scale in eighths; every death takes a note out of the tune for good",
        "death": "the dying Voice's note falls a fourth over one beat into the reverb",
        "cage": "the telegraph tick on every beat of the telegraph bar, the impact sample on the burn",
        "heldNote": "the last Voice's note held for the whole telegraph bar, rising an octave as the beam fires",
    },
    hues={
        "rule": "eight hues 45 degrees apart round the OKLCH wheel (lightness 0.80, chroma 96% of the gamut), starting at the Track's first enemy hue; any hue within 25 degrees of the danger colour moves to the nearer edge of that band. This is the one boss that wears every colour",
        "pearl": {"oklchLightness": PEARL[0], "oklchHue": PEARL[1], "chromaFraction": PEARL[2], "use": "halos, faces and strings at rest"},
        "singing": "a singing Voice is drawn white-hot over its own hue with a two-arm flare across its mouth and a gold core: the gold is the weak point",
    },
    entrance="on the Drop's downbeat the Voices warp in clockwise, one every eighth, each singing its note as it lands: a rising scale. On bar 2 all eight sing together and the name card THE CHOIR holds for two bars; firing starts on bar 3",
    voiceDeath="the hood splits along its axis and falls outward; its two neighbour strings snap back to the neighbours with a white flash; the ring stops for one beat and turns the other way",
    death="the eight hoods return as outlines in their own hues for one bar and sing the last chord (D, A, D), every string white; then they shatter outward clockwise, one every eighth, each a large kill, the last as a boss break: three rings, 300+ sparks, the 30 px distortion ring, 50 ms hit-stop, then 220 Shards",
    coop="health x (1 + 0.5 x (players - 1)); fans aim at the nearest player to the singing Voice; in Solo each Voice circles a different player while two live",
)


def build():
    return {
        "note": "Generated by design/tools/boss_choir.py. World units. A Voice's shapes are local, +x toward the ring's centre; polylines are lists of points. The hood is closed by the mouth's two lips.",
        "bpm": BPM,
        "parts": {
            "voice": {
                "radius": VOICE_RADIUS,
                "glass": True,
                "hood": HOOD,
                "ribs": RIB,
                "mouthClosed": horn(0.0),
                "mouthOpen": horn(1.0),
                "face": {"x": FACE_X, "radius": FACE_R, "sides": 10},
                "core": {"x": FACE_X, "radius": CORE_R, "note": "gold while singing: the weak point"},
                "halo": {"radius": HALO_R, "fromDeg": HALO_SPAN[0], "toDeg": HALO_SPAN[1], "note": "an arc behind the hood, pearl; brightens with the Voice's own note"},
                "wavefronts": {"radii": [96.0, 135.0, 174.0], "halfAngleDeg": 26.0, "note": "three arcs leaving the open mouth, one beat to fade"},
            },
        },
        "numbers": NUMBERS,
    }


class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == "reef")
        self.glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
        self.font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
        lin = lambda e: np.asarray(e["linear"], np.float32)
        cands = [self.pal["enemyCandidates"][i] for i in self.pal["soloEnemies"]]
        self.white = np.ones(3, np.float32)
        self.accent = lin(self.pal["accent"])
        self.danger = lin(self.pals["constants"]["danger"])
        p = self.pals["constants"]["players"][0]
        self.p1, self.p1_deep, self.p1_light = lin(p), lin(p["deep"]), lin(p["light"])
        self.gold = lin(self.pals["constants"]["pickup"])
        self.grid = lin(self.pal["grid"])
        self.bg0, self.bg1 = lin(self.pal["bg0"]), lin(self.pal["bg1"])
        self.pearl = ld.oklch(PEARL[0], PEARL[1], cf=PEARL[2])[0].astype(np.float32)
        start = cands[0]["oklch"][2]
        danger_h = self.pals["constants"]["danger"]["oklch"][2]
        self.hues = []
        for k in range(VOICES):
            h = (start + 45.0 * k) % 360.0
            d = (h - danger_h + 180.0) % 360.0 - 180.0
            if abs(d) < 25.0:
                h = (danger_h + (25.0 if d >= 0 else -25.0)) % 360.0
            self.hues.append(h)
        self.voice = [ld.oklch(0.80, h, cf=0.96)[0].astype(np.float32) for h in self.hues]
        self.deep = [ld.oklch(0.58, h, cf=0.96)[0].astype(np.float32) for h in self.hues]


def preview(path):
    a = Art()
    W, H = 2400, 1080
    s = 1.5 * CAMERA_ZOOM
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.5, a.bg1 * 0.5, cx=0.5, cy=0.5)
    cv.nebula([(420, 250, 760, a.voice[2], 0.020), (1980, 830, 760, a.voice[6], 0.020), (1200, 540, 620, a.pearl, 0.012)])
    step_px = 60 * s
    grid = [(gx, 0, gx, H) for gx in np.arange(W / 2 % step_px, W + 1, step_px)] + [(0, gy, W, gy) for gy in np.arange(H / 2 % step_px, H + 1, step_px)]
    cv.lines(grid, a.grid, 0.15, 1.0)
    centre = (W / 2 + 150.0, H / 2)
    rot = math.radians(-112.5)
    ang = [rot + k * PI / 4 for k in range(VOICES)]
    pos = [(centre[0] + RING * s * math.cos(t), centre[1] + RING * s * math.sin(t)) for t in ang]
    caller, answerer = 7, 3            # the caller sang on beat 1; the Voice opposite draws breath for beat 2
    opening = {caller: 1.0, answerer: 0.35}

    # the octagon: neighbour strings, each from one Voice's hue to the next
    for k in range(VOICES):
        j = (k + 1) % VOICES
        (x0, y0), (x1, y1) = pos[k], pos[j]
        n = 40
        pts = []
        for i in range(n + 1):
            u = i / n
            off = math.sin(u * PI * 6) * 5.0 * s * math.sin(u * PI)
            dx, dy = x1 - x0, y1 - y0
            ln = math.hypot(dx, dy)
            pts.append((x0 + dx * u - dy / ln * off, y0 + dy * u + dx / ln * off))
        cv.lines(ld.polyline(pts), ld.f_linear(x0, y0, x1, y1, [a.voice[k], a.pearl, a.voice[j]]), 0.9, 1.6)

    # the call string: a standing wave across the Round between the singer and the Voice that will answer
    (x0, y0), (x1, y1) = pos[caller], pos[answerer]
    dx, dy = x1 - x0, y1 - y0
    ln = math.hypot(dx, dy)
    field = ld.f_linear(x0, y0, x1, y1, [a.voice[caller], a.white, a.voice[answerer]])
    for sign, level in ((1.0, 2.4), (-1.0, 2.4), (0.55, 1.0), (-0.55, 1.0), (0.0, 0.5)):
        pts = []
        for i in range(161):
            u = i / 160
            off = sign * 44.0 * s * math.sin(u * PI * 3) * (0.35 + 0.65 * math.sin(u * PI))
            pts.append((x0 + dx * u - dy / ln * off, y0 + dy * u + dx / ln * off))
        cv.lines(ld.polyline(pts), field, level, 2.6 if abs(sign) == 1.0 else 1.3)
    for u in (1 / 3, 2 / 3):          # the nodes glint
        cv.flare(x0 + dx * u, y0 + dy * u, 46 * s, a.white, 1.4, 1.1, 4, math.atan2(dy, dx))

    # the ship: inside the Round, pouring fire into the caller's open mouth
    sx, sy = centre[0] - 70 * s, centre[1] + 170 * s
    cxp, cyp = pos[caller]
    aim = math.atan2(cyp - sy, cxp - sx)

    bullets = []
    for k in range(VOICES):
        x, y = pos[k]
        face = ang[k] + PI
        o = opening.get(k, 0.0)
        hue, deep = a.voice[k], a.deep[k]
        tp = lambda pts: ld.transform_points(pts, x, y, s, face)
        tl = lambda pts, closed=False: ld.transform(ld.polyline(pts, closed=closed), x, y, s, face)
        field = ld.f_radial(x, y, 90 * s, [hue * 0.55 + a.white * 0.45, hue, deep])
        cv.glow(x, y, 150 * s, hue, 0.10 + 0.14 * o)
        halo = [(HALO_R * math.cos(math.radians(d)), HALO_R * math.sin(math.radians(d))) for d in np.linspace(HALO_SPAN[0], HALO_SPAN[1], 25)]
        cv.lines(tl(halo), a.pearl, 1.0 + 1.6 * o, 1.6)
        cv.fill(tp(HOOD), a.white if o == 1.0 else hue, 0.16 if o == 1.0 else 0.08)
        cv.lines(tl(HOOD), field, 2.8, 2.6)
        cv.lines(tl(HOOD), a.white, 0.9 + 1.8 * o, 0.9 + 0.7 * o)
        for rib in RIB:
            cv.lines(tl(rib), deep * 0.5 + hue * 0.5, 1.6, 1.4)
        for lip in horn(o):
            cv.lines(tl(lip), a.white if o > 0 else hue, 2.2 + 1.2 * o, 2.2)
        fx, fy = tp([(FACE_X, 0.0)])[0]
        cv.lines(ld.transform(ld.polygon(10), fx, fy, FACE_R * s), a.pearl, 1.6, 1.4)
        cv.discs([(fx, fy, CORE_R * s)], a.gold if o > 0 else hue * 0.6, 1.3 if o > 0 else 0.7)
        if o == 1.0:
            cv.glow(fx, fy, 60 * s, a.gold, 0.5)
            mx, my = tp([(50.0 * K, 0.0)])[0]
            cv.flare(mx, my, 120 * s, a.white, 1.9, 1.4, 2, face + PI / 2)
            for i, r in enumerate((96.0, 135.0, 174.0)):
                arc = [(30.0 * K + r * math.cos(t), r * math.sin(t)) for t in np.linspace(-math.radians(26), math.radians(26), 15)]
                cv.lines(tl(arc), ld.f_solid(hue * 0.5 + a.white * 0.5), 1.8 - 0.45 * i, 1.8)
        elif o > 0:
            cv.flare(fx, fy, 44 * s, a.white, 1.5, 1.1, 4, face)
        # the call's fans: beat 1's is a beat out; in Unison the answerer's last fan (beat 4 of the bar before) is two beats out
        if k == caller:
            to_ship = math.atan2(sy - y, sx - x)
            for i in range(-2, 3):
                t = to_ship + math.radians(11.0 * i)
                d = (VOICE_RADIUS + 0.55 * BEAT * BULLET_SPEED) * s
                bullets.append((x + math.cos(t) * d, y + math.sin(t) * d))
        if k == answerer:
            to_ship = math.atan2(sy - 30 - y, sx - 60 - x)
            for i in range(-2, 3):
                t = to_ship + math.radians(11.0 * i)
                d = (VOICE_RADIUS + 1.55 * BEAT * BULLET_SPEED) * s
                bullets.append((x + math.cos(t) * d, y + math.sin(t) * d))

    # ship and its fire
    hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
    r = 14 * s
    cv.glow(sx, sy, r * 6, a.p1, 0.13)
    cv.lines(ld.transform(hull, sx, sy, r, aim), ld.f_radial(sx, sy, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
    cv.lines(ld.transform(hull, sx, sy, r, aim), a.white, 2.0, 0.8)
    halo3 = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo3, sx, sy, r, 0.4), ld.f_angular(sx, sy, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)
    for lane in (-10.0, 0.0, 10.0):
        ox, oy = -math.sin(aim) * lane * s, math.cos(aim) * lane * s
        for u in np.arange(0.16, 0.9, 0.2):
            hx2, hy2 = sx + (cxp - sx) * u + ox, sy + (cyp - sy) * u + oy
            tx2, ty2 = hx2 - math.cos(aim) * 31 * s, hy2 - math.sin(aim) * 31 * s
            cv.lines([(tx2, ty2, hx2, hy2)], ld.f_linear(tx2, ty2, hx2, hy2, [a.p1, a.white]), 4.6, 2.2)
    mx, my = cxp + math.cos(ang[caller] + PI) * 60 * s, cyp + math.sin(ang[caller] + PI) * 60 * s
    for i in range(9):                                      # sparks off the open mouth
        an = aim + PI + (i - 4) * 0.24
        cv.lines([(mx + math.cos(an) * 12, my + math.sin(an) * 12, mx + math.cos(an) * (36 + 9 * (i % 3)), my + math.sin(an) * (36 + 9 * (i % 3)))], a.white, 2.2, 1.3)

    for bx, by in bullets:
        cv.threat(ld.transform(ld.polygon(4), bx, by, 7.5), a.danger * 0.75 + a.white * 0.25, 2.1, 1.5)

    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))
    note("THE CHOIR", 110, 60, 44, a.pearl, 1.6)
    note("BOSS OF TRACK 3  REEF  132 BPM", 110, 126, 14, a.accent, 1.0)
    note("EIGHT VOICES, EIGHT NOTES: THE RING IS THE SCALE", 110, 934, 13)
    note("ONE CALLS, THE ONE OPPOSITE ANSWERS A FIFTH AWAY", 110, 960, 13)
    note("A VOICE CAN ONLY BE HURT WHILE IT SINGS", 110, 986, 13, a.gold)
    note("KILL A VOICE AND ITS BEAT FALLS SILENT", 110, 1012, 13)
    for k in range(VOICES):
        lx = centre[0] + (RING - 128) * s * math.cos(ang[k])
        ly = centre[1] + (RING - 128) * s * math.sin(ang[k])
        note(NOTES[k][0], lx - 7, ly - 8, 16, a.voice[k], 1.3)
    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "choir.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "boss_choir.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
