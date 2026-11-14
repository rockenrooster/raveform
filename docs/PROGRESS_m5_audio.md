# M5 audio: the fights' pending sounds, play test 3's sound notes (2026-11-13)

Branch `m5-audio` (worktree `Raveform-m5-audio`), merged up to main `a008b71`. The audio engineer's notes for this round; the
lead folds them into `docs/PROGRESS.md` at the merge. Phone numbers come from the **shared** Moto One 5G Ace (`ZY22BXMJG3`),
under the phone lock (`m5audio`), with this branch's APK deployed right before the run.

| Asked | Status | Evidence |
|---|---|---|
| Every row of `src/game/pending_sounds.cpp` plays what its page asks | **Done**: 132 rows, 0 with `pending` | `raveform_headless --render fights` prints the count |
| Colossus's ram toms level with the music | **Done**: +3.4 dB -> **-0.0 dB** | `--render upgrade-levels`, "Colossus: and a tom on the root for each enemy rammed" |
| Kick Pulse's tom against a full Drop | **Audible, not raised**: in the 85 ms after each kick its 250 Hz octave is **-6.0 to -7.2 dB** under the rest of the mix there (First Light, Afterglow, Last Light) | `--render kick-pulse` |
| The Choir's pad voice, reverb send 0.5 | **Done** (the Voices, a Voice's death, the held note, the Reprise's Choir solo) | content voice send (`VoiceStart mods[5]`) |
| The worst case grown to two Boosts a player and Fusions, the fights on top | **Built**; **Moto P95 31.5%: over the 25% limit** (below) | `--band max`; offline `--render max` / `max-upgrades` |
| Play test 3, 1: the Nova's distortion | **Done**: a look-ahead limiter replaces the soft knee; THD+N of the worst instant **0.03-0.66%** (the old knee 0.02-5.8%) | `--render nova-check` |
| Play test 3, 2: a chain of kills is one sound | **Done**: one family a 1/32 step, never louder than one large kill, 0.7 a further step of the beat down to 0.3 | test "the kill family" |
| Play test 3, 3: hit confirmation on bosses | **Not done**: its rows are not on main yet | |
| Play test 3, 4: Extra Life, the Orbitals' ring | **Engine done** (four cues); **the game does not post them yet** (no simulation events for them on main) | `audio::Cue` 19-22 |
| Play test 3, 5: the Short form | **Checked, nothing to change**: every Track's Groove and Drop are 8 bars in its data; the engine's literals are the content engineer's change, not on main yet | below |
| `scripts\test.ps1` | Audio suite (2714 checks) and the calibration pass; **the simulation, determinism and network parts fail on main `a008b71` itself** (`design/arena.json`'s Track 1 has no structures; the loader refuses it), not on anything here | |
| Listening sheet | `captures\m5audio\sheets\fights.wav` (4:06, 16-bit) and `fights.txt` | |

## What was built

### Engine (`src/audio`)

- **Recipes, appended** (`cues.h`, values 22-30): `Clap`, `Snare` (a ghost note at a low gain), `Stab` (the band's stab on the
  stab bus: the chord at the tick or up to five notes, `stabNotes()`; velocity up to 1.2), `Crack` (a 3 ms burst of 3-9 kHz
  noise into the glass's partials), `CrackleShot` (a baked crackle, flat or swelling into its tick), `DashReverse`, `Thup`
  (Gravity's thup at a note), `SubSine` (held), `EliteGated` (the arrival's fifth, keyed, gated on the eighths: a sample voice
  gate with 2 ms edges). The gong has a 0.7 s version (`p1` = 1: Armored's "short"). Cache version 8; 407 one-shots.
- **Notes**: `NoteFrom::Arp` (the bar's arpeggio note k), `ArpRank` (its distinct pitches from the lowest), `Lead` (a player's
  last lead note); `chordNoteAt()` resolves a note some ticks before it plays (an Arpeggiator block's own note when it reaches
  the ship's row).
- **Commands, appended**: `CutBed` (buses silenced for a window from a tick, 8 ms ramps; the hats and shaker skipped by the
  sequencer), `HighPassBed` (the music high-passed for a window, 50 ms fades), `PadTone` (the band's pad cutoff and gain),
  `DelayBoost` (the lead delay's feedback and repeats), `LeadEcho` (a player's lead note played again on the fight's own lead,
  `kFightLeadPlayer`, at most 3 at once: as recorded, mirrored about its chord's root, reversed (a swell cut off), or a note
  given; octaves, a bend, a fixed cutoff; each player's notes are kept, 64 a player, with their ticks). A `Recipe` with `b = 1`
  is aggregated (one shot of it at its note on a 1/32 step, 1 - e^(-n / 2.5), the cues' cap of 1.6 x). `VoiceStart` takes a
  reverb send and a chordNote it follows bar by bar; `VoiceSet` a new pan.
- **The output stage** (play test 3, 1): a look-ahead limiter. 128 frames (2.7 ms) of delay; each 32-frame segment, aligned to
  the song's frames, may take ceiling / its peak (ceiling 0.8, -1.9 dBFS as before); its gain is the least of 40 segments
  before it (27 ms: a 38 Hz sine's period, so the gain is flat over a sub's cycle) to 3 after, released over 120 ms, and moves
  linearly across the segment. The old knee stays above it as a safety (0.85 to 0.95), not reached by the check. The band's
  heard clock takes the delay off (`Engine::outputDelayFrames`); offline renders are shifted by it (onsets unchanged); the
  raw output (calibration) has none.
- **The kill family** (play test 3, 2; GAME_DESIGN 6.6): kill pops of every size, Supernova's thud, Shatter's and Freeze Frame's
  glass, Arc's zap, Confetti's run and Burn's crackle are gathered on each 1/32 step and played together: their gains summed
  never over one large kill's (the crackle's level scaled with them), each further step of the same beat at 0.7 of the one
  before, down to 0.3, from 1 again on the next beat.
- **Cues, appended** (play test 3, 4): `ExtraLifePickup` (the Cell's note, the player's lead at 0.85 an octave over the root,
  and the fifth above it), `ExtraLifeUse` (the Nova's blast and the Track's tonic chord on the stab), `OrbitalsBreak` (the
  glass crack two octaves down), `OrbitalsRelight` (value = the shard: the shaker a scale step higher each).
- **Fixes found on the way**: Standing Wave took its slot on the block being planned (cutting what rang there at a block
  boundary: random bursts rendered differently once all its slots were busy); it now takes it on its own frame. A reversed
  cymbal asked for late still ends on its tick (what is left of it plays).
- **Cost**: the content voices' saw pairs run a knot at a time in locals (the PolyBLEP by a reciprocal); the players' Beam
  drones (one note, one cutoff, one bar line) are one voice at the sum of their levels; a teammate's Feedback Echoes are its
  own device's (they would be 18 dB and more under the lead here), as its extra lead notes already were.
- **The worst case** (`maxLoadSetup` / `maxLoadFrame`): two Boosts a player (the local player Fractal and Chorus; the
  teammates the shared layers: Beam and Colossus, Beam and Chain Storm, Beam and Freeze Frame), the local player's eight
  Fusions, and the fights' sounds on top in turn, ten seconds each: the arena's (three Rift drones following the chord, their
  cutoff swept; the Storm Cell's crackle; a Lens's glass every 1/32 step, a Storm Cell's zaps a beat, a Rift's toms a bar) and a
  fight's (Feedback's Howl: sixteen delay taps, the pad's tone, its echoes a note every 1/16; Event Horizon's sub; Cadence's stab
  at 1.2 and a bell arpeggio a bar; a cut of the pads and bass every two seconds). `--band full` and `stress` are the M2-era
  loads and are unchanged; `max` is their superset.
- **Tools** (`raveform_headless`): `--render fights` (the sheet), `kick-pulse`, `nova-check`, `max-upgrades` (the worst case
  without the fights); the profile has a `content` stage.

### The rows (`src/game/pending_sounds.cpp`)

The engine's commands go through `audio::CommandSink` (the band in the game; an engine offline for the sheet). What lasts
while something lasts follows the world frame by frame (`FightScene`, read in `updateFightVoices`): held voices are started,
glided, re-panned and released there. The rows that were pending now play:

| Cue | Now |
|---|---|
| Event Horizon, eat | the arp note k (the meal) an octave down on the Track's lead patch, bending down a fifth; **the drone**: a sub sine on the root, a semitone lower each three cells of the meal, springing back as the bar line empties it |
| Event Horizon, starved | the pads and bass cut for the first beat, a reversed cymbal swelling through it |
| Feedback, echo | the player's note of the Echo's delay before, an octave down, at 1.2 kHz, -9 dB, far over; the second Echo -15 dB at 700 Hz |
| Feedback, open | the root two octaves up held while the Head is open, a semitone higher each tenth of health lost; the pad opened (x 1.6) while open, ducked (-6 dB) and dull (x 0.55) while closed; Howl: the delay's feedback +0.25, sixteen repeats |
| Moth Queen | the open hat panned round with the court's turn; the clap on the burst; open: the bed down to kick and pad |
| Brood Mother | the threads: a pad voice for each living broodling (root, third, fifth, seventh: four at most), following the chord; the bell at the break |
| Blink Star, fire | the run's five notes as one stab with the zap |
| Cadence | the pillar's root held a bar and its chord (the progression's) on the stab at 1.2; fans on the firing pillars' own roots; Inversion high-passed at 800 Hz for a bar; death: each pillar's chord, then the tonic with the gong |
| Arpeggiator | each block's bell when it reaches this device's ship's row (its own note: the arpeggio's of its step, the second machine's the chord tone); the second machine's arrival on the stab |
| Fata Morgana | the storm's crackle held while the ship is outside the calm (the low-pass and the whoosh as before); the blink a ghost snare; the leap the arpeggio's notes up, as chirps |
| Kaleidos Reprise | Feedback's solo: the player's notes of a bar before, reversed; the last pattern doubled by the lead at 0.5; death: the tonic held from the death to a bar past the card |
| Mirror Mimic | each lead note mirrored about its chord's root a 1/32 later at 0.4; its Dash reversed |
| Twin Tether | the beam's drone, its cutoff opening as it comes round to the ship, silent while slack |
| Weaver Prime | a crackle swelling over the beat into the cage's going off, the stab on it |
| Deep Well | the thup a scale step higher each ring lit (aggregated); a reversed cymbal into each ring the bar's fill will give back (sent a beat and a half ahead), the stab on the ring |
| Elites | Armored's short gong, Splitting's crack, Volatile's crackle, Regenerating's two bells, Magnetic's chirps falling, Phasing's gated fifth; the chosen ones (no sound on the page) keep what they played, marked "chosen" |
| Arena | the Vault's bells up the chord with the glass; the Pylon's three bells; each open Rift's drone on the chord's bass (the nearest three; a Rift Surge's three a fifth apart), its cutoff rising through the Build; a Rift's four bodies one aggregated tom; the Bumper's stab with its zap; the Lens's glass a fifth over its player's lead (aggregated); the Comet's announcing glide (the other events' pages name none); the Comet's Shards one aggregated bell a beat; the Eclipse's low-pass let go at its end; a Convoy tom a beat for each carrier, panned to it; the Storm Cell's zaps on the chord's three notes (aggregated) and its crackle, denser and louder as it nears |

### Hooks outside my files

- `src/game/play.cpp`: `updateFightVoices` is given the world, this device's seat and the band's offset (one call).
- `CMakeLists.txt`: `src/game/pending_sounds.cpp` is built into `raveform_headless` (the sheet).
- `src/tools/audio_render.cpp`, `tests/audio_tests.cpp`: the sheet, the checks, the tests.

## Numbers

### The Moto (`bench_idle --band max --track 10`, 55 s after 5 s, our APK, the lock; shared phone)

| Run | Callbacks | Load P50 / P95 / P99 / max | Callback us P50 / P95 / max | Underruns | Callback allocations | Voices peak |
|---|---|---|---|---|---|---|
| Before (the last round's worst case: a Boost each, no Fusions, no fights), 120 s | 26,629 | 0.192 / 0.245 / 0.321 / 0.678 | 940 / 1170 / 1880 | 0 | 0 | 106 |
| **This round's worst case**, 55 s | 12,207 | 0.246 / **0.315** / 0.393 / 0.780 | 1200 / 1500 / 1830 | **0** | **0** | 140 |

The run was valid (the app kept focus); the panel at 60 Hz, 0 frames missed.

**Over the limit.** Offline (`--profile`, Track 10, ms of engine time a second of audio, about +-5% between runs): the last
round's worst case 28.6; the grown loadout alone (`max-upgrades`) 33.6-34.2, of which Standing Wave's eight held pad voices
about 3, Colossus's doubled bass and the Beam drone about 1.5; with the fights' sounds (`max`) 36-38, the arena's or a fight's
held voices about 2 of it. The limiter and the kill family cost nothing measurable. So about four points of the Moto's seven
come from the grown loadout, two from the fights' sounds. What would bring it back, for the lead's ruling (none taken):
Standing Wave at six voices (its seventh-oldest note is still at -9 dB when cut); a teammate's Beam drone as its own device's;
the local lead's cap at 10 notes while it owns Quartet; the Rifts' drones two at most and Feedback's echoes two at most.

### The worst instant (`--render nova-check`)

THD+N below 1 kHz over the 400 ms from the blast (a gain fitted to each 5 ms piece, so a gain that moves slowly is not counted),
the Drop held at intensity 1, a player firing, eighty kills over the 0.35 s after the blast, effects at full:

| Track | Blast | New stage | Old soft knee (same input) | Into the stage | Out |
|---|---|---|---|---|---|
| First Light | Nova | 0.45% | 0.07% | -1.5 dBFS | -2.2 dBFS |
| First Light | Black Hole | 0.18% | 4.72% | **+3.9 dBFS** | -2.3 dBFS |
| First Light | Last Light | 0.03% | 0.02% | -2.3 dBFS | -2.4 dBFS |
| Last Light | Nova | 0.10% | 0.05% | -2.0 dBFS | -2.0 dBFS |
| Last Light | Black Hole | 0.66% | 5.77% | **+3.6 dBFS** | -2.1 dBFS |
| Last Light | Last Light | 0.15% | 0.51% | -0.6 dBFS | -2.0 dBFS |

All under 3%. Black Hole's blast with its sub drop is the instant that overloads the stage; with the kill family the kills no
longer pile on top (the input above was measured with it).

### Tests

`raveform_tests`: 2714 checks, 0 failures (new: the fights' sounds: the cut, the high-pass, the pad's tone, a fight's lead
(recorded an octave down, mirrored, bent, its cap), aggregation, the gate, the five-note stab; the kill family; the worst case
with random bursts and the fights' feed, no steal, no allocation). Calibration current (no loudness moved: the staged renders
are the same; `kDspVersion` unchanged).

## The Short form (play test 3, 5)

Every Track's `arrangement` in `design/music/track01-10.json` already has Groove 8, Build 4, Drop 8, Break 4 bars, every
progression is four chords, and what the conductor plays in a Groove or a Drop repeats within 8 bars (the bass's alternate
bar, the fill on the eighth, the Surge's two gears by halves of the section). No Track needed its phrases split between odd and
even cycles. The engine's two literal copies of the section lengths (`planConductorBar`) are the content engineer's change on
main; when it arrives here nothing else in the band needs to move.

## Not done or not verified

- **Hit confirmation** (play test 3, 3): its rows are not on main; they sound when they reach this branch.
- **Extra Life and the Orbitals' ring**: the band has the four cues; the simulation has no events for them on main, so the game
  does not post them yet (`band.cue(Cue::ExtraLifePickup ...)` from wherever the pickup is decided).
- **The budget**: 31.5% on the Moto, over 25% (above).
- **By ear**: nothing here has been heard (this PC is silent). The sheet is for the user.
- `scripts\test.ps1`'s simulation, determinism and network parts fail on main `a008b71` (the arena data); the audio parts pass.
- The tablet and the OnePlus: not attached.
