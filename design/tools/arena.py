"""The arena's structures and events (GAME_DESIGN 9): numbers, shapes and a contact sheet.

Writes design/arena.json (rules common to all, one entry each, the shapes as glyph segments in the
format of design/glyphs.json) and design/previews/arena.png.
Run: . .\\scripts\\env.ps1; python -I design\\tools\\arena.py
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
    return [[round(float(s[0]), 4), round(float(s[1]), 4), round(float(s[2]), 4), round(float(s[3]), 4), layer] for s in segs]


def arcs(r, count, span_deg, n=6, rot_deg=0.0):
    out = []
    for k in range(count):
        a0 = math.radians(rot_deg + k * 360.0 / count)
        out += ld.transform(ld.arc(1.0, a0, a0 + math.radians(span_deg), n), 0, 0, r)
    return out


def shapes():
    """Unit shapes, y up, layers as in glyphs.json: 0 hull, 1 ornament, 2 white mark."""
    hexa = lambda r, rot: ld.transform(ld.polygon(6, 1.0, rot), 0, 0, r)   # noqa: E731
    g = {}
    g["pylon"] = (tag(ld.polyline([(0, 1.25), (0.45, 0), (0, -1.25), (-0.45, 0)], closed=True), 0) + tag([(0, 0.7, 0, -0.7)], 1)
                  + tag([(0.62, 0.25, 0.9, 0.25), (0.62, -0.25, 0.9, -0.25), (-0.62, 0.25, -0.9, 0.25), (-0.62, -0.25, -0.9, -0.25)], 2))
    g["rift"] = tag(arcs(1.0, 3, 96, 8, 10), 0) + tag(arcs(0.62, 3, 70, 6, 70), 1) + tag(arcs(0.3, 3, 60, 4, 130), 1)
    g["vault"] = tag(hexa(1.0, 0.0), 0) + tag(hexa(0.72, PI / 6), 1) + tag(hexa(0.44, 0.0), 2)
    g["bumper"] = (tag(ld.transform(ld.polygon(24), 0, 0, 1.0), 0) + tag(ld.transform(ld.polygon(16), 0, 0, 0.58), 1)
                   + tag([(math.cos(a) * 0.72, math.sin(a) * 0.72, math.cos(a) * 0.92, math.sin(a) * 0.92) for a in (PI / 4, 3 * PI / 4, 5 * PI / 4, 7 * PI / 4)], 2))
    g["lens"] = tag(ld.polyline([(-1, 0), (0, 0.11), (1, 0), (0, -0.11)], closed=True), 0) + tag([(-0.8, 0, 0.8, 0)], 2)
    g["comet"] = (tag(ld.polyline([(1, 0), (0, 0.42), (-0.6, 0), (0, -0.42)], closed=True), 0)
                  + tag([(-0.6, 0, -1.9, 0), (-0.35, 0.2, -1.5, 0.3), (-0.35, -0.2, -1.5, -0.3)], 1))
    g["carrier"] = (tag(ld.transform(ld.polygon(8, 1.0, PI / 8), 0, 0, 1.0), 0)
                    + tag(ld.polyline([(-0.55, 0.3), (0.55, 0.3), (0.55, -0.3), (-0.55, -0.3)], closed=True), 1)
                    + tag([(-0.2, 0, 0.2, 0), (0, -0.2, 0, 0.2)], 2))
    return g


SIZE = {"pylon": 26.0, "rift": 80.0, "vault": 60.0, "bumper": 45.0, "lens": 160.0, "comet": 30.0, "carrier": 44.0}

COMMON = {
    "note": "World units. A structure stands in the world for one Track; an event is a few bars long. Both are placed by the run's seed relative to the players' centre at the moment they are placed, never inside a player's frame, never within 400 wu of a wall, and never while a boss or a mini-boss is alive. Health and damage follow the Track through 'trackScale': the mini-boss health of that Track in design/bosses/minibosses.json (Track 1 uses 300), times the fraction given.",
    "structuresPlaced": "on the first downbeat of the Track, 1,000 to 1,800 wu from the players' centre, at least 700 wu from each other; the last Track's dissolve over the Track change's Break unless a player is within 600 wu, in which case they stay until that player leaves",
    "events": "Track 1 has one, the Comet, in the Groove of its second cycle; from Track 2 one event a Track, in the Groove of its second cycle; from Track 4 a second one in the Groove of its first cycle (every Track is two cycles since 2026-11-14). Drawn by the seed from the Track's list, never the event of the Track before. Announced one bar ahead by a gold edge chevron and its name as world text at the place (cap 22, gold), then it runs",
    "indicators": "structures get no edge chevron (they are found); events get the gold one; a Rift's is the enemy hue with a ring at its tip",
    "coop": "placed round the players' centre; every reward is for each player (Boost orbs and Shards are each player's own, as everywhere)",
    "caps": "at most 6 structures and 1 event alive; what an event or a structure spawns counts against the enemy and bullet caps",
}

STRUCTURES = [
    dict(id="vault", name="Vault", glyph="vault", radius=60.0,
         what="an armoured crystal: break it for a Boost and Shards",
         rules=["health: trackScale x 0.33", "takes damage from shots, Riffs and Novas; nothing else touches it; it does not hurt",
                "each quarter of its health lost, one ring of its ornament shatters and it rings one step higher",
                "broken: one Boost orb and 40 Shards for each player"],
         look="three nested hexagons in white and gold with a glass fill and a gold pool at 0.2; it turns a twelfth of a turn a bar and swells 6% on the kick",
         sound="each hit a glass tick on a note of the scale, one step up for each quarter lost; the break an arpeggio of bells up the chord with a glass burst"),
    dict(id="pylon", name="Pylon", glyph="pylon", radius=26.0,
         what="a turret you tune by standing still",
         rules=["its ring is 150 wu, dashed in the palette accent: stand inside for four beats and it fills clockwise; leave and it empties twice as fast",
                "tuned, it takes the tuner's colour and on the One of every bar fires a ring that runs to 520 wu at 700 wu/s and deals 6 x the Track's number to each enemy once",
                "it stays tuned for one cycle (36 bars; 24 in Track 1), then dims and can be tuned again; enemies ignore it"],
         look="a tall diamond with a white spine and four white ticks; untuned at 0.8 in the accent, tuned at 2.4 in the player's colours with a pool at 0.15; its pulse is a Riff ring in that player's colours (rings are a language: look.md 6)",
         sound="the fill is a woodblock on each of the four beats, rising; tuned is three bells up the chord; each pulse a bell on the chord's root an octave up, on the One"),
    dict(id="rift", name="Rift", glyph="rift", radius=80.0,
         what="a tear the crowd comes through: close it",
         rules=["opens with the Groove of the Track's second cycle and stays through the Build; at the Drop it closes by itself and gives nothing",
                "while open, four of the Track's fodder come through it every bar (they are part of the crowd target, not on top of it)",
                "health: trackScale x 0.25; destroyed: the flow stops, one Boost orb and 30 Shards for each player"],
         look="three broken rings in the first enemy hue turning against each other, a black disc inside (the only filled black in the game: the lattice is pulled 20 wu into it) and sparks spiralling in; off screen its chevron carries a ring",
         sound="a drone on the chord's bass note while it is open, its cutoff rising through the Build; a tom for each body that comes through; destroyed: a reversed cymbal cut short, then glass"),
    dict(id="bumper", name="Bumper", glyph="bumper", radius=45.0,
         what="it bounces everything, and it is tuned",
         rules=["ships bounce off it unhurt with a kick of 600 wu/s; player shots reflect off it (a free bounce, and x 1.5 damage afterwards); enemies and their bullets bounce too",
                "every player shot that hits it charges it; at eight it discharges on the next beat: a ring to 260 wu dealing 10 x the Track's number to enemies",
                "a Track that has Bumpers has three or four, 300 to 500 wu apart: a cluster to play in"],
         look="a ring with a smaller ring inside and four white ticks; its fill brightens with its charge (glass 0.05 to 0.4) and the discharge is a white shock ring",
         sound="each Bumper of a cluster owns a chord tone (root, third, fifth, octave): a bounce is a tom on its note, so a shot rattling between them plays the chord; the discharge is the chord's stab with a zap"),
    dict(id="current", name="Current", glyph=None, radius=0.0,
         what="a lane that carries whatever enters it",
         rules=["a lane 240 wu wide and 1,600 long; anything inside moves 260 wu/s faster along it: ships, enemies, enemy bullets",
                "player shots travelling with it are 30% faster and deal 30% more; against it, 30% slower and no less",
                "a Track that has Currents has two, crossing"],
         look="no walls: rows of chevrons in the palette's grid colour at 0.6 streaming along it on the eighths, and the lattice inside it stretched 12 wu downstream",
         sound="a whoosh that is heard only inside, panned along the flow; entering it is a short upward sweep"),
    dict(id="lens", name="Lens", glyph="lens", radius=160.0,
         what="a bar of glass that splits your shots in three",
         rules=["a line 320 wu long; a player shot crossing it becomes three: the middle keeps its damage, the two beside it fan 14 degrees out at half damage",
                "ships, enemies and enemy bullets pass through it untouched; it cannot be hurt; a shot is split once by each Lens",
                "the split shots take the hues 60 degrees either side of the player's, as Prism's lanes do"],
         look="a thin lozenge of white glass with a bright spine; it turns slowly, one turn every eight bars, so the fan sweeps",
         sound="each split is a glass tick a fifth above the lead's note"),
]
STRUCTURES[-1]["note"] = "GAME_DESIGN 9 called this structure 'Prism'; it is the Lens here so that it is not confused with the Boost of that name"

EVENTS = [
    dict(id="vault_drop", name="Vault Drop", bars=8,
         what="a rich Vault that will not wait",
         rules=["a gold Vault warps in 1,000 to 1,400 wu from the players' centre under a column of light", "health trackScale x 0.4; broken: two Boost orbs and 80 Shards for each player",
                "a ring round it empties over eight bars; then it warps out"],
         look="the Vault in gold with a flare 400 px tall for its first bar and a thin timer ring at 90 wu", sound="it lands with the gong; its ring ticks a woodblock on each bar line, faster in the last bar"),
    dict(id="rift_surge", name="Rift Surge", bars=0,
         what="three Rifts at once",
         rules=["at the start of a Build three Rifts open on a triangle 900 wu from the players' centre, each letting six through a bar", "they close at the end of the Drop",
                "each one destroyed pays as a Rift; all three: a Cell for each hurt player, 100 Shards for the others"],
         look="as the Rift; the three are joined by a faint triangle of their hue until one is closed", sound="three drones a fifth apart; each closing takes its note away"),
    dict(id="elite_hunt", name="Elite Hunt", bars=12,
         what="an elite that runs",
         rules=["one elite of the Track's highest-threat family warps in 1,200 wu away carrying two traits (three from Track 7) and flees the nearest player at 0.8 of its speed",
                "killed within twelve bars: a Trio for each player (as a level's) and 150 Shards; otherwise it warps out"],
         look="the elite's own look with a white edge chevron; a dotted white line trails it for a bar so that it can be followed", sound="the elite's arrival fifth, then its family's own voice receding; the kill is the upgrade chord"),
    dict(id="comet", name="Comet", bars=0,
         what="a neutral body crossing the arena, trailing Shards",
         rules=["it crosses on a straight line passing within 300 wu of the players' centre at 700 wu/s and leaves five Shards a beat behind it",
                "health trackScale x 0.1; broken: 60 Shards and one Boost orb for each player", "it hurts nothing and nothing but player fire hurts it"],
         look="a white kite with a three-line tail in gold fading over 200 wu, a sparkle at its head", sound="a saw glide rising over the two beats of its warning, then a bell on each Shard it drops; no name card, it is too fast: a white chevron two beats ahead"),
    dict(id="eclipse", name="Eclipse", bars=8,
         what="lights out: the crowd is seen on the beat",
         rules=["for eight bars the scene falls dark and comes back for a flash on every beat (the kick's envelope, 110 ms)",
                "the priority layer never dims: every bullet, beam and ship is as bright as ever", "kills score double while it lasts"],
         look="scene light x 0.15 between beats (enemy lines, effects, lattice heat), the lattice at 0.05, the background off; players' own light is untouched; a dark ring sweeps across the frame as it begins and ends. Reduce flashing: x 0.4 between beats and the flash adds 0.3",
         sound="the music bus low-passed at 1.2 kHz for the eight bars; a soft tick on each flash"),
    dict(id="meteor_rain", name="Meteor Rain", bars=4,
         what="impacts on the beat, each one announced",
         rules=["for four bars, on every beat, three dashed danger rings of 110 wu appear in the frame, one of them where a player stands",
                "two beats later, on the beat, each one lands: a cell from any ship inside, 20 x the Track's number to every enemy inside"],
         look="the ring fills from its edge to its centre over the two beats, on the priority layer; the impact is a large kill in white with a hot lattice dent", sound="each warning is a saw glide falling over its two beats; each impact the sub drop and a crash, on the beat"),
    dict(id="convoy", name="Convoy", bars=12,
         what="slow carriers with heavy loot, and an escort",
         rules=["three carriers cross in a line at 60 wu/s, starting 1,500 wu from the players' centre, with eight Hounds of the Track's tier round them",
                "each carrier: health trackScale x 0.33; one Boost orb and 50 Shards for each player; the last of the three also drops a Cell",
                "after twelve bars whatever is left warps out"],
         look="the carrier: an octagon in gold with a white hold and a plus, radius 44, a gold pool; the three joined by a thin gold line", sound="a low tom on every beat while they roll, one voice a carrier; each one broken is the Boost bells"),
    dict(id="storm_cell", name="Storm Cell", bars=8,
         what="a field of lightning that roams: a weapon if you lead the crowd into it",
         rules=["a circle of 320 wu drifting toward the players' centre at 90 wu/s for eight bars",
                "on every beat lightning joins up to six bodies inside it: 15 x the Track's number to each enemy",
                "a ship inside on a beat is warned (its halo crackles); still inside on the next beat it loses a cell"],
         look="a ring in the palette accent with the lattice hot inside it; the strikes are zigzag lines drawn for one beat in white and the accent", sound="a crackle that rises as it nears; each strike a zap on the chord's three notes"),
]

BY_TRACK = {
    "1": {"structures": ["vault"], "events": ["comet"]},
    "2": {"structures": ["vault", "pylon", "pylon"], "events": ["vault_drop", "comet"]},
    "3": {"structures": ["vault", "pylon", "rift"], "events": ["rift_surge", "vault_drop", "comet"]},
    "4": {"structures": ["bumper", "bumper", "bumper", "vault", "rift"], "events": ["elite_hunt", "meteor_rain"]},
    "5": {"structures": ["current", "current", "pylon", "rift"], "events": ["convoy", "storm_cell"]},
    "6": {"structures": ["lens", "lens", "bumper", "bumper", "vault"], "events": ["eclipse", "elite_hunt"]},
    "7": {"structures": ["pylon", "pylon", "current", "vault"], "events": ["vault_drop", "convoy", "comet"]},
    "8": {"structures": ["bumper", "bumper", "bumper", "bumper", "lens", "lens"], "events": ["meteor_rain", "rift_surge"]},
    "9": {"structures": ["current", "current", "lens", "rift", "rift"], "events": ["storm_cell", "eclipse"]},
    "10": {"structures": ["vault", "pylon", "rift", "bumper", "current", "lens"], "events": ["elite_hunt", "meteor_rain", "convoy", "storm_cell"]},
    "note": "Track 1 has no event before its second cycle (the Comet). In the loop each Track keeps its list.",
}


def build():
    sh = shapes()
    glyphs = [{"id": k, "radius": SIZE[k], "segments": v, "note": "unit shape; multiply by radius; layers 0 hull, 1 ornament, 2 white mark"} for k, v in sh.items()]
    return {"note": "Generated by design/tools/arena.py. GAME_DESIGN 9: six structures (the mini-bosses' Pen is the seventh, in design/bosses/minibosses.json) and eight events.",
            "common": COMMON, "structures": STRUCTURES, "events": EVENTS, "byTrack": BY_TRACK, "glyphs": glyphs}


def preview(path, data):
    pals = json.load(open(os.path.join(DESIGN, "palettes.json"), encoding="utf-8"))
    font = ld.load_font(os.path.join(DESIGN, "vector_font.txt"))
    lin = lambda e: np.asarray(e["linear"], np.float32)   # noqa: E731
    pal = next(p for p in pals["palettes"] if p["id"] == "aurora")
    white, gold = np.ones(3, np.float32), lin(pals["constants"]["pickup"])
    danger, p1 = lin(pals["constants"]["danger"]), lin(pals["constants"]["players"][0])
    accent, enemy = lin(pal["accent"]), lin(pal["enemyCandidates"][0])
    W, H = 2400, 1080
    cv = ld.Canvas(W, H)
    cv.radial_bg(lin(pal["bg0"]) * 0.5, lin(pal["bg1"]) * 0.45)
    note = lambda t, x, y, cap=13, col=white, lev=1.0: cv.text(font, t, x, y, cap, col, lev, max(1.3, cap / 9.0))   # noqa: E731
    seg = {g["id"]: g["segments"] for g in data["glyphs"]}

    def draw(gid, x, y, r, rot, hull, orn, lev=2.4):
        for layer, col, w in ((0, hull, 2.4), (1, orn, 1.8), (2, white, 1.6)):
            s = [(a[0], -a[1], a[2], -a[3]) for a in seg[gid] if a[4] == layer]
            if s:
                cv.lines(ld.transform(s, x, y, r, rot), col, lev if layer == 0 else lev * 0.8, w)

    note("STRUCTURES", 60, 40, 26, white, 1.5)
    cols = [200, 590, 980, 1370, 1760, 2150]
    y = 300
    # Vault
    cv.glow(cols[0], y, 150, gold, 0.2)
    draw("vault", cols[0], y, 90, 0.2, gold * 0.6 + white * 0.4, gold)
    # Pylon: tuned, with its ring and a pulse
    cv.lines(ld.transform(ld.polygon(72), cols[1], y, 150)[::2], accent, 0.8, 1.6)
    cv.lines(ld.transform(ld.arc(1.0, -PI / 2, PI * 0.7, 40), cols[1], y, 150), p1, 2.0, 2.4)
    draw("pylon", cols[1], y, 44, 0.0, p1, p1 * 0.5 + white * 0.5)
    cv.glow(cols[1], y, 120, p1, 0.15)
    # Rift
    cv.glow(cols[2], y, 170, enemy, 0.16)
    draw("rift", cols[2], y, 100, 0.0, enemy, enemy * 0.5 + white * 0.5)
    # Bumpers: a cluster of three and a shot bouncing
    for (bx, by) in ((cols[3] - 70, y - 50), (cols[3] + 80, y - 20), (cols[3] - 10, y + 80)):
        cv.fill([(bx + 48 * math.cos(a), by + 48 * math.sin(a)) for a in np.linspace(0, 2 * PI, 24, endpoint=False)], accent, 0.08)
        draw("bumper", bx, by, 48, 0.0, accent, accent * 0.5 + white * 0.5, 2.0)
    path_ = [(cols[3] - 170, y + 120), (cols[3] - 50, y + 44), (cols[3] + 36, y - 2), (cols[3] - 30, y - 62), (cols[3] + 150, y - 150)]
    cv.lines(ld.polyline(path_), p1 * 0.6 + white * 0.4, 1.6, 1.6)
    # Current
    for k in range(7):
        for row in (-1, 0, 1):
            cx_, cy_ = cols[4] - 150 + k * 50, y + row * 50
            cv.lines(ld.transform(ld.polyline([(-0.5, 0.7), (0.5, 0.0), (-0.5, -0.7)]), cx_, cy_, 13), lin(pal["grid"]) * 0.6 + white * 0.4, 0.9 + 0.2 * (k % 2), 1.6)
    # Lens with a shot split in three
    draw("lens", cols[5], y, 150, PI / 2 - 0.25, white, white, 2.6)
    cv.lines([(cols[5] - 190, y + 30, cols[5] - 8, y + 4)], p1, 3.0, 2.2)
    for da, col in ((-0.24, np.array([0.3, 1.0, 0.5], np.float32)), (0.0, p1), (0.24, np.array([0.5, 0.4, 1.0], np.float32))):
        a = -0.14 + da
        cv.lines([(cols[5] + 6, y + 2, cols[5] + 6 + math.cos(a) * 170, y + 2 + math.sin(a) * 170)], col, 2.6, 2.0)
    labels = [("VAULT", "BREAK IT: A BOOST AND SHARDS"), ("PYLON", "STAND FOUR BEATS: IT FIRES ON EVERY ONE"), ("RIFT", "THE CROWD COMES THROUGH: CLOSE IT"),
              ("BUMPER", "BOUNCES ALL. EACH IS A NOTE OF THE CHORD"), ("CURRENT", "CARRIES WHATEVER ENTERS IT"), ("LENS", "SPLITS YOUR SHOTS IN THREE")]
    for x, (a, b) in zip(cols, labels):
        note(a, x - len(a) * 6 * 1.6, y + 190, 18, white, 1.4)
        note(b, x - len(b) * 6 * 0.92, y + 226, 10, accent)

    note("EVENTS", 60, 590, 26, white, 1.5)
    y = 790
    ex = [300, 900, 1500, 2100]
    # Comet
    draw("comet", ex[0], y, 40, -0.35, white, gold, 2.8)
    cv.glow(ex[0] + 36, y - 13, 16, white, 0.8)
    for k in range(6):
        cv.glow(ex[0] - 90 - k * 46, y + 34 + k * 17, 9, gold, 0.5)
    # Meteor Rain: three rings filling
    for (mx, my, f) in ((ex[1] - 120, y - 40, 0.3), (ex[1] + 60, y + 40, 0.6), (ex[1] + 150, y - 70, 0.9)):
        cv.threat(ld.transform(ld.polygon(40), mx, my, 110)[::2], danger * 0.8 + white * 0.2, 2.2, 1.4)
        cv.threat(ld.transform(ld.polygon(40), mx, my, 110 * (1 - f)), danger * 0.8 + white * 0.2, 1.6, 1.0)
    # Storm Cell
    cv.lines(ld.transform(ld.polygon(64), ex[2], y, 170), accent, 1.8, 2.2)
    rng = np.random.default_rng(3)
    pts = [(ex[2] + rng.uniform(-120, 120), y + rng.uniform(-110, 110)) for _ in range(6)]
    for a, b in zip(pts, pts[1:]):
        mid = ((a[0] + b[0]) / 2 + rng.uniform(-22, 22), (a[1] + b[1]) / 2 + rng.uniform(-22, 22))
        cv.lines([(a[0], a[1], mid[0], mid[1]), (mid[0], mid[1], b[0], b[1])], white * 0.6 + accent * 0.4, 3.0, 1.6)
    cv.glow(ex[2], y, 200, accent, 0.12)
    # Convoy
    for k in range(3):
        cx_ = ex[3] - 150 + k * 150
        cv.glow(cx_, y, 90, gold, 0.16)
        draw("carrier", cx_, y, 52, 0.0, gold, gold * 0.5 + white * 0.5)
    cv.lines([(ex[3] - 98, y, ex[3] - 52, y), (ex[3] + 52, y, ex[3] + 98, y)], gold, 0.8, 1.4)
    elabels = [("COMET", "TRAILS SHARDS. BREAK IT FOR A BOOST"), ("METEOR RAIN", "TWO BEATS OF WARNING, THEN THE BEAT"),
               ("STORM CELL", "LEAD THE CROWD IN. DO NOT STAY"), ("CONVOY", "THREE CARRIERS, HEAVY LOOT, AN ESCORT")]
    for x, (a, b) in zip(ex, elabels):
        note(a, x - len(a) * 6 * 1.6, y + 170, 18, gold, 1.4)
        note(b, x - len(b) * 6 * 0.92, y + 206, 10, accent)
    note("NOT DRAWN: VAULT DROP, RIFT SURGE, ELITE HUNT (THINGS ABOVE, ARRIVING) AND ECLIPSE (THE SCENE DARK BETWEEN BEATS)", 60, H - 40, 10, white, 0.8)
    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    with open(os.path.join(DESIGN, "arena.json"), "w", encoding="utf-8", newline="\n") as f:
        json.dump(data, f, indent=1)
    out = os.path.join(DESIGN, "previews", "arena.png")
    preview(out, data)
    print("structures", len(data["structures"]), "events", len(data["events"]), "glyphs", len(data["glyphs"]))
    print("wrote", out)


if __name__ == "__main__":
    main()
