# RAVEFORM — Game Design

Companion to `PLAN.md`. This describes what the game is. All numbers are initial values to tune through
playtests; names of systems are canonical and used in code. Catalog entries (upgrades, species, bosses)
are a starting roster — replace or rework individual entries freely, but keep the counts, the variety
and the rules they follow. Everything here must stay original: do not borrow enemy designs, names,
layouts or music from existing games.

World units: `wu`. The camera always shows 1,152,000 wu² (1600×720 on a 20:9 phone, about 1358×849 on
the 16:10 tablet), so every device sees the same amount of world.

---

## 1. Experience goals

- **0–5 s:** launch, tap PLAY, already moving and shooting. No login, no loading bar worth noticing.
- **First 30 s:** slow tempo, a few simple shapes, the gun plays a melody when you hold fire. Ghost
  outlines show the two sticks once, then vanish.
- **Minute 1:** first Build and Drop — the screen tightens, the beat drops, a wave pours in and you are
  briefly overpowered. First upgrade choice in the calm that follows.
- **Minute 10:** hundreds of enemies, four Riffs layering the music you have built, the arena pulsing.
- **Game over:** stats, one tap to retry. Runs last 5–20 minutes.

---

## 2. Controls

| Input | Touch | Gamepad | Desktop (dev) |
|---|---|---|---|
| Move | Left half: floating stick | Left stick | WASD |
| Aim + fire | Right half: floating stick; fires while deflected | Right stick | Mouse aim, LMB fire |
| Nova | Button above the aim zone | RB / R1 | E or RMB |
| Dash | Smaller button beside Nova | LB / L1 | Space |
| Pause | Small top-centre button; system Back | Start | Esc |

Rules:

- Sticks float: the origin is where the thumb lands and follows the thumb if dragged past the stick
  radius. Dead zone 8%. Sizes are physical (dp), never a fraction of the screen — stick radius about
  10 mm, buttons at least 9 mm — so phones and the tablet feel the same. Positions and sizes are
  adjustable in settings; left/right can be swapped.
- **Auto-aim assist (on by default):** while the aim stick is idle the ship fires at the nearest threat
  on a half-rate grid. One thumb is enough to play; two thumbs are clearly stronger.
- When the right thumb leaves the stick to press Nova or Dash, firing continues along the last aim
  direction for 400 ms.
- Buttons show a clear pressed state (filled, brighter, slightly larger) and a cooldown ring.
- HUD and controls respect display cut-outs and the system gesture areas.

---

## 3. The Conductor — rhythm rules

Music runs in 4/4. The grid step is a 1/16 note (1/32 and 1/16-triplet steps exist for upgrades).
The single musical clock is the audio engine's (TECH_SPEC §4, §6).

| Rule | Statement |
|---|---|
| R1 | **Control is never quantized.** Movement, Dash and Nova respond on the frame of the input. |
| R2 | **Fire cadence is quantized.** While fire is held, the primary weapon fires on grid steps. Each shot plays the next note of the lead arpeggio, so holding fire plays the melody and releasing it mutes the lead. |
| R3 | **Riffs play themselves.** Each Riff (autonomous weapon) fires on its own repeating pattern and is one instrument layer in the mix. |
| R4 | **Enemies keep time.** Spawns land on beats after a warp-in of exactly two beats. Every ranged or dash attack is telegraphed for at least one beat and resolves on a beat. |
| R5 | **Seen when heard.** Every beat-driven visual fires at the moment its sound reaches the ear, on every device and audio route. |
| R6 | **Nova inhales, then drops.** Pressing Nova gives instant invulnerability and a pull-in effect; the blast lands on the next beat. |
| R7 | **Rhythm skill is a bonus, never a gate.** A Dash within ±60 ms of a beat is a Perfect Dash. Nothing in the game requires hitting a beat. |

---

## 4. Run structure

### 4.1 Phrase cycle

The run is an endless DJ set built from repeating cycles of four sections. The director and the music
engine move through them together, always on bar lines.

| Section | Director | Music | Screen | Player |
|---|---|---|---|---|
| **Groove** | Normal spawning to the threat budget | Drums, bass, the players' layers | Steady pulse | Farm Shards, build Chain |
| **Build** | Spawn rate ramps; a Rift may open | Snare roll, riser, filter opens; last bar the drums drop out | Colours drain toward white, grid tightens, slow zoom-in; last bar darkens and time dips to 85% | Tension |
| **Drop** | Mega-wave, mini-boss or boss enters on the downbeat | Full arrangement | Bloom burst, shockwave from every player, zoom snaps out, palette accent swaps | **Surge** for the first half, in two gears: fire grid one step faster for its first half, two steps for its second (triplets, then double time; capped at 1/32), score ×2, magnet ×2 |
| **Break** | Fodder dissolves into Shards in the first bar; nothing dangerous spawns | Pads, filtered drums | Cool, desaturated, slow drifting particles | Upgrade **Trio** appears; new-species spotlight in the last two beats |

Forms: **Short** = 8 / 4 / 8 / 4 bars (24 bars); **Standard** = 12 / 4 / 16 / 4 bars (36 bars: the
Drop is the longest part). Until play test 2 (2026-11-12) Standard was 16 / 8 / 16 / 8; the user
asked for Tracks three quarters as long.

### 4.2 Tracks

A Track is a block with its own tempo, key, palette, enemy families and boss. Track 1 uses three Short
cycles (72 bars, about 2.4 minutes) so the first upgrade arrives within 45 seconds. Later Tracks use three
Standard cycles (108 bars, about 3.4 minutes). A mini-boss arrives on the Drop of the middle cycle from Track 2 on; the
boss arrives on the last Drop. Killing the boss ends the Track, grants a bonus Trio and starts the
next Track's Break, during which tempo, key and palette cross-fade.

| Track | BPM | Key / progression | Feel | Palette | New families | Boss |
|---|---|---|---|---|---|---|
| 1 First Light | 120 | A minor: i–VI–III–VII | Warm four-on-the-floor | Ultraviolet (violet, cyan, magenta on indigo) | Drifters, Hounds, Shoal, Rams | Kaleidos |
| 2 Solar Wind | 126 | F minor: i–VII–VI–VII | Driving off-beat bass | Solar (amber, gold, teal on black-teal) | Flankers, Skitters, Caroms, Sentries | Longwave |
| 3 Reef | 132 | D dorian: i–IV vamp | Rolling techno | Reef (aqua, lime, coral on deep blue) | Moths, Brood, Blinks | The Choir |
| 4 Aurora | 138 | E minor: i–iv–VI–V (the V major) | Trance: a tune under a high pedal note, galloping bass | Aurora (green, violet, ice on night blue) | Wyrms, Tethers | Event Horizon |
| 5 Furnace | 144 | G phrygian: i7–♭IImaj7 vamp | Tribal and warm: toms, bass and lead in 3-3-2, a mallet lead | Furnace (amber, white, cyan on charcoal) | Weavers, Wells | Polyrhythm |
| 6 Zenith | 150 | C dorian: i–III–VII–IV | The peak-time anthem; chords in 3-3-4-3-3 | Zenith (white, gold, electric blue) | Mimics | Feedback |
| 7 Afterglow | 156 | D♭ major: I–V–vi–IV | The first major key; half-time drums under big soft chords | Afterglow (peach, lavender, mint on plum) | Elites carry two traits | Cadence |
| 8 Arcadia | 162 | G major: I–vi–IV–V | The arcade: square-wave octave bass, broken chords, dry | Arcadia (green, blue, magenta on black, a green grid) | — | Arpeggiator |
| 9 Mirage | 168 | B♭ lydian: I–II–vi–V over a held bass | Floating: a harp lead, a broken kick | Mirage (turquoise, sand, violet on deep teal, a sand grid) | — | Fata Morgana |
| 10 Last Light | 174 | A minor: First Light's chords and lead, note for note | Liquid drum and bass; the run comes home | Last Light (violet, gold, blue on indigo, a warm horizon) | — | Kaleidos, Reprise |
| The loop | as the Tracks | — | The encore (section 8) | — | Elite traits stack | Bosses return under the loop's ramps |

**Ten Tracks** (the user, 2026-11-12: "I want 10 tracks total, don't be afraid to experiment with
different themes, but they should all be very pleasing to the ear"). The tempo climbs six beats a
minute a Track, from 120 to 174. The themes are chosen to differ in what the ear notices first: the
mode (four minor, two modal, two major, one lydian, and the first theme again at the end), the drum
pattern (four-on-the-floor, 3-3-2 toms, half time, a broken kick, a two-step break) and what the
lead does (broken chords, a three-note cell, a pedal point, a mallet, a harp). All ten are written
(`design/music/track01.json` to `track10.json`, `design/music/README.md`) and have their palettes; Tracks 7 to 10 still need their
bosses' full pages (7.6 has the concepts) and their rosters.

**A run plays all ten, in order** (the user's call, 2026-11-12; the alternative offered was five
drawn for each run). That is about 29 minutes of music before the encore: 72 bars at 120, then nine
Tracks of 108 bars at 126 to 174, each a little shorter than the last. The loop of section 8 begins
after the last Track the game has — after Track 3 in the build of 2026-11-12, after Track 10 when the
set list is complete — so nothing in the loop may be counted from a fixed Track number.

### 4.3 What ends a run

Solo: a hit with no Shield cells left. Co-op: all players down at the same time (§10). There are no
continues. The game-over screen shows time, Track reached, score, best Chain, kills, favourite Riff,
and a single RETRY button.

---

## 5. The player

| Property | Value |
|---|---|
| Speed | 520 wu/s, near-instant acceleration, slight glide on release |
| Size | Visual radius 14 wu; hit radius 5 wu (forgiving) |
| Primary weapon | One shot per 1/16 step, 1400 wu/s, lead-arp note per shot |
| Shield cells | Start 4, up to 8 with Extra Cell; one cell comes back at every Break, more with Regen, and **Cells** dropped by enemies restore one each (section 6.1). A hit removes one, fires a panic pulse (clears bullets within 220 wu, shoves enemies), grants 2.5 s invulnerability and halves the Chain. Health is shown on the ship itself: its halo has one arc per cell |
| Nova | Start with 3, capacity 4 (up to 6 with Mods); one comes back at every Break. Rule R6. Clears bullets and non-boss enemies in view; heavy damage to bosses. **Auto Nova** (on by default, a switch under CONTROLS): a hit that would take the last Shield cell fires a Nova from the stock instead and costs no cell |
| Dash | One charge, recharges in 2 beats. 220 wu in 0.12 s, invulnerable throughout. Perfect Dash refunds half the recharge and emits a small shock ring |

**Scoring.** Score = enemy value × Chain. The Chain is a multiplier: it starts at ×1 and never goes
below it. Every enemy death drops **Shards** for each player separately
(1–5 by tier, 20 for elites, 200 for bosses); each Shard collected adds +1 to that player's Chain.
Shards drift toward a player within 90 wu and fade after four bars. Losing a Shield cell halves the
Chain. Nothing else decays it. The Chain has no cap: with the crowd it climbs by hundreds a minute
and a long run scores in the billions. That is intended (big numbers are part of the show, and one
hit costs half of it), so the score is a 64-bit number and both score and Chain are shown with
thousands separators (`X12,369`).

**Permanence.** Everything gained in a run lasts until the run ends. Nothing carries into the next run
except settings, local high scores and lifetime stats.

---

## 6. Upgrades

### 6.1 How upgrades arrive

- **Trio.** At every Break, three orbs appear on a ring of 170 wu around each player, each showing an
  icon, its name, a two-to-four-word effect and its stack pips. **A choice is never an accident**
  (user play test, 2026-11-12: orbs were taken by flying through them): the orbs cannot be taken for
  the Break's first two beats, and after that one is taken by **staying inside it for two beats**
  while its ring fills. Flying through does nothing; leaving empties the ring. The others pop. The
  ring is the clock; if none is taken, the one nearest the ship is taken at the end of the Break.
  There is no pause and no menu. Layout: `design/screens.md`, "The Trio".
- Trio options are drawn from Mods, new Riffs and Riff level-ups with weights. Guarantees: at least one
  option affects weapons; if the player has fewer than four Riffs and none was offered in the last two
  Trios, one is.
- **Fusion.** When both ingredients of a Fusion are maxed, it appears as a gold fourth orb.
- **Shard levels** (play test 2, 2026-11-12: "upgrades need to happen more often, maybe scale by
  collected Shards"). Collected Shards fill a level meter under the Chain. Each time it fills the
  player gains a level, and a Trio opens round the ship on the next bar line, wherever the fight
  is: the same three orbs, held to be taken. Level n needs 170 × 1.17^(n−1) Shards (170, 199,
  233, 272 …): the curve the bot runs settled on, giving a level about every 28 seconds in
  Track 1, 16 in Track 2 and 31 in Track 3 (the Shard income rises sevenfold from Track 1 to 2,
  so no one curve is even; the first guess of 25 × 1.22 gave a level every 9 seconds). Levels that fill while a Trio is open are banked and
  the next Trio opens on the bar line after the last one closes. A level-up Trio lasts four bars
  (the ring is the clock; the nearest orb is taken at the end). The fight does not step back for
  it, so the orbs are shelters: nothing spawns inside the ring, **a ship inside an orb's ring
  cannot be hurt**, and bodies touching that ring are shoved away. The Break's Trio stays as it
  is, free of charge. When every Mod and Riff is at its cap the orbs offer three different Boosts.
- **Cells** (health pickups; play test 2). A Cell restores one Shield cell. Kills drop them only
  while the player is below full health: about one every 40 seconds with one cell missing, about
  one every 15 seconds on the last cell (director controlled, like Boosts; at most two loose at
  once). Every elite and mini-boss drops one and a boss two, whatever the health. A Cell lasts
  eight bars and is pulled by the magnet like a Shard. Look: `design/look.md` section 6.
- **Boss kill:** an extra Trio. **Boosts** drop from enemies (roughly one every 20–30 s, director
  controlled), from Vaults and from events.
- Limits: 4 Riff slots, Riff levels 1–5, Mod caps per the table, 2 Boosts active at once (a third
  refreshes the oldest). Boost durations are in bars, so they are musically exact.

### 6.2 Mods (36) — permanent for the run, stackable

Weapon

| # | Mod | Effect per stack | Cap | Sound / look |
|---|---|---|---|---|
| 1 | Split | +2 projectiles in a tight fan: 3, 5, then 7 lanes | 3 | Adds a harmony note to each shot (3rd, 5th, octave, 7th) |
| 2 | Double-Time | Fire grid 1/16 → 1/16 triplet → 1/32 | 2 | Lead doubles in speed |
| 3 | Pierce | Shots pass through +2 enemies | 3 | Longer shots, longer note release |
| 4 | Ricochet | +2 bounces: off walls and structures, and from each enemy hit to the nearest other enemy within 300 wu | 3 | Pitch-up blip per bounce |
| 5 | Fork | On first hit, split into 2 half-damage shots | 2 | Y-shaped flash |
| 6 | Seeker | Shots curve toward targets | 3 | Slight portamento on the lead |
| 7 | Heavy Step | Every 4th shot is a slow orb that explodes | 3 | Low octave thump |
| 8 | Accent | The shot on beat 1 is a full-length piercing lance; then beats 1+3; then every beat | 3 | Accented note, white flash line |
| 9 | Echo | Each shot repeats as a ghost one beat later at half damage; then two echoes | 2 | Delay line on the lead |
| 10 | Wave | Shots travel in sine paths with a wider hit radius | 2 | Vibrato |
| 11 | Overcharge | +50% damage, larger hotter shots: the shot's head changes colour with every stack (look.md section 5, "The build on the shots") | 5 | More drive on the lead |
| 12 | Rear Guard | Also fire backward on off-beats; then sideways too | 2 | Off-beat counter-melody |
| 13 | Arc | Kills chain lightning to 2 more enemies | 3 | Zap arpeggio |
| 14 | Burn | Hits ignite for 2 beats; burning enemies ignite neighbours on death | 3 | Crackle layer |
| 15 | Shatter | Killed enemies' line segments fly out as damaging shrapnel | 3 | Glassy burst |
| 16 | Gravity Rounds | Hits tug nearby enemies toward the impact | 2 | Grid dimples at each hit |

Defence and movement

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 17 | Extra Cell | +1 Shield cell capacity and refill every cell (up to 8 cells) | 4 |
| 18 | Regen | A cell grows back by itself every 16 bars; then every 8; then every 4. The halo's next arc draws itself as it grows | 3 |
| 19 | Afterimage | Dash leaves a decoy enemies chase for 2 beats | 2 |
| 20 | Reserve | +1 Dash charge | 2 |
| 21 | Shockstep | Dash emits a damaging ring; a Perfect Dash doubles it | 3 |
| 22 | Aegis | A rotating arc blocks bullets from one side; wider per stack | 3 |
| 23 | Last Light | Taking a hit fires a free mini-Nova, once per cycle | 1 |
| 24 | Slipstream | +8% speed | 4 |

Economy

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 25 | Magnet | +100% Shard pull radius (90 wu, then 180, 270, 360, 450) | 4 |
| 26 | Prospector | 25% chance of an extra Shard per kill | 4 |
| 27 | Nova Stock | +1 Nova capacity and gain one | 3 |
| 28 | Encore | Boosts last one bar longer | 3 |
| 29 | Lucky Break | Trios offer one more option | 2 |
| 30 | Collector | Every 250 Shards spawns a Boost; then every 150 | 2 |

Rhythm

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 31 | On-Beat Crit | Shots on quarter-note beats deal ×3, ×5, ×8, flash, and burst over 80 wu | 3 |
| 32 | Syncopation | Off-beat shots pierce and deal +50% | 2 |
| 33 | Bar Line | At each bar start a laser cross flashes through your position; then diagonals; then longer | 3 |
| 34 | Crescendo | +5% damage per bar without being hit, up to +50%; then +100% | 2 |
| 35 | Kick Pulse | Every kick drum emits a knockback ring around you; later adds damage and radius | 3 |
| 36 | Afterparty | Surge lasts two bars longer and its fire grid is one step faster | 2 |

### 6.3 Riffs (12) — autonomous weapons, each a layer in the mix

| # | Riff | Pattern and effect | Levels add | Instrument layer |
|---|---|---|---|---|
| 1 | Orbitals | 3 shards orbit you, one turn per bar; they also destroy enemy bullets they touch | +2 shards each (3, 5, 7, 9, 11) | Shaker ticks as they pass 12 o'clock |
| 2 | Pulse Ring | Expanding damaging ring every beat, twice the first build's damage | Double ring; wider; heavier; a ring on every eighth | Tom / sub thump |
| 3 | Sentinel | Drone fires a burst of 3 at the nearest enemy on off-beats | Burst size; a second drone at level 3, a third at level 5 | Off-beat stab |
| 4 | Lancer | Charges 3 beats, fires a full-length beam along your aim on beat 4 | Width; every 2 beats | Chord hit |
| 5 | Minelayer | Drops a mine each bar; all detonate on the next bar's beat 1 | Clusters | Clap + sub |
| 6 | Storm Coil | Zaps nearby enemies on 1/8 triplets | Targets, radius | Pentatonic zaps |
| 7 | Swarm | A volley of homing micro-missiles each bar, staggered as a 1/32 run | Count | Rising arpeggio run |
| 8 | Singularity | Every 4 bars a gravity well ahead of you pulls for 3 beats, then detonates on beat 1 | Radius, damage | Reverse swell + boom |
| 9 | Mirror | A ghost wingman mirrored across your aim axis fires your primary at 50% | 60 / 70 / 80 / 100% | Lead doubled an octave up |
| 10 | Comet | A boomerang disc thrown along your aim each bar | Size, count | Pitch-glide whoosh |
| 11 | Sweeper | A laser arm rotates around you once per 2 bars | Length; second arm | Filtered drone that opens as it sweeps |
| 12 | Flak | On beats 2 and 4, a radial burst of pellets | Count; pellets explode | Snare accent layer |

### 6.4 Boosts (16) — timed pickups, 4 bars unless noted

| # | Boost | Effect |
|---|---|---|
| 1 | Prism | Every shot refracts into a 5-way rainbow fan; hits spawn mini-refractions. The fan is a real rainbow in saturated colour (play test 2: the first build's lanes were too pale): the centre lane keeps the player's colour, the lanes beside it sit 60° and 120° round the hue wheel to either side at lightness 0.70 and full chroma, coloured end to end (look.md section 5); the mini-refractions take their lane's hue |
| 2 | Hyper | Primary and all Riffs run at double grid |
| 3 | Supernova Rounds | Every shot explodes on impact |
| 4 | Colossus | Ship ×3 size, invulnerable, rams through enemies (2 bars) |
| 5 | Time Stretch | World at half speed, you at full; music drops to half-time (2 bars) |
| 6 | Freeze Frame | Enemies in view freeze for 2 bars and shatter in one hit, chaining |
| 7 | Halo | Absorbs up to 3 hits (until broken, at most 8 bars) |
| 8 | Magnet Storm | All your Shards on the map fly to you; kills drop double |
| 9 | Black Hole | Your next Nova becomes a 3-beat singularity that drags the screen in, then explodes |
| 10 | Chain Storm | Every beat, lightning links all enemies near you |
| 11 | Chorus | Two ghost ships fly formation and copy your shots |
| 12 | Beam | Primary becomes a continuous sweeping laser |
| 13 | Ricochet Fever | All shots gain +5 bounces and speed up per bounce |
| 14 | Confetti | Every kill bursts into 6 homing sparks |
| 15 | Fractal | Shots fork twice, recursively |
| 16 | Carousel | Six orbiting bombs; one launches outward on each beat |

### 6.5 Fusions (8)

| Fusion | Ingredients (both maxed) | Result |
|---|---|---|
| Helix Array | Split + Wave | Shots braid into a helix that sweeps a wide corridor |
| Standing Wave | Echo + Accent | Accent lances persist as lethal lines for a bar |
| Tesla Bloom | Pulse Ring + Arc | Rings arc lightning between everything they touch |
| Shard Halo | Orbitals + Shatter | Orbitals fling shrapnel streams on every kill |
| Downbeat Cannon | Lancer + On-Beat Crit | A screen-wide critical beam every beat 1 |
| Collapse Field | Minelayer + Gravity Rounds | Mines pull enemies in before detonating |
| Murmuration | Swarm + Seeker | Missiles become a persistent homing flock |
| Quartet | Mirror + Rear Guard | Three wingmen in formation covering every quadrant |

### 6.6 Design rules for upgrades

- Every upgrade must be visible **and** audible within one bar of picking it up.
- Prefer new behaviour over bigger numbers. Flat stat Mods exist but are the minority.
- **Every pick is felt at once** (user play test, 2026-11-12: "permanent upgrades should be much better").
  The first stack of any Mod or Riff at least doubles what it touches or adds a behaviour the ship
  did not have; no stack is a small percentage. The numbers of the first slice (Mods 1-4, 11, 17,
  25, 31; Riffs 1-3) were raised to this rule on that day; M5 writes the rest to the same scale.
- No upgrade may break the entity caps (TECH_SPEC §7): effects that multiply projectiles carry their own caps.
- First slice (M3): Mods 1–4, 11, 17, 25, 31, and 18 since play test 2; Riffs 1–3; Boosts 1, 2, 7, 8.

---

## 7. Enemies

### 7.1 Genome

`species = family × tier × variant` → 16 × 5 × 3 = **240 species**, plus elite **traits** on top.
Species are rows in a data table (id, family, tier, variant, health, speed, radius, score, threat cost,
glyph parameters, unlock Track and cycle). Behaviour code is per family and per attack, never per species.

- **Family** fixes the movement and the silhouette.
- **Tier I–V** scales health and speed, adds glyph complexity, and unlocks extras: tier III gains a
  secondary ability, tier V gains the family's signature move.
- **Variant** fixes the attack: **A** contact only, **B** the family's ranged or area attack,
  **C** the family's special.

### 7.2 Families (16)

| Family | Movement | Silhouette | Variant B | Variant C |
|---|---|---|---|---|
| Drifters | Wander, bounce off walls | Hexagon cell | Slow aimed shot every 2 bars | Split into two smaller on death |
| Hounds | Steer straight at the nearest player | Arrowhead | 3-shot burst on beat 1 | Frontal shield — must be flanked |
| Flankers | Approach on wide arcs, encircle | Crescents | Fire sideways while circling | Speed up as others die |
| Skitters | Sidestep out of the aim line | Kite with tails | Drop mines when dodging | Blink a short hop when shot at |
| Rams | Telegraph one beat, then dash in a line | Horned wedge | Leave a damaging wake | Rebound off walls for a second dash |
| Moths | Spiral inward around the player | Petal rings | Fire petals outward on 1/8 notes | Explode when the spiral closes |
| Shoal | Boid flocking in dozens | Tiny darts | Stream through in lines | Reform into a spear on each bar |
| Wyrms | Head leads a chain of segments | Linked segments | Segments fire sideways | Severed halves grow new heads |
| Caroms | Straight lines, reflect off walls, faster per bounce | Nested squares | Shed bullets at each bounce | Split on every third bounce |
| Blinks | Teleport on beats along a pattern, then strike | Sharp four-point star | Fire along the teleport path | Leave a decoy at each hop |
| Sentries | Stationary once placed | Gear turret | Rotating bullet pattern with a gap | Shield neighbours with a dome |
| Brood | Slow carriers | Egg sac | Hatch minis every bar | Burst into a swarm on death |
| Tethers | Linked pairs, lethal beam between | Triangle nodes and a beam | Beam sweeps as they orbit each other | Triads forming a lethal triangle |
| Weavers | Curving paths that leave lethal light trails for 2 bars | Trefoil | Close trails into loops | Trails detonate on the next bar line |
| Wells | Drift; pull players and bend shots | Concentric rings | Swallow shots, return them as a ring on beat 1 | Grow when fed, burst when overfed |
| Mimics | Mirror the player's motion or move only on beats | Figure eight | Fire when the player fires | Copy the player's last Dash |

### 7.3 Elite traits (12)

Armored (extra health) · Swift · Giant · Splitting · Volatile (explodes on death, telegraphed) ·
Regenerating · Cloaked (visible only on beat flashes) · Shielded (frontal arc) · Magnetic (steals
loose Shards) · Twin (arrives as a linked pair) · Phasing (intangible on off-beats) · Commander
(speeds up nearby enemies). Elites carry one trait at first; from Track 7 they stack.

### 7.4 Visual grammar

| Signal | Meaning |
|---|---|
| Silhouette | Family — learn it once, recognise it at every tier |
| Number of layers and vertices, brightness | Tier |
| White equipment marks: twin barrels or spikes (B), bracket arcs (C) | Variant — what it will do to you |
| Aura or outline effect | Trait |
| **Reserved danger colour** (hot pink-red, constant across palettes) and hollow-diamond shape | Anything that hurts at range: bullets, lasers, lethal trails, telegraphs |

Enemy bodies use the palette's enemy hues, chosen at run start to sit as far as possible from the
danger hue and from the colours of the players present. Names are generated: `HOUND III·B`.

### 7.5 Mini-bosses (12)

| Mini-boss | Family | Signature |
|---|---|---|
| Alpha Hound | Hounds | Calls packs that charge in waves on each bar |
| The Hive | Shoal | A dense cloud that reforms into ring, spear and wall on phrase boundaries |
| Battering Ram | Rams | Wall-to-wall charges; stun it by making it hit a wall |
| Carom King | Caroms | Splits into faster bouncers at each quarter of health |
| Moth Queen | Moths | Her court spirals in sync while she fires petals on 1/8 notes |
| Brood Mother | Brood | Hatches every bar; clear the hatchlings to expose her |
| Twin Tether | Tethers | Two heavy nodes, a beam rotating once per 2 bars |
| Weaver Prime | Weavers | Draws a closing cage of light around a player |
| Blink Star | Blinks | Teleports around a five-point star on beats, firing along its edges |
| Deep Well | Wells | Swallows shots, returns them as a ring on the downbeat |
| Sentry Array | Sentries | A ring of turrets with one rotating safe gap |
| Mirror Mimic | Mimics | Mirrors a player's movement and fires when they fire |

### 7.6 Bosses (10)

Every boss follows the same rules: its patterns are locked to the bar structure; it shows a
weak-point rhythm the player can read; it owns a music layer that changes with its health; it has three
phases; it fits inside the entity caps.

| Boss | Concept |
|---|---|
| **Kaleidos** | A huge rotating kaleidoscope wheel. Mirrored arms fire symmetric fans on 1/8 notes; shoot arms off to expose the core. A gentle teacher of "attacks land on the beat". |
| **Longwave** | An arena-length serpent whose body is a travelling sine wave. Segments are vulnerable only as they crest; the wave speeds up each phase. |
| **The Choir** | Eight linked sentries around the player trading call-and-response volleys. Each one destroyed removes its voice from the boss music and changes the pattern. |
| **Event Horizon** | A giant well that drags in enemies, bullets and the grid itself. Starve it by killing what it feeds on; it erupts on bar lines. |
| **Polyrhythm** | Two rotating emitters firing three-against-four patterns. The safe gaps align once per bar; reading the rhythm is the fight. |
| **Feedback** | Copies the players' current builds and plays their own patterns back one bar later. The stronger the build, the harder the echo. |
| **Cadence** (Track 7, Afterglow) | The boss is the chord progression. Four great bodies marked I, V, vi and IV stand round the frame; the one whose chord the band is playing is silent and open, the other three fire slow fans in half time, on one and three. The player follows the harmony round the room a bar at a time. Later they change places on the bar line, then the chords come twice as fast. |
| **Arpeggiator** (Track 8, Arcadia) | The bullets are the tune. Four lanes cross the frame and the lead's broken chord runs down them as blocks, lane 1-2-3-4-3-2-3-4, so the player weaves the shape of the arpeggio they have been firing. The next bar's chart shows above the lanes; the machine at the top opens as it loads each bar. Later the lanes turn a quarter, then two charts cross. |
| **Fata Morgana** (Track 9, Mirage) | The safe place moves. A storm closes the frame and the only calm is an oasis that drifts; the boss hides in the storm wall and shows where the wall thins. Then there are two oases and one is a mirage that evaporates on the snare; then the oasis leaps on the broken kick's three hits and the storm parts for a beat to let the player cross. |
| **Kaleidos, Reprise** (Track 10, Last Light) | The first boss comes back as the first theme does. Its own fans, now on the two-step break; then a medley, each arm playing one earlier boss's idea (a travelling wave, a call and its answer, a pull, three against four, an echo), to be shot off one by one; then the bare core fires First Light's arpeggio as its last pattern while the lattice turns to Ultraviolet for the encore. |

The four for Tracks 7 to 10 are concepts (2026-11-12, when the set list grew to ten); their full
pages under `design/bosses/` come before M5 batch 4. Each is built on something no earlier boss or
mini-boss asks of the player: following the harmony, reading the melody as a pattern, keeping up
with a safe place that moves, and recognising six fights inside a seventh.

---

## 8. Director and difficulty

- **Threat budget.** Each bar has a budget `B = base(track, cycle, section) × players × assist`. Every
  species has a threat cost; the director spends the budget on **formations** rather than single
  spawns: ring around a player, edge sweep, corner swarms, pincer, spiral rain, Rift bursts, escort
  around an elite, closing wall.
- **The crowd and the threats** (2026-11-12, after the first bot runs showed 10–20 enemies alive
  where this document promises hundreds). The threat budget above buys only the *threats*: hunters
  and specials, the species that can hurt a careless player. The *crowd* is fodder (Drifters,
  Shoals, Caroms and their later kin) and follows a **population target** instead: the number of
  enemies the director keeps near the ships, by Track and cycle (`design/species.json`,
  `firstSlice.crowd`). Each bar the director counts the enemies **in and just around each player's
  frame** (the camera's view grown by a quarter each way) and warps in fodder formations (floods
  of 16–80 bodies along a frame edge, rings just outside the frame, swarms at its corners) to
  close the gap over the next two bars. The target is what the player sees: the first build
  counted a 1,500 wu circle, five times the frame, and the screen stayed a fifth as full as the
  numbers said. A strong build therefore gets more to kill, more Shards and a louder song, and the
  danger still rises only with the threat budget. Fodder left two frames behind every ship for
  two bars dissolves without Shards and is replaced at the frame.
- **Sections** scale the budget and the population target: Groove 1.0, Build ramps 1.0 → 1.6 (crowd
  1.0 → 1.3), Drop 2.0 then decays to 1.2 (crowd 1.6 → 1.2), Break 0.
- **Debuts.** New species unlock on a schedule by Track and cycle. Each debut gets a spotlight at the
  end of a Break: a small group alone, with its name on screen, before it joins the pool.
- **Assist.** After a lost Shield cell the budget eases 10% for one cycle; after two clean cycles it
  rises 10%. Bounds 0.8–1.3.
- **How hard: easy, a power fantasy** (the user's call after playing, 2026-11-12). The screen is
  full and the player wins. The crowd targets stay as they are or rise; what gives is the danger.
  Targets for the bots over 20 seeds of 30 minutes: the careful bot's median first death at minute
  15 or later, Track 3 reached in at least 70% of its runs and no more than one boss fight in ten
  lost; the careless bot's median first death at minute 4 or later and Track 2 reached in at least
  half of its runs. Levers, in this order: the upgrades' strength (section 6), the cells and Novas
  that come back at each Break (section 5), then the threat budget and the bosses' bullet density,
  neither below 0.7 of the first slice's numbers (both are at 0.7 since play test 1). Enemy counts are not a lever.
  The first boss of a run has 0.7 of its health: it is the teacher. The careless bot never dodges and
  uses neither Nova nor Dash, so it is a floor, not a player: Auto Nova and the longer invulnerability
  after a hit (section 5) are for the people it stands for.
- **Fairness.** No spawn within 260 wu of any player or inside the central third of any player's view;
  every spawn warps in for two beats; no off-screen ranged attack without an edge indicator.
- **Caps.** The director never exceeds the entity caps. When the budget exceeds what the caps allow, it
  spends the remainder on higher tiers and elites instead of more bodies.
- **Endless.** From Track 7, tiers shift upward, elites gain stacked traits, and enemy health and speed
  rise 6% per Track. Tempo rises to its cap, then holds.
- **The loop is where a run ends** (2026-11-12: with Shard levels, Cells and Regen neither bot
  died in 30 minutes, and a run that cannot end is not a run). The first pass through the Tracks
  is the power fantasy and stays as tuned. Every Track after it multiplies the threat budget and
  the bosses' bullet density by 1.3 and enemy health by 1.25, and stretches the Cell clocks by
  1.25, each compounding. Targets over 20 seeds of 40 minutes: the careful bot's median first
  death between minutes 18 and 30, the careless bot's between 9 and 16; in the first pass the
  careful bot loses no run and the careless bot at most one in five. Those three
  factors alone never reached the ship (the bots lived 40 minutes at 1.5 each: the build kills
  everything first and Regen, Cells, the Breaks' refills and Auto Nova outrun the hits), so the
  loop also takes away what keeps the ship alive and speeds up what hunts it. From the first
  Track of the loop: **the Breaks no longer give back a cell or a Nova**; Regen's periods stretch
  by 1.5 for each Track of the loop; enemy speed rises 6% a Track (at most × 1.6) and enemy
  bullet speed 6% a Track (at most × 1.5, so a pattern can still be read); every shooter's bullet
  count rises 15% a Track (at most × 3, inside the pools). The loop's first Track card says so:
  `ENCORE` above the Track's name, and under it `NO MORE REFILLS`.
  Even all of that at the top of its range did not end a run (the bot runs of 2026-11-12: a
  careful player is hit about once a Track, in the boss fight, whatever the ramp, and on the
  last cell the Cells came faster than the hits). So the loop ends a run with a clock the
  player can see. **The halo burns down:** on the first downbeat of every Track of the loop
  the ship loses two cells of capacity (never below one), the arcs shattering outward on the
  beat, and the Track card's last line reads `HALO 6`, then `HALO 4`, `HALO 2`, `HALO 1`. And
  **no more refills** means all of them: in the loop no kill drops a Cell, Regen stops, and the
  Breaks give nothing back. What is left is the Cell from a mini-boss, the two from a boss, a
  Halo Boost, and the Novas in stock. The power fantasy is the first pass; the encore is how
  long the player can hold on to it. Enemy counts still are not a lever.
  **Measured and accepted** (the lead's ruling, 2026-11-12; 20 seeds of 40 minutes): the careful
  bot's median first death is at minute 32, in Track 12, and the careless bot's at minute 22, in
  Track 8 (the halo is down to one cell from Track 7, about minute 19); neither dies in the first
  pass. That is later than the targets above, which were set before the burn existed, and every
  lever has been tried at the top of its bounds. The targets are now what was measured: the
  careful bot between minutes 26 and 36, the careless bot between 17 and 25, nobody in the first
  pass; a change that moves either median out of its band needs a reason. Whether an encore of
  ten to twenty minutes is too long is the user's to say at the next play test. If it is, the
  lever is the clock itself (the loop's Tracks play two cycles instead of three, so the halo
  burns down a third sooner), not more damage.
- **The ten-Track run** (2026-11-12). The numbers above were measured on a pass of three Tracks.
  With all ten in a run the encore begins at Track 11, about half an hour in, and half an hour
  without danger would be flat: from Track 4 on the first pass has to climb. Targets to tune toward
  as the Tracks arrive (bots, 20 seeds, hits counting): the careful bot finishes the ten in at least
  seven runs of ten and its median first death is in the encore; the careless bot's median first
  death is between Tracks 6 and 9, and no more than one of its runs in five ends before Track 4.
  The first three Tracks stay as tuned. Levers for Tracks 4 to 10, in this order: what each Track's
  own families, mini-boss and boss ask of the player; the threat budget and the crowd target
  (`design/species.json`, a step a Track as Tracks 1 to 3 have); enemy tiers. Not enemy health alone,
  and never the refills: Cells, Regen and the Breaks stay generous until the encore takes them away.
- **Co-op.** Budget × (1 + 0.6 × (players − 1)); elite, mini-boss and boss health × (1 + 0.5 × (players − 1)).

Tune with the offline simulation: bot players run 30 minutes headless and the tool charts budget,
entity counts, kills per second and damage taken per cycle.

---

## 9. Arena

- **Size:** 9600 × 5400 wu, bounded by glowing walls — about 45 screens. The warping grid covers it.
- **Camera:** follows the player with look-ahead along velocity (0.125 s of travel: 65 wu at full
  speed) and a small bias toward the aim direction (30 wu). Both are half the first build's, which
  swung the view too far on every change of direction (user play test, 2026-11-12); zoom breathes between 0.85× and 1.25× with enemy density and boss fights. Each device has
  its own camera. **Boss fights:** while a boss lives within 1,800 wu of the ship the camera holds
  0.85× and leans toward the boss by 30% of the offset to it (at most 320 wu), so the boss is in the
  frame whenever its bullets are.
- **Indicators:** edge arrows for elites, bosses, teammates, events and incoming ranged attacks; an
  optional corner radar.

Structures (a handful per run, placed by seed, changing with the Track):

| Structure | Behaviour |
|---|---|
| Pylon | Pulses every bar. Stand in its ring for 4 beats to tune it: it then fires a damaging pulse every bar for a cycle |
| Rift | An enemy portal that opens during Groove and Build. Destroy it to stop the flow and drop a Boost |
| Vault | An armoured crystal. Break it for a Boost and Shards |
| Bumper | Bounces ships and shots; charges when hit and discharges on the beat |
| Current | A flow lane that speeds up anything inside it, enemies included |
| Prism | A line obstacle that splits player shots passing through it into three |

Events (scheduled by the director, announced by an edge indicator; they give reasons to cross the map
and to split up in co-op): Rift Surge · Vault Drop · Elite Hunt · Comet (a fast neutral object trailing
Shards) · Eclipse (lights out for 8 bars; enemies show only on beat flashes) · Meteor Rain
(beat-synced, telegraphed impacts) · Convoy (slow carriers with heavy loot) · Storm Cell (a roaming
lightning field).

---

## 10. Co-op (2–4 players, LAN)

- One shared arena, swarm and musical clock. Each device shows its own player's view.
- **Loot is instanced.** Every death drops Shards for every player; Boost orbs and Trios exist once per
  player. Nobody can steal anything. Chains are individual; the team score is the sum, and the game-over
  screen ranks the players.
- **Down and revive.** A hit at zero cells shatters the ship into a beacon. A teammate standing in the
  beacon's ring for two beats revives it with one cell; otherwise it revives at the next Break. A
  revived player keeps every upgrade and loses half their Chain. The run ends when all players are down
  at once.
- No friendly fire. Enemies split their attention by distance with some stickiness, so a lone player
  far from the group still gets company.
- **The band.** The shared bed of the music (drums, bass, pads) is identical on every device; each
  device plays its own player's layers loudest and the others quietly by distance. Players have fixed
  colours and lead timbres: P1 cyan / saw pluck, P2 lime / square pluck, P3 violet / bell, P4 gold / stab.
- Pause: any player may pause; it applies to everyone on the next beat.
- Joining happens in the lobby. If the host leaves, the run ends and scores are kept.

---

## 11. Audio-visual language

### 11.1 Pulse map — what reacts to what

| Musical event | Reaction |
|---|---|
| Kick | Grid ripple from each player; exposure/bloom pump (+6–10%); 0.5% camera zoom punch; enemy glyphs swell 5–8%; light haptic tick |
| Snare / clap (beats 2, 4) | Arena border flash; brief chromatic aberration; background sparkle burst |
| Hats (1/8, 1/16) | Fine grid shimmer; enemy inner layers step-rotate; Chain counter flickers |
| Bass notes | Background hue and brightness follow the bass envelope; border thickness breathes |
| Lead notes (player shots) | Muzzle flash; ship pulse; shot hue shifts within the player's colour by pitch |
| Bar line | Palette accent rotates; radar sweep |
| Build | Saturation drains toward white; vignette tightens; grid stiffens; slow zoom-in; last bar: silence, dim, 85% time |
| Drop | Bloom burst; grid shockwave from every player; zoom snaps out; particle confetti from the walls; Surge auras; heavy haptic |
| Break | Cool desaturated palette; low-pass on the music; slow drifting motes |

### 11.2 Look

- Everything in the world is **light on black**: thin, hot, anti-aliased vector lines with HDR cores
  that bloom, velocity-stretched sparks, afterglow trails, and a spring-mass grid that every explosion,
  well and kick drum bends.
- Palettes are defined in OKLCH with fixed roles: background (two tones), grid, enemy hues (five candidates, three used per run — §7.4),
  accent, plus the constants — danger colour, player colours, white-gold for pickups.
- Very hot cores go white; the colour lives in the halo.
- The numbers, the palettes and every shape live in `design/look.md`, `design/palettes.json` and
  `design/glyphs.json`, with reference images in `design/previews/`.

### 11.3 Readability rules

1. Enemy bullets, lasers, lethal trails and telegraphs are redrawn on a **threat layer** after tone
   mapping, with a thin dark rim, so no amount of bloom or particles can hide them.
2. The danger colour and the hollow-diamond bullet shape are used for nothing else.
3. Particles within 120 wu of the local player are dimmed by half.
4. The local player's hit point is always marked by a small bright dot.
5. Screen shake is capped at 6 px (reference 1080p) and never moves the HUD. Hit-stop is at most 50 ms
   and is used only for boss phase breaks and the player's own hits. Slow-motion is reserved for the
   Build's last bar, Time Stretch and the final hit of a run.

### 11.4 Reduce-flashing mode (first-launch notice plus a setting)

Clamps full-screen luminance changes to at most three per second and limits their size, disables
strobing and the Drop's white burst, halves bloom pump and shake, and slows colour cycling. Saturated
red flashes are avoided in every mode.

### 11.5 Haptics

Light tick on kicks (optional), medium on own hits and Perfect Dash, heavy on Nova and Drop. One call
per event at most, rate-limited, off when the system setting says so, strength adjustable.

---

## 12. UX flow and settings

Flow: logo sting (≤ 2 s, skippable) → title with the bot playing in the background → **PLAY** /
**CO-OP** / settings → run → game over → **RETRY**. Co-op: HOST or JOIN → lobby (players, colours,
room code) → run.

Settings (few, plain, each with a visible pressed state). On screen they are three tabs: SOUND,
CONTROLS and LOOK (video and accessibility together); layout in `design/screens.md`:

| Group | Options |
|---|---|
| Controls | Auto-aim assist; stick size and position; swap sides; button layout; gamepad mapping |
| Audio | Master, music, effects; **sound**: Club / Arcade / Velvet (three characters of the same music, `design/music/variants.json`; Club is the default); **sync calibration** (flash-and-click slider); haptics on/off and strength; in co-op: "music from host only" |
| Video | Effects: Auto / Low / Medium / High / Ultra; frame-rate target: Auto / 60 / 90 / 120; reduce flashing; shake amount; vivid colour (where supported) |
| Accessibility | Colour-blind-safe palettes; high-contrast threats; UI scale |

Local high scores: top 10 for solo and for each co-op player count. Lifetime stats screen.

---

## 13. Content targets by milestone

| | Species | Families | Mods | Riffs | Boosts | Fusions | Tracks | Mini-bosses | Bosses | Structures / events |
|---|---|---|---|---|---|---|---|---|---|---|
| M3 slice | 12 | 6 | 8 | 3 | 4 | 0 | 2 | 0 | 1 | walls only |
| M5 batch 1 | 80 | 11 | 16 | 6 | 8 | 2 | 3 | 4 | 3 | 3 / 3 |
| M5 batch 2 | 160 | 14 | 26 | 9 | 12 | 5 | 5 | 8 | 5 | 5 / 6 |
| M5 batch 3 | 240+ | 16 | 36 | 12 | 16 | 8 | 6 | 12 | 6 | 6 / 8 |
| M5 batch 4 | 240+ | 16 | 36 | 12 | 16 | 8 | 10 + the loop | 12 | 10 | 6 / 8 |

Batch 1 has three bosses, not two, so that each of its three Tracks ends in its own (the user's
note of 2026-11-12: more variety in Tracks, bosses and mini-bosses). In every batch the bosses,
mini-bosses and Tracks are built first.

**The four batches are one delivery** (the user, later the same day: "all of the rest of the content
should go in the next batch. All of it"). The rows above are the order of work; the next build to be
played has the last row.

Quality bar for every batch: each new thing is recognisable in a contact-sheet screenshot, audible in
an offline render, survives the bot harness, and does not move the benchmark numbers.
