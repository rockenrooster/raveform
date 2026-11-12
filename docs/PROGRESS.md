# RAVEFORM — Progress

## Play test 2 — 2026-11-12 (the user's second session; design in `8b654f1`)

Five notes, done between M4 stage B's third checkpoint (`8dbcd3b`) and the rest of stage B.

### What changed

- **Tracks three quarters as long** (GAME_DESIGN 4.1, 4.2): Track 1 plays three Short cycles (72 bars), later Tracks
  three Standard cycles of 12 / 4 / 16 / 4 (108 bars). Everything that assumed four cycles or 16 / 8 / 16 / 8 was
  checked: the director (`kSectionBars`, `cyclesOfTrack`), the band's form table and its first Groove
  (`engine.cpp`), the risers it bakes (four-bar Builds in both forms now), the phrase bar and the set list (they read
  the form and the clock), `SpeciesTable::kCycleBars`, the tests (the 36-bar form, three cycles in Track 1, the
  mini-boss slot the middle cycle's Drop, the boss the last), the `hud_drop` picture (Track 1's third Drop is the
  boss's now: the second cycle's, at 72.75 s) and the densest part's fast-forward (below). The loudness calibration
  is untouched: it hashes the music data and renders the staged arrangement, neither of which changed
  (`--calibrate --check` passes).
- **Shard levels** (GAME_DESIGN 6.1): collected Shards fill a meter under the Chain (`LV n`, a 220 x 5 px bar, gold
  on a white outline at 0.3; it flashes white when full with a two-note rise of the player's lead, the fifth then
  the octave, and empties as the Trio opens). Level n takes 170 x 1.17^(n-1) Shards (below). A full meter opens a
  level-up Trio round the ship on the next bar line, in the fight: gold ring, four bars, `LEVEL n` at its centre for
  the first bar, the same lock, hold and nearest-orb rule, each orb's thin ring drawn double. Its orbs are shelters:
  nothing spawns inside the ring, a ship inside one of its orbs' rings cannot be hurt, bodies touching a ring are
  set outside it and shoved off (420 wu/s). Levels filled while a Trio is open are banked and open one at a time.
  When fewer than three upgrades can be taken the remaining orbs offer different Boosts (three once everything is
  capped). Each player's meter and Trios are their own (co-op: the others hear of the Trio, its level and kind).
- **Cells** (health pickups): a director clock per hurt player, due about every 40 s with one cell missing and 15 s
  on the last cell (linear between); the next kill near the player then drops it; at most two loose; a boss drops
  two for each player. Eight bars of life, pulled by the magnet, only for their player (each device holds its own
  player's). Drawn as look.md says (a gold ring of 16 wu with a white plus, 15% swell on the beat, a 120 px
  vertical flare; off screen on the last cell a gold chevron with a plus); taken, the halo's arc draws itself in
  over a quarter beat with a rising lead note and eight gold sparks.
- **Extra Cell to cap 4** (eight cells: `kMaxCells` 8, `halo_7` and `halo_8`); **Regen** (Mod 18, now Mod 9 of the
  slice: Riffs are numbered one later, protocol version `0x52460404`): a missing cell grows back every 16, 8, then 4
  bars, drawn as a dashed arc filling clockwise in the place of the next arc (the halo shows its whole cells in an
  n + 1 layout: `halo_k_of_n` glyphs made at load from the halo's arcs).
- **Prism** (look.md 5): lanes at 0, ±60 and ±120 degrees round the owner's OKLCH hue at lightness 0.70 and full
  chroma, out of the 30-degree band round the danger hue; the lane's colour end to end with white only in the front
  15%, at 0.6 of the shot level and 1.3 of its width; glow, sparks and mini-refractions in the lane's colour.
  **The tone map no longer washes it out** (`captures\playtest2\prism_compare.png`): orange, green, the player's
  cyan, blue and magenta read as such; only the front 15% is white.
- **The build on the shots** (look.md 5): the tail in the owner's colour, the head (front 45%) by Overcharge (white,
  amber, gold, white with a gold rim, ice blue, violet with a white core), width x1 to x1.8 and +8% length a stack;
  sparks off every second shot at 3 and every shot at 4-5; an afterglow streak at 5; the muzzle flash and impact
  sparks in the head's colour, the impact flash doubled at 5. On-Beat Crit's four-point star (10, 14, 18 wu, white to
  gold) and a gold 80 wu burst ring; Pierce's chevrons; what a Riff throws by its level (base, light, white core with
  the player's rim).
- **The network** (found by the soaks): the faster game doubled the crowd's churn and both four-player soaks
  failed (270 and 326 kbit/s worst second; 22 wu error with loss). A spawn now costs 7 bytes in a formation (17
  before: only what differs from the item before) and a death a byte or two (a varint in serial order). Both soaks
  pass with more room than before play test 2: 213 and 211 kbit/s worst second (limit 250), error 4.8 and 7.0 wu
  (limit 12), predicted kills undone 1.0% with 5% loss and 80 ms jitter.

### The level pace (bots, 20 seeds x 30 minutes)

| seconds a level | Track 1 | Track 2 | Track 3 | the loop |
|---|---|---|---|---|
| careful bot | 27.5 | **16.2** | **31.2** | 99 |
| careless bot | 20.0 | **17.2** | **33.6** | 102 |
| Shards collected a second (careful) | 10 | 78 | 152 | 236 |

The design's 25 x 1.22 gave a level every 9 s in Track 1. **No base and ratio put all three Tracks inside 20-30
s**: the Shard income rises about 7x from Track 1 to Track 2 but only 2x from Track 2 to Track 3, while a geometric
need rises by the same factor every level. Tried (careful bot, 8 seeds x 12 min): 160 / 1.22 (29.6, 20.3, 40.7 s),
150 / 1.20 (27.7, 17.7, 36.9), 190 / 1.16 (30.7, 15.8, 30.6), 130 / 1.18 (20.7, 16.5, 31.7), **170 / 1.17** (28.3,
16.5, 30.1): the smallest miss. Track 2 runs about 3 s fast and Track 3 1-4 s slow.

Levels and upgrades held in the first run (both bots, every run still in it):

| | careful: level, upgrades taken (Mod stacks, Riff levels) | careless |
|---|---|---|
| minute 3 | 6.3, 9.8 (5.2, 4.6) | 8.7, 12.2 (6.8, 5.5) |
| minute 6 | 17.9, 24.9 (15.3, 9.6) | 19.0, 26.4 (15.8, 10.6) |
| minute 10 | 24.9, 37.2 (23.1, 14.2), 5.8 cells | 25.8, 38.2 (23.8, 14.5), 6.0 cells |

### The bots against play test 1's targets (`--sim crowd --seeds 20 --minutes 30`)

| | play test 1, second pass | play test 2 | target |
|---|---|---|---|
| careful: first death | none in 18 of 20 runs | **none in 20 of 20** | minute 15 or later |
| careful: boss fights lost | 1.4% | **0 of 220** | at most 1 in 10 |
| careless: median first death | 6.37 min | **none in 20 of 20** | minute 4 or later |
| careless: Track 2 reached | 20 of 20 | 20 of 20 (Track 3 in 20) | 10 of 20 |
| careless: boss fights lost | 45.2% | **0 of 220** | |
| kills a minute (careful / careless) | 6,010 / 1,539 | 9,773 / 9,928 | |
| pools | no overflow | no overflow; at most 835 shots a player (cap 1,000) | none |

**The game is now far easier than the targets**: neither bot dies in 30 minutes and no boss fight is lost; Cells
(56 taken by the careful bot, 151 by the careless one) and Regen (107 cells grown back for the careless bot) keep
even the careless bot alive. Not retuned (the brief: ask first).

Pools with the faster pace: no overflow in any crowd run or soak; Shards up to 1,561 (the crowded rule holds them);
the four-player shot pool's worst case (Split 7, Prism, Hyper, 174 BPM) peaks at 3,478 of 4,096 (871 a player); a
device holds its own player's Shards and Cells only, so four players add none to its Shard pool.

### The Moto (`bench_play`, our own APK, the lock, `--silent` only on the PC)

| run (120 s measured) | frames missed (panel) | panel P95 | max | main P95 | GPU P95 | sim tick P95 / max | alive mean / max | in view mean / max |
|---|---|---|---|---|---|---|---|---|
| minute 10, the real game (`-Warmup 600`) | 0 of 7,188 | 1 period | 21.0 ms | 5.34 ms | 13.17 ms | 0.54 / 1.37 ms | 575 / 900 | 103 / 389 |
| the densest part (`--fast-forward 640 --nohits`, the loop's Track 5) | 0 of 7,188 | 1 period | 19.7 ms | 5.33 ms | 13.42 ms | 0.51 / 1.46 ms | 605 / 900 | 121 / 426 |
| Track 3, cycle 2 (`--fast-forward 450 --nohits`) | 0 of 7,188 | 1 period | 20.1 ms | 5.17 ms | 14.02 ms | 0.44 / 1.60 ms | 520 / 900 | 96 / 353 |

**All three PASS the contract** (graded on the panel: P95 at most 16.9 ms, missed at most 5%, max at most 50 ms);
no audio underruns, no heap allocations of ours, no pool overflow, Shards up to 1,561. The ungraded CPU-side line
(the present call's return) reads P95 17.8 ms in all three, as before. At minute 10 the bot is in the loop's Track 5
with 41 upgrades (`captures\bench\ZY22BXMJG3-bench_play-20261112-161201.json`, `-160921`, `-160642`).

### Tests

`raveform_sim_tests` 5,460 checks, 0 failures: level thresholds (base x ratio^(n-1), rising), banking (one Trio at
a time, each on the bar line after the last closed), a level that would run into a Break waits, the Break's Trio
waits for an open level-up Trio and opens with the rest of the Break, the shelter (no hit inside an orb's ring, a
body set outside it, a hit outside), nothing spawning inside the ring, the Cells' clock (40 s with one missing, 15 s
on the last, resting at full health), a kill dropping the due Cell, at most two loose, taken on touch, waiting at
full health, gone after eight bars, a boss's two, Regen at 16 / 8 / 4 bars and nothing while full, eight cells, the
36-bar form, three cycles in Track 1, Boost orbs. `raveform_net_tests` 131,340 checks (compact spawns and deaths
round-trip at 7 and 1-2 bytes); `scripts\test.ps1` passes (determinism, both soaks).

### Pictures (`captures\playtest2\`, each with `.small.png`; Debug, 0 validation messages)

`prism_compare` (before | after), `pt2_level_trio` (a level-up Trio in a fight, the ship sheltered in an orb, LEVEL
7), `overcharge_sheet` (stacks 0-5), `pt2_cell_crowd` (a Cell in a crowd on the last cell, and a Cell's off-screen
chevron), `pt2_halo8` and `pt2_halo8_zoom` (eight cells, one growing back), `pt2_hud_level` (the Shard level).

### Choices made (play test 2)

- Levels start at 0; the first full meter is `LEVEL 1`. A level-up Trio does not open when a Break begins within its
  four bars (it waits, banked); if the boss dies while one is open, the Break's Trios wait for it to close.
- The shelter is the Trio owner's; it ends when the clock runs out (the nearest orb flying in). A body touching a
  ring is set outside it and pushed at 420 wu/s; the boss is not moved, bullets pass.
- Boost orbs fill any orb no upgrade can (not only when everything is capped), in the Break's Trio too.
- Cells: at zero cells the clock runs as on the last cell; the clock rests and restarts at full health; a Break's
  dissolve is not a kill; with two loose a due Cell waits; a Cell is neither pulled nor taken at full health (it
  lies until it fades); Magnet Storm pulls Cells while the player is hurt; taken within 30 wu. No elites or
  mini-bosses exist in the slice, so only the boss drops Cells unconditionally.
- Regen counts bar lines while a cell is missing (a hit does not reset it); it is silent; the growing arc is drawn
  on your own ship only (a teammate's Regen is not on the wire).
- Overcharge's coloured heads are drawn at Prism's 0.6 of the shot level, or the tone map bleaches them to white as it
  did the first Prism; Overcharge 3's spark flies off sideways from the muzzle; Overcharge 1-4 hits get a small
  impact flash so that 5's can double.
- The densest part is now the loop's Track 5 (`--fast-forward 640`, 121 enemies in view on average against 96 for
  Track 3 cycle 2 at 450 s); both were measured.

### The lead's review of play test 2 (`f48a193`)

- **The coloured head is 65% of the streak** (the rear 35% stays the owner's colour):
  `captures\playtest2\overcharge_2_5_head65.png` (Overcharge 2 and 5).
- **The Trio's hint sits above an active Boost's label** (it was drawn over `HYPER / FIRE RATE X2`):
  `captures\playtest2\review\pt2_level_trio.png`.
- **The loop ramps** (GAME_DESIGN 8, "The loop is where a run ends"): from Track 4 on, every Track multiplies the
  threat budget (the Drop's mega-wave too) and the bosses' bullet density by `kLoopThreatRamp`, enemy health (the
  boss's too) by `kLoopHealthRamp` and the Cell clocks by `kLoopCellRamp`, compounding (`tuning.h`,
  `sim/director.h loopRamp`); the first pass is untouched. A boss's bullet density stops at 3.5x kaleidos.json's
  counts (`kBossBulletDensityMax`): without that cap 40-minute runs overflowed the 2,048 enemy bullets hundreds of
  thousands of times by Track 15 (the naked core's ring at 12x); with it, no pool overflows.
- **The targets cannot be reached with the three factors inside 1.15-1.5.** Neither bot dies in 40 minutes even at
  the top of the range:

  | factors (threat, health, Cells) | runs | careful: deaths, hits a run, Track reached | careless: deaths, hits a run, Track reached |
  |---|---|---|---|
  | 1.3, 1.25, 1.25 (the design's) | 6 seeds x 40 min | none, 8, Track 15 | none, 58, Track 15 |
  | 1.5, 1.5, 1.5 (the most allowed) | 4 seeds x 40 min | none, 17, Track 13 | none, 99, Track 12-13 |

  The ramps never reach the ship: at Track 13 enemies have 57x their health and the threat budget is 57x, yet the
  careful bot is hit about 0.4 times a minute (nearly all in boss fights) and still kills 6,800 enemies a minute; the
  alive cap (900) holds the counts. The careless bot is hit about 2.5 times a minute and is kept alive by Regen (78
  cells grown back a run), Cells, the Breaks' refills, Auto Nova and Halo (from Boost orbs once everything is
  capped). The factors stay at the design's 1.3 / 1.25 / 1.25 until the lead picks another lever (asked).
- The Moto's densest part with the ramp (`--fast-forward 640 --nohits`, Track 5): 0 of 7,188 frames missed, panel
  P95 1 period, max 19.7 ms, **PASS**; alive 601 / 901, enemy bullets 9 / 134, in view 120 / 447
  (`captures\bench\ZY22BXMJG3-bench_play-20261112-163516.json`).
- Tests: `raveform_sim_tests` 5,466 checks (the ramp: 1 through Track 3, then compounding; a Track 5 enemy's health
  and Cell clock), `scripts\test.ps1` passes.

### The loop takes the sustain away (the lead's decision, `2fba23c`)

From the loop's first Track on, besides the three factors: the Breaks give back no cell and no Nova
(`kLoopBreakRefills`); Regen's periods stretch by 1.5 a Track (the view's growing arc follows,
`World::regenPeriod`); enemies move 6% a Track faster (at most x1.6), enemy bullets 6% (at most x1.5), and every
shooter's volley (a Hound B's burst, a Sentry's arms; the bosses keep their own density ramp) grows 15% a Track (at most
x3, the fraction by the enemy's own random stream, and only while a quarter of the 2,048 enemy bullets is free).
The loop's first Track card reads `ENCORE` over `TRACK 4`, the name, and `NO MORE REFILLS` under it
(`captures\playtest2\loop_encore.png`).

**The targets cannot be reached inside the bounds** (each factor within a third of the design's step: threat 1.2-1.4,
health and Cells 1.167-1.333, Regen 1.333-1.667, the speeds 1.04-1.08, shooters 1.10-1.20; the caps as designed).
What each lever did, added one at a time at the top of its bound (6 seeds x 40 minutes each bot):

| levers | careful: deaths in 6, hits a run | careless: median first death (Tracks) |
|---|---|---|
| the design's values, all levers | 0, 12 | 33.6 min (12-14) |
| top of bounds: threat, health, Cells only | 0, 14 | none in 40 min |
| + the Breaks refill nothing | 0, 13 | none |
| + Regen stretched x1.667 a Track | 0, 11 | 31.7 min (11-12) |
| + enemy speed x1.08 a Track | 0, 15 | 31.5 min (11-12) |
| + enemy bullet speed x1.08 a Track | 0, 13 | 30.6 min (11-12) |
| + volleys x1.2 a Track (every lever at its top) | 1 (36.9 min, Track 13), 13 | 30.2 min (11-12) |

Targets: the careful bot's median first death at minutes 18-30, the careless bot's at 9-16. Only Regen's stretch
moved the careless bot (from no death to about minute 31, Track 11-12, where Regen has all but stopped); every other
lever moved it by less than a minute, and the careful bot is hit about 13 times a run whatever the levers do. Nobody
dies in the first pass. Stopped there as asked; the levers stay at the design's values, which keep every pool inside
its cap (overflow 0 in all fourteen runs).

- The Moto's densest part with these levers (`--fast-forward 640 --nohits`, Track 5): 0 of 7,188 frames missed,
  panel P95 1 period, max 20.8 ms, **PASS**; alive 566 / 900, enemy bullets 20 / 276 (they were 9 / 134), in view
  128 / 543 (`captures\bench\ZY22BXMJG3-bench_play-20261112-164949.json`).
- `scripts\test.ps1` passes (`raveform_sim_tests` 5,472 checks: no refill at a loop Break, Regen's stretched period,
  faster enemy bullets, a Track 7 Sentry's larger volley; both soaks 211-213 kbit/s worst second, error 5.1 / 7.0 wu).

## Play test 1, second pass — 2026-11-12 (the lead's ruling in `39afc70`)

- **Auto Nova** (on by default; `AUTO NOVA  FIRES ONE BEFORE YOUR LAST CELL GOES` under CONTROLS, saved as
  `auto_nova`): a hit that would take the last Shield cell fires a Nova from the stock instead and costs no cell;
  the Nova keeps R6 (invulnerable at once, the blast on the next beat). It is a rule of the ship (`Rules::autoNova`,
  this device's players; the bots have it as anyone does). Chosen: it also catches the hit that would end the run
  at zero cells, which can only happen when the stock was empty at the last cell's hit. With no Nova in stock the
  hit lands as before.
- **Invulnerability after a hit 2.5 s** (was 1.5). **The first boss of a run (Track 1's Kaleidos) has 0.7 of its
  health**; later bosses and the loop are unchanged.
- **The director's alive cap 900 solo** (was 600; +150 a seat stays: 1,350 with four). The Moto's densest part
  (`bench_play --fast-forward 640 --nohits`, the stronger build, now the loop's Track 4 at that point;
  `captures\bench\ZY22BXMJG3-bench_play-20261112-133722.json`): 0 of 7,188 frames missed, panel P95 1 period,
  max 23.1 ms: **PASS**, so 900 stays. Main 4.89 / 5.23 / 7.85 ms, GPU 11.89 / 13.56 / 14.92 ms, sim tick
  0.30 / 0.48 / 0.71 ms, alive 528 / 900, enemies in view 184 / 583 (stage C2: 126 / 396), Shards up to 1,540, no
  pool overflow, heap 0.
- The Shard pool needed one more rule with the higher cap (656 Shards dropped over 20 careful runs): while it is
  crowded (more than 1,536 out) a death drops one Shard, and none within 64 of the 2,048. None dropped since.

Bots, 20 seeds of 30 minutes (`--sim crowd`, the final build), against play test 1's first pass:

| | first pass (levers 0.7, cap 600) | second pass | target |
|---|---|---|---|
| careful: first death | no death in 15 of 20 runs | **no death in 18 of 20** (the two at 15.6 and 23.3 min) | 15 min or later |
| careful: Track 3 / boss fights lost | 20 of 20 / 3.5% | 20 of 20 / **1.4%** | 70% / at most 10% |
| careless: median first death | 3.01 min | **6.37 min** | 4 min or later |
| careless: Track 2 reached | 6 of 20 | **20 of 20** (Track 3 in 4) | 10 of 20 |
| careless: boss fights lost | 93.6% | 45.2% | |
| kills a minute (careful / careless) | 4,069 / 697 | 6,010 / 1,539 | |
| pools | no overflow | no overflow | none |

Both bots meet their targets; tuning stops here. Enemies in view during Groove (careful bot), Track 3 cycles 1-3:
117 / 124 / 124 before, **127 / 145 / 147** after; the loop's Track 4: 94 / 92 / 93 / 101 before, **129 / 128 / 128 /
130** after (alive 730 there, the cap 900 no longer binding in Groove).

The network: with more to kill and the bots living longer, the loopback soak's worst second rose to 264-278 kbit/s
per client (the limit is 250), and the host's tick log overflowed once at 1,350 alive (a Break dissolving the whole
crowd). The tick log now holds every enemy's death, and the corrections' share of the downlink is 160 kbit/s instead
of 184: both soaks pass at 239 kbit/s worst second (clean: error 7.5 wu RMS, limit 12, was 3.8; with 5% loss and
80 ms jitter: 7.5 wu, predicted kills undone 0.8%), with the +150 a seat kept (1,350 alive with four).

Tests: Auto Nova fires on the last cell, not without a Nova, not when switched off; a hit that leaves cells is a
hit with 2.5 s of invulnerability; Track 1's boss at 0.7; the cap at 900 (`raveform_sim_tests` 5,268 checks, 0
failures; `scripts\test.ps1` passes). The APK and the desktop build are rebuilt.

## Play test 1 — 2026-11-12 (the user's first play test; design in `004c594`)

Three changes to solo play, done before the rest of M4 stage B (whose first checkpoint, `988f718`, came just before).

### What changed

- **A Trio is never taken by accident** (GAME_DESIGN 6.1, screens.md "The Trio"). The orbs are locked for the
  Break's first two beats: the ring draws itself over the first beat, the orbs fade in over the second as outlines
  at 0.5 with their words readable, and on the third beat they light (a short white flash) with a chime. After
  that an orb is taken by staying inside its thin ring (1.42 x the icon) for two beats while it fills as a solid
  arc in the player colour, clockwise from twelve, with two rising notes of the player's lead (the root as the fill
  starts, the fifth a beat later). Flying through does nothing; leaving empties it at once. If none is taken, the
  orb nearest the ship flies to it at the Break's end (it was a random one). The hint reads `STAY ON ONE TO TAKE IT`
  on the first three Trios of a run. The bot holds still on its choice (both policies). In co-op the rule is each
  player's own, on its own device.
- **The camera leads half as far** (GAME_DESIGN 9): `kLookAheadS` 0.25 -> 0.125 s and `kAimBiasWu` 60 -> 30 wu in
  `play_view.cpp`; the ease times are unchanged. Nothing else was tuned around the old lead: the crowd is counted in
  a frame centred on the ship (not the camera), floods pick their edge from the ship's heading, edge chevrons
  follow the camera, the boss lean has its own constants. The one copy of the old numbers was the camera model in
  `--sim crowd` (its "in view" column), now 0.125 s and 30 wu too.
- **Upgrades felt at once, an easy game** (GAME_DESIGN 5, 6.2, 6.3, 6.6, 8): 4 Shield cells at the start (cap 6), 3
  Novas (capacity 4), one of each back at every Break (every present living player, on every device), the Dash in
  two beats. Split +2 lanes a stack (3, 5, 7; cap 3); Pierce +2; Ricochet +2 bounces, off walls and from each enemy
  hit on to the nearest other solid enemy within 300 wu; Overcharge +50%; Extra Cell +1 capacity and every cell
  refilled (cap 2); Magnet +100% of the base radius a stack (90, 180, 270, 360, 450 wu); On-Beat Crit x3, x5, x8 with a
  burst over 80 wu. Orbitals 3, 5, 7, 9, 11 shards that also destroy the enemy bullets they touch; Pulse Ring on
  every beat from level 1 at twice M3's damage, then a second ring, wider, heavier, and every eighth at level 5;
  Sentinel a burst of 3, then 4, a second drone, 5, a third drone. The effect words are screens.md's new table. The
  per-stack numbers live in one place (`sim/upgrades.h`), read by the world, the view and the words.
- New engine command `LeadNote` (a player's lead on the Track's root, now or on a tick: the chime and the hold's
  notes; M4's revive fill will use it), and two for stage B's band (`SetBeacon`, `SetHostOnly`), idle until then.

### Bots against the targets (`raveform_headless --sim crowd --seeds 20 --minutes 30`, both policies)

| | before (the first slice) | after, levers 1.0 | after, levers 0.7 (shipped) | target |
|---|---|---|---|---|
| careful: median first death | 6.07 min | no death in 16 of 20 runs | no death in 15 of 20 runs | 15 min or later |
| careful: Track 3 reached | 0 of 20 | 20 of 20 (Track 8-9 by minute 30) | 20 of 20 | 70% |
| careful: boss fights lost | 39 of 116 (34%) | 5 of 145 (3.4%) | 5 of 143 (3.5%) | at most 10% |
| careful: kills a minute | 553 | 4,327 | 4,069 | |
| careless: median first death | 1.21 min | 2.69 min | **3.01 min** | 4 min or later |
| careless: Track 2 reached | 0 of 20 | 1 of 20 | **6 of 20** | 10 of 20 |
| careless: boss fights lost | 11 of 11 | 86 of 88 | 88 of 94 | |
| careless: kills a minute | 503 | 683 | 697 | |
| pools | no overflow | **13,061 Shards dropped** (careful) | no overflow | none |

Enemies in view during Groove (careful bot, Track 1 cycles 1-4 / Track 2 cycles 1-3): before 10.7 / 28.6 / 47.7 /
70.7 / 86.5 / 115.6 / 124.6, after 9.7 / 26.8 / 40.3 / 63.9 / 88.8 / 106.5 / 126.0; Track 3 is now reached (117 /
124 / 124) and the loop's Track 4 too (92-101, the director's alive cap of 600 holds it there). The crowd targets
are untouched.

**The careless bot misses both of its targets** even with both levers at their floor (`tuning.h`:
`kThreatBudgetScale` 0.7, `kBossBulletScale` 0.7: the bosses' fans and core rings at 0.7 of kaleidos.json's
counts). Its first death is the first boss fight (Track 1's Kaleidos comes at about 3.2 min): 93.6% of its boss
fights are lost, and half of all its hits are Shoal bodies it flies into (it never dodges bodies or bullets, and
uses neither Nova nor Dash). Neither lever reaches that. The next levers would be the lead's: what a Shoal or
Drifter body costs on contact, the invulnerability after a hit (1.5 s), or Kaleidos's health for Track 1.

### Choices made (play test 1)

- **The shot pool's caps** (the brief asked for a rule if one was needed): a player's live shots are counted each
  tick. Above 576, every pair of Split lanes beyond the middle one fires on every second step (the pairs take
  turns) and Prism's mini-refractions stop; above 800 the middle lane alone fires (with its Prism fan); a player
  never has more than 1,000 (a guard). A Prism shot refracts on its first hit only. Measured worst case (sim test:
  four players, each with Split 7, Double-Time 2, Pierce 3, Ricochet 3, Crit 3, every Riff at level 5, Prism and
  Hyper refreshed, 174 BPM, a crowd of 500, 20 s): pool peak 3,478 of 4,096, a player 871 of 1,024, nothing
  dropped, the guard never reached. The bots' 30-minute runs peak at 642 shots.
- **The Shard pool**: the stronger build kills eight times as fast late in a run, and the 2,048 Shards overflowed
  (13,061 dropped over 20 runs). While more than 1,536 are out, a loose Shard lasts one bar instead of four. No
  Shard was dropped after.
- Ricochet: Pierce first (a shot passes through while it has Pierce left), then each hit bounces on to the nearest
  other solid enemy within 300 wu; with none there the shot is spent. On-Beat Crit's burst: every other enemy
  within 80 wu of the hit takes half the beat shot's damage, drawn as a quick ring in the shooter's colours.
- Pulse Ring's "twice as often" at level 5 is a ring every eighth; Sentinel's second and third drones sit mirrored
  beside the ship and behind it, each picking its own nearest enemy.
- The chime is a bell (P3's timbre) two octaves over the Track's root, placed on the unlock beat; the hold's notes
  are the player's own lead, 0.8 of its level.

### Checks

- **Moto, the real game at minute 10** (bench_play, the careful bot, 600 s of warm-up then 120 s; tier medium;
  shared phone, foreign load 1.01 cores; nothing in front before the run; `captures\bench\ZY22BXMJG3-bench_play-20261112-131820.json`):
  0 of 7,188 frames missed, panel P95 1 period, max 19.7 ms: **PASS**. Main thread 4.66 / 4.93 / 5.81 ms, GPU
  13.45 / 14.22 / 15.14 ms, sim tick 0.25 / 0.34 / 0.75 ms (stage C2: 0.28 / 0.66 / 1.23), heap allocations ours
  0, no pool overflow. The stronger build is already in **Track 4** at minute 12 (C2: Track 2), 30,680 kills in the
  window, alive 398 / 600, Shards 393 / 807, enemies in view 83 / 293, no death.

- `raveform_sim_tests`: 5,257 checks, 0 failures, with the tests moved to the new rules (Trio lock, hold, leaving,
  the nearest at the end; cells and Novas back at a Break; Ricochet, the Crit burst, Orbitals eating bullets; the
  per-stack numbers and words; the shot pool's worst case). Determinism passes (new hash `6f07f9f66a12842c`: the
  rules changed). Audio tests 2,134 / 0, loudness check, network tests 87 / 0.
- To play it: the APK `android\app\build\outputs\apk\dev\app-dev.apk` (installed on the Moto by the run above) and
  the desktop build `build\desktop-dev\raveform.exe` (`scripts\run-desktop.ps1 -Config dev -NoBuild`; add `-Sound` to
  hear it).
- Pictures in `captures\playtest1\` (`*.small.png`): `trio_locked` (under two beats in: outlines, words readable),
  `trio_hold` (the hold half filled), `split7`, `riff_orbitals` (level 3: seven shards), `bot_10` (the careful bot's
  real run at minute 10: Track 3, score 2.6 billion, Chain x13,363), from `scripts\review-m3b.ps1 -Out
  captures\playtest1 -Only ...` (the first four Debug with validation, `bot_10` RelWithDebInfo).

## M4 stage A "LAN co-op: the network" — 2026-11-12

Built on `main` (never pushed): `0fb5003` (the network core: transport, sync, replication, co-op simulation,
loopback soak), `de58136` (co-op in the game, bot clients in the process, silent PC runs) and the closing commit
(budget, docs). Stage B (lobby, discovery, room codes, the screens) is next.

### What was built

- **`src/net`**, a static library like `src/sim` (no SDL, Vulkan or audio; the game, `raveform_headless` and the
  tests link it). ENet 1.3.18 (tag `v1.3.18`, commit `2662c0de09e3`, MIT) is fetched by `fetch-deps.ps1` with tag
  and commit checked and compiled unmodified: a forced include (`enet_remap.h`) routes its socket and clock calls to
  `wire.cpp`, whose virtual sockets are real UDP or an in-process loopback network, with a network condition
  simulator under both (delay, jitter, loss, duplication, reordering; PCG32 draws). ENet's memory comes from fixed
  pools. Ports 47778 (game) and 47777 (beacon, stage B); channel 0 reliable, 1 unreliable, 2 control.
- **Time and song sync** (`sync.h`): NTP-style pings at 4 Hz (20 Hz until 16 samples); the minimum-RTT sample of
  the last 16 leads a 1/RTT²-weighted window, averaged over about 20 s; the client's clock follows with at most 1%
  dilation (a snap only before a run). The host publishes its song anchored to host time (tick, tempo now, the next
  bar line and the tempo after it) with the 4 Hz heartbeat. In the game the host's song is its band's heard clock;
  a client ticks at 60 Hz of its host time, each tick at the song's position then, and steers its band toward the
  host's song: 0.03% of tempo per millisecond off, at most 0.3%, taken on bar lines; more than 60 ms off (the start,
  a stall) the band holds its next bar line until the host's song reaches one (the re-seek; engine commands
  `SetRate`, `HoldBar`).
- **Replication** (`session.cpp`, TECH_SPEC 8.1): poses at 30 Hz (two states; the host relays the others'), player
  events reliable, hit claims and the host's verdicts with the 300 ms undo of predicted kills, spawn batches at least
  two beats early, the director's state on change, the heartbeat with the boss's health, a census (count and an
  order-free digest of the live serials) every 2 s, and enemy corrections by a priority accumulator at 15 Hz a
  client (at most 32 a packet, under 1,000 bytes), applied as an offset decaying over 150 ms, snapped above 96 wu.
  Enemy bullets and boss attacks are not sent. Every packet is validated whole before any of it is used.
- **Co-op simulation** (`world_net.cpp`, the world's co-op branches; solo play unchanged): an enemy picks a player
  by distance and keeps it (looks again every fourth sixteenth, changes for one 0.7 times as near); Shards local,
  Boost orbs and Trios once per player; each player's own Chain, the team's score the sum (the HUD shows it); down
  and revive (a beacon; a teammate two beats in its ring revives it, else the next Break; half the Chain lost; all
  down ends the run and the host starts the next on a bar line); no friendly fire; pause for everyone on the next
  beat (P); boss health x(1 + 0.5 per extra player), threat budget x(1 + 0.6 per extra player).
- **Peers without a screen**: `raveform_headless --net soak|host|join` (commands in CLAUDE.md). The game hosts or
  joins from the command line (`--host`, `--join <ip>`), the bot or a person playing; `--loopback-bots N` runs N
  bot clients in the host's process on a worker thread. Teammates, their shots and downed players' beacons are drawn
  in their seat colours. The overlay's network page (F2, `--net-overlay`): RTT, loss, bandwidth, corrections,
  position error, clock, song and band offsets, census, the bots' worker time. The bench result gains a `net`
  object. `--net-capture-bar` and `--net-view-seat` take every peer's picture at one musical moment.
- **Tests** (`scripts\test.ps1`): 87 net checks (round trips; the packet fuzz: 200,000 random and 200,000 mutated
  packets, nothing allocated; the wire; clock and song; a short session) and both 10-minute loopback soaks.
- **PC runs are silent** (the user's rule, 2026-11-12): `--silent` zeroes the output at the device after the
  limiter (everything still runs and is measured); `run-desktop.ps1` and the review scripts pass it unless `-Sound`.

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Loopback soak, clean, 10 simulated min (`test.ps1`, seed 1) | 0 disconnects, 0 stalls; census 900 checks, 0 mismatches; on-screen error 5.57 wu RMS; song offset 1.47 ms RMS, 2.59 max; down 234.9 / up 38.2 kbit/s worst second; largest datagram 1,385 B; 0 overflow, 0 ENet pool fallbacks, 0 heap allocations | < 12 wu, < 10 ms, 250 / 60, ≤ 1,472 B, 0 |
| Loopback soak, 5% loss + 80 ms jitter, with a pause | 0 / 0; census 894 / 0; 3.55 wu; 1.82 / 4.09 ms; 199.9 / 36.2; 1,391 B; 0 / 0 / 0; the pause held and resumed on the same tick everywhere | as above |
| Ten seeds each way (final code) | all pass: clean 1.6-5.6 wu RMS, song max 2.6 ms, down ≤ 234.9, up ≤ 40.4; impaired 3.5-5.9 wu, song max ≤ 6.7 ms, down ≤ 232.1, up ≤ 41.5; no census mismatch | as above |
| Four processes over UDP on 127.0.0.1, 15 min | 0 disconnects, 0 stalls on all four; census 448 checks a client, 0 mismatches; on-screen error 3.53 / 6.59 / 8.46 wu RMS (as each correction lands); true song offset 2.27 / 5.27 / 2.93 ms RMS, max 4.19 / 9.79 / 5.73 ms; down ≤ 222.3, up ≤ 36.7 kbit/s worst second; largest datagram 1,388 B (host); 0 overflow, 0 pool fallbacks, 0 heap allocations on every peer (`captures\m4\udp4_final`; an earlier run with the 192 kbit/s budget: 4.6-6.4 wu, song max 7.9 ms) | as above |
| Solo unchanged | `test.ps1` passes (determinism for both bots: hashes `a328fe20f6918107`, `b78fc8ac8fa23f01` as before); `--sim crowd` 5 x 30 min careful and 5 x 10 min careless, `--sim soak` and determinism outputs identical to the pre-M4 baseline (timing lines aside) | unchanged |
| Moto, `bench_play` at minute 10, hosting three in-process loopback bots (`ZY22BXMJG3-bench_play-20261112-114310.json`) | 7,188 frames in 120 s, 0 missed (panel and present calls), worst interval 22.8 ms (P95 17.87; solo 17.71); heap allocations 0 (Swappy's and the other threads' counts as in solo); host tick p50/p95/max 0.36/0.59/8.30 ms (solo 0.28/0.66/1.23) with 653 enemies on average, 1,050 at most (solo 378/600); the bots' worker 1.2/1.9/7.1 ms a frame on its own core; GPU 12.72/13.87/14.31 ms (solo 13.19/13.64/14.09) | ≤ 5% missed, no frame > 50 ms, 0 allocations |
| Moto hosting, the PC's bot joining by IP over Wi-Fi, 5 min (`...-115632.json`, `captures\m4\wifi\pc_client.txt`) | PC client: 0 disconnects, 0 stalls, census 150 / 0, on-screen error 1.95 wu RMS (as each correction lands), down 180.0 / up 32.4 kbit/s, RTT 22-24 ms; Moto: 17,966 frames, 7 shown one period late (0.04%), worst 33.3 ms, tick 0.17/0.29/0.86 ms | as above |

Numbers are taken on a shared phone. The bench's "cpu (present call returns)" line grades P95 against 16.9 ms and
says FAIL in both co-op runs (17.87 and 17.70 ms), as it did in the accepted solo run (17.71); the panel grading
(Swappy's statistics) passes. The co-op Moto run used a downlink budget of 192 kbit/s, the final code 184 (no
effect on a host whose clients are in its own process). The Wi-Fi host's `net` object shows one disconnect: the
PC client finished its five minutes 2 s before the phone's window closed.

The song offset across processes peaked at 9.79 ms on one client of the final 15-minute run, close to the 10 ms
limit (its RMS 5.27 ms; four real-time peers share four cores on the PC); the loopback soaks' peaks are 2.6-6.7 ms.

On-screen error in the soak is measured every 0.3 s against the host's world in the same process; across processes
it is measured as each correction lands (the drawn position against the host's sample, both carried to the same
moment: the error at its largest), for enemies on that client's screen. The true song offset across processes is
measured with one PC clock (`--same-clock`); across the PC and the phone it cannot be (two clocks).

Pictures (`captures\m4\`, with `.small.png`): `coop_host_bar40` and `coop_client_bar40` (each peer's own view and
the network page at the same tick), `coop_same_place_host` and `coop_same_place_client` (both cameras on the host's
ship at the same tick: the enemies stand in the same places).

### Decisions taken alone

- Unreliable traffic goes unsequenced: ENet's sequenced channel threw away about 40% of packets under 80 ms jitter.
  Poses and anchors carry their own time; each correction is checked against the newest applied to that enemy.
- The accumulator knows only the host's copies, so a client also asks for the enemies on and around its screen
  (1.5 frames) that no correction has reached for 0.75 s (eight serials, 5 Hz, with its pose); the host sends those
  next, and a school's members always together. Without it, copies that drifted onto a client's screen (Shoals in a
  boss fight, a Carom that landed toward another player) went 5 s uncorrected.
- The downlink budget counts every byte to a client on the wire (ENet's headers, acknowledgements and resends
  included): 184 kbit/s with 1.5 KB saved; corrections take what the rest leaves. The margin to 250 is a wave's
  spawn batch and jitter bunching arrivals.
- Everything a device sends leaves with its pose (30 Hz): one datagram a frame to a peer.
- Claims leave with the pose, merged per enemy (hits add up, a Nova's kill replaces them) and delta-coded (a byte
  or two an item); TECH_SPEC 8.1 says "batched per tick". The host's result is the same.
- ENet's resend timeout is capped at one round-trip timeout (no doubling): at 5% loss each way four losses in a row
  stalled the reliable stream for 3.5-6 s (spawns arriving after their enemies should have landed).
- In co-op the gathering ticks of a school's flocking follow the run's tick count (the same on every device), not
  the world's.
- A client's simulation ticks on the host's song (its clock estimate), not on its band; its band is steered to the
  song. Co-op shots fire on the simulation's own grid, not the band's heard lead steps.
- Remote players' shots, Riffs and Novas are cosmetic on other devices (a remote Nova clears enemy bullets there);
  every death drops Shards only for the players within two frames of it; each owner's device opens its Trio and
  takes its Boost orb.
- When every player is down, the host starts the next run on a bar line by itself (no lobby in stage A).
- A pause takes hold on a beat at least 120 ms ahead; the run starts on a bar line at least a bar away, 3 s after
  the last seat joined.
- The condition simulator draws from PCG32 (xorshift32's correlated draws made loss bursts that disconnected peers).
- Teammates in their seat colours (`palettes.player[seat]`), the local ship in its own; beacons as a ring that fills
  as a teammate revives.

### Numbers chosen

`src/sim/tuning.h`: `kCoopAliveCapPerPlayer` 150 (600 to 1,050 with four), `kTargetRethinkSteps` 4,
`kTargetStick` 0.7, `kCoopShardReachW/H` 1,600 / 720 wu, `kReviveRingWu` 90 (quoted: budget 0.6, boss health 0.5,
revive 2 beats, undo 0.30 s, offset 0.150 s, snap 96 wu). `src/net/session.cpp`: downlink 23,000 B/s, 1,500 B saved;
32 corrections a packet at 15 Hz; poses 30 Hz; heartbeat 4 Hz; census 2 s; remote players drawn 66 ms behind;
priority weights 0.6 base, 8 within 1,100 x 520 wu of the client's ship, 2 within 1,700 x 800, x0.05 warping, +4 for
2 s after landing, x(1 + speed / 300); attention 5 Hz, 0.75 s, 1.5 frames, 8 serials; start lead 3 s.
`src/game/play_net.cpp`: band rate 0.03%/ms up to 0.3%, re-seek beyond 60 ms. `src/net/link.cpp`: resend timeout
at most RTT + 4 x variance (50 ms floor).

### Not done or not verified

- The band following the host's song was checked on the desktop (two processes: one re-seek at the start, then
  within 0.4 ms of the host's song), not on a phone client, and nobody has listened to two devices together.
- The OnePlus 12R and the tablet are not attached (`adb devices` lists the Moto only).
- Under 5% loss and 80 ms jitter 12-16% of a client's predicted kills are undone (their verdict came after 300 ms);
  an enemy then reappears until the host's verdict lands.
- Stage B: the lobby and its screens, beacons on 47777, room codes, the MulticastLock, "music from host only".

## M3 closed — the lead's ruling, 2026-11-12

M3 (stages A, B, C and C2) is accepted on the Moto and the desktop. PLAN §7 exit: the solo loop is
playable; the real game at minute 10 and in Track 3 meets the contract on the Moto (0 of 7,188 frames
missed twice, checked by the lead in the result files); cold start is inside 5 s; determinism passes; the
bot runs 30 simulated minutes with no crash, overflow or allocation. The OnePlus 12R and the tablet were
never attached, so "both devices" is open and stays on the user's list. M4 is next.

Rulings on what stage C2 left open:

- **In view at about 0.6 of the target is correct, not a miss.** The target counts the frame grown by a
  quarter each way, which is 1.56 times the view's area, and the director holds that count at 0.91-1.05
  of the target in Tracks 1 and 2. The lead's 0.6 floor sat on the geometric limit. `count.frameScale`
  stays 1.25: counting the plain frame would put 1.6 times as many enemies in view, and the careful bot
  is already at the 6-minute floor.
- **Track 3 and the loop level off at 130-150 in view** because `kDirectorAliveCap` (600) binds while
  more than half of the live bodies are outside the counted frame (alive 470-540 for 210-245 counted).
  Deferred to M5 batch 1, where Track 3 gets its own roster: first cut the share outside the frame
  (arrivals nearer, strays dissolving sooner), and raise the cap only with a Moto run behind it.
- **The careless bot's 1.41 minutes is accepted for M3.** The floor of 1.5 was a first guess for a bot
  that does not dodge. The user's play of the first two minutes decides. If it is too hard, the lever is
  the Drop in Track 1's first two cycles: its peak in view is 2.8-2.9 times the Groove target where the
  design's section scale says 1.6, because kills are replaced and the debt is paid on top.

## M3 stage C2 "the crowd in the frame" — 2026-11-12

Built on `main` (never pushed): `fc01068` (the crowd counted in the frame, Prism's rainbow, Chain
separators) and the docs commit. Design input: `37a6442` (GAME_DESIGN 5, 7 and 8; species.json
`firstSlice.crowd` `count`, `arrive`, `stray`; the reference renderer's soft lights).

### What changed

- **The crowd is counted in the frame** (`world_spawner.cpp`): a player's frame in the simulation is the
  reference view, 1,600 x 720 wu (2400 x 1080 px at 1.5 px a wu, zoom 1) centred on the ship
  (`tuning::kFrameHalfWWu/HWu`). The director counts the enemies in that frame grown by `count.frameScale`
  (1.25) and the crowd's fodder still warping in; floods land along one frame edge `arrive.floodOutsideWu`
  (120-320 wu) outside it, the edge the ship is heading for likelier; rings on the ellipse
  `arrive.ringFrameScale` (1.15) x the frame; corner swarms `arrive.cornerOutsideWu` (150 wu) outside each
  corner; fodder outside twice the frame (`stray.frameScale`) for `stray.bars` (2) bars dissolves without
  Shards. Arrivals keep 260 wu from a ship and out of the Trio's ring (tested). The neediest frame is
  filled (co-op ready).
- **The refill** (my mechanism, numbers chosen): each bar orders the frame's whole gap (species.json's
  `refillBars` 2 held the frame near half its target in view while the bot kills 550 a minute), plus a
  running debt (half the last shortfall, at most the last bar's kills and half the target, so an idle
  frame is not overfilled), plus the kills not yet replaced; killed fodder is replaced on the beat, a
  formation (at least 16) at a time; the Groove's crowd lands on the third beat of the Break's last bar
  and the Drop's with the mega-wave.
- **Fodder behaviour** (tuned): Drifters home at 1.0 rad/s (was 0.15) at their own speed; Shoals seek at
  0.5 (was 1.0) at 0.7 of their speed (133 wu/s, the -30% allowed); while a boss lives both keep 450 wu off
  the ships (they turn away inside it), so the fight has room.
- **The bot** (`bot.cpp`): in a boss fight it shoots a body only within 140 wu (was 250 for any target), and
  it keeps its last Nova for bullets (it used to spend them all on the crowd before the boss came).
- **Prism's rainbow** (GAME_DESIGN 7): the centre lane in the player's colour, the others 50 and 100 degrees
  round the OKLCH hue wheel to either side at lightness 0.80 and full chroma, white cores; the
  mini-refractions keep their lane's hue (the lane rides in the shot's flags, bits 2-4).
- **Score and Chain**: 64-bit throughout (sim `u64`, HUD, `highscores.txt` `%llu`, the bench JSON's
  64-bit integer); Chain with thousands separators wherever it shows (`X12,369`), as the score was.
- `--sim crowd`: in view with the game's own camera (its lead, the boss's lean and zoom), the counted
  frame, alive, hits by cause, boss fights won and lost; `--trace` adds a line a bar and a line a second
  of each boss fight. `--nohits` (app): hits cost nothing (a measured run deep in the game).
  `--picture bot_N` is now the careful bot's real run (it retries after a death); `bot_N_nohits` the old one.

### Numbers (M3 stage C2)

Enemies in view (the camera's frame) during Groove against the target, careful bot 20 seeds x 30 min; the
no-hits run (20 x 24 min) for Track 3 and the loop:

| Track, cycle | Target | In view (ratio) | Counted frame | Drop peak in view / target |
|---|---|---|---|---|
| 1, 1-4 | 20 / 45 / 80 / 120 | 10.7 / 28.6 / 47.7 / 70.7 (0.54 / 0.64 / 0.60 / 0.59) | 19 / 47 / 79 / 118 | 2.79 / 2.94 / 2.50 |
| 2, 1-3 | 150 / 190 / 230 | 86.5 / 115.6 / 124.6 (0.58 / 0.61 / 0.54) | 144 / 189 / 209 | 2.16 / 1.89 |
| 3, 1-3 (no hits) | 240 / 270 / 300 | 129 / 147 / 134 (0.54 / 0.54 / 0.45) | 210 / 234 / 223 | 1.47 / 1.17 |
| 4, 1-4 (no hits) | 320 | 136-147 (0.42-0.46) | 233-244 | 1.00-1.16 |

Difficulty: careful bot median first death **6.07 min**, Track 2 in **20 of 20** runs (Track 3 in none in
30 minutes), 4.3 deaths a run, boss fights won 77 (80 s on average) and lost 39; careless bot median first
death **1.41 min** (1.5-4 wanted), 18.4 deaths a run. Kills a minute: careful 553, careless 519, no hits
1,400. No overflow, no NaN, no heap allocation in the loops, no Shard dropped. What hits the careful bot:
bullets and beams 290 of 350 (189 of them in boss fights), fodder bodies 42. Files: `captures\m3c2\crowd_*.txt`.

- Tests: audio 2134, sim 5236 (the crowd's arrivals: 260 wu from the ship, within twice the frame plus a
  pack's scatter), 0 failures; determinism PASS for both bots.
- Desktop Debug, bench_play 60 s at minute 10 (fast-forward 600 s; Track 2): 0 validation messages, 0 heap
  allocations (frame, pacer, other threads, audio callback); enemies in view 102 / 274, bullets 9 / 60 (the
  desktop lost focus three times, so its timing is not graded). The densest part on desktop (fast-forward
  640 s with `--nohits`: Track 3, cycle 2, Drop): alive 500 / 600, in view 205 / 427, bullets 65 / 174.
- The M1 stills against the re-exported references (the reference fades soft lights as `glow.frag` does):
  `look_card` without bloom mean abs 0.36, soft light p10 / p50 / p90 0.99 / 0.99 / 1.00; with bloom 1.26,
  0.93 / 0.98 / 1.00; `frame_reef` 1.10, luminance ratio 0.982, 0.96 / 0.98 / 0.99 (`captures\m3c2\m1_stills\`).
- **Moto, the real game at minute 10** (bench_play, 600 s of warm-up, 120 s measured; tier medium, governor
  off, no cuts; shared phone, foreign load 0.82 cores; `captures\bench\ZY22BXMJG3-bench_play-20261112-084705.json`):
  0 of 7,188 frames missed, panel P95 1 period, max 19.7 ms: **PASS**. Main thread 4.60 / 4.97 / 5.98 ms
  (P50 / P95 / max), GPU 13.19 / 13.64 / 14.09 ms, sim tick 0.28 / 0.66 / 1.23 ms, audio underruns 0, heap
  allocations ours 0. **Enemies in view 108 / 379, enemy bullets in view 11 / 96** (mean / max; stage C:
  58 / 157 and 12 / 113); alive 378 / 600; Track 2, cycle 2 at the end, one death.
- **Moto, the densest part** (bench_play `--fast-forward 640 --nohits`: Track 3, cycles 2-3, 120 s;
  `...-085929.json`): 0 of 7,188 missed, P95 1 period, max 19.7 ms: **PASS**; main 4.69 / 5.02 / 5.91 ms,
  GPU 13.20 / 13.88 / 14.69 ms, sim tick 0.32 / 0.49 / 1.03 ms; enemies in view 126 / 396, bullets in view
  12 / 110, alive 464 / 602; heap 0.

### Pictures (`captures\m3c2\`, 2400x1080, `*.small.png`)

bot_3, bot_6, bot_10, bot_15 (the careful bot's real run: it died twice in Track 2 and went again, so
minute 15 is a new run's Track 1), bot_3_nohits ... bot_15_nohits (one run: minute 15 is Reef's
Kaleidos), hud_drop (Track 1's third Drop, 80 in view), boost_prism (the rainbow fan, Debug), and
moto_inapp_track2 (the Moto, in-app, Track 2's Build with `--nohits`). Bot frames and hud_drop from the dev
build.

### Not done (stage C2)

- **In view is short of 0.6 in three of the seven Track 1-2 cells** (0.54, 0.58, 0.54) and in Track 3 and
  the loop (0.42-0.54); the Drop peaks fall under 1.2 in Track 3's second cycle and the loop. Why: the
  counted frame is 1.56 times the view's area and every arrival lands outside the view, so with the
  frame at its target the view holds about 0.6 of it; the bot kills 550 a minute, so the frame sags
  between refills; from Track 3 the 600-body cap binds (alive 470-540, arrivals and threats included).
  Holding 0.6 everywhere would need the count in the plain frame (`count.frameScale` 1.0) or arrivals
  nearer, and a higher cap: the lead's call.
- **The careless bot dies at 1.41 min** (1.5 wanted): Shoal schools in the frame from Track 1's second
  cycle; slower and less seeking Shoals (0.7 speed, seek 0.5 or 0.25, turn rate halved) moved it from 1.23
  to 1.38-1.42 only.
- Kaleidos is still every Track's boss; the medley waits for M6.

Built on `main` (never pushed): `acc7b93` (the crowd, Reef, director and crowd tests), `416fdf7` (the
refill replaces killed fodder; the Groove's pre-spawn), `255ab44` (the bot fights the boss within reach),
`e7d3227` (items 3-9 of the brief) and the docs commit. Design input: `a5caacd` (GAME_DESIGN 8 and 9,
species.json `firstSlice.crowd` and `track3`, look.md 6, screens.md).

### What changed

- **The crowd** (GAME_DESIGN 8; `world_spawner.cpp`, `director.*`): the threat budget (and the Drop's
  mega-wave) buys threats only, hunters and specials. Fodder follows a population target near the ships
  (alive within `nearWu` 1,500 wu of any ship): `aliveTarget` by Track and cycle (20/45/80/120,
  150/190/230, 240/270/300; then +20 a Track up to 400) x the section's scale (Groove 1, Build 1 -> 1.3,
  Drop 1.6 -> 1.2, the boss's Drop 0.5, Break 0). Each bar the director closes the gap over `refillBars`
  (2) **plus every fodder body killed since the last bar** (without that the kill rate held the crowd at
  0.67-0.81 of its target), at least 16 and at most 80 a formation: floods (half; clusters of up to 8 along
  a line 900 wu out, 220 wu apart), rings (a quarter; up to six groups 700 wu round) and corner swarms (the
  rest; +-760 / +-330 wu); on the last beat of a Break it lands the coming Groove's target. Fodder further
  than `strayWu` (2,400 wu) from every ship for `strayBars` (4) bars dissolves without Shards. The 600 cap,
  the warp-in tells and the Trio ring's no-spawn rule stand.
- **Reef is Track 3** (decision 1 of stage B overruled by the lead): `track03`, 132 BPM, Standard form, three
  cycles, Track 2's roster, budget `track3`, palette reef, Kaleidos at 2x; then the loop over the three
  Tracks, each 4 BPM above the one before (up to 174), Kaleidos +6% a Track after Track 3. Names, palettes,
  the set list and the band follow (`songName`, `songOfTrack`).
- **Soft lights** (`glow.frag`): the Gaussian fades to zero over the last fifth of the cut-off radius, on a
  circle inside the quad, so no square edge shows at any intensity (the boss's death flash, warped-in
  Hounds).
- **Kaleidos's death** (kaleidos.md): the white flash clamped at 0.9 (the M1 boss break's) and gone in
  0.12 s, an accent glow after it (0.6, 0.45 s; reduce flashing 0.45 / 0.3); **50 ms hit-stop**
  (`look.json` `bossBreak.hitStopMs`): Play holds the ticks and the view's clock while the song runs on,
  then runs the held ticks three a frame on the same musical positions (the sim does not notice).
- **Edge chevrons** (look.md 6, `look.json` `edgeIndicator`; `PlayView::edgeChevron`): 44 px chevrons
  (danger for engaged shooters, gold for Boosts), Kaleidos a 64 px double chevron in its accent that swells
  15% on every beat; each on a dark disc at 0.5, 28 px inside the edge, with its 44 px soft light at 0.3;
  drawn on the threat layer (a capsule opacity above the flags, `capsuleOpacity`).
- **Boss camera** (GAME_DESIGN 9): while Kaleidos lives within 1,800 wu the zoom target is 0.85x and the
  camera leans toward it by 30% of the offset, at most 320 wu.
- **Boosts**: on the field the name sits under the orb (gold, cap 11 wu, 6 wu under its ring); the HUD row
  puts the name under the icon (cap 16.5 px = 11 wu at the reference, 9 px gap), pips to the right, and the
  effect words under the name for the two bars after the pickup.
- **Effect words**: screens.md's table word for word (changed: Double-Time `FIRE RATE X1.5` / `FIRE RATE
  X2`, Pierce `SHOTS PIERCE +1`, Hyper `FIRE RATE X2`, Magnet Storm `PULLS EVERY SHARD`); the stage B words
  below are superseded.
- **Track name card** (screens.md; `play_hud.cpp`): on every Track's first downbeat, the run's first too:
  `TRACK n` cap 28 white over the name at cap 110 in the palette's three hues, 8 px tubes, written left to
  right in a beat with a two-arm flare riding the writing, held to the end of the second bar, then the
  letters streak out to both sides on the third bar's downbeat; drawn at 0.8.
- **Leftovers**: SOFT and STRONG haptics differ (`RaveformActivity.haptic`, kinds 3-5 for STRONG; the
  vibrator's amplitude where it has amplitude control, else the length; a medium tick previews the setting);
  the pause puts the music bus behind a 600 Hz low-pass ramped over 150 ms (two one-poles, `SetMuffle`;
  the 0.45 dip stays); the boss pair's growl plays an octave down (an Sfx playback rate). Also fixed: RESTART
  from pause now lifts the pause's dip, and the boss card's subtitle (`BOSS OF <Track>`) sits under the name
  instead of across it.
- **The bot** (`bot.cpp`): with the crowd's pull it wandered thousands of wu from Kaleidos while aiming at
  it, so some boss fights never ended (4 of 20 god-mode runs stalled 10+ minutes); it now aims at the boss
  only within 1,000 wu (a shot flies 1,050) and comes back within 750 wu of it during the fight.
- `--sim crowd [--seeds N] [--minutes M] [--policy reckless] [--god] [--trace]`: the acceptance table.

### Numbers (M3 stage C)

The crowd, mean alive within 1,500 wu of the ship during Groove (20 seeds x 30 min a bot; the god-mode run,
20 seeds x 24 min with hits costing nothing, fills the cells the careful bot does not reach):

| Track, cycle | Target | Careful | Ratio | Careless | God mode | Drop peak / target (careful; god) |
|---|---|---|---|---|---|---|
| 1, 1 | 20 | 17.5 | 0.88 | 17.4 | 17.4 | 1.83; 1.76 |
| 1, 2 | 45 | 40.3 | 0.89 | 39.7 | 40.5 | 1.71; 1.69 |
| 1, 3 | 80 | 71.2 | 0.89 | 69.9 | 71.2 | 1.75; 1.65 |
| 1, 4 (boss) | 120 | 106.0 | 0.88 | 103.3 | 106.8 | boss Drop |
| 2, 1 | 150 | 135.3 | 0.90 | - | 136.3 | 1.65; 1.62 |
| 2, 2 | 190 | 171.9 | 0.90 | - | 171.7 | 1.63; 1.60 |
| 2, 3 (boss) | 230 | 206.2 | 0.90 | - | 208.1 | boss Drop |
| 3, 1 | 240 | 214.9 | 0.90 | - | 218.2 | 1.61 (1 run); 1.61 |
| 3, 2 | 270 | 241.4 | 0.89 | - | 244.1 | -; 1.59 |
| 3, 3 (boss) | 300 | - | - | - | 272.4 (0.91) | boss Drop |
| 4 (loop), 1-4 | 320 | - | - | - | 295.7 / 292.2 / 285.0 / 277.9 | 1.56 / 1.58 / 1.53 |

Every cell within -14% of its target (acceptance: +-25%); every Drop peak 1.5-1.9x (acceptance: 1.4x).
Difficulty: careful bot, median first death **7.09 min** (4.34-9.97), Track 2 reached in **20 of 20** runs,
Track 3 in 3, 3.8 deaths a run; careless bot, median first death **2.09 min** (1.23-3.38), 12.6 deaths a
run, never past Track 1's Kaleidos. Kills a minute: careful 425, careless 341, god mode 965 (higher Tracks).
No pool overflow, no NaN, 0 heap allocations in the loops, no Shard dropped (the Shard pool never filled).
No fodder number needed tuning: speeds, flood places and Shoal behaviour are unchanged; the targets are the
data's. Before the bot fix: careful 8.91 min median, 20 of 20 (the stalled boss fights hid deaths).
Files: `captures\m3c\crowd_careful.txt`, `crowd_careless.txt`, `crowd_god.txt`.

- Tests: audio 2134 checks (new: the pause's low-pass takes the brightness out and gives it back; at half
  rate the growl rings twice as long), sim 5235 (Reef in the director checks; the crowd: targets, scales, a
  world that fills the crowd near an idle ship, threats still bought, stray fodder dissolving without
  Shards), 0 failures; determinism PASS for both bots.
- Desktop Debug (validation on), bench_play 60 s at minute 10 (fast-forward 600 s; Track 2, cycle 3): 0
  validation messages, 0 heap allocations (frame, pacer, other threads, audio callback); alive 209 / 400,
  enemies in view 60 / 197, enemy bullets in view 13 / 45 (mean / max); main CPU 5.9 / 6.1 ms (P50 / P95).
  `captures\m3c\desktop_debug_bench_play_min10_60s.log`.
- Light at minute 10 of the real game (desktop, 2400x1080, 32 frames at 10:50, Track 2's Build, 378 alive,
  39 in view): between kicks (frame 26) dark<48 83.5%, dark<96 97.2%, >=200 0.5%, luminance 0.0213; the
  frame after the kick 0.0242 (reference: dark<48 at least 15%, luminance at most 0.16).
  `captures\m3c\light_min10b\`.
- The M1 stills against the reference after the soft-light change (the reference, `design/tools/lookdev.py`
  `_add_glow`, still cuts at the square, so the fade shows as a difference): `look_card` without bloom mean
  abs error 0.70 (was 0.34), soft light p10 / p50 / p90 0.82 / 0.99 / 1.00 (0.99 / 1.00 / 1.01); with bloom
  1.61 (1.22), 0.85 / 0.97 / 1.00 (0.93 / 0.98 / 1.00); `frame_reef` 1.33 (1.16), luminance ratio 0.978
  (0.982), 0.94 / 0.98 / 0.99. `captures\m3c\m1_stills\`.
- **Moto, the real game at minute 10** (bench_play, the bot from the start, 600 s of warm-up then 120 s
  measured; tier medium from the probe's cache, governor off, no cuts; shared phone, foreign load 0.82 cores;
  `captures\bench\ZY22BXMJG3-bench_play-20261112-073514.json`): panel P95 1 period (16.667 ms), 0 of 7,188
  frames missed, none over 50 ms (max 19.8 ms CPU side): **PASS** (the present call's own P95 17.7 ms is
  reported, not graded). Main thread CPU 4.40 / 4.61 / 5.31 ms (P50 / P95 / max); GPU 13.07 / 13.48 / 13.88
  ms; sim tick 0.145 / 0.230 / 0.592 ms; audio underruns 0, worst callback 1.19 ms; heap allocations ours 0.
  On screen: enemies in view **58 / 157** (mean / max), enemy bullets in view **12 / 113**; alive 221 / 381,
  enemy bullets 27 / 202, Shards 95 / 361; 6,389 kills, one death (the run restarted; it ended in Track 2,
  cycle 2, a Drop). Stage B's run at minute 10 had 23 / 158 in view and 49 / 175 alive.
- Moto in-app capture at 10:10 (`captures\m3c\moto_inapp_min10.png`, Track 1's Drop after the restart):
  dark<48 63.6%, luminance 0.0164. The Moto's vibrator has no amplitude control (logged), so there SOFT and
  STRONG differ by length.

### Numbers chosen in stage C (`tuning.h` crowd block, `bot.cpp`, `play_view*.cpp`, `play_hud.cpp`, `band.cpp`, `RaveformActivity.java`)

Crowd formations: flood share 0.5, ring share 0.25, flood 900 wu out with clusters 220 wu apart, ring 700
wu, corners +-760 / +-330 wu. Bot: boss aim within 1,000 wu, back within 750 wu. Soft light fade: from 0.8
of the cut-off radius. Death flash: 0.9 white, 60 -> 420 wu, 0.12 s; accent 0.6, 120 -> 520 wu, 0.45 s.
Chevrons: 0.4 of the height deep (the boss's pair 0.3, 0.32 apart), disc 0.68 of the height, stroke half
width 0.055 of it with the threat layer's 1.5 px rim. Boss camera lean eases over 0.5 s (the zoom keeps its
2 s). HUD Boost row 220 px apart, words cap 11 px for two bars. Track card: a beat to write, out on the
third bar's downbeat, a beat to streak; the name's top at 0.62 of the height. Haptics with amplitude
control: 12 / 18 / 40 ms at SOFT 60 / 90 / 130 and STRONG 170 / 220 / 255; without it SOFT 8 / 12 / 30 ms,
STRONG 20 / 30 / 70 ms. Low-pass from 18 kHz. Pictures: `hud_drop` at Track 1's third Drop (121.1 s).

### Decisions taken alone (stage C)

1. The crowd's refill adds the fodder killed since the last bar (feed-forward), and the Break's last beat
   lands the next Groove's target, so the Groove does not start empty.
2. The hit-stop holds the simulation, not the song: the held ticks run afterwards, three a frame.
3. Haptics use the vibrator (amplitude or length), so the in-game OFF / SOFT / STRONG governs them rather
   than the system's touch-feedback switch; without a vibrator they fall back to performHapticFeedback.
4. The bot frames (`bot_*`) and `hud_drop` run with hits costing nothing, so minute 15 is Track 3.
5. Boost effect words show in the HUD row for two bars after the pickup (screens.md's table says "HUD").

### Pictures (`captures\m3c\`, 2400x1080, fixed 1/60 s steps, `*.small.png`)

bot_3, bot_6 (Track 2's first Break, three bars before its end: the crowd's Break target is 0, so the
field is empty), bot_10, bot_15 (Track 3, Reef, Kaleidos at 2x), hud_drop
(Track 1's third Drop, 88 in view), strip_boss_death (eight frames: the hold, the flash, the tumble),
edge (Kaleidos's double chevron, a Boost's and a shooter's), boost_prism (the fan in flight), track_change
(Track 2's name card), moto_inapp_min10 (the Moto, in-app, minute 10). The bot frames and hud_drop from the
dev build, the rest Debug with 0 validation messages.

### Not done

- Kaleidos is still every Track's boss (Reef's own boss is M5); the game-over medley waits for M6.
- No elites in the slice, so no white chevrons yet.
- Prism's mini shots are the player's colour, not a rainbow (unchanged; not in this brief).
- The design's reference renderer still cuts soft lights at the square (above); the compare numbers moved.
- In view the crowd is a fraction of its near count: the view at 1x covers about a fifth of the 1,500 wu
  circle and the careful bot keeps its distance (minute 10: 378 alive, 9-39 in view).
- With the crowd the Chain (+1 a Shard, halved only by hits) reaches about 3,000 by minute 10 and 12,000
  by minute 15 in a run without hits, scores in the hundreds of millions to billions: a design question.

## Contract change

- 2026-11-10: at the user's request the frame-rate contract moved from the 99th to the 95th percentile:
  P95 present-to-present interval within the device's limit (16.9 ms at 60 Hz, 11.3 at 90, 8.6 at 120),
  at most 5% of frames missing vsync, no frame over 50 ms (PLAN §5). P99, the missed-vsync percentage
  and the maximum are still recorded.
- 2026-11-10: the contract is judged on the panel side (when each frame reached the display, counted in
  whole refresh periods); the present-call interval is reported, not graded (PLAN §5).
- 2026-11-11 (M2): benchmarks play the band's fullest arrangement by default, so the contract is measured
  with sound on; `--no-audio` gives the silent comparison (M0's silent stream, as M1 measured).

## M3 Vertical slice, stage B "the rest of the slice" — status 2026-11-12 (night)

Stage B is built on `main` (commits `7e9b97b`, `9ac3547`, `be3c086`, `7fe32f5`, `cea10b3`, `d7bcbc0` and the docs commits): the director,
the Surge's two gears, upgrades with the Trio, Kaleidos, the full HUD, the screens with saved settings and
high scores, the start-up cache, the bot for all of it, tests and the review pictures. The user asked the
lead to continue without check-ins; the decisions taken alone are listed below.

### How it is built (M3 stage B)

- **Director** (`src/sim/director.h/.cpp`, `world_spawner.cpp`): `phraseAt(state, bar)` places every run
  bar in the cycle (Track 1: four Short cycles; its fourth Drop is Kaleidos's and is held while it lives;
  the boss's death schedules the eight-bar Track-change Break on the first bar line at least half a bar
  away; Track 2: three Standard cycles; then the loop, +4 BPM a Track up to 174). Per bar it spends
  `base(track, cycle) x section scale x assist` on formations (cluster, ring, pincer, sweep, corners) from
  the debuted roster; the Drop's mega-wave warps in over the Build's last two beats; debuts get a
  spotlight group over the Break's last two beats with their name on screen; the Break's first bar
  dissolves fodder into Shards; a Boost is due every 20-30 s and drops from the next kill; the boss's Drop
  spends 30% on fodder only. The phrase gives the sim its Surge (score and magnet x2, the fire grid one
  then two steps faster) and the Build's last bar at 85% (world time and the simulated clock).
- **The band follows the director** (`Play::conductBand`): in the second half of every heard bar the band
  is told the next bar (`SetPhrase`: section, bar in it, length), a new run (`SetTrack` at once, Track 1 at
  its tempo), the Track change (`SetTrack` through the Break, with the loop's tempo), the boss's bar
  (`SetBoss`: the firing pairs' P3 bells on their steps, the iris's gong, a beam's riser and stab) and the
  local player's loadout (lead harmony, grid with Double-Time / Surge gear / Hyper, Riff layers). A run
  and RETRY start on the first bar line at least half a bar away, so the band starts them on the same line.
  Pause freezes the run and holds the band; RESUME counts four beats in and goes on where the run paused
  within its bar (the run's ticks = the band's minus a whole number of bars).
- **Upgrades** (`src/sim/upgrades.*`, `world_upgrades.cpp`): eight Mods, three Riffs, four Boosts; the
  Trio's draw (weights, the weapon guarantee, the Riff guarantee), a bonus Trio after the boss, the ring as
  clock, a random orb homing at the end. Effect words are in `upgradeWords()`.
- **Kaleidos** (`src/sim/boss.*`, `world_boss.cpp`; data parsed from the embedded kaleidos.json): pairs,
  rest rule, lens plates, iris, four phases, fans, core rings, beams with their telegraph bar, drift,
  contact, Nova damage, pair death (one-beat flinch, the wheel reverses), death (Shards, the Track ends).
- **View and HUD** (`play_view_b.cpp`, `play_hud.cpp`, `content/icons.*`): section looks (Build drain and
  zoom-in, last bar darker; Drop shockwave, bloom and zoom snap; Break cool), the Trio as screens.md draws
  it with icons from icons.json, Riffs, Boosts, Kaleidos (arms, glass, white-hot resting pair, reflections,
  lens, core, iris, gold health arc, beams, entrance, pair death, death), HUD per look.md 6, edge chevrons
  (danger for shooters, gold for Boosts, white for the boss), the palette following the Track with a
  cross-fade in the Track-change Break.
- **Screens** (`game/screens.*`, `game/settings.*`): first-launch notice, title over the attract run (live
  waveform from a master-bus scope in the band), pause with count-in, game over with the set list,
  settings (SOUND cards with live signature lines, faders over a meter, SYNC opening the M2 calibration,
  haptics; CONTROLS: swap sides, auto-aim; LOOK: effects tier, frame-rate target, reduce flashing).
  `settings.txt` and `highscores.txt` in the app's private storage, written to a temporary file and renamed.
- **Start-up**: the baked one-shots are cached (`baked_audio.bin`, 30 MB, keyed by the music hash and the
  rate); the first-launch tier probe runs behind the notice / title (PLAY stops it a tier below the one
  being measured).

### How to run

| What | Command |
|---|---|
| The game with its screens | `scripts\run-desktop.ps1` (Android: the app's icon) |
| The screens with the bot playing every run | `--autoplay` |
| Review pictures (`captures\m3b\`) | `scripts\review-m3b.ps1 [-Only trio,boss_beam]` |
| Bot 30 minutes headless (minute table) | `build\desktop-dev\raveform_headless.exe --sim soak --minutes 30 [--seed N] [--policy reckless]` |
| The fight at minute 10, measured | `--scenario bench_play --fast-forward 600 --seconds 60` (the run realigns to the band's bars) |

### Numbers (M3 stage B; the phones are shared with other sessions)

- Tests: audio 2122 checks, sim 5219 checks (new: director cycle, Trio rules, upgrade stacking, Kaleidos
  damage and phases), 0 failures; determinism PASS for both bots over 2 min (`test.ps1`), 12 and 30 min.
- Bot, 30 simulated minutes headless (careful bot, seeds 1, 7 and 3; reckless bot, seed 1): no crash, no
  overflow, no NaN, 0 heap allocations in the loop; tick P95 0.010 / 0.063 / 0.006 ms, max 0.059 / 0.185 /
  0.141 ms. Seed 1: 5,452 kills, 2 deaths (both at Track 2's Kaleidos, 10:19 and 19:26), Tracks reached
  2; Kaleidos of Track 1 falls at 5:43 (entered 2:49), 14:35 and 24:22. Seed 7: 4,775 kills, 2 deaths (both
  at Track 2's Kaleidos), Track 1's boss 1:41 the first time but over 7 minutes the third time (fodder piles
  up to 226 alive meanwhile). Seed 3: 7,598 kills, 1 death, both Kaleidos fights won (4:28, 11:56), Track 3 reached (up to 611 kills a minute, 245 enemies alive). Reckless: 15 deaths, the longest life 196 s, no boss beaten. **First reading
  of the difficulty:** over 22 seeds of 30 minutes the careful bot dies 1-4 times (longest lives 9-21 minutes,
  most 13-19; it beats Track 1's Kaleidos in 1.5-3 minutes, Track 2's in some runs and dies to it in others,
  and reaches the loop in about a third of them); the careless bot (10 seeds) dies 11-15 times, never living past
  4 minutes: Track 1's Kaleidos is its wall. 60-minute soaks (seeds 11-13): 3-4 deaths, no overflow. Per-minute tables:
  `captures\m3b\bot_30min_*.txt`.
- Desktop Debug (validation on), bench_play: 60 s of the boss fight (fast-forward 172 s): 0 validation
  messages, 0 heap allocations, 0 audio-callback allocations, 16 / 37 enemies (mean / max), 198 / 320
  bullets. 60 s at minute 10 (fast-forward 600 s; the bot had cleared Track 2 and was in Track 3, the loop,
  Drop): 0 validation, 0 allocations, 21 / 81 enemies.
- Start-up on the Moto (shared phone): first launch after a data clear, activity start to the first frame
  of the game (the notice over the tier probe) 3.4 s (baking 2.6 s of it; the probe then chose medium in
  5.3 s behind the notice); later launches 0.67 s (the one-shots from the cache in 20 ms).
- Look v2.3 (`afterglow.knee` 0.9, read from look.json): `captures\m1` re-shot with `review-m1.ps1`;
  `scenes.py light` (dark<48, dark<96, >=200, luminance): bench_mid frame 600 38.6% / 78.7% / 3.0% /
  0.0752; bench_endgame frame 600 15.8% / 48.4% / 11.8% / 0.1590; the quiet strip (frame 407, eight frames)
  16.6-17.4% / 49.6-50.6% / 10.1-10.7% / 0.1493-0.1531.
- **Moto, the real game at minute 10** (bench_play, the bot playing from the start, 600 s of warm-up then 120 s
  measured; the tier the first-launch probe chose: medium; shared phone, foreign load 0.81 cores; result
  `captures\bench\ZY22BXMJG3-bench_play-20261111-234125.json`): panel P95 16.667 ms (1 period), 0 of 7,188 frames
  missed, none over 50 ms (max 20.0 ms CPU side): **PASS**. Sim tick 0.077 / 0.130 / 0.298 ms (P50 / P95 / max);
  main thread CPU 4.32 / 4.53 / 6.25 ms; GPU 12.77 / 13.15 / 14.08 ms; audio underruns 0, worst callback 1.19 ms;
  heap allocations ours 0 (main thread and audio callback). On screen: the bot had beaten both Kaleidos fights and
  was in Track 3 (First Light at 130 BPM), cycle 4, the boss's Drop, with 12 upgrades and no death; enemies alive
  49 / 175 (mean / max), in view 23 / 158, enemy bullets 33 / 264, Shards 96 / 310, 3,830 kills.
- Regression, Moto bench_endgame 30 s (`--tier low`, as stage A): 0 of 1,797 missed, main 4.64 / 4.86 ms (stage A
  4.63 / 4.87), GPU 13.45 / 13.79 ms (13.50 / 13.81): unchanged. `test.ps1` passes; the M1 stills compare as at
  the v2.3 re-shoot (soft light mean abs 0.00020 / 0.00169 / 0.00138).

### Effect words (two to four words, mine; superseded in stage C by screens.md's table)

Split `+1 SHOT LANE` (screens.md), Double-Time `TRIPLET FIRE GRID` then `THIRTY-SECOND FIRE GRID`, Pierce
`+1 ENEMY PIERCED`, Ricochet `+1 WALL BOUNCE`, Overcharge `+25% SHOT DAMAGE`, Extra Cell `+1 SHIELD CELL`,
Magnet `WIDER SHARD PULL` (the Trio picture), On-Beat Crit `BEAT SHOTS X2` / `X3` / `X4`; Orbitals
`NEW RIFF: TWO ORBITERS` (the Trio picture) then `LEVEL n: THREE / FOUR ... ORBITERS`; Pulse Ring
`NEW RIFF: BEAT RINGS`, `LEVEL 2: EVERY BEAT`, `LEVEL 3: SECOND RING` (screens.md), `LEVEL 4: WIDER RINGS`,
`LEVEL 5: HEAVIER RINGS`; Sentinel `NEW RIFF: OFF-BEAT DRONE` then `LEVEL n: BURST OF n`. Boosts (HUD):
Prism `FIVE-WAY RAINBOW FAN`, Hyper `DOUBLE-SPEED GRID`, Halo `BLOCKS THREE HITS`, Magnet Storm `EVERY SHARD
COMES`.

### Numbers chosen (`src/sim/tuning.h` stage B block, `play_view_b.cpp`, `game/screens.cpp`)

Director: mega-wave 3 bars of Drop budget; the loop's budget +2 per cycle past species.json's last (12);
no new bodies while 600 enemies live; spotlight group 560 wu from the ship (3, Shoal its minimum 12, Ram
and Carom 2, Sentry 1). Trio: touched when the ship's hit circle is 6 wu inside the orb; the timed-out orb
flies at 900 wu/s; draw weights Mod 1.0, new Riff 1.2, Riff level 1.0. Mods: Split lanes 5 degrees apart,
Overcharge +15% shot radius a stack, at most 15 shots a step. Riffs: Orbitals at 64 wu, 10 wu hit, 1 damage
an eighth; Pulse Ring 230 wu (290 from level 4) in 0.25 s, 2 damage (x1.5 at level 5), second ring 0.10 s
later; Sentinel range 700 wu, station 34 left / 30 up of the ship, follow 0.12 s, bursts 4 degrees apart.
Boosts: pickup 26 wu, uncollected 8 bars; Prism lanes 9 degrees, mini shots +-35 degrees for 0.22 s at
half damage; Halo 0.6 s invulnerable per absorbed hit. Kaleidos: pairs 500 and core 2,000 points
(x Chain); a pair drops 20 Shards; a Nova deals 60 to each pair and to the core once open; the arms' hit
shape from 78 to 274 wu; the loop's Kaleidos +6% health a Track after Track 2. View: Build drain 0.45 and
zoom-in 10%, last bar exposure 0.78; Drop zoom snap 8%; Break drain 0.22; Boost orb 20 wu. Screens: the
title waveform 750 scope points (128 frames each), the pause music dip 0.45 (no low-pass yet).

### Decisions taken alone

1. Tracks after 2 loop First Light and Solar Wind (Track 3 = First Light at 130 BPM, ...); the key is not
   transposed and the palette not recombined (each song keeps its own); Kaleidos is every Track's boss.
2. The Track-change Break after a boss holds two Trios: the boss's bonus Trio first (half the Break as its
   clock), then the Break's own.
3. Assist: a lost cell lowers it 10% at once (once a cycle); two clean cycles raise it 10%; 0.8-1.3.
4. A held boss Drop keeps its Surge for the first half of its nominal length only.
5. Kaleidos's entrance bar: it neither fires nor takes damage; the rest rotation counts from the bar after.
   In Refraction the firing pair fans from beat 2 (beat 1 is its beam's).
6. Boss damage reaches the core only through an open sector (a dead pair's plates) or the 8-degree gaps
   between plates, judged where the shot crosses the lens ring.
7. A pause can come mid-bar; the run resumes where it paused within its bar after a four-beat count-in,
   so its bars stay on the band's (screens.md's "starts on the bar line after" holds when paused on one).
8. Game over holds a Break in the band; RETRY / PLAY start Track 1 at once on the next safe bar line.
9. The first-launch notice offers REDUCE FLASHING and CONTINUE; my wording.
10. Fusions wait for M5: the Trio keeps a fourth orb slot.
11. The frame-rate target setting applies from the next start.
12. Debut names on screen are family and tier (`HOUND II  B`).

### Contradictions and gaps found

- species.json `firstSlice` has no elite rules, so the slice spawns no elites.
- GAME_DESIGN 8's Drop "2.0 then decays to 1.2" has no curve: linear across the Drop's bars.
- kaleidos.md gives no score for the boss and no Nova damage number; chosen (above).
- screens.md's pause count-in "on the bar line after" cannot hold for a mid-bar pause without moving the
  run's bars off the band's (decision 7).

### Pictures for the lead (`captures\m3b\`, 2400x1080, desktop Debug, fixed 1/60 s steps; `*.small.png`)

screen_notice, screen_title, screen_settings_sound / _controls / _look, screen_pause (mid count-in),
screen_game_over; trio, trio_answer, strip_trio_take (8 frames); hud_groove, hud_build, hud_drop (Surge),
hud_break; riff_orbitals, riff_pulse, riff_sentinel; boost_prism, boost_hyper, boost_halo,
boost_magnet_storm; edge (danger chevron for a shooter off screen, gold for a Boost); boss_bloom,
boss_refraction, boss_spiral, boss_naked, boss_beam_tell, boss_beam, boss_iris, strip_boss_pair_death,
strip_boss_death; track_change (Track 1 to 2, bar 5 of the Break); bot_3, bot_6, bot_10. From the Moto:
moto_title (the live title, adb screencap), moto_trio, moto_boss_bloom, moto_screen_game_over (the app's own
capture), moto_pause, moto_countin and moto_game_over_loop (screencaps during the loop run).

### Stage B changes in shared files

`audio/engine.*` (SetPhrase, SetBoss with its bells / gong / riser / stab, SetTrack at once or with a tempo,
the Surge's two gears from conductor.json), `audio/band.*` (the director's API, the master-bus scope and
meter, the baked-sound cache), `audio/music_data.*` (`drop.surgeSteps`), `audio/bake.*` (cache save and
load), `platform/app.*` (the probe behind the title, SYNC from settings and back, the settings' tier and
frame rate, the first-frame log, the run's place in the bench result), `core/args.*` (`--autoplay`),
`game/audio_scene.h` (`clearQuit`), `CMakeLists.txt` (kaleidos.json and icons.json embedded, new sources).

### Not done, not verified

- The game-over set list's ten-second medley; the pause's low-pass (a music dip stands in); the growl an
  octave down (the elite patch at its own pitch); haptic strength (SOFT and STRONG are both "on"); the 50 ms
  hit-stop at Kaleidos's death; CO-OP (M4).
- Not tried by a person: touch feel of the Trio and the screens, readability of the boss fight, the
  difficulty curve (the bot's reading above), SYNC from settings and back on a phone, swap sides.
- **The full loop on the Moto** (`--autoplay --reckless-after 600`, scripted adb taps; log in
  `captures\m3b\moto_loop_log.txt`): title, PLAY tapped, Track 1's Trios at bars 20, 44 and 68, a pause by Back
  and RESUME with its count-in a minute in, Kaleidos on bar 84 (falls at 283 s of the run), the bonus Trio and
  the Break's own, Track 2 (Standard cycles at 126 BPM, Trios at 190 and 238), its Kaleidos on bar 270 (falls
  after 43 s), the Track change into Track 3 (First Light at 130 BPM), the bot turned careless at 600 s and lost
  the ship in Track 3 at bar 332 (score 44,276,250), the game-over screen, RETRY tapped: the band went back to
  Track 1 at 120 BPM on the next bar line and a new run began. The band followed every section on its bar line.
- What a person must try: flying into Trio orbs with the sticks; the screens' buttons by touch; reading the
  boss (the resting pair, the plates, the beams' telegraph) at phone size; whether Build, Drop and Break land
  musically on headphones; a mid-bar pause and its count-in; SYNC from settings and back; swap sides.

## M3 Vertical slice, stage A "the fight" — status 2026-11-11 (late night)

Stage A is built on `main`: the simulation, the fight on the band's heard clock with sticks, gamepad,
keyboard and mouse, the bot, the simple spawner, a minimal HUD and the stress scenario. Stage B (the
director with the phrase cycle, the Trio, the HUD beyond the essentials, title / game over / pause, the
boss, attract mode, the Surge's two gears) is not started. Device numbers come from the **shared** Moto One
5G Ace (`ZY22BXMJG3`), taken under the phone lock, deployed right before each run.

| Done criterion (stage A brief) | Status | Evidence |
|---|---|---|
| 1. A run can be played from spawn to death and retried, with sound, on touch (Moto) and keyboard + mouse or gamepad (desktop) | **Built**; played by the bot on both (real time, the band's clock), touch driven with `adb input swipe` on the Moto. Feel, sound and the gamepad need the user | Moto log: the sim's musical clock within 1-2.3 ticks of the band's heard tick, every lead-rule shot from the band (62 / 62, 142 / 142); `captures/m3/moto_inapp_touch.png` |
| 2. Determinism test (headless, `scripts\test.ps1`); unit tests for the grid, the pools and the Chain rules | **Met** | `raveform_headless --sim determinism`: two live runs and a replay of the recorded inputs agree at every 10 s checkpoint, with the careful bot (no deaths) and the reckless one (deaths and retries); another seed differs. `raveform_sim_tests`: 247 checks, 0 failures |
| 3. Bot: 10 simulated minutes headless, no crash, assertion, pool overflow or NaN | **Met** | Below |
| 4. Stress through the real simulation on the Moto at Low (60 s after 60 s), plus `bench_endgame` 30 s | **Met**: panel 0 of 3,594 missed, sim tick P95 1.96 ms, main thread P95 5.63 ms, 0 allocations, 0 underruns; `bench_endgame` unchanged | `captures/bench/ZY22BXMJG3-bench_stress-20261111-220734.json`, `…-bench_endgame-20261111-221012.json` |
| 5. Desktop Debug, 60 s of bot play and 60 s of stress: 0 validation messages, 0 allocations in the frame, the tick and the callback | **Met** | `captures/desktop_m3_bench_play_debug_60s_20261111-2204.log`, `…_bench_stress_…` |
| 6. Acceptance stills unchanged | **Met**: `frame_reef`, `look_card`, `look_card_nobloom` compare output identical to `captures/m1/*.compare.txt`; the whole M1 review set re-shot, every `scenes.py light` line identical to `captures/m1/light.txt` | `captures/m3/regress/`, `captures/m3/review_m1/` |
| 7. Pictures for the lead | **Done** | List below |
| 8. PROGRESS.md and CLAUDE.md | **Done** | This section; CLAUDE.md commands and arguments |

### How it is built (M3 stage A)

| Piece | Where | What |
|---|---|---|
| Species | `src/sim/species.*` | `design/species.json` embedded and parsed (CMake, like the other design data): family base x tier x variant for all 6 x 5 x 3 = 90 species of the six first-slice families (health, speed, radius, score, threat, Shards, hue role, motion keys), `enemyBullet`, the first-slice roster with debuts and the Groove budgets |
| World | `src/sim/world.*`, `world_enemies.cpp` | The fixed 60 Hz tick (TECH_SPEC 7.2: players, shots, Nova, spawner, grid, enemies, projectiles, collisions, deaths, swap-remove compaction) over struct-of-arrays pools at TECH_SPEC 7.1's caps (enemies 3,072 with generation-checked handles, enemy bullets 2,048, shots 1,024, Shards 2,048), the 64 wu counting-sort grid (150 x 85 cells), an event buffer (kills, impacts, shots, warp-ins, telegraphs, pickups, hits, deaths, Nova, Dash, restart). Musical time is an input of each tick (its heard start and end in 96 PPQ ticks): every beat crossed resolves on the beat, advanced by the remainder. No wall clock, own random streams, no allocation after init, `hash()` and `finite()` |
| Enemies | `world_enemies.cpp` | Drifter (wander, a new turn rate each beat, bounces off walls), Hound (steers at the nearest player; B: a glint on beat 4, a three-bullet fan on beat 1), Shoal (boids in schools seeking the player), Ram (telegraph one beat with chevrons, dash one beat, rest two), Carom (straight lines, faster per bounce off the walls), Sentry (stationary; B: a glint of one beat, then three rotating arms every two sixteenths). Ranged enemies only attack inside a rectangle around a player and get an edge indicator when that is off screen. Warp-in for exactly two beats (R4) |
| Player | `src/sim/player_controller.*` | The local player is an input (TECH_SPEC 4): integrated every frame with the real frame time; 520 wu/s with the glide, walls, Dash (220 wu in 0.12 s, invulnerable, four beats to recharge, Perfect within 60 ms of a beat refunds half), Nova press, fire modes: aim stick on the full grid, 400 ms of fire after the thumb leaves it, auto-aim assist on the half-rate grid at the nearest enemy (with a lead and a sticky target). The world applies hits (cell lost, panic pulse 220 wu, 1.5 s invulnerability, Chain halved, time dip 85% for 200 ms), death, the Nova (invulnerable from the press, blast on the beat chosen from heard time plus the latency so it lands with the band's blast, the front clearing bullets and enemies in view as it passes) |
| Shards, Chain, score | `world.cpp` | Drops per tier for each player, a burst then rest, the 90 wu magnet (homing), +1 Chain per pickup, halved by a hit and by nothing else, score = value x max(1, Chain), four-bar life |
| Spawner | `world_spawner.cpp` | Stage A's stand-in for the director: on bar lines spends `threatBudgetPerBar` (Groove x 1.0) on packs from the roster with debuts by elapsed bars (Track 1: 4 x 24 bars, Track 2: 3 x 48, then the last budget holds); an unaffordable pick waits for the bank. `Stress` mode holds 700 enemies (32 of them firing Sentries) around the ship |
| Bot | `src/sim/bot.*`, `sim_runner.*` | Potential field (bodies, bullets by time to closest approach, Ram paths, Shards, walls, a working distance), aim at the nearest enemy with a lead, Dash out of an imminent hit, Nova when surrounded; policies Play, Stress (a slow circle) and Reckless (no dodging, for tests). `SimRunner` steps it on the simulated clock |
| The game | `src/game/play.*`, `music_clock.*` | Scenarios `play` (default), `bench_play`, `bench_stress`; the accumulator (at most three ticks a frame, drawn interpolated); the musical clock: sim time advances by the band's heard time, each tick's musical position follows the heard tick (one tick ahead for the interpolation; it never runs back; jumps forward beyond a bar); without the band the simulated clock. The band bridge: fire mode, lead-rule shots, kills aggregated by the engine, one Shard note per 1/32 (a rising run), warp-in swells once per landing beat, telegraph tick and the shot's blip on its beat, Hit, Dash / Perfect Dash, Nova, Game Over; Groove held, intensity 0.15 + threat on screen / 16. Picture scripts (`--picture`) |
| View | `src/game/play_view.*` | The look v2.2 on the sim: camera with look-ahead and an aim lean, zoom 1.0 -> 0.85 with the load, the lattice inside the arena with kick ripples and impulses, walls in the accent, light budget, enemies with fields, marks, spin and hat steps, warp-in rings, kill recipes small / medium / large (sparks, flash, pool, rings, embers, shatter, distortion, shake), telegraphs (Ram chevrons, shooters' glint) and enemy bullets on the threat layer, the ship (two-tone, halo of one arc per cell, the last arc in the danger colour, priority layer, hit-point dot, trail, pool), shots by the streak rule, Shards with tails above 480 wu/s, Nova inhale (sparks in from 600 wu, the screen dims) and front (bloom pump from `look.json` `beat.novaPump`), Dash ghosts and the Perfect Dash ring and sparkle, cell shatter and panic ring, edge indicators |
| HUD, touch | `src/game/play_hud.*` | Score (white to the accent), Chain (white, gold, amber; pulses), Nova stars, Dash bar, TAP TO RETRY; floating sticks (10 mm, dead zone 8%, the origin follows past the radius), NOVA and DASH (9 mm at least, pressed: filled, brighter, larger; cooldown ring), all in physical sizes on Android, tone-mapped into the HUD batch with a few soft pools for glow |
| Band | `src/audio/band.*`, `engine.cpp` | Fire mode 2 (the assist's half-rate grid) in the engine's lead and in `computeShots`; the Nova blast on the game's beat (`Timing::AtTick`); the effect interface `kill`, `shard`, `nova`, `warp`, `sfxAt`, `holdSection`; the game mode (`BandDefaults::game`). No DSP change (calibration unchanged) |
| Headless, tests | `src/tools/sim_headless.cpp`, `tests/sim_tests.cpp` | `raveform_headless --sim determinism|soak|stress`; `raveform_sim_tests`; both in `scripts\test.ps1` (also an audio test for the half-rate lead and the Nova tick) |

### How to run

- Desktop: `scripts\run-desktop.ps1 -Config dev` starts the game (WASD, mouse aim with the left button,
  E / right button Nova, Space Dash; a gamepad works through SDL3's gamepad API). `-AppArgs '--bot'` lets
  the bot play. `--scenario touch_test` is M0's old default scene.
- Moto: `scripts\deploy-android.ps1 -Serial moto` installs and launches the game (no arguments needed).
- Measured: `scripts\bench-android.ps1 -Serial moto -Scenario bench_stress -Seconds 60 -Warmup 60 -AppArgs
  '--tier low' -Deploy`; `bench_play` likewise; `raveform_headless --sim soak --minutes 10`.

### Numbers

Headless (desktop, dev build, one core; Track 1's 120 BPM on the simulated clock; seed 1):

| Run | Kills | Deaths | Peak enemies / bullets / shots / Shards | Tick P50 / P95 / max (ms) | Overflow, NaN, allocations |
|---|---|---|---|---|---|
| `--sim soak --minutes 10` (the bot) | 1,950 | 0 (longest life 600 s) | 186 / 75 / 6 / 123 | 0.005 / 0.022 / 0.053 | 0, none, 0 |
| `--sim soak --minutes 10 --policy reckless` | 1,000 | 6 (6 retries) | 16 / 0 / 6 / 23 | 0.004 / 0.006 / 0.049 | 0, none, 0 |
| `--sim stress --seconds 120` | 590 | 0 (invulnerable) | 700 / 1,530 / 1 / 13 (mean 694 / 1,420) | 0.396 / 0.523 / 0.624 | 0, none, 0 |

The careful bot never died in ten minutes of the stage A spawner: Groove budgets alone (2.0-12.0 threat
per bar, no Build or Drop) are gentle; the director's ramps are stage B.

Moto One 5G Ace (shared phone), Swappy 60 Hz, Low, 2400x1080, overlay on, the game's band (Groove held,
Track 1):

| Run | Panel missed / P95 / max (CPU side) | Main thread P50 / P95 | Sim tick P50 / P95 / max | GPU (governor's clock) P50 / P95 | Alive (mean / max): enemies, bullets; in view | Allocations, underruns, callback load P95 | File |
|---|---|---|---|---|---|---|---|
| `bench_stress`, 60 s after 60 s | **0 of 3,594** / 1 period / 19.7 ms: PASS | 5.18 / 5.63 ms | **1.57 / 1.96 / 2.18 ms** | 12.06 / 12.30 ms | 699 / 700, 1,472 / 1,530; 649 enemies, 877 bullets in view | 0 ours, 0 underruns, 9.5% | `ZY22BXMJG3-bench_stress-20261111-220734.json` |
| `bench_endgame` (M1, unchanged), 30 s after 20 s | 0 of 1,797 / 1 period / 19.9 ms: PASS | 4.63 / 4.87 ms | – | 13.50 / 13.81 ms | 705 enemies, 997 bullets in view | 0, 0 | `ZY22BXMJG3-bench_endgame-20261111-221012.json` |

The `bench_endgame` check matches the M2 numbers (main thread 4.67 / 4.90, GPU 13.41 / 13.76 ms): nothing
regressed. Battery 25.7 -> 26.4 °C, thermal status 0. The stress run plays the game's own band (18
voices at peak), not M2's `--band full` load test. Swappy's own allocations inside its calls (4 a frame)
and on its threads (1 a frame) as in every run since M0; ours 0.

Desktop Debug with validation, 60 s each after 5 s: `bench_play` and `bench_stress` 0 validation messages,
0 heap allocations in the frame and the tick, 0 in the callback, no pool overflow, finite state (desktop
timing informational; the window lost focus once, so `valid` is false). Moto: 6 background / foreground
cycles of the game (`lifecycle-android.ps1 -Cycles 6`): PASS.

### Numbers chosen (no document gives them; `src/sim/tuning.h`, the constants block of `play_view.cpp`)

- Player: acceleration time constant 30 ms, glide 90 ms; shots live 0.75 s (1,050 wu), collide with a
  4 wu radius, leave 10 wu along the aim; auto-aim within 900 wu, a held target kept while within 1.25 x
  the nearest one's distance; the hull turns at most 18 rad/s; Nova invulnerability from the press to
  0.3 s after the blast; the panic pulse pushes enemies at 600 wu/s, decaying 6/s; contact uses 85% of an
  enemy's radius.
- Fairness: ranged enemies attack only inside +-900 x +-430 wu of a player; the Nova clears inside the
  widest view (+-941 x +-424 wu).
- Enemies: bullets live 4 s; drifters lean toward the player at 0.15 rad/s; hounds keep 26 wu apart; the
  Hound B fan is +-0.12 rad; Shoal weights cohesion 0.9, alignment 1.1, separation 2.6, seek 1.0, turn
  5 rad/s; a Ram engages within 650 wu; caroms leave toward the player +-0.35 rad; the Sentry's arms turn
  once in four bars.
- Shards: burst 60-160 wu/s, drag 4/s, homing 3,000 wu/s² up to 900 wu/s, collected within 20 wu.
- Spawner: the bank keeps at most three bars of budget; packs land 450-780 wu out horizontally and 0.45
  of that vertically; packs of up to 3 drifters, 3 hounds, 2 caroms, 1 Ram or Sentry, Shoals 12-40; the
  newest debut is picked twice as often; after Track 2's third cycle the budget holds.
- Stress: 700 enemies, 32 Sentries, a box of +-780 x +-340 wu, recycled beyond 1,300 wu, at most 160 new
  per beat, the ship invulnerable.
- Band: intensity 0.15 + threat in view / 16, followed over 2 s, posted on a change of 0.02; one Shard note
  per 1/32 (at most 8 waiting).
- View: look-ahead 0.25 s of velocity (capped at cruising speed) plus 60 wu along the aim while firing,
  eased over 0.25 s; zoom 1.0 -> 0.85 between a load of 60 and 300 (2 s); walls in the accent at 1.6, 2.6
  px, +60% on the snare, +30% width with the bass; shake 4 px on a hit, 3 on the Nova, 6 on death; muzzle
  flash 10 px at 0.9 for 60 ms; the inhale dims the exposure by up to 30% with 140 sparks; the front is
  6 px at 3.0; panic ring 0.25 s; Dash ghosts 0.25 s; indicators 46 px from the edge, 16 px chevrons.
- Controls: gamepad dead zone 0.2, the right stick fires beyond 0.3; the Chain's colours are P4's gold and
  P4's deep (amber); the touch layout scales out from the corners when physical sizes exceed 1080 ones.

### Contradictions and gaps found in the design documents

1. `species.json` says radius is "before the glyph's own size factor", but its values already are 14 wu x
   the glyph's size (shoal 8 ~ 7.7, ram 18 ~ 17.5, sentry 16 ~ 16.1); applying the size again would make a
   Shoal 4.4 wu. The radius is used as the drawn and collision radius, the size factor not applied twice.
2. GAME_DESIGN 5: "Score = enemy value x Chain" with a Chain that starts at 0: the multiplier is
   max(1, Chain).
3. look.md §5 warp-in examples ("a Shoal's is 30 px across, a Ram's 130") fit neither reading of the
   formula; the formula (2.5 x the glyph's extent) is what is drawn.
4. GAME_DESIGN 2 wants buttons of at least 9 mm; the target frame's Dash (60 ref px) is 7.6 mm on the Moto.
   9 mm wins on the phone (Dash 71 px, Nova scaled with it, the layout pushed out to keep the spacing).
5. "Hound B: 3-shot burst on beat 1": built as a three-bullet fan on the beat (as M1's stage drew it), so
   the attack resolves on the beat; a burst over sixteenths would not.
6. "Sentry B: rotating bullet pattern with a gap" with `patternArms: 3`: three rotating arms (the gaps are
   between them); a ring with one gap would be another pattern.
7. look.md §7 draws the HUD after tone mapping, never bloomed; the target frame's HUD is bloomed scene
   light. The HUD follows §7 (colours tone-mapped by hand) with soft pools behind the lit parts.
8. Hit-stop (GAME_DESIGN 11.3) or the time dip (look.md §5) for a lost cell: the dip (85% for 200 ms on
   the world, never on the player's controls).
9. Fast bright rings and the afterglow: at low load (swarm gain 1) the Nova front (about 100 px a frame)
   and the inhale ring leave soft copies in the afterglow, the effect v2.2 fixed for shots and Shards
   (`strip_nova_front_*`). Drawing them swept like shots filled the disc with light; they stay single
   rings as M1's Drop front. The lead's call.
10. No wall style in the look bible: the walls are the palette accent.
11. The game plays Track 1, so its palette is Ultraviolet (GAME_DESIGN 4.2), where M1 and M2 used Reef.

### Pictures for the lead (`captures/m3/`, 2400x1080, desktop Debug with validation, fixed 1/60 s steps)

Strips of eight consecutive frames `strip_<name>_0..7.png` with a contact sheet `strip_<name>_sheet.png`
and its 1600 px copy `.small.png` (scripted scenes, `--picture`): `fire` (a stream into a drifter),
`kill_small` (tier I), `kill_medium` (tier III), `kill_large` (Ram V: shatter, distortion), `shards`
(a pickup run with tails), `hit` (a cell lost: arc shatter, panic ring, Chain 24 -> 12), `nova` (press
to the blast) and `nova_front` (the front clearing), `dash`, `perfect_dash`, `warp` (the first frames of a
warp-in) and `warp_land` (the landing), `sentry` (the end of the glint and the first volley). Single
frames: `bot_0030s.png`, `bot_0120s.png`, `bot_0300s.png` (the bot at 30 s, 2 and 5 minutes),
`stress_0020s.png`; `moto_inapp_touch.png` (Moto in-app capture, the aim stick held by `adb input swipe`,
the touch controls drawn). Regenerate with `scripts\review-m3.ps1`.

### Not done, not verified

- Needs the user: feel, sound, the touch controls under real thumbs, a physical gamepad (none here), and
  whether the stage A difficulty is right (the bot survives ten minutes).
- Stage B, not started: the director and the phrase cycle (Build, Drop, Break; Surge in two gears), the
  Trio and upgrades, the phrase bar and the rest of the HUD, title / game over / pause, the boss,
  attract mode, the tier probe behind the title. `Spawner` and `Rules` (score and magnet multipliers) are
  the hooks the director replaces or sets.
- Not verified: edge indicators in a picture (none falls in the review scenes); the musical clock after a
  stream reopen (the lifecycle cycles pass, the clock's behaviour was not measured); the tablet and the
  12R (not attached).
- The game's band was measured, not M2's `--band full` load with the stress scene.

### M3 changes in shared files

| File | Change |
|---|---|
| `CMakeLists.txt` | `species.json` embedded; `raveform_sim` library; the game sources in `raveform_core`; `sim_headless.cpp` in `raveform_headless`; `raveform_sim_tests` |
| `src/platform/app.*` | `Content::Play` (the default), input routing (keys, mouse, gamepads, touch without gestures), picture capture by the script's moment, bench series and the `sim` object, overlay lines, palette by Track |
| `src/core/args.*` | `--bot`, `--seed`, `--fast-forward`, `--picture`; `--palette` recorded as given |
| `src/audio/band.*`, `engine.cpp` | Above (fire mode 2, Nova on a tick, effect interface, game mode) |
| `src/game/lattice.*` | Optional world bounds (the arena) |
| `src/content/look.*` | `beat.novaPump` read |
| `src/gfx/vector_font.*` | `drawField` (a colour field across the cap height) |
| `src/tools/audio_render.cpp` | `--sim` goes to `sim_headless.cpp` |

## M2 Audio engine and beat clock — status 2026-11-11 (night)

Built on branch `m2-audio` (worktree `Raveform-m2`, cut from `4ef7086`), accepted by the lead (the renders
re-measured with the lead's own script: loudness, six bands, third octaves, 50 ms envelope) and merged
into `main` (`14cc3cf`; the one conflict, `src/platform/app.cpp`, kept both sides: M1's tier probe and
benchmark start, M2's band update and audio scenes). Since the merge: the stage runs on the band's
heard clock, benchmarks play the full band, Track 3 is embedded and calibrated, the Surge listening file
is rendered and the sync screen follows reduce-flashing mode. Device numbers come from the **shared**
Moto One 5G Ace (`ZY22BXMJG3`), taken under the phone lock, at the media volume already set.

| Done criterion (brief / PLAN §7 M2) | Status | Evidence |
|---|---|---|
| Offline match with the prototype, Tracks 1–3, three variants | **Met** | Table below; `captures/m2/match_table.md`, `captures/m2/eng/*.match.json` |
| Every onset within one sample of its grid position | **Met** (0 samples, 1226–1329 onsets per render) | `scripts/audio_check.py onsets`; unit tests (every Track staged, the conductor through the Build's 85% bar) |
| True peak ≤ −1 dBFS, loudness −14 ± 2 LUFS | **Met** (TP −5.0 to −5.9; −14.00) | Same |
| Moto: callback within 25% of the burst period at P95, every layer playing | **Met**: 22.0% over 10 minutes (`bench_idle --band full`); contract run below | `captures/bench/ZY22BXMJG3-bench_idle-20261111-180929.json` |
| Moto: zero underruns over 10 minutes | **Met**: 0 underruns, 133,130 callbacks, every layer playing | Same file |
| Moto: the frame-rate contract with the band playing | **Met** at Low (`bench_endgame`, full band on Track 3, 10-minute warm-up then 120 s): panel 0 of 7,188 frames missed, worst frame 20.3 ms (CPU side), main thread P50 / P95 4.70 / 4.92 ms, 0 underruns, callback load P95 23.4% | `captures/bench/ZY22BXMJG3-bench_endgame-20261111-191123.json` |
| Calibration screen works on desktop and Moto, flash drawn from heard time with the edited offset | **Built**; "flash and click coincide" needs the user | `captures/m2/desktop_sync.small.png`, `moto_sync.small.png` |
| Desktop Debug: 0 validation messages; 0 heap allocations in the frame and in the callback | **Met** with the band: `bench_endgame` 60 s (full band, Track 3): validation 0, `heap_allocs` 0, pacer 0, other threads 0, `callback_heap_allocs` 0 | `run-desktop.ps1 -AppArgs '--scenario bench_endgame --seconds 60'` |
| Unit tests for tempo map, sequencer timing, queues, run by a script | **Met** (12 tests, 2118 checks) | `scripts\test.ps1` |
| Renders for the user | **Done** | `captures/m2/*.mp3` (list at the end) |

### How it is built (M2)

| Piece | Where | What |
|---|---|---|
| Music data | `src/audio/music_data.*` | Embeds and parses `design/music/{patches,variants,conductor,track01,track02,track03}.json` (CMake, like the other design data). Resolves every Track × variant in the prototype's order: patches.json, then the variant, then the Track's `mix`, `send` and `patch` overrides. conductor.json: structured numbers read directly; the numbers it states in sentences ("Level rises 0.35 -> 1.0", "85% speed", "steps 0, 6 and 12", ...) are extracted with a check of how many there are, so an edit that changes a sentence's shape fails at start-up instead of being misread |
| One-shots | `src/audio/bake.*`, `fft.*` | Drum kit per Track × variant, clap, snare, shaker, pulse, impact, risers, swells and every sound effect of `sfxproto.py`, synthesised at load with the prototype's recipes: 139 one-shots, 30.1 MB, 0.96 s on the desktop (Tracks 1–2 only: 109, 19.8 MB, 0.66 s). Pitched effects are baked on the chord tones the Tracks use; others play at a rate from the nearest |
| Engine (audio thread) | `src/audio/engine.*`, `dsp.h`, `reverb.*`, `tempo_map.*`, `spsc.h` | Tempo map (96 PPQ, segments start on exact ticks), sequencer scanning the grid every 2 ticks, modes **Staged** (the Track's `arrangement`, the prototype's logic), **Conductor** (phrase cycle Groove/Build/Drop/Break in the Short/Standard forms, bed by intensity with hysteresis, fills, Build rules incl. the 85% last bar, Drop impact and Surge, Break echo, Riffs, four players' leads with Split/Double-Time/Pierce/Overcharge/On-Beat Crit/Prism/Hyper, Track change through an 8-bar Break with a tempo ramp and the key on bar 5), **Metronome** (the sync screen), **Script** (sheets). Voices: P1–P4 leads, bass, stabs, pads, sampler; buses, sidechain, ping-pong delay, FDN reverb, section gain, soft clip, output gain, effect ducks, limiter. Commands in and beat events out through lock-free SPSC rings. No allocation, lock, log, file or JNI in `render()`; denormals flushed by the device layer |
| Device layer | `src/audio/audio_device*.{h,cpp}` | Oboe: presentation timestamps (`getTimestamp(CLOCK_MONOTONIC)`), stream generations, reopen after a disconnect (route change) or any other stream error (retry every 0.5 s), underruns and buffer growth, the device id read once per stream (Oboe's `getDeviceId()` can allocate). SDL: no timestamps; a buffer estimate |
| Band (main thread) | `src/audio/band.*`, `platform_audio.*`, `audio_args.*` | Owns music data, bank, engine; the callback publishes `{engine frame, stream frame, wall time}` (seqlock). **Heard time** for the frame being drawn: the device timestamp (else the buffer estimate + 10 ms; a timestamp older than 0.5 s or past the write head is ignored) at the moment the frame reaches the panel (2 refresh periods ahead), minus the sync offset; slewed 10% per frame, snapped beyond 25 ms. Releases beat events at heard time, mirrors the tempo map, computes the lead rule's shots (TECH_SPEC 6.3), haptics (light tick on heard kicks and clicks, heavy on the Drop impact, ≥ 120 ms apart, `RaveformActivity.haptic`), Bluetooth detection, sync offset in `SDL_GetPrefPath()/audio_sync.txt`. Picks its default mode from the scenario (below) |
| Stage on the band | `src/game/stage.*`, `src/platform/app.cpp` | Below |
| Scenes | `src/game/audio_scene.*` | `--scenario audio_test` and `--scenario sync` (design/screens.md), reduce-flashing mode included |
| Offline renderer | `src/tools/audio_render.cpp` → `raveform_headless` | Same engine against a simulated device (fixed or random bursts) and scripted MixState timelines (`staged`, `mock`, `full`, `surge`, the sheets); WAV + event log + stems; loudness calibration; dev seed search |
| Tests | `tests/audio_tests.cpp` → `raveform_tests` | Queues (incl. two-thread stress), seqlock, tempo map (constant, changes, capacity, 126 BPM rounding = prototype), each Track's staged render bit-identical with 192-frame and random 1–1000-frame bursts with every onset on the grid, conductor grid through the 85% bar, lead follows fire, variant on the bar line, Track change through the Break, no allocation in any mode, music data (override order incl. Track 3's, conductor numbers, calibration current) |

### The stage on the band's clock (what each run follows)

- **Real time** (benchmarks, any interactive run of a stage): `Stage::setBeat(&band_.view())`. The song clock
  is the band's heard song time; the sixteenths fire as the heard tick passes their grid position; the kick,
  snare (clap or snare roll), hat and bass envelopes come from heard events with look.md §7's decays; the
  lattice ripples follow the last two heard kicks, the ornaments step on heard hats, the Pulse Ring and the
  Lancer beam count heard beats. The stage's own script (Drop every 8 bars, Build bar, boss break every 16)
  is counted in heard bars from the band's next bar line; the band's sections are not followed yet (the
  benchmark's band holds its Drop). The song clock waits while the band's clock is unknown and then
  advances by the heard time's progress (at most 0.1 s a frame, never backwards); at most four sixteenths
  fire in a frame; a heard clock that jumps back more than a bar resynchronises the sixteenths, one more
  than a bar ahead for half a second (an app that was stopped) skips them. The first contract run on the
  Moto showed why: right after the stream started, AAudio returned a timestamp of position 0 taken 88 s
  earlier, the heard clock jumped 88 s ahead for one frame, and the stage, then without these limits,
  skipped its sixteenths there and waited (11 kills in 12 minutes, no Drop). The band now also ignores a
  device timestamp older than 0.5 s or one that puts the ear past the write head (the buffer estimate
  stands in), and logs the first twelve snaps of its heard clock and ignored timestamps; the stage logs
  its anchors and jumps.
- **Pictures** (`--screenshot`, `--fixed-step`): the scripted 132 BPM clock of scenes.md §2 and no band
  unless `--band` asks (the stage keeps the script even then). The acceptance numbers are unchanged:
  `frame_reef`, `look_card`, `look_card_nobloom` compare output identical to `captures/m1/*.compare.txt`,
  and every `scenes.py light` line identical to `captures/m1/light.txt` (`bench_endgame` frame 600: 14.0%,
  0.1645), regenerated by `scripts\review-m1.ps1 -Out captures\m2\review_m1`.
- **Benchmarks** (`bench_idle`, `bench_mid`, `bench_endgame`): `--band full` by default (every layer playing:
  the Drop held, four players on the 1/16-triplet grid with one Split harmony, every Riff at level 2, kills
  every frame), on the palette's Track (`palettes.json` `track`: Reef = Track 3, 132 BPM like the stage's
  script). `--no-audio` turns the band off for comparison.
- **Tier probe**: its own stage on the scripted clock.

### Match against the prototype (staged arrangement of each Track, `scripts\render-audio.ps1`)

Prototype: `design/tools/musicproto.py` run as a library by `scripts/audio_ref.py` (references in
`captures/m2/ref/`). Engine: `raveform_headless --render staged`. Same measurement code for both
(`scripts/audio_metrics.py`, which calls musicproto's `lufs` and `band_report`). Band and section
columns: the largest absolute difference over the six bands / the eight sections.

| Render | LUFS engine | LUFS prototype | True peak dBFS | Worst band diff dB | Worst section diff dB | Onsets checked | Worst onset error (samples) | Verdict |
|---|---|---|---|---|---|---|---|---|
| track01_club | -14.00 | -14.00 | -5.19 | 0.18 | 0.58 | 1226 | 0 | pass |
| track01_arcade | -14.00 | -14.00 | -5.20 | 0.19 | 0.60 | 1226 | 0 | pass |
| track01_velvet | -14.00 | -14.00 | -5.91 | 0.49 | 0.38 | 1226 | 0 | pass |
| track02_club | -14.00 | -14.00 | -5.58 | 0.42 | 0.28 | 1254 | 0 | pass |
| track02_arcade | -14.00 | -14.00 | -5.59 | 0.39 | 0.36 | 1254 | 0 | pass |
| track02_velvet | -14.00 | -14.00 | -5.70 | 0.31 | 0.32 | 1254 | 0 | pass |
| track03_club | -14.00 | -14.00 | -4.96 | 0.19 | 0.14 | 1329 | 0 | pass |
| track03_arcade | -14.00 | -14.00 | -5.00 | 0.20 | 0.12 | 1329 | 0 | pass |
| track03_velvet | -14.00 | -14.00 | -5.18 | 0.17 | 0.17 | 1329 | 0 | pass |

- Loudness is equal by construction (the output gain is calibrated on the engine's own render, as the
  prototype normalises its own); the match shows in the loudness **before** that gain: engine −8.96 /
  −8.92 / −8.25 (Track 1), −8.52 / −8.53 / −8.41 (Track 2), −9.23 / −9.19 / −9.00 (Track 3) LUFS against
  the prototype's −8.94 / −8.90 / −8.33, −8.43 / −8.54 / −8.34, −9.19 / −9.14 / −8.99 (within 0.09 LU).
  Track 3's output gains: −4.77 / −4.81 / −5.00 dB (club / arcade / velvet); Tracks 1–2 unchanged.
- The worst section of Tracks 1–2 is the Break (−0.2 to −0.6 dB): the pad stem is 0.7 dB below the
  prototype's, mostly the oscillators' random start phases (the prototype's own pad moves by 0.58 dB
  between phase draws; numpy's draw cannot be reproduced). Track 3's Break is within 0.17 dB.
- Effect sheet and lead timbres (`sfxproto.py`'s renders): engine −15.23 LUFS / −4.37 dBFS peak vs −15.2 /
  −4.4; −14.03 / −2.69 vs −14.0 / −2.6. Gameplay mock: −12.94 LUFS, true peak −1.71 (the prototype's own
  mock, a different 16-bar script: −12.9 / −2.4).
- One-shots against the prototype's synthesis, sample by sample (`scripts/audio_samples.py`):
  deterministic ones are identical; noise-based ones agree in their band statistics, within the spread
  of the noise draw. Stems (`--stems`): bass, stab, pulse, snare, lead, drums within 0.15 dB RMS; pad
  −0.68 dB; reverb return +0.26 dB (FDN, below).

### Where the engine differs from the prototype, and why

1. **Reverb.** The prototype convolves with 1.82 s of decaying noise per channel. Real time: two mono
   feedback delay networks (8 prime-length lines, Hadamard feedback, per-line losses for the exact RT60)
   after the same input mix, pre-delay and first-order 320 Hz / 7 kHz band limits, with four allpass
   diffusers. The output gain gives the impulse response unit energy (as the prototype normalises its
   IR), corrected so the 250 Hz–6 kHz share of the energy equals the prototype IR's: the return is now
   0.26 dB above the prototype's. The effects' reverb send joins the music reverb, divided by the
   master's present gain: one reverb instead of two (CPU).
2. **Zero-phase FFT filters on running buses** (pad high-pass 220 Hz, Break hats low-pass 5 kHz, master
   high-pass 30 Hz): causal Butterworth biquads, same magnitude, other phase. Baked one-shots keep the
   exact zero-phase filters (Bluestein FFT for deterministic signals; spectral synthesis for noise).
3. **Noise** comes from the engine's own generator, so noise one-shots match statistically. The kick's
   click seed (1061) was chosen with `raveform_headless --seed-search kick` to give the prototype's click
   spectrum (all kicks within about ±0.7 dB per band).
4. **Riser**: the twelve band noises are synthesised frame by frame in the frequency domain with the exact
   band shapes (STFT overlap-add), instead of 24 FFT filters as long as the Build.
5. **Loudness normalisation**: a fixed output gain per Track × variant (`src/audio/loudness_calibration.json`,
   written by `scripts\calibrate-audio.ps1`, embedded; a hash of the music data and `kDspVersion` flags it
   stale at start-up and in `scripts\test.ps1`).
6. **Control rate**: filter coefficients and envelopes are exact every 32 samples of each voice's age and
   interpolated linearly between, so nothing is divided per sample and the output does not depend on the
   device's callback sizes (tested bit for bit).
7. **Polyphony**: each player's lead holds at most 12 notes; the oldest then fades out in 5 ms.
8. **Output stage**: after the music's soft clip and output gain, a soft knee (linear to 0.66, ceiling
   0.8) keeps music plus effects below −1 dBFS true peak; the music alone peaks near −5 dBFS untouched.
9. Where the staged arrangement and conductor.json disagree, conductor mode follows conductor.json: Surge
   moves the lead one grid step (below), the Build keeps the bed as it stood (clap included), Riffs rest in
   the Break. The Build's last bar runs at 85%: the riser is stretched over it and the swell ends on the
   downbeat. Kill aggregation: small kills as the prototype's mock, medium and large so one kill plays at
   its LEVEL, more up to +4 dB. A Shard run restarts after a 0.5 s pause.

### Surge listening test (for the lead and the user)

`captures/m2/surge_compare.mp3` (`raveform_headless --render surge`, also `.wav`): Track 1, Club, conductor
mode, Short form, intensity 1.0 (the whole Groove bed), P1 firing throughout, no Riffs. Groove 4 bars
(0:00), Build 4 (0:08), **Drop 1 at 0:16.353** with Surge as the rule has it (conductor.json `lead.grid`: one
step, the 1/16 triplet, for the Drop's first 4 bars, then 1/16); Groove again at 0:32.353, Build 0:40.353,
**Drop 2 at 0:48.706** with Surge at 1/32 (the staged arrangement's "double" lead, which the user heard and
liked; here with the conductor's accents and pan); the Break from 1:04.706, faded out by 1:06.7. Each
Build's last bar runs at 85%, so the Drops sit 0.353 s later than whole bars. The event log confirms 24 lead
notes per bar at 16 ticks in Drop 1's Surge, 32 at 12 ticks in Drop 2's, 16 at 24 ticks after.
The rule is unchanged; the test uses a listening-only command (`CmdType::SetSurge`, default one step).

The arp has 16 entries and the triplet grid 24 steps per bar: step k (0–23, its position in the bar)
plays `arp[k mod 16]`. The whole arp takes the first 2⅔ beats; steps 16–23 replay `arp[0..7]` over the last
1⅓ beats, and the next bar starts again at `arp[0]` (conductor.json: "the arpeggio position is the step's
position in the bar"). At 1/32 (32 steps) the arp plays twice per bar, at 1/16 once.

**Decided (lead, 2026-11-11):** the user heard both, found both excellent and left the choice to the lead.
Both are kept, one after the other: the first half of the Surge runs one grid step faster, the second half
two (`conductor.json` `drop.surgeSteps: [1, 2]`, GAME_DESIGN §4.1; capped at 1/32). Not built yet: the
conductor still plays one step for the whole Surge. Build it with the gameplay Surge in M3 (it is a fire
rate as well as a sound).

### Device (Moto One 5G Ace, shared phone, Swappy 60 Hz)

| Run | Panel missed | CPU-side P95 / max ms | Main thread CPU P50 / P95 | Underruns | Engine time per callback P50 / P95 / max | Load P95 (time / callback length) | Result file |
|---|---|---|---|---|---|---|---|
| `bench_idle`, no band (`--band off`, 15 s) | 0% | 17.78 / 19.51 | – / 1.69 ms | 0 | – | – | `ZY22BXMJG3-bench_idle-20261111-175755.json` |
| `bench_idle`, full band (30 s) | 0% | 17.93 / 20.31 | – / 1.78 ms | 0 | 0.90 / 1.15 / 1.29 ms | 0.237 | `ZY22BXMJG3-bench_idle-20261111-180352.json` |
| `bench_idle`, full band, **10 minutes** (133,130 callbacks) | 0% | 17.90 / 21.36 | – / 1.78 ms | **0** | 0.90 / 1.06 / 1.34 ms | **0.220** (P99 0.265) | `ZY22BXMJG3-bench_idle-20261111-180929.json` |
| **Contract run**: `bench_endgame --tier low`, full band on Track 3, 10-minute warm-up then 120 s (7,188 frames) | **0%** (PASS) | 17.83 / 20.34 | 4.70 / 4.92 ms | **0** | 1.02 / 1.13 / 1.30 ms (CPU time P95 1.08) | **0.234** (P99 0.289) | `ZY22BXMJG3-bench_endgame-20261111-191123.json` |
| **Contract run repeated on the final build** by the lead (same command, after the overlay fix; 159,742 callbacks) | **0%** (PASS), worst frame 22.4 ms | 17.82 / 22.40 | 4.67 / 4.90 ms | **0** | worst callback 1.38 ms | – | `ZY22BXMJG3-bench_endgame-20261111-201832.json` |

- "Every layer playing" (`--band full`): peak 72 voices on Track 1 (81 on Track 3 on the desktop), no
  steals. Burst 240 frames (5 ms), buffer 480, AAudio shared (not MMAP). Callback wall time equals its
  thread CPU time (no preemption); the callback runs on the big cores 6 and 7.
- Main thread per frame (`Band::update`): 25 µs P50, 31 µs P95. Heard-clock latency 32–35 ms, from the
  device's timestamps.
- Heap allocations: 0 in the callback. On the main thread the overlay's `AudioDevice::stats()` allocated
  about 2 a second with or without the band (Oboe 1.11's `getDeviceId()`, added in M2); the id is now read
  once per stream: 0 in a 30 s check run (`ZY22BXMJG3-bench_endgame-20261111-192659.json`).
- Contract run against M1's run without sound (`-164531`, Low): panel 0 missed in both; main thread P50 /
  P95 4.70 / 4.92 ms against 4.59 / 4.81; GPU at the governor's clock 13.44 / 13.77 ms against 13.35 /
  13.72; the callback ran on the big cores 6 and 7, next to the main thread; 81 voices at peak, no
  steals; heard-clock latency 34.6 ms. The stage carried its endgame load on the band's clock (73,517 kills,
  49 Drops, 24 boss breaks in the 12 minutes). Its 239 main-thread allocations in the window are the
  overlay's, fixed after it (above). The repeat on the final build (`-201832`) has 0 main-thread
  allocations in the window, 73,470 kills, GPU 13.41 / 13.76 ms, battery 25.6 → 28.0 °C, thermal status 0.
  The 4 per frame inside the pacer and 1 per frame on another thread are Swappy's own, as in every run
  since M0.
- The first contract run (`-185010`) is void: the stage stopped after its first sixteenth (the stale
  timestamp, above). The valid run still had the stale timestamp at start-up (the stage's limits rode it
  out: one resynchronisation 0.08 s after the start); the band's guard came after it and the stale
  timestamp did not recur in the one check run since, so the guard has not been exercised on the device.

### M2 blocks in shared files (all in `// M2 audio begin/end` blocks unless noted)

| File | Change |
|---|---|
| `CMakeLists.txt` | Embed the music JSON and the calibration; `raveform_audio` static library linked into `raveform_core`; band and scene sources; desktop `raveform_headless` and `raveform_tests` |
| `src/core/args.{h,cpp}` | `AudioArgs audio;` in `AppArgs`; one `isAudioArg` branch (parsed in `audio/audio_args.cpp`) |
| `src/platform/app.{h,cpp}` | `Band band_`, `AudioScene audioScene_`, `Content::Audio`; band defaults from the scenario, pictures and palette; band and scene init before the device opens; `openAudio()`; key and touch hooks; `band_.update()` before the frame's content; the stage's `setBeat` in real time; key R also reaches the sync screen; `band_.resetMeasurements()` at the measure start; `band_.writeJson(j)`; one overlay line |
| `src/game/stage.{h,cpp}` | `setBeat`, heard-clock `advance` / `envelopes`; the scripted path is unchanged arithmetic |
| `android/.../RaveformActivity.java` | `haptic(int)` and `audioDeviceType(int)` |

### Not done, not verified, open

- Needs the user: that it grooves; that flash and click coincide on a speaker; that Bluetooth can be
  corrected with the slider (no Bluetooth device here); whether haptics on kicks feel right; Surge at the
  rule's 1/16 triplet or at 1/32 (the file above); Track 3's three-note lead (design/music/README.md).
- The stage follows heard time and heard beat events, not yet the band's sections (its Drops are the
  script's) nor the lead rule's shots (`view().shots`; the scripted player fires on its own grid). A Track
  change would restart the band's song clock under a running stage; no stage run can change Track yet.
- The pad stem is 0.7 dB below the prototype's (mostly start phases): inside the match.
- The callback load at P95 (22–24% of the burst) leaves little margin under 25%; next step: lead voices
  processed in pairs or fours.
- Start-up: the one-shots bake before the device opens (0.96 s desktop; 2.51 s on the Moto with Track 3,
  1.74 s without).
  TECH_SPEC 6.5 caches baked sounds on disk; not done (or bake on a worker during the logo).
- 12R and tablet not attached: no numbers.
- Settings screen (sound variant, haptics strength, volumes) comes later; variants switch on the next bar
  line from the key, the tap and `--sound`.

### Files for the lead (`D:\Stuff\DevFolder\Android_Games\Raveform\captures\m2\`)

`surge_compare.mp3`, `track01_club.mp3`, `track01_arcade.mp3`, `track01_velvet.mp3`, `track02_club.mp3`,
`track03_club.mp3`, `sfx_sheet.mp3`, `lead_timbres.mp3`, `gameplay_mock.mp3` (each also as a 16-bit `.wav`),
`match_table.md`, `review_m1\` (the M1 review set on the merged build), sync screenshots `*.small.png`,
`sync_strip\` (30 frames of the sync screen, normal and reduce-flashing).

## M1 Look and renderer — status 2026-11-11 (evening)

Step 1 (stills) **done and accepted** (re-run below, unchanged). Step 2 (motion) **built** and now on
look.md v2.1: the afterglow is the soft tail of §7, the light rules of §3 hold, the endgame carries the
load of scenes.md §4 in view. Step 3 (proof): **`bench_endgame` meets the contract on the Moto at Low,
Medium and High** (10-minute soak, then 120 s: 0 missed frames on the panel in each; GPU work at the top
clock 11.05 / 12.21 / 14.28 ms at P95 against the 12 ms budget). **Open:** the endgame
frame 600 still misses the light targets of scenes.md §5 after look.md v2.2 (14.0% dark, luminance
0.1645 against at least 15% and at most 0.16; a quiet bar meets both); reported to the lead with the
breakdown, nothing else tuned. The 12R and the tablet are not attached: every item for them is pending.
The look is Signal (user decision 2026-11-11); Laser and Haze are developer switches and were not re-shot.

### Look bible v2.2 built (2026-11-11, evening; the lead's `39d63a0`)

- **Afterglow on the light budget** (look.md §3, §7; `look.json` `afterglow.gain = "swarm"`, read at
  start-up): the composite adds `0.7 × swarmGain × max(previous × e^(−dt/τ) − source, 0)`; knee, τ and the
  source level unchanged. At frame 600 (swarm gain 0.338) it now adds 0.0076 of luminance (was 0.020).
- **Shot streaks** (look.md §3 player shots; `look.json` `shotStreak`): every shot (player, wingman,
  Sentinel) is a capsule from its head back over speed × 1/60 s + 8 wu, never under 16 wu; the player's
  shots fly at GAME_DESIGN §4's 1,400 wu/s (was 1,500): 31 wu capsules, Sentinel shots 27 wu. Behind
  every shot the afterglow is now one even streak: `captures/m1/afterglow_shot_tail_mid600.png` (top:
  frame 600; bottom: without the afterglow), against `afterglow_shot_beads_mid600_lanes.png`, the same
  crop before v2.2, where each lane is a row of dots. Correction: the earlier
  `afterglow_shot_beads_mid600.png` crop (left of the ship) shows mostly a stream of **Shards** flying to
  the ship, not shots; Shards still leave soft dots (`afterglow_shard_beads_mid600.png`, same crop as that
  one, on this build). v2.2 does not cover them.
- **Drop confetti as a shower** (look.md §5; `look.json` `dropConfetti`): each spark starts 0-200 wu
  beyond the view edge (spread along the edges by their length) and is born 0-250 ms after the Drop
  (stateless birth time in the future), 300-700 wu/s toward the ship, 1.0-1.6 s. The streak direction was
  checked before the change and was already the spark's own velocity (the particle shader streaks along
  `v0·e^(−drag·t)`; at the top-left edge the streaks ran at about 31° down to the right, at the top-right
  about 37° down to the left, on the left edge level: each toward the ship); the "same slant" was the
  sparks all born on the edge line in one frame. The new `strip_bench_endgame_879_*` shows no border in
  any frame; the shower comes in over the following half second.

| `scenes.py light` (desktop, Signal, Reef) | Required | Before v2.2 | **v2.2** |
|---|---|---|---|
| `bench_endgame` frame 600, High | ≥ 15%, ≤ 0.16 | 13.3% / 0.1725 | **14.0% / 0.1645** (misses both) |
| `bench_endgame` quiet bar, frames 400-407 | ≥ 15%, ≤ 0.16 | 14.2-15.0% / 0.161-0.165 | 15.6-16.2% / 0.154-0.157 (meets) |
| `bench_endgame` frame 600, Low | | 28.1% / 0.149 | 30.6% / 0.140 |
| `bench_mid` frame 600 | ≥ 25%, ≤ 0.11 | 35.1% / 0.083 | 36.1% / 0.081 (meets) |

Breakdown of the v2.2 frame 600 (luminance each class adds, one off at a time): hulls 0.052, pools 0.020,
kick pump 0.014, player 0.013 (the longer shots: +0.002), afterglow 0.0076, background 0.008, lattice
0.007, bullets 0.006, Shards 0.006, sparkles 0.004, sparks 0.003, bloom 0.071. Without the afterglow
16.0% / 0.157; without the pump 15.8% / 0.151. Frame 600 sits on a kick (all pumps at their peak).

Acceptance stills: identical (0.38 / 1.27 / 1.11 mean error, luminance 0.995 / 0.983 / 0.982, worst
probe 4). Desktop Debug `bench_endgame` 60 s at Ultra and Low: valid, 0 validation, 0 loader messages, 0
heap allocations (`captures/desktop_m1_endgame_*_60s_20261111-1748.log`). Moto, the lead's short check
(Low, 20 s warm-up, 30 s, deployed right before, lock taken): 0 missed of 1,797 on the panel, max 19.1 ms,
main thread 4.53 / 4.74 ms (P50 / P95), GPU at the governor's clock 13.36 / 13.68 ms (before v2.2 in the
contract run: 4.59 / 4.81 and 13.35 / 13.72), 0 heap allocations, thermal 0
(`ZY22BXMJG3-bench_endgame-20261111-174725.json`). Nothing got slower.

### This session (2026-11-11, afternoon)

- **Afterglow v2.1** (look.md §7) was already in the code the previous coder left (renderer, `afterglow.frag`,
  committed in `2976716`); the pictures that showed ghost rows (`captures/m1` of the morning, now in
  `captures/archive/m1-2026-11-11-morning/`) were older than it. Checked against §7 line by line: source =
  the bloom level of σ 6 ref px (level 3, W/8) less the 0.25 knee, history at that size re-aimed by the
  camera, `0.7 × max(previous × e^(−dt/τ) − source, 0)` added. Strips show soft tails and no line-sharp
  ghosts, but sparse fast shots still leave rows of soft dots (see "Light at endgame").
- **Light rules** (look.md §3) checked one by one against the code: small kills sparkle-flash only, no pool,
  no ring; ≤ 24 kill pools (oldest fades early); no tier auras; bullet and Shard sparkles High and up; ≤ 6
  shock rings; warp-in rings from 2.5 × the glyph's extent (elites 90 wu); gains as in the §3 table. Fixed:
  the boss stand-in drew an aura on each of its 48 parts (the target frame has none: they are ordinary
  glyphs, not a boss design); kill pools lived 1.2 s for every kill (now the recipe's longest spark, 0.9 /
  1.2 s, as in the target); the ship was 16 wu (glyphs.json: 14); Lancer and Sweeper as the target draws them.
- **Load in view** (scenes.md §4): the endgame had about 515 enemies and 560 bullets in view (rings of
  480–600 wu and ±900 × ±450 scatters do not fit a 720 wu tall view). Formations are now fitted to the 20:9
  screen (rings squashed vertically as the target frame does, packs charging edge to edge, scatters and
  boxes on screen, the boss 600 wu across and 180 wu up and down): **706 enemies and 961 bullets in view**
  on average over the Moto run (max 709 / 1,247; the target frame holds 699 / 1,137).
- **Boss stand-in** in the Caroms' hue (hue 3, violet in Reef). **Enemy bullets** upright diamonds
  (`enemy_bullet`: points on the axes, `facesHeading` false); see the deviation below.
- **`--cuts N`** applies look.md §10's cuts in order on top of a tier; Low is back to TECH_SPEC's Low with
  glass and 24 kill pools (dropping them are cuts 1 and 7). **`--reduce-flashing`** (key R): every amount of
  the §7 beat table × `beat.reduceFlashingScale`, no Drop burst. **First-launch tier probe** (TECH_SPEC 5.4,
  below). **`--mute sparkles|pools`** and `scripts\light-breakdown.ps1` for the light table;
  `scripts\review-m1.ps1` re-shoots the review set.

### Light at endgame (scenes.md §5; `python -I design\tools\scenes.py light`)

Desktop, 2400×1080, Signal, Reef, High (the picture default), frame 600 (fixed 1/60 s steps):

| Picture | Dark (< 48) | Mean luminance |
|---|---|---|
| Target `design/previews/frame_endgame_ref.png` | 16.7% | 0.148 |
| Required | at least 15% | at most 0.16 |
| Before (HEAD `2976716`, the lead's measurement reproduced) | 8.5% | 0.193 |
| **After** (this build) | **13.3%** | **0.1725** |
| After, a quiet bar (frames 400–407, `strip_bench_endgame_407_*`) | 14.2–15.0% | 0.161–0.165 |
| After, Low tier (desktop) | 28.1% | 0.149 |
| `bench_mid` frame 600 (required ≥ 25%, ≤ 0.11) | 35.1% | 0.083 |

Frame 600 is exactly on a kick (beat 22): the exposure (+8%), bloom (+10%) and enemy-swell pumps are at
their peak, and the beat's kills (ten small, two medium, one large) and the enemies killed two beats
earlier (finishing their warp-in) all flash on it. The target frame is a still at a quiet instant,
without pumps or afterglow.

Breakdown, frame 600, one class of light off at a time (`scripts\light-breakdown.ps1`, pictures in
`captures/light/`; before = HEAD `2976716`):

| Off | Before: dark / luminance | After: dark / luminance | After: luminance it adds |
|---|---|---|---|
| nothing | 8.5% / 0.193 | 13.3% / 0.1725 | — |
| lattice | 10.3% / 0.185 | 16.0% / 0.1656 | 0.007 |
| hulls (horde, elites, boss) | 11.8% / 0.142 | 18.0% / 0.1168 | 0.056 |
| enemy bullets' scene light | 13.4% / 0.187 | 17.5% / 0.1658 | 0.007 |
| sparks | 8.7% / 0.188 | 13.3% / 0.1690 | 0.004 |
| pools (all soft light ≥ 16 ref px) | 11.0% / 0.156 | 16.8% / 0.1527 | 0.020 |
| sparkles (all soft light < 16 ref px) | 9.4% / 0.186 | 14.1% / 0.1672 | 0.005 |
| Shards | 8.6% / 0.181 | 13.3% / 0.1647 | 0.008 |
| player (ship, shots, Riffs, trail, pool) | 9.0% / 0.182 | 16.4% / 0.1611 | 0.011 |
| **afterglow** | 12.1% / 0.166 | **16.7% / 0.1523** | **0.020** |
| background | 13.0% / 0.185 | 17.7% / 0.1643 | 0.008 |
| bloom | 32.3% / 0.097 | 40.3% / 0.0921 | 0.080 |
| priority layer | 7.0% / 0.198 | 10.8% / 0.1758 | −0.003 |
| kick pump (frame 600 is on the kick) | 10.0% / 0.178 | 14.8% / 0.1588 | 0.014 |

Before, the auras (20 elites' and the boss parts' 48) added 0.010 and the kill pools 0.023; before, the
endgame also held only about 515 enemies and 560 bullets in view. Without the afterglow the frame meets
both targets (16.7%, 0.152); without the pump it meets the luminance (0.159) but not the darkness (14.8%).
The lead's own model of the v2.1 afterglow (`crowd.py study`, the whole horde drifting 5 px a frame) adds
0.0085 to the target (15.4%, 0.157); in the engine it adds 0.020 because much of the light moves much
faster than that (shots 37 px a frame, the Sweeper arm, Shards, charging packs, the boss's fans spawning
on its turning arm tips) and the tail of each flash on the beat. Where its 0.020 comes from (afterglow
on minus off, with one class muted): the player's light 0.005, the horde 0.003, Shards 0.002, sparkles
0.002, the boss 0.001, bullets 0.001, sparks 0.000; the remaining 0.006 only exists where several classes
overlap. Blurred to σ 6 the dense middle of the endgame sits above the 0.25 knee almost everywhere, so
the tail is not limited to hot light at this density: 36% of the frame's pixels gain more than 8 levels
from it (`captures/light/afterglow_adds_x3.png`: what it adds, ×3). The knee is absolute while the horde's
light follows the crowd gain; whether to change the knee, the strength or the gains is the lead's call.
At Low (lattice every 32 wu, so its lines survive the σ 6 blur) the afterglow adds 0.020 too (28.1% /
0.149 with it, 34.5% / 0.129 without) and the lattice shows through its haze as a faint grid, cut out
where the static lines already hold the light (`captures/light/afterglow_adds_low_x3.png`).

**Ghost dots are not fully gone** (v2.1; see "Look bible v2.2 built" for what changed). A shot moved
37.5 px a frame and the source is blurred to σ 6 px, so where shots are sparse (`bench_mid`) each shot
left a row of soft dots about 37 px apart (`captures/m1/afterglow_shot_beads_mid600_lanes.png`). The
crop first offered as evidence, `afterglow_shot_beads_mid600.png`, shows mostly a stream of Shards flying
to the ship, which beads the same way. In the endgame the shots are dense and the dots merge into a haze
along the lanes. Anything moving faster than about 2σ a frame (shots, young sparks, Shards near the
ship) does it.

### Stills acceptance (`design/scenes.md`, `design/tools/scenes.py compare`)

Engine at 2400×1080, `--still --no-overlay`, desktop Debug with validation: 0 validation messages, 0
loader messages. References regenerated by the lead on 2026-11-11 with soft light, clouds and the
background gradient at pixel centres (the `.rfscene` files did not change).

| Still | Mean abs error (/255) | p95 / p99 / max | Linear luminance ratio | Soft-light ratio p10 / p50 / p90 | Worst probe | Pixels off by ≥ 16 |
|---|---|---|---|---|---|---|
| `look_card`, bloom off | 0.34 | 1 / 1 / 24 | 0.997 | 0.99 / 1.00 / 1.01 | 3 (`pool_16`) | 72 |
| `look_card`, bloom on | 1.22 | 4 / 8 / 24 | 0.984 | 0.93 / 0.98 / 1.00 | 3 (`tone_p1_2`) | 6981 |
| `frame_reef` | 1.01 | 3 / 4 / 20 | 0.984 | 0.97 / 0.98 / 0.99 | (no probes) | 1 |

Re-run after the motion work (background at W/4, interleaved-gradient dither, half-precision
composite): 0.35 / 1.23 / 1.05 mean error, luminance 0.997 / 0.984 / 0.983, soft light unchanged, probes
within ±3, 0 validation messages (2026-11-11 09:20). After the low-resolution terms moved into the last
bloom pass (below): 0.38 / 1.27 / 1.11, luminance 0.995 / 0.983 / 0.982, soft light p10-p90 0.98-1.00 /
0.93-1.00 / 0.96-0.99, worst probe 4 (`pool_16` blue), 0 validation messages (2026-11-11 09:45). All
inside the look.md §9 targets. **Re-run on this session's build (2026-11-11 17:00, `scripts\review-m1.ps1`):
0.38 / 1.27 / 1.11 mean error, luminance 0.995 / 0.983 / 0.982, soft light p10-p90 0.98-1.00 / 0.93-1.00 /
0.96-0.99, worst probe 4 (`pool_16`), 0 validation messages: unchanged, inside every target.**

History of these numbers:

1. First acceptance, against the first references (soft light and background sampled at integer pixel
   coordinates, so the engine reproduced that half-pixel offset): 0.34 / 1.22 / 1.01, probes within ±5.
2. The lead's run of that build against the regenerated references: 0.39 / 1.25 / 1.08, probes ±4.
3. Half-pixel reproduction removed from the engine (`shaders/glow.frag`, `shaders/composite.frag`): 0.34 /
   1.22 / 1.01 against the regenerated references.
4. Threat layer changed from straight alpha with the colour clamped to 1 to premultiplied "over" (the
   reference mixes and then clips, so a display-linear core colour above 1 still brightens the
   anti-aliased edge): threat-core edges were up to 24 levels too dark in red. Pixels off by ≥ 16 fell
   from 1030 to 72 (card, bloom off), 7939 to 6981 (card, bloom on) and 2167 to 1 (reef); the `kill_core`
   probe went from −5 to 0 / −1. Means unchanged. This is the table above.

What is left: with bloom on, smooth differences in the bloom's shape (the pyramid approximates the six
Gaussians of `look.json`; at most about 20 levels on a soft ramp next to the brightest bars), and single
pixels at threat edges where the reference keeps about 0.3% coverage of a very hot colour. Files:
`captures/m1/<still>.png`, `<still>.cmp.png` (engine, reference, difference ×4), `<still>.compare.txt`.
Command: `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 4 -Screenshot captures\m1\<still>.png
-AppArgs '--scene design/scenes/<scene>.rfscene --still --bloom on|off --no-overlay'`, then
`python -I design\tools\scenes.py compare captures\m1\<still>.png <scene> [--nobloom]`.

Watch item from the lead's review (no action yet): thin hot lines at shallow angles (the SHOT 4.5 row of
the card, 7° lines) look slightly ropey in the engine and in the reference, because the tone map hardens
the one-pixel anti-aliasing ramp at high intensity. The frame strips will show whether rotating glyphs or
shots crawl; if they do, the lead gets a strip and decides whether hot lines get a wider ramp. The
coverage formula stays as it is until then (the stills are matched to it).

### How the renderer is built (M1)

| # | Pass | Size | Content |
|---|---|---|---|
| 1 | Scene | W × H | Additive into `B10G11R11_UFLOAT` (fallback `R16G16B16A16_SFLOAT`), cleared: capsules (lines, discs, text, lattice, shots, trails, beams), instanced glyphs, stateless sparks, soft lights under 16 reference px ("sparkles"), triangles (glass) |
| 2 | Pools | W/4 | Soft lights of 16 reference px and up (flashes, kill pools, auras, player and well pools), times the look variant's pool level |
| 3 | Background | W/4 (W/2 on Ultra) | Radial gradient and up to 8 nebula clouds at the pixel centres they cover (TECH_SPEC 5.2 pass 0) |
| 4 | Bloom down | W/2 … W/2^L | Level k from level k−1 (level 1: the scene plus the pools) with 4 bilinear taps at ±a_k source texels. a_k is solved at start-up so that the spread accumulated at level k, plus what the upsampling adds, equals the look's σ_k (`look.json`, reference px × H/1080). L = 6 / 5 / 4 by tier, weights renormalised |
| 5 | Bloom up | … W/2 | Each level = bilinear coarser level × its weight + this level × w_k. The last one (W/2) also gathers the pools, the afterglow excess and the background, each divided by the bloom strength, so the composite reads one low-resolution texture (`bloom_final.frag`) |
| 6 | Afterglow | W/8 (the bloom level of σ 6 ref px) | look.md v2.1 §7. Source = bloom level 3 (σ 6 ref px) less the knee: `s × max(m − 0.25, 0) / m`. Two outputs: history = max(source, previous × e^(−dt/τ)) with the previous history re-aimed by the camera (scale and shift) and faded over 4% at its edges; excess = max(previous × decay − source, 0), added × 0.7 through the last bloom pass. No line-sharp history anywhere. Every tier with the afterglow on (off on Low only as cut 6) |
| 7 | Distortion | W/8 (W/4 on Ultra) | `R16G16_SFLOAT` displacement in logical px from up to 8 rings (Gaussian profile, "wide" = full width at half maximum) and 4 wells, evaluated analytically |
| 8 | Composite | swapchain | scene(uv + displacement) (three taps with chromatic aberration on High and up) + strength × gathered bloom (bloom + pools + afterglow + background) → drain toward white (the Build) → vignette → exposure → hue-preserving tone map with white bleed → ±½ code value of dither (interleaved gradient noise, moved to linear light through the sRGB slope on an sRGB target). Then, in the same render pass: the threat layer (all dark rims, then all cores, premultiplied over; capsules and instanced glyphs) and the HUD (additive) |

- Passes 1-7 are one command buffer submitted without waiting; pass 8 is a second command buffer that
  waits for the acquired image, so GPU timestamps separate work from the wait for the panel.
- Every HDR pass is in logical landscape; only the swapchain pass applies the pre-rotation (composite
  UVs, threat and HUD clip space).
- Instance data: one persistently mapped buffer with a region per primitive per frame in flight, plus a
  static region for a replayed `.rfscene`; glyph instances are staged on the CPU and grouped by shape
  when the frame is recorded (one instanced draw per shape in use); sparks live in a persistent ring
  whose slots are reused only after every frame that could draw them has finished. No allocation per
  frame.
- Design data: `design/look.json` and `design/look_variants.json` are embedded at build time and parsed
  once at start (`src/core/json_doc.*`, `src/content/look.*`); `--look <id>` picks the variant (default
  `signal`). `.rfscene` replay for the stills: `--scene`, `src/game/rfscene.*`.
- Primitives and formats: `src/gfx/capsule.h`. Shaders: `capsule.*`, `glow.*`, `tri.*`, `fullscreen.vert`,
  `bloom_down.frag`, `bloom_up.frag`, `composite.frag`.

### Deviations from TECH_SPEC §5.2 / §5.3 (so far)

- **Capsule instance:** endpoints (f32×4), then two f16×4 words: start colour RGB + half width, end
  colour RGB + flags (flat start, flat end, disc). Still 32 bytes. Colours are linear HDR with the
  intensity folded in, so there is no separate intensity, no RGBA8 colour and no skirt. Reason: look.md
  v2 draws colour gradients along a stroke and flat joints on polygons, and the glow comes from bloom (the
  reference has no skirt).
- **Two more primitives:** soft light (centre, radius, f16 colour; 24 bytes; Gaussian with σ = r/2.45,
  square cut at 1.25 r) for sparkles and kill pools, and triangles (16 bytes per vertex, colour per vertex)
  for glass fills. Reason: look.md v2 and the stills use both. Pools are drawn at quarter resolution, so a
  large pool costs 1/16 of its full-resolution fill (§5.1 principle 2).
- **Bloom:** 4-tap down with solved offsets and 1-tap bilinear up at every tier, matched to the six
  Gaussians of `look.json`. The 13-tap prefilter and tent upsampling listed for High are not built (the
  solved 4-tap pyramid already matches the reference); the tiers change the level count (4 / 5 / 6).
- **Glyph instance:** position (f32×2), two rotations (f16×4: layers 0 and 2, layer 1), scale, hull
  intensity, hull half width and colour mode (f16×4), two RGBA8 linear colours: 32 bytes, not 24. The
  colour fields of look.md §2 need two colours per instance; the modes cover enemies, white copies and
  marks, the player hull, the two ring fields and flat colours (Shards, warp-in rings, bullets, the
  threat layer). Glyph meshes are flipped to y down at load, layer 0 first (the low-detail LOD and the
  white copies draw that prefix), with look.md §4 joint flags per layer. Three unit circles (32, 64 and
  128 segments) are added for rings.
- **Spark:** birth position, birth time (f32×3), velocity, life, drag (f16×4), intensity and half width
  (f16×2), RGBA8 colour, 4 spare bytes: 32 bytes. Space-warp of sparks by wells (TECH_SPEC 5.3 details)
  is not built. Embers (10 per medium or large kill) are CPU-drawn sparkles; shatter pieces are CPU
  capsules; both are few.
- **Afterglow** (v2.1): the source is bloom level 3 as look.md asks (the pools are part of the bloom's
  input, so they are part of the source too). It runs only when the bloom does (stills turn both off).
  TECH_SPEC 5.4's "quarter res or off" on Low is now W/8 on every tier.
- **Composite precision:** colour maths in half precision, positions in full; the dither is
  interleaved gradient noise (TECH_SPEC 5.2 allows it) and its amplitude in linear light uses
  max(1.82·√x, 1/12.92) for the sRGB slope (within −20% / +30% of exact).
- **Gathered low-resolution terms:** pools, the afterglow excess and the background reach the composite
  through the last bloom pass instead of three more full-resolution taps (measured: each tap costs
  about 0.27 ms on the Moto). They go through one more bilinear step and the `B10G11R11` W/2 target; the
  Build's drain now also touches the background. Stills: table above.
- **Threat layer:** capsules and instanced glyphs, a dark rim and a core per segment, premultiplied
  over. The Moto's and the desktop's swapchains are `R8G8B8A8_SRGB` / `B8G8R8A8_SRGB`; with a UNORM
  swapchain (fallback only) the threat layer and HUD would blend in encoded space.
- **Governor v1** steps down after 0.5 s with more than 5% of the last 120 frames missing a refresh, the
  GPU saturated (work above 97% of the period) or thermal status SEVERE; it steps up after 10 s with no
  missed frame and GPU work under 75% of the period. TECH_SPEC 5.4's inputs (P95 interval, GPU time
  against a fixed budget) do not work as written on these phones: the present-call P95 sits 1 ms above
  the period with no frame late, and the GPU's clock governor keeps the GPU work near 85% of the period
  whatever the load. Spark density moves first (in steps of 0.25), then the tier; refresh-rate steps
  (120 → 90 → 60) are not built yet. A tier change waits for the GPU and rebuilds the targets (one missed
  frame on the desktop). Off in benchmarks.
- **Bench scenes** follow `design/scenes.md`; where it leaves freedom: shoal streams follow an
  arc-length-parametrised figure of eight (650 × 260 wu); shots fly at 1,500 wu/s; kill pools live as
  long as the recipe's longest spark (medium 0.9 s, large 1.2 s, as `crowd.py` draws the target);
  medium and large shock rings reach 80 and 120 wu; the bass envelope decays in 180 ms; the
  background's 8° hue drift toward the accent is a small mix toward the accent's colour; the boss
  phase break falls on bar 16k + 12 and the Drop on every 8th bar; the far pool (3,072 − 709 slots)
  orbits 1,500-4,200 wu from the ship with far-mode updates only.
- **Endgame formations fitted to the screen** (2026-11-11 pm), so the counts in view are scenes.md §4's:
  the table's distances do not fit a 720 wu tall view (a 480–560 wu ring is 1,000 wu tall). Rings keep
  their width and are squashed vertically (Sentries 0.55, Moths 0.82, Tethers 500 × 200, Weavers
  0.45), as the target frame squashes its rings; the shoal's figure of eight and the Wyrms' paths are
  softly fitted into ±680 × ±300 wu (tanh per axis); the spear and the Hound packs run to the edge of
  that box's ellipse; Drifters and Caroms are scattered in that box; the boss stand-in is 600 wu across
  and 180 wu up and down; Sentry arms have 16 wu between bullets (330 wu arms). `bench_mid` keeps its
  explicit ±900 × ±450 and ring geometry: about 290 of its 500 enemies are in view at frame 600.
- **Enemy bullets are drawn upright** (points on the screen's axes, never turning). Confirmed by the lead
  and written into look.md v2.2 §3 (the first brief had asked for bullets turned to their travel).
- **Boss stand-in:** no aura on its 48 parts (the target frame has none; look.md's boss aura would be
  one aura for a real boss).
- **Background:** the engine shows fewer nebula clouds than look.md's "about six on screen" (8 clouds on
  a 3,600 × 1,800 wu tile, 1-3 on screen; the composite has 8 cloud slots and six on screen would need
  about 34 on the tile) and about 270 star-dust sparkles on screen instead of 420 (tile 2,000 × 900 wu).
  Not changed this session: both are faint (the whole background adds 0.008 luminance at frame 600).
- **Tiers:** Low is TECH_SPEC's Low (4 bloom levels, no distortion, aberration or clouds, sparks × 0.35,
  lattice 32 wu, simplified LOD) and keeps glass and 24 kill pools; look.md §10's cuts are `--cuts N` on
  top of any tier. The afterglow is W/8 on every tier.
- **First-launch tier choice** (TECH_SPEC 5.4): the probe runs on screen under a "SETTING UP FOR THIS
  DEVICE" label, not off screen, and takes 6.3 s on the Moto (four tiers from Ultra down, 1-2 s each under
  a 20-draw saturating load) instead of two seconds; the GPU name is not used as a hint (the cache is keyed
  by GPU and driver). Budget: GPU work at the top clock ≤ 72% of the refresh period (PLAN §5's 12 ms at
  60 Hz). Pictures (`--screenshot`, `--scene`) and tests default to High so they stay comparable. The
  lead accepted the on-screen probe for M1; **from M3 it should run behind the title screen.**
- **Reduce-flashing mode** halves the amounts of look.md §7's beat table (exposure, bloom, zoom, enemy
  swell, aberration, lattice ripple and shimmer, background bass and hue drift) and removes the Drop's
  bloom burst; the Drop's white front, confetti and zoom snap stay (scenes.md events, not in the table).
  The Nova (§5) is not in M1.

### Corrections to M0 found in M1

1. **Resolution.** The Moto rendered at 2174×1020, not 2400×1080, through M0 and until 2026-11-11 09:05.
   On Android 11 `RaveformActivity` hid the system bars but did not call `setDecorFitsSystemWindows(false)`,
   so the window kept the bar and cut-out insets (2400 − 106 − 120, 1080 − 60). The app had logged
   "surface 2174x1020 differs from the display mode 2400x1080". Fixed; the log now says "matches". All
   M0 `bench_idle` numbers were measured at 86% of the native pixel count. The 12R (Android 14) is likely
   affected the same way: re-check when it is attached.
2. **Panel-side grading with Swappy.** Swappy counts a frame late only against the time it asked for:
   when the GPU needs two periods per frame, it reports 0 late frames (a 30 fps run graded PASS).
   `bench-android.ps1` now also counts present intervals above 1.5 periods (the app's `missed`, far
   outside the 1-2 ms present-call jitter) and floors the panel grade at the CPU-side median in whole
   periods. M0's clean runs had no such intervals, so their grades stand.

### GPU timing method

- Timestamps after every pass (`gpu_ms` in the result; `total` is work only, `wait` the gap before the
  swapchain pass). The GPU's frequency governor picks the clock that keeps the GPU about 85% busy, so in
  a normal run the timestamps sit near 14 ms whatever the load (a run whose composite wrote a constant
  still read 14 ms in total). The shell user cannot read the GPU clock (`/sys/class/kgsl` is denied).
- Cost at the top clock: `--gpu-burn 40` adds 40 untimed full-screen draws before the timed passes, so
  the GPU is saturated (frames take 60-290 ms) and runs at its highest clock; the timed passes then show
  what they cost there. This is the number to hold against the 12 ms budget.

### Moto contract runs (2026-11-11 pm; PLAN §5: 10-minute soak, then 120 s, graded on the panel at P95)

`bench_endgame`, 2400×1080 (surface matches the display mode), Signal, Reef, Swappy at 60 Hz, overlay
on, main thread on the big cores, this session's build deployed right before each run (shared phone,
quiet: 0.86-0.99 foreign cores on average, mostly SurfaceFlinger, the audio server and the composer). In view on
average: 706 enemies, 961 bullets, 2,345 glyph instances; crowd gain 0.479, swarm 0.331. 0 heap
allocations on our thread in every run; 0 audio underruns.

| Tier | Panel: P95 / missed / max (CPU side) | Verdict | Present call P95 / P99 (ms) | Main thread P50 / P95 (ms) | GPU at the governor's clock P50 / P95 (ms) | GPU at the top clock P50 / P95 (ms) | Thermal, battery | Result file (`captures/bench/ZY22BXMJG3-bench_endgame-…`) |
|---|---|---|---|---|---|---|---|---|
| Low | 1 period (16.67 ms) / 0 of 7,188 / 24.9 ms | **PASS** | 17.72 / 18.58 | 4.59 / 4.81 | 13.35 / 13.72 | **10.74 / 11.05** | 0 → 0, 26.5 → 28.8 °C | `20261111-164531`, top clock `-171046` |
| Medium | 1 period (16.67 ms) / 0 of 7,188 / 19.0 ms | **PASS** | 17.68 / 18.37 | 5.52 / 5.83 | 12.16 / 14.82 | **11.99 / 12.21** | 0 → 0, 28.8 → 29.7 °C | `-165823`, top clock `-171128` |
| High | 1 period (16.67 ms) / 0 of 7,188 / 20.7 ms | **PASS** | 17.61 / 18.07 | 6.46 / 6.84 | 14.05 / 14.52 | **14.03 / 14.28** | 0 → 0, 29.7 → 30.6 °C | `-171253`, top clock `-171209` |

GPU passes at the top clock (P50, ms): Low scene 3.15, pools 0.15, background 0.14, bloom (4 levels)
2.98, afterglow 0.05, composite + threat + HUD + overlay 4.29; Medium 3.41 / 0.16 / 0.49 / 3.01 (5) /
0.05 / distortion 0.04 / 4.79; High 4.37 / 0.17 / 0.49 / 3.06 (6) / 0.05 / 0.04 / 5.82. Against PLAN §5's
12 ms: Low fits (11.05 at P95), Medium is at the line (12.21), High is over (14.28) and still holds 60 Hz
with the GPU at about 86% of the period at its top clock (no headroom left for a hotter phone or a
heavier scene). The three runs ran back to back over 40 minutes; thermal status stayed 0 (none). The v2.1 afterglow
costs 0.05 ms (W/8) where the first version cost 0.33 (Low) and 0.72 (Medium). The load in view is
up from about 500 enemies and 660 bullets this morning (Low then: 10.30 / 10.58 ms at the top clock).
No cuts were needed: Low passes as TECH_SPEC defines it.

In-app captures (`capture-android.ps1 -InApp`, upright, 2400×1080): `captures/m1/frame_reef_moto.png`
compares with the reference at mean error 1.16, luminance ratio 0.982, soft light 0.96 / 0.98 / 0.99
(desktop 1.11 / 0.982); `bench_endgame_600_moto_low.png` 28.1% dark, luminance 0.149 (the desktop's Low
frame: 28.1%, 0.149); `bench_endgame_600_moto_medium.png` 19.8%, 0.163.

First-launch tier probe on the Moto (`--tier auto`): GPU work at the top clock Low 10.81, Medium 12.10,
High 14.01, Ultra 15.99 ms (the `--gpu-burn` runs above: 10.74 / 11.99 / 14.03); it chose **Low** in 6.3 s
(Medium is 0.1 ms over the 12 ms line) and cached it (`files/tier.txt`, read with `run-as`). A first
version measured the stage's first half second (load not yet built up) and chose Medium; the probe now
pre-rolls two seconds of the stage and follows real time.

### Moto results earlier on 2026-11-11 (bench_endgame, 2400×1080, Signal, Reef, Swappy 60 Hz, cold, 30 s unless noted)

Per frame in view: about 500 enemies (max 660), 660 enemy bullets (max 870), 1,800 glyph instances,
1,700 live sparks, 3,000 capsules; 3,072 enemy slots alive. Main thread pinned to the big cores.

| Tier | Panel missed | CPU-side P95 / max (ms) | Main thread P50 / P95 (ms) | GPU total at the governor's clock P50 / P95 (ms) | GPU at the top clock P50 / P95 (ms) | Result file |
|---|---|---|---|---|---|---|
| Low | 0 / 1797 | 17.66 / 20.27 | 4.25 / 4.50 | 14.16 / 14.89 | 11.69 / 12.05 | `ZY22BXMJG3-bench_endgame-20261111-092633.json`, top clock `-093031` |
| Medium | 0 / 1797 | 17.65 / 20.51 | 5.22 / 5.53 | 13.64 / 14.02 | 13.46 / 13.68 | `-092726`, top clock `-093115` |
| High | 3 / 1794 (0.17%) | 17.62 / 33.60 | 6.10 / 6.58 | 15.64 / 16.70 | not measured | `-092820` |

GPU passes at the top clock, Low: scene 2.63, pools 0.29, background 0.14, bloom (4 levels) 2.80,
afterglow (1/4) 0.33, composite + threat + HUD 5.51 ms. Medium: scene 2.87, pools 0.53, background 0.49,
bloom (5) 2.85, afterglow (1/2) 0.72, distortion 0.04, composite 5.94 ms. Heap allocations in the
measured window: 0 on our thread in every run. These are short, cold runs on a shared phone, with the
morning's lighter load in view and the first afterglow; the contract runs are above.

Where the composite's time went (Low, top clock, by switching parts off): a constant output with the
threat layer and the overlay 2.25 ms (the overlay 0.33 of it), the pools/bloom/afterglow taps 0.8 ms,
the rest the colour maths. Then:

| Change (top clock, Low) | GPU total P50 / P95 (ms) | Composite (ms) |
|---|---|---|
| Before (above) | 11.69 / 12.05 | 5.51 |
| Tone-map reciprocals precomputed, cheaper dither slope, profiling switches removed | 11.01 / 11.38 | 4.85 |
| Pools, afterglow and background gathered into the last bloom pass | **10.30 / 10.58** | 3.85 (bloom 3.05) |

Medium at the top clock after both: 12.04 / 12.35 ms (scene 2.82, pools 0.52, background 0.49, bloom
3.17, afterglow 0.72, distortion 0.04, composite 4.26). Files: `ZY22BXMJG3-bench_endgame-20261111-093631.json`
and the two burn runs after it.

Before the background moved to W/4, Medium and High ran at 30 fps (GPU 26.5 ms, composite 16.9 ms:
eight nebula clouds evaluated per full-resolution pixel).

### Desktop validation (Debug, validation and synchronization validation on)

- This session's build (2026-11-11 17:00): `bench_endgame` 60 s at Ultra and at Low, 2400×1080: valid,
  0 validation and 0 loader messages, 0 heap allocations in the window, 2 missed of 3,600 (desktop
  timing is informational), 4 Drops and 2 boss breaks inside the run; 706 enemies and 953 / 962 bullets
  in view on average, crowd gain 0.479 (`captures/desktop_m1_endgame_ultra_60s_20261111-1651.log`,
  `..._low_60s_20261111-1653.log`). Every review picture, strip and still: 0 messages. The tier probe
  (`--tier auto`) and `--reduce-flashing`: 0 messages.
- Earlier build (morning): the same two runs clean; `lifecycle_test` 20 cycles: 0 messages (not re-run:
  no lifecycle code changed).

### Pictures for review (`captures/m1/`, 2400×1080, desktop unless noted)

Re-shot 2026-11-11 17:50 on the v2.2 build with `scripts\review-m1.ps1` (Signal only, per the user's
decision; Moto captures from 17:25, before v2.2); the
morning's set is in `captures/archive/m1-2026-11-11-morning/`. `*.small.png` are 1600 px review copies;
`light.txt` holds the `scenes.py light` line of every moving-scene picture.

- `look_card_nobloom.png`, `look_card.png`, `frame_reef.png`, each with `.cmp.png` and `.compare.txt`.
- `glyph_sheet_1.png`, `_2.png`, `_3.png` (families 1-9, 10-17, then marks, elites, the ship with every
  halo in the four player colours, a Shard and an enemy bullet).
- `bench_mid_600_signal_reef.png`, `bench_endgame_600_signal_reef.png` (and `_low.png`: the Low tier),
  `bench_endgame_600_signal_ultraviolet.png`, `_furnace.png`.
- Strips of eight consecutive frames: `strip_bench_endgame_879_0..7.png` across the first Drop (bar 8,
  frame 873) and `strip_bench_endgame_407_0..7.png` in a quiet bar; `_sheet.png` tiles each at half size.
- Moto in-app captures: see "Moto contract runs".
- Light analysis: `captures/light/` (one class off at a time, now on the v2.2 build; `afterglow_adds_x3.png`
  and `afterglow_adds_low_x3.png` are from before v2.2).
- Afterglow behind fast things: `afterglow_shot_tail_mid600.png` (v2.2, shot lanes),
  `afterglow_shot_beads_mid600_lanes.png` (the same lanes before v2.2), `afterglow_shard_beads_mid600.png`
  (v2.2, a Shard stream: still beads), `afterglow_shot_beads_mid600.png` (before v2.2, mostly that Shard
  stream).

Checked by eye against scenes.md §5's list (v2.2 build): no line-sharp ghost rows (shots leave one even
tail; Shard streams flying to the ship still leave rows of soft dots, mostly visible in `bench_mid`; the
boss's fans leave soft red smears where seven bullets spawn on one turning arm tip); the Drop strip has
no border of streaks; no beads on rings; the danger diamonds and the ship (priority layer) read over the brightest
light; the boss stand-in is violet. The Drop strip jumps in brightness at frame 873 (bloom burst, zoom
snap, white front with an RGB fringe from the aberration of its distortion ring).

### Not built in M1 (or not verified)

- Space-warp of sparks by wells (not in M1); refresh-rate steps in the governor (120 → 90 → 60; needs
  the 12R); the 13-tap bloom prefilter for High. Reduce-flashing mode and the first-launch tier choice
  are built (2026-11-11 pm); reduce-flashing has no on-device switch yet (argument or key R).
- The light targets of scenes.md §5 for `bench_endgame` are not met (above): the lead's decision.
- 12R and tablet: not attached; every M1 item for them is pending (including the surface-size check,
  the contract at 120 / 90 Hz and the in-app captures).
- Audio was still the M0 test tone: beat envelopes came from the scripted 132 BPM clock. Superseded by
  M2 (above): in real time the stage now runs on the band's heard clock; pictures keep the script.

### Waiting on the user (carried from M0 and the lead)

Audible test tone, 180° rotation, lock/unlock, the 12R's refresh setting (High) and the 12R itself, the
tablet. Decided 2026-11-11: the look is Signal; the sound variants ship as a player setting; M2 runs in
parallel on its own branch.

## M0 Bootstrap — status 2026-11-10 (evening): device checks done except the tablet and the user checks

All device numbers come from **shared phones**: other sessions use both phones (CLAUDE.md "Shared test
devices"). Each result file in `captures/bench/` carries the phone's CPU load during the run
(`device_load`), so a disturbed run can be told apart. `captures/` is not in git; the tables below are
the record.

### How a run is measured and graded (since this session)

- `bench_idle`: 5 s warm-up, then 60 s measured. `bench-android.ps1` sends no adb command to the phone
  during the measured window (see "Moto pacing" for why), samples the phone's CPU load once a second
  with shell builtins plus `grep`/`sleep`, and pulls the app's full result file.
- **Panel side (graded):** what reached the display. Plain FIFO: VK_GOOGLE_display_timing present
  times for every frame. The panel only changes image at a vsync, so intervals are counted in refresh
  periods; the raw present-fence timestamps scatter by about ±0.25 ms around the period (fence
  signalling, not display timing): on the Moto the raw P95 is 16.92-16.95 ms in runs with **zero**
  missed vsyncs, and the panel's measured period is 16.695 ms (59.9 Hz). Swappy: Swappy owns the
  extension, so its own statistics are used (frames shown one refresh period or more later than it
  asked). That counter also counts a steady extra frame of latency as "late" (seen on the 12R, below),
  so FIFO display timing is the exact measure and Swappy's is a conservative one.
- **CPU side (reported, not graded):** when the present call returned. It jitters by 1-2 ms around the
  period: on the Moto its P95 is 17.5-17.9 ms in every configuration, also with zero missed vsyncs (the
  12R at 60 Hz: 17.6-17.9 ms), so it fails 16.9 ms in every run while the panel shows every frame on time.
- Every late frame is attributed (`src/tools/frame_probe.h`): the interval split into the main
  thread's phases, its CPU time, run-queue wait (runnable but not running), context switches, page
  faults and CPU number, the worst 16 frames with their breakdown, and a per-second timeline that the
  script lines up with the phone's foreign CPU load and new processes.

### M0 exit criteria (PLAN §7)

| Criterion | Status | Evidence |
|---|---|---|
| One command builds and runs the desktop app; one command builds, installs and launches on a serial | Verified | `run-desktop.ps1`, `deploy-android.ps1` (used throughout) |
| Desktop 60 s with zero validation messages; screenshot written and inspected | Verified | `captures/desktop_validation_60s_20261110-2047.log`: 0 validation, 0 loader messages, heap 0; `lifecycle_test` 20 cycles 0 messages (`captures/desktop_lifecycle_20261110-2048.log`). Desktop screenshot inspected in the previous session; in-app phone captures inspected now |
| Moto and tablet: `bench_idle` holds the panel rate with P95 inside the contract | **Moto: met on the panel side** (FIFO display timing: 0 missed in every clean run, P95 = one refresh period, panel max 17.1-17.4 ms; Swappy: 0 late). CPU-side P95 17.5-17.9 ms does not meet 16.9 (jitter, see above): the user decides which measure counts. **Tablet: not attached, not run** | Tables below |
| 20 background/foreground cycles, 180° rotation, lock/unlock: no crash, no leak, swapchain recovers | **20 cycles verified on the Moto and the 12R** (final build). 180° rotation and lock/unlock: need the user. Tablet: pending | `lifecycle-android.ps1`: 20/20 backgrounds, 20/20 resumes presenting, 20/20 surface released in time, no problem or crash lines, on both phones |
| Test tone on both devices, zero underruns over 60 s; the user confirms it is audible | **0 underruns over 60 s on the Moto and the 12R** (final build). Audible: needs the user. Tablet: pending | `ZY22BXMJG3-bench_idle-20261110-212330.json`, `a0e622a5-bench_idle-20261110-205932.json` |

**Correction (2026-11-11, M1):** every Moto number in this M0 section was measured with a 2174×1020
surface (the window kept the system-bar and cut-out insets), not the native 2400×1080; see "Corrections
to M0 found in M1" above. The 12R is likely affected the same way and needs a re-check.

Also verified: in-app captures (`capture-android.ps1 -InApp`) upright and readable on both phones
(`captures/ZY22BXMJG3-20261110-212649-inapp.small.png`, `captures/a0e622a5-20261110-210252-inapp.small.png`).
The focus-loss validity flag fired on a genuine interruption on the desktop (window lost focus:
`"valid": false, "interruptions": 1`); no phone run was interrupted, so it is not confirmed on a phone.

### Moto results (bench_idle 60 s, Moto One 5G Ace, 60 Hz; 3 runs per row)

`os` = the scheduler places the main thread (it lives on the little cores 0-5); `big` = pinned to the
big cores 6-7 (now the default). P95/P99/max = CPU side; panel = graded measure.

| Configuration (quiet phone) | CPU P95 | CPU P99 | CPU max | CPU missed | Panel missed | Panel verdict | Worst run-queue wait |
|---|---|---|---|---|---|---|---|
| Swappy, os, overlay | 17.50-17.52 | 17.97-18.16 | 19.8-22.4 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 4.6-6.0 ms |
| Swappy, os, no overlay | 17.46-17.49 | 17.94-18.04 | 21.1-22.3 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 4.8-8.5 ms |
| FIFO, os, overlay | 17.81-17.85 | 18.84-18.88 | 22.9-39.1 | 2 / 10780 (*) | 2 / 10768 (*) | 3/3 PASS | 4.1-7.2 ms |
| FIFO, os, no overlay | 17.76-17.78 | 18.89-18.99 | 22.2-24.3 | 0 / 10782 | 0 / 10770 | 3/3 PASS | 4.7-5.1 ms |
| Swappy, big, overlay | 17.67-17.74 | 18.32-18.36 | 20.3-22.2 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 2.5-4.3 ms |
| FIFO, big, overlay | 17.59-17.62 | 18.08-18.17 | 19.7-20.8 | 0 / 10782 | 0 / 10770 | 3/3 PASS | 1.3-3.3 ms |

(*) Both misses in one run, in the second in which I sent `dumpsys activity` and `pidof` to the phone
by mistake (7 foreign process starts in that second; both were compositor-side: our frame was ready).
FIFO panel: P50 16.695-16.70 ms, raw P95 16.92-16.95 ms, panel max 17.1-17.4 ms in clean runs.

| Configuration, with the old script's adb polling (`pidof` + `logcat -d` every 2 s) | CPU P95 | CPU P99 | CPU max | CPU missed | Panel missed | Worst run-queue wait | Own-work max |
|---|---|---|---|---|---|---|---|
| Swappy, os | 17.67-17.77 | 25.2-25.4 | 39.8-43.1 | 36, 36, 36 (1.01%) | 2.84-2.90% (Swappy) | 13.6-21.5 ms | 13.2-27.5 ms |
| Swappy, big | 17.89-17.95 | 20.6-20.8 | 31.7-34.4 | 18, 15, 14 (0.39-0.50%) | 3.43-4.87% (Swappy) | 4.1-5.1 ms | 2.0-4.6 ms |
| FIFO, os | 18.35-18.42 | 28.2-33.8 | 47.0-47.9 | 42-49 (1.18-1.37%) | 28-30 (0.79-0.84%) | 8.2-12.4 ms | 9.7-17.7 ms |
| FIFO, big | 17.83-17.85 | 28.7-32.0 | 42.9-46.1 | 44-48 (1.24-1.35%) | 30-36 (0.84-1.01%) | 2.8-6.4 ms | 1.8-2.3 ms |

FIFO panel misses under the polling: 187 of 188 had our frame produced on time (normal CPU-side
interval, positive present margin): the compositor missed. All runs above stay inside the contract
on the panel side (P95 one refresh period, under 5% missed, no frame over 50 ms).

Final build, default configuration (Swappy, big): `212044` 0 missed, CPU P95 17.73, max 19.7; FIFO:
`212207` 0 missed on the panel, panel max 17.3; with `--tone`: `212330` 0 missed, 0 underruns.

Earlier runs (old build or old script), CPU side: run A (Swappy) P95 17.57, P99 19.52, max 41.7, 0.61%
missed; FIFO 15 s P95 17.87, P99 29.29, max 47.1, 1.12%; run C (Swappy + SurfaceFlinger polling every
0.7 s) P95 20.07, P99 35.13, max 44.8, 3.34% (SurfaceFlinger: 124 of 3311 missed); this session's first
baseline (old build, old script, quiet phone) P95 17.71, P99 26.03, max 43.6, 1.04%.

### OnePlus 12R (stands in for the tablet; panel held at 60 Hz by the phone's setting)

- 120 Hz: the refresh-rate vote never reached the system (`ANativeWindow_none -> -1`: `dlsym(RTLD_DEFAULT)`
  cannot see libnativewindow.so). Fixed: it is opened explicitly; now `setFrameRateWithChangeStrategy -> 0`.
  The panel still stays at 60 Hz because the phone's refresh-rate setting is Standard
  (`peak_refresh_rate=60.000004`; DisplayModeDirector vote `PRIORITY_USER_SETTING_PEAK_REFRESH_RATE`
  max 60). The 120 Hz check (P95 ≤ 8.6 ms) needs the user to set Display → Screen refresh rate → High.
- While the vote is overruled: the target and the missed count now follow the live panel rate (before:
  target 8.333 ms, "100% missed"), and Swappy's swap interval follows it too (before: Swappy aimed at
  8.33 ms presents and showed 82.5% of frames one refresh later than asked; after: 0).
- 9 runs × 60 s (os ×3, big ×3, tone ×2, earlier ×1): 0-1 CPU-side missed, CPU P95 17.58-17.88, max
  18.9-27.3 ms; panel 0 late with the fix. Tone: 0 underruns. The main thread runs on the mid cores
  (3-6) either way. The system gives the app 2376×1080: the phone's resolution setting is FHD+
  (panel native 1264×2780).
- Lifecycle: 20/20 PASS; it rebuilt the swapchain 79 times (about 4 per cycle: the launcher is portrait,
  so every cycle rotates the surface) and counted 4 audio underruns across the 20 stream reopens (the
  buffer restarts at 2 bursts and grows after an underrun); 0 underruns in steady state.

### Moto pacing (Part B): cause, evidence, changes

**Cause: foreign load on the phone, mostly from our own benchmark tooling.** The old `bench-android.ps1`
ran `adb logcat -d` and `pidof` every 2 s for the whole run and `dumpsys display` inside the window
(run C also polled `dumpsys SurfaceFlinger --latency` every 0.7 s). Each poll starts processes on the
phone and makes logd, system_server or SurfaceFlinger work. Left to the scheduler, our light main thread
lives on the little cores 0-5 (big cores 6-7 sat at 1% load), next to SurfaceFlinger, the composer and
the audio server, so a burst there makes it wait for a CPU, or delays the compositor.

Evidence:
1. Same build, same phone, quiet, script silent during the window: 0 missed in 17 of 18 runs (the 18th:
   my own two adb commands, see above). Re-adding only the old 2 s polling: 36 missed in each of 3 runs
   (1.01%), P99 25.2-25.4, max 39.8-43.1 — the same as the old-script baseline of this session (37,
   1.04%, P99 26.0, max 43.6). Every late second of those runs (26, 23, 26) had foreign process starts.
2. The intermediate script (polling `pidof` from just before the window ended): 11 of the 12 late seconds
   in its 8 runs fell at t = 57-60 s.
3. Attribution of every late frame: the main thread was runnable but waiting for a CPU (run-queue wait
   ≥ 2 ms in 97 of 108 late frames of the disturbed unpinned runs, up to 21.5 ms), or the compositor
   missed with our frame ready (FIFO: normal CPU interval, positive present margin). Never a blocking
   call outside the waits, no major page faults; own work above half a frame only when preempted
   inside it.
4. The `cpu_ms.max` lead (17.8-18.8 ms in the old runs): `cpu_ms` excludes the three waits but not
   preemption. Under the polling it returns (own-work max 13-27 ms), and those frames show 9-21 ms of
   run-queue wait inside scene build, recording or submit: preemption, not a stall in our code. Quiet:
   own-work max 2.3-7.1 ms (unpinned, 12 runs), 0.7-3.7 ms (pinned, 11 runs).

Changes (ours):
- Bench tooling: no adb traffic during the measured window, 5 s warm-up, device-load sampler (foreign
  CPU, new processes, top processes, late seconds joined with foreign activity), full result file,
  panel-side measurement, P95 grading. This removes the disturbance that produced most earlier misses.
- Main thread pinned to the non-little cores by default (`--affinity big`; `--affinity all` restores
  the scheduler's choice), re-applied after every resume. Quiet: worst run-queue wait 1.3-5.8 ms instead
  of 4.1-8.5, own work about 3× faster, audio callback worst 0.07-0.16 ms instead of 0.36-1.0 ms. Under the
  polling: CPU-side late frames halved (14-18 instead of 36) and our thread no longer preempted (worst
  4.1-5.1 ms instead of 13.6-21.5); the remaining misses are outside our thread. On the panel (FIFO
  display timing) the polling costs 0.8-1.0% of frames with or without pinning, all compositor-side, so
  at M0's idle load pinning neither helps nor hurts the panel (Swappy's late counter rose, 2.9% →
  3.4-4.9%, but it also counts latency shifts). It stays the default for the CPU headroom it gives M1
  (own work 3× faster, no preemption inside it); M1's `bench_endgame` is measured both ways.
- Not ours, unchanged: compositor-side misses under foreign load; Swappy allocates on our thread inside
  its calls (≈4 per frame) and on its own threads (≈1 per frame) — our code: 0 in every run.

Remaining risk for M1: the idle scene does not load the GPU or the CPU; a busy phone (other sessions,
background apps, adb) costs frames that no in-app change prevents, so benchmarks must keep the phone
quiet and report its load. With the main thread on the two big cores, M1's worker pool on the Moto
should start with at most one worker on the big cores (PLAN §5 "big cores minus one"). Thermals: only
idle runs so far.

### Done this session (code and scripts)

- `src/tools/frame_probe.*` (new): per-frame attribution described above.
- `src/platform/cpu_affinity.*` (new): pins the calling thread to the non-little cores.
- `src/gfx/frame_pacer.*`: vote fixed (dlopen libnativewindow.so) and recorded; target and Swappy swap
  interval follow the live panel rate; VK_GOOGLE_display_timing measurement in plain FIFO; Swappy
  statistics; allocations inside Swappy counted on our thread only.
- `src/gfx/renderer.*`: phase timestamps; Swappy frame start.
- `src/platform/app.*`: probe, affinity (applied at start and every resume), overlay graded on P95 and
  refreshed only while shown, summary `RFBENCH` line plus full result file, allocations per thread.
- `src/core/json.h` (arrays), `src/core/alloc_counter.*` (per-thread count), `src/core/args.*`
  (`--affinity`), `CMakeLists.txt` (new sources, Ninja job pool).
- Scripts: `env.ps1` (4-core limit), `build-desktop.ps1`, `build-android.ps1` (job limits, own Gradle
  daemon, `-Offline`, `-Clean`), `bench-android.ps1` (above), `lifecycle-android.ps1` (collects our log
  after every cycle: the 12R's log buffer rotates within seconds).
- PC CPU limit (user rule): see CLAUDE.md. Clean desktop Debug build 200.8 s wall, 237.4 s CPU (1.18
  cores average), 1235 processes all at mask `0x55000000`, at most 8 compiler processes (4 jobs × driver
  + cc1); clean Android build 46.8 s wall, 135.1 s CPU (2.89 cores), 286 processes all at `0x55000000`,
  at most 4 compilers, in a fresh daemon with `-XX:ActiveProcessorCount=4`.

### Not verified / needs the user

- Tablet (`R5GL362BEHX`): not attached. All tablet items pending (90/60 Hz contract, lifecycle, tone).
- 12R at 120 Hz: set Display → Screen refresh rate → High, then
  `scripts\bench-android.ps1 -Serial op12r -Scenario bench_idle`.
- 180° rotation, lock/unlock, audibility of the test tone: user checks (commands in the M0 report).
- Contract measure: the user decides whether the panel-side measure (graded here) or the CPU-side one
  counts; on the Moto the CPU-side P95 cannot reach 16.9 ms at idle.

### Deviations

- `c++_shared` (Oboe prefab), no VMA, no ImGui (lead's direction). Network permissions deferred to M4.
- `VK_LAYER_PATH` set per process: the registry lists missing SDK 1.1.92.1 layers.
- Benchmarks warm up 5 s (was 2 s); the scripts stay silent during the measured window.
- `heap_allocs` now counts our main thread outside Swappy; `heap_allocs_in_pacer` (Swappy's calls on our
  thread) and `heap_allocs_other_threads` are reported separately.
- The main thread is pinned to the non-little cores by default (measured above; `--affinity all` to
  compare).

### All runs (bench_idle; 60 s unless noted; result files in `captures/bench/`)

P50-Max = CPU side (present call), ms. Panel = graded measure: P95 in ms after counting intervals in refresh
periods, and missed %. Verdict against the P95 contract on each measure. `old script` = 2 s adb polling
during the run; `adb polling 2 s` = that polling re-added on purpose (disturbance test). The 12R row
`195918` predates the target fix (its "100% missed" is against an 8.33 ms target the panel never ran at).

| Result file | Device | Config | Panel Hz | P50 | P95 | P99 | Max | Missed (CPU) | Panel: P95 graded / missed | Verdict | Underruns | heap_allocs | Valid |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ZY22BXMJG3-bench_idle-20261110-190609` | Moto | swappy, old build, old script | 60 | 16.70 | 17.57 | 19.52 | 41.7 | 22 (0.61%) | not measured | -, FAIL cpu | 0 | 17905 | n/a |
| `ZY22BXMJG3-bench_idle-20261110-191030` | Moto | fifo, old build, old script | 60 | 16.69 | 17.87 | 29.29 | 47.1 | 10 (1.12%) | not measured | -, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-191340` | Moto | swappy, old build, old script | 60 | 16.72 | 20.07 | 35.13 | 44.8 | 118 (3.34%) | not measured | -, FAIL cpu | 0 | 1796 | True |
| `ZY22BXMJG3-bench_idle-20261110-195923` | Moto | swappy, old build, old script | 60 | 16.71 | 17.71 | 26.03 | 43.6 | 37 (1.04%) | not measured | -, FAIL cpu | 0 | 1604 | True |
| `ZY22BXMJG3-bench_idle-20261110-201854` | Moto | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.55 | 18.23 | 36.6 | 3 (0.08%) | 16.67 / 0.17% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-202741` | Moto | fifo, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.84 | 19.16 | 36.7 | 1 (0.03%) | 16.70 / 0.03% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-202903` | Moto | swappy, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.50 | 18.07 | 24.3 | 0 (0.00%) | 16.67 / 0.03% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203026` | Moto | fifo, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.79 | 18.93 | 41.6 | 2 (0.06%) | 16.70 / 0.03% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203149` | Moto | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.51 | 18.05 | 34.9 | 1 (0.03%) | 16.67 / 0.06% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203312` | Moto | fifo, os, script polled pidof from ~1 s before the end | 60 | 16.71 | 17.83 | 18.99 | 27.3 | 1 (0.03%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203435` | Moto | swappy, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.69 | 17.46 | 17.95 | 32.8 | 1 (0.03%) | 16.67 / 0.06% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203559` | Moto | fifo, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.73 | 19.08 | 45.5 | 2 (0.06%) | 16.70 / 0.06% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203911` | Moto | swappy, big | 60 | 16.71 | 17.67 | 18.32 | 19.0 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204035` | Moto | fifo, big | 60 | 16.70 | 17.60 | 18.16 | 20.1 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204308` | Moto | swappy, os | 60 | 16.70 | 17.50 | 17.97 | 22.4 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204433` | Moto | fifo, os | 60 | 16.70 | 17.85 | 18.88 | 39.1 | 2 (0.06%) | 16.70 / 0.06% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204557` | Moto | swappy, big | 60 | 16.71 | 17.67 | 18.34 | 22.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204721` | Moto | fifo, big | 60 | 16.70 | 17.59 | 18.08 | 19.7 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204845` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.46 | 17.99 | 21.1 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205009` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.78 | 18.99 | 22.2 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205133` | Moto | swappy, os | 60 | 16.71 | 17.52 | 18.16 | 19.8 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205257` | Moto | fifo, os | 60 | 16.71 | 17.84 | 18.87 | 22.9 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205422` | Moto | swappy, big | 60 | 16.71 | 17.74 | 18.36 | 22.1 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205546` | Moto | fifo, big | 60 | 16.72 | 17.62 | 18.09 | 20.4 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205711` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.49 | 17.94 | 22.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205836` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.78 | 18.89 | 24.3 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210001` | Moto | swappy, os | 60 | 16.70 | 17.52 | 18.07 | 22.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210125` | Moto | fifo, os | 60 | 16.70 | 17.81 | 18.84 | 23.5 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210250` | Moto | swappy, big | 60 | 16.71 | 17.68 | 18.32 | 20.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210415` | Moto | fifo, big | 60 | 16.70 | 17.61 | 18.17 | 20.8 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210539` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.48 | 18.04 | 21.6 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210704` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.76 | 18.89 | 23.4 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210845` | Moto | swappy, os, adb polling 2 s | 60 | 16.70 | 17.76 | 25.36 | 40.8 | 36 (1.01%) | 16.67 / 2.87% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211346` | Moto | swappy, big, adb polling 2 s | 60 | 16.71 | 17.91 | 20.57 | 34.4 | 18 (0.50%) | 16.67 / 3.43% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211509` | Moto | swappy, os, adb polling 2 s | 60 | 16.71 | 17.77 | 25.32 | 39.8 | 36 (1.01%) | 16.67 / 2.84% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211633` | Moto | swappy, big, adb polling 2 s | 60 | 16.71 | 17.89 | 20.84 | 33.4 | 15 (0.42%) | 16.67 / 4.30% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211756` | Moto | swappy, os, adb polling 2 s | 60 | 16.71 | 17.67 | 25.18 | 43.1 | 36 (1.01%) | 16.67 / 2.90% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211920` | Moto | swappy, big, adb polling 2 s | 60 | 16.70 | 17.95 | 20.72 | 31.7 | 14 (0.39%) | 16.67 / 4.87% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212044` | Moto | swappy, big | 60 | 16.70 | 17.73 | 18.42 | 19.7 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212207` | Moto | fifo, big | 60 | 16.70 | 17.60 | 18.15 | 23.8 | 0 (0.00%) | 16.69 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212330` | Moto | swappy, big, tone | 60 | 16.71 | 17.68 | 18.30 | 21.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212731` | Moto | fifo, os, adb polling 2 s | 60 | 16.70 | 18.35 | 33.75 | 47.0 | 49 (1.37%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212855` | Moto | fifo, big, adb polling 2 s | 60 | 16.71 | 17.85 | 32.05 | 44.9 | 48 (1.35%) | 16.70 / 1.01% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213019` | Moto | fifo, os, adb polling 2 s | 60 | 16.71 | 18.35 | 31.89 | 47.9 | 46 (1.29%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213142` | Moto | fifo, big, adb polling 2 s | 60 | 16.71 | 17.84 | 28.73 | 42.9 | 44 (1.24%) | 16.70 / 0.96% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213306` | Moto | fifo, os, adb polling 2 s | 60 | 16.71 | 18.42 | 28.23 | 47.3 | 42 (1.18%) | 16.70 / 0.79% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213429` | Moto | fifo, big, adb polling 2 s | 60 | 16.70 | 17.82 | 31.61 | 46.1 | 46 (1.29%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-195918` | 12R | swappy, old build, old script | 60 | 16.59 | 17.58 | 18.06 | 19.4 | 3618 (100.00%) | not measured | -, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-201900` | 12R | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.58 | 17.65 | 18.12 | 18.9 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204310` | 12R | swappy, os | 60 | 16.59 | 17.60 | 18.00 | 19.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204435` | 12R | swappy, big | 60 | 16.60 | 17.88 | 18.32 | 20.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204559` | 12R | swappy, os | 60 | 16.57 | 17.65 | 18.11 | 19.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204723` | 12R | swappy, big | 60 | 16.60 | 17.84 | 18.34 | 19.8 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204846` | 12R | swappy, os | 60 | 16.59 | 17.59 | 18.02 | 19.7 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205010` | 12R | swappy, big | 60 | 16.61 | 17.83 | 18.34 | 27.3 | 1 (0.03%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205248` | 12R | swappy, big, tone | 60 | 16.61 | 17.69 | 18.24 | 20.8 | 0 (0.00%) | 33.33 / 82.50% (Swappy) | FAIL panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205932` | 12R | swappy, big, tone | 60 | 16.59 | 17.88 | 18.32 | 19.5 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
