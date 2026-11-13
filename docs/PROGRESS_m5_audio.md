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

Not built yet (next): the patterned parts: Riffs 4-12's instruments (Lancer, Minelayer, Swarm, Singularity, Mirror,
Comet, Sweeper, Flak; Storm Coil's strikes are built), the Mods' changes to the lead and mix (Seeker, Heavy Step,
Accent, Echo, Wave, Rear Guard, Syncopation, Bar Line, Crescendo, Kick Pulse, Afterparty), the Boosts' layers
(Colossus, Time Stretch, Freeze Frame, Chain Storm, Chorus, Beam, Fractal, Carousel), the eight Fusions, their sheets,
the move of the three blocks into conductor.json, and the Moto with all of it playing.

## Not done or not verified

- By ear: the user has not heard the engine's ten Tracks (the match is by numbers; the medley is for that).
- The game's director still plays three songs and the loop (`kSongs = 3`, the other engineer's).
- `CLAUDE.md` still says `--track 1|2|3`.
- The one-shot bank is 48.3 MB in memory and in the cache file; the four-bar risers are half of it (int16 risers or
  baking per Track on demand would halve it). First-launch bake time on the Moto not measured (2.5 s for three Tracks
  in M2; about 2.2 times the desktop's work now).
- 12R and tablet: not attached, no numbers.
