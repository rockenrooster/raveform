# KALEIDOS, REPRISE — boss of Track 10, Last Light

The first boss comes back as the first theme does. Six arms, and each one plays one boss the player has already beaten.

174 BPM, A minor: First Light's chords and lead. Data: `design/bosses/reprise.json` (load it, do not retype it). Picture: `design/previews/bosses_7_10.png`.
Tool: `design/tools/bosses_7_10.py`. It arrives on the last Drop of its Track and stays until it is dead; the music holds its Drop for the whole fight.

What it teaches:

1. You have fought all of this before. Each arm's solo is one earlier fight in two bars.
2. Shoot the soloist off and its tune is gone for good.
3. The last pattern is the first song.

---

## Parts

| Part | What it is |
|---|---|
| **The wheel** | Kaleidos as built, a quarter larger: a core of radius 80 and six arms 300 wu long. Each arm takes the accent colour of the Track whose boss it plays: Ultraviolet, Solar, Reef, Aurora, Furnace, Zenith |
| **The soloist** | The arm whose turn it is: at 3.0 with its white core line, a pool of its colour under it, the other arms at 1.0 |

## Phases

| Phase | Health | What happens |
|---|---|---|
| **Reprise** | 100 to 70% | Kaleidos's own fight at 174: mirrored fans of five from each pair of arms, now on the two-step's hits (kick on steps 0 and 10, snare on 4 and 12) instead of the eighths<br>The arms cannot be hurt yet (they are white-edged); the core is open, x 1, and x 2 on the One |
| **Medley** | 70 to 20% | The core closes. The arms take turns round the wheel, two bars each; the soloist is the only thing that can be hurt, and a dead arm's solo is skipped from then on<br>Kaleidos's arm: its mirrored fans on the eighths<br>Longwave's arm: it lets go as a wave of eight segments that crosses the frame once; only the crests can be hurt<br>The Choir's arm: a volley on the One, answered on three by the arm opposite<br>Event Horizon's arm: for its two bars ships and bullets are drawn toward the wheel at 200 wu/s, and a ring of twenty-four leaves the core on its second bar line<br>Polyrhythm's arm: two rings a bar with a gap each, one on three pulses and one on four; the gaps meet on the One<br>Feedback's arm: an Echo of the nearest player replays that player's last bar from the far side of the wheel |
| **Last Light** | 20 to 0% | The bare core. It fires First Light's arpeggio: one bullet on every sixteenth, its direction 15 degrees round for every semitone the note stands above the bar's lowest, the whole figure turning 45 degrees a bar, 260 wu/s<br>x 1, and x 2 on the One. Over this phase the palette cross-fades from Last Light to Ultraviolet |

## Numbers

| | |
|---|---|
| Health | 9,000 to start from. The number given is a first guess: tune it with the careful bot until the fight lasts what the page says with the build a player has by then, as was done for Longwave and The Choir; x (1 + 0.5 per extra player) |
| coreRadius | 80.0 |
| armLength | 300.0 |
| turn | one revolution every eight bars; it keeps 450 to 650 wu from the nearest player as Kaleidos does |
| armShare | in the Medley each arm has a sixth of that phase's health |
| Fight length to tune to | 110 to 140 seconds |
| Reward | A bonus Trio and 500 Shards for each player; the Track ends |
| Weak point | The core on the One in the first and last phases; the soloist in the Medley |
| Crowd | Half of the Drop's crowd, fodder only, unless the page says otherwise |
| Camera | Holds 0.85 for the fight (GAME_DESIGN 9) |
| Caps | One wheel, eight wave segments, one Echo, 200 bullets |
| On the grid | Everything it does lands on the song's grid and is computed from the song's clock, so every device in a co-op run sees the same fight with nothing new on the wire |

## Death

No burst: the core dims to a point, one white ripple crosses the whole lattice, and the ENCORE card comes up over First Light. The largest reward of the run.

## Sound

- Each solo is in its boss's own voice: Kaleidos's bells, Longwave's rubber string, a pad voice for The Choir, the sub drop for Event Horizon, toms three against four for Polyrhythm, the player's lead reversed for Feedback.
- A dead arm takes its voice out of the medley.
- The last pattern is First Light's arpeggio on the P3 bell, doubled by the band's lead: the tune of the first minute, at 174.
- Death: the tonic held, the ripple a reversed cymbal ending on the encore's first downbeat.

In the words of `design/music/sound_m5.json`'s recipes; a sound they cannot express is reported, not improvised.
