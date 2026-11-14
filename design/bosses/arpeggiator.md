# ARPEGGIATOR — boss of Track 8, Arcadia

The bullets are the tune. Four lanes run from a machine across the frame and the lead's broken chord comes down them as blocks, so the player weaves the shape of the arpeggio they have been firing all Track.

162 BPM, G major: G, E minor, C, D. Data: `design/bosses/arpeggiator.json` (load it, do not retype it). Picture: `design/previews/bosses_7_10.png`.
Tool: `design/tools/bosses_7_10.py`. It arrives on the last Drop of its Track and stays until it is dead; the music holds its Drop for the whole fight.

What it teaches:

1. The pattern is the melody you already know: it climbs and falls with the chord, a lane at a time.
2. The next bar is written above the lanes before it is played.
3. It is open while it loads a bar: on the One.

---

## Parts

| Part | What it is |
|---|---|
| **The Machine** | A console 900 wu wide and 120 deep with a core of radius 70 at its middle behind a shutter, four ports along its lower edge (one for each lane) and above it the chart: a matrix of 4 by 16 dots, gold, 400 by 60 wu, the lit dots the next bar's notes and a white cursor crossing it in time |
| **Lanes** | Four lanes 200 wu wide and 900 long running from the ports. Drawn as five faint lines in the grid's colour with the lane's number at the far end |
| **Blocks** | A note: a danger-coloured capsule 36 wu wide and 12 deep on the priority layer with a white-pink line down its middle, drawn as a lethal beam is (look.md section 5), falling along its lane's middle at 340 wu/s. Its glow, which hurts nobody, makes up the rest of its size: about the 72 by 28 the block had before (the owner, 2026-11-14: "they need to be much smaller with the glow making up the rest of the size"; 72 wu wide and 28 deep until then, and 90 wide at 420 wu/s before that, when the assist bot lost its only two runs of twenty to them). It cannot be shot; a Nova clears the lanes |

## Phases

| Phase | Health | What happens |
|---|---|---|
| **Quarter notes** | 100 to 66% | The machine stands above the players and plays the accented notes of the lead: steps 0, 4, 8 and 12 of each bar, one block each, in the lane of its note<br>Its shutter opens for the first beat of every bar as it loads the next chart: x 2 on the core for that beat; x 1 on the core at other times; the console sheds shots |
| **Eighth notes** | 66 to 33% | Every second step: eight blocks a bar<br>Every fourth bar it moves to the next side of the players (above, right, below, left); its new lanes are drawn dashed for the bar before it moves |
| **Two hands** | 33 to 0% | A second machine, white, plays from the side at right angles to the first, so the lanes cross in a grid of sixteen cells<br>The first plays the lead's quarter notes; the second plays the chord stab's rhythm (steps 0, 3, 6, 9, 12, 14), each hit in the lane of the bar's chord tone in turn<br>Either core can be hurt; they share the health |

## Numbers

| | |
|---|---|
| Health | 6,400 to start from. The number given is a first guess: tune it with the careful bot until the fight lasts what the page says with the build a player has by then, as was done for Longwave and The Choir; x (1 + 0.5 per extra player) |
| coreRadius | 70.0 |
| laneWidth | 200.0 |
| laneLength | 900.0 |
| blockWidth | 36.0 |
| blockDepth | 12.0 |
| blockSpeed | 340.0 |
| place | 380 wu from the players' centre on its side of them; it follows them along its own axis at 120 wu/s and its lanes move with it |
| laneOfNote | rank the distinct pitches of the bar's arpeggio from low to high; a note's lane is (its rank mod 4) + 1. For G that is 1 2 3 4 3 2 3 4 1 4 3 4 2 1 4 3 |
| Fight length to tune to | 90 to 110 seconds |
| Reward | A bonus Trio and 340 Shards for each player; the Track ends |
| Weak point | The core, x 2 for the first beat of each bar |
| Crowd | Half of the Drop's crowd, fodder only, unless the page says otherwise |
| Camera | Holds 0.85 for the fight (GAME_DESIGN 9) |
| Caps | Two machines, 48 blocks |
| On the grid | Everything it does lands on the song's grid and is computed from the song's clock, so every device in a co-op run sees the same fight with nothing new on the wire |

## Death

The chart reads GG, every block in flight turns gold and climbs back up its lane as Shards, and the machine plays the arpeggio's last run upward.

## Sound

- Each block sounds its own note, on the P3 bell at 0.3, as it passes the row the nearest player is in: with fire held, the blocks and the player's lead are the same tune in unison.
- The shutter is a woodblock on the One with a sweep opening over the beat.
- A change of side is a whoosh over the bar before; the second machine arrives with the chord's stab.
- Death: Shard chirps running up the scale over two beats.

In the words of `design/music/sound_m5.json`'s recipes; a sound they cannot express is reported, not improvised.
