# CADENCE — boss of Track 7, Afterglow

The boss is the chord progression. Four great bodies stand round the players, one for each chord; the one whose chord the band is playing is silent and open, and the other three fire.

156 BPM, D flat major: D flat, A flat, B flat minor, G flat (I, V, vi, IV). Data: `design/bosses/cadence.json` (load it, do not retype it). Picture: `design/previews/bosses_7_10.png`.
Tool: `design/tools/bosses_7_10.py`. It on the last Drop of its Track and stays until it is dead; the music holds its Drop for the whole fight.

What it teaches:

1. The harmony tells you where to aim. The open body changes on every bar line, in the order of the song: I, V, vi, IV, round the square.
2. Half time is room. The three that are not sounding fire only on one and three, slowly; the way toward the open body is always clear of its own fire.
3. In the last phase the rule turns over, and the music says so.

---

## Parts

| Part | What it is |
|---|---|
| **Pillar (x 4)** | A ring of twelve petals, radius 90, with its numeral inside in the stroke font (cap 60 wu): I, V, vi, IV. Each has its own hue from the palette: I the peach-gold, V the lavender, vi the periwinkle, IV the mint. Sounding: its petals spread to radius 120, its numeral is white at 2.6 and a pool of its hue lies under it. Silent: petals closed, numeral at 0.8 |
| **The square** | The four stand on the corners of a square 700 wu on a side, in the song's order clockwise from the top left (I, V, vi, IV); a thin line in the grid's colour joins them, and the side leading to the next chord brightens over the bar |

## Phases

| Phase | Health | What happens |
|---|---|---|
| **Progression** | 100 to 66% | The pillar of the bar's chord is open and silent; it is the only one that can be hurt. On the One of its bar it takes x 2 for the beat<br>The other three each fire a fan of five at the nearest player on beats 1 and 3 (the half-time kick and snare), 180 wu/s<br>So the open pillar goes round the square one corner a bar, clockwise |
| **Modulation** | 66 to 33% | On every fourth bar line, as the song comes back to I, the square turns a quarter turn the other way in one beat, so the next I is where IV stood: the numerals, not the corners, say who is next<br>Each firing pillar adds a second fan of seven on the snare |
| **Inversion** | 33 to 0% | The rule turns over, announced by the word INVERSION and a bar of the bed high-passed: now the sounding pillar is closed and it alone fires, a rosette of sixteen on the One and twelve on three; the other three are open and silent<br>Three targets at once: the fight ends quickly from here, as a last chorus should |

## Numbers

| | |
|---|---|
| Health | 5,200 to start from. The number given is a first guess: tune it with the careful bot until the fight lasts what the page says with the build a player has by then, as was done for Longwave and The Choir; x (1 + 0.5 per extra player) |
| pillarRadius | 90.0 |
| squareSide | 700.0 |
| follow | the square's centre follows the players' centre at 40 wu/s |
| fanBullets | 5 |
| fanSpreadDegrees | 40.0 |
| bulletSpeed | 180.0 |
| bulletLifeSeconds | 4.0 |
| Fight length to tune to | 80 to 100 seconds |
| Reward | A bonus Trio and 300 Shards for each player; the Track ends |
| Weak point | The open pillar, and x 2 on the One of its bar |
| Crowd | Half of the Drop's crowd, fodder only, unless the page says otherwise |
| Camera | Holds 0.85 for the fight (GAME_DESIGN 9) |
| Caps | Four bodies, 120 bullets |
| On the grid | Everything it does lands on the song's grid and is computed from the song's clock, so every device in a co-op run sees the same fight with nothing new on the wire |

## Death

The progression resolves: the four burst in the song's order on the four beats of the last bar and the band holds the tonic chord over a gong.

## Sound

- Each pillar is its chord: while it sounds, a pad voice holds its root and the band's stab plays that chord on the One at 1.2.
- Each firing pillar's fan is a tom on its own chord's root, on one and three: three toms, a slow melody of the chords that are not playing.
- The turn of the square is a reversed cymbal into the bar line; Inversion is the music bus high-passed at 800 Hz for one bar.
- Death: the four chords in turn, then the tonic with the gong.

In the words of `design/music/sound_m5.json`'s recipes; a sound they cannot express is reported, not improvised.
