# M5 audio: the ten Tracks of the set list — 2026-11-13

Branch `m5-audio` (worktree `Raveform-m5-audio`, cut from `737949e`). For the lead to fold into PROGRESS.md.
Phone numbers come from the **shared** Moto One 5G Ace (`ZY22BXMJG3`), under the phone lock (`m5audio`), with this
branch's APK deployed right before each run.

| Done criterion (brief) | Status | Evidence |
|---|---|---|
| Ten Tracks embedded, loaded, selectable (`--track 1..10`); every Track-sized table holds ten; ten palettes | **Met** | `MusicData` / `SampleBank` / `FdnReverb` / calibration tables at `kMaxTracks = 10`; `Palettes::kMax` 12 (an error if the file lists more); tests "music data", "palettes" |
| Nothing assumes a kick on every beat; beats and bars from the song clock; the sidechain from the pattern's kicks | **Met** (engine and band); game-side readers listed below | tests "no kick on the beat" (Tracks 7, 9, 10): beats, bars, kicks, the sidechain frame by frame, the Nova, the fill, the Build's roll, riser, swell, impact |
| Every Track x variant matches the prototype; calibration regenerated; `scripts\test.ps1` passes | **Met**: 30 of 30 pass, every onset 0 samples off | `captures\m5audio\match_table.md`; `test.ps1` all passed (audio 2519 checks) |
| Track changes at all nine boundaries; medley with its events | **Met** | test "the set list: nine Track changes"; `captures\m5audio\setlist_medley.{wav,mp3,events.csv,txt}` |
| Moto: full band at Track 10 and at the heaviest Track inside M2's limits | **Met**: P95 22.4% (Track 10), 22.6% (Track 9), 0 underruns, 0 callback allocations | below |

## What changed

- **Ten Tracks** (`music_data.*`, CMake): `track04` to `track10` embedded like the first three; `kMaxTracks = 10` at
  namespace scope, the calibration table sized by it (it was `[4][4]`). Every override the new Tracks carry is bound
  (none is reported unused): `duck` (Afterglow's pad 0.7, Arcadia's bass 0.45), `patch.kick_hz`, `lead.length`,
  `stab.length`, `delay.*`, `reverb.rt60`. One-shot bank (`bake.*`): risers sized for ten Tracks (it held 8 entries),
  the swell tables initialised to -1 for every Track (only the first four were), 512 samples, cache version 2. 304
  one-shots, 48.3 MB (30.1 MB for three Tracks), 2.0 s on the desktop; the phone bakes once and caches.
- **Kicks are not beats**: Beat and Bar events were already the clock's (`onTick`) and the sidechain already followed
  `playKick`; the tests now prove it on the three Tracks without a kick on every beat. Kick events carry the pattern's
  velocity in `x`; haptics tick on the Track's kicks (GAME_DESIGN 11.5) but not on ghost kicks below half velocity
  (Last Light's step 13); `BeatView::beat(decay)` is the clock's beat envelope for anything meant "on the beat".
  Fills (the clap's pick-up, the hats doubling from `hat_16`), the Build's roll (clock divisions) and the risers
  (baked per Track at its tempo) never read kicks. Risers also follow a tempo override now (`bpmOverride_`).
- **Shard runs in key** (`bake.cpp`): a major pentatonic where the scale has no minor third (Afterglow, Arcadia,
  Mirage); the minor one put a minor third and seventh over them. Tracks 1-6 and 10 unchanged.
- **Found on the faster Tracks** (pre-existing since M2; Tracks 1-3 never reached them in the 20 s tests):
  1. Voices were summed in slot order, and the slot a voice got depended on where the callbacks fell: random bursts
     differed from fixed ones by float rounding, which the reverb carried on. Each pool now keeps its voices in start
     order (start frame, then allocation) and is summed in that order; pending effects fire in planned order; the lead's
     per-player cap is decided at the new note's frame.
  2. The section gain's ramp across a bar line: the next section's first point was added only when its bar was planned,
     so the 30 ms before each section change held the old gain and the ramp jumped half way at the downbeat (by how
     much depended on the callbacks). The next point is now planned a section ahead (staged) or on the last bar
     (conductor; replaced if the director chooses otherwise).
  Since then random 1-1000-frame bursts render bit for bit like 192-frame ones on all ten Tracks.
- **Cost** (the full band at 174 BPM has 40% more lead notes in flight than at 120): per-voice caches of each knot
  interval's coefficients and envelopes (bit for bit the same values), the lead's and pads' PolyBLEPs four at a time
  (`dsp.h` `polyBlep4`), a polynomial sine for the P3 bell and the bass's sub, and **a teammate's lead holds at most 8
  notes** (the local player's 12): at 174 BPM every player reached 12 (48 lead voices against 33 at 120 BPM); the notes
  cut short are past 0.23 s and more than 20 dB under the local lead's onsets. Staged renders never reach either cap.
  A 4-lane lead kernel (four voices a vector) and two-voice interleaving were measured and dropped: no gain on the
  A77 or the desktop.
- **Why the rounding changed (kDspVersion 3 -> 5)**: the summation order (above) and the vector PolyBLEPs compute
  `t * (1/dt)` where the scalar code divided `t / dt`, and sum the pads' saws pairwise; both move a sample by about one
  float rounding step. The loudness calibration moved by at most 0.001 dB; Tracks 1-3's match is unchanged to two
  decimals in every column (table below, identical to M2's), their loudness before the output gain too (-8.96 /
  -8.92 / -8.25, -8.52 / -8.53 / -8.41, -9.23 / -9.19 / -9.00 LUFS).
- **Tools**: `raveform_headless --render medley`, `--render full --track N` (it always played Track 1), `--timbre N`
  (profiling), events with full-precision `x`; `--audio-profile` (app) adds the engine's time per stage, the lead per
  timbre, to a benchmark's `audio_engine`; `audio_check.py tempo-onsets` (onsets against the tempo map rebuilt from the
  log); `render-audio.ps1 -Out` and every `track??.json`; `audio-device.ps1 -LockName`.

## Match against the prototype (`scripts\render-audio.ps1 -Out captures\m5audio`, final engine)

| Render | LUFS engine | LUFS prototype | True peak dBFS | Worst band diff dB | Worst section diff dB | Onsets checked | Worst onset error | Verdict |
|---|---|---|---|---|---|---|---|---|
| track01 club / arcade / velvet | -14.00 | -14.00 | -5.19 / -5.20 / -5.91 | 0.18 / 0.19 / 0.49 | 0.58 / 0.60 / 0.38 | 1226 | 0 | pass |
| track02 | -14.00 | -14.00 | -5.58 / -5.59 / -5.70 | 0.42 / 0.39 / 0.31 | 0.28 / 0.36 / 0.32 | 1254 | 0 | pass |
| track03 | -14.00 | -14.00 | -4.96 / -5.00 / -5.18 | 0.19 / 0.20 / 0.17 | 0.14 / 0.12 / 0.17 | 1329 | 0 | pass |
| track04 | -14.00 | -14.00 | -5.29 / -5.16 / -5.44 | 0.09 / 0.12 / 0.17 | 0.31 / 0.32 / 0.27 | 1262 | 0 | pass |
| track05 | -14.00 | -14.00 | -5.42 / -5.42 / -5.71 | 0.12 / 0.09 / 0.15 | 0.14 / 0.12 / 0.08 | 1301 | 0 | pass |
| track06 | -14.00 | -14.00 | -5.74 / -5.74 / -5.62 | 0.19 / 0.22 / 0.29 | 0.57 / 0.51 / 0.55 | 1248 | 0 | pass |
| track07 | -14.00 | -14.00 | -5.36 / -5.43 / -5.27 | 0.35 / 0.36 / 0.29 | 0.31 / 0.30 / 0.23 | 993 | 0 | pass |
| track08 | -14.00 | -14.00 | -6.76 / -6.68 / -7.04 | 0.12 / 0.08 / 0.11 | 0.42 / 0.41 / 0.33 | 1337 | 0 | pass |
| track09 | -14.00 | -14.00 | -5.79 / -5.78 / -5.65 | 0.13 / 0.15 / 0.13 | 0.09 / 0.10 / 0.10 | 1164 | 0 | pass |
| track10 | -14.00 | -14.00 | -5.41 / -5.34 / -5.26 | 0.08 / 0.10 / 0.09 | 0.48 / 0.47 / 0.56 | 989 | 0 | pass |

Worst sections are the Breaks (pad start phases, as in M2), except Afterglow's third Groove section (-0.31, its long
room) and Mirage's second (-0.09). Loudness before the output gain within 0.14 LU of the prototype's on every render
(Afterglow 0.14, Arcadia 0.12, the rest within 0.09).

## Calibration (`src/audio/loudness_calibration.json`, kDspVersion 5)

Tracks 1-3 unchanged (to 0.001 dB). New: track04 -5.216 / -4.996 / -5.387, track05 -5.336 / -5.327 / -5.642, track06
-5.581 / -5.695 / -5.558, track07 -5.258 / -5.282 / -5.078, track08 -6.631 / -6.571 / -6.926 (the loudest raw render,
-7.37 LUFS: Arcadia's square bass), track09 -5.479 / -5.532 / -5.383, track10 -5.099 / -5.115 / -4.997 dB
(club / arcade / velvet).

## Track changes and the medley

The test drives the conductor bar by bar as `Play::conductBand` does (SetPhrase for the next bar, SetTrack with the
Break's first bar): at all nine boundaries the Break is eight bars from the bar asked for, the tempo steps linearly
bar by bar from the old Track's to the new one's (within 0.001 BPM), the Track changes on the Break's fifth bar, the
Build runs at the new tempo with its 85% bar and the new Track's riser (baked four bars at that tempo, played at
4 / (3 + 1/0.85)), the Drop lands at the new tempo; the main thread's mirrored tempo map agrees on every onset.

`captures\m5audio\setlist_medley.mp3` (and `.wav`, 16-bit; `.events.csv`; `.txt` with the times): Track 1's Groove
(2 bars) and Build, then about 20 s of every Track's Drop with the real change between them (8-bar Break, 4-bar Build),
one player firing in Builds and Drops and resting in Breaks, three Riffs, intensity 1; 6:44 in all, -13.6 LUFS, peak
-4.95 dBFS; 13,645 onsets on the tempo map's grid (0 samples off). Drops at 0:12 First Light, 0:56 Solar Wind, 1:39
Reef, 2:21 Aurora, 3:02 Furnace, 3:42 Zenith, 4:22 Afterglow, 5:00 Arcadia, 5:38 Mirage, 6:15 Last Light.
`raveform_headless --render medley --out x.wav --events x.csv`; check with `audio_check.py tempo-onsets x.csv`.

## Moto (`bench_idle --band full`, our APK, the lock; shared phone)

| Run | Callbacks | Load P50 / P95 / P99 / max | Callback us P50 / P95 / max | Underruns | Callback allocations | Voices peak |
|---|---|---|---|---|---|---|
| M2: Track 1, 30 s | | 0.237 at P95 | 900 / 1150 / 1290 | 0 | 0 | 72 |
| M2: Track 3 contract run (bench_endgame) | | 0.234 at P95 (P99 0.289) | 1020 / 1130 / 1300 | 0 | 0 | 81 |
| Before the cost work: Track 10, 120 s | 26,629 | 0.247 / **0.285** / 0.336 / 0.693 | 1210 / 1380 / 2300 | 0 | 0 | 84 |
| **Final: Track 10, 120 s** | 26,628 | 0.192 / **0.224** / 0.286 / 0.625 | 950 / 1070 / 1320 | **0** | **0** | 72 |
| **Final: Track 9, 120 s** (heaviest offline, with Track 10) | 26,629 | 0.194 / **0.226** / 0.284 / 0.736 | 960 / 1080 / 1260 | **0** | **0** | 74 |
| Final: Track 1, 60 s | 13,314 | 0.172 / 0.206 / 0.270 / 0.700 | 840 / 990 / 1170 | 0 | 0 | 68 |
| **Final: Track 10, 10 minutes** | 133,134 | 0.193 / **0.225** / 0.279 / 0.687 | 950 / 1070 / 1440 | **0** | **0** | 72 |

Offline profile of the full band (`--render full --track N --profile`, ms of engine time a second of audio, before the
teammates' cap): Track 1 29, Tracks 2-6 34-37, Tracks 7, 9, 10 38.2-39.8 (9 and 10 tie within noise). On the phone
(`--audio-profile`, Track 10, us a callback): lead 707 (saw 177, square 171, bell 194, stab 165), pad 139, bass 67,
master 64, reverb 63. P99 stays above 25% (as in M2, 0.265-0.289): the callbacks' tail is the phone's.

## Game code that reads a kick as the beat (src/game, not changed here)

`envelope(BeatKind::Kick)` is the pattern's kick (right for the kick pump, the halo, the lattice ripples, the logo:
look.md and screens.md say "kick"). These say "on the beat" and read the kick, so on Afterglow, Mirage and Last Light
they pulse on beat 1 and the and of 3 (and 2) only; `BeatView::beat(decay)` gives the clock's beat:
`play_view_b.cpp` 276 (Trio orbs "swell 6% on the beat"), 294 (Boost orbs), 317 and 320 (chevron, Cells "swell on the
beat"), `play_view_coop.cpp` 73 and 108 (chevron, beacon flare), `play_hud.cpp` 290 (the full meter's flash);
`stage.cpp` 1627 and 1644 (the bench stage's Pulse Ring and Lancer "every second beat" from `lastKickT_`);
`play_view.cpp` 706-719 (the no-band fallback synthesises a kick on every beat; fine for pictures of Tracks 1-6).

## Follow-ups after the lead's acceptance (2026-11-13)

- **Effects in key** (`95e9e6c`): the effects written in A minor (Boost bells, Upgrade chord and bells, Game over,
  Elite's fifth, Perfect Dash bell, Shield) move to the Track's root by a playback rate (-6..+5 semitones: Solar Wind
  -4, Reef +5, Aurora -5, Furnace -2, Zenith +3, Afterglow +4, Arcadia -2, Mirage +1) and take a baked major-third
  variant on Afterglow, Arcadia and Mirage (Boost's top note C#, Amaj9 with G# for the Upgrade and its bells, the Game
  over's C#). The UI ticks (confirm, back) stay as they are. On Furnace (G phrygian) the Upgrade chord's ninth (A over
  G) is outside the scale as written. Tested on all ten Tracks.
- **Start-up** (`26696ab`): after `pm clear` the Moto baked 375 one-shots (56.8 MB) in 6.1 s before the first frame,
  over PLAN's 5 s; a normal launch read the cache in 54 ms. Memory then: PSS 149 MB, native heap 75 MB. The risers of
  Tracks 2-10 are now baked on a worker while Track 1 plays, then cached. Numbers after the change: the report.
- **Teammates' leads at eight notes**: accepted, on the user's co-op listening list.

## M5 content sound (sound_m5.json): state

Published first: `src/audio/cues.h` (`386da70`): Mod 36, Riff 12, Boost 16, Fusion 8 (GAME_DESIGN numbering), the
`PlayerLoadout`, `Cue` (19 game events), `Recipe` (16) and 16 held-voice slots; `Band::setLoadout(slot, PlayerLoadout)`,
`cue()`, `playRecipe()`, `startVoice() / setVoice() / stopVoice()`, `lowPassBed()`. The engine takes every Mod's stacks,
all sixteen Boosts and the Fusions per player.

Built (`2d33052`): every recipe (tom, bell, gong, zap, glass baked on a grid of notes three semitones apart; sub drop,
woodblock, sweeps, reversed cymbal, whooshes, Gravity Rounds' thup once; saw glide, drone, pad voice and crackle live;
the roll and the reversed cymbal end on their tick; the timed low-pass bed) and every cue of cues.h as sound_m5.json
says, aggregated per 1/32 (Storm Coil per triplet eighth). Recipe levels are engine starting values (`recipeLevel()` in
`content_sound.cpp`: the voices' own mix levels, e.g. the tom is the pulse's 0.36). Sheets with what plays when:
`captures\m5audio\sheets\recipes.{mp3,wav,txt}` (48 s) and `cues.{mp3,wav,txt}` (28 s), dry on Track 1 like the effect
sheet (`raveform_headless --render recipes|cues [--track N]`). Aegis's tick, written "at 0.15" of the 0.05 tick, is at
about -63 dBFS in the sheet: inaudible in a mix.

The patterned parts (Riffs 4-12, the Mods' changes, the Boosts' layers, the Fusions), their sheets, the move into
conductor.json and the Moto with all of it playing: the next section.

## M5 upgrades in the band: Riffs 4-12, the Mods, the Boosts, the Fusions (second engineer, 2026-11-13)

Commits on `m5-audio`: `0274bf2` (first pass, the move into conductor.json), `dd38d06` (tests, the worst case's
cost), `3a048c7` (the content engineer's eight, the sheets: the user accepted the four upgrade sheets as rendered
here), `72d8a14` (the cold start), and the write-up after it. Everything plays on the band's own clock from each player's
loadout (`Band::setLoadout(slot, PlayerLoadout)`); nothing new runs unless someone owns an upgrade.

| Done criterion (brief) | Status | Evidence |
|---|---|---|
| Riffs 4-12, the Mods, the Boosts, the eight Fusions built from sound_m5.json | **Met**, with the engine's readings below | `content_sound.cpp` (upgradeBar / upgradeStep / leadUpgrades), `engine.cpp` (the lead's bends, the drums under Time Stretch, Freeze Frame's high-pass) |
| The three blocks moved into conductor.json; ProseReader counts right | **Done in this worktree** (lead: the same move on main) | `design/music/conductor.json`; `music_data.cpp` loadUpgrades (every number of the moved sentences read, counts checked) |
| Tests: each Riff's pattern, each Mod on the lead, each Boost and Fusion, no allocation | **Met** | `scripts\test.ps1`: audio 2677 checks, 0 failures (new: "Riffs 4-12 ...", "the Mods ...", "the Boosts' layers", "the Fusions ...", "the upgrades' worst case ...", "notes against the chord ...") |
| No upgrades: renders unchanged; calibration valid | **Met**: staged x10, mock, surge, medley bit for bit the same as `3fb6995`; `--calibrate --check` passes | the match table cannot have moved (the staged renders are identical) |
| Listening sheets for the four groups; cues re-rendered | **Met** | `captures\m5audio\sheets\{riffs,mods,boosts,fusions,recipes,cues}.{mp3,wav,txt}` |
| Moto worst case (`--band max`, Track 10) P95 <= 25% | **Met, narrowly**: 0.245, 0 underruns, 0 callback allocations, 106 voices | below (was 0.374 before the cuts) |
| Cold start after `pm clear` <= 5 s | **Met**: 3.2 / 3.1 s (was 6.6 / 6.0 s on this branch, 6.1 s before) | below |
| The match table 30 of 30 | **Met** | `scripts\render-audio.ps1 -Out captures\m5audio -SkipRef -NoListening`: 30 pass, every onset 0 samples off |

### The move (design data)

`conductor.json` now holds the "mods", "riffs" and "boosts" blocks of `sound_m5.json` (f7cd516), merged verbatim into
its own blocks in GAME_DESIGN order (the first slice's entries kept as they were); the three notes merged into one each.
`sound_m5.json` is not in this worktree (it never was): on main it keeps "fusions", "recipes", "effectsInKey" and "where".
The engine reads every number of the moved sentences through ProseReader with its count checked (an edit that changes a
sentence's shape fails loudly at load). Numbers written as words are the engine's reading and are listed in
`loadUpgrades`: Lancer's charge "three beats" (a beat after the last shot), Rear Guard's "two" and "five" degrees,
Accent's beats per stack, Echo's "one beat", Carousel's beats 1 to 4. Lancer's level-3 pattern is read from the quoted
pattern in its "says". The calibration hash does not include conductor.json: no recalibration.

### What plays (and where the engine had to read the design)

Riffs (heard while any player owns them, the highest level; silent in the Break and the Build's last bar like the first
three; Hyper doubles every 16-step pattern):
- **Lancer**: the full chord (the stab, length and decay x 2.0) on beat 4, from level 3 on beats 2 and 4; the charge is
  the Track's bar-long reverse swell (the Break's) cut to the span from a beat after the last shot (three beats at level
  1, one at level 3) at 0.3 of the swell's level.
- **Minelayer**: a closed hat on step 8 (the hat's own level: none given), then on the next One the clap (0.8) and the
  impact (0.4); the first bar after the pick has no blast (nothing was laid). The impact is reported as an Sfx event,
  not an Impact (whose heavy haptic is the Drop's).
- **Storm Coil**: as built (the cue), its level now read from conductor.json.
- **Swarm**: bells on the 1/32 grid from the One, "the chord's arp from step 0, one note a missile" (24 at level 5:
  notes past the arp's 16 an octave up), panned left to right across the volley, on the stab bus. **For the lead's
  ruling**: the arp is not a scale, so "a rising run" rises for four notes and then follows the arp's own shape (A C E A
  E C E C ... on Am). A strictly rising run would need another rule (e.g. the arp's notes sorted, octave by octave).
- **Singularity**: on the band's clock: the swell over beats 2-4 of every bar whose song index + 1 is a multiple of 4,
  the boom (impact 0.8) on the next One. The game's well should follow that clock (the Bar event's index).
- **Mirror**: the player's own note an octave up at 0.4-0.7 by level.
- **Comet**: a whoosh out on the One and a new baked return whoosh (3 kHz -> 400 Hz) on three, each stretched to the
  two beats it has, 0.3 x the pattern's velocity.
- **Sweeper**: the drone recipe on the chord's bass an octave down (the recipe's "an octave under the bass"), following
  the chord bar by bar over 20 ms; its cutoff 300 -> 2400 Hz as ((1 + cos 2 pi u) / 2)^2 of the arm's place u in its
  two-bar turn, twelve o'clock on the One of every second bar of the song. Its "second arm" level has no sound in the
  design: not built.
- **Flak**: the snare at 0.5 on two and four; from level 4 the clap (at 0.5 of the clap's level: none given) a 1/32
  later.

Mods (on the lead of the player who owns them):
- **Seeker**: each note glides in from the player's last over 12 ms a stack (stacks read as cumulative: 12, 24, 36 ms).
  **Wave**: vibrato 5.5 Hz, 18 cents a stack, its phase from the song's clock (a player's notes sway together). Both at
  the control rate (once every 32 samples of a voice's age), so they render the same whatever the callbacks.
- **Heavy Step**: lead steps k with k mod 4 = 3 on the player's grid, the note an octave down on the bass patch at 0.8,
  held a grid step. **Accent**: beat 1 / 1 and 3 / every beat at 1.4 with the stab at 0.3. **Echo**: the delay one
  beat, mix + 0.15 a stack, from the bar line (the highest stacks of the players: the delay is the bus's).
- **Rear Guard**: steps 2, 6, 10, 14 two scale degrees under the arp at 0.5 (the second stack: steps 1 and 9, five
  under), on the sixteenth grid while fire is held, panned to the other side of the lead.
- **Syncopation**: notes off the quarter-note beat x (1 + 0.3 a stack) with a P2 square blip an octave up at 0.3 of the
  lead and 0.3 of its length (levels not given: Ricochet's blip). **Bar Line**: every One, firing or not, the stab at
  0.5 and a zap on the root (the zap's own level).
- **Crescendo**: +0.2 dB a bar line while owned, to +2 dB; the pad's cutoff opens by the same ratio (the local
  player's); `Cue::PlayerHit` drops both at once (the pads that are sounding too). Stacks do not raise the +2 dB (the
  design gives one number).
- **Kick Pulse**: a tom an octave above the Track's kick pitch at 0.2 x the kick's velocity on every kick the pattern
  plays (tested on Afterglow's half time). **Afterparty**: the Surge's second gear two bars longer a stack and every
  Surge gear one step faster; the main thread's lead rule (`computeShots`) uses the same rule (`leadGridIndex`).
- The cue Mods (Fork, Arc, Burn, Shatter, Gravity Rounds, Aegis, Last Light ...) read their numbers from conductor.json.

Boosts (any player's for the shared layers, the player's own for its lead):
- **Colossus**: the bass's octave below at 0.5. **Time Stretch**: the drums (kick, clap, hats) at half rate, their 16
  steps over two bars (the sidechain follows the kicks); bass, pad, lead and Riffs keep their grid.
  **Freeze Frame**: the glass chord at its start; the music (not the effects) through an 800 Hz high-pass, faded in and
  out over 50 ms, while any player's lasts. **Magnet Storm** (first slice, was missing): the local player's Shard run
  climbs two notes a Shard.
- **Chain Storm**: three zaps on every beat. **Chorus**: the scale's fifth above and the octave below at 0.4.
  **Beam**: decay and length x 4, velocity x 0.6 ("softly": engine choice), a drone on the arp's first note at 850 Hz
  (engine choice: the middle of the Sweeper's sweep). **Fractal**: the octave a 1/32 later (0.3), the double octave a
  1/32 after (0.15). **Carousel**: toms on root, third, fifth, octave (the chord's own tones) on beats 1-4.

Fusions (a Fusion replaces its ingredients' sounds for the player who owns it):
- **Helix Array**: the Split harmony sways in the opposite phase to the lead. **Standing Wave**: each accented note held
  a bar as a pad voice (at 0.5 of the pad voice: none given); no Accent stab or x 1.4, no Echo delay.
  **Tesla Bloom**: three zaps on each ring at 0.35 (none given: Chain Storm's); Arc's zaps are dropped.
  **Shard Halo**: glass ticks double the Orbitals' shaker, in turn in the seventh and sixth octave ("an octave apart");
  Shatter's glass is dropped. **Downbeat Cannon**: the chord at 1.2 and the impact at 0.5 on every One; Lancer's
  pattern and On-Beat Crit's octave are dropped.
- **Collapse Field**: a reverse swell (0.5: none given) over the last beat into each detonation. **For the lead's
  ruling**: "replaces its ingredients' sounds" and "runs into the mines' detonation" disagree; the detonation (Minelayer's
  hat, clap and impact) is kept and Gravity Rounds' thup is dropped.
- **Murmuration**: a 1/32 trill of bells on the chord's third and fifth at 0.25 while the Riffs play; Swarm's volley
  and Seeker's glide are dropped. **Quartet**: the third, fifth and octave above at 0.4; Mirror and Rear Guard dropped.

### Listening sheets (`captures\m5audio\sheets\`, each `.mp3`, `.wav`, `.txt` with the times)

`riffs` (2:45), `mods` (3:56), `boosts` (2:59), `fusions` (1:49): through the conductor, each entry a few bars without
the upgrade and then with it, on a Track that shows it (one player; the Groove held at a low intensity where the
upgrade rides on the lead; Afterparty in the Drop, named bar by bar as the director does; what the game would post,
such as Storm Coil's strikes or a hit for Crescendo, fed every 1/60 s). For the Fusions, "without" is both ingredients
maxed. `recipes` (1:13) adds the content engineer's eight; `cues` (0:28) has Aegis's tick at 6 dB under the Shard
chirp. `raveform_headless --render riffs|mods|boosts|fusions|recipes|cues --out x.wav`.

Each upgrade's own level in the mix (`raveform_headless --render upgrade-levels`: the "with" bars rendered with and
without it, the difference against the music, RMS dB; for those that change the lead's notes the difference also
counts the shifted phases of the notes, so it overstates them):
- Loud by the numbers: **Colossus's ram toms** +3.4 dB over the music (the cue's tom at the pulse's 0.36 on the effects
  bus, which the music's output gain does not lower; a ram every ~0.3 s in the sheet), **Carousel** -2.5, **Downbeat
  Cannon** -2.0, **Black Hole** -0.4 (the blast). Freeze Frame takes 31 dB off the music under 250 Hz (as written).
- Quiet: **Kick Pulse** -21 to -23 dB, its peak -27 dBFS under a kick near -6 dBFS: it may be masked by the kick it
  doubles. **Shard Halo** -23, **Accent** -25 (one note +3 dB and a 0.3 stab a bar). Everything else -8 to -18.

### The content engineer's eight (cues.h, appended; ids for the content engineer)

`chordNote(amount, octave, NoteFrom::ScaleSteps | Semitones | ChordTone)` (value >= `kChordNote` = 0x10000) for any
`midi` of playRecipe / startVoice / setVoice, resolved at the tick it plays (chord tones from the chord's own notes:
Aurora's B major gives D#, not the scale's D). New recipes (`Recipe`, values 16-21): **RubberString** 16 (the bass
patch, twice its decay, p1 sixteenths; default the chord's bass an octave up), **Impact** 17 (p1 semitones up; or a
note for its 38 Hz sine), **Hat** 18 (closed; p1 = 1 open), **TomRoll** 19 (p1 beats ending on the tick, rising p2
semitones, default 2 and 12), **DrivenKick** 20 (tanh(p1 x kick), default 3), **RimShot** 21 (baked on the grid of
pitched recipes). `Band::setVoice(voice, param, gain, midi, glideSeconds)` glides a held voice (pad voice, drone) to a
note; a PadVoice's start `param` > 0 is its attack in seconds (choir.md's 20 ms). Not done: choir.md's "reverb send
0.5" (the content voices share the effects' send).

### Moto: the worst case (`bench_idle --band max --track 10`, our APK, the lock `m5audio`; shared phone)

`--band max` (engine.h `maxLoadSetup` / `maxLoadFrame`; offline `raveform_headless --render max --track 10 --profile`):
the full band (the Drop held, intensity 1, four players firing) with every Riff at level 5 shared out four slots a
player and Mirror with each, every Mod that adds sound at its cap for all four (Split 3, Double-Time 2, Pierce 3,
Overcharge 5, On-Beat Crit 3, Seeker 3, Heavy Step 3, Accent 3, Echo 2, Wave 2, Rear Guard 2, Syncopation 2, Bar Line
3, Crescendo 2, Kick Pulse 3, Afterparty 2, Fork, Arc, Burn, Shatter, Gravity Rounds), a Boost each (Fractal, Fractal,
Chorus, Prism), and every 1/60 s the full band's kills and Shards plus Storm Coil's strikes, Arc's chains, Fork's
answers, Shatter's bursts, Gravity's hits and four players' Burn counts.

| Run | Callbacks | Load P50 / P95 / P99 / max | Callback us P50 / P95 / max | Underruns | Callback allocations | Voices peak |
|---|---|---|---|---|---|---|
| Before (predecessor): full band, Track 10, 120 s | 26,628 | 0.192 / 0.224 / 0.286 / 0.625 | 950 / 1070 / 1320 | 0 | 0 | 72 |
| This build: full band, Track 10, 60 s | 13,314 | 0.196 / 0.231 / 0.308 / 0.953 | 970 / 1100 / 1690 | 0 | 0 | 72 |
| Worst case, first build, 60 s | 13,314 | 0.279 / **0.374** / 0.435 / 0.854 | 1370 / 1800 / 2190 | 0 | 0 | 138 |
| Worst case, after the first cuts, 60 s | 13,314 | 0.235 / **0.323** / 0.393 / 0.801 | 1150 / 1540 / 2070 | 0 | 0 | 133 |
| **Worst case, final, 120 s** | 26,629 | 0.192 / **0.245** / 0.321 / 0.678 | 940 / 1170 / 1880 | **0** | **0** | 106 (lead 37 offline) |

The final 120 s had one disturbed second (89 foreign process starts on the phone). The margin is 0.005: a heavier
case (two Boosts each, Fusions) or a busier phone may cross it. P99 stays above 25% as in M2 and the full band.

What it cost and what was cut (offline profile, ms of engine time a second of audio: full band 28.7, worst case first
build 57.4, final 27.9):
1. **A bug in the lead's cap** (not a cut): a note that starts later (Fractal's, Fork's) cut the player's oldest note at
   its own later frame, so later notes counted that note again and the player's notes piled up (96 lead voices and
   13,000 steals in the first run). Now a cut note is not counted again (`culled`), and a later note makes its room when
   it is placed. Without notes that start later this changes nothing (the full band was bit for bit the same).
2. **A teammate's lead on this device is its melody**: the arpeggio note on its grid, at most four notes held (was 8,
   the predecessor's cut); its harmony (Split, Prism, On-Beat Crit) and every extra its upgrades add (Mirror, Chorus,
   Fractal, Quartet, Syncopation's blips, Rear Guard, Heavy Step's bass, Standing Wave's pads, Fork's answers, the bounce
   blips) are its own device's; its Overcharge raises its level but does not drive it. A teammate is at 0.35 or 0.15 of
   the local lead. This also changes the full band (`--band full`) and plain co-op: four notes held is the whole note
   at 1/16 at 174 BPM, a beat of them at 1/32. **For the lead's ruling** (the user listened to single-player sheets).
3. **Exact or below hearing**: Overcharge's drive by a rational tanh (Lambert's continued fraction, off by < 2e-5; the
   local player's lead only; the sheets were rendered with it); the upgrades' bass notes the same; the bends computed once
   a knot; the sample voices' interpolation with the voice in locals (bit for bit); the upgrades' stabs on one tick one
   voice (summed velocity); hits, bursts and strikes aggregated across players (the design's own "one sound of a kind on
   a 1/32 step"); one Burn crackle for every player (density from the sum), its bursts drawn burst by burst with uniform
   noise. The sheets the user accepted (`3a048c7`) were rendered after all of these.

### Cold start (`pm clear`, `am start`; shell `date +%s.%N` to the "play: first frame" line of `logcat -d -v epoch --pid`)

| Launch | First frame after am start | TOTAL PSS | Notes |
|---|---|---|---|
| Predecessor (`26696ab` measured before it) | 6.1 s | 149 MB | 375 one-shots baked before the first frame |
| This branch before `72d8a14`, cleared x2 | 6.61 / 5.99 s | 149 MB | 394 one-shots, 36.2 MB baked in 5.12 s before the first frame |
| **Final, cleared x2** | **3.20 / 3.12 s** | 141 / 143 MB | the later Tracks' kits and risers baked behind the game in 4.0 s, then cached |
| **Final, normal x2** (the cache) | **0.90 / 0.97 s** | 145 / 146 MB | |

The bake's own log now splits its time; on the desktop the thirty drum kits were half of it (1.2 of 2.2 s). Shared
phone; the screen was asleep before the second set and was woken with KEYCODE_WAKEUP (the launcher in front, no
keyguard).

### Not done or not verified (this part)

- By ear: the recipes and cues sheets have not been heard; the upgrades' levels are the design's starting values and
  the engine's choices listed above (the user accepted the four upgrade sheets).
- The worst case's margin is 0.005 (above). Not measured: two Boosts a player, the Fusions in the worst case, the 12R
  and the tablet.
- `seq latest` (an M2 thread test) failed once under load from a parallel Gradle build and passed on its rerun.
- Main-checkout file touched by mistake: `src/audio/engine.cpp`'s modification time (a PowerShell read-and-write ran in
  the main checkout's folder; its content is byte for byte what it was: `git diff --ignore-cr-at-eol` empty).

## Not done or not verified

- By ear: the user has not heard the engine's ten Tracks (the match is by numbers; the medley is for that).
- The game's director still plays three songs and the loop (`kSongs = 3`, the other engineer's).
- `CLAUDE.md` still says `--track 1|2|3`.
- The one-shot bank is 57.5 MB in memory and in the cache file (394 one-shots); the four-bar risers are half of it
  (int16 risers or baking per Track on demand would halve it). First launch on the Moto: 3.1-3.2 s to the first frame
  (the M5 upgrades section).
- 12R and tablet: not attached, no numbers.
