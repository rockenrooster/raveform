"""Boss 6, Feedback: a tape loop that records each player's bar and plays it back from the far side.

Writes design/bosses/feedback.json (numbers) and design/previews/boss_feedback.png.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\boss_feedback.py
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

BPM = 150.0
BEAT = 60.0 / BPM
BAR = 4 * BEAT
STEP = BAR / 16                    # a sixteenth: 0.1 s
HEAD = 84.0                        # radius of the Head
LOOP = 420.0                       # radius of the tape loop (middle of the band)
BAND = 12.0                        # half the band's width
ECHO_SHOT_SPEED = 400.0
ECHO_SHOT_LIFE = 3.2               # seconds: two bars
PLAYER_SHOT_SPEED = 1400.0
CAMERA_ZOOM = 0.75
FAN_DEG = 18.0                     # the preview's build: three lanes

NUMBERS = dict(
    health=4800,
    headRadius=HEAD,
    loop={
        "radius": LOOP, "bandWidth": 2 * BAND, "steps": 16,
        "note": ("a ring of tape round the Head, one turn a bar, step 0 at twelve o'clock, clockwise. A gold head runs round it on the song's sixteenths. "
                 "As it passes a step it first reads what is there (the Echo plays it) and then writes what the player did in that step. "
                 "Each player has a lane of the tape in their colour; a shot is a bright tick. So the whole tape is always the next bar of echo, drawn before it happens. It does not hurt (until the third phase)"),
    },
    record=("for every player, every simulation tick of the last two bars: position relative to the Head, aim, primary shots fired (with the Mods they carried), Dash, Nova, and the Riffs and Boosts held. "
            "The distance from the Head is clamped to 220-900 wu when it is played back, so an Echo never sits on the Head or leaves the fight"),
    echo={
        "delayBars": 1,
        "place": "the player's recorded position reflected through the Head's centre; aim turned half a turn. Point reflection keeps the sense of rotation: circle clockwise and your Echo circles clockwise opposite you",
        "bodyRadius": 16.0,
        "contact": "an Echo's body takes a Shield cell; it cannot be hurt and player shots pass through it",
        "shots": {"speed": ECHO_SHOT_SPEED, "lifeSeconds": ECHO_SHOT_LIFE, "radius": 5.0, "streakLength": 22.0,
                  "note": ("one echo shot for every recorded shot, same lane, same Mods (Pierce, Split, Ricochet and the rest do to players and walls what they did to enemies), fired on the same sixteenth one bar later. "
                           "They fly at 400 wu/s, slower than the player's own, pass through the Head and through enemies, and take one Shield cell from any player they touch. A Nova erases them like bullets")},
        "riffs": ("the Echo carries its player's Riffs at the recorded levels, running one bar behind. What the player aimed is reflected (Lancer, Comet, Mirror); what aimed itself (Sentinel, Swarm, Storm Coil) aims at the nearest player; "
                  "what surrounds the ship surrounds the Echo (Orbitals, Pulse Ring, Sweeper, Flak). Everything they throw is a threat in the danger colour and takes one cell"),
        "boosts": "only Boosts that change shots or Riffs are echoed (Prism, Hyper, Supernova Rounds, Chorus, Beam, Ricochet Fever, Fractal). The others are the player's alone",
        "nova": "a Nova is echoed as a Null: one bar later a danger ring of the same radius bursts from the Echo and takes a cell from any player inside it",
        "dash": "echoed as movement only",
        "cap": "at most 500 echo shots alive. A player's bar holding more than 120 shots is played back with every second shot left out",
    },
    weakPoint=("the Head is open only while every Echo is silent. An Echo counts as playing from each echoed primary shot until two sixteenths after it; Riffs do not count. "
               "Open: the shutter is drawn back, the core is gold and every hit counts. Closed: the shutter is across, shots are swallowed with a dull thud and do nothing. "
               "So what you play now shuts the Head a bar from now: fire a phrase, then leave the same phrase empty in the next bar and use it to get out of the way of your own shots. "
               "The tape shows it ahead of time: where there are ticks the Head will be shut"),
    lesson="play, then listen. Never the same thing twice in a row",
    camera={"zoom": CAMERA_ZOOM, "note": "eased over the entrance bar; the whole Loop in the frame when the ship is inside it"},
    phases=[
        {"name": "Slapback", "when": "above 60% health",
         "echoes": "one for each player, one bar behind, at the mirror point",
         "note": "the widest safe rhythm is half and half: two beats on, two beats off, swapped in the next bar; or a bar on and a bar off"},
        {"name": "Regeneration", "when": "60% to 30%",
         "echoes": "two for each player: the first as before, the second two bars behind and reflected twice, which puts it exactly where the player stood two bars ago, firing as they fired",
         "note": "nowhere you have just been is safe to return to, and a phrase now shuts the Head twice: one bar in three is open"},
        {"name": "Howl", "when": "30% and below",
         "echoes": "the tape runs double speed: the two Echoes are two beats and four beats behind",
         "loop": {"shrinkPerBar": 15.0, "minRadius": 300.0, "hot": True,
                  "note": "the Loop burns and tightens by 15 wu on every bar line down to 300: touching it takes a cell, so a player is inside with the Head or outside with the long flight times, and crosses with a Dash"},
         "note": "two beats on, four beats off: a three-beat figure against the bar. The Head's tone rises through the whole phase"},
    ],
    damageScale={"open": 1.0, "closed": 0.0},
    contact="the Head and every Echo take a cell on touch; the Loop only in the third phase",
    crowd="half, fodder only; echo shots pass through enemies",
    reward={"shards": 320, "note": "a bonus Trio and 320 Shards for each player; Track 6 ends and the endless loop begins"},
    music={
        "echo": "every echo shot plays its player's own lead note as it was recorded: an octave down, low-passed at 1.2 kHz, 9 dB quieter and panned to the far side. The second Echo is 15 dB down and darker. It is a tape delay on the player's own playing, and it is the boss's music layer",
        "head": "open: the pad's filter opens and the Head holds the Track's root two octaves up, quietly; closed: the pad is ducked and dull. A swallowed shot is a closed hat",
        "health": "the held root rises a semitone for every 10% of health lost; in Howl the delay's own feedback is turned up so the echoes smear into each other",
        "note": "a player who rests hears the bar they just played come back alone: the rest is audible",
    },
    look={
        "head": "a loudspeaker seen from the front in the palette's electric blue: surround, ribbed cone and a shutter over the core. Open, the shutter's six blades are drawn back and the core is gold; closed, the blades meet and the core is dark. The cone pushes out 15% on every kick",
        "loop": "two thin white lines 24 wu apart with sixteen faint slots; ticks in the player's colour; the gold head carries a small flare. In Howl the lines turn to the danger colour on the priority layer",
        "echo": "the player's own ship drawn in the danger colour with a white core and three after-images, on the priority layer like every threat. A faint dashed line in the player's colour runs from the Echo through the Head to a small ring where the player stood one bar ago",
        "shots": "echo shots are short streaks in the danger colour with the threat rim: the player's shot, turned",
    },
    entrance=("on the Drop's downbeat the music cuts for one beat. The last bar the player fired comes back out of the dark, alone, from the far side: the first echo. "
              "The Loop draws itself once round over the next bar and the Head assembles inside it. The name card FEEDBACK holds for two bars; recording starts on bar 3 and the first Echo plays on bar 4"),
    death=("the tape is cut: the Loop opens at the head and unspools outward, its ticks flying off as sparks in the players' colours. Every Echo stops, turns from the danger colour to its player's own colour and flies home to its player as 100 Shards. "
           "The Head holds the Track's root for two bars while the screen blooms to white, then the boss break on the One: three rings, 300 sparks, the 30 px distortion ring, 50 ms of hit-stop, and the rest of the Shards"),
    coop=("health x (1 + 0.5 x (players - 1)); every player has their own Echoes and tape lane; damage is scaled by the share of living players' Echoes that are silent, so the band has to breathe together; "
          "echo shots hurt any player; a downed player's Echoes fade one bar later. Echoes are worked out on each device from its own view of each player's history (the local player's is exact), and a hit is decided by the device that owns the player, as always"),
)


def build():
    return {
        "note": "Generated by design/tools/boss_feedback.py. World units. The Head is a fixed point; everything else is the players' own play, one bar late.",
        "bpm": BPM,
        "parts": {
            "head": {"radius": HEAD, "coneRadius": 66.0, "coilRadius": 48.0, "coreRadius": 24.0, "ribs": 16, "shutterBlades": 6},
            "loop": {"radius": LOOP, "bandWidth": 2 * BAND, "steps": 16, "tickLength": 32.0, "widthRefPx": 1.2},
            "echo": {"bodyRadius": 16.0, "afterImages": 3, "afterImageBeats": 0.5},
        },
        "numbers": NUMBERS,
    }


class Art:
    def __init__(self):
        self.pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
        self.pal = next(p for p in self.pals["palettes"] if p["id"] == "zenith")
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
        self.blue = ld.oklch(0.72, 255.0, cf=0.96)[0].astype(np.float32)
        self.blue_deep = ld.oklch(0.48, 264.0, cf=0.96)[0].astype(np.float32)


def preview(path):
    a = Art()
    W, H = 2400, 1080
    s = 1.5 * CAMERA_ZOOM
    cv = ld.Canvas(W, H)
    cv.radial_bg(a.bg0 * 0.6, a.bg1 * 0.7, cx=0.54, cy=0.5)
    cv.nebula([(820, 300, 760, a.blue, 0.024), (1850, 800, 760, a.gold, 0.012)])
    step_px = 60 * s
    grid = [(gx, 0, gx, H) for gx in np.arange(W / 2 % step_px, W + 1, step_px)] + [(0, gy, W, gy) for gy in np.arange(H / 2 % step_px, H + 1, step_px)]
    cv.lines(grid, a.grid, 0.15, 1.0)

    hx, hy = W * 0.54, H * 0.5
    at = lambda ang, r: (hx + math.cos(ang) * r * s, hy + math.sin(ang) * r * s)
    R = 300.0                          # the ship keeps this far from the Head
    TURN = math.radians(30.0)          # the ship circles clockwise by this much a bar
    NOW = 12                           # the step the tape head is on
    th_now = math.radians(200.0)

    def player_angle(bars_back, step):
        """Where the ship was at `step` of the bar `bars_back` bars before this one."""
        return th_now - TURN * (bars_back + (NOW - step) / 16.0)

    # ---- the Loop: sixteen slots, this bar's ticks, the head --------------------------------
    for rr in (LOOP - BAND, LOOP + BAND):
        cv.lines(ld.transform(ld.polygon(128, 1.0, 0.0), hx, hy, rr * s), a.white, 0.55, 1.2)
    tape = lambda k: -PI / 2 + k * 2 * PI / 16
    for k in range(16):
        x0, y0 = at(tape(k), LOOP - BAND)
        x1, y1 = at(tape(k), LOOP + BAND)
        cv.lines([(x0, y0, x1, y1)], a.blue, 0.5, 1.0)
    for k in range(8, NOW):            # the ship has fired since step 8 of this bar
        x0, y0 = at(tape(k), LOOP - BAND - 4)
        x1, y1 = at(tape(k), LOOP + BAND + 4)
        cv.lines([(x0, y0, x1, y1)], ld.f_linear(x0, y0, x1, y1, [a.p1, a.white, a.p1]), 3.4, 4.0)
        cv.glow((x0 + x1) / 2, (y0 + y1) / 2, 40 * s, a.p1, 0.18)
    mx, my = at(tape(NOW), LOOP)
    ox, oy = at(tape(NOW), LOOP + BAND + 34)
    wing = 0.035
    cv.lines(ld.polyline([at(tape(NOW) - wing, LOOP + BAND + 34), at(tape(NOW), LOOP + BAND + 6), at(tape(NOW) + wing, LOOP + BAND + 34)], closed=True), a.gold, 3.0, 2.4)
    cv.lines([(at(tape(NOW), LOOP - BAND - 8) + at(tape(NOW), LOOP + BAND + 6))], a.gold, 3.0, 2.6)
    cv.flare(mx, my, 80 * s, a.gold, 1.6, 1.3, 4, tape(NOW))
    cv.glow(mx, my, 70 * s, a.gold, 0.22)

    # ---- the Echo: the ship of one bar ago, reflected through the Head -----------------------
    hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
    r_ship = 14 * s
    e_ang = player_angle(1, NOW) + PI
    ex, ey = at(e_ang, R)
    e_aim = math.atan2(hy - ey, hx - ex)
    was_x, was_y = at(player_angle(1, NOW), R)
    # the mirror line: Echo, Head, where the ship stood a bar ago
    ln = math.hypot(was_x - ex, was_y - ey)
    ux, uy = (was_x - ex) / ln, (was_y - ey) / ln
    dashes = [(ex + ux * d, ey + uy * d, ex + ux * (d + 14), ey + uy * (d + 14)) for d in np.arange(30, ln - 30, 26)]
    cv.lines(dashes, a.p1, 0.45, 1.2)
    cv.lines(ld.transform(ld.polygon(32, 1.0, 0.0), was_x, was_y, 20 * s), a.p1, 0.9, 1.4)
    cv.lines(ld.transform(hull, was_x, was_y, r_ship * 0.9, math.atan2(hy - was_y, hx - was_x)), a.p1, 0.35, 1.0)
    for k, lev in ((3, 0.25), (2, 0.4), (1, 0.6)):       # after-images: an eighth of a beat apart
        ga = player_angle(1, NOW - k * 0.9) + PI
        gx_, gy_ = at(ga, R)
        cv.lines(ld.transform(hull, gx_, gy_, r_ship, math.atan2(hy - gy_, hx - gx_)), a.danger, lev, 1.2)
    cv.glow(ex, ey, r_ship * 6, a.danger, 0.14)
    cv.threat(ld.transform(hull, ex, ey, r_ship, e_aim), a.danger * 0.7 + a.white * 0.3, 2.4, 1.5)
    for k in range(2):                 # its Orbitals: the ship's own Riff, turned
        oa = 0.9 + k * PI
        qx, qy = ex + math.cos(oa) * 62 * s, ey + math.sin(oa) * 62 * s
        cv.threat(ld.transform(ld.polygon(4, 1.0, oa), qx, qy, 9 * s), a.danger * 0.75 + a.white * 0.25, 2.0, 1.4)
    cv.lines(ld.transform(ld.polygon(48, 1.0, 0.0), ex, ey, 62 * s), a.danger, 0.22, 1.0)

    # ---- the echo's shots: last bar's steps 0-7, fired through the Head at where the ship was --
    echo_shots = []
    for j in range(0, 8):
        age = (NOW - j) * STEP
        src = player_angle(1, j) + PI
        sx0, sy0 = at(src, R)
        base = math.atan2(hy - sy0, hx - sx0)
        for lane in (-FAN_DEG, 0.0, FAN_DEG):
            d = base + math.radians(lane)
            dist = ECHO_SHOT_SPEED * age * s
            px, py = sx0 + math.cos(d) * dist, sy0 + math.sin(d) * dist
            echo_shots.append((px - math.cos(d) * 22 * s, py - math.sin(d) * 22 * s, px, py))

    # ---- the Head, open: nothing is echoing right now ---------------------------------------
    cv.glow(hx, hy, 330 * s, a.blue, 0.16)
    circle = lambda rr, n=64: ld.transform(ld.polygon(n, 1.0, 0.0), hx, hy, rr * s)
    cv.fill([at(t, HEAD) for t in np.linspace(0, 2 * PI, 48, endpoint=False)], a.blue_deep, 0.10)
    cv.lines(circle(HEAD), ld.f_radial(hx, hy, HEAD * s, [a.white, a.blue, a.blue_deep]), 3.0, 3.0)
    cv.lines(circle(HEAD), a.white, 1.4, 1.0)
    cv.lines(circle(66.0), a.blue, 2.2, 2.0)
    cv.lines(circle(48.0), a.blue * 0.6 + a.white * 0.4, 2.4, 2.2)
    ribs = [at(t, 48.0) + at(t, 66.0) for t in np.linspace(0, 2 * PI, 16, endpoint=False)]
    cv.lines(ribs, a.blue, 1.0, 1.2)
    for k in range(6):                 # the shutter's blades, drawn back against the coil
        t0 = k * PI / 3 + 0.2
        cv.lines([at(t0, 45.0) + at(t0 + 0.8, 31.0)], a.white, 2.4, 2.2)
    cv.lines(circle(24.0, 32), a.gold, 3.2, 2.6)
    cv.discs([(hx, hy, 13 * s)], a.gold * 0.7 + a.white * 0.3, 2.6)
    cv.glow(hx, hy, 120 * s, a.gold, 0.42)
    cv.flare(hx, hy, 150 * s, a.gold * 0.6 + a.white * 0.4, 1.5, 1.3, 4, PI / 4)

    # ---- the ship: moved on, and firing now that the Head is open ----------------------------
    sx, sy = at(th_now, R)
    aim = math.atan2(hy - sy, hx - sx)
    trail = [at(player_angle(1, NOW) + (th_now - player_angle(1, NOW)) * u, R) for u in np.linspace(0.0, 1.0, 24)]
    cv.lines(ld.polyline(trail), a.p1, 0.5, 1.4)
    cv.glow(sx, sy, r_ship * 6, a.p1, 0.15)
    cv.lines(ld.transform(hull, sx, sy, r_ship, aim), ld.f_radial(sx, sy, r_ship * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
    cv.lines(ld.transform(hull, sx, sy, r_ship, aim), a.white, 2.0, 0.8)
    halo3 = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
    cv.lines(ld.transform(halo3, sx, sy, r_ship, 0.4), ld.f_angular(sx, sy, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)
    for k in range(2):
        oa = 0.9 + k * PI + PI
        qx, qy = sx + math.cos(oa) * 62 * s, sy + math.sin(oa) * 62 * s
        cv.lines(ld.transform(ld.polygon(4, 1.0, oa), qx, qy, 9 * s), ld.f_radial(qx, qy, 9 * s, [a.white, a.p1]), 2.6, 1.8)
    cv.lines(ld.transform(ld.polygon(48, 1.0, 0.0), sx, sy, 62 * s), a.p1, 0.22, 1.0)
    for j in range(8, NOW):
        age = (NOW - j) * STEP
        src = player_angle(0, j)
        px0, py0 = at(src, R)
        base = math.atan2(hy - py0, hx - px0)
        for lane in (-FAN_DEG, 0.0, FAN_DEG):
            dist_wu = PLAYER_SHOT_SPEED * age
            if lane == 0.0 and dist_wu > R - HEAD:
                continue                                   # the centre lane has struck the open Head
            d = base + math.radians(lane)
            px, py = px0 + math.cos(d) * dist_wu * s, py0 + math.sin(d) * dist_wu * s
            tx, ty = px - math.cos(d) * 31 * s, py - math.sin(d) * 31 * s
            cv.lines([(tx, ty, px, py)], ld.f_linear(tx, ty, px, py, [a.p1, a.white]), 4.6, 2.2)
    fx, fy = hx - math.cos(aim) * HEAD * s, hy - math.sin(aim) * HEAD * s
    for i in range(11):
        an = aim + PI + (i - 5) * 0.22
        r0, r1 = 8, 36 + 12 * (i % 3)
        cv.lines([(fx + math.cos(an) * r0, fy + math.sin(an) * r0, fx + math.cos(an) * r1, fy + math.sin(an) * r1)], a.gold * 0.55 + a.white * 0.45, 2.4, 1.3)
    cv.glow(fx, fy, 60 * s, a.gold, 0.35)

    for seg in echo_shots:
        cv.threat([seg], a.danger * 0.75 + a.white * 0.25, 2.8, 1.5)

    note = lambda text, x, y, cap=13, colour=None, level=1.0: cv.text(a.font, text, x, y, cap, a.white if colour is None else colour, level, max(1.3, cap / 9.0))
    note("FEEDBACK", 110, 60, 44, a.white, 1.6)
    note("BOSS OF TRACK 6  ZENITH  150 BPM", 110, 126, 14, a.accent, 1.0)
    note("THE LOOP RECORDS YOUR BAR: EVERY SHOT IS A TICK ON THE TAPE", 110, 934, 13, a.p1)
    note("ONE BAR LATER YOUR ECHO PLAYS IT BACK FROM THE FAR SIDE", 110, 960, 13, a.danger * 0.7 + a.white * 0.3)
    note("THE HEAD IS OPEN ONLY WHILE YOUR ECHO IS SILENT", 110, 986, 13, a.gold)
    note("PLAY, THEN LISTEN", 110, 1012, 13)
    note("ECHO", ex + 44, ey - 70, 16, a.danger * 0.7 + a.white * 0.3, 1.3)
    note("ONE BAR AGO", was_x - 190, was_y + 34, 12, a.p1, 0.9)
    note("OPEN", hx + 124, hy + 96, 14, a.gold, 1.3)
    note("REC", ox - 84, oy - 8, 14, a.gold, 1.3)
    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "feedback.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "boss_feedback.png")
    preview(out)
    print("wrote", out)


if __name__ == "__main__":
    main()
