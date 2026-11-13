# FEEDBACK — boss of Track 6

> **M5, 2026-11-13.** Armour takes a quarter: the shut Head takes 0.25 of a shot's damage, where this
> page says none (`damageScale` in the data; GAME_DESIGN 7.6, "A boss's rule is a bonus, not a wall").
> Hint line (screens.md): `STOP FIRING TO OPEN IT`. The health is tuned with the novice bot.

A loudspeaker inside a loop of tape. It has no attack of its own. It records each player's bar and,
one bar later, an **Echo** of that player plays it back from the far side: the same ship, the same
shots, the same Riffs, aimed the other way. It arrives on the last Drop of Track 6 "Zenith" (150 BPM,
C minor) and is the last boss before the run loops; the music holds its Drop section for the whole
fight.

Kaleidos: hit the quiet part. Longwave: move with the music. The Choir: listen ahead. Event Horizon:
the bar is the unit. Polyrhythm: hold two counts. Feedback is the one the player writes:

1. **What you play comes back.** Every shot you fire is fired at you a bar later. A stronger build
   is a harder echo, and nobody else chose it.
2. **Silence is part of the song.** The Head can be hurt only while your Echo is silent, and your
   Echo is silent only where you were silent a bar ago.
3. **Play, then listen.** Fire a phrase; leave that phrase empty in the next bar and spend it getting
   out of the way of your own shots.

Data: `design/bosses/feedback.json` (parts, numbers, rules — load it, do not retype it).
Picture: `design/previews/boss_feedback.png`. Tool: `design/tools/boss_feedback.py`.

Track 6's mini-boss, Mirror Mimic, is the rehearsal: it copies a player's movement and fires when
they fire, with no delay.

---

## Anatomy (world units)

| Part | Shape | Size |
|---|---|---|
| **Head** | A loudspeaker seen from the front: surround, ribbed cone, and a six-blade shutter over a gold core | Radius 84. 4,800 health. It does not move |
| **Loop** | A ring of tape round the Head: two thin white lines with sixteen faint slots, one turn a bar, step 0 at twelve o'clock, clockwise. A gold head runs round it on the song's sixteenths | Radius 420, band 24 wide |
| **Echo** | The player's own ship in the danger colour with a white core and three after-images | Body radius 16. One for each player (two from the second phase) |
| **Echo shots** | The player's shots, turned: short streaks in the danger colour with the threat rim | 400 wu/s, 3.2 s, radius 5 |

## Rules

1. **The tape.** As the gold head passes a step it reads what is there (the Echo plays it) and then
   writes what the player did in that step. Each player has a lane of the tape in their colour and a
   shot is a bright tick. So the tape is always the next bar of echo, drawn before it happens.
2. **The Echo stands at the mirror point**: the player's position of one bar ago reflected through
   the Head's centre, with the aim turned half a turn. Circle clockwise and it circles clockwise
   opposite you. A faint dashed line in the player's colour runs from the Echo through the Head to a
   small ring where the player stood a bar ago. Its distance from the Head is held between 220 and
   900 wu.
3. **It fires what you fired**, on the same sixteenth one bar later: one echo shot for each of your
   shots, same lane, same Mods. Echo shots pass through the Head and through enemies and take one
   Shield cell from any player they touch. A Nova erases them like bullets.
4. **It carries your build.** Riffs run on the Echo one bar behind, at the recorded levels: what you
   aimed is reflected (Lancer, Comet, Mirror), what aimed itself aims at the nearest player
   (Sentinel, Swarm, Storm Coil), what surrounds the ship surrounds the Echo (Orbitals, Pulse Ring,
   Sweeper, Flak). Boosts that change shots or Riffs are echoed (Prism, Hyper, Supernova Rounds,
   Chorus, Beam, Ricochet Fever, Fractal); the others are the player's alone. A Nova comes back as a
   **Null**: a danger ring of the same radius from the Echo one bar later. A Dash comes back as
   movement only.
5. **The Head is open only while every Echo is silent.** An Echo counts as playing from each echoed
   primary shot until two sixteenths after it; Riffs do not count. Open: the shutter is drawn back,
   the core is gold, every hit counts. Closed: the shutter is across and shots are swallowed with a
   dull thud. Where the tape has ticks, the Head will be shut.
6. The Echo's body takes a cell on touch. It cannot be hurt, and player shots pass through it.
7. At most 500 echo shots are alive. A bar holding more than 120 of a player's shots is played back
   with every second shot left out.

Echoes are a function of the players' own recorded play, not of song time alone. Each device works
them out from its own view of every player's history (the local player's is exact), and a hit is
decided by the device that owns the player, as always.

## Phases

| Phase | When | Change |
|---|---|---|
| 1 **Slapback** | Above 60% | One Echo for each player, one bar behind. The widest safe rhythm is half and half — two beats on, two beats off, swapped in the next bar — or a bar on and a bar off |
| 2 **Regeneration** | 60% to 30% | A second Echo, two bars behind and reflected twice: it stands exactly where the player stood two bars ago and fires as they fired. Nowhere you have just been is safe to return to, and a phrase now shuts the Head twice: one bar in three is open |
| 3 **Howl** | 30% and below | The tape runs double speed: the Echoes are two beats and four beats behind, so the figure is two beats on, four beats off — three against the bar. The Loop burns and tightens by 15 wu on every bar line down to 300; touching it takes a cell, so a player is inside with the Head or outside with the long flight times, and crosses with a Dash |

## Numbers

| | Value |
|---|---|
| Health | 4,800 (× 1 + 0.5 per extra player) |
| Damage scale | Open × 1 · closed 0. In co-op: × the share of living players' Echoes that are silent |
| Contact | The Head and every Echo take a cell; the Loop only in Howl |
| Camera | 0.75 for the fight; the whole Loop is in the frame when the ship is inside it |
| Crowd | Half, fodder only; echo shots pass through enemies |
| Reward | A bonus Trio and 320 Shards per player; Track 6 ends and the endless loop begins |

With the build a player has by the end of Track 6 (about 110 damage a second on target) and the
Head open half the time in the first phase and a third of the time after it, expect about two
minutes. Tune the health with the bot once it can fire in phrases.

## Sound

- **The echo is the boss's music layer.** Every echo shot plays its player's own lead note as it
  was recorded: an octave down, low-passed at 1.2 kHz, 9 dB quieter and panned to the far side. The
  second Echo is 15 dB down and darker. It is a tape delay on the player's own playing.
- **Open:** the pad's filter opens and the Head holds the Track's root two octaves up, quietly.
  **Closed:** the pad is ducked and dull. A swallowed shot is a closed hat.
- The held root rises a semitone for every 10% of health lost. In Howl the delay's own feedback is
  turned up, so the echoes smear into each other. The tone is band-limited and sits under the
  limiter: it is never a squeal.
- A player who rests hears the bar they just played come back alone. The rest is audible.

## Look

- The Head is the palette's electric blue with a gold core; it pushes out 15% on every kick. The
  Loop is white; ticks are the players' colours; the head on the tape is gold with a small flare.
- Echoes and everything they throw are the danger colour on the priority layer, like every threat.
  An Echo's shape is its player's ship, so each player knows which one is theirs.
- In Howl the Loop's lines turn to the danger colour and the lattice inside it is pulled 20 px
  toward the Head on every bar line.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat the music cuts for one beat. The last bar the player fired
  comes back out of the dark, alone, from the far side: the first echo. The Loop draws itself once
  round over the next bar and the Head assembles inside it. The name card holds for two bars;
  recording starts on bar 3 and the first Echo plays on bar 4.
- **Death:** the tape is cut. The Loop opens at the head and unspools outward, its ticks flying off
  as sparks in the players' colours. Every Echo stops, turns from the danger colour to its player's
  own colour and flies home to its player as 100 Shards. The Head holds the Track's root for two
  bars while the screen blooms to white, then the boss break on the One — three rings, 300 sparks,
  the 30 px distortion ring, 50 ms of hit-stop — and the rest of the Shards.

## Co-op

Every player has their own Echoes and their own lane of the tape. One player who never rests keeps
their share of the Head shut for everyone, so the band has to breathe together: call, then
response. Echo shots hurt any player, and an Echo stands where its player's partner may want to be.
A downed player's Echoes fade one bar later.
