# POLYRHYTHM — boss of Track 5

Two rotors joined by a ruled line of light: **Three**, a cyan triangle, and **Four**, an amber
square, 720 wu apart. Three fires three times a bar, Four fires four times a bar, and what they
fire is walls. It arrives on the last Drop of Track 5 "Furnace" (144 BPM, G phrygian) and stays
until both are dead; the music holds its Drop section for the whole fight.

Kaleidos: hit the quiet part. Longwave: move with the music. The Choir: listen ahead. Event
Horizon: the bar is the unit. Polyrhythm asks for all of it at once:

1. **Two rhythms, one body.** Each rotor alone is easy to read. Together they are three against
   four, and the player has to hold both counts.
2. **The gap is the weak point.** Every ring has a gap in front of every flat face, and the faces
   are the only place a rotor can be hurt. Where it is safe is where you do damage.
3. **The One.** Once a bar both fire together with their gaps along the link. Everything else in
   the bar is getting back there.

Data: `design/bosses/polyrhythm.json` (parts, numbers, patterns — load it, do not retype it).
Picture: `design/previews/boss_polyrhythm.png`. Tool: `design/tools/boss_polyrhythm.py`.

---

## Anatomy (world units)

| Part | Shape | Size |
|---|---|---|
| **Three** | A triangle with a smaller one turning the other way inside it; a bright node on each corner; a pane of glass with a gold bar behind it on each flat face | Radius 92. 1,400 health |
| **Four** | A square built the same way | Radius 92. 1,400 health |
| **Link** | A white line from Three to Four ruled into twelve: the bar in twelfths. Three's ticks (0, 4, 8) are cyan, Four's (0, 3, 6, 9) amber, tick 0 gold. A playhead runs along it once a bar and the next tick to fire glows a beat ahead | 720 long |
| **Walls** | Solid arcs of light thrown by the corners, growing outward | 10 thick, 260 wu/s, 2.6 s |

## Rules

1. **Walls, not bullets.** On each of its pulses a rotor throws one arc from every corner (Three:
   three arcs of 80°; Four: four arcs of 60°). There is no slipping between them: only the gaps,
   or a Dash, get through. A Nova erases every wall it touches.
2. **A gap in front of every face**, 40° wide for Three and 30° for Four, and it stays in front of
   that face's direction as the wall grows.
3. **Only the faces can be hurt.** Corners and body shed shots. Stand in a gap and the gold bar
   behind the glass is open to you.
4. **The rotors turn between pulses**: Three by 40°, Four by 22.5°. So each new ring's gaps are
   a step round from the last, and riding one rotor's gaps means circling it at its own tempo.
   Both are back where they started every bar.
5. **The One.** On beat 1 both fire together, and those two rings have a gap along the link: they
   pass either side of anyone standing on it, and each rotor's face is open down its own gap.
   Rings from the other pulses still cross the link. The One is a moment, not a shelter.
6. **The link is the score.** It shows what fires next and when; it does not hurt, except in the
   second phase while it burns.
7. The pivot (the link's middle) follows the players' centre at 50 wu/s and the link turns once
   every 8 bars. **Everything is a function of song time**, as for every boss.

## Phases

| Phase | When | Change |
|---|---|---|
| 1 **Three against four** | Both alive, above 60% of their joint health | The pattern as ruled |
| 2 **Hemiola** | Both alive, 60% and below | Each rotor reverses its turn every bar, so the gaps swing back and forth instead of circling. Four's walls fly at 300. Every second bar the **link burns** on beats 3 and 4, after a bar of danger dashes: the aisle is still open on the One, but nobody can live in it |
| 3 **Solo** | One rotor dead | The survivor plays both parts: its own ring on its own pulses, and the dead rotor's count on a second ring that turns the other way. The two rings' gaps line up once a bar, on the One, in front of one face. It walks toward the nearest player at 70 wu/s and takes × 1.25 |

Kill them close together and the Solo is short. Kill one early and the other carries both parts for
a long time: balancing the damage is the players' choice.

There are no bullets in this fight: at most about 30 walls are alive at once.

## Numbers

| | Value |
|---|---|
| Health | 1,400 each (× 1 + 0.5 per extra player) |
| Damage scale | Face × 1 · corner and body 0 · the Solo survivor's faces × 1.25 |
| Contact | A rotor hurts. A wall takes a Shield cell like a bullet and is erased where it hit |
| Camera | 0.75 for the fight, both rotors in the frame |
| Crowd | Half, fodder only; walls pass through enemies |
| Reward | A bonus Trio and 240 Shards per player; the Track ends |

With the build a player has by the end of Track 5 (about 80 damage a second on target) and a face
in reach perhaps a third of the time, expect 100–120 seconds. Tune the health with the bot once it
can ride a gap.

## Sound

- **Three** is the tom, tuned to the Track's root, on twelfths 0, 4 and 8. **Four** is the rim,
  tuned to the flat second, on twelfths 0, 3, 6 and 9: the Track's own two-chord vamp, played as
  three against four. The One is both and the gong.
- Each drum rises a semitone for every quarter of its rotor's health lost.
- In the Solo the survivor plays both drums at its own pitch.
- The link burning: the telegraph tick on each beat of the telegraph bar, the impact on beat 3.

## Look

- Three is the palette's cyan, Four its amber; the link, the nodes and the One are white; the
  weak points are gold. Walls are the danger colour on the priority layer, 3 px with the threat
  rim, thinning to 2 px in their last half second.
- A corner glints in the danger colour one twelfth before it fires.
- On the One a white flare runs the link and two faint rails mark the aisle through both sets of
  rings.
- The lattice gets an impulse of 160 under each rotor on each of its pulses, so the grid beats
  three against four as well.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat Three drops in on beat 1 and Four on beat 3, each landing
  with its drum; the link draws itself between them over the next bar, tick by tick; the name card
  holds for two bars; firing starts on bar 3.
- **Death:** the first rotor to die spins down over a bar and bursts on the One. When the second
  dies the playhead runs one last bar with every tick firing a ring of sparks instead of walls,
  then the boss break on the One — three rings, 300 sparks, the 30 px distortion ring, 50 ms of
  hit-stop — then the Shards.
