# M5 audio: the fights' pending sounds, play test 3's sound notes (2026-11-13)

Branch `m5-audio` (worktree `Raveform-m5-audio`), merged up to main `a008b71`. The audio engineer's notes for this round; the
lead folds them into `docs/PROGRESS.md` at the merge. Phone numbers come from the **shared** Moto One 5G Ace (`ZY22BXMJG3`),
under the phone lock (`m5audio`), with this branch's APK deployed right before the run.

| Asked | Status | Evidence |
|---|---|---|
| Every row of `src/game/pending_sounds.cpp` plays what its page asks | **Done**: 134 rows (with main's two hit cues), 0 with `pending` | `raveform_headless --render fights` prints the count |
| Colossus's ram toms level with the music | **Done**: +3.4 dB -> **-0.0 dB** | `--render upgrade-levels`, "Colossus: and a tom on the root for each enemy rammed" |
| Kick Pulse's tom against a full Drop | **Audible, not raised**: in the 85 ms after each kick its 250 Hz octave is **-6.0 to -7.2 dB** under the rest of the mix there (First Light, Afterglow, Last Light) | `--render kick-pulse` |
| The Choir's pad voice, reverb send 0.5 | **Done** (the Voices, a Voice's death, the held note, the Reprise's Choir solo) | content voice send (`VoiceStart mods[5]`) |
| The worst case grown to two Boosts a player and Fusions, the fights on top | **Built**; Moto P95 31.5% first; with the lead's cuts and the new caps 29.0%; with held pads and drones at one voice **28.0%, 0 underruns** (the lead's band of 25-28%: accepted, over the 25% goal) | `--band max`; offline `--render max` / `max-upgrades` |
| Play test 3, 1: the Nova's distortion | **Done**: a look-ahead limiter replaces the soft knee; THD+N of the worst instant **0.03-0.66%** (the old knee 0.02-5.8%) | `--render nova-check` |
| Play test 3, 2: a chain of kills is one sound | **Done**: one family a 1/32 step, never louder than one large kill, 0.7 a further step of the beat down to 0.3 | test "the kill family" |
| Play test 3, 3: hit confirmation on bosses | **Done** (main `61a7f09` merged): a weak point's bright tick climbing the chord through a beat's steps, armour's dull knock; two new recipes; on the band's next 1/32 step at the sim's chain gain | sheet 3:53 and 4:00 |
| Play test 3, 4: Extra Life, the Orbitals' ring | **Done**: the game posts the four cues from main's events (the pickup; the stab on the free Nova's blast; the ring broken; each shard relit, polled from `World::orbitalsLit`) | sheet 4:08 and 4:15 |
| Play test 3, 5: the Short form | **Checked, nothing to change** in the data; main's engine literals (Standard retired) merged | below |
| `scripts\test.ps1` | **All passed** after merging main `61a7f09`: audio 2716 checks, calibration current, simulation 5875, determinism, network 131,366, both 6-minute loopback soaks | |
| Listening sheet | `captures\m5audio\sheets\fights.wav` (4:36, 16-bit, -14.2 LUFS) and `fights.txt`; play test 3's hits, Extra Life and ring from 3:53 | |

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
  and the fifth above it), `ExtraLifeUse` (the Track's tonic chord on the stab, posted for the tick of the free Nova's blast:
  the Nova itself, inhale and blast, is the game's own), `OrbitalsBreak` (the glass crack two octaves down), `OrbitalsRelight`
  (value = the shard: the shaker a scale step higher each).
- **Recipes, appended** (play test 3, 3; values 31-32): `Tick` (a bright tick at a note: the sine with its second and third
  harmonics, 25 ms, over a 1 ms click of 4-10 kHz noise; baked MIDI 84-108), `Knock` (a dull low knock, unpitched: a sine
  falling 220 -> 150 Hz, 25 ms, a woody partial at 2.3 x and a 4 ms thud of 250-1200 Hz noise). Levels 0.24 and 0.30: in the
  sheet's Drop (Solar Wind, intensity 0.8, the player firing) a beat's first tick is **+2.0 dB** over the music's median in
  1-2.5 kHz (20 ms windows, `--render fights --stems`), the knock **-2.5 dB** in 80-300 Hz and -3.5 dB in 300-1000 Hz; the
  later steps of a beat fall under it (0.7 a step). Starting values for the user's ear. Cache version 9.
- **Fixes found on the way**: Standing Wave took its slot on the block being planned (cutting what rang there at a block
  boundary: random bursts rendered differently once all its slots were busy); it now takes it on its own frame. A reversed
  cymbal asked for late still ends on its tick (what is left of it plays).
- **Cost**: the content voices' saw pairs run a knot at a time in locals (the PolyBLEP by a reciprocal); a teammate's Beam
  drone and its Feedback Echoes are its own device's, as its extra lead notes already were; and the lead's rulings' cuts
  (Numbers, below).
- **The worst case** (`maxLoadSetup` / `maxLoadFrame`): two Boosts a player (the local player Fractal and Chorus; the
  teammates the shared layers: Colossus and Chain Storm, Carousel and Freeze Frame, Beam and Prism), the Mods at play test 3's
  caps, the local player's eight Fusions, and the fights' sounds on top in turn, ten seconds each: the arena's (a Rift's drone
  following the chord, its cutoff swept; the Storm Cell's crackle; a Lens's glass every 1/32 step, a Storm Cell's zaps a beat,
  a Rift's toms a bar) and a
  fight's (Feedback's Howl: sixteen delay taps, the pad's tone, its echoes a note every 1/16; Event Horizon's sub; Cadence's stab
  at 1.2 and a bell arpeggio a bar; a cut of the pads and bass every two seconds). `--band full` and `stress` are the M2-era
  loads and are unchanged; `max` is their superset.
- **Tools** (`raveform_headless`): `--render fights` (the sheet; `--stems dir` writes each bus), `kick-pulse`, `nova-check`,
  `max-upgrades` (the worst case without the fights); the profile has a `content` stage.

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
| Brood Mother | the threads: one pad voice on the chord's root while any broodling lives (the page's voice a broodling, cut to one by the lead's ruling), following the chord, silent when she is open; the bell at the break |
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
| A boss hit (play test 3) | a weak point: the `Tick` on the chord's root in octave 6, then the third, the fifth and the octave for a beat's further steps (the sim's gain says which step: 1, 0.7, 0.49, 0.343 and the floor); armour: the `Knock`; both on the band's next 1/32 step at the row's gain times the sim's |
| Arena | the Vault's bells up the chord with the glass; the Pylon's three bells; the nearest open Rift's drone on the chord's bass (one, by the lead's ruling; a Rift Surge's a fifth up for its second and third), its cutoff rising through the Build; a Rift's four bodies one aggregated tom; the Bumper's stab with its zap; the Lens's glass a fifth over its player's lead (aggregated); the Comet's announcing glide (the other events' pages name none); the Comet's Shards one aggregated bell a beat; the Eclipse's low-pass let go at its end; a Convoy tom a beat for each carrier, panned to it; the Storm Cell's zaps on the chord's three notes (aggregated) and its crackle, denser and louder as it nears |

### Hooks outside my files

- `src/game/play.cpp`: `updateFightVoices` is given the world, this device's seat and the band's offset (one call); an
  `EventType::ExtraLife` case hands the event to `UpgradeCues` (a spent one with the free Nova's blast tick).
- `src/game/upgrade_sounds.h/.cpp`: a Hit that broke the Orbitals' ring counts `OrbitalsBreak`; an Extra Life picked up or
  spent counts its cue; `flush` follows the ring's lit shards (`World::orbitalsLit`) and posts `OrbitalsRelight` for each one
  relit while the ring was broken.
- `CMakeLists.txt`: `src/game/pending_sounds.cpp` is built into `raveform_headless` (the sheet).
- `src/tools/audio_render.cpp`, `tests/audio_tests.cpp`: the sheet, the checks, the tests.

## Numbers

### The Moto (`bench_idle --band max --track 10`, 55 s after 5 s, our APK, the lock; shared phone)

| Run | Callbacks | Load P50 / P95 / P99 / max | Callback us P50 / P95 / max | Underruns | Callback allocations | Voices peak |
|---|---|---|---|---|---|---|
| Before (the last round's worst case: a Boost each, no Fusions, no fights), 120 s | 26,629 | 0.192 / 0.245 / 0.321 / 0.678 | 940 / 1170 / 1880 | 0 | 0 | 106 |
| This round's worst case, first (`5fc58a8`), 55 s | 12,207 | 0.246 / 0.315 / 0.393 / 0.780 | 1200 / 1500 / 1830 | 0 | 0 | 140 |
| With the lead's five cuts and play test 3's caps, 55 s | 12,208 | 0.230 / 0.290 / 0.376 / 0.959 | 1130 / 1360 / 1490 | 0 | 0 | 140 |
| **With held pads and drones at one voice**, 55 s | 12,206 | 0.219 / **0.280** / 0.367 / 0.721 | 1070 / 1310 / 1430 | **0** | **0** | 141 |

Each run was valid (the app kept focus); the panel at 60 Hz, 0 frames missed. The last is in the lead's accepted band (25% to
28% with 0 underruns), on its edge, and over the 25% goal.

Offline (`--profile`, Track 10, ms of engine time a second of audio, about +-5% between runs): the last round's worst case
28.6; this round's first 36-38 (the grown loadout alone 33.6-34.2: Standing Wave's eight held pads about 3, Colossus's
doubled bass and the Beam drones about 1.5; the fights' held voices about 2); after the cuts below **31.5**. The limiter and
the kill family cost nothing measurable.

The cuts (the lead's rulings of 2026-11-13):
1. **The five approved**: Standing Wave at six voices; a teammate's Beam drone only on its own device (this device holds its
   own player's); the local lead at 10 notes while its player owns Quartet; at most two Rift drones and two Feedback echoes
   (the fight's own lead, `kFightLeadVoices` 2); only the local player's Feedback Echoes on each device.
2. **The caps** (play test 3, GAME_DESIGN 6.2): the worst case's loadout has Split 2, Double-Time 1, Fork 1 (`maxLoadSetup`),
   set here ahead of the content engineer's change on main. Its Boosts: Fractal and Chorus for the local player; Colossus and
   Chain Storm, Carousel and Freeze Frame, Beam and Prism for the teammates (their Beam drones are theirs now).
3. **Over 28% (29.0%), so held pads and drones one voice each, longest-lived first**: Standing Wave's held pads to **one
   voice** (each new accent glides the held voice to its note over 20 ms and holds it a bar from there, its envelope going on:
   no click); the Rifts' drones to **the nearest one**; the Brood Mother's threads to **one pad voice** (the chord's root while
   any broodling lives, silent when she is open). Then measured once: 28.0%.

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

`raveform_tests`: 2716 checks, 0 failures (every recipe sounds, the two new ones included; new: the fights' sounds: the cut, the high-pass, the pad's tone, a fight's lead
(recorded an octave down, mirrored, bent, its cap), aggregation, the gate, the five-note stab; the kill family; the worst case
with random bursts and the fights' feed, no steal, no allocation). Calibration current (no loudness moved: the staged renders
are the same; `kDspVersion` unchanged).

## The Short form (play test 3, 5)

Every Track's `arrangement` in `design/music/track01-10.json` already has Groove 8, Build 4, Drop 8, Break 4 bars, every
progression is four chords, and what the conductor plays in a Groove or a Drop repeats within 8 bars (the bass's alternate
bar, the fill on the eighth, the Surge's two gears by halves of the section). No Track needed its phrases split between odd and
even cycles. The engine's two literal copies of the section lengths (`advanceCycle`, `planConductorBar`) came with the content
engineer's batch (main `61a7f09`, merged): every Track Short, Standard retired; nothing else in the band needed to move (the
conductor test walks every Track's 8 / 4 / 8 / 4).

## Not done or not verified

- **The hit cues' levels** (0.24 and 0.30) are measured, not heard: a beat's first hit about level with the music in its own
  band in a Drop, louder against a Groove; the user sets them by ear.
- **The budget**: 28.0% on the Moto after the rulings' cuts (above): in the accepted band, not under the 25% goal. The hit cues
  (one sample voice a 1/32 step at most) and the Extra Life / Orbitals cues came after that run and were not measured on it.
- **By ear**: nothing here has been heard (this PC is silent). The sheet is for the user.
- The tablet and the OnePlus: not attached.
