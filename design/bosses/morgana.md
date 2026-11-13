# FATA MORGANA — boss of Track 9, Mirage

The safe place moves. A storm closes the frame and the only calm is an oasis that drifts; later there are two, and one of them is not there.

168 BPM, B flat lydian: B flat major 7, C over B flat, G minor 7, F add 9. Data: `design/bosses/morgana.json` (load it, do not retype it). Picture: `design/previews/bosses_7_10.png`.
Tool: `design/tools/bosses_7_10.py`. It on the last Drop of its Track and stays until it is dead; the music holds its Drop for the whole fight.

What it teaches:

1. Stay in the calm, and the calm does not stay still.
2. A mirage blinks on the snare. The real water never blinks.
3. It crosses the water every fourth bar: that is when it can be caught.

---

## Parts

| Part | What it is |
|---|---|
| **The storm** | Everything outside the oasis. It hurts on the kick: a ship outside the calm when a kick lands (this Track's broken kick: steps 0, 6 and 10) loses a cell. Drawn as sixty streaks of sand in the danger colour at 0.5 streaming across the frame on the priority layer, with the lattice whipped 10 wu along the wind; never a fill |
| **The oasis** | A disc of calm 260 wu in radius. Its edge is a ring in the palette's accent at 2.2 with a second ring 12 wu inside it that swells on the beat; inside, the lattice is at rest and tinted with the accent |
| **Morgana** | A body of radius 76 made of three veils (broken arcs) round a white core, turning against each other. She rides the storm just outside the oasis's edge |

## Phases

| Phase | Health | What happens |
|---|---|---|
| **Drift** | 100 to 66% | The oasis drifts; the player walks with it. On each kick she fires a fan of five into the calm at the nearest player: three fans a bar, in the kick's own rhythm<br>Every fourth bar the oasis rests for the bar and she crosses it from side to side: x 2 while she is over the water; x 1 at other times |
| **Mirage** | 66 to 33% | At every fourth bar line a second oasis appears 500 wu from the first. One is real. The mirage goes out for an eighth on every snare (two and four) and after two bars it is gone; a ship in it is then in the storm<br>Which one is real is drawn by the seed. The real one never blinks |
| **Crossing** | 33 to 0% | Every second bar the oasis leaps: the next one is drawn dashed 600 wu away for the bar before; on the bar line the old one shrinks to nothing over one bar while a corridor of calm 160 wu wide joins the two<br>She leaps with it and lands dazzled: x 2 for the first beat after each leap |

## Numbers

| | |
|---|---|
| Health | 7,600 to start from. The number given is a first guess: tune it with the careful bot until the fight lasts what the page says with the build a player has by then, as was done for Longwave and The Choir; x (1 + 0.5 per extra player) |
| oasisRadius | 260.0 |
| bodyRadius | 76.0 |
| drift | the oasis drifts along a figure of eight 900 wu wide at 110 wu/s |
| orbit | Morgana circles the oasis 70 wu outside its edge, one turn every four bars |
| fanBullets | 5 |
| bulletSpeed | 200.0 |
| Fight length to tune to | 90 to 120 seconds |
| Reward | A bonus Trio and 380 Shards for each player; the Track ends |
| Weak point | X 2 while she crosses the resting oasis (one bar in four), and for the beat after each leap in the last phase |
| Crowd | Half of the Drop's crowd, fodder only, unless the page says otherwise |
| Camera | Holds 0.85 for the fight (GAME_DESIGN 9) |
| Caps | One body, 90 bullets, 60 streaks of sand (light only) |
| On the grid | Everything it does lands on the song's grid and is computed from the song's clock, so every device in a co-op run sees the same fight with nothing new on the wire |

## Death

The storm drops at once: the whole frame is calm, the lattice takes the oasis's tint from edge to edge, and the sand falls as Shards.

## Sound

- The storm is a crackle and a whoosh that are heard outside the oasis; outside it the music bus is also low-passed at 2 kHz, so the calm is where the song sounds whole.
- A mirage's blink is a soft tick on the snare.
- The leap is the lead's arpeggio as a run of chirps, up.
- While she is over the water, a bell on every beat.

In the words of `design/music/sound_m5.json`'s recipes; a sound they cannot express is reported, not improvised.
