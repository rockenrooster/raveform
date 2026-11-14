"""The bosses of Tracks 7 to 10: Cadence, Arpeggiator, Fata Morgana and Kaleidos, Reprise.

Writes design/bosses/<id>.json and <id>.md for each (the page is made from the data, so the two cannot
disagree) and design/previews/bosses_7_10.png (four panels).
Run: . .\\scripts\\env.ps1; python -I design\\tools\\bosses_7_10.py [nopicture]
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

COMMON = {
    "arrives": "on the last Drop of its Track and stays until it is dead; the music holds its Drop for the whole fight",
    "health": "the number given is a first guess: tune it with the careful bot until the fight lasts what the page says with the build a player has by then, as was done for Longwave and The Choir; x (1 + 0.5 per extra player)",
    "grid": "everything it does lands on the song's grid and is computed from the song's clock, so every device in a co-op run sees the same fight with nothing new on the wire",
    "reward": "a bonus Trio and Shards for each player; the Track ends",
    "camera": "holds 0.85 for the fight (GAME_DESIGN 9)",
    "crowd": "half of the Drop's crowd, fodder only, unless the page says otherwise",
}

BOSSES = [
    dict(
        id="cadence", name="Cadence", track=7, trackName="Afterglow", bpm=156, key="D flat major: D flat, A flat, B flat minor, G flat (I, V, vi, IV)",
        line="The boss is the chord progression. Four great bodies stand round the players, one for each chord; the one whose chord the band is playing is silent and open, and the other three fire.",
        lessons=["The harmony tells you where to aim. The open body changes on every bar line, in the order of the song: I, V, vi, IV, round the square.",
                 "Half time is room. The three that are not sounding fire only on one and three, slowly; the way toward the open body is always clear of its own fire.",
                 "In the last phase the rule turns over, and the music says so."],
        parts=[
            ("Pillar (x 4)", "A ring of twelve petals, radius 90, with its numeral inside in the stroke font (cap 60 wu): I, V, vi, IV. Each has its own hue from the palette: I the peach-gold, V the lavender, vi the periwinkle, IV the mint. Sounding: its petals spread to radius 120, its numeral is white at 2.6 and a pool of its hue lies under it. Silent: petals closed, numeral at 0.8"),
            ("The square", "The four stand on the corners of a square 700 wu on a side, in the song's order clockwise from the top left (I, V, vi, IV); a thin line in the grid's colour joins them, and the side leading to the next chord brightens over the bar"),
        ],
        numbers={"health": 5200, "pillarRadius": 90.0, "squareSide": 700.0, "follow": "the square's centre follows the players' centre at 40 wu/s", "fanBullets": 5, "fanSpreadDegrees": 40.0, "bulletSpeed": 180.0, "bulletLifeSeconds": 4.0,
                 "fightSeconds": [80, 100], "rewardShards": 300},
        phases=[
            ("Progression", "100 to 66%", ["The pillar of the bar's chord is open and silent; it is the only one that can be hurt. On the One of its bar it takes x 2 for the beat",
                                             "The other three each fire a fan of five at the nearest player on beats 1 and 3 (the half-time kick and snare), 180 wu/s",
                                             "So the open pillar goes round the square one corner a bar, clockwise"]),
            ("Modulation", "66 to 33%", ["On every fourth bar line, as the song comes back to I, the square turns a quarter turn the other way in one beat, so the next I is where IV stood: the numerals, not the corners, say who is next",
                                           "Each firing pillar adds a second fan of seven on the snare"]),
            ("Inversion", "33 to 0%", ["The rule turns over, announced by the word INVERSION and a bar of the bed high-passed: now the sounding pillar is closed and it alone fires, a rosette of sixteen on the One and twelve on three; the other three are open and silent",
                                         "Three targets at once: the fight ends quickly from here, as a last chorus should"]),
        ],
        weak="the open pillar, and x 2 on the One of its bar",
        death="the progression resolves: the four burst in the song's order on the four beats of the last bar and the band holds the tonic chord over a gong",
        sound=["Each pillar is its chord: while it sounds, a pad voice holds its root and the band's stab plays that chord on the One at 1.2",
               "Each firing pillar's fan is a tom on its own chord's root, on one and three: three toms, a slow melody of the chords that are not playing",
               "The turn of the square is a reversed cymbal into the bar line; Inversion is the music bus high-passed at 800 Hz for one bar",
               "Death: the four chords in turn, then the tonic with the gong"],
        caps="four bodies, 120 bullets",
    ),
    dict(
        id="arpeggiator", name="Arpeggiator", track=8, trackName="Arcadia", bpm=162, key="G major: G, E minor, C, D",
        line="The bullets are the tune. Four lanes run from a machine across the frame and the lead's broken chord comes down them as blocks, so the player weaves the shape of the arpeggio they have been firing all Track.",
        lessons=["The pattern is the melody you already know: it climbs and falls with the chord, a lane at a time.",
                 "The next bar is written above the lanes before it is played.",
                 "It is open while it loads a bar: on the One."],
        parts=[
            ("The Machine", "A console 900 wu wide and 120 deep with a core of radius 70 at its middle behind a shutter, four ports along its lower edge (one for each lane) and above it the chart: a matrix of 4 by 16 dots, gold, 400 by 60 wu, the lit dots the next bar's notes and a white cursor crossing it in time"),
            ("Lanes", "Four lanes 200 wu wide and 900 long running from the ports. Drawn as five faint lines in the grid's colour with the lane's number at the far end"),
            ("Blocks", "A note: a danger-coloured bar 90 wu wide and 28 deep on the priority layer, falling along its lane's middle at 420 wu/s. It cannot be shot; a Nova clears the lanes"),
        ],
        numbers={"health": 6400, "coreRadius": 70.0, "laneWidth": 200.0, "laneLength": 900.0, "blockWidth": 90.0, "blockSpeed": 420.0,
                 "place": "380 wu from the players' centre on its side of them; it follows them along its own axis at 120 wu/s and its lanes move with it",
                 "laneOfNote": "rank the distinct pitches of the bar's arpeggio from low to high; a note's lane is (its rank mod 4) + 1. For G that is 1 2 3 4 3 2 3 4 1 4 3 4 2 1 4 3",
                 "fightSeconds": [90, 110], "rewardShards": 340},
        phases=[
            ("Quarter notes", "100 to 66%", ["The machine stands above the players and plays the accented notes of the lead: steps 0, 4, 8 and 12 of each bar, one block each, in the lane of its note",
                                               "Its shutter opens for the first beat of every bar as it loads the next chart: x 2 on the core for that beat; x 1 on the core at other times; the console sheds shots"]),
            ("Eighth notes", "66 to 33%", ["Every second step: eight blocks a bar",
                                             "Every fourth bar it moves to the next side of the players (above, right, below, left); its new lanes are drawn dashed for the bar before it moves"]),
            ("Two hands", "33 to 0%", ["A second machine, white, plays from the side at right angles to the first, so the lanes cross in a grid of sixteen cells",
                                         "The first plays the lead's quarter notes; the second plays the chord stab's rhythm (steps 0, 3, 6, 9, 12, 14), each hit in the lane of the bar's chord tone in turn",
                                         "Either core can be hurt; they share the health"]),
        ],
        weak="the core, x 2 for the first beat of each bar",
        death="the chart reads GG, every block in flight turns gold and climbs back up its lane as Shards, and the machine plays the arpeggio's last run upward",
        sound=["Each block sounds its own note, on the P3 bell at 0.3, as it passes the row the nearest player is in: with fire held, the blocks and the player's lead are the same tune in unison",
               "The shutter is a woodblock on the One with a sweep opening over the beat",
               "A change of side is a whoosh over the bar before; the second machine arrives with the chord's stab",
               "Death: Shard chirps running up the scale over two beats"],
        caps="two machines, 48 blocks",
    ),
    dict(
        id="morgana", name="Fata Morgana", track=9, trackName="Mirage", bpm=168, key="B flat lydian: B flat major 7, C over B flat, G minor 7, F add 9",
        line="The safe place moves. A storm closes the frame and the only calm is an oasis that drifts; later there are two, and one of them is not there.",
        lessons=["Stay in the calm, and the calm does not stay still.",
                 "A mirage blinks on the snare. The real water never blinks.",
                 "It crosses the water every fourth bar: that is when it can be caught."],
        parts=[
            ("The storm", "Everything outside the oasis. It hurts on the kick: a ship outside the calm when a kick lands (this Track's broken kick: steps 0, 6 and 10) loses a cell. Drawn as fifteen streaks of sand in the danger colour at 0.8 streaming across the frame, each drawn as a lethal line is (look.md section 5: a 4 wu capsule with the dark rim, a white-pink axis and a glow), flaring on every kick, when the storm hurts; with the lattice whipped 10 wu along the wind; never a fill (the owner, 2026-11-14: \"thin red lines streaking across the screen ... too small, not visible enough and are too frequent, tune down to like 0.25x\"; sixty thin ones at 0.5 until then). The streaks hurt nobody: the storm does, outside the oasis, on the kick"),
            ("The oasis", "A disc of calm 260 wu in radius. Its edge is a ring in the palette's accent at 2.2 with a second ring 12 wu inside it that swells on the beat; inside, the lattice is at rest and tinted with the accent"),
            ("Morgana", "A body of radius 76 made of three veils (broken arcs) round a white core, turning against each other. She rides the storm just outside the oasis's edge"),
        ],
        numbers={"health": 7600, "oasisRadius": 260.0, "bodyRadius": 76.0, "drift": "the oasis drifts along a figure of eight 900 wu wide at 110 wu/s",
                 "orbit": "Morgana circles the oasis 70 wu outside its edge, one turn every four bars", "fanBullets": 5, "bulletSpeed": 200.0,
                 "fightSeconds": [90, 120], "rewardShards": 380},
        phases=[
            ("Drift", "100 to 66%", ["The oasis drifts; the player walks with it. On each kick she fires a fan of five into the calm at the nearest player: three fans a bar, in the kick's own rhythm",
                                      "Every fourth bar the oasis rests for the bar and she crosses it from side to side: x 2 while she is over the water; x 1 at other times"]),
            ("Mirage", "66 to 33%", ["At every fourth bar line a second oasis appears 500 wu from the first. One is real. The mirage goes out for an eighth on every snare (two and four) and after two bars it is gone; a ship in it is then in the storm",
                                       "Which one is real is drawn by the seed. The real one never blinks"]),
            ("Crossing", "33 to 0%", ["Every second bar the oasis leaps: the next one is drawn dashed 600 wu away for the bar before; on the bar line the old one shrinks to nothing over one bar while a corridor of calm 160 wu wide joins the two",
                                        "She leaps with it and lands dazzled: x 2 for the first beat after each leap"]),
        ],
        weak="x 2 while she crosses the resting oasis (one bar in four), and for the beat after each leap in the last phase",
        death="the storm drops at once: the whole frame is calm, the lattice takes the oasis's tint from edge to edge, and the sand falls as Shards",
        sound=["The storm is a crackle and a whoosh that are heard outside the oasis; outside it the music bus is also low-passed at 2 kHz, so the calm is where the song sounds whole",
               "A mirage's blink is a soft tick on the snare", "The leap is the lead's arpeggio as a run of chirps, up",
               "While she is over the water, a bell on every beat"],
        caps="one body, 90 bullets, 15 streaks of sand (light only; 60 until 2026-11-14)",
    ),
    dict(
        id="reprise", name="Kaleidos, Reprise", track=10, trackName="Last Light", bpm=174, key="A minor: First Light's chords and lead",
        line="The first boss comes back as the first theme does. Six arms, and each one plays one boss the player has already beaten.",
        lessons=["You have fought all of this before. Each arm's solo is one earlier fight in two bars.",
                 "Shoot the soloist off and its tune is gone for good.",
                 "The last pattern is the first song."],
        parts=[
            ("The wheel", "Kaleidos as built, a quarter larger: a core of radius 80 and six arms 300 wu long. Each arm takes the accent colour of the Track whose boss it plays: Ultraviolet, Solar, Reef, Aurora, Furnace, Zenith"),
            ("The soloist", "The arm whose turn it is: at 3.0 with its white core line, a pool of its colour under it, the other arms at 1.0"),
        ],
        numbers={"health": 9000, "coreRadius": 80.0, "armLength": 300.0, "turn": "one revolution every eight bars; it keeps 450 to 650 wu from the nearest player as Kaleidos does",
                 "armShare": "in the Medley each arm has a sixth of that phase's health", "fightSeconds": [110, 140], "rewardShards": 500},
        phases=[
            ("Reprise", "100 to 70%", ["Kaleidos's own fight at 174: mirrored fans of five from each pair of arms, now on the two-step's hits (kick on steps 0 and 10, snare on 4 and 12) instead of the eighths",
                                        "The arms cannot be hurt yet (they are white-edged); the core is open, x 1, and x 2 on the One"]),
            ("Medley", "70 to 20%", ["The core closes. The arms take turns round the wheel, two bars each; the soloist is the only thing that can be hurt, and a dead arm's solo is skipped from then on",
                                      "Kaleidos's arm: its mirrored fans on the eighths",
                                      "Longwave's arm: it lets go as a wave of eight segments that crosses the frame once; only the crests can be hurt",
                                      "The Choir's arm: a volley on the One, answered on three by the arm opposite",
                                      "Event Horizon's arm: for its two bars ships and bullets are drawn toward the wheel at 200 wu/s, and a ring of twenty-four leaves the core on its second bar line",
                                      "Polyrhythm's arm: two rings a bar with a gap each, one on three pulses and one on four; the gaps meet on the One",
                                      "Feedback's arm: an Echo of the nearest player replays that player's last bar from the far side of the wheel"]),
            ("Last Light", "20 to 0%", ["The bare core. It fires First Light's arpeggio: one bullet on every sixteenth, its direction 15 degrees round for every semitone the note stands above the bar's lowest, the whole figure turning 45 degrees a bar, 260 wu/s",
                                         "x 1, and x 2 on the One. Over this phase the palette cross-fades from Last Light to Ultraviolet"]),
        ],
        weak="the core on the One in the first and last phases; the soloist in the Medley",
        death="no burst: the core dims to a point, one white ripple crosses the whole lattice, and the ENCORE card comes up over First Light. The largest reward of the run",
        sound=["Each solo is in its boss's own voice: Kaleidos's bells, Longwave's rubber string, a pad voice for The Choir, the sub drop for Event Horizon, toms three against four for Polyrhythm, the player's lead reversed for Feedback",
               "A dead arm takes its voice out of the medley",
               "The last pattern is First Light's arpeggio on the P3 bell, doubled by the band's lead: the tune of the first minute, at 174",
               "Death: the tonic held, the ripple a reversed cymbal ending on the encore's first downbeat"],
        caps="one wheel, eight wave segments, one Echo, 200 bullets",
    ),
]


def page(b):
    n = b["numbers"]
    md = ["# %s — boss of Track %d, %s" % (b["name"].upper(), b["track"], b["trackName"]), "",
          b["line"], "",
          "%d BPM, %s. Data: `design/bosses/%s.json` (load it, do not retype it). Picture: `design/previews/bosses_7_10.png`." % (b["bpm"], b["key"], b["id"]),
          "Tool: `design/tools/bosses_7_10.py`. It arrives %s." % COMMON["arrives"], "",
          "What it teaches:", ""]
    md += ["%d. %s" % (i + 1, t) for i, t in enumerate(b["lessons"])]
    md += ["", "---", "", "## Parts", "", "| Part | What it is |", "|---|---|"]
    md += ["| **%s** | %s |" % (a, t) for a, t in b["parts"]]
    md += ["", "## Phases", "", "| Phase | Health | What happens |", "|---|---|---|"]
    for name, share, rules in b["phases"]:
        md.append("| **%s** | %s | %s |" % (name, share, "<br>".join(rules)))
    md += ["", "## Numbers", "", "| | |", "|---|---|"]
    for k, v in n.items():
        if k == "fightSeconds":
            md.append("| Fight length to tune to | %d to %d seconds |" % (v[0], v[1]))
        elif k == "rewardShards":
            md.append("| Reward | A bonus Trio and %d Shards for each player; the Track ends |" % v)
        elif k == "health":
            md.append("| Health | %s to start from. %s |" % (format(v, ","), COMMON["health"][0].upper() + COMMON["health"][1:]))
        else:
            md.append("| %s | %s |" % (k, v))
    md += ["| Weak point | %s |" % (b["weak"][0].upper() + b["weak"][1:]),
           "| Crowd | %s |" % (COMMON["crowd"][0].upper() + COMMON["crowd"][1:]),
           "| Camera | %s |" % (COMMON["camera"][0].upper() + COMMON["camera"][1:]),
           "| Caps | %s |" % (b["caps"][0].upper() + b["caps"][1:]),
           "| On the grid | %s |" % (COMMON["grid"][0].upper() + COMMON["grid"][1:]),
           "", "## Death", "", b["death"][0].upper() + b["death"][1:] + ".", "", "## Sound", ""]
    md += ["- %s." % s_ for s_ in b["sound"]]
    md += ["", "In the words of `design/music/sound_m5.json`'s recipes; a sound they cannot express is reported, not improvised.", ""]
    return "\n".join(md)


def preview(path):
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    lin = lambda e: np.asarray(e["linear"], np.float32)   # noqa: E731
    pal = {p["id"]: p for p in pals["palettes"]}
    white, gold = np.ones(3, np.float32), lin(pals["constants"]["pickup"])
    danger = lin(pals["constants"]["danger"])
    pl = pals["constants"]["players"][0]
    p1, p1_deep, p1_light = lin(pl), lin(pl["deep"]), lin(pl["light"])
    glyphs = {g["id"]: g for g in json.load(open(os.path.join(DESIGN, "glyphs.json"), encoding="utf-8"))["glyphs"]}
    W, H = 2400, 1080
    PW, PH = W // 2, H // 2
    cv = ld.Canvas(W, H)
    cv.radial_bg(lin(pal["afterglow"]["bg0"]) * 0.5, lin(pal["afterglow"]["bg1"]) * 0.4)
    cv.lines([(PW, 20, PW, H - 20), (20, PH, W - 20, PH)], white, 0.35, 1.2)
    note = lambda t, x, y, cap=13, col=white, lev=1.0: cv.text(font, t, x, y, cap, col, lev, max(1.3, cap / 9.0))   # noqa: E731
    circle = lambda x, y, r, n=64: ld.transform(ld.polygon(n, 1.0, 0.0), x, y, r)   # noqa: E731

    def ship(x, y, aim, s=1.25):
        r = 14 * s
        hull = [(g[0], -g[1], g[2], -g[3]) for g in glyphs["ship"]["segments"]]
        cv.glow(x, y, r * 6, p1, 0.15)
        cv.lines(ld.transform(hull, x, y, r, aim), ld.f_radial(x, y, r * 1.3, [white, p1, p1_deep]), 2.8, 2.0)
        cv.lines(ld.transform(hull, x, y, r, aim), white, 2.0, 0.8)
        halo = [(g[0], -g[1], g[2], -g[3]) for g in glyphs["halo_3"]["segments"]]
        cv.lines(ld.transform(halo, x, y, r, 0.4), ld.f_angular(x, y, [p1, white, p1, p1_light]), 1.5, 1.5)

    def bullet(x, y, r=6.0):
        cv.threat(ld.transform(ld.polygon(4, 1.0, 0.0), x, y, r), danger * 0.75 + white * 0.25, 2.0, 1.4)

    def title(x0, y0, name, l1, l2, col):
        note(name, x0 + 44, y0 + 34, 28, white, 1.5)
        note(l1, x0 + 44, y0 + PH - 62, 12, col)
        note(l2, x0 + 44, y0 + PH - 38, 12, gold)

    def grid(x0, y0, col, step=56):
        gs = [(gx, y0 + 8, gx, y0 + PH - 8) for gx in np.arange(x0 + step / 2, x0 + PW, step)] + [(x0 + 8, gy, x0 + PW - 8, gy) for gy in np.arange(y0 + step / 2, y0 + PH, step)]
        cv.lines(gs, col, 0.12, 1.0)

    # ---- 1: Cadence (Afterglow): V sounds, the other three fire ---------------------------------
    x0, y0 = 0, 0
    a = pal["afterglow"]
    grid(x0, y0, lin(a["grid"]))
    hues = [lin(a["enemyCandidates"][i]) for i in (0, 1, 2, 3)]
    cx, cy, half = x0 + 600, y0 + 236, 168
    corners = [(cx - half, cy - half), (cx + half, cy - half), (cx + half, cy + half), (cx - half, cy + half)]
    names = ["I", "V", "VI", "IV"]
    cv.lines(ld.polyline(corners, closed=True), lin(a["grid"]) * 0.6 + white * 0.4, 0.5, 1.2)
    cv.lines([(corners[1][0], corners[1][1], corners[2][0], corners[2][1])], white, 1.2, 1.6)        # the side to the next chord
    sx, sy = cx + 20, cy + 10
    for i, (px, py) in enumerate(corners):
        sounding = i == 1
        r = 62 if sounding else 46
        for kpet in range(12):
            an = kpet * PI / 6
            cv.lines(ld.transform(ld.arc(1.0, an + 0.08, an + PI / 6 - 0.08, 4), px, py, r), hues[i], 2.8 if sounding else 1.6, 2.4 if sounding else 1.8)
        if sounding:
            cv.glow(px, py, 170, hues[i], 0.2)
            cv.lines(circle(px, py, 34, 32), white, 1.0, 1.2)
        note(names[i], px - len(names[i]) * 9, py - 13, 26, white, 2.6 if sounding else 0.8)
        if not sounding:
            an0 = math.atan2(sy - py, sx - px)
            for f in (-0.35, -0.17, 0.0, 0.17, 0.35):
                for d in (95, 165):
                    bullet(px + math.cos(an0 + f) * d, py + math.sin(an0 + f) * d)
    aim = math.atan2(corners[1][1] - sy, corners[1][0] - sx)
    ship(sx, sy, aim)
    for d in (44, 88, 132):
        hx, hy = sx + math.cos(aim) * d, sy + math.sin(aim) * d
        cv.lines([(hx - math.cos(aim) * 24, hy - math.sin(aim) * 24, hx, hy)], ld.f_linear(hx - math.cos(aim) * 24, hy - math.sin(aim) * 24, hx, hy, [p1, white]), 4.6, 2.2)
    title(x0, y0, "CADENCE", "THE PILLAR WHOSE CHORD IS PLAYING IS OPEN AND SILENT. THE OTHER THREE FIRE ON ONE AND THREE", "IT GOES ROUND THE SQUARE A CORNER A BAR: I, V, VI, IV. X2 ON THE ONE", hues[1])

    # ---- 2: Arpeggiator (Arcadia): quarter... eighth-note chart falling down four lanes ----------
    x0, y0 = PW, 0
    a = pal["arcadia"]
    grid(x0, y0, lin(a["grid"]))
    col = lin(a["enemyCandidates"][1])
    mx, my = x0 + 600, y0 + 96
    lane_w, top, bottom = 110, my + 34, y0 + 430
    left = mx - 2 * lane_w
    cv.lines(ld.polyline([(mx - 250, my - 26), (mx + 250, my - 26), (mx + 250, my + 26), (mx - 250, my + 26)], closed=True), col, 2.6, 2.4)
    cv.lines(circle(mx, my, 20, 24), white, 2.4, 2.0)
    cv.glow(mx, my, 60, gold, 0.4)
    for kk in range(5):
        cv.lines([(left + kk * lane_w, top, left + kk * lane_w, bottom)], lin(a["grid"]) * 0.5 + white * 0.5, 0.5, 1.2)
    lanes = [1, 2, 3, 4, 3, 2, 3, 4, 1, 4, 3, 4, 2, 1, 4, 3]
    for step in range(0, 16, 2):                                   # the bar in flight: eighth notes
        ly = bottom - 16 - (step / 2) * 46
        lx = left + (lanes[step] - 0.5) * lane_w
        cv.threat([(lx - 34, ly, lx + 34, ly)], danger * 0.75 + white * 0.25, 9.0, 1.6)
    for step in range(16):                                          # the next bar's chart, above the machine
        for lane in range(4):
            on = lanes[step] == lane + 1 and step % 2 == 0
            cv.glow(mx - 120 + step * 16, my - 78 + lane * 11, 3.2 if on else 1.6, gold if on else white, 2.0 if on else 0.25)
    sx, sy = left + 1.5 * lane_w + 30, bottom - 150
    ship(sx, sy, -PI / 2 + 0.1)
    for kk, lab in enumerate("1234"):
        note(lab, left + (kk + 0.5) * lane_w - 5, bottom + 6, 12, white, 0.9)
    title(x0, y0, "ARPEGGIATOR", "THE LEAD'S BROKEN CHORD COMES DOWN FOUR LANES. THE NEXT BAR IS WRITTEN ABOVE", "ITS SHUTTER OPENS ON THE ONE AS IT LOADS THE BAR: X2", col)

    # ---- 3: Fata Morgana (Mirage): the oasis, a mirage, the storm --------------------------------
    x0, y0 = 0, PH
    a = pal["mirage"]
    grid(x0, y0, lin(a["grid"]))
    accent, col = lin(a["accent"]), lin(a["enemyCandidates"][2])
    ox, oy, orad = x0 + 470, y0 + 250, 150
    rng = np.random.default_rng(5)
    for _ in range(70):                                             # sand, outside the calm
        sx_, sy_ = x0 + rng.uniform(20, PW - 20), y0 + rng.uniform(70, PH - 80)
        if math.hypot(sx_ - ox, sy_ - oy) > orad + 14 and math.hypot(sx_ - (ox + 330), sy_ - (oy - 30)) > orad * 0.8 + 10:
            cv.threat([(sx_, sy_, sx_ + 26, sy_ - 7)], danger * 0.5, 1.4, 0.8)
    cv.glow(ox, oy, orad * 1.5, accent, 0.14)
    cv.lines(circle(ox, oy, orad, 80), accent, 2.2, 2.4)
    cv.lines(circle(ox, oy, orad - 8, 80), accent, 0.7, 1.2)
    mxx, myy = ox + 330, oy - 30                                    # the mirage: dashed, dimmer
    cv.lines(circle(mxx, myy, orad * 0.8, 60)[::2], accent, 1.0, 1.8)
    note("?", mxx - 8, myy - 14, 26, accent, 0.9)
    ba = math.radians(205.0)
    bx, by = ox + math.cos(ba) * (orad + 46), oy + math.sin(ba) * (orad + 46)
    for kk, (r, sp) in enumerate(((50, 250), (38, 210), (26, 170))):
        for j in range(3):
            a0 = j * 2 * PI / 3 + kk * 0.7
            cv.lines(ld.transform(ld.arc(1.0, a0, a0 + math.radians(sp) / 3, 8), bx, by, r), col, 2.6, 2.2)
    cv.lines(circle(bx, by, 9, 16), white, 3.0, 2.0)
    cv.glow(bx, by, 120, col, 0.18)
    shx, shy = ox + 40, oy + 20
    an0 = math.atan2(shy - by, shx - bx)
    for f in (-0.3, -0.15, 0.0, 0.15, 0.3):
        bullet(bx + math.cos(an0 + f) * 110, by + math.sin(an0 + f) * 110)
    ship(shx, shy, an0 + PI)
    title(x0, y0, "FATA MORGANA", "OUTSIDE THE CALM THE STORM BITES ON EVERY KICK. THE OASIS DRIFTS: GO WITH IT", "A MIRAGE BLINKS ON THE SNARE. SHE CROSSES THE WATER EVERY FOURTH BAR: X2", accent)

    # ---- 4: Kaleidos, Reprise (Last Light): six arms, one soloing as a wave ---------------------
    x0, y0 = PW, PH
    a = pal["lastlight"]
    grid(x0, y0, lin(a["grid"]))
    arm_cols = [lin(pal[k]["accent"]) for k in ("ultraviolet", "solar", "reef", "aurora", "furnace", "zenith")]
    wx, wy = x0 + 760, y0 + 250
    for i in range(6):
        an = i * PI / 3 + 0.25
        solo = i == 1
        if solo:
            continue                                                 # Longwave's arm has let go: it is the wave
        ex, ey = wx + math.cos(an) * 170, wy + math.sin(an) * 170
        nx, ny = -math.sin(an) * 16, math.cos(an) * 16
        cv.lines(ld.polyline([(wx + math.cos(an) * 40 + nx, wy + math.sin(an) * 40 + ny), (ex, ey), (wx + math.cos(an) * 40 - nx, wy + math.sin(an) * 40 - ny)]), arm_cols[i], 1.6, 2.0)
    cv.lines(circle(wx, wy, 34, 32), white, 2.0, 2.2)
    cv.lines(circle(wx, wy, 44, 40)[::2], white, 0.8, 1.4)
    sx, sy = x0 + 260, y0 + 280
    pts = [(wx - 110 - t * 62, wy + 20 + 58 * math.sin(t * 1.15 + 0.6)) for t in range(8)]
    cv.lines(ld.polyline(pts), arm_cols[1], 1.2, 1.6)
    for t, (qx, qy) in enumerate(pts):
        crest = math.sin(t * 1.15 + 0.6) < -0.55
        cv.lines(ld.transform(ld.polygon(6), qx, qy, 17), white if crest else arm_cols[1], 3.0 if crest else 2.0, 2.2)
        if crest:
            cv.glow(qx, qy, 46, gold, 0.4)
    cv.glow(wx, wy, 230, arm_cols[1], 0.10)
    ship(sx, sy, math.atan2(pts[4][1] - sy, pts[4][0] - sx))
    note("LONGWAVE'S ARM", pts[5][0] - 60, pts[5][1] + 78, 10, arm_cols[1], 1.0)
    title(x0, y0, "KALEIDOS, REPRISE", "SIX ARMS, ONE FOR EACH BOSS YOU HAVE BEATEN. EACH TAKES A SOLO OF TWO BARS", "SHOOT THE SOLOIST OFF AND ITS TUNE IS GONE. THE LAST PATTERN IS THE FIRST SONG", arm_cols[0])

    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    for b in BOSSES:
        data = {"note": "Generated by design/tools/bosses_7_10.py. World units. The page design/bosses/%s.md is made from this data." % b["id"],
                "id": b["id"], "name": b["name"], "track": b["track"], "bpm": b["bpm"], "key": b["key"], "common": COMMON,
                "parts": [{"name": a, "what": t} for a, t in b["parts"]], "numbers": b["numbers"],
                "phases": [{"name": n, "health": s, "rules": r} for n, s, r in b["phases"]],
                "weakPoint": b["weak"], "death": b["death"], "sound": b["sound"], "caps": b["caps"]}
        with open(os.path.join(DESIGN, "bosses", b["id"] + ".json"), "w", encoding="utf-8", newline="\n") as f:
            json.dump(data, f, indent=1)
        open(os.path.join(DESIGN, "bosses", b["id"] + ".md"), "w", encoding="utf-8", newline="\n").write(page(b))
        print("wrote", b["id"])
    if "nopicture" not in sys.argv:
        out = os.path.join(DESIGN, "previews", "bosses_7_10.png")
        preview(out)
        print("wrote", out)


if __name__ == "__main__":
    main()
