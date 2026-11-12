# RAVEFORM — Progress

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

Stage B is built on `main` (commits `7e9b97b`, `9ac3547`, `be3c086` and the docs commit): the director,
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
  damage and phases), 0 failures; determinism PASS for both bots over 2 min (`test.ps1`) and 12 min.
- Bot, 30 simulated minutes headless (careful bot, seed 1 / seed 7; reckless bot, seed 1): no crash, no
  overflow, no NaN, 0 heap allocations in the loop; tick P95 0.010 / 0.063 / 0.006 ms, max 0.059 / 0.185 /
  0.141 ms. Seed 1: 5,452 kills, 2 deaths (both at Track 2's Kaleidos, 10:19 and 19:26), Tracks reached
  2; Kaleidos of Track 1 falls at 5:43 (entered 2:49), 14:35 and 24:22. Seed 7: 4,775 kills, 2 deaths (both
  at Track 2's Kaleidos), Track 1's boss 1:41 the first time but over 7 minutes the third time (fodder piles
  up to 226 alive meanwhile). Reckless: 15 deaths, the longest life 196 s, no boss beaten. **First reading
  of the difficulty:** a careful bot clears Track 1 in about 5 minutes and is stopped by Track 2's Kaleidos
  (1.5x health) every time; a careless one dies every 1.5-2 minutes. Per-minute tables:
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
- MOTO_NUMBERS

### Effect words (two to four words, mine)

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
MOTO_PICTURES.

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
- MOTO_GAPS

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
