"""Mini-bosses. Batch 1: Alpha Hound, The Hive, Battering Ram, Carom King.
Batch 2: Moth Queen, Brood Mother, Blink Star, Sentry Array.
Batch 3: Twin Tether, Weaver Prime, Deep Well, Mirror Mimic.

Writes design/bosses/minibosses.json (numbers) and design/previews/minibosses_1.png,
minibosses_2.png and minibosses_3.png (four panels each).
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
    pick="the run's seed draws one whose family the run has already met, never the one met in the Track before. Batch 1: Track 2 draws from Alpha Hound and The Hive, Track 3 from Battering Ram and Carom King. Batch 2: Track 4 draws from Moth Queen and Brood Mother, Track 5 from Blink Star and Sentry Array. Batch 3: Track 6 is always Mirror Mimic (the rehearsal for Feedback), Track 7 draws from Twin Tether and Weaver Prime, Track 8 from Deep Well and whichever of those two Track 7 did not draw. Track 9 draws any mini-boss the run has not met and gives it one elite trait (Armored, Swift or Regenerating), Track 10 the same with two. In the loop: any, with as many of those traits as the loop has gone round, to three",
    drop="the Drop holds while the mini-boss lives, for at most 8 bars past its 16; then it warps out and leaves nothing",
    healthByTrack={"2": 600, "3": 1100, "4": 1700, "5": 2400, "6": 3200, "7": 4100, "8": 5100, "9": 6200, "10": 7400, "loopFactorPerTrack": 1.25,
                   "note": "times the mini-boss's own healthFactor; in co-op x (1 + 0.5 x (players - 1)), as GAME_DESIGN 8 has it. Sized for 25 to 35 seconds of fire with the build of that Track's middle cycle: tune with the bot"},
    crowd={"sectionScale": 0.75, "penScale": 0.35, "penMax": 120,
           "note": ("fodder only, on top of what the mini-boss calls itself. In a Pen fight the crowd is 0.35 of the Drop's and never more than 120 bodies inside the Pen: "
                    "the Pen is 1.7 frames of ground, and three quarters of a Drop's crowd inside it (the first build: up to 580 in view) hid the lock line and the bounces the fight is about")},
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
    # ---------------------------------------------------------------- batch 2
    dict(
        id="moth_queen", name="Moth Queen", family="moth", glyph="moth_5", radius=70.0, healthFactor=1.0, pen=False,
        idea="her court is a clock that closes on you; she is open while she winds it again",
        move="holds 520 wu from the nearest player, drifting round them one turn every eight bars; she never closes in herself",
        pattern=[
            "a court is four bars. Bar one, beat 1: twelve Moths of the Track's tier warp in as a ring of radius 420 round the nearest player and begin to turn, a quarter turn a bar, stepping inward on every beat",
            "bars one to three: the ring spirals in from 420 to 140 wu. On every eighth note the Queen fires a rosette: five petals (bullets) 72 degrees apart, each rosette turned 12 degrees from the last, 240 wu/s; a petal ends 700 wu from her",
            "bar four, beat 1: the court closes. Every Moth of it still alive bursts into a ring of six bullets and is gone. The Queen stops firing and spreads her wings for the bar while the next court gathers round her",
        ],
        weakPoint="open for the whole fourth bar, wings spread and core gold: damage x 2. In bars one to three she takes x 1, but her petals are thickest near her",
        half="two courts of eight, the inner turning one way and the outer the other, closing a bar apart; her rosettes come in pairs (ten petals an eighth note). She is open only for the second half of the fourth bar",
        death="the court loses its hold: every Moth alive flies straight out from her and falls as Shards",
        sound="the court is the open hat, stepping round the stereo field with the ring's turn; each rosette is a plucked note of the Track's arpeggio; bar three is a riser; the burst is the clap on the downbeat; while she is open the bed drops to kick and pad",
        cap="twelve court Moths (sixteen from half health), counted against the enemy cap; at most 240 bullets",
    ),
    dict(
        id="brood_mother", name="Brood Mother", family="brood", glyph="brood_5", radius=90.0, healthFactor=1.1, pen=False,
        idea="her children hold her shell shut: keep the nest clean and she is yours",
        move="drifts toward the players' centre at 40 wu/s; touching her hurts",
        pattern=[
            "beat 1 of every bar: she lays a clutch of six eggs in a ring 170 wu round her. An egg is white, pulses on the beats and dies to one shot",
            "beat 3: the eggs still whole hatch into broodlings (brood_1, radius 8, the health of a tier-1 Shoal dart), which chase the nearest player at 260 wu/s",
            "a thread of light runs from every living broodling to her, and while there is one thread her dome is shut: she takes a quarter of the damage",
        ],
        weakPoint="exposed: with no broodling alive the dome breaks with a flash and she takes x 2 until the next egg hatches. Shooting the eggs before beat 3 keeps her exposed for the whole bar",
        half="two clutches of five a bar, on beats 1 and 3, each hatching two beats after it is laid; broodlings are 15% faster",
        death="her last clutch: sixteen gold eggs scatter across the frame and lie for two bars. Each one touched or shot breaks into ten Shards",
        sound="the laying is a low tom on beat 1 and the hatching a chirp for each egg, arpeggiated up the chord on beat 3; the threads are a pad with one voice for each living broodling, so the dome can be heard: when the pad falls silent she is open, and a bell marks the break",
        cap="36 broodlings alive, counted against the enemy cap; a clutch that would pass it lays fewer eggs",
    ),
    dict(
        id="blink_star", name="Blink Star", family="blink", glyph="blink_5", radius=60.0, healthFactor=0.9, pen=False,
        idea="it draws a star round you one edge a beat, then the star fires; it ends where it began",
        move="does not travel: it blinks from point to point of a five-point star of radius 420 wu, centred where the nearest player stood on the first beat",
        pattern=[
            "a star is two bars. Bar one, beat 1: it appears at the first point. On beats 2, 3 and 4 and on beat 1 of bar two it blinks to the next point in star order (every second point), so the fifth hop brings it back to where it began. Each hop leaves its edge behind as a dashed danger line",
            "bar two, beat 2: the star fires. All five edges are lethal beams 16 wu wide for one beat. Off the lines is safe: the pentagon in the middle (about 260 wu across), the corners of the five points, or outside the star",
            "bar two, beats 3 and 4: it rests at the first point, dazed. Then the next star, centred on the player again",
        ],
        weakPoint="dazed for the two beats after the star fires, at the point where it began: damage x 2. While it draws it can be hit at each point for the beat it stands there, x 1",
        half="every point it leaves keeps a white decoy; when the star fires each decoy also throws a fan of three bullets at the nearest player. The star is drawn a quarter larger",
        death="the last star stays lit in gold for a bar, harmless, and its edges run to the players as Shards",
        sound="each hop is the next note of a five-note pentatonic run on the Track's root, so the star is a rising phrase the player learns; the firing is all five notes at once as a stab with a zap; dazed is the stab's tail with the bed low-passed",
        cap="one body, five beams, five decoys, fifteen bullets",
    ),
    dict(
        id="sentry_array", name="Sentry Array", family="sentry", glyph="sentry_5", radius=44.0, healthFactor=1.0, pen=False,
        idea="you are inside a clock of guns, and the hand is the only safe place",
        move="eight turrets warp in evenly on a ring of radius 340 wu round the players' centre and stand. Domes of light join each to its neighbours into a fence: ships cannot cross it and shots stop on it. The crowd warps in inside",
        pattern=[
            "the hand: a white line from the centre that points at one turret and moves on by 45 degrees on every beat, one turn every two bars",
            "on every beat every turret but the one under the hand fires a fan of five bullets inward across its own share of the ring, 220 wu/s. Together they are a closing ring with one notch; the bullets end at the centre",
            "each ring's notch is one step round from the last: the player rides the notches round the clock",
            "when a turret dies, the others step round on the next bar line to stand evenly again; the hand keeps its time, so each turret is silent for longer",
        ],
        weakPoint="the turret under the hand has no dome: it is the only one that can be hurt, and it takes x 2. The Array's health is shared evenly between the eight",
        half="with four left the clock runs backwards on every second bar: the hand flashes white a beat ahead and turns the other way. Volleys come on the eighth notes at half the fan (three bullets, then two)",
        death="the last dome turns inside out: every bullet in flight turns gold and falls to the players as Shards, and the fence falls outward",
        sound="the hand is a woodblock on every beat, panned to where it points and one step up the scale for each step round, an octave a turn; the volleys are closed hats; a turret's death is a tom and a gap in the scale; the turn back is a reverse cymbal into its bar line",
        cap="eight turrets, at most 200 bullets",
    ),
    # ---------------------------------------------------------------- batch 3
    dict(
        id="mirror_mimic", name="Mirror Mimic", family="mimic", glyph="mimic_5", radius=60.0, healthFactor=0.9, pen=False,
        idea="it stands where you would be on the far side of the mirror and it shoots when you shoot: fire in phrases",
        move="its place is the nearest player's place turned half round the mirror point, a small white ring where the players' centre was when it arrived; it moves as the player moves, at once. Touching the mirror point is touching the Mimic",
        pattern=[
            "while the player fires, it fires: on every step the player's lead sounds, one bullet leaves it along the player's aim turned half round, 400 wu/s. Aim at it and its fire comes straight back down the line",
            "while the player holds fire, it holds fire. A player who fires for two beats and steps aside for two is never where the answer lands",
            "it copies the player's Dash one beat late: the same length, turned half round",
        ],
        weakPoint="caught in the copy: for the beat of its late Dash and the beat after, it cannot fire and takes damage x 2. At other times x 1",
        half="a second image: one Mimic mirrors through the point, the other across the line through the point that lies level with the frame. Both fire when the player fires; both copy the Dash",
        death="the mirror breaks: glass, and every bullet of its last volley falls as Shards",
        sound="its fire is the player's lead upside down: each lead note answered a thirty-second later by the note as far below the chord's root as the lead's is above, at 0.4. Its Dash is the Dash's sweep reversed. Death is glass",
        cap="two bodies, at most 60 bullets",
    ),
    dict(
        id="twin_tether", name="Twin Tether", family="tether", glyph="tether_5", radius=56.0, healthFactor=1.2, pen=False,
        idea="two bodies and the rope between them: dash through the rope and it goes slack",
        move="two nodes 520 wu apart turn about their middle, one turn every two bars, and the middle follows the nearest player at 60 wu/s. The beam between them is lethal and sweeps everything inside their circle once a bar",
        pattern=[
            "the beam never stops: a player keeps ahead of it, or goes through it. A Dash is invulnerable, and a Dash through the beam cuts a strand",
            "a cut strand: the beam goes dark for one beat and both nodes take x 2 for that beat. A Perfect Dash through it (on a beat) makes it two beats",
            "each node has half the health. They take x 1 at other times, from any side",
        ],
        weakPoint="slack: for one beat after any player dashes through the beam (two after a Perfect Dash), both nodes take damage x 2 and the beam is off",
        half="one node dead: the survivor swings what is left as a flail, a beam 420 wu long turning once a bar about itself. A Dash through it still cuts",
        death="the rope snaps: its two halves whip outward once, as light only, and fall as Shards",
        sound="the beam is a drone whose filter opens as it comes round to the nearest player, so it is heard coming; a cut strand is a zap and the drone falls silent for its beat (silence is safe); each node is a tom as it passes twelve o'clock",
        cap="two nodes and one beam",
    ),
    dict(
        id="weaver_prime", name="Weaver Prime", family="weaver", glyph="weaver_5", radius=54.0, healthFactor=1.0, pen=False,
        idea="it draws a cage round you, a lap a bar, and ties a knot on every One: that is when it stands still",
        move="it circles the nearest player leaving a lethal trail that lasts two bars: each lap takes three beats, starts and ends at the same point, and is 50 wu tighter than the last (300, 250, 200, 150)",
        pattern=[
            "beat 1 of every bar: the knot. It stands still for the beat where the lap began, tying the trail's end to its start",
            "beats 2 to 4: the next lap, 50 wu inside the last. The cage's centre is where the player stood at the first knot; it does not follow",
            "after the fourth lap, on the next bar line, the cage goes off: every trail segment alive bursts 40 wu wide. The middle of the last lap is clear; near the trail is not. Then a new cage, round wherever the player is",
            "a Dash crosses a trail unharmed",
        ],
        weakPoint="the knot: for the beat it stands still on every One, damage x 2. On its laps it takes x 1 and moves at about 1,000 wu/s",
        half="warp and weft: a second Weaver draws the other way round, a lap of its own between the first one's laps, its knot on beat 3",
        death="the cage unravels from the knot outward in one beat and falls as Shards",
        sound="each lap is a saw glide rising over its three beats, a step higher each lap; the knot is a woodblock and a bell on the One; the cage going off is a crackle swelling into the chord's stab",
        cap="two bodies, 160 trail segments (drawn and counted as beams)",
    ),
    dict(
        id="deep_well", name="Deep Well", family="well", glyph="well_5", radius=70.0, healthFactor=0.8, pen=False,
        idea="what you fire into it comes back on the One: so give it more than it can hold",
        move="drifts toward the nearest player at 30 wu/s; shots within 400 wu bend into it and ships within 300 wu are drawn in at up to 120 wu/s",
        pattern=[
            "it swallows every player shot that reaches it and takes no harm. Its four rings light from the inside out as the bar's fill rises",
            "on every One it gives back what the last bar fed it: a ring of bullets, one for every 3 damage swallowed, at most 48, 220 wu/s",
            "its stomach holds 5% of the Track's mini-boss health in one bar. Fed more than that inside a bar, it chokes: no ring on the next One, and for that whole bar its rings are gold and it cannot swallow",
        ],
        weakPoint="choked: for the bar after it is overfed it takes real damage, x 2. That is the only time it can be hurt",
        half="it holds half as much again, drifts at 90 wu/s, and gives the bar back as two rings, on beats 1 and 3",
        death="it bursts: a ring of Shards, one for everything it swallowed in the fight, to 300",
        sound="each swallowed shot is a soft low thup, a step higher for each ring lit, so the fill can be heard; choking is the gong with the bed low-passed for the bar; a ring given back is a reversed cymbal into the One and a stab",
        cap="one body, 96 bullets (two rings alive)",
    ),
]


def build():
    return {
        "note": "Generated by design/tools/minibosses.py. World units. Rules common to every mini-boss, then one entry each. All twelve: batches 1, 2 and 3 (GAME_DESIGN 7.5 and 13).",
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


class Kit:
    """Drawing helpers shared by the preview sheets (panels of PW x PH on canvas cv)."""

    def __init__(self, cv, a, PW, PH):
        self.cv, self.a, self.PW, self.PH = cv, a, PW, PH

    def note(self, text, x, y, cap=13, colour=None, level=1.0):
        return self.cv.text(self.a.font, text, x, y, cap, self.a.white if colour is None else colour, level, max(1.3, cap / 9.0))

    def grid(self, x0, y0, pal, step):
        PW, PH = self.PW, self.PH
        gs = [(gx, y0 + 8, gx, y0 + PH - 8) for gx in np.arange(x0 + step / 2, x0 + PW, step)] + [(x0 + 8, gy, x0 + PW - 8, gy) for gy in np.arange(y0 + step / 2, y0 + PH, step)]
        self.cv.lines(gs, pal["grid"], 0.12, 1.0)

    def body(self, gid, x, y, r, rot, col, partner, level=3.0):
        cv, a = self.cv, self.a
        cv.glow(x, y, r * 3.2, col, 0.16)
        cv.lines(ld.transform(a.segs(gid, (0,)), x, y, r, rot), ld.f_radial(x, y, r * 1.2, [a.white, col, partner]), level, 2.6)
        cv.lines(ld.transform(a.segs(gid, (0,)), x, y, r, rot), a.white, level * 0.5, 0.9)
        cv.lines(ld.transform(a.segs(gid, (1, 2)), x, y, r, rot), partner * 0.5 + a.white * 0.5, level * 0.8, 1.8)

    def small(self, gid, x, y, r, rot, col, level=2.2, width=1.6):
        self.cv.lines(ld.transform(self.a.segs(gid, (0,)), x, y, r, rot), col, level, width)

    def ship(self, x, y, s, aim):
        cv, a = self.cv, self.a
        r = 14 * s
        hull = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["ship"]["segments"]]
        cv.glow(x, y, r * 6, a.p1, 0.15)
        cv.lines(ld.transform(hull, x, y, r, aim), ld.f_radial(x, y, r * 1.3, [a.white, a.p1, a.p1_deep]), 2.8, 2.0)
        cv.lines(ld.transform(hull, x, y, r, aim), a.white, 2.0, 0.8)
        halo = [(g[0], -g[1], g[2], -g[3]) for g in a.glyphs["halo_3"]["segments"]]
        cv.lines(ld.transform(halo, x, y, r, 0.4), ld.f_angular(x, y, [a.p1, a.white, a.p1, a.p1_light]), 1.5, 1.5)

    def danger_line(self, x0, y0, x1, y1, dash=18.0, gap=12.0):
        a = self.a
        ln = math.hypot(x1 - x0, y1 - y0)
        ux, uy = (x1 - x0) / ln, (y1 - y0) / ln
        segs = [(x0 + ux * d, y0 + uy * d, x0 + ux * min(d + dash, ln), y0 + uy * min(d + dash, ln)) for d in np.arange(0, ln, dash + gap)]
        self.cv.threat(segs, a.danger * 0.8 + a.white * 0.2, 2.2, 1.4)

    def bullet(self, x, y, r):
        a = self.a
        self.cv.threat(ld.transform(ld.polygon(4, 1.0, 0.0), x, y, r), a.danger * 0.75 + a.white * 0.25, 2.0, 1.4)

    def pen(self, cx, cy, s):
        cv, a = self.cv, self.a
        h = PEN_SIDE / 2 * s
        box = ld.polyline([(cx - h, cy - h), (cx + h, cy - h), (cx + h, cy + h), (cx - h, cy + h)], closed=True)
        cv.lines(box, a.white, 2.4, 3.0)
        cv.lines(ld.polyline([(cx - h - 7, cy - h - 7), (cx + h + 7, cy - h - 7), (cx + h + 7, cy + h + 7), (cx - h - 7, cy + h + 7)], closed=True), a.white, 0.5, 1.2)
        return h

    def title(self, x0, y0, name, line1, line2, colour):
        a = self.a
        self.note(name, x0 + 44, y0 + 34, 28, a.white, 1.5)
        self.note(line1, x0 + 44, y0 + self.PH - 62, 12, colour)
        self.note(line2, x0 + 44, y0 + self.PH - 38, 12, a.gold)

    def shot(self, x, y, aim, length=26.0):
        a = self.a
        tx, ty = x - math.cos(aim) * length, y - math.sin(aim) * length
        self.cv.lines([(tx, ty, x, y)], ld.f_linear(tx, ty, x, y, [a.p1, a.white]), 4.6, 2.2)


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
    k = Kit(cv, a, PW, PH)
    note, grid, body, small, ship, danger_line, bullet, pen, title = k.note, k.grid, k.body, k.small, k.ship, k.danger_line, k.bullet, k.pen, k.title

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


def preview_2(path):
    """Batch 2: Moth Queen and Brood Mother in Aurora (Track 4), Blink Star and Sentry Array in Furnace (Track 5)."""
    a = Art()
    W, H = 2400, 1080
    PW, PH = W // 2, H // 2
    cv = ld.Canvas(W, H)
    aurora, furnace = a.palette("aurora"), a.palette("furnace")
    cv.radial_bg(aurora["bg0"] * 0.5, aurora["bg1"] * 0.45, cx=0.5, cy=0.5)
    cv.nebula([(600, 270, 620, aurora["accent"], 0.012), (1800, 270, 620, aurora["accent"], 0.012),
               (600, 810, 620, furnace["accent"], 0.016), (1800, 810, 620, furnace["accent"], 0.016)])
    cv.lines([(PW, 20, PW, H - 20), (20, PH, W - 20, PH)], a.white, 0.35, 1.2)
    k = Kit(cv, a, PW, PH)
    circle = lambda x, y, r, n=64: ld.transform(ld.polygon(n, 1.0, 0.0), x, y, r)   # noqa: E731

    # ---- 1: Moth Queen (Aurora): the court half closed, the Queen firing rosettes ------------
    x0, y0, u = 0, 0, 0.60                                   # u = pixels per world unit in this panel
    k.grid(x0, y0, aurora, 60 * u * 1.5)
    col, par = aurora["enemies"][1]
    sx, sy = x0 + 372, y0 + 262
    cv.lines(circle(sx, sy, 420 * u, 96)[::2], col, 0.35, 1.2)                     # where the court warped in
    cv.lines(circle(sx, sy, 140 * u), a.white, 0.8, 1.4)                           # where it closes
    ring = 280 * u
    for i in range(12):
        t = i * 2 * PI / 12 + 0.2
        mx, my = sx + math.cos(t) * ring, sy + math.sin(t) * ring
        if i % 3 == 0:                                                             # the way it came: a quarter turn a bar, inward
            path_ = [(sx + math.cos(t - v * 0.9) * (ring + v * 140 * u), sy + math.sin(t - v * 0.9) * (ring + v * 140 * u)) for v in np.linspace(0, 1, 10)]
            cv.lines(ld.polyline(path_), col, 0.45, 1.2)
        k.small("moth_2", mx, my, 15, t, col * 0.75 + a.white * 0.25, 2.4, 1.6)
    qa = math.radians(-14.0)
    qx, qy = sx + math.cos(qa) * 520 * u, sy + math.sin(qa) * 520 * u
    k.body("moth_5", qx, qy, 70 * u * 1.15, 0.2, col, par)
    for gen in range(1, 6):                                                        # rosettes: five petals, 12 degrees on each time
        for j in range(5):
            an = j * 2 * PI / 5 + math.radians(12.0 * gen)
            rr = (70 + 62 * gen) * u
            k.bullet(qx + math.cos(an) * rr, qy + math.sin(an) * rr, 6.5)
    k.ship(sx, sy, 1.25, qa)
    k.title(x0, y0, "MOTH QUEEN", "HER COURT CLOSES ON YOU OVER THREE BARS AND BURSTS ON THE FOURTH", "WHILE SHE CALLS THE NEXT COURT SHE IS OPEN: X2", col)

    # ---- 2: Brood Mother (Aurora): the dome shut, a clutch laid, four broodlings out ---------
    x0, y0, u = PW, 0, 0.90
    k.grid(x0, y0, aurora, 60 * u)
    col, par = aurora["enemies"][0]
    mx, my = x0 + 790, y0 + 250
    sx, sy = x0 + 300, y0 + 300
    k.body("brood_5", mx, my, 90 * u * 0.9, 0.0, col, par)
    cv.lines(circle(mx, my, 128 * u), a.white, 1.5, 2.0)                           # the dome, shut
    cv.lines(circle(mx, my, 136 * u), a.white, 0.4, 1.1)
    for i in range(6):                                                             # the clutch: six eggs at 170 wu
        t = i * PI / 3 + 0.35
        ex, ey = mx + math.cos(t) * 170 * u, my + math.sin(t) * 170 * u
        cv.lines(circle(ex, ey, 9, 16), a.white, 2.0, 1.6)
        cv.lines(circle(ex, ey, 15, 20), a.white, 0.5, 1.0)
        cv.glow(ex, ey, 22, a.white, 0.25)
    for (bx, by) in ((x0 + 520, y0 + 205), (x0 + 455, y0 + 330), (x0 + 600, y0 + 365), (x0 + 395, y0 + 235)):   # broodlings and their threads
        cv.lines([(bx, by, mx + (bx - mx) / math.hypot(bx - mx, by - my) * 128 * u, my + (by - my) / math.hypot(bx - mx, by - my) * 128 * u)], col, 0.7, 1.2)
        k.small("brood_1", bx, by, 11, math.atan2(sy - by, sx - bx), col * 0.7 + a.white * 0.3, 2.6, 1.6)
    aim = math.atan2(235 - 300, 395 - 300)
    k.ship(sx, sy, 1.25, aim)
    for d in (38, 74):
        k.shot(sx + math.cos(aim) * d, sy + math.sin(aim) * d, aim)
    k.title(x0, y0, "BROOD MOTHER", "SIX EGGS ON BEAT 1, HATCHED ON BEAT 3. EVERY BROODLING HOLDS HER DOME SHUT", "NONE ALIVE: THE DOME BREAKS, X2. SHOOT THE EGGS AND IT STAYS BROKEN", col)

    # ---- 3: Blink Star (Furnace): three edges drawn, two to come ----------------------------
    x0, y0, u = 0, PH, 0.52
    k.grid(x0, y0, furnace, 60 * u * 1.5)
    col, par = furnace["enemies"][1]
    cx, cy = x0 + 600, y0 + 258
    R = 420 * u
    pts = [(cx + math.cos(-PI / 2 + j * 2 * PI / 5) * R, cy + math.sin(-PI / 2 + j * 2 * PI / 5) * R) for j in range(5)]
    order = [0, 2, 4, 1, 3, 0]
    for n in range(5):
        p, q = pts[order[n]], pts[order[n + 1]]
        if n < 3:
            k.danger_line(p[0], p[1], q[0], q[1], 16.0, 11.0)
        else:                                                                      # the edges it has yet to draw
            ln = math.hypot(q[0] - p[0], q[1] - p[1])
            cv.lines([(p[0] + (q[0] - p[0]) * d / ln, p[1] + (q[1] - p[1]) * d / ln, p[0] + (q[0] - p[0]) * (d + 4) / ln, p[1] + (q[1] - p[1]) * (d + 4) / ln)
                      for d in np.arange(0, ln - 4, 16)], a.white, 0.45, 1.3)
    inner = [(cx + math.cos(PI / 2 + j * 2 * PI / 5) * R * 0.382, cy + math.sin(PI / 2 + j * 2 * PI / 5) * R * 0.382) for j in range(5)]
    cv.fill(inner, a.p1, 0.05)
    cv.lines(ld.polyline(inner, closed=True), a.p1, 0.7, 1.3)                      # the room in the middle
    for n in range(5):
        px, py = pts[order[n]]
        ox, oy = (px - cx) / R, (py - cy) / R
        off = 74 if n == 3 else 34                                                 # clear of the body standing at 4
        k.note(str(n + 1), px + ox * off - 6, py + oy * off - 8, 16, a.gold, 1.3)
        if n < 3:
            cv.flare(px, py, 44, a.white, 1.2, 1.1, 4, PI / 4)
    bx, by = pts[order[3]]
    k.body("blink_5", bx, by, 60 * u * 1.25, 0.3, col, par)
    k.ship(cx + 14, cy + 8, 1.25, math.atan2(pts[0][1] - cy, pts[0][0] - cx))
    k.title(x0, y0, "BLINK STAR", "FIVE HOPS, ONE A BEAT. THEN EVERY EDGE IT DREW FIRES", "IT ENDS WHERE IT BEGAN, AT 1, DAZED FOR TWO BEATS: X2", col)

    # ---- 4: Sentry Array (Furnace): two rings in flight, the hand on the silent turret ------
    x0, y0, u = PW, PH, 0.55
    k.grid(x0, y0, furnace, 60 * u * 1.5)
    col, par = furnace["enemies"][0]
    cx, cy = x0 + 600, y0 + 240
    R = 340 * u
    ang = lambda j: -PI / 2 + j * PI / 4                                           # noqa: E731
    hand = 1
    cv.lines(circle(cx, cy, R, 96), a.white, 0.9, 1.6)                             # the fence
    for j in range(8):
        tx, ty = cx + math.cos(ang(j)) * R, cy + math.sin(ang(j)) * R
        k.body("sentry_5", tx, ty, 44 * u * 1.05, ang(j) + PI, col, par, 2.6)
        if j == hand:
            cv.discs([(tx, ty, 6)], a.gold * 0.7 + a.white * 0.3, 2.6)
            cv.glow(tx, ty, 60, a.gold, 0.45)
        else:
            cv.lines(circle(tx, ty, 44 * u * 1.5, 40), a.white, 1.3, 1.6)          # its dome
    hx, hy = cx + math.cos(ang(hand)) * (R - 40), cy + math.sin(ang(hand)) * (R - 40)
    cv.lines([(cx, cy, hx, hy)], a.white, 1.5, 1.6)                                # the hand
    cv.discs([(cx, cy, 4)], a.white, 1.6)
    for beats_ago in (1, 2):                                                       # each ring's notch is one step behind the hand
        rr = R - 110 * u * beats_ago - 18
        notch = hand - beats_ago
        for j in range(8):
            if (j - notch) % 8 == 0:
                continue
            for f in (-18, -9, 0, 9, 18):
                an = ang(j) + math.radians(f)
                k.bullet(cx + math.cos(an) * rr, cy + math.sin(an) * rr, 6.0)
    sa = ang(hand - 1) + 0.12
    px, py = cx + math.cos(sa) * 150 * u, cy + math.sin(sa) * 150 * u
    tx, ty = cx + math.cos(ang(hand)) * R, cy + math.sin(ang(hand)) * R
    aim = math.atan2(ty - py, tx - px)
    k.ship(px, py, 1.25, aim)
    for d in (40, 78):
        k.shot(px + math.cos(aim) * d, py + math.sin(aim) * d, aim)
    k.title(x0, y0, "SENTRY ARRAY", "EVERY BEAT ALL BUT ONE FIRE INWARD. THE NOTCH GOES ROUND WITH THE HAND", "THE TURRET UNDER THE HAND HAS NO DOME: X2. THE FENCE HOLDS YOU IN", col)

    cv.render(bloom=0.95).save(path, optimize=True)


def preview_3(path):
    """Batch 3: Mirror Mimic in Zenith (Track 6), Twin Tether and Weaver Prime in Afterglow (Track 7), Deep Well in Arcadia (Track 8)."""
    a = Art()
    W, H = 2400, 1080
    PW, PH = W // 2, H // 2
    cv = ld.Canvas(W, H)
    zenith, glow_, arcadia = a.palette("zenith"), a.palette("afterglow"), a.palette("arcadia")
    cv.radial_bg(glow_["bg0"] * 0.5, glow_["bg1"] * 0.45, cx=0.5, cy=0.5)
    cv.nebula([(600, 270, 620, zenith["accent"], 0.010), (1800, 270, 620, glow_["accent"], 0.012),
               (600, 810, 620, glow_["accent"], 0.012), (1800, 810, 620, arcadia["accent"], 0.012)])
    cv.lines([(PW, 20, PW, H - 20), (20, PH, W - 20, PH)], a.white, 0.35, 1.2)
    k = Kit(cv, a, PW, PH)
    circle = lambda x, y, r, n=64: ld.transform(ld.polygon(n, 1.0, 0.0), x, y, r)   # noqa: E731

    # ---- 1: Mirror Mimic (Zenith): the player fires at it; its fire comes back down the line ----
    x0, y0, u = 0, 0, 0.62
    k.grid(x0, y0, zenith, 60 * u * 1.5)
    col, par = zenith["enemies"][1]
    mx, my = x0 + 600, y0 + 258                                   # the mirror point
    sx, sy = mx - 300, my + 120
    ex, ey = 2 * mx - sx, 2 * my - sy
    cv.lines(circle(mx, my, 16, 24), a.white, 1.6, 1.6)
    ln = math.hypot(ex - sx, ey - sy)
    cv.lines([(sx + (ex - sx) * d / ln, sy + (ey - sy) * d / ln, sx + (ex - sx) * (d + 6) / ln, sy + (ey - sy) * (d + 6) / ln) for d in np.arange(40, ln - 40, 18)], a.white, 0.4, 1.2)
    aim = math.atan2(ey - sy, ex - sx)
    k.ship(sx, sy, 1.25, aim)
    for d in (50, 100, 150):
        k.shot(sx + math.cos(aim) * d, sy + math.sin(aim) * d, aim)
    k.body("mimic_5", ex, ey, 60 * u * 1.2, aim + PI, col, par)
    for d in (70, 125, 180, 235):                                 # its answer, coming back down the line
        bx, by = ex - math.cos(aim) * d, ey - math.sin(aim) * d
        cv.threat([(bx, by, bx + math.cos(aim) * 16, by + math.sin(aim) * 16)], a.danger * 0.75 + a.white * 0.25, 2.2, 1.4)
    k.note("MIRROR", mx - 36, my - 44, 10, a.white, 0.9)
    k.title(x0, y0, "MIRROR MIMIC", "IT STANDS WHERE YOU WOULD BE ACROSS THE MIRROR AND FIRES WHEN YOU FIRE", "IT COPIES YOUR DASH A BEAT LATE AND IS CAUGHT IN THE COPY: X2", col)

    # ---- 2: Twin Tether (Afterglow): the beam sweeping, a Dash through it ----------------------
    x0, y0, u = PW, 0, 0.62
    k.grid(x0, y0, glow_, 60 * u * 1.5)
    col, par = glow_["enemies"][1]
    cx, cy = x0 + 620, y0 + 255
    ang, half = math.radians(-28.0), 260 * u
    n0 = (cx + math.cos(ang) * half, cy + math.sin(ang) * half)
    n1 = (cx - math.cos(ang) * half, cy - math.sin(ang) * half)
    cv.lines(circle(cx, cy, half, 72)[::2], col, 0.35, 1.2)                           # the circle the nodes turn on
    cv.lines(ld.transform(ld.arc(1.0, ang + 0.25, ang + 0.9, 12), cx, cy, half + 26), a.white, 0.9, 1.4)   # which way it turns
    cv.threat([(n0[0], n0[1], n1[0], n1[1])], a.danger * 0.75 + a.white * 0.25, 3.2, 1.6)
    cv.lines([(n0[0], n0[1], n1[0], n1[1])], a.danger, 2.4, 3.0)
    for (nx, ny) in (n0, n1):
        k.body("tether_5", nx, ny, 56 * u * 1.1, ang, col, par, 2.6)
    px, py = cx - 60, cy + 95                                       # the ship, dashing up through the beam
    for i, g in enumerate((0.25, 0.4, 0.6)):                        # Dash ghosts behind it
        gx, gy = px + 22 * (2 - i) - 12, py + 40 * (2 - i) - 28
        hull = [(q[0], -q[1], q[2], -q[3]) for q in a.glyphs["ship"]["segments"]]
        cv.lines(ld.transform(hull, gx, gy, 14 * 1.25, math.radians(-118)), a.p1, 2.8 * g, 2.0)
    k.ship(px - 40, py - 112, 1.25, math.radians(-118))
    cv.flare(px - 12, py - 60, 50, a.white, 1.6, 1.2, 4, PI / 4)
    k.title(x0, y0, "TWIN TETHER", "THE BEAM SWEEPS THE CIRCLE ONCE A BAR. A DASH GOES THROUGH IT", "A DASH THROUGH THE BEAM CUTS A STRAND: DARK FOR A BEAT, BOTH NODES X2", col)

    # ---- 3: Weaver Prime (Afterglow): two laps drawn, the knot on the One ----------------------
    x0, y0, u = 0, PH, 0.62
    k.grid(x0, y0, glow_, 60 * u * 1.5)
    col, par = glow_["enemies"][0]
    cx, cy = x0 + 600, y0 + 250
    for lap, r in enumerate((300, 250)):
        ring = [(cx + math.cos(-PI / 2 + t) * r * u, cy + math.sin(-PI / 2 + t) * r * u) for t in np.linspace(0, 2 * PI, 60)]
        cv.threat(ld.polyline(ring), a.danger * 0.75 + a.white * 0.25, 2.0, 1.3)
        cv.lines(ld.polyline(ring), a.danger, 1.4 if lap == 0 else 2.2, 2.4)
    cv.threat([(cx, cy - 300 * u, cx, cy - 250 * u)], a.danger * 0.75 + a.white * 0.25, 2.0, 1.3)   # lap to lap
    kx, ky = cx, cy - 250 * u
    k.body("weaver_5", kx, ky, 54 * u * 1.15, 0.3, col, par)
    cv.discs([(kx, ky, 6)], a.gold * 0.7 + a.white * 0.3, 2.6)
    cv.glow(kx, ky, 60, a.gold, 0.45)
    cv.lines(circle(cx, cy, 200 * u, 64)[::3], a.white, 0.45, 1.2)                    # the lap to come
    aim = -PI / 2
    k.ship(cx, cy + 20, 1.25, aim)
    for d in (44, 84):
        k.shot(cx + math.cos(aim) * d, cy + 20 + math.sin(aim) * d, aim)
    k.title(x0, y0, "WEAVER PRIME", "A LAP A BAR, EACH ONE TIGHTER. AFTER FOUR THE CAGE GOES OFF", "IT TIES A KNOT ON EVERY ONE AND STANDS STILL FOR THE BEAT: X2", col)

    # ---- 4: Deep Well (Arcadia): shots bending in, three rings lit, last bar's ring going out ---
    x0, y0, u = PW, PH, 0.62
    k.grid(x0, y0, arcadia, 60 * u * 1.5)
    col, par = arcadia["enemies"][2]
    wx, wy = x0 + 700, y0 + 250
    k.body("well_5", wx, wy, 70 * u * 1.2, 0.0, col, par, 2.2)
    for i, r in enumerate((16, 30, 44)):                                               # three of its four rings lit
        cv.lines(circle(wx, wy, r, 32), a.gold * 0.5 + a.white * 0.5, 2.6, 2.0)
    sx, sy = x0 + 300, y0 + 330
    k.ship(sx, sy, 1.25, math.atan2(wy - sy, wx - sx) - 0.25)
    for lane in (-0.42, -0.25, -0.08):                                                 # shots curving into it
        a0 = math.atan2(wy - sy, wx - sx) + lane
        pts = []
        for t in np.linspace(0.08, 1.0, 14):
            bx = sx + math.cos(a0) * 420 * t
            by = sy + math.sin(a0) * 420 * t
            pull = t * t
            pts.append((bx + (wx - bx) * pull * 0.9, by + (wy - by) * pull * 0.9))
        cv.lines(ld.polyline(pts), ld.f_linear(pts[0][0], pts[0][1], pts[-1][0], pts[-1][1], [a.p1, a.white]), 2.6, 1.8)
    for j in range(22):                                                                # what it gave back on the last One
        an = j * 2 * PI / 22 + 0.1
        k.bullet(wx + math.cos(an) * 205, wy + math.sin(an) * 205, 6.0)
    k.title(x0, y0, "DEEP WELL", "IT SWALLOWS YOUR SHOTS AND GIVES THEM BACK AS A RING ON THE ONE", "FEED IT MORE THAN IT HOLDS IN ONE BAR AND IT CHOKES: X2, NO RING", col)

    cv.render(bloom=0.95).save(path, optimize=True)


def main():
    data = build()
    os.makedirs(os.path.join(DESIGN, "bosses"), exist_ok=True)
    with open(os.path.join(DESIGN, "bosses", "minibosses.json"), "w", encoding="utf-8") as f:
        json.dump(data, f, indent=1)
    for name, draw in (("minibosses_1.png", preview), ("minibosses_2.png", preview_2), ("minibosses_3.png", preview_3)):
        if len(sys.argv) > 1 and sys.argv[1] not in name:
            continue
        out = os.path.join(DESIGN, "previews", name)
        draw(out)
        print("wrote", out)


if __name__ == "__main__":
    main()
