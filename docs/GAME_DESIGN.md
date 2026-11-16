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
  at **half rate**: every second step of the fire grid, half of whatever the build's rate is (the
  owner, 2026-11-14: "Auto aim needs to shoot half speed like before"; play test 3 had put it on the
  full grid for a day), always the off-beat's step; on the 1/32 grid and faster it skips one step in four
  instead (2026-11-15: every other step of 1/32 was 4 shots a beat, the same as the triplet grid's, so
  Double-Time's second rank, the Drop's second Surge gear and Afterparty gave the assist nothing). So 2,
  4 and 6 shots a beat as Double-Time grows (`core/lead_rule.h`). A thumb on the aim stick fires at the full rate; Riffs, Novas and everything
  that fires by itself are not the assist and keep their own rate. In a boss or mini-boss fight
  the assist aims at the boss: its open weak point when it has one, else its nearest part; an
  ordinary enemy within 250 wu of the ship comes first. One thumb is enough to play **and to finish
  the run** (7.6, "Beatable with the assist alone"); a second thumb chooses what dies first.
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
| **Break** | Fodder dissolves into Shards in the first bar; nothing dangerous spawns | Pads, filtered drums | Cool, desaturated, slow drifting particles | New-species spotlight in the last two beats. No Trio (2026-11-14, the owner: "Remove The Break's Trio"), except a boss's in the Break that follows its death |

Forms: **Short** = 8 / 4 / 8 / 4 bars (24 bars); **Standard** = 12 / 4 / 16 / 4 bars (36 bars: the
Drop is the longest part). Until play test 2 (2026-11-12) Standard was 16 / 8 / 16 / 8; the user
asked for Tracks three quarters as long. **Play test 3 (2026-11-13): "tracks are still too long ...
lets try 0.5x". Every Track now uses the Short form and Standard is retired.** And on 2026-11-14:
"tracks should be the same length as track 1": every Track is two Short cycles.

### 4.2 Tracks

A Track is a block with its own tempo, key, palette, enemy families and boss. **Every Track is two Short
cycles, 48 bars** (the owner, 2026-11-14: "tracks should be the same length as track 1"; Tracks 2 to 10
were three): 105 seconds at Track 1's 110 BPM, 100 at 115, 74 at Track 10's 155, before its boss. The first upgrade
arrives within 45 seconds. A mini-boss arrives on the first cycle's Drop from Track 2 on; the boss
arrives on the second, the last. A Track's Trios are its Shard levels and its boss's (the
cycle's Break deals none since 2026-11-14). What was
written in bars for the longer Tracks keeps its bars unless said otherwise (elites come as many bars
apart as before, so half as many a Track); an arena event never outlasts its Groove. Killing the boss ends the Track, grants a bonus Trio and starts the
next Track's Break, during which tempo, key and palette cross-fade.

| Track | BPM | Key / progression | Feel | Palette | New families | Boss |
|---|---|---|---|---|---|---|
| 1 First Light | 110 | A minor: i–VI–III–VII | Warm four-on-the-floor | Ultraviolet (violet, cyan, magenta on indigo) | Drifters, Hounds, Shoal, Rams | Kaleidos |
| 2 Solar Wind | 115 | F minor: i–VII–VI–VII | Driving off-beat bass | Solar (amber, gold, teal on black-teal) | Flankers, Skitters, Caroms, Sentries | Longwave |
| 3 Reef | 120 | D dorian: i–IV vamp | Rolling techno | Reef (aqua, lime, coral on deep blue) | Moths, Brood, Blinks | The Choir |
| 4 Aurora | 125 | E minor: i–iv–VI–V (the V major) | Trance: a tune under a high pedal note, galloping bass | Aurora (green, violet, ice on night blue) | Wyrms, Tethers | Event Horizon |
| 5 Furnace | 130 | G phrygian: i7–♭IImaj7 vamp | Tribal and warm: toms, bass and lead in 3-3-2, a mallet lead | Furnace (amber, white, cyan on charcoal) | Weavers, Wells | Polyrhythm |
| 6 Zenith | 135 | C dorian: i–III–VII–IV | The peak-time anthem; chords in 3-3-4-3-3 | Zenith (white, gold, electric blue) | Mimics | Feedback |
| 7 Afterglow | 140 | D♭ major: I–V–vi–IV | The first major key; half-time drums under big soft chords | Afterglow (peach, lavender, mint on plum) | Elites carry two traits | Cadence |
| 8 Arcadia | 145 | G major: I–vi–IV–V | The arcade: square-wave octave bass, broken chords, dry | Arcadia (green, blue, magenta on black, a green grid) | — | Arpeggiator |
| 9 Mirage | 150 | B♭ lydian: I–II–vi–V over a held bass | Floating: a harp lead, a broken kick | Mirage (turquoise, sand, violet on deep teal, a sand grid) | — | Fata Morgana |
| 10 Last Light | 155 | A minor: First Light's chords and lead, note for note | Liquid drum and bass; the run comes home | Last Light (violet, gold, blue on indigo, a warm horizon) | — | Kaleidos, Reprise |
| The loop | as the Tracks | — | The encore (section 8) | — | Elite traits stack | Bosses return under the loop's ramps |

**Ten Tracks** (the user, 2026-11-12: "I want 10 tracks total, don't be afraid to experiment with
different themes, but they should all be very pleasing to the ear"). The tempo climbs five beats a
minute a Track, from 110 to 155 (the owner, 2026-11-14: "the game should start at 110 bpm and scale up
slightly slower"; six a Track from 120 to 174 until then). A fight keeps the seconds it was tuned to: a
boss's and a mini-boss's health follow their Track's tempo down (the players' fire is on the grid, so
their damage a second does too). The loop holds Last Light's tempo, as it held 174 before. The themes are chosen to differ in what the ear notices first: the
mode (four minor, two modal, two major, one lydian, and the first theme again at the end), the drum
pattern (four-on-the-floor, 3-3-2 toms, half time, a broken kick, a two-step break) and what the
lead does (broken chords, a three-note cell, a pedal point, a mallet, a harp). All ten are written
(`design/music/track01.json` to `track10.json`, `design/music/README.md`) and have their palettes; Tracks 7 to 10 still need their
bosses' full pages (7.6 has the concepts) and their rosters.

**A run plays all ten, in order** (the user's call, 2026-11-12; the alternative offered was five
drawn for each run). That is about 14 and a half minutes of music before the encore (13 at the first tempi), plus the
fights (19 until 2026-11-14, 29 until play test 3): ten Tracks of 48 bars at 110 to 155, each a little
shorter than the last. The loop of section 8 begins
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
| Shield cells | Start 4, up to 8 with Extra Cell; one cell comes back at every Break, more with Regen, and **Cells** dropped by enemies restore one each (section 6.1). A hit removes one, fires a panic pulse (clears bullets within 220 wu, shoves enemies), grants 2.5 s invulnerability and halves the Chain. Health is shown on the ship itself: its halo has one arc per cell. **Nobody meets a boss on their last cell** (M5, 2026-11-13): as a boss arrives, a player with fewer than three cells grows back up to three (two at first; M6, 2026-11-14: with the crowd eased the novice's runs ended in the fights, 32 of 41 run-ending hits), the arcs drawing themselves over its entrance bar as Regen's do. And **a boss pays for its phases**: at each phase change it drops one Cell for each player, at the boss, drifting to them. The novice bot went into the Longwave fights it lost with 1.2 cells on average and into those it won with 3.1 |
| Extra Life | (Play test 3: "a revive powerup to grant a 1x revive to continue without starting over".) A gold ship-shaped orb, a pickup of its own kind and not a Boost: it takes no Boost slot and is never offered in a Trio. Since 2026-11-15 (the owner: "An Extra Life every Track instead of every other"; from 2026-11-14 it was every other Track, 1, 3, 5, 7 and 9) one drops in every Track, for each player holding fewer than two: in Track 1 from a kill during the first cycle's
Drop, from Track 2 from the mini-boss as it dies or warps out (it shares that Drop since every Track is two cycles); it lies for 8 bars with a gold edge chevron while off screen. A player holds at most two, each a small gold ship beside the Nova stock. (Until then one dropped in every Track but only for a player holding none, and a mini-boss that warped out dropped none, so one kept unspent stopped every later drop.) It is spent on the hit that would end the run (after Auto Nova): the ship comes back at once with two cells, a free Nova blast, one bar of invulnerability and half its Chain, under the words `EXTRA LIFE`. Co-op: each player their own; it is spent instead of going down. None drop in the loop; one held is carried in |
| Nova | Start with 3, capacity 4 (up to 7 with Nova Stock's three ranks); one comes back at every Break and with every Shard level (the owner, 2026-11-15: "leveling up via shards should also give a nova"), up to the capacity. Rule R6. Clears bullets and non-boss enemies in view; heavy damage to bosses. **Auto Nova** (on by default, a switch under CONTROLS): a hit that would take the last Shield cell fires a Nova from the stock instead and costs no cell |
| Dash | **Two charges** (the owner, 2026-11-15: "just remove the dash mods entirely and buff the base dash"; one until then), each coming back 2 beats after the last. 220 wu in 0.12 s, invulnerable throughout. **Every Dash ends in a shock ring** that hurts what it reaches (140 wu, 3 damage: Shockstep's first rank, the Dash's own now; a Perfect Dash doubles the damage). Perfect Dash refunds half the recharge |

**Scoring.** Score = enemy value × Chain. The Chain is a multiplier: it starts at ×1 and never goes
below it. Every enemy death drops **Shards** for each player separately
(1–5 by tier, 20 for elites, 200 for bosses); each Shard collected adds +1 to that player's Chain.
Shards drift toward a player within 90 wu and fade after four bars. **A Shard is never lost to the
crowd** (the owner, 2026-11-14: "if too many shards are on screen it glitches out and they timeout or
something and dissapear"): while more than 1,536 are out, the loose ones older than a bar **merge** with
their player's others within 60 wu into one Shard worth them all, left where it lies to be collected, and
a death drops one Shard worth all of its own. A Shard's worth shows: bigger and golder as it grows (2: a
third bigger, 8: twice the size and gold, more: towards white, with a slow pulse). (The owner, 2026-11-15:
until then the old ones flew to their player, "a free magnet storm"; before 2026-11-14 they went out, all
of them at once.) Losing a Shield cell halves the
Chain. Nothing else decays it. The Chain has no cap: with the crowd it climbs by hundreds a minute
and a long run scores in the billions. That is intended (big numbers are part of the show, and one
hit costs half of it), so the score is a 64-bit number and both score and Chain are shown with
thousands separators (`X12,369`).

**Permanence.** Everything gained in a run lasts until the run ends. Nothing carries into the next run
except settings, local high scores and lifetime stats.

---

## 6. Upgrades

### 6.1 How upgrades arrive

- **Trio.** Three orbs on a ring of 170 wu around each player, each showing an icon, its name, a
  two-to-four-word effect and its stack pips. A Trio comes with each **Shard level** (below) and with
  each **boss's death** (in the Track-change Break that follows). **The cycle's Break deals none**
  (2026-11-14, the owner: "Remove The Break's Trio"; with Shard levels it was one Trio too many).
  **A choice is never an accident** (user play test, 2026-11-12: orbs were taken by flying through
  them): the orbs cannot be taken for their first two beats, and after that one is taken by
  **staying inside it for two beats** while its ring fills. Flying through does nothing; leaving
  empties the ring. The others pop. The ring is the clock; if none is taken, the one nearest the
  ship is taken when it runs out (a level's four bars, the boss's Break). There is no pause and no
  menu. Layout: `design/screens.md`, "The Trio".
- **How a Trio is dealt** (M5, 2026-11-13: with all 36 Mods and 12 Riffs in an even draw a build
  spread thin, and the bots killed so slowly that the bosses' health had to be cut to a third).
  Every Mod and Riff can be offered from the first Trio of a run: nothing is locked behind a Track.
  - **The first Trio of a run offers Split and Double-Time** (the owner, 2026-11-14), the third orb
    drawn as the other orbs are (never a third staple twice). In co-op, each player's first Trio.
    After it the rules below hold.
  - **The first orb is firepower.** It is drawn from what makes the build the player has hit
    harder: the weapon and rhythm Mods they hold, the Riffs they hold, and the four staples (Split,
    Double-Time, Overcharge, On-Beat Crit) whether held or not; whatever is at its cap is left out.
    A player can always choose to hit harder. When none of these can grow it is drawn like the others.
  - **The other orbs are the whole pool**, drawn by weight: a Mod not yet held 1, a Mod held and
    under its cap 3, a new Riff 3 (none once four are held), a level of a held Riff 3. Extra Cell and
    Regen count 3 from the start: they are what keeps a careless player alive. An ingredient of a
    Fusion whose other ingredient the player holds counts double. The first orb uses the same
    weights. No two orbs of a Trio are the same upgrade.
  - With fewer than four Riffs and none offered in the last two Trios, one of the other orbs is a
    new Riff (as before). Lucky Break's extra orbs are drawn like the other orbs.
  - What the weights are for, and what they gave (M5 part 3, twenty runs of each bot): a build
    deepens (the careful bot has 5.2 Mods at their cap when Track 3 ends, two or more in every run)
    and a Fusion is reachable (it held one at the end of Track 5 in every run that got there). With
    Extra Cell and Regen at 1 the careless bot grew back 5 cells in twenty runs, against 107 in play
    test 2, and died in the first three Tracks in 9 runs of 20; at 3, in 1 of 20.
- **Fusion.** When both ingredients of a Fusion are maxed, it appears as a gold fourth orb, above the ring (screens.md
  "The Trio"; built 2026-11-15). Every orb that is half of a Fusion not yet taken names the other half, its rank and the
  Fusion (`+ WAVE 1/2 = HELIX ARRAY`), so a player can build toward one (the owner, 2026-11-15: "The game never shows
  which upgrades combine"). The pace stays as it is (the owner: "They come too late. Its fine, no change needed").
- **Shard levels** (play test 2, 2026-11-12: "upgrades need to happen more often, maybe scale by
  collected Shards"). Collected Shards fill a level meter under the Chain. Each time it fills the
  player gains a level, and a Trio opens round the ship on the next bar line, wherever the fight
  is: the same three orbs, held to be taken. **Each level also gives a Nova** at once, up to the
  capacity (the owner, 2026-11-15; none in the loop, where the Breaks give none either: section 8;
  the HUD's newest Nova star swells and flashes gold). The first level needs **100** Shards (the owner,
  2026-11-14: "shards needed for the 1st level powerup should be less"; it needed 170), level n after
  it 170 × 1.17^(n−1) (199, 233, 272 …): the curve the bot runs settled on, giving a level about every 28 seconds in
  Track 1, 16 in Track 2 and 31 in Track 3 (the Shard income rises sevenfold from Track 1 to 2,
  so no one curve is even; the first guess of 25 × 1.22 gave a level every 9 seconds). Levels that fill while a Trio is open are banked and
  the next Trio opens on the bar line after the last one closes. A level-up Trio lasts four bars
  (the ring is the clock; the nearest orb is taken at the end). The fight does not step back for
  it, so the orbs are shelters: nothing spawns inside the ring, **a ship inside an orb's ring
  cannot be hurt**, and bodies touching that ring are shoved away. A boss's Trio is held the same
  way, for the whole Break after the boss. When every Mod and Riff is at its cap the orbs offer three different Boosts.
- **Cells** (health pickups; play test 2). A Cell restores one Shield cell. Kills drop them only
  while the player is below full health: about one every 40 seconds with one cell missing, about
  one every 15 seconds on the last cell (director controlled, like Boosts; at most two loose at
  once). Every elite and mini-boss drops one and a boss two, whatever the health. A Cell lasts
  eight bars and is pulled by the magnet like a Shard. Look: `design/look.md` section 6.
- **Boss kill:** an extra Trio. **Boosts** drop from enemies (roughly one every 20–30 s, director
  controlled), from Vaults and from events.
- Limits: 4 Riff slots, Riff levels 1–5, Mod caps per the table, 3 Boosts active at once (6.4: the
  same one again refreshes itself, a fourth replaces the oldest). Boost durations are in bars, so they are musically exact.

### 6.2 Mods (33; 36 until the three Dash Mods were removed, 2026-11-15) — permanent for the run, stackable

Weapon

| # | Mod | Effect per stack | Cap | Sound / look |
|---|---|---|---|---|
| 1 | Split | +1 projectile a rank in a tight fan: 2, 3, 4, then 5 lanes (the owner, 2026-11-14: "not such drastic jumps every time"; it was 3 then 5 in two ranks since play test 3, 3-5-7 before). Neighbouring lanes keep the fan's spacing, so the fan widens a lane a rank; an even count straddles the aim line. Seven lanes and more are Prism's to give | 4 | Adds a harmony note to each new lane, one a rank (3rd, 5th, octave, 7th) |
| 2 | Double-Time | Fire grid 1/16 → 1/16 triplet → 1/32 (the original two ranks again, the owner, 2026-11-14; play test 3 had cut it to one) | 2 | Lead doubles in speed |
| 3 | Pierce | Shots pass through +3 enemies, a quarter heavier after each (the owner's buff, 2026-11-15: +2, no growth) | 3 | Longer shots, longer note release |
| 4 | Ricochet | +2 bounces: off walls and structures, and from each enemy hit to the nearest other enemy within 300 wu | 3 | Pitch-up blip per bounce |
| 5 | Fork | On first hit, split into 2 shots of 0.75 damage that home in and fly 0.9 of a shot's life (play test 3: one rank; forking again is Fractal's, whose halves keep half damage; 2026-11-15: half damage, straight, 0.6) | 1 | Y-shaped flash |
| 6 | Seeker | Shots curve toward targets and pass through 1, 2, then 3 bodies, hunting the next (turning 5, 9, 14 rad/s within 420 wu; 2026-11-15: 3, 6, 10 within 320, no pierce). In a fight a homing shot keeps to the boss unless a body is within 80 wu. On a boss only toward a part the shot already flies at (within 35°), its weak point first (2026-11-15: curving to the nearest part, most often armour, made the fights a fifth longer) | 3 | Slight portamento on the lead |
| 7 | Heavy Step | Every 4th shot is a slow orb that explodes | 3 | Low octave thump |
| 8 | Accent | The shot on beat 1 is a full-length piercing lance; then beats 1+3; then every beat | 3 | Accented note, white flash line |
| 9 | Echo | Each shot repeats as a ghost one beat later at half damage; then two echoes | 2 | Delay line on the lead |
| 10 | Wave | **A wave front rolls out on every beat** along the aim: an arc 90° wide growing to 400 wu over 0.45 s, 2 damage to each body it passes and to each boss part once; rank 2: 140°, 550 wu, 3. The shots fly straight (the owner's rework, 2026-11-15: they rode sine paths, 28 then 40 wu to each side, which carried the auto-aim's shots off the boss; Helix Array keeps its own strands) | 2 | Vibrato; a crescent of light |
| 11 | Overcharge | +50% damage, larger hotter shots: the shot's head changes colour with every stack (look.md section 5, "The build on the shots") | 5 | More drive on the lead |
| 12 | Rear Guard | Also fire backward with every volley; then sideways too (2026-11-15: on the off-beats only) | 2 | Off-beat counter-melody |
| 13 | Arc | Kills chain lightning to 1 more enemy within 160 wu, then 2, then 3; an arc's own kill arcs no further (play test 3: half of what it was) | 3 | Zap arpeggio |
| 14 | Burn | Hits ignite for 3 beats, 2 damage a beat a stack; a burning enemy that dies ignites neighbours within 80 wu, once: their fire spreads no further (play test 3: half; 2026-11-15: 2 beats, 1, 45 wu) | 3 | Crackle layer |
| 15 | Shatter | Killed enemies' line segments fly out as damaging shrapnel: 2, 3, then 4 pieces; a piece's kill throws none (play test 3: half) | 3 | Glassy burst |
| 16 | Gravity Rounds | Hits tug nearby enemies toward the impact and crush them: 1, then 2 damage to every body within 130, then 180 wu (2026-11-15: the pull alone, 110 and 150 wu, drew the crowd onto the ship) | 2 | Grid dimples at each hit |

Defence and movement

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 17 | Extra Cell | +1 Shield cell capacity and refill every cell (up to 8 cells) | 4 |
| 18 | Regen | A cell grows back by itself every 8 bars; then every 4 (the owner, 2026-11-15: "remove the lowest rank", 16 bars). The halo's next arc draws itself as it grows | 2 |
| 19-21 | ~~Afterimage, Reserve, Shockstep~~ | **Removed** (the owner, 2026-11-15: "just remove the dash mods entirely and buff the base dash"): a player who never dashes, a young one above all, got nothing from them. Reserve's charge and Shockstep's ring are the base Dash's now (section 5); Afterimage's decoy is gone. Never dealt (their numbers are kept so the ones after them do not move) | 0 |
| 22 | Aegis | A rotating arc blocks bullets from one side; wider per stack | 3 |
| 23 | Last Light | Taking a hit fires a free mini-Nova (600 wu), at most once in 4 bars (2026-11-15: once a cycle, 420 wu) | 1 |
| 24 | Slipstream | +20% speed, one rank (the owner, 2026-11-15: "buffed slightly and rank 1 be max rank"; +15% a rank, two; M5: +8% four times over was a pick nobody feels, rule 6.6) | 1 |

Economy

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 25 | Magnet | +100% Shard pull radius (90 wu, then 180, 270, 360, 450) | 4 |
| 26 | Prospector | Half an extra Shard per kill a stack, the whole part sure and the rest a chance: one more every kill at two stacks, two at four (2026-11-15: a 25% chance a stack, at most one) | 4 |
| 27 | Nova Stock | +1 Nova capacity and gain one | 3 |
| 28 | Encore | Boosts last one bar longer | 3 |
| 29 | Lucky Break | Trios offer one more option | 2 |
| 30 | Collector | Every 500 Shards spawns a Boost (the owner, 2026-11-14: "too op late game (make it every 500 shards at rank 1 and capped at rank 1)"; every 250, then 150 at two ranks, until then) | 1 |

Rhythm

| # | Mod | Effect per stack | Cap |
|---|---|---|---|
| 31 | On-Beat Crit | Shots on quarter-note beats deal ×3, ×5, ×8, flash, and burst over 80 wu | 3 |
| 32 | Syncopation | Off-beat shots pierce and deal +50% | 2 |
| 33 | Bar Line | At each bar start a laser cross flashes through your position; then diagonals; then longer | 3 |
| 34 | Crescendo | +10% damage per bar without being hit, up to +50%; then +100%. A hit halves the climb (2026-11-15: +5%, and a hit ended it) | 2 |
| 35 | Kick Pulse | Every kick drum emits a knockback ring around you; the second stack makes it hurt (the owner, 2026-11-15: "Limit kick pulse to 2"; a third stack widened the ring). The ring is on the steps of the Track's kick pattern at velocity 5 or more, as its tom is, so in Tracks 7, 9 and 10 it is not on every beat; its push and damage are scaled by 4 / (kicks a bar) so the Mod is worth the same in every Track. None in a Break | 2 |
| 36 | Afterparty | Surge lasts four bars longer and its fire grid is one step faster; rank 2: the whole Drop, a boss's included (2026-11-15: two bars a stack) | 2 |

### 6.3 Riffs (12) — autonomous weapons, each a layer in the mix

| # | Riff | Pattern and effect | Levels add | Instrument layer |
|---|---|---|---|---|
| 1 | Orbitals | **A guard ring** (play test 3: "doesn't seem to do a whole lot ... maybe it can be like an extra shield with a regen component"). 3 shards orbit you 64 wu out, one turn per bar; each is 14 wu across, cuts what it touches for 3 damage an eighth note and destroys enemy bullets it touches. While every shard is lit the ring is a spare cell: a hit on the ship breaks the ring instead of taking a cell (the panic pulse and one second of invulnerability as for any hit). The shards then go dark and relight one by one; the ring guards again when all are back, 8 bars after it broke | Levels 2 and 4: +2 shards (5, 9). Levels 3 and 5: it relights faster (6 bars, then 4) and +2 shards (7, 11) | Shaker ticks as they pass 12 o'clock |
| 2 | Pulse Ring | Expanding damaging ring every beat, twice the first build's damage | Double ring; wider; heavier; a ring on every eighth | Tom / sub thump |
| 3 | Sentinel | Drone fires a burst of 3 at the nearest enemy on off-beats | Burst size; a second drone at level 3, a third at level 5 | Off-beat stab |
| 4 | Lancer | Charges 3 beats, fires a full-length beam along your aim on beat 4 | Width; every 2 beats | Chord hit |
| 5 | Minelayer | Drops a mine each bar; all detonate on the next bar's beat 1 | Clusters | Clap + sub |
| 6 | Storm Coil | Zaps nearby enemies on 1/8 triplets | Targets, radius | Pentatonic zaps |
| 7 | Swarm | A volley of homing micro-missiles each bar, staggered as a 1/32 run | Count | Rising arpeggio run |
| 8 | Singularity | Every 4 bars a gravity well ahead of you pulls for 3 beats, then detonates on beat 1 | Radius, damage | Reverse swell + boom |
| 9 | Mirror | A ghost wingman mirrored across your aim axis fires your primary at 50% | 60 / 70 / 80 / 100% | Lead doubled an octave up |
| 10 | Comet | A boomerang disc thrown along your aim each bar, hurting what it touches every sixteenth for 5 (2026-11-15: every eighth for 3) | Size, count | Pitch-glide whoosh |
| 11 | Sweeper | A laser arm rotates around you once per 2 bars | Length; second arm | Filtered drone that opens as it sweeps |
| 12 | Flak | On beats 2 and 4, a radial burst of pellets | Count; pellets explode | Snare accent layer |

### 6.4 Boosts (16) — timed pickups, 4 bars unless noted

**Three at once** (the owner, 2026-11-14: "can only seem to have 2 temp powerups active at a time"; two until then):
the same Boost taken again refreshes itself, and one more replaces the oldest.

| # | Boost | Effect |
|---|---|---|
| 1 | Prism | Every shot refracts into a 5-way rainbow fan; hits spawn mini-refractions. The fan is a real rainbow in saturated colour (play test 2: the first build's lanes were too pale): the centre lane keeps the player's colour, the lanes beside it sit 60° and 120° round the hue wheel to either side at lightness 0.70 and full chroma, coloured end to end (look.md section 5); the mini-refractions take their lane's hue |
| 2 | Hyper | Primary and all Riffs run at double grid |
| 3 | Supernova Rounds | Every shot explodes on impact for half the shot's damage (play test 3) |
| 4 | Colossus | Ship ×3 size, invulnerable, rams through enemies (2 bars) |
| 5 | Time Stretch | World at half speed, you at full; music drops to half-time (2 bars). In co-op the world slows for everyone and every player keeps full speed: it is a gift to the team |
| 6 | Freeze Frame | Enemies in view freeze for 2 bars and shatter in one hit, chaining to at most 12 at a time (play test 3) |
| 7 | Halo | Absorbs up to 3 hits (until broken, at most 8 bars) |
| 8 | Magnet Storm | All your Shards on the map fly to you; kills drop double |
| 9 | Black Hole | Goes off by itself the moment it is taken, as a free Nova: a 3-beat singularity that drags the screen in, then explodes. It spends no Nova (the owner, 2026-11-14: "some temporary power-ups don't seem like they work (like black hole)": it waited for the next Nova, which a player on the assist never fires) |
| 10 | Chain Storm | Every beat, lightning links all enemies near you |
| 11 | Chorus | Two ghost ships fly formation and copy your shots |
| 12 | Beam | Primary becomes a continuous sweeping laser |
| 13 | Ricochet Fever | All shots gain +5 bounces and speed up per bounce |
| 14 | Confetti | Every kill bursts into 6 homing sparks |
| 15 | Fractal | Every shot splits in two on its first hit and each half splits again on its next, at half damage each time (up to four from one shot; with Fork, one generation more). **The split shows** (the owner, 2026-11-14: "fractal ... isn't visually clear at all on what happens, i still don't know what it does"): a Y of light at the hit, its two arms along the halves' paths, and the halves turned round the hue wheel as Prism's lanes are (60 degrees, then 120 for the last), so a tree of colour grows through the crowd. Fork's halves show the same way |
| 16 | Carousel | Six orbiting bombs; one launches outward on each beat |

### 6.5 Fusions (8)

| Fusion | Ingredients (both maxed) | Result |
|---|---|---|
| Helix Array | Split + Wave | Shots braid into a helix that sweeps a wide corridor (M5: the Split lanes stop fanning and ride strands in pairs, one of each pair the mirror of the other, the middle lane straight down the axis; one wave to two beats of flight; the outer pair 80 wu to each side, the pairs inside it at 2/3 and 1/3 of that) |
| Standing Wave | Echo + Accent | Accent lances persist as lethal lines for a bar |
| Tesla Bloom | Pulse Ring + Arc | Rings arc lightning between everything they touch |
| Shard Halo | Orbitals + Shatter | Orbitals fling shrapnel streams on every kill |
| Downbeat Cannon | Lancer + On-Beat Crit | A critical beam the whole length of the screen every beat 1 (M5: 64 wu wide, the widest any beam hurts; look.md v2.5) |
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
- **No pick is a trap** (M5). Measured by the simulation on one reference build, a stack of any
  weapon or rhythm Mod and a level of any Riff is worth between 0.6 and 1.5 of a staple, counted as
  gain over the reference build: against the Track's boss (seconds to kill) its gain over a stack of
  Overcharge's, against a Groove's crowd (kills a minute) its gain over a stack of Split's. A pick
  is judged on the better of its two figures, the one it is made for. (Overcharge was the one
  yardstick at first; fodder dies to one shot, so against a crowd Overcharge gains almost nothing
  and every crowd pick looked many times too strong.) One outside the band has its numbers
  changed, not its odds of being offered.
- No upgrade may break the entity caps (TECH_SPEC §7): effects that multiply projectiles carry their own caps.
- **A chain of kills is one sound, not a hundred** (play test 3: "too loud (multiple enemy pops at the
  same time)"). Everything a kill sets off counts as one family on a 1/32 step: the kill pops of
  every size and every upgrade's blast, crack, zap or crackle together are never louder than one
  large kill, and a chain that goes on plays each further step of the same beat at 0.7 of the one
  before, down to 0.3.
- **The crowd may not break them either** (M5, 2026-11-13: with four players Track 7 refused 2,650 to
  4,030 enemy bullets in three minutes, the pool full). The director buys no shooting species while
  the enemy bullets alive are above 70% of their pool, and a boss's or mini-boss's own patterns come
  first: nothing a page scripts is ever the bullet that is refused. The pool does not grow: the
  phone's frame was measured with it.
- **Thinning never flickers** (M5, 2026-11-13; part 3 fired the outer Split lanes on alternate steps
  above a player's soft cap, and the fan changed shape from step to step). While a player is over
  the soft cap every lane still fires on every step, and the lanes beyond the middle three fly half
  as far; the multipliers' extras (forks, echoes, ghosts' shots, sparks, missiles, pellets, bombs)
  wait. Thinning starts above the soft cap, ends below three quarters of it, and starts or ends
  only on a beat. Above the second cap the middle lane fires alone, as before.
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
(speeds up nearby enemies). Elites carry one trait at first; from Track 7 they stack. The numbers, which Track brings which
trait, how often elites come and what they drop are in `design/traits.md`.

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

All twelve are designed in full in `design/bosses/minibosses.md` (rules common to all, which Track
draws which, health by Track, and each fight); the table below is the one-line version.

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

**A boss's rule is a bonus, not a wall** (M5, 2026-11-13). The first six bosses were written so that
shots anywhere but the weak point did nothing: Longwave off its crest, a silent Voice, a rotor's
corners, Feedback's shut Head. On paper that is the lesson; in the hand it is a boss that will not
die and does not say why, and the user never got past Longwave. So:

- **Armour takes a quarter.** Any part a boss's page calls armoured, closed, silent, shut or fed
  takes 0.25 of a shot's damage (the pages' data say so now); the weak point takes what it took.
  Knowing the rule is worth three or four times the damage, not all of it. The same holds for a
  mini-boss: the Ram's horns, the Sentry Array's domed turrets, the Brood Mother's dome and
  whatever else of a mini-boss's own body took nothing take a quarter. A shot on armour still
  sparks and stops as armour does, and the boss's health bar still moves.
- **The game says the rule.** Each boss has one hint line (design/screens.md, "Boss hint"), shown
  under its name as it arrives and again whenever a player's shots have met only armour for two
  bars.
- **Fights are tuned for a person.** The careful and the careless bot aim perfectly, never stop
  firing and know every rule; both pass Longwave in at least nineteen runs of twenty. A third policy, the
  **novice**, stands in for a player's first week:
  - it aims at the nearest body (of a boss: its nearest part, not its weak point), 200 ms behind
    where that body is, and wanders 6° to either side;
  - it learns a boss's rule only from the hint: for the first 16 bars of a fight it ignores the weak
    point, after that it aims at it half the time;
  - it reacts to a threat 250 ms late, never rides a wave or threads a gap on purpose, dashes out of
    half the hits it sees coming and fires its Nova at one cell;
  - it takes a Trio's first orb half the time, otherwise any orb.
- **The lines, over 20 runs** (starting values; bosses 7 to 10 at the balance pass). The novice
  reaches Track 2's boss in at least 16 runs. It wins its first meeting with Kaleidos in at least
  18, with Longwave and the Choir in at least 16, with the bosses of Tracks 4 to 6 in at least 12.
  A fight it wins lasts at most 1.5 times the longer length on the boss's page. **And a fight is
  short** (the owner, 2026-11-14, after Event Horizon: "way too much hp", then "check the HP on the
  later bosses", then "Anything around 30-60"): **every fight the novice wins lasts 30 to 60
  seconds**; one outside is brought to about the middle of the band's near half (a long one to about
  50, a short one to about 30). This outranks the pages' lengths, and the pace
  rule then holds an aiming player's fight to the same. Measured before it (the novice through
  the half-rate assist, a won fight, af4b101): Event Horizon 75 s (halved by the owner), Polyrhythm
  20, Feedback 63, Cadence 91, Arpeggiator 69, Fata Morgana 41, Reprise 114; Kaleidos 55, Longwave
  83, the Choir 34. So Longwave, Feedback, Cadence, Arpeggiator and Reprise come down, and
  Polyrhythm goes up to about 30 as long as the novice still wins it as often. **The finale is the
  longest:** Kaleidos Reprise lasts about 60 seconds (the owner, 2026-11-14, after it came in at 35). The careful bot's
  fight lasts at least half the shorter length. Where the two pull apart, the novice's line wins.
- **What that costs, and until when.** Tuned for the novice (2026-11-13: Kaleidos at 0.55 of its
  page's health, Longwave at 0.77), the careful bot kills Kaleidos in 9 s and Longwave in 18 s: a
  strong player's fights are three to five times too short, and the careful bot's line is not held.
  That stays until the balance pass, which makes a boss tougher for players who are hurting it
  faster (measured in the fight itself, on its weak point) and never easier than the novice's tune.
- **A boss keeps pace** (the balance pass's rule, written 2026-11-13). A boss's health is the
  novice's tune. On every bar line of the fight's first eight bars the game sets the fight's
  **pace**: the damage the players have done to the boss so far (after armour, per player in
  co-op), divided by what the novice has done by that bar (its eight-bar number, measured, in
  `tuning.h`, taken in proportion to the bars gone), held between 1 and 4; a ratio under 1.5
  counts as 1 (1.25 at first: the novice's own good bars got past it), so a novice having a good bar is not punished for it. After the eighth bar it
  stands. (First written as one reading at the eighth bar: the careful bot kills Kaleidos, the
  Choir and Polyrhythm before that bar comes.) From each bar line on the boss takes its damage
  divided by the pace. Nothing
  jumps on the health bar; a player who knows the rule and has the build simply meets a boss
  that lasts about as long as the novice's does, which is the length on its page. A novice's pace
  is 1 by construction. **Only aimed damage counts** (2026-11-14): shots fired through the assist are
  the novice's own fire and never raise the pace, so a player who never aims meets the novice's boss
  (the assist bot, which never aims, met Longwave and the Choir at paces of 2.1 to 2.8 and missed
  its bar). Bosses only: a mini-boss leaves with its Drop. In co-op the host sets the
  pace and applies it; solo is the same code. With it the careful bot's line (a fight at least
  half the page's shorter length) is held again.
- **Beatable with the assist alone** (play test 3, the owner: "this is meant to be an 'easy' game,
  it should be 100% beatable with just auto aim"). This is the difficulty bar, above every other
  line here. The novice bot no longer aims: it moves as it did and fires only through the assist
  (section 2), which now aims at a boss's weak point. In Feedback's fight the assist rests on
  every second bar, so the Head opens for a player who never lifts a thumb. A second policy,
  **assist**, is the careful bot's movement with the assist's fire: it must finish the ten Tracks
  in at least 19 runs of 20, so aim is never what ends a run. Health is tuned with the novice as
  before, from fight starts. Since 2026-11-14 the assist fires at half rate (section 2), so the
  novice does about half the damage it did: the bosses' and mini-bosses' health factors and the
  novice's eight-bar numbers are measured again with it, and every line here holds as written. The
  fights get no longer for it: a boss the assist meets at half rate has about half the health (the pace
  rule then holds a thumb-aimed player's fight to the page's length, as before). The crowd is left
  as it is unless the assist bot's runs end there.
- **Every hit on a boss is seen and heard** (play test 3: "no hit confirmation effects"). A shot on
  a weak point: the part flashes white for 60 ms (a white copy of its hull at 2.0), six sparks in
  the part's hue fly back along the shot, its segment of the health bar flashes, and a bright tick
  sounds in the Track's key. A shot on armour: three grey sparks thrown sideways, no flash, a dull
  low knock. Both sounds are counted on the 1/32 step like kills. Boss sparks are never the ones
  dropped when the particle pool is full.
- **Counting a line.** Twenty runs cannot tell 15 from 17. A result within two of its line is run
  again over forty seeds and judged there (the line scaled: 32 for 16). What a person says after
  playing outranks both.
- **Calibration.** On the build the user played (part 3: Longwave's health at 2.2 times its page,
  armour at nothing) the novice must lose to Longwave in most runs. A novice that passes it there
  is not one.

| Boss | Concept |
|---|---|
| **Kaleidos** | A huge rotating kaleidoscope wheel. Mirrored arms fire symmetric fans on 1/8 notes; shoot arms off to expose the core. A gentle teacher of "attacks land on the beat". |
| **Longwave** | An arena-length serpent whose body is a travelling sine wave. Segments take full damage as they crest and a quarter otherwise; the wave speeds up each phase. |
| **The Choir** | Eight linked sentries around the player trading call-and-response volleys. Each one destroyed removes its voice from the boss music and changes the pattern. |
| **Event Horizon** | A giant well that drags in enemies, bullets and the grid itself. Starve it by killing what it feeds on; it erupts on bar lines. |
| **Polyrhythm** | Two rotating emitters firing three-against-four patterns. The safe gaps align once per bar; reading the rhythm is the fight. |
| **Feedback** | Copies the players' current builds and plays their own patterns back one bar later. The stronger the build, the harder the echo. |
| **Cadence** (Track 7, Afterglow) | The boss is the chord progression. Four great bodies marked I, V, vi and IV stand round the frame; the one whose chord the band is playing is silent and open, the other three fire slow fans in half time, on one and three. The player follows the harmony round the room a bar at a time. Later they change places on the bar line, then the chords come twice as fast. |
| **Arpeggiator** (Track 8, Arcadia) | The bullets are the tune. Four lanes cross the frame and the lead's broken chord runs down them as blocks, lane 1-2-3-4-3-2-3-4, so the player weaves the shape of the arpeggio they have been firing. The next bar's chart shows above the lanes; the machine at the top opens as it loads each bar. Later the lanes turn a quarter, then two charts cross. |
| **Fata Morgana** (Track 9, Mirage) | The safe place moves. A storm closes the frame and the only calm is an oasis that drifts; the boss hides in the storm wall and shows where the wall thins. Then there are two oases and one is a mirage that evaporates on the snare; then the oasis leaps on the broken kick's three hits and the storm parts for a beat to let the player cross. |
| **Kaleidos, Reprise** (Track 10, Last Light) | The first boss comes back as the first theme does. Its own fans, now on the two-step break; then a medley, each arm playing one earlier boss's idea (a travelling wave, a call and its answer, a pull, three against four, an echo), to be shot off one by one; then the bare core fires First Light's arpeggio as its last pattern while the lattice turns to Ultraviolet for the encore. |

The four for Tracks 7 to 10 were added on 2026-11-12, when the set list grew to ten; their full
pages are `design/bosses/cadence.md`, `arpeggiator.md`, `morgana.md` and `reprise.md` (picture:
`design/previews/bosses_7_10.png`). Each is built on something no earlier boss or
mini-boss asks of the player: following the harmony, reading the melody as a pattern, keeping up
with a safe place that moves, and recognising six fights inside a seventh.

---

## 8. Director and difficulty

- **Enemy bullets by Track** (the owner, 2026-11-14: "tone down the number of enemy bullets by like
  0.5x in tracks 1-3, 4-7 can be 0.75x"). Every enemy bullet the game would fire (a shooter's shot,
  a fan, a ring, a spray, a boss's or mini-boss's pattern, an elite's, an arena event's) is counted
  at **0.5 in Tracks 1 to 3** and **0.75 in Tracks 4 to 7**; Tracks 8 to 10 and the loop fire as
  written. The bullets are drawn a third larger since 2026-11-14 (look.md section 4), their hit radius
  unchanged. (The owner's "red line enemy projectiles in track 9 ... tune down to like 0.25x" of the
  same day was Fata Morgana's sand streaks, not bullets: `design/bosses/morgana.md`.) A pattern keeps its shape: a fan or ring loses bullets evenly round it, keeping its
  symmetry and never going below the smallest count its page allows; a shooter whose volley is one
  bullet fires on every second volley (Tracks 1 to 3) or three volleys in four (4 to 7), on the beat
  as before. Lethal beams, bodies, the Arpeggiator's blocks and trails are not bullets and stay. Co-op
  the same. The fights' health is not tuned again for it.
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
  Track of the loop: **the Breaks no longer give back a cell or a Nova** (nor a Shard level a Nova,
  2026-11-15); Regen's periods stretch
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
  lever is the clock itself (every Track has played two cycles since 2026-11-14, so the halo
  burns down a third sooner than when this was measured), not more damage. These loop numbers are
  measured again at the balance pass, not before.
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
  frame whenever its bullets are. A boss's own page may hold the camera further out, to 0.75× at most (Longwave and The Choir do), when the fight cannot be read otherwise; the Moto's contract is measured at that zoom.
- **Indicators:** edge arrows for elites, bosses, teammates, events and incoming ranged attacks; an
  optional corner radar.

Structures (a handful per run, placed by seed, changing with the Track; the full design with numbers,
shapes, sound and which Track has which is `design/arena.md`):

| Structure | Behaviour |
|---|---|
| Pylon | Pulses every bar. Stand in its ring for 4 beats to tune it: it then fires a damaging pulse every bar for a cycle |
| Rift | An enemy portal that opens during Groove and Build. Destroy it to stop the flow and drop a Boost |
| Vault | An armoured crystal. Break it for a Boost and Shards |
| Bumper | Bounces ships and shots; charges when hit and discharges on the beat |
| Current | A flow lane that speeds up anything inside it, enemies included |
| Lens | A bar of glass that splits player shots passing through it into three (first called Prism; renamed so that it is not confused with the Boost) |

Events (scheduled by the director, announced by an edge indicator; they give reasons to cross the map
and to split up in co-op): Rift Surge · Vault Drop · Elite Hunt · Comet (a fast neutral object trailing
Shards) · Eclipse (lights out for 8 bars; enemies show only on beat flashes) · Meteor Rain
(beat-synced, telegraphed impacts) · Convoy (slow carriers with heavy loot) · Storm Cell (a roaming
lightning field).

---

## 10. Co-op (2–4 players, LAN)

M8 (`docs/MODES_PLAN.md`, `design/modes/README.md` 8): every mode is played solo, in co-op or in **versus** (each player for
themselves, nobody can hurt a friend); the room's mode and game are the host's choice, on MODES from the lobby.

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

M8 (`design/modes/screens.md`): the title's mode pill over PLAY says what PLAY starts (THE SET until another mode is played); it
opens MODES (the seven modes as cards, a mode's Track heard as it is chosen, PLAY); PLAY on the title starts the pill's mode on
its remembered Track. Modes: `design/modes/` (README.md 1 for the line-up).

Flow: logo sting (≤ 2 s, skippable) → title with the bot playing in the background → **PLAY** /
**CO-OP** / settings → run → game over → **RETRY**. Co-op: HOST or JOIN → lobby (players, colours,
room code) → run.

Settings (few, plain, each with a visible pressed state). On screen they are three tabs: SOUND,
CONTROLS and LOOK (video and accessibility together); layout in `design/screens.md`:

| Group | Options |
|---|---|
| Controls | Auto-aim assist; Auto Nova; swap sides; reset hints |
| Audio | Master, music, effects; **sound**: Club / Arcade / Velvet (three characters of the same music, `design/music/variants.json`; Club is the default); **sync calibration** (flash-and-click slider); haptics on/off and strength; in co-op: "music from host only" |
| Video | Effects: Auto / Low / Medium / High / Ultra; frame-rate target: Auto / 60 / 90 / 120; reduce flashing; vivid colour (where supported) |
| Accessibility | UI scale |

(2026-11-15, the owner: of the rows not yet built, "just do Stick size and position, button layout, gamepad mapping":
shake amount, colour-blind-safe palettes and high-contrast threats are cut. Later that day the owner dropped those three
too, "No need batch 2": stick size and position, button layout and gamepad mapping are not built; `design/screens.md`
keeps their spec as a record.)

Local high scores: top 10 for solo and for each co-op player count (a co-op run's team score goes to its count's table,
never to solo's). Lifetime stats beside them on the SCORES screen (`design/screens.md` "Scores"). Not built: the owner
dropped the SCORES screen, its tables and the lifetime stats on 2026-11-15 ("No need batch 2"); the title's BEST line is
as built.

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
