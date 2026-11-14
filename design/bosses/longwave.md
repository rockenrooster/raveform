# LONGWAVE — boss of Track 2

A serpent whose body is a travelling sine wave: a head, twenty-four segments and a whip of a tail,
2,400 wu from end to end. It arrives on the last Drop of Track 2 "Solar Wind" (126 BPM) and stays until
it is dead; the music holds its Drop section for the whole fight.

Kaleidos taught that attacks land on the beat and that the quiet part is the part to hit. Longwave
teaches the next thing:

1. **Move with the music.** The weak point travels, on the grid, at a speed you can fly.
2. Standing still is safe and useless; flying along with the wave is the whole fight.
3. You can hear where the weak point is: it is a scale running up.

Data: `design/bosses/longwave.json` (parts, numbers, patterns — load it, do not retype it).
Picture: `design/previews/boss_longwave.png`. Tool: `design/tools/boss_longwave.py`.

---

## Anatomy (world units)

| Part | Shape | Size |
|---|---|---|
| **Head** | Two mandibles that open like a beak, a crown of three spines behind them, two star eyes, and a seed of light inside the jaw | Radius 70. 600 health |
| **Segments** | Twenty-four lozenges, 100 wu apart along the spine, each with a swept fin on either flank. The fin on the side a segment is swinging toward opens as it crests | Radius 34. 60 health each |
| **Spine** | One line of light through every live segment, head to tail | 3.4 px with a 1.2 px core |
| **Tail** | Three blades in a fan: the whip | 90 wu |
| **Echoes** | Two faint copies of the spine line where the body was an eighth and a quarter of a wave ago: the wave's after-images | At 0.22 and 0.10 |

## Rules

1. **The body is a wave.** Every segment swings sideways off the spine as a sine: 150 wu each way,
   eight segments to a wavelength, so a whole body carries three crests and three troughs.
2. **The crest is where it hurts.** The segment at the top of the wave is white-hot and takes full
   damage for that one grid step. Every other segment is armoured: shots that hit armour spark and
   stop, and do a quarter of their damage (M5, 2026-11-13: it was none, nothing on screen said so,
   and the user never got past this boss; GAME_DESIGN 7.6, "A boss's rule is a bonus, not a wall").
3. **The crest runs from head to tail, one segment per step.** In the first phase a step is an eighth
   note, which makes the crest travel at 420 wu/s: slower than the ship (520). Fly beside it and every
   shot counts in full; stand still and seven in eight count a quarter.
4. **Troughs fire.** The segment at the bottom of the wave spits to both sides, square to the body, on
   the same steps. So the dangerous side of the wave is always half a wavelength from the safe one, and
   it moves with it.
5. **The jaw opens on beat 1.** For the first beat of every bar the mandibles part and the head takes
   full damage through them; closed, it is armour like the body (a quarter). A horn marks it. **Kill the head and it is over**, whatever is left of the body.
6. **A dead segment closes up.** The body behind it slides up along the spine to close the gap over one
   step, its swing easing to its new place in the wave as it goes, with a white flash at the join. The
   body is one segment shorter and the crest skips on without missing a step: the crest counts the new
   body at once; only the places ease.
7. **Everything is a function of song time**, as for every boss: the head's place on its path, the
   wave, every bullet. Only health, and the times at which phases changed, are shared in co-op.

## Phases

The phase is how many segments are left. The wave speeds up; the body speeds up; it never jumps.
**Nothing on it is ever placed, only moved** (2026-11-14, the owner: it "jumps across the screen every so
often"): no part of the boss travels farther in a frame than its speed allows, and the body only ever
moves the way the head went.

| Phase | Segments | Crest step | Crest speed | Lap of the path | Attack |
|---|---|---|---|---|---|
| 1 **Swell** | 24–17 | 1/8 | 420 wu/s | 16 bars | Each trough: one bullet to each side on its step |
| 2 **Chop** | 16–9 | 1/8 triplet | 630 wu/s | 12 bars | Each trough: a fan of three to each side, 16° apart. The crest now outruns the ship: each one overtakes you slowly, and the next is close behind |
| 3 **Whitecap** | 8–1 | 1/16 | 840 wu/s | 8 bars | Troughs as phase 1, on sixteenths. Every second bar the head **lunges**: a dashed line for a bar through where the nearest player stood, then the head dashes 700 wu along it over the downbeat's beat, jaw open for two beats. The body follows down the spine: each segment makes the same move a sixteenth after the one ahead of it, so the dash runs down the body like a crack of the whip, and the figure of eight goes on from where the head landed |
| **Bare head** | 0 | — | — | — | A lunge every bar; a ring of 16 on beats 2 and 4, alternate rings turned half a step. The open jaw takes triple damage |

On the downbeat after each phase change the tail **cracks**: a ring of 32 from its tip.

Bullets fly at 220 wu/s and live five seconds. The most alive at once is about 380 (phase 2).

## Numbers

| | Value |
|---|---|
| Segment health / head health | 60 / 600 (× 1 + 0.5 per extra player, as for every boss) |
| Damage scale | Crest × 1 · any other segment × 0.25 · head through the open jaw × 1 (× 3 when bare) · closed jaw × 0.25 |
| Contact | Head and segments hurt |
| Path | A figure of eight, 3,000 wu wide and 1,040 tall, around the centre of the players when it arrives (kept 1,100 wu from every wall). The head swims it; segment k is 100 × k wu behind along the same path, then swings with the wave |
| Camera | Pulls back to 0.75 for the fight so the wave can be read, eased over the entrance bar |
| Other enemies during the fight | The director's budget drops to 30% and only fodder families spawn |
| Reward | A bonus Trio and 200 Shards per player; the Track ends |

With the build a player has by the end of Track 2 (about 30 damage a second on target), riding one
crest down the whole body takes three bars and strips about seven health from every segment; expect
70–100 seconds for the fight. A player who never moves with the wave does about a third of that
damage (an eighth before armour took a quarter). The health is tuned with the novice bot, which
does not surf (GAME_DESIGN 7.6): it must win, in at most 150 seconds.

**Hint line** (screens.md): `SHOOT THE WHITE SEGMENT`.

## Sound

- **The run.** The boss owns a rubber-string voice (the bass patch an octave up, twice the decay): one
  note on every crest step, its pitch the crest's place in its wave as a degree of the Track's scale —
  0, 2, 3, 5, 7, 8, 10, 12 above the chord root. The crest running down the body is a scale running up,
  once a bar. In phase 2 it becomes triplets, in phase 3 sixteenths: the fight audibly accelerates.
- Troughs tick (the telegraph tick) on every step they fire.
- The jaw opening is the low fifth of the elite-arrives patch, on beat 1.
- The whip is the impact, a fifth up, on its downbeat.

## Look

- The body's colour runs along it: the run's first enemy hue at the head, the second in the middle,
  the third at the tail, each toward its partner at the flanks. Segments are drawn as elites (white
  copy of the hull, glass).
- **Second colour family:** electric blue (OKLCH lightness 0.70, hue 258), the complement of the Solar
  palette: fins, crown, eyes, the spine's core line and the echoes. Nothing else on screen has it.
- The crest is white-hot: a white copy of its hull at 2.6, glass at 0.18, its leading fin white, a long
  two-arm flare across the body and a short one along it. The two segments either side of it are
  warming or cooling (white copy scaled by the wave's height cubed), so the eye sees the crest coming.
- A trough about to fire has a pool of the danger colour under it; its bullets are upright diamonds
  on the priority layer, like every threat.
- The echoes are what make it a wave and not a wiggle: keep them even on the lowest tier (they are
  two polylines).
- The lattice answers the body: one impulse of 140 under each crest on each step, so the grid shows
  the wave's wake.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat the head breaks in from the nearest wall and the body follows
  it onto the path over two bars, already waving; the name card holds for two bars.
- **Segment death:** it shatters along the wave's direction; the join flashes white.
- **Death:** the jaw locks open, the clamped white flash, the rest of the body unravels tail-first,
  one segment per sixteenth, each a medium kill; then the head as a boss break — three rings, 300
  sparks, the 30 px distortion ring, 50 ms of hit-stop — then the Shards.
