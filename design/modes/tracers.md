# TRACERS — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md` (the Stage, timed runs, the knock-out, versus).
> Numbers: `modes.json` `tracers`, `stage`, `forms.timed48`. Screens: `screens.md`. Sound: `sound.md`.

The ship leaves a tracer: a line of light that lasts two bars. Fly a ring with it, and when the line
meets itself everything inside is caught, pops on the next beat, and the dance floor inside lights up in
your colour. Light the whole floor and the house comes down; then do it again. 48 bars.

The gun is off. There is nothing to aim: the left thumb does everything. The line itself is the lead:
it drops a note of the melody on every sixteenth it is drawn, so drawing is playing.

What it is for:

1. A game for one thumb and for a five-year-old that is still a game for the player who wants a score.
2. Sound becomes light, said the plainest way the game can say it: you move, a line of light and a
   tune come out, and where you close the line the floor lights.
3. The best co-op and versus the game has for one screen: lines that weld, lines that cut.

Home Track: Mirrorball (`README.md` 11). Any of the twelve can be chosen; a faster Track is a shorter
tracer (its life is in beats) and so a harder floor.

Picture: `design/previews/modes_tracers.png` (two players on the Stage at a phone's zoom in Mirrorball's
palette: 48% of the floor lit in their two colours, P1's tracer about to close round a pack with its
hint, P2's ring locked round thirteen bodies), drawn by `design/tools/modes.py`. It is a layout
reference, not a target frame: the engine's own pictures are judged against look.md.

---

## The pieces

| Piece | What it is |
|---|---|
| **The floor** | The Stage (`README.md` 4): 1,720 x 840 wu, all of it on screen, 903 tiles of 40 wu. A tile is dark or lit in one player's colour |
| **The tracer** | The line behind the ship. It is laid while the ship moves faster than 120 wu/s. Every point of it lives **8 beats**, then goes out from the tail. At full speed that is about 2,050 wu of line at 122 BPM (2,270 at 110, 1,610 at 155): enough for a ring 650 wu across |
| **A node** | Where a sixteenth fell while the line was being laid: a bead on the line, and one note of the Track's lead (rule R2's shot, without the shot) |
| **A ring** | The closed part of a tracer: from where the line met itself, round to the ship |
| **A house** | The floor lit to nine tenths. It comes down (below) and the next house begins |

## The run

The 48-bar form (`modes.json` `forms.timed48`): Groove 8, Build 4, Drop 8, Break 4, Groove 8, Build 4,
Drop 12. It ends at its peak (`README.md` 6).

| Section | The floor |
|---|---|
| Groove | The Track's crowd wanders in and hunts; rings of five and ten |
| Build | The crowd thickens to 1.3; the room tightens; the last bar is dim and slow, and the tracer burns brighter in it |
| Drop | A flood at 1.6: the rings of forty are here. Surge doubles the score |
| Break | Nothing spawns; what is left drifts. Four bars to ring empty floor: ground for nothing |
| The last downbeat | Everything pops, and **the floor is counted**: a line of light crosses the Stage left to right in two beats and every lit tile it passes pays and flashes, with a note a tile column climbing the scale (`screens.md`, "The count") |

## Rules

### The tracer

1. **Laid by moving.** While the ship is faster than 120 wu/s the tracer grows from it, a point every
   16 wu of travel (at most 160 points a player). Slower, nothing is laid and what is there keeps
   burning away from its tail. A Dash lays its line straight through.
2. **It is the lead.** While the tracer is being laid the band plays the player's lead as if fire were
   held, and each sixteenth heard drops a node on the line (the lead rule of TECH_SPEC 6.3 with the
   bullets left out: `LeadShot` steps become nodes). Stop, and the lead stops. Nothing else of the
   ship fires: no shots, and the assist rests.
3. **A hit breaks it.** A hit on the ship (or its knock-out) puts the whole tracer out. So does being
   spun out or cut in versus (below).
4. **Its length grows with the run.** Each Shard level adds a beat to its life, to 16 beats at most
   (that is the level's gift here: no Trio opens in this mode; the level still gives its Nova). The
   HUD says `TRACER 9 BEATS`.

### Closing a ring

5. **It closes when the line meets itself**: when the newest piece of the tracer crosses an older
   piece (not its newest six points), or the ship comes within 36 wu of one (the snap: nobody should
   miss a ring by a hair). From 120 wu away a thin dashed line already joins the ship to the nearest
   place it could close on, so the player sees the ring before it is one.
6. **The ring locks at once**, on the frame (R1). The line from the meeting place round to the ship
   becomes the ring; the tail behind the meeting place is cut loose and goes out; a new tracer starts
   from the ship. A ring smaller than four tiles fizzles: it locks nothing and plays one dry tick.
7. **What is inside is caught.** Every body whose centre is inside the ring as it locks is caught: it
   stops (as under Freeze Frame), turns to the ring's owner's colour and cannot hurt anyone. Bodies
   still warping in are not caught.
8. **It pops on the next eighth line** (R6's way: the lock is the inhale, the pop is on the grid; at
   most a quarter of a second). Every caught body dies. Elites, mini-bosses and arena bodies do not die
   to one ring: each loses a quarter of its full health, and is let go. Kills pay the set's score times
   the Chain times **the ring's bonus**:

   | Caught | 1-4 | 5-9 | 10-19 | 20-39 | 40 or more |
   |---|---|---|---|---|---|
   | Bonus | x 1 | x 2 | x 3 | x 5 | x 8 |

   Shards fall as always, for every player, and the Chain climbs with them.
9. **The floor inside lights.** Every tile whose centre is inside the ring becomes its owner's.

### The floor

10. **A lit tile stays lit** until its house comes down. Nothing the crowd does puts it out.
11. **FULL HOUSE.** When 813 of the 903 tiles (nine tenths) are lit, the house is full. On the next bar
    line: the tiles still dark light by themselves in a chase; then the whole crowd pops in a ring from
    the Stage's centre (no ring bonus; the set's score); the house pays **5,000 x its number x the
    Chain**; and over the bar after, the floor goes dark from the centre out. The next house begins
    with the crowd's target and the hunters' budget 15% higher than the last. The HUD says `HOUSE 2`.
12. **At the end the floor pays** 10 a lit tile, times the Chain.

### The crowd

13. **The Track's crowd, for a floor.** Its roster, at half its crowd target (never above 200 on the
    Stage) and half its threat budget, by section as in the set. **Contact only**: variant A of every
    family, and no Sentries, Tethers or Weavers: nothing in this mode fires or lays a lethal line. So
    no enemy bullet exists in TRACERS and the danger colour is seen only on a Ram's tell.
14. **Elites** come one every eight bars from the second Groove on, with the Track's traits except
    Volatile and Cloaked. An elite takes four rings, or a Nova.
15. **Boost orbs** fall one every six bars, from this mode's list: Time Stretch, Freeze Frame, Halo,
    Magnet Storm, Black Hole, Colossus, Confetti, and two of the mode's own, **gels**:

    | Gel | For 4 bars | Its orb |
    |---|---|---|
    | **LONG** | The tracer lasts twice as long | Encore's icon; words `TRACER TWICE AS LONG` |
    | **ECHO** | Every ring pops twice: again a beat later, half as large again about its centre, catching what has walked in | Echo's icon; words `EVERY RING POPS TWICE` |

    The others do what they do in the set, and each is good here for its own reason: a Black Hole
    gathers the room into one ring; Freeze Frame holds it still to be ringed; Time Stretch gives the
    line more time than the crowd.

### The ship

16. **Nova and Dash** as in the set. A Nova's kills light no floor. Cells, Auto Nova, the halo: as in
    the set. Four cells at the start.
17. **Nothing ends the run.** With no cell left: the knock-out (`README.md` 7). Here it also costs
    **the lights**: the oldest quarter of the lit tiles go dark.

## Numbers

| | |
|---|---|
| Length | 48 bars: 1:45 at 110 BPM, 1:34 on Mirrorball, 1:14 at 155 |
| Tracer | 8 beats; +1 a Shard level to 16; laid above 120 wu/s; a point every 16 wu; at most 160 points |
| Closing | Crossing, or within 36 wu; the hint from 120 wu; the newest six points do not count |
| A ring | At least 4 tiles; pops on the next eighth; at most 48 corners (the line thinned to that) for what it catches and lights; at most 8 alive at once |
| Floor | 903 tiles; full at 813; 5,000 x the house x the Chain; 10 a tile at the end |
| Crowd | 0.5 of the Track's target, at most 200; threat 0.5; +15% a house |
| Boost orbs | One every 6 bars |
| Record | `tracers_<track>`: best score, full houses |

A full ring at Mirrorball's tempo holds about 208 tiles, a quarter of the floor less a little: a house
is five to eight good rings. A careful player fills two or three houses in a run; the first is the
easy one.

## Look

Everything is line and light (look.md 1). Nothing is filled.

| Thing | Drawn |
|---|---|
| **The tracer** | One line, 2.2 px at 2.6, white at the ship running to the player's colour and then to `deep` at its tail (the player trail's field, look.md 2). Never dimmed by the crowd (the light budget's "never dimmed" list). Its last two beats of life are drawn dashed and flicker: the player sees how much line is left |
| **A node** | A 9 px sparkle on the line where its note fell, white at 0.7, flaring to 1.6 for 60 ms as it sounds |
| **The closing hint** | A dashed line 1 px at 0.5 in the player's `light` colour from the ship to the place it would close on, and a small ring there; within the snap's 36 wu the ring fills |
| **A lock** | The ring's line flashes white (3 px at 3.5 for 60 ms), then holds at 2.8 in the player's colour, drawn flat-joined (look.md 4, joints). Each caught body gets a white dot at its centre and its hull turns to the owner's colour at 0.8 of its light. The lattice inside goes to its warm level |
| **A pop** | The caught bodies die by the kill recipes of look.md 5, staggered from the ring's line inward over a thirty-second, so a big ring is a wave closing on its centre, not one flash. The ring's line leaves as a shock ring (1.3, 0.2 s). One pool of the owner's colour at the ring's centre, as wide as the ring, 0.30 falling to nothing in half a beat. The lattice inside is thrown outward (one impulse of 140 from the centre; 300 for a ring of 40). From x 5 up: two of the rig's fixtures flash a beam each onto the ring's centre for a beat (the Stage's beam, `jam.md`), and a shake of 3 px (6 for x 8) |
| **The ring's words** | `RING X5` in world text over the ring's centre (cap 22 wu, the player's colour, the priority layer's dark rim), one beat; from x 5 up only. `x 8`: cap 34, with a four-arm flare |
| **A lit tile** | No fill. The lattice's lines inside a lit tile are drawn in its owner's colour field at 0.55 (the lattice's rest is 0.30), swelling a quarter on the kick. A tile lights from the ring's line inward over a quarter beat |
| **A lit region's edge** | Where lit meets dark or another colour: a 1.5 px line in the owner's colour at 1.4, along the tiles' sides, with a soft glow (10 wu, 0.2, a pool). It is what makes the floor read as tiles: squares of light, stepped at their edges |
| **FULL HOUSE** | The dark tiles light in a spiral from the last ring's centre over two beats, each with a tick of white; every fixture of the rig throws a beam that sweeps the floor for those two beats; then the crowd's pop and `FULL HOUSE` at cap 110 in the mode's colours with `X 2` under it, a clamped white flash (look.md's Drop burst), a shower of confetti in the players' colours (the Drop's recipe), the lattice ringing from the centre. Then the tiles go out ring by ring from the centre over a bar. Reduce flashing: no flash, no beams' sweep; the chase and the words stay |
| **The lights go out** (a knock-out) | The oldest quarter of the lit tiles blink twice on the eighths and go dark, edge tiles first |
| **The count** (the end) | `screens.md`, "The count": the sweep line is the title's rule stood upright, 2.4 px in the mode's colours with its white core |

A teammate's or rival's tracer, rings and tiles are in their colour, at the levels look.md 6 "Co-op"
gives other players' things (their line at 0.7 of the tracer's level; their tiles as bright as yours:
the floor is everyone's picture).

## Sound

In the words of `sound.md`; the short of it:

- **The tracer is the player's lead**, on the Track's arpeggio, a note a sixteenth while it is laid.
- **A lock**: a glass tick two octaves up on the bar's chord root, at once.
- **A pop**: the kills' own sounds (aggregated, as always) under **the ring's chord**: the band's stab
  on the bar's chord, larger by the bonus: x 1 one note of it (the root), x 2 its triad, x 3 its four
  notes, x 5 those and the octave with a tom on the root, x 8 the same at 1.2 with the sub drop. On
  the eighth line it pops on.
- **A fizzle**: one woodblock.
- **A tile column lighting**: nothing (the pop is the sound); **FULL HOUSE**: a tom roll over the two
  beats of the chase into the Drop's impact, the gong, and the stab on the home chord.
- **The count**: a chirp a tile column, climbing the scale, into the "upgrade taken" chord.
- **Gels**: LONG, the lead's notes ring twice as long (its release doubled); ECHO, the ring's chord
  again a beat later an octave up at half level.

## HUD

The set's, with:

- the goal line `FLOOR 62%   HOUSE 2` in the mode's colours, the percentage counting up as tiles light;
- under it the run bar (`screens.md`);
- where the Shard level's bar stands, the same bar labelled `TRACER 9 BEATS`;
- no Riff slots (there are none): the Boost cards sit a row lower.

No touch control changes: the right half of the screen is simply not needed. NOVA and DASH stay where
they are. The first run's hints (`screens.md`): `MOVE TO DRAW A LINE`, then `CLOSE A RING ROUND THEM`,
then on the first lit tiles `LIGHT THE WHOLE FLOOR`.

## Co-op

Two to four players, one floor. Every player's rings light it in their own colour and all colours
count toward the house. The crowd's target and the hunters' budget scale by the players as in the set.

**Lines weld.** When two teammates' tracers cross in two places, the two pieces between the crossings
close a ring together: a **duet ring**.

- It locks when the second crossing happens, on the device of the player who made it, as that player
  sees both lines.
- It pays both players, each with their own Chain, at **twice** the ring's bonus.
- Its tiles are lit in a check of the two colours, tile by tile.
- Both lines go on from their ships; the pieces used are cut loose.
- Its chord is played by both players' leads, a third apart.

A house is the team's. The score is the team's; the record `team_tracers_<track>`. The knock-out's cost
(a quarter of the lit tiles, the oldest) is taken from the knocked-out player's own tiles.

## Versus: turf

`README.md` 8, V1 to V9. The floor is the scoreboard.

| | |
|---|---|
| Counts | **Tiles in your colour when the music stops.** Shown all match as a bar across the top in the players' colours (`screens.md`, "The versus HUD") and as the floor itself |
| Repainting | A ring lights everything inside it in its owner's colour, whoever had it. There are no houses in versus: the floor never comes down |
| **The cut** | Fly across a rival's tracer and it is cut there: the part from the cut back to its tail goes dark at once; the part still joined to their ship lives. Costs the cutter nothing |
| **Caught in a ring** | A rival ship inside your ring as it pops is **spun out**: pushed 300 wu out through the ring's nearest side (a push it rides), its tracer put out, and for two beats it lays no line (its hull blinks, as a ship that cannot be hurt does). No cell is lost |
| The shove | V3: a Dash's ring pushes rivals, and a pushed ship's tracer bends with it |
| Kills | Pay Shards, the Chain and Shard levels (a longer tracer) as always, and no tiles of their own |
| The underdog | Their tracer lasts 4 beats longer |
| The knock-out | A quarter of your tiles go dark (to nobody) |
| Ends | The count is the race's last word: the sweep line crosses, each colour's tiles tick up side by side, the bar across the top fills, the leader's part flashes. Then the podium |

Why it works for a child against a parent: ground changes hands by the screenful, the leader has the
most to lose, and the last Drop's twelve bars are where the floor is decided.

## The result

`screens.md`, "Results". Headline: the score. Four numbers: `FLOOR` (its share at the end), `FULL HOUSES`,
`BEST RING` (the most caught), `BEST CHAIN`. The bar: the run's seven sections in the Track's accent, a
gold star where a house came down, a white tick for each ring of x 5 and more, a cross for each hit. In
place of `YOUR BAND`: the floor as it stood at the end, drawn small (43 x 21 squares of 8 px in the
owners' colours). Controls: **RETRY**, HOME.

## Caps and the wire

| State (in the world and its hash) | Size |
|---|---|
| Tracer points | 160 a player, 640 |
| Rings alive | 8, each at most 48 corners |
| Tiles | 903 bytes: the owner, and the order it was lit in (for the knock-out's "oldest") as a 16-bit count beside it |
| Caught marks | A bit an enemy |

| On the wire | Who decides | How |
|---|---|---|
| A tracer | Its player | Not sent. Every device builds a teammate's line from that player's poses (30 Hz, TECH_SPEC 8.1), for drawing and for crossings |
| A ring | Its player's device (a duet: the device that made the second crossing) asks; **the host orders** | Reliable: the ring's corners (at most 48, 16 bits each way: under 200 bytes), its pop's tick, its owners. The host gives it a number and sends it to all; every device catches, pops and lights from the same corners in the same order, so every floor is the same floor |
| Caught bodies' deaths | The ring's owner claims them; the host confirms | The set's claims (TECH_SPEC 8.1) |
| The floor | The host | Lit counts a player in the heartbeat (the census); on a mismatch the host sends the whole floor (903 tiles at 3 bits: one packet) |
| FULL HOUSE, the knock-out's tiles, a cut, a spin-out | The host (a cut and a spin-out: the device of the ship it happens to says so; the host tells all) | Reliable events |

The densest moment: the last Drop with four players and a full crowd of 200. There are no shots in
the room, so it is lighter than the set's own Drops; its benchmark is in `docs/MODES_PLAN.md`.

## What to check

- Geometry, as unit tests: a square flown clockwise and one flown the other way light the same tiles;
  a figure of eight closes its second loop only; the snap closes at 36 wu and not at 37; a ring of
  three tiles fizzles; a ring thinned to 48 corners lights within one tile's width of the full line's.
- The same rings in the same order give the same floor on two worlds (the determinism test, and the
  loopback soak with four tracer bots: every device's lit counts equal the host's at every heartbeat).
- A bot that flies circles round the nearest twelve bodies fills at least one house on every Track.
- The child-like bot (0.5 s reactions, no Dash) is knocked out no more than twice a run on Mirrorball.
- Pictures (`docs/MODES_PLAN.md`): a line half drawn with its hint, a lock, a pop as a strip a
  thirty-second apart, the floor at 60% in four colours, FULL HOUSE as a strip, the count.
