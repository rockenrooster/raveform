# RAVEFORM — Modes plan (M8)

Status: **a plan, written 2026-11-15 for the owner's review. Nothing is built and nothing is approved.**
Written by the design lead for the engineer who will build it (a different session, as every milestone).

| Doc | Purpose | When to read |
|---|---|---|
| `docs/MODES_PLAN.md` (this) | What changes, the frame every mode stands on, the code map, the wire, the order of work, the checks | First |
| `design/modes/README.md` | The line-up and the rules all modes share: the Stage, the kit, timed runs, the knock-out, versus, records, the bonus Tracks | Before part 0 |
| `design/modes/<mode>.md` | One page a mode | As its part comes up |
| `design/modes/screens.md`, `sound.md` | Every screen and transition; every sound in the engine's recipes; what the band lacks | Part 0 (screens), each part (its sounds) |
| `design/modes/modes.json` | The numbers. Embed it; never retype one | Part 0 |

The owner's request (2026-11-15), in full: new modes that are "visual and audio spectacles. Looking for
fun, creativity, and satisfaction (and fit the games theme). One mode can be playing a specific track
1-10. new music for new modes if needed"; "detailed plans and subplans for each mode"; "co-op/vs for
each mode"; "integration within existing menus. menus may need expansion. everything must feel
polished, transitions, vfx, sfx, and fit the games theme".

---

## 0. Briefing

1. **Everything in PLAN 0 and 8 and in CLAUDE.md stands**: you cannot hear or see motion, so verify
   with numbers, pictures and strips; the user's rules for this PC (four cores, silent, off screen);
   the shared phones' rules; commit at working checkpoints, push only when asked.
2. **Design is data and pages.** `design/modes/` is the lead's; code does not redesign it. A number no
   page gives goes in `src/sim/tuning.h` (or the view's constants block) and is listed in PROGRESS, as
   always. A page that turns out wrong in the hand is reported, with what was measured.
3. **What nobody has checked yet**: whether any of it is fun (the owner plays each part as it lands);
   the two bonus Tracks by ear; every number `modes.json` marks as an estimate.
4. **THE SET must not move.** The set's simulation is bit for bit what it is today for a given seed:
   its determinism hash is recorded before part 0 and checked at the end of every part. A mode is
   added beside the set's code path, never threaded through it.
5. **Each part ends playable** and is handed to the owner with a note saying what to try. The parts
   are in the order that proves the frame cheapest and puts something new in the owner's hands soonest.

---

## 1. What changes, and what does not

| PLAN 2's decision | Now |
|---|---|
| "One Endless mode" | Seven modes: THE SET (that mode, unchanged) and six more (`design/modes/README.md` 1) |
| "LAN co-op, 2-4 players, inside the single Endless mode" | LAN co-op **and versus**, 2-4 players, in every mode |
| Engine, targets, performance contract, generative music, run-only permanence, the name, the asset rules | Unchanged |

Permanence: nothing is unlocked and nothing carries between runs. The only new things kept on the
device are records and the mode last played (`modes.txt`).

The performance contract (PLAN 5) is unchanged and applies to each mode's densest moment (section 7).

---

## 2. The frame

Build this first (part 0). Every mode is a small amount of code on top of it.

### 2.1 The run's setup

One small value says what a run is. It goes wherever a seed goes today.

```cpp
namespace rf::sim {
enum class Mode : u8 { Set, OneTrack, Headliners, FullBlast, Tracers, Songline, Jam, Count };
struct RunSetup {
    Mode mode = Mode::Set;
    u8 track = 1;         // 1..12, for a mode that takes one (11 Mirrorball, 12 Toybox); else 1
    bool versus = false;  // each player for themselves (design/modes/README.md 8)
    bool help = true;     // versus: the underdog's help (V7)
};
}
```

- `World::resetRun(seed, startTick, players, presentMask, setup)`; the default setup is the set, so
  every existing call is the set. The setup is part of the world's state and of its hash.
- `Play` holds the setup the next run will use (the title's pill, MODES, the lobby); `PlayOptions`
  takes it from the command line (2.10); `net::Session` carries it in the lobby's state and in the
  run's start (section 4).
- A restart (RETRY, REMATCH) keeps the setup; NEXT TRACK changes its Track.

### 2.2 Forms: the director by mode

Today `phraseAt` computes where a bar sits from the Track's cycles. A mode gives its own **form**: its
sections in order (`modes.json` `forms`).

- `Mode::Set` takes the existing code path, untouched (briefing 4).
- **ONE TRACK** is the set's path with the Track fixed and one difference: the boss's death schedules
  a four-bar outro and the run's end instead of the Track change.
- **HEADLINERS** is acts: the first is Groove 4, Build 4, the boss's Drop (held); each later one is
  the set's own Track-change Break (the existing `changeBar` machinery, into the next Track), Build 4,
  the boss's Drop. No cycles, no mini-boss.
- **FULL BLAST, TRACERS, SONGLINE** are fixed lists of sections with a length (`timedBars`); the run
  ends on the last bar line (2.6).
- **JAM** is a Groove held for ever; DROP inserts `forms.jamDrop` at a bar line the host chooses (kept
  in the director's state as the Track change's bar is). POP-OFF is the fixed list `forms.popOff`.
- `Phrase` gains what the HUD, the band and the view need: the run's bars left (timed), the wave
  (FULL BLAST), the act (HEADLINERS), whether the run has ended. `DirectorState` gains the setup and
  what a mode's form must remember; all of it is in the hash and in `World::DirectorSync`.
- The band follows the director as it does now (`Play::conductBand`: `setPhrase`, `changeTrack`).
  A timed run's last bar and last downbeat are in `sound.md`.

### 2.3 A mode's state in the world

New header `src/sim/modes.h`: one struct a mode for what it adds (the tracers, rings and tiles; the
Spark and the gates; the room's mixer, pads and shelves; the kit; the versus counts), all fixed arrays
sized as each page's "Caps and the wire" says, reset by `resetRun`, digested by `World::hash()`.
Its code is one file a mode: `src/sim/world_mode_tracers.cpp`, `world_mode_songline.cpp`,
`world_mode_jam.cpp`, and `world_modes.cpp` for what they share (the kit, timed ends, the knock-out,
versus). `World::tick` calls a mode's update where the arena's is called today, and nothing when the
mode is the set.

### 2.4 The Stage

`design/modes/README.md` 4. In code it is the arena's walls moved in:

- `World` gains its walls as state (`wallMin`, `wallMax`): the arena's as today for four modes, the
  Stage's rectangle for TRACERS and JAM.
- `kArenaHalfW` / `kArenaHalfH` are read in 98 places in 29 files. Only what can happen on the Stage
  must read the walls instead: ships (`player_controller.cpp`), ordinary bodies and their bounces
  (`world.cpp`, `world_enemies.cpp`, `world_families*.cpp`), bouncing shots and Riffs
  (`world_upgrades*.cpp`), the spawner, the bot, and the view's walls and lattice bounds
  (`play_view*.cpp`). The bosses', mini-bosses' and arena's files do not change: none of them is ever
  on the Stage. The spatial grid stays the arena's.
- The camera: a fixed mode in the view (`PlayView`), its zoom from `stage.fitMarginWu` and
  `stage.minZoom`; no look-ahead, lean or breathing.
- The rig's eight fixtures and the Stage's beams are the view's (`play_view_modes.cpp`).

### 2.5 The kit and the load-in

`design/modes/README.md` 5. `World::dealKit(player, count)` runs the Trio's own dealing (`drawTrio`) and
the bot's pick score that many times from a stream seeded by the run's seed, the seat and (HEADLINERS)
the act, and applies each pick as a taken Trio is applied, without opening a Trio. The load-in is the
view's and the band's: the world logs one event a pick with its landing tick, a sixteenth apart.
`kit.atTrackStart` and `kit.atBoss` are measured in part 1 and written back into `modes.json`.

### 2.6 Timed runs and the knock-out

`design/modes/README.md` 6 and 7. One implementation for FULL BLAST, TRACERS, SONGLINE and POP-OFF:
`LAST CALL`, the last bar, the last downbeat (everything pops, half time for a beat), a `RunEnd` event,
the count's bar, then the screen. The knock-out replaces the down and the game over when the setup
says so (a timed mode, or versus): a ship state beside `down`, with its return tick on a bar line.

### 2.7 Versus

`design/modes/README.md` 8. In the world: each player's count for the mode; the shove (a third slot in
`PlayerState::shoves`, applied by the pushed ship's own device when it sees a rival's Dash end in
range); shared Boost orbs (one orb, an owner of "anyone"); the crown (the leader as of the last bar
line); the underdog (decided on bar lines by the host). In the game: the versus HUD, the podium, the
room's crowns. No friendly fire exists to switch off: a player's shots never test other ships.

### 2.8 Records and settings

`design/modes/README.md` 9: `modes.txt`, through `core/save_file` like the other two files, loaded
and saved by path so the tests link it (`game/save_data.cpp`). `Settings::hintsSeen` grows to twenty
bits (`screens.md`, "First-run hints"); the load's mask (`0x7FF`) goes with it.

### 2.9 Data

- `design/modes/modes.json` is embedded as the bosses' files are and read at start-up into
  `sim::ModesData` (a missing or malformed number fails the load loudly, as `conductor.json` does).
- The bonus Tracks: move `design/modes/music/track11.json` and `track12.json` to `design/music/` in the
  commit that embeds them (the build's list of Tracks, `audio::kMaxTracks` 10 to 12, the engine's
  per-Track tables, `tuning::kTrackBpm`, `songName`, the palette of a Track, the loudness
  calibration, the baked risers). `design/tools/setlist.py` globs `design/music/track??.json`: once
  they are there it renders twelve, and its sampler's table in `design/music/README.md` gains two rows.
- The set list stays ten: `World::setSetList` is already the smaller of the music's Tracks and
  `kBossesBuilt`. Songs 10 and 11 are reachable only through a setup's Track.
- The palettes: add the two recipes of `design/modes/README.md` 11 to `design/tools/palettes.py` and
  run it (`Palettes::kMax` is 12).
- All of this is in the co-op content hash, so a room and a joiner on different data are refused as
  they are today.

### 2.10 Arguments

| Argument | Meaning |
|---|---|
| `--mode <id>[:<track>[:<moment>[:hold]]]` | The run's mode and Track: `set`, `one`, `headliners`, `blast`, `tracers`, `songline`, `jam`. A moment jumps a picture or a benchmark there, as `--fight` does: `blast:10:wave4`, `tracers:11:drop2`, `songline:4:drop2`, `jam:12:drop`, `jam:12:all` (every layer on), `headliners:1:act7`; `:hold` keeps it from ending |
| `--versus`, `--no-help` | With `--host`, `--coop host` or the headless tools: a versus run; the underdog's help off |
| `--coop-mode <id>[:<track>][:vs]` | The co-op screens from the command line (`--coop host`): the room's mode, as the host's MODE pill would set it |
| `--picture m8_*` | The review pictures (section 7) |

`--track` stays the band's own argument (`src/audio/audio_args.h`).

---

## 3. Code map

Where each thing goes. New files are named; inside them the choices are the engineer's.

| Layer | New | Changed |
|---|---|---|
| `src/sim` | `modes.h` (the setup, `ModesData`, each mode's state), `world_modes.cpp` (the kit, timed ends, the knock-out, versus), `world_mode_tracers.cpp`, `world_mode_songline.cpp`, `world_mode_jam.cpp`, `modes_data.cpp` (reads `modes.json`) | `world.h` / `world.cpp` (the setup, the walls, the mode's update and hash, new events), `director.h` / `director.cpp` (forms), `world_spawner.cpp` (a mode's crowd rules: the Stage, contact-only, along the line, the dancers), `player_controller.cpp` (the walls; the shove), `world_upgrades.cpp` (shared Boost orbs; no shots in TRACERS; gels), `world_net.cpp` (section 4), `bot.cpp` / `bot.h` (three policies: the ring-flier, the rider, the wanderer), `tuning.h` (twelve tempi; what the pages do not give) |
| `src/game` | `screens_modes.cpp` (MODES, the mode pill, the results, the podium), `play_modes.cpp` (the flow: choosing, starting, ending, records; the load-in's band and HUD side), `play_view_modes.cpp` (the Stage, its rig and beams, tracers, rings, tiles, the Spark, gates, pads, shelves, the load-in, the count, the crown), `play_pictures_m8.cpp` | `play.h` / `play.cpp` (the setup; `conductBand` by form; `endRun` by mode), `play_hud.cpp` (the goal line, the run bar, the versus HUD), `play_transitions.cpp` (the rule's lift; the drop-in from MODES), `title_v2.cpp` (the pill), `screens_coop.cpp` (the lobby's mode, crowns, the join rows), `play_coop.cpp` / `play_net.cpp` (the setup in a room; versus), `lattice.h` / `lattice.cpp` (a tile's tint: an owner grid the lattice looks up at each node), `settings.h` / `save_data.cpp` (`modes.txt`, the hints' bits), `upgrade_sounds.cpp` / `pending_sounds.cpp` (the modes' cues, one row each, as the fights' are) |
| `src/audio` | — | `music_data.h` (`kMaxTracks` 12), `band.h` / `band.cpp` and `engine.*` (the five needs of `sound.md`: `leadNoteAt`, the lead's length, `holdChord`, a fire mode of every fourth step and a fifth lead slot, `setBedMask`), `loudness_calibration.json` (twelve Tracks) |
| `src/net` | — | `protocol.h` / `protocol.cpp` (section 4), `session.*` (the setup; versus; the mode's messages), `lobby.*` (the beacon's mode) |
| `src/core` | — | `args.h` / `args.cpp` (`--mode`, `--versus`, `--no-help`) |
| `src/tools` | — | `sim_headless.cpp` (`--sim mode`), `net_headless.cpp` (`--net soak --mode`), `audio_render.cpp` (the bonus Tracks; the mixer's 256 settings) |
| `tests` | Suites: forms, the kit, tracer geometry, the floor, the line, the modes' messages, `modes.txt` | The determinism test runs every mode |
| `scripts` | `review-m8.ps1` (the pictures), `bench-modes.ps1` or new rows for `bench-android.ps1` | `test.ps1` (the new suites), `render-audio.ps1` (twelve Tracks) |
| `design` | — | `tools/palettes.py` (two recipes), `palettes.json` (exported), `music/` (the two Tracks moved in) |
| `CMakeLists.txt`, `android/` | — | The new files; `modes.json` and the two Tracks embedded. Nothing in the manifest or the activity |

---

## 4. The wire

A new protocol version. Everything below is validated as every message is (lengths, counts, ids and
enums checked; nothing allocated from a size field; a malformed packet dropped whole), and the packet
fuzz test covers it.

| Message | Change |
|---|---|
| `MsgLobby` | + the room's setup (mode, track, versus, help), + each seat's crowns (a byte) |
| `MsgRunStart` | + the setup |
| The beacon | + the mode and a versus bit (two bytes: `kBeaconBytes` 34); the join list shows them |
| `MsgStats` | + the player's count for the mode (four bytes), so the ranking moves at 4 Hz |
| `World::DirectorSync` | + the form's place (the wave, the act, a JAM Drop's bar), the Spark's checkpoint (its step, place and heading), the floor's census (lit tiles a player), the mixer's mask, the leader and the underdog |
| `MsgHeartbeat` | + each player's share of the damage on the boss (a byte each: HEADLINERS in versus) |
| **`Msg::Mode`** (new, reliable, channel 0) | One message with a kind and that kind's fixed payload: |

| Kind | From | Payload | Page |
|---|---|---|---|
| `RingAsk` | A client | Owner(s), the pop's tick, up to 48 corners (16 bits each way) | `tracers.md` |
| `Ring` | The host | The same with its number in the run: every device lights and pops from it in that order | |
| `Floor` | The host | All 903 tiles at 3 bits (339 bytes), on a census mismatch | |
| `House` | The host | The house's number, its bar line | |
| `Cut`, `SpinOut` | The device of the ship it happens to; the host tells all | The player, the place | |
| `GateTake` | A client | The gate's step | `songline.md` |
| `GateOwner` | The host (versus) | The step, the player | |
| `SparkBend` | The host | From which step, toward where | |
| `PadAsk` | A client | The pad | `jam.md` |
| `Pad` | The host | The mixer's mask, a Drop's bar, a Track, the sound, each with its bar line | |
| `Knock` | The player's own device | Out, and the bar line it is back on | `README.md` 7 |
| `Crown`, `Underdog`, `ActWon` | The host | The player | `README.md` 8 |

Who decides what is on each page under "Caps and the wire". The principles are the set's (TECH_SPEC
8.1): a player's own device is the authority for that player (its tracer, its rings, the gates it
threads, the toys it takes, its knock-out); the host orders what must be the same everywhere (the
floor, a gate's owner in versus, the room's mixer); what is computed from the seed and the song is
not sent. **What a ring catches is its owner's to say**, as a kill is the claimant's: other devices
draw as caught what is inside the ring in their own view, and take the deaths from the claims.

Budgets (TECH_SPEC 8.4) do not move: the modes add a few hundred bytes a bar at most.

---

## 5. The band

`design/modes/sound.md`, "What the band needs": five additions (a lead note at any pitch; the lead's
length for a player; holding the progression on a chord; a fire mode of every fourth step and a fifth
lead slot; the bed by a mask) and two more Tracks. Rules for the work:

- The callback contract holds (no allocation, lock, log or JNI); `scripts\test.ps1`'s audio suite
  gains a case for each addition (timing to the sample, no allocation).
- `kDspVersion` does not change: nothing here changes a render's loudness. The calibration is run for
  the two new Tracks (`scripts\calibrate-audio.ps1`).
- The Moto's audio budget was measured with four lead slots. Measure a fifth
  (`scripts\audio-device.ps1 -Run bench -AppArgs '--band full'` with the Spark's slot on); if the
  callback's P99 leaves its quarter of the burst, the Spark in a four-player run plays through the
  band's stab voice instead and the fifth slot is not built.
- Listening files for the owner: the two Tracks from the engine (as `render-audio.ps1` makes the
  ten's), and one sheet a mode of its new sounds, as the upgrades' sheets were made.

---

## 6. Bots, headless tools, tests

The executor cannot play. Each new way to play gets a bot before it gets a picture.

| Bot | Policy | Used for |
|---|---|---|
| The ring-flier (TRACERS) | Flies a circle round the centre of the nearest twelve bodies, as wide as its tracer allows; dashes through a body in its way; Novas when ringed in itself | Runs, benchmarks, the soak |
| The rider (SONGLINE) | Flies at the Spark's side toward the next gate of its string; leaves the line for a body within 200 wu and comes back | The same |
| The wanderer (JAM) | Crosses the room pad to pad at random, holds what it lands on, takes a toy from each shelf | The room's soak; POP-OFF |
| The set's five policies | As built | ONE TRACK, HEADLINERS, FULL BLAST |
| The child (`--policy child`) | The novice with 0.5 s reactions and no Dash: the temporary build of 2026-11-15 (PROGRESS, "balance for young players"), made a standing policy | Every page's line for the youngest player |

| Tool | Command | Prints |
|---|---|---|
| A mode over many seeds | `raveform_headless.exe --sim mode --mode <id>[:<track>] [--seeds 20] [--policy assist\|novice\|play\|child] [--players N] [--versus]` | One `RFSIM` line and a table: for ONE TRACK and HEADLINERS the fights' table of `--sim crowd`; FULL BLAST score, Chain, knock-outs, the shot pool's peak; TRACERS houses, rings by size, floor share, knock-outs; SONGLINE gates in a hundred, in time, best streak, how often the line came back; JAM pops a minute, pool peaks |
| The kit | `--sim crowd --policy assist --seeds 20 --minutes 40` (as built), its table of upgrades held by Track | The two rows of `kit` |
| Determinism | `--sim determinism --mode <id>` | The same hash twice, every mode |
| The net soak | `--net soak --mode <id> [--versus] [--players 4] [--minutes 5]` | The set's `RFNET` figures and the mode's: floor census mismatches, gates whose owner was corrected, the Spark's largest error |
| The mixer | `--render mixer --track 12 --out <dir>` | A bar of each of the 256 settings, with the peak and loudness of each |

Unit tests (new suites in `scripts\test.ps1`): the forms (every mode's bars add up; a timed run ends
on its bar; the set's phrase is unchanged for 2,000 bars); the kit (the same seed and seat give the
same kit; its size); tracer geometry and the floor (`tracers.md`, "What to check"); the line
(`songline.md`); the new messages' round trips and the fuzz; `modes.txt` (a damaged file loads as none).

---

## 7. Pictures and benchmarks

**Pictures** (`scripts\review-m8.ps1 -Part p0..p7 [-Only name,...]`, in `captures\m8\<part>\`, Debug,
exit 1 on a validation message or a layout warning; scripted in `play_pictures_m8.cpp`):

| Part | Pictures |
|---|---|
| p0 | `m8_title_pill_<mode>` (seven), `m8_modes_<mode>` (seven), `m8_modes_bonus`, `m8_modes_room`, `m8_lobby_host`, `m8_lobby_join`, `m8_lobby_versus`, `m8_join_modes`; strips a quarter beat apart: `m8_t_lift`, `m8_t_back`, `m8_t_mode`, `m8_t_track`, `m8_t_drop` |
| p1 | `m8_loadin` (a strip a sixteenth apart), `m8_card_one`, `m8_one_outro`, `m8_result_one`, `m8_vs_hud`, `m8_vs_crown`, `m8_vs_shove` (strip), `m8_knock` (strip), `m8_podium_2`, `m8_podium_4`, `m8_podium_draw`, `m8_podium_arrive` (strip) |
| p2 | `m8_card_act`, `m8_topup` (strip), `m8_vs_damage`, `m8_result_headliners` |
| p3 | `m8_torrent` (strip), `m8_wave_<1..4>`, `m8_powercut` (strip), `m8_loadout` (strip), `m8_result_blast` |
| p4 | `m8_stage_phone`, `m8_stage_tablet` (the Stage at 2400x1080 and at 1920x1200), `m8_tracer`, `m8_tracer_hint`, `m8_lock`, `m8_pop` (strip a thirty-second apart), `m8_ring_x8`, `m8_floor_60`, `m8_house` (strip), `m8_lights_out`, `m8_duet`, `m8_turf_4`, `m8_cut`, `m8_spin`, `m8_count_tracers` (strip), `m8_result_tracers`, and each of the nine Boosts' moments here |
| p5 | `m8_spark`, `m8_string_groove`, `m8_runway`, `m8_tube` (strip), `m8_gate_in_time`, `m8_streak_<1..4>`, `m8_gold_gate`, `m8_braid_4`, `m8_race_4`, `m8_bend_back`, `m8_finale` (strip), `m8_count_songline`, `m8_result_songline` |
| p6 | `m8_room_1`, `m8_room_4`, `m8_room_8`, `m8_pad_states`, `m8_shelf`, `m8_drop_ready`, `m8_drop_resting`, `m8_room_drop`, `m8_empty` (strip), `m8_popoff_draft`, `m8_popoff_drop` |

Each new look is checked against look.md the way the upgrades' were: `scenes.py light` on the fullest
picture of each mode (dark share and the share above 200), and no line of the danger colour anywhere
it does not hurt.

**Benchmarks** (the Moto, `--tier low`, our APK deployed first, the lock; shared phones: say so):

| Moment | Command (`scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Seconds 60 -Warmup 3 -Deploy -AppArgs ...`) | Must hold |
|---|---|---|
| FULL BLAST, wave 4 | `'--mode blast:10:wave4:hold --nohits --tier low'` | The contract; the shot pool's peak reported |
| The same with four players | `'--mode blast:10:wave4:hold --nohits --tier low --host --loopback-bots 3'` | The contract; else the fallback of `full_blast.md` |
| TRACERS, the last Drop, four players | `'--mode tracers:10:drop2:hold --tier low --host --loopback-bots 3'` | The contract |
| SONGLINE, the last Drop, four players | `'--mode songline:10:drop2:hold --nohits --tier low --host --loopback-bots 3'` | The contract; 256 gates in one draw |
| JAM, every layer, a Drop | `'--mode jam:12:drop:hold --loadout max --tier low'` | The contract |
| The Stage at its zoom | Any of the Stage's above | GPU P95 against the set's at 0.75 (the lattice covers more of the screen at a low zoom) |
| The band with the Spark's slot | Section 5 | The audio budget |

ONE TRACK and HEADLINERS add no moment the set has not: their benchmarks are the set's (`--drop`,
`--fight`). A P95 regression above 10% in the set's own benchmarks blocks a part (PLAN 5).

---

## 8. The order of work

Eight parts. Sizes are relative (S, M, L). Each ends with its tests, pictures, benchmarks where it has
any, an entry in PROGRESS and a build for the owner.

### Part 0 — The frame and the menus (L)

Read: this page; `design/modes/README.md`, `screens.md`; `design/screens.md` "Title", "Transitions", "Co-op".

Build: the setup through the world, the game, the lobby and the run's start; forms (with only the
set's and a test form); `modes.json` embedded; `modes.txt`; the title's mode pill; MODES with all seven
cards, emblems, the Track strip (ten chips until the bonus Tracks are in) and the Track heard as it is
chosen; the room's mode in the lobby, the join rows and the beacon; the transitions; `--mode`. Until a
mode is built its card is drawn, its PLAY is the disabled control, and its caption line reads
`COMING IN THIS BUILD'S NEXT PART` (dev builds only: no release build ships a card that does nothing).

Exit:
- [ ] The set's determinism hash is unchanged; `scripts\test.ps1` passes.
- [ ] p0's pictures: no validation message, no layout warning; the title with the pill keeps its 24 px.
- [ ] PLAY on the title still starts a run within the same number of beats as before.
- [ ] A room's mode reaches every seat and the join list on two devices (the PC and a phone).

Owner: do the menus feel like the game? The rule's lift, the cards, a Track heard as it is tapped.

### Part 1 — ONE TRACK, the kit, and versus (L)

Read: `one_track.md`; `README.md` 5, 7, 8, 9; `screens.md` "Results", "The versus HUD", "The podium".

Build: the kit and the load-in; ONE TRACK with its outro and result; NEXT TRACK; records; the kit's
measurement written into `modes.json`; the knock-out; versus for THE SET and ONE TRACK: counts, the
crown, the shove, shared Boosts, the underdog, the podium, REMATCH, the room's crowns.

Exit:
- [ ] The assist bot clears each of the ten Tracks in at least 19 of 20 runs; the novice's fights are
      within 30 to 60 seconds (the set's lines), reported by Track.
- [ ] Two phones, a versus ONE TRACK on Reef and a rematch: the same ranking on both at every bar
      line in the logs; the crowns stand in the lobby.
- [ ] The net soak with `--versus`, four bots, five minutes: the set's figures hold.

Owner: one Track as a game of its own; the first versus match in the room.

### Part 2 — HEADLINERS (M)

Read: `headliners.md`. Build: the acts' form, the top-up, the act's card and bill, the clock, the
result; versus's damage race.

Exit:
- [ ] The assist bot plays all ten acts in at least 19 of 20 runs; each fight's length for the novice
      reported against 30 to 60 seconds; the kit at each boss as measured.
- [ ] Versus, an aiming bot against an assist bot: the acts won with the help on and off.

### Part 3 — FULL BLAST (S)

Read: `full_blast.md`. Build: the loadout's deal, the waves, the Boost rain, the torrent, the power
cut, the load-out, the result.

Exit:
- [ ] Both benchmarks of section 7, or the fallback taken and said.
- [ ] A bot's run on each of the twelve Tracks ends on its bar with no pool refusing anything.

Owner: is three minutes of everything fun, or tiring? The waves' pacing is the lever.

### Part 4 — The Stage, Mirrorball and TRACERS (L)

Read: `tracers.md`; `README.md` 4, 11; `sound.md` "TRACERS"; look.md 5.

Build: the walls as state and the fixed camera; the bonus Tracks' plumbing (twelve Tracks, the
palettes) with Mirrorball; the tracer on the lead rule; rings, the catch, the pop; the floor, its
tint in the lattice and its edges; houses; the gels; the count; the result; co-op's welds; versus's
turf, the cut and the spin-out; the mode's messages; the ring-flier.

Exit:
- [ ] The geometry and floor tests; the determinism test; the loopback soak with four ring-fliers:
      every device's floor is the host's at every heartbeat.
- [ ] The Stage's pictures on the phone's and the tablet's shapes: everything on it readable at its
      zoom (the lead looks at these before anything else is built on the Stage).
- [ ] The ring-flier fills a house on every Track; the child-like bot is knocked out at most twice
      a run on Mirrorball.
- [ ] The Moto's benchmark.
- [ ] Mirrorball rendered by the engine, matched to the prototype as the ten were, calibrated.

Owner: the first play of a new way to play. Is drawing a ring satisfying in the first ten seconds?
Mirrorball by ear.

### Part 5 — SONGLINE (L)

Read: `songline.md`; `sound.md` "SONGLINE" and "What the band needs" 1 and 4.

Build: the line, the Spark and its slot in the band, gates as one instanced glyph, threading, the
streak and its tiers, gold gates, the crowd along the line, the camera's lean, the line bending back,
the finale and the count, the result; co-op's strings, braid and chords; versus's race; the rider.

Exit:
- [ ] The line's tests (in the walls, never too tight, two worlds agree).
- [ ] The rider threads at least 90% on every Track; a slow bot is found again within four bars.
- [ ] The Moto's benchmark; the band's with the fifth slot.

Owner: does it feel like flying with the tune? The Spark's speed and the gates' size are the levers.

### Part 6 — Toybox, JAM and POP-OFF (L)

Read: `jam.md`; `sound.md` "JAM" and "What the band needs" 5.

Build: Toybox; the bed by a mask; the room's pads; the eight lights; the shelves; DROP; the Track pads
and SOUND; EMPTY; presents; POP-OFF; the wanderer.

Exit:
- [ ] The mixer's 256 settings rendered: none clips, none louder than the Track's Drop.
- [ ] The wanderer's ten minutes: nothing refused, nothing left sounding.
- [ ] The room's pictures on the phone: every pad's word readable.

Owner: hand it to the youngest player in the house and watch. Toybox by ear.

### Part 7 — The whole (M)

The polish pass and the closing checks: every transition as a strip; reduce flashing through every
new flash; haptics; the UI scale's four sizes on every new HUD line; every mode from every way in
(the title, MODES, RETRY, REMATCH, NEXT TRACK, a room, the pause's RESTART) and out; the sanitizer
runs with a bot's run of each mode (`scripts\sanitize.ps1`); the release build's size; the documents
of section 9; the cards' dev-only line gone.

Exit:
- [ ] The benchmark matrix of section 7 on the Moto, recorded in PROGRESS; the set's own benchmarks
      within 10% of where they were.
- [ ] Four phones, one room, each mode once in TEAM and once in VERSUS (`scripts\coop-phones.ps1`
      with `--coop-mode`), the logs scanned as that script does.

---

## 9. Documents to bring up to date as parts land

| Document | What |
|---|---|
| `docs/PLAN.md` 1, 2, 7 | The two decisions of section 1; M8 as a milestone |
| `docs/GAME_DESIGN.md` 4, 10, 12 | Modes (a pointer to `design/modes/`), versus beside co-op, the flow with MODES |
| `docs/TECH_SPEC.md` 7.1, 8.1, 8.3 | The modes' state and caps; the table of who is the authority for what; the beacon |
| `design/screens.md` | The parts of `design/modes/screens.md` that are built move in, with the date |
| `design/look.md` 5, 6 | The tracer, a ring's pop, lit tiles, the Spark, gates, the rig's beams, pads, the crown |
| `design/music/README.md` | The two bonus Tracks, the listening table, the tool's twelve rows |
| `CLAUDE.md` | The new commands (`--mode`, `--sim mode`, `review-m8.ps1`, the benchmarks), the new files' map |
| `docs/PROGRESS.md` | Each part: what was built, measured, decided alone, not done |

---

## 10. Risks

| # | Risk | Level | Mitigation, and the fallback |
|---|---|---|---|
| 1 | Scope: six modes on a finished game | High | The parts' order: three cheap ones first on a shared frame; each part ships alone. Any of parts 2, 3, 5 and 6 can be dropped without touching the others; part 4 needs part 0 only |
| 2 | The Stage is too small to read at 0.75, most of all on the tablet | Medium | Part 4 opens with its pictures. Fallback: the Stage at 1,520 x 760 and a zoom of 0.85, with 760 tiles |
| 3 | TRACERS does not feel good in the hand: the ring does not close when the player thinks it has | Medium | The snap, the hint line, the lock on the frame; the snap's 36 wu is the first lever, then the tracer's life |
| 4 | Floors or gates differ between devices | Medium | The host orders rings and gates; the census and the full floor; the soak asserts them |
| 5 | A fifth lead slot costs the audio thread its budget | Low-Med | Measured in part 5; the fallback is in section 5 |
| 6 | The owner does not like a bonus Track | Medium | Each mode's home falls back to a set Track (`design/modes/README.md` 11); nothing else depends on them |
| 7 | FULL BLAST with four players misses the contract on the Moto as host | Medium | The fallback in `full_blast.md`: the co-op crowd's scale, never the resolution |
| 8 | Versus brings out what co-op hid: a client that claims what it should not | Low | The trust model is the set's (friends on one network, TECH_SPEC 8.1); the host still orders everything shared |
| 9 | JAM's pads are words, and its player may not read | Medium | Each pad also carries its layer's moving line and lights its own lamp; flying over anything does something. If the owner's own test says words are not enough, the pads get pictures: the lead draws them |
| 10 | The kit's estimates are far off, and ONE TRACK's later Tracks are too hard or too easy | Medium | Measured in part 1 before anyone plays it; the numbers, not the fights, move |

---

## 11. For the owner to decide

1. **The line-up.** All six, or fewer? The names (HEADLINERS, FULL BLAST, TRACERS, SONGLINE, JAM)?
2. **The order.** As above, the cheap three come first. If the owner would rather play TRACERS first,
   part 4 can follow part 0 directly.
3. **TRACERS has no gun**, no Trios and no Riffs: the line is everything. The alternative is the set's
   upgrades on top; the page argues against it.
4. **Help for the one behind** in versus: on unless the host switches it off. Or off unless switched on?
5. **HEADLINERS is scored by the clock**, not by points.
6. **The two bonus Tracks**, by ear: `design/modes/music/previews/`.
7. **THE SET in versus ends at the tenth boss**, with no encore.
