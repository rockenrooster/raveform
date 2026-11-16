# SONGLINE — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md` (timed runs, the knock-out, versus, the kit).
> Numbers: `modes.json` `songline`, `forms.timed48`. Screens: `screens.md`. Sound: `sound.md`.

The tune is a thing in the world. A spark of light flies through the arena playing the Track's melody,
and every note it plays hangs behind it as a ring: a gate. Fly with the spark and through its gates.
Each gate you thread plays its note again in your own voice and feeds your Chain; thread them without
a miss and your gun grows. The crowd comes at you along the line. 48 bars.

What it is for:

1. Speed and flow: the one mode that is about moving fast through the big arena with the camera
   chasing, where the others stand in a room or a fight.
2. The melody made visible. In the set the player's gun plays the lead and nobody sees it. Here the
   lead is a comet, its notes are rings in the air, and the line they make waves up and down with
   the tune.
3. One thumb: steer. The gun fires by itself, as always.

Home Track: Aurora (a tune on the even sixteenths under a held high note; a night sky). Any of the
twelve can be chosen; a faster Track sets the gates closer together.

Picture: `design/previews/modes_songline.png` (a Groove on Aurora: the ship a bar behind the Spark with
three rings of streak, the string of gates ahead of it riding one side of the line as that bar's tune
does, the dotted guide, the crowd coming down the line), drawn by `design/tools/modes.py`. It is a
layout reference, not a target frame: the engine's own pictures are judged against look.md.

---

## The pieces

| Piece | What it is |
|---|---|
| **The Spark** | The lead, flying. A four-point star of white light with a tail in the palette's accent, 0.8 of the ship's speed in a Groove (416 wu/s), so a ship can always keep up, fall behind to fight and come back |
| **The line** | The Spark's path: a smooth wandering curve through the arena, computed from the run's seed and the song (never from the players, but for one case: rule 4) |
| **A gate** | A ring of 34 wu left where the Spark played a note. It lasts two bars. Its place across the line follows its pitch: high notes to one side, low to the other, so the string of gates waves with the melody |
| **Threading** | Flying through a gate: the ship's centre within the gate's ring |
| **The streak** | Gates threaded without a miss |

## The run

The 48-bar form: Groove 8, Build 4, Drop 8, Break 4, Groove 8, Build 4, Drop 12 (`README.md` 6 ends it).

| Section | The Spark | Gates | The line | The crowd |
|---|---|---|---|---|
| **Groove** | 0.80 of the ship's speed | One every eighth: every second note of the bar's arpeggio, from its first (the beat's half of the grid, where a Track's tune is) | Wanders in wide curves | The Track's, at 0.7; formations land ahead on the line |
| **Build** | 0.90, and it brightens | Every eighth, the wave across the line shrinking to nothing by the last bar | Straightens: a runway | Thickens to 1.3, closing on the line from both sides |
| The Build's last bar | Lays nothing; it draws a halo and holds its note | — | Straight | — |
| **Drop** | 0.95 | **Every sixteenth**: every note of the arpeggio; the gates stand closer than they are wide, so the string reads as a tube of light | Great loops | A flood at 1.6 across the line. Surge doubles the score |
| **Break** | 0.58 | One every beat, large (60 wu) and worth three | Slow curves | None |
| The last two bars | Spirals inward | 32 gates on a closing spiral | — | — |
| **The last downbeat** | Bursts: a firework of eight rings in the palette's hues and three hundred sparks | — | — | Everything pops; the gates are counted (`screens.md`, "The count") |

## Rules

### The Spark and the line

1. **The Spark keeps the music's time, not the player's.** It never waits, slows for a fight or
   hurries. Its speed is a share of a ship's own (520 wu/s) by section, so the notes it lays stand a
   set distance apart: in a Groove at 110 BPM a gate every 113 wu, at 155 every 80; in a Drop every 67
   and 48.
2. **The line** is the Spark's heading turned a little at every step by two slow waves (`modes.json`
   `songline.path.curve`: their sum never turns it tighter than a circle of 320 wu), scaled by the
   section (a Build's is nearly straight), with the walls steering it back: when the line 600 wu ahead
   would come within 700 wu of a wall, it turns toward the arena's centre.
3. **A gate's place across the line** is its note's height above the middle of its bar's arpeggio:
   7 wu a semitone, at most 56 wu to either side. So a rising arpeggio is a string of gates climbing
   across the line, and the same bar of the song is the same shape every time it comes round.
4. **The tune finds you.** When every ship is more than 1,400 wu from the Spark, the line bends back
   toward the nearest ship and the Spark flies at 0.8 of its section's speed until one is within 600 wu.
   Nobody is left behind by a song. (This is the one case in which the line follows the players; the
   host decides it.)
5. **It is known ahead.** The line is computed a bar ahead of the Spark, and drawn there as a faint
   dotted guide, so a player can see where the tune is going.

### Gates and the streak

6. **Threading a gate** pays 10 times the Chain, adds one to the Chain (as a Shard does) and one to
   the streak, and plays the gate's note in the player's own lead voice.
7. **In time.** A gate threaded while the Spark is within 80 wu of the ship, which is to say as it is
   laid, pays double, and the player's voice plays a third above the Spark's note instead of the same
   note: flying beside the Spark is singing with it. This is the mode's Perfect Dash: a bonus for
   rhythm, never a need (R7).
8. **A miss** is a gate that fades unthreaded. It takes the streak down to the start of the tier
   below (the table), at most once a bar: one bad bar costs one step.
9. **The streak powers the gun**, on top of the kit, while it holds:

   | Streak | The ship gains | Seen |
   |---|---|---|
   | 16 | Its Shard pull doubled, and score x 1.5 | One thin ring round the halo in the mode's colours |
   | 32 | The fire grid one step faster (a Surge's first gear) | Two rings |
   | 64 | Prism: the five-way rainbow fan | Three rings; the fan |
   | 128 | The grid two steps faster, and Chorus: two ghost ships | Four rings; the ghosts |

   Each is something the game already has and already shows and sounds (GAME_DESIGN 4.1, 6.4).
10. **A gold gate** every eight bars, on a bar line: 60 wu, in the gifts' gold, carrying a Boost's
    icon. Threading it takes the Boost and pays 100 times the Chain.

### The ship, the gun and the crowd

11. **The kit.** The run starts with the build a run of the set has at that Track's first bar
    (`kit.atTrackStart`, `README.md` 5), so the gun is a match for the Track's crowd without a streak.
    On a bonus Track: the kit of the Track whose crowd it borrows.
12. **The gun fires as in the set**: the assist, or a thumb on the aim stick. Its shots make no notes
    here: the lead is the Spark's and the gates'. A shot is a soft tick (`sound.md`).
13. **No Trios.** A Shard level gives a Nova and one cell back. (A Trio is held by standing still, and
    this mode is about not standing still.)
14. **The crowd** is the Track's roster with every variant, at 0.7 of its crowd target and threat
    budget, by section as in the set. Bullets by Track as in the set. **It comes along the line**: the
    director lands its formations 600 to 1,200 wu ahead of the Spark, as rings round coming gates,
    walls across the line and escorts beside it, with the fairness rules of GAME_DESIGN 8 (never
    within 260 wu of a ship; every spawn warps in for two beats).
15. **Nothing ends the run.** With no cell left: Auto Nova, then the knock-out (`README.md` 7). Here it
    costs **the streak** (to nothing), and the ship comes back beside the Spark, not where it fell.

## Numbers

| | |
|---|---|
| Length | 48 bars: 1:45 at 110 BPM, 1:32 on Aurora, 1:14 at 155 |
| Spark | 0.80 / 0.90 / 0.95 / 0.58 of 520 wu/s in Groove / Build / Drop / Break |
| Gates | 34 wu; a Break's 60; live 2 bars and fade over a beat; at most 256 alive |
| Gates in a run | 512 a player on the 48-bar form (64 + 24 + 128 + 16 + 64 + 24 + 192: a Build's last bar lays none, and the last Drop's 192 hold the spiral's 32), the same at every tempo |
| The wave | 7 wu a semitone, at most 56 wu |
| In time | Within 80 wu of the Spark |
| Streak tiers | 16, 32, 64, 128 |
| Gold gate | Every 8 bars |
| Crowd | 0.7 of the Track's target and budget; formations 600 to 1,200 wu ahead |
| Record | `songline_<track>`: best score, gates threaded in a hundred |

## Look

| Thing | Drawn |
|---|---|
| **The Spark** | A four-point star, arms 60 and 36 wu, white at 3.5 with a pool of the accent (radius 120 wu, 0.2); it swells a fifth on each note it plays. Its tail: the line it flew over the last beat, 2.4 px, white running to the accent and out. On the priority layer with the dark rim, like the ship: it must be findable in a Drop |
| **The line ahead** | A dotted guide for one bar ahead of the Spark: dots 1.5 px at 0.35 in the accent, 24 wu apart, brightening toward the Spark. In the scene's light only: it is a hint |
| **A gate** | A ring glyph (one instanced glyph, never capsules: there are hundreds), 34 wu, 1.6 px, in the accent turned by its pitch (the lead-note rule of GAME_DESIGN 11.1: hue shifts with pitch, here 4 degrees a semitone round the accent) at 1.8, with a faint inner ring at 0.6. Born: it snaps open from a point in a sixteenth as its note sounds. Aging: steady, then its last beat fading |
| **A gate threaded** | It bursts into eight sparks along the ship's heading in the player's colour, its ring leaving as a small shock ring (to 70 wu in 0.15 s), a 9 px sparkle at the ship. In time: the ring is white and twice as bright, and a thin line of light joins the ship and the Spark for that sixteenth |
| **A gate missed** | It fades grey over its last beat; nothing flashes. A miss is quiet |
| **The string in a Drop** | Gates 48 to 67 wu apart, each 68 wu wide: they overlap into a tube. Their light is under the crowd's gain (look.md 3: they are many and each needs only its place), the Spark's and the ship's never |
| **The streak's rings** | One to four thin rings round the ship outside the halo (1 px at 1.2, the mode's two colours running round them), a new one snapping on at each tier with a ring leaving the ship; on a miss the outermost shatters outward |
| **A gold gate** | 60 wu, in P4's gold, the Boost's icon inside it at half size, a four-arm gold flare swelling on the beat, a 120 px vertical flare so it is seen from afar (the Cell's device, look.md 6) |
| **The Break's gates** | 60 wu, white, slow; each bursts into a ring that ripples the lattice |
| **The Spark off screen** | An edge chevron: the boss's double chevron (64 px, swelling on the beat) in the accent |
| **The finale** | The spiral of 32 gates closes to a point; the Spark flies it; on the last downbeat it bursts as a firework: eight rings in the palette's hues one after another a thirty-second apart, to 900 wu, three hundred sparks, a clamped white flash. Reduce flashing: no flash; the rings stay |

The camera (the follow camera, as built) leans toward the Spark by 30% of the offset to it, at most
320 wu, and holds 0.85 while the Spark is within 1,800 wu: the boss fight's camera, with the Spark as
its boss.

## Sound

In the words of `sound.md`; the short of it:

- **The Spark is the band's lead**, playing all the time, in the bell voice (P3's), on the grid its
  section gives (eighths, sixteenths, quarters). It is what the set's player hears from their own gun,
  given to the Spark.
- **A gate threaded**: that gate's note in the player's own lead voice (a new call on the band,
  `sound.md` "What the band needs"), placed on the next thirty-second. In time: a third above.
- **The streak's tiers**: at 16, 32, 64 and 128 the "upgrade taken" bells, one more of the four each
  time; a miss's step down is a soft glass tick an octave down.
- **The gun**: no lead notes; each volley a soft closed hat at 0.3 of the hat's level (so the fire is
  felt, not heard as a tune).
- **A gold gate**: the Boost bells, as any Boost.
- **The Build's last bar**: the Spark holds the bar's top note as a pad voice swelling into the Drop.
- **The finale**: a tom roll over the spiral's two bars rising an octave, the Drop's impact with the
  gong on the burst, then the count: a chirp for every 16 gates threaded, climbing.

## HUD

The set's, with:

- the goal line `GATES 148   STREAK 32` in the mode's colours; the streak's number swells at each tier;
- the run bar (`screens.md`);
- the Riff slots and Boost cards as in the set (the kit has Riffs).

Hints on a first run (`screens.md`): `FOLLOW THE SPARK`, then at the first gate in view `FLY THROUGH
THE RINGS`, then at the first tier `KEEP THE STREAK: MORE POWER`.

## Co-op

Two to four players. **The Spark lays a string for each player**, side by side across the line, in
that player's colour: two players 50 wu either side of the line, three at -100, 0 and +100, four at
-150, -50, +50 and +150, each waving with the tune as one string does. Every four bars the strings
change places across the line over one bar, so the players weave round each other: a braid of light.

- A player threads only their own gates. Another player's are drawn at a third.
- **The harmony.** Each string is a voice: the first seat's the tune, the second's a third above it,
  the third's a fifth, the fourth's the octave (two, four and seven scale steps up, in the Track's
  scale). Four players threading in step are a chord.
- **A chord.** When every player present threads their gate of the same step, that step bursts white
  on every string, each player's Chain gains one more, and the band's stab plays the bar's chord.
- Streaks, Chains, kits and Boosts are each player's own; a gold gate can be taken once by each player.
- The Spark waits for no one, and comes back only when every ship has lost it (rule 4).
- The score is the team's; the record `team_songline_<track>`.

## Versus: the race

`README.md` 8, V1 to V9. One string of white gates for everyone.

| | |
|---|---|
| Counts | **Gates taken.** A gate in time counts two |
| First come | The first ship through a gate has it: the gate turns that player's colour and plays in their voice, and is gone for the others. Two ships through in the same tick: the one nearer its centre |
| Reading the tune | The dotted guide shows the line a bar ahead for all, but not where across it the next gate will stand: that is its pitch (rule 3). Whoever knows how the tune goes is already on the right side. A melody heard twice is a race line learned |
| The streak | Each player's own, by their own gates and misses: a gate a rival took is no miss |
| The shove | V3: a Dash's ring pushes rivals off the line |
| Gold gates | First come (V4) |
| The underdog | Their ship threads gates from half as far again (51 wu), drawn as a wider thin ring round the ship |
| The knock-out | Costs the streak; the ship comes back beside the Spark |
| Ends | The count: each colour's gates tick up side by side. Then the podium |

## The result

`screens.md`, "Results". Headline: the score. Four numbers: `GATES` (threaded of laid, and in a hundred:
`446 OF 512   87%`), `IN TIME` (in a hundred), `BEST STREAK`, `BEST CHAIN`. The bar: the run's seven
sections in the Track's accent; over it the string as it was flown, one mark a gate, lit where it was
threaded, white where in time, dark where missed: the run as a line of light with its gaps. A cross
for each hit. `YOUR BAND` as built (the kit). Controls: **RETRY**, HOME.

## Caps and the wire

| State (in the world and its hash) | Size |
|---|---|
| The Spark | Its place, heading, distance flown, the next step |
| The line ahead | One bar of steps: 16 places |
| Gates | 256: place, note, step, who has threaded it (a bit a player), its owner in versus |
| Streak | A count a player, the bar of its last step down |

| On the wire | Who decides | How |
|---|---|---|
| The Spark, the line, the gates' places | Nobody: the seed and the song's clock | Nothing. Every device computes them step by step in the same order. The host puts the Spark's place, heading and step in the director's message once a bar, and a client more than 2 wu off takes the host's |
| The line bending back (rule 4) | The host | A reliable event: from which step, toward where |
| A gate threaded | The player's own device | That player's event (the gate's step); co-op: nothing to decide. Versus: the host gives the gate to the first claim by its tick, tells all, and a device that had guessed otherwise corrects the gate's colour |
| A chord | The host | With the gate events it already has |

The densest moment: the last Drop with four players: 4 strings of gates (at most 256 gates, one
instanced draw), the flood, four kits firing. Its benchmark is in `docs/MODES_PLAN.md`.

## What to check

- The line, as a unit test: for each Track and ten seeds, 48 bars of it stay 400 wu inside the walls,
  never turn tighter than 250 wu, and two worlds stepped separately agree on every gate to 0.01 wu.
- The gates of one bar of each Track's progression are the same shape each time that bar comes round.
- A bot that flies at the Spark's side threads at least 90% of the gates on every Track; a bot at
  half the ship's speed is found again by the line (rule 4) within four bars every time.
- The child-like bot threads at least half the gates on Aurora and is knocked out no more than twice.
- Pictures: a Groove's string with the guide, a Build's runway, a Drop's tube as a strip a sixteenth
  apart, a gate threaded in time, four players' braid, the finale as a strip, the count.
