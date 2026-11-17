# RAVEFORM — Progress

The network engineer's follow-up to M4 (the Moto-hosted error's cause and fix, the resend back-off, motion corrections for
moving Shoals, remote players predicted on direct links, the tools) is in `docs/PROGRESS_m4_net.md` (merged into main with
M5 part 4; protocol `0x52460505` holds both). The sound engineer's M5 work is in `docs/PROGRESS_m5_audio.md` (merged 6121b37).

## 2026-11-16, Wave's crest; Extra Cell, Magnet and Overcharge re-ranked; Prism's glow

The owner, on Wave: "rank 1 arc width needs to be 70", and from this session's list: "a real crest: a white-hot leading edge
fading into your colour, a soft wider band behind it, and a slight shimmer along it; ripples the background grid as it passes; a
spark and flash on each enemy it hits; stack 2 gets a second crest just behind the first; a soft swell on the beat each time a
wave goes out". Then, of the second crest: "go to one ring for the wave instead of 2. too much visual".
- **The arc.** Rank 1 is 70° wide (`tuning::kWaveHalfArcDeg` 35°; it was 90°); rank 2 stays 140°, 550 wu, 3 damage.
- **The crest** (`PlayView::drawWaveFront`, play_view_upgrades.cpp; the front drawn at the frame's radius, between ticks): three
  overlapping arcs graded from a white-hot edge (cooling as it travels) through the light colour to the colour, a soft band of
  pooled glow 16 wu behind it, a +-25% shimmer running along it and three glints riding it (no shimmer or glints with reduce
  flashing). One crest at either rank (the second, tried first, was dropped).
- **The lattice** takes impulses along the crest as it passes (a 34 wu Gaussian every 45 wu of arc, 55 a 60 Hz frame, scaled by the
  frame's time), out then back: about a medium kill's ripple; 26 hardly showed.
- **Hits.** The view finds the bodies the front reaches each frame (as `World` does on the owner's device, so every player's front
  shows them on every device) and throws six sparks on along the front's way, a soft flash, and a little star for the first ten a
  front. The world's own `Impact` sparks still come on the owner's device.
- **The swell** (`Engine::upgradeStep`, content_sound.cpp `kWaveSwell` 0.2): the last half beat of the Track's half-bar reverse swell
  runs into every beat while anyone holds Wave, the beat a front goes out on (`UpgradeSound::WaveSwell`); no level given, so two
  thirds of the Lancer's charge.
The owner, on three upgrades: "these should be 3 with the same max stats: EXTRA CELL, MAGNET; make this one 4 with the same max
stats: OVERCHARGE".
- **Extra Cell** three ranks adding 1, 1 and 2 cells (8 at the cap, as four ranks of one gave; `tuning::kExtraCellAdds`; the third's
  words `+2 CELLS, ALL REFILLED`). **Magnet** three stacks of +133% (x2.3, x3.7, x5; `SHARD PULL X2.3` ...). **Overcharge** four
  stacks of +62.5% damage and +18.75% radius (x3.5 and x1.75 at the cap, as five of +50% and +15%; `+62.5% SHOT DAMAGE`); its head's
  colour, sparks and flash and the lead's drive (`overchargeLook`, sim/upgrades.h; `bandLoadout`) take the fourth stack as the old
  fifth, 1-3 as before. GAME_DESIGN 6.2's rows say so; the determinism hashes did not move (the bot's runs never take them in time).
The owner: "Prism needs more glow and color pop", then "prism doesn't need the line trails", "still a very faint trail on prism"
and "put the glow back on the head, and tune the glow to be half as much".
- Each lane's head has a soft halo (22 ref px, 0.06) and its streak a pooled glow of its colour hugging it: two Gaussians of sigma
  0.25 of its length, a third and two thirds of the way back from the head, at 0.125 (play_view_shots.cpp): absolute levels, not
  the shot's 2.2. The afterglow (look.json, its knee 0.9 on the scene and the pools blurred to sigma 6 ref px) prints whatever
  clears the knee as a beaded line behind a moving shot; the streak and the head's sparkle already come to about 0.65 at the head.
  A brighter halo at the head, a wider under-line in the scene, then the halo at 0.12 with the glow at 0.25 each printed the line
  (`--mute afterglow` and `--mute pools` each took it away: the pools reach the afterglow's source too); half of the last is the
  glow now. Measured against the same frame with `--mute afterglow`: at most 4 of 255 along the lanes past the muzzle (3 pixels
  over 3), the muzzle's flash as any shot's.
  look.md 5's Prism paragraph (0.6 of the shot level, 1.3 of its width, white only in its front 15%) is unchanged; the glow is the
  owner's addition.

Pictures: `captures\wave2\wave_sheet.png` (every third frame of the rank 2 strip `--picture m5_up_wave --strip 27`),
`wave_zoom.png`; `captures\prism2\boost_prism.png`, `prism_zoom.png`. Not seen on a phone.

## 2026-11-16, a first launch: the tier probe behind the loading screen; the title's demo ship fades with its run

The owner, on a very first launch (build 341): "tons of garbage on screen like its in the middle of a mid game fight before the game
loads", and "once the game loads, the demo ship is invisible for a bit but still shooting bullets".
- **The fight was the tier probe.** On a device with GPU timers the first launch measures its tiers on the M1 endgame stage (about
  700 enemies and 1,500 bullets under a saturating GPU load), and it ran behind the notice and the title at a third of its light
  (M3 stage B). In the game with its screens (`content_ == Play`, `flows()`, not a benchmark) the probe now runs in `App::init`,
  right after the bake, frame by frame under the loading screen (`App::probeLoadingFrame`: the stage advanced, drawn and measured
  as before, an opaque veil over it on `FrameDraw::under`, the loading screen's word and full line on the HUD), and the notice or
  the title comes after it. The loading screen is drawn on the HUD layer in both phases now (a soft halo under the word in place of
  the bloom), so the two look alike. Desktop (`--tier auto`): 0.7 s behind the loading screen, the same tier as before (ultra).
  Benchmarks and stages keep the old probe (its stage on screen, `SETTING UP FOR THIS DEVICE`). Devices without GPU timers (the
  Fire 7) never ran it.
- **The ship.** In a transition (the title's run coming up over the bar after its build, a run dissolving under a wait) the run's
  ship was hidden while its light stayed under 0.7 (`vi.noShips`, `Play::render`) and its shots went on at that light. The ship now
  fades with the run like its shots (its hull, halo, trail, pool and Dash ghosts by `ViewInput::runLight`), its priority layer with
  it (`params.threatLevel` takes the lower of the screen's level and the run's light); the drop-in's first beat still draws the
  screen's own ship alone. The first-run hints stay hidden under a run below 0.7 as before.

`scripts\test.ps1` and the solo flow test (52 checks) pass. Not seen on a phone with GPU timers: the Galaxy tablets were in other
sessions' use and the Motos were not attached; on the way the tablet `R5GL362BEHX`'s RAVEFORM data was cleared (`pm clear`, its
dev build, while RAVEFORM was in front: a mistake of this session's; whatever settings and scores were on it are gone).

## 2026-11-16, the first launch shows a loading screen, not a black one

The owner, after build 340's first launch on the Fire 7: the screen stayed black for about 13 s. With no cache the band bakes
its one-shots before the first frame (`Band::init`: 460 sounds, 1.7 s on the Moto, 12.8 s on the Fire's Cortex-A53s; the later
Tracks' kits and risers already came behind the game). Now, when the app gives `BandDefaults::waiting`, that bake runs on a
worker and the game thread draws `App::loadingFrame` about once a refresh: the RAVEFORM word in the title's colours (white
through the palette accent, in the scene, so it blooms), a 600 px line under it filling with the bake (`SampleBank::bakeShare`:
its six sections weighted by their time on the Fire, the fill eased, never backwards) and a playhead pulsing at 120 BPM, and
`TUNING UP`; Track 1's palette and background, fading in over half a second. Events are pumped (the lifecycle watch runs); with
no surface (in the background) it waits a frame.

Measured: on the Fire (dev build, first launch) 851 frames drawn over the 14.2 s bake (it took 12.8 s with the game thread idle),
then the notice; on this PC 57 frames over 0.9 s. The baked cache is byte-identical to the one baked before the change
(SHA-256 compared), and the cache's later starts are unchanged (0.1 s on the Fire). `scripts\test.ps1` passes. Picture:
`captures\fire\loading_screen_fire.png`. The loading screen is not in `design/screens.md` (the lead's file).

## 2026-11-16, the low end: an Amazon Fire 7 (2022)

The owner added a low-end tablet over wireless debugging: "see if anything can be done for optimization". It is an Amazon Fire 7
(2022, `KFQUWI`, `fire` in `RF_DEVICES`): MediaTek MT8168, four Cortex-A53 at 2.0 GHz, Mali-G52 (Bifrost r26) with Vulkan 1.1.131
and **no GPU timestamps**, 2 GB, 1024 x 600 at 59.82 Hz, Fire OS 8.3 (Android 11), well below the Moto. The owner's own settings on
it were EFFECTS Low; the first-launch probe could not measure without GPU timers and had chosen Medium.

**Measured before** (build c197549; tier low, governor off as in every benchmark, shared device over Wi-Fi):

| Moment | Panel missed | Main thread p50 / p95 (ms) |
|---|---|---|
| THE SET at minute 2 | 0.28%: PASS | 7.3 / 9.0 |
| THE SET's densest (`--drop 10.1`) | 0.20%: PASS (Medium: 0.17%) | 8.1 / 9.5 |
| FULL BLAST, wave 4 held | **every frame two periods: 30 fps, FAIL** | 16.2 / 21.3 |

A CPU profile of FULL BLAST (simpleperf, `cpu-clock`, call graphs): the game thread was 54% of the process's time, the audio callback
33%, the Mali driver's thread 11%. On the game thread: the lattice 25% (drawing 11, writing its capsules a good part of 9.5, the
springs 6), the HUD 9% (the band row's icons: gradient colours by `atan2` per point), `powf` 5% (`srgbToLinear` on every Stroke
line), the simulation 14% per two ticks.

**What changed** (view-side only: THE SET's determinism hashes are unchanged, `f8d9e8972c671db2` / `6d9e281fa6d95425`):
- **The lattice** (`game/lattice.cpp`): `slot()` took four integer divisions and the springs and the drawing called it a dozen times
  a node; the window's slots are now two small tables (`colSlot_`, `rowBase_`, rebuilt when the window moves), the arithmetic on the
  nodes unchanged and in the same order. The joints' straightness test compares squares (no square roots).
- **Instance writers**: `floatToHalf4` (`core/half.h`) converts four floats in one NEON `FCVTN` on arm64 (the scalar path elsewhere,
  so the desktop's pictures are unchanged); `writeCapsule` is inline and uses it, as do glows, glass, glyph instances and sparks.
- **`srgbToLinear`** reads a 256-entry table (the same formula, the same floats). **Icons**: every point's gradient colour once at
  load (`IconSet::ptColour_`).
- **The governor below Low** (`Governor::cuts`, `App::applyTier`): at Low with the spark density at its floor and still over
  budget, it takes look.md §10's cuts one at a time (glass, sparkles, nebula, ... lattice, ornaments); with room to spare (GPU
  timers only) the last one comes back first. The settings' tier change and the probe's end reset the governor before applying.
- `Start-RfApp` dismisses a lock screen (the Fire sleeps behind one between runs; no PIN on the test devices).

**Measured after** (the same moments; the Fire):
- FULL BLAST, governor off: the main thread 12.2 / 18.4 ms at 30 fps (two ticks a frame): **-25%**, still locked at 30. A 20 s run
  holds 60 (3.3% missed, main 7.7 ms p50); a 60 s run falls to 30 at its first spike and stays: a missed frame doubles the next
  one's simulation and events, so near its limit a device locks at half rate. With **the governor on, as in play**: 60 s at 60 fps,
  **2.6% missed: PASS**, main 7.6 / 9.9 ms, two cuts taken in the warm-up (glass, sparkles).
- Every mode's heaviest moment, 30 s, governor on, tier low: TRACERS (four players' floor) 0.5% missed, SONGLINE 0.11%, JAM with
  every toy 0.39% (one cut), HEADLINERS act 10 0.22%, THE SET's Track 10 Drop held 0.11% (main 6.2 ms p50, was 8.1): all PASS.
- A Galaxy Tab A11+ (regression, Wi-Fi): THE SET's densest 0 missed, main 2.4 ms p50, GPU 14.3 ms p95. `scripts\test.ps1` passes.
- Pictures: `captures\GN41WE04450600BJ-20261116-133246.png` (FULL BLAST on the Fire, overlay on), `captures\fire\` (desktop).

Decided alone: hardware half conversion rounds exact ties to even where the scalar path rounds them up (one half-ulp on exact ties,
arm64 only); the cuts stay for the session on a device without GPU timers (nothing tells the governor it has room again).

Not done, not verified: **the Moto was not attached** (not re-measured; nothing it ran changed but the view's arithmetic). The probe
still falls back to Medium without GPU timers (the governor now takes it down as needed). The Fire's audio callback is about half a
core in FULL BLAST (one underrun a minute, the buffer growing 1920 to 2880 frames); the DSP was not touched (a change there needs the
loudness calibration). **The Fire's storage is 95% full** (547 MB free of 10 GB): an install was refused until this session's 51 MB of
profiling files were deleted; the owner's to clear. The Fire now runs this PC's dev build.

## 2026-11-16, the owner's first play of the modes: the title's controls and MODES's swipe

The owner, playing build-334: "mode selection is still unresponsive for a while once main menu is loaded, all buttons should be
pressable once drawn, decoupled from intro animations. also mode selection UX is very twitchy and sensitive when swiping to choose a
mode. should be 1:1 when swiping left/right."
- **The mode pill waited for the title's transition to end.** `Play::openModes` returned while any transition ran, and the intro's
  runs four beats past the title's build (the run behind coming up), home's after a run and the build back from MODES as long: a
  drawn, lit pill did nothing for two seconds or more (the earlier fix, 99567e7, ended the build on an input but not that tail). Now
  a tap on it ends the title's transition where it stands (`Play::settleTitle`: the band's filter opened) and MODES lifts in.
- **Every title control takes a tap from its first light**: CO-OP, SETTINGS and the pill took taps only once fully faded in
  (`TitleLook::sideTaps` at 1); PLAY and they now from above 0. The quiet title (it leaves after eight bars without an input):
  while PLAY or the side controls are still, or again, on screen they take a tap; only the input that wakes a title whose controls
  have all gone is eaten (a tap where PLAY was would press it as it came back).
- **MODES's row follows the finger 1:1**: it stepped a whole card at every 80 px of a drag, so a swipe skipped cards. Now, pressed on
  the cards' band, past 12 px of slop the row moves with the finger (a card's step of travel is a card, the slop's distance
  included), the centre changing as a card passes half way (its tick, its note and the pulse, as a step); let go, it settles on the
  nearest card over half a beat; a press on a row still settling catches it where it stands; a drag never taps a side card (a tap,
  under the slop, still brings it to the centre).
Pictures (`captures\menus\`): `m8_intro_pill` (a tap on the pill at intro beat 3.4, as it fades in: MODES lifts in at once) and
`m8_modes_drag` (TRACERS at the centre, a finger drawn 1.3 cards to the left over 24 frames and let go; the log's lines give the
finger and the row each frame: the row is the finger's, re-centred on SONGLINE at half a card, settling from -0.3 to 0). The
solo flow test passes (52 checks). Not yet seen under a finger on a phone.

## 2026-11-16, the pause's controls stand out mid and late game

The owner: "pause menu buttons need more visibility and visual/audio interaction, sometimes they are hard to see when pausing
mid/late game." Three causes, seen in pictures ten minutes into a run: the pause dims the run by its exposure (x 0.2), which cannot
dim a late run's saturated light (hundreds of overlapping lines stay white after the tone map); the priority layer (every bullet's
rim and core, the ships' cores) is drawn after the tone map, so the dimming never reached it; and the run's own HUD went on at full
level under the controls (a frozen Track card across RESUME, RESTART and SETTINGS, a Boost's words across QUIT RUN). Now:
- A scrim over the run behind the pause (`Ui::scrim`, black at 0.8 on a new layer `FrameDraw::under`, drawn after the priority
  layer and before the HUD), lifting over the count-in's three beats; under SETTINGS opened from the pause too.
- A dark backing under each of the pause's controls (`Ui::setBacking`, 0.75, 6 px larger than the pill).
- The priority layer fades with the run (`FrameParams::threatLevel`: each segment's opacity scaled in `capsule.vert` and
  `glyph.vert`): 0.3 under the pause and SETTINGS from it, 0.4 under a run's end, back to full over the count-in.
- The run's HUD is gone under the pause (`setHudUnder`; the pause's own line names the score, the Track, the time and the Chain) and
  returns over the count-in (the Track card from its second half).
- Interaction: a finger landing on a control is heard (the interface tick) and felt (a light pulse) at once; a tapped control
  leaves an echo (a white flash in it, its outline swelling 22 px out and fading over 0.35 s, on whatever screen follows); YES is
  the confirm sound, NO the back sound (solo and co-op pause).

Pictures (`--picture screen_pause_rest`, `screen_pause_press`, `screen_pause_tap` with `--strip 8`; `_late`: ten minutes of the
bot's run first): `captures\pause\`, and on a Galaxy Tab A11+ (Mali-G615) the same. Debug: no validation message; the flow test's
solo ways in and out pass. Not played by hand yet; `design/screens.md` "Pause" (the lead's file) does not have it.

## 2026-11-16, a tap during the title's intro works at once

The owner: "It seems the menu buttons are locked until the full Raveform intro plays." As built (`design/screens.md` "Title"), an
input during the intro skipped it but was eaten, and the controls took taps only from the intro's fourth beat: a tap on PLAY
started the skip, and only a second tap, once everything had arrived, played. Now any input ends the intro at once (everything on
screen in that frame, the word's notes not yet placed not played) and the input is the screen's: a tap on PLAY plays, on CO-OP or
the mode pill opens it (`Play::titleIdleFrame`). The same for the other builds of the title that held the controls: home's after a
run, and the title building back from MODES's BACK; and MODES's lift (`Play::liftComplete`): an input puts MODES at rest at once and
is MODES's. The intro with no input plays as before. Picture `m8_intro_tap` (`review-m8.ps1 -Part p0`): the intro, a tap where PLAY
stands at rest half way through the word: the title whole in that frame and the drop-in started by the same tap. `design/screens.md`
(the lead's) still says the input is eaten: the owner's word changes it.

## 2026-11-15/16, the two-hour stress test on five devices

The owner: "stress test the game (2 hours) with connected devices". `scripts\stress-devices.ps1 -RoundMinutes 38 -Rounds 3`
(the build of 403cd1a and c786795, as deployed), 23:32 to 01:34: three rounds of four-seat co-op with the bots playing
(`coop-phones.ps1`: round 1 the Moto hosting, the others joining by address; round 2 a tablet hosting, joining by room code, the
Moto sent HOME for 18 s at 604 s and back; round 3 the second Moto hosting, joining by the room list, a tablet's app killed at 1370 s),
while the fifth device played alone with `--autoplay`; every device's PSS and battery temperature every 5 minutes
(`captures\stress\20261115_233207\`).

- No crash, no ANR, no disconnect anywhere. Each round's room played the set to SET COMPLETE (16 to 16.5 minutes, every boss and
  mini-boss falling in order) and went on into the loop (Track 14 to 17).
- Round 2's HOME: the room paused for the away seat (`away: seats away now 0x4`) and went on when it was back 18 s later.
- Round 3's killed app: `seat 3 left the run` and the three others played on; all three went down at Cadence in the loop's Track
  17 (the loop's ramps), the host's `--coop-start` started the next run and it ran to the round's end.
- The net in the long runs: round trips 16 to 30 ms, loss 0 to 0.1%, downstream to a client 210 to 270 kbit/s, the on-screen
  error 3.6 to 12 wu RMS at the end of a round.
- Memory (PSS) flat across the two hours: it rises 5 to 25 MB in a round's first minutes and holds (the Motos 148 to 160 MB, the
  tablets 204 to 247 MB), back down with each round's new process. Battery at most 33.8 C (the second Moto), the tablets 29.8 to
  32.7 C.
- Not covered: the OnePlus 12R (not attached); the release build (the dev build as deployed).

## M8 — the modes (docs/MODES_PLAN.md), from 2026-11-15

The owner: "can you implement this ... No subagents" (the lead's line-up: THE SET and six modes). The plan's own answers to its
section 11 are taken (the owner gave no others): all six, the cheap three first, TRACERS with no gun and no Trios, the underdog's
help on by default, HEADLINERS by the clock, THE SET in versus ending at the tenth boss. The lead's files were committed as
written (1a3276b) on the owner's word.

**The set's determinism hashes before part 0** (`raveform_headless --sim determinism`, c786795; they must not move): seed 1, two
minutes: careful `f8d9e8972c671db2` (other seed `06b4a9490bb9f158`), careless `6d9e281fa6d95425` (`29b1864dde16a0dd`); seed 7,
four minutes (`--seed 7 --minutes 4`): careful `037b1a9f3492c90a` (`ed4d283029a634b0`), careless `3d1a03187dfbc288`
(`1db80cb7a2516f3a`). The run's setup enters the hash only when it is not the set's (so the set's stays as it was).

### Part 0, the frame and the menus (2026-11-16)

Built (`docs/MODES_PLAN.md` 8, part 0): the setup through the world (`World::resetRun(..., setup)`, `setNextSetup`, restarts keep
it), the game (PLAY on the title starts the pill's mode; the title's demo, home and a cancelled drop-in are always the set's), the
lobby and the run's start (`MsgLobby` + the room's setup and each seat's crowns, `MsgRunStart` + the setup, the beacon + the mode
and a versus bit: protocol `0x5246050C`, `kBeaconBytes` 34); the forms (`sim::formPlace`; tested on the timed forms and a test
form with a held section; no mode uses them until part 1); `modes.json` and the bonus Tracks embedded (`ModesData`, in the content
hash); `modes.txt` (`ModeRecords`: the mode last played, each mode's Track, every record; the hints' bits to twenty); the title's
mode pill (its emblem moving on the beat, its caption, the best line of the pill's mode); MODES (the seven cards in the line-up's
order wrapping round, the seven emblems of README 10, the dots, the Track line, the strip of ten chips, the set list for THE SET
and HEADLINERS; a side card tapped, the row dragged 80 px, arrows, D-pad or stick; a chosen Track heard from the next bar line with
a crash, NEXT BAR counting the bar out, the palette cross-fading over the bar); the room's mode in the lobby (the host's MODE pill
opens MODES for the room: DONE sends the mode, its Track and TEAM / VERSUS / HELP to every seat, BACK changes nothing; the
others read it at the right margin; a change struck white-hot for 60 ms with a glass tick; the versus band line; crowns under the
seats), the join list's rows (the room's mode and VS); the transitions (`Trans::Lift`: the pill held pressed with its ring, the
title leaving over the next beat, the rule rising from y 432 to 150 over the beat after while MODES is struck onto it a letter a
sixteenth, the cards thrown down from the rule in its second half, the controls rising on the third, any input skipping it to its
end; `Trans::Fold`: BACK folds the cards up into the rule (sides first) and streaks MODES out over half a beat, the rule sinks to
the title's place over a beat and the title builds from it as home builds it; the room's way in and out: the lobby fading, the
rule thrown out at y 150; the drop-in from MODES: the cards, strip and BACK fade and sink, the tubes run hot, the rule draws in to
the frame's centre, PLAY shrinks into the point, the burst's rays in the mode's two colours); `--mode`, `--versus`, `--no-help`,
`--coop-mode`; the pictures (`scripts\review-m8.ps1 -Part p0`).

Decided alone:
- **A release build shows no card that does nothing** (the plan): it lists only the built modes, so while THE SET is the only one
  it has no pill at all (the title as before) and no TEAM / VERSUS in the room's MODES. Dev builds show all seven, the unbuilt ones
  with PLAY disabled and `COMING IN THIS BUILD'S NEXT PART`. The owner sees MODES on a dev build (`scripts\deploy-android.ps1`).
- The pill's caption sits 28 px under the pill as CO-OP's does (its top; baseline 682): the page's "55 px over PLAY" put it 16 px
  from the pill, inside the 24 px rule.
- The lobby's `WAITING FOR ...` line moved from right of START to its left (the MODE pill has START's right); the MODE pill
  narrows where the screen is narrower than 2400 x 1080 (a tablet's 16:10) so it never reaches START.
- The room's choice is taken on DONE (one change, one strike for everyone), not as the host browses the cards.
- The lift's notes: the letters A3 C4 E4 A4 C5 a sixteenth apart from its second beat, the centre card's E5 as it closes (with the
  fourth letter), the sides' G5 and A5 with the fifth. Stepping along the row plays the first seven, a card each.
- BACK's fold: about five beats from the tap to the title's controls taking taps (half a beat of folding, a beat of the rule
  sinking, then home's build from its half beat); Back during MODES's drop-in cancels it as on the title.
- A Track chosen in a room's MODES is heard on that device only; the others hear the room's choice when the run starts.
- `--mode` and `--coop-mode` refuse a mode not built yet (the start fails: exit 1).

Measured: `scripts\test.ps1` passes (the sim suite's new `modes` test: modes.json, the forms walked, `--mode`'s grammar, the setup
in the world's state; the save suite's `modes.txt`; the net suite's setup in the lobby's message and the run's start, the beacon's
mode, the refusals). **The set's determinism hashes are unchanged**: careful `f8d9e8972c671db2`, careless `6d9e281fa6d95425`.
The p0 pictures (`captures\m8\p0\`): no validation message, no layout warning, the title with the pill keeps its 24 px. PLAY on the
title takes the same path as before (`dropInTap`, the run on the first bar line at least two beats away).

### Part 1, first half: ONE TRACK (2026-11-16)

Built: the run's setup for ONE TRACK in the world (`resetRun`: the chosen Track of the set from its first Groove, the set list at
ten, nothing of the loop); the kit (`World::dealKit`, `src/sim/world_modes.cpp`: the Trio's own dealing and the careful bot's pick,
`botPickOrb`, from a stream of the seed, the seat and the act; every device deals every seat's alike; each pick applied as it lands,
one a sixteenth after the first downbeat over two bars, the last ones doubled up; `EventType::KitLand`); the boss's death brings
the outro (`DirectorState::outroBar`, four bars of Break from `modes.json` `oneTrack.outroBars`; no Trio opens; every Shard flies
in) and the run's end on its last bar line (`EventType::RunEnd`); co-op carries the outro's bar (`MsgDirector`, protocol
`0x5246050D`). In the game (`src/game/play_modes.cpp`): the name card's words (`BEAT ITS BOSS`), the load-in (the kit's icons
spiralling in a quarter turn from 300 wu with a trail, a chirp a landing climbing a minor pentatonic over two octaves, the closing
ring and chord on bar 3's downbeat, the HUD's band row flashing), `TRACK COMPLETE` with the score through the outro, the result
(`TRACK COMPLETE` in gold or `RUN OVER`, TIME, BEST CHAIN, KILLS, BOSS seconds, `TRACK BEST` or `NEW BEST`), NEXT TRACK (the next
Track as a new run with its own kit), `one_<track>` and `team_one_<track>` (never the set's table); the band starts a run on its
Track (`conductBand`) and holds a Break under the result. Headless: `raveform_headless --sim mode [--track N] [--seeds 20] [--policy
assist|novice]` (ONE TRACK's first runs by Track: cleared, kit, minutes, the boss's seconds) and `--sim kit` (the kit measured);
`--kit-scale F` (tuning). Pictures: `scripts\review-m8.ps1 -Part p1` (`m8_loadin`, `m8_card_one`, `m8_one_outro`, `m8_result_one`).

**The kit, measured** (`--sim kit --seeds 20`, the assist bot in THE SET, every run reaching Track 10) and written into
`design/modes/modes.json` as its page asks (the lead's file; `measured: true`, the date and the estimates in its note):
atTrackStart 0, 4, 12, 18, 23, 27, 31, 34, 37, 38 (estimated 0, 6, 14, 19, 23, 27, 31, 35, 39, 43); atBoss 1, 8, 16, 22, 26, 30,
32, 35, 37, 39 (estimated 4, 12, ..., 46): the later Tracks hold fewer than the estimate (the pool caps).

**The exit's first line** (`--sim mode --seeds 20`): the assist bot clears every Track 20 of 20, and so does the novice. The novice's
boss fights (median seconds, Tracks 1 to 10): 39, 27, 11, 19, 6.5, 17, 13, 17, 15, 28; the assist bot's 25, 11, 10, 15, 6.8, 7.8,
11, 18, 12, 35. **Short of the 30 to 60 second line on Tracks 3 to 9**: the kit is the careful bot's picks and count, stronger than a
novice's own build in THE SET. With half the kit (`--kit-scale 0.5`) the novice's fights are 40, 35, 18, 33, 10, 27, 19, 29, 37, 49
and it clears 11 or 12 of 12; with a quarter, longer still but Track 9 cleared 3 of 12. Left at the measured kit (the page's
definition, and the owner's rule that the game is easy and beatable with the auto-aim alone): **for the owner** whether ONE TRACK's
fights should be longer (a smaller kit) at the cost of some clears.

### Part 1, second half: versus for THE SET and ONE TRACK (2026-11-16)

Built (`design/modes/README.md` 7 and 8): the knock-out (`World::knockOut`: a hit with no cell, after Auto Nova and an Extra Life,
knocks the ship out until the second bar line after; it comes back where it was with two cells, a panic pulse, a bar unhurt and half
its Chain; its own device decides, `kPeKnock` tells the others; `BACK IN 2`, `BACK IN 1` where it stood); the crown and the
underdog on every bar line (`World::versusBar`, the host's or solo's: the leader by score, none while level; the last, under
`modes.json` `versus.underdogShare` of the leader, with the host's HELP on), sent in `MsgDirector` (protocol `0x5246050E`); the
underdog's Shards count double for the Chain; the shove (`World::rivalShove`: a Dash's 140 wu ring pushes each rival ship in it
200 wu over a quarter second, a push it rides in the third shove slot, applied by the rival's own device; not a ship that cannot be
hurt); shared Boost orbs (one orb, owner "anyone"); THE SET in versus ends at the set list's last boss with the outro, no encore; the
versus HUD (your own score; the ranking with the crown and UNDERDOG), the crown over the leader's ship (18 wu, 26 wu above, the
pickup's white-gold), the podium at the match's end (WINNER or A DRAW, the winner's ring, ship and crown, the table with each
seat's crowns, the host's REMATCH, LEAVE), the room's crowns (the host gives the winner one); versus keeps no records. TEAM /
VERSUS is offered in the room's MODES from this build. `raveform_headless --net soak --versus`.

Measured: the 5-minute four-bot net soak, versus against team: both pass, census 450 checks and 0 mismatches each; the on-screen
error RMS 1.70 wu (team 1.56), p95 2.0 (2.25), max 165 wu (85: a shoved or knocked-out ship's moment); downstream 268 kbps at most
(264). The set's determinism hashes are unchanged.

### Part 7, the parts' open items (2026-11-16)

The owner's `/goal finish the plan`: every part's "Not done" list worked through, the plan's exits measured. Commits 464cb30 to the
last of the day.

Built:
- **The moments** (MODES_PLAN 2.10, `--mode <id>:<track>:<moment>[:hold]`): `dropN` / `waveN` (a timed form's Nth Drop; held, the
  section counts on in its last half and the run never ends), `actN` (HEADLINERS's act N, its boss's fight; held, the boss keeps its
  health), JAM's `drop` (DROP on the first bar; held, the Drop never ends and the room's DROP and Track pads wait) and `all` (every
  layer on, the mixer locked). Solo, the fast-forward runs to the moment; **a networked host** (`--host --loopback-bots`) sends it in
  the run's setup (`RunSetup::moment`, the setup's flags bits 2-7: protocol `0x52460512`), so every world of the room starts at that
  section, held, with no fast-forward. `--coop-mode <id>:<track>:dropN[:vs]` does the same for a room's runs (tests).
- **A timed run's count**: the result (or the podium) now arrives on the bar line after the count's bar (README.md 6; it came at
  the last downbeat and hid the count in the game, the pictures having shown it without the flow). **The run bar** under the goal
  line (the form's sections at 0.3, Drops 0.6, a white dot; blinking on the beat in the last eight bars, `LAST CALL` under it in the
  mode's colours for two).
- **The band** holds the progression on one chord (`Band::holdChord`, sound.md "What the band needs" 3; the audio suite's test).
  **sound.md "Every mode"**: the name card's opening sweep and stab, LAST CALL's two bells, the last bar's roll, riser and impact, the
  last downbeat's low-pass, the knock-out's falling glide, BACK IN's ticks, the come-back's sweep and Cell note, the load-in's bed (kick
  and hats, a layer every four landings); ONE TRACK's outro (the gong, the home chord held, the lead off, TRACK COMPLETE's chord an
  octave up, the reversed cymbal into the result); HEADLINERS's act card (the elite's fifth, a bell a fallen act) and HEADLINERS
  COMPLETE (gong, stab at 1.2, the title's eight); the haptics table.
- **FULL BLAST**: the torrent (the Riffs at 0.8 with their rings, the 700 wu ring), nothing hostile in the opening's first three bars,
  each wave's reversed cymbal and stab, **the power cut** (the music behind the pause's low-pass, the band row at 0.3, `BACK IN n`;
  back: a ring 90 to 600 wu, the row flashing in turn, the Drop's impact), **the load-out** (the band row's icons out of the ship a
  sixteenth at a time, the halo's arcs last, the score's digits rolling, the chirps down into the back chord), the result's nine
  sections with the waves named, `TONIGHT'S BAND`, the top word in the mode's colours, versus's underdog Chain not halved by a hit.
- **The podium on the music** and **LOBBY** (`Session::backToLobby`: every device back in the room's lobby with its seats, crowns
  and mode; `--coop-back-lobby` taps it after a rematch).
- **HEADLINERS**: the bill (ten stars under the act's card); **versus's damage race** (each act to its boss's most damage as the host
  counts it, arena operations 11 and 15; the race bar under the boss's; `ACT n: Pn` with its bell and lead; the underdog's +20% an act
  behind; a tie on acts to the bill's damage leader).
- **TRACERS**: a pop's bodies staggered from the line inward over a thirty-second, the edges' glow, the rig's beams (x 5 and up, FULL
  HOUSE's chase), FULL HOUSE's flash and confetti, the pop's sound by its bonus, ECHO's chord, the floor going dark, the lights going
  out, a cut, a spin-out, the count's chirp a column, the three first-run hints.
- **SONGLINE**: the gold gate's Boost icon, a step down's ring shattering outward, the underdog's wider ring, another player's gates in
  its own voice. **JAM**: the mixer pads' live signal lines, the walls swelling on the kick, the background breathing with BASS, the
  dancers standing still with the kick off, SOUND naming the next sound, the shelves named (`WEAPONS 1 OF 3`, `RIFFS 2 OF 2`), EMPTY's
  icons flying back to the pads, a hold's two rising notes, the pause's `LEAVE` (a full control). **A versus match's count** side by
  side in the players' colours (POP-OFF's chirps by tens).
- **Every way in and out**: `--flow-test` (`game/play_flow_m8.cpp`) drives the game's own screens by taps on their controls (found by
  label where the screen drew them) and the pause key, mode after mode; `scripts\flow-m8.ps1 -Part solo|room|all` adds the room's ways
  (two processes a room on this PC).

Measured:
- **Every way in and out** (`scripts\flow-m8.ps1`): solo, 52 checks and no failure (the seven modes: the title's PLAY, MODES's PLAY with
  the row stepped to the mode and a Track chosen, the pause's RESTART and QUIT RUN (JAM's LEAVE), the result's RETRY, ONE TRACK's NEXT
  TRACK, the result's HOME; HEADLINERS's runs played to the tenth act's fall, 456 and 542 s); the rooms, two processes each, every one
  passing: TRACERS, SONGLINE, FULL BLAST and POP-OFF in VERSUS (START, the match to its podium, REMATCH, the second podium, LOBBY: both
  peers back in the lobby), TRACERS and FULL BLAST in TEAM (START, the run's end, the host's RETRY, the second end). On a tablet (a
  Galaxy Tab A11+, shared, over wireless debugging; the dev APK with `--flow-test`, the FLOW lines read from its log): the same 52
  checks through the game's own screens, no failure, in 5 min 40 s (the Motos were not attached that day).
- **MODES_PLAN 7's benchmark matrix with the moments held** (the Moto, shared, tier low, 60 s; the panel's grade, Swappy's statistics):

  | Moment | Frames, missed | GPU p50 / p95 / max (ms) | Notes |
  |---|---|---|---|
  | FULL BLAST, wave 4 held (`blast:10:wave4:hold`, warm-up 3 s) | 3,594, 0 | 13.34 / 14.55 / 16.05 | 619 bodies (913 at most), 450 shots alive, none capped; main thread p99 7.8 ms |
  | The same hosting three bots (warm-up 12 s) | 3,589, 0 (5 late on the CPU side, 0.14%) | 12.63 / 14.38 / 15.90 | 1,313 bodies at most, 554 shots; main thread p99 10.6 ms: the contract holds, no fallback needed |
  | TRACERS, the last Drop held, four players | 3,594, 0 | 13.09 / 13.96 / 15.24 | 200 bodies (the Stage's cap) |
  | SONGLINE, the last Drop held, four players | 3,594, 0 | 13.37 / 14.47 / 15.38 | 619 bodies at most |
  | JAM, the Drop held, `--loadout max` | 3,594, 0 | 13.56 / 14.44 / 15.23 | (a first try lost its Drop to the wanderer's Track pad: now a held Drop keeps its form) |
  | TRACERS, four players, on a tablet (1920 x 1200) | 3,628, 0 | 13.99 / 14.48 / 16.17 | the Stage at the tablet's zoom |

  No heap allocation in any measured window.
- **HEADLINERS's race** (MODES_PLAN part 2's exit: `--net soak --minutes 9 --players 2 --mode headliners --versus --policies
  play,assist`, three seeds each way; acts won by the aiming bot against the assist bot, every peer counting the same): help on 8-2,
  6-4, 3-7 (17 to 13); help off 6-4, 3-7, 7-3 (16 to 14). The two bots are close; the help moved one act in thirty. Every run reached
  act 10, census clean.
- **FULL BLAST's runs on all twelve Tracks** (`--sim mode --blast`): every run ends on its 84th bar, no pool refused anything; the
  child-like bot on Track 10, five seeds: no knock-out (the bar: three).
- **The soaks**: FULL BLAST in co-op and versus with four players (3 minutes), TRACERS, SONGLINE and JAM in versus (1.5 minutes),
  JAM's room (2 minutes): pass, census 0 mismatches. The unit suites: sim 6,201 checks, net 129,669, audio 2,822 (the held chord's test
  new), no failure. **THE SET's determinism hashes are unchanged** (careful `f8d9e8972c671db2`, careless `6d9e281fa6d95425`).
- **Pictures** (`review-m8.ps1`, `review-m4b.ps1`): `m8_bl_*` (part 3: the torrent, the waves, LAST CALL, the power cut and back, the
  load-out, the result), `m8_card_act3`, `m8_podium_table`, `m8_podium`, `m8_lobby_back`, `m8_race`, `m8_act_won`, `m8_modes_bonus`,
  `m8_jam_pad_states`, `m8_jam_empty`, `m8_sl_gold_gate`, `m8_sl_streak_1..4`, `m8_sl_bend_back`, `m8_sl_race_4`, `m8_tr_lights_out`,
  `m8_tr_turf_4`, the strips onto each new card `m8_t_mode_<mode>`, `m8_podium_4` and `m8_vs_hud` (four seats): no layout warning (rendered with the dev build; the Debug build's validation not rerun for these).
- **Part 1's two-phone check** (`coop-phones.ps1 -Serials moto,moto2 -HostArgs '--coop-mode one:3:vs --coop-back-lobby'`, the bots
  playing both seats, `captures\m8\phones\085215_m8_part1_vs\`): ONE TRACK on Reef in VERSUS, two matches (TRACK COMPLETE at 79 and
  84 s, the podium on both phones each time), the host's REMATCH between them, LOBBY after the second: both phones back in the room's
  lobby with its crowns `1 1 0 0` (each seat won one). The rankings each phone logged on its bar lines: the same on 96 of the 99 bar
  lines both logged; the three that differ are early bars of a run with the two scores a few points apart (a phone counts the other's
  score as it last heard it). The crown and the underdog (the host's word) the same on every one of the 99 within a bar (79 on the
  very bar line).
- **The sanitizers** again over the changed code (`sanitize.ps1 -Only tests,modes`: the four unit suites, determinism, a bot's run of
  each mode and four of their rooms): all clean.

Decided alone:
- **A timed run's result waits for its count**: one bar after the last downbeat, on the bar line, as the page says; the run behind
  goes on (its bodies popped, the ship still flying) through the count.
- **LAST CALL** is the run bar's (under it, two bars, the mode's colours), as screens.md has it, not the goal line's (which keeps its
  count).
- FULL BLAST: the torrent's Riff rings go to 220 wu and the power back's to 600 (the page names the 600 and the closing 700 only);
  the Trios close at a timed run's end (nothing left to choose in the count); the opening's quiet is its first three bars.
- The podium's table is 1,320 px wide (at 840 the names and the counts overlapped); a seat named for itself (`P1`) is named once.
- HEADLINERS's race: an act nobody hurt goes to nobody; the clients' race bars take the host's shares once a beat; a tie on acts at
  the end goes to the bill's damage leader when it shares the top; the underdog's bonus is weighed on the shooter's own device (its
  claims carry it, as armour's share does).
- TRACERS: a pop's bodies leave at the pop and their kill flashes follow by up to a thirty-second (outermost first).
- JAM: the shelves' names are WEAPONS (shelves 1-3), RHYTHM (4), UTILITY (5), RIFFS (6-7), BOOSTS (1-3) (the page names two); the
  dancers stand still with the kick off (their timers run on); a held Drop (the benchmark's moment) refuses DROP and the Track pads.
- **A networked moment starts the run at its section** (the ones before skipped) rather than fast-forwarding the host alone; its
  crowd fills during a 12 s warm-up (the plan's 3 s is for the solo fast-forward, which arrives with the crowd).
- `--coop-back-lobby` taps LOBBY after a rematch, so one room shows START, REMATCH and LOBBY.
- The flow test runs the screens on the simulated clock (a step a frame) and saves nothing; its runs are fast-forwarded to their
  ends with hits costing nothing (the ends are the screens' business here, not the fights').

Since taken (the same day): the TRACERS pictures with a second ship flown by script (`m8_tr_cut`, `m8_tr_spin`, `m8_tr_duet`), a
ring of x 5 and up (`m8_tr_ring_big`: the ship circling 320 wu under LONG in a Drop) and each of the nine Boosts of the mode's
list on the Stage (`m8_tr_boost_<name>`); part 1's versus moments (`m8_vs_shove`, a Dash's ring shoving the rival, and
`m8_vs_knock`, strips) and the podium of a draw (`m8_podium_draw`: two rings, no crown; the draw forced for the picture, as two
counts level at the end cannot be scripted between two processes), `m8_podium_4`, `m8_vs_hud`; a duet ring's two leads (its
owner's on the chord's root, its partner's on the third, from the mode's tune data: `Play::barChordNotes`); a pop of x 2 as the
chord's triad alone (`Recipe::Stab` with p2 = 1, the engine's chord tones). The flow test also ran on a tablet (below).

`design/screens.md` took the built parts in part 7's closing pass (7d02e5e: its dated section "Modes (M8, built 2026-11-16)"
names them and points at `design/modes/screens.md`, the lead's file, as their spec, with PROGRESS's "M8" for what each left
undone). The Spark's pad patch is not a part to build: `design/modes/sound.md` ("SONGLINE", the Spark) gives it only if the
owner's ear says the bell is not enough (the engine's `sparkTimbre_` holds the voice; the pad's patch would be a fifth lead timbre).

**The gate the plan gives each part is the owner's play, and nobody has played the modes by hand yet.** What to try in each (the
plan's own questions, `docs/MODES_PLAN.md` part by part): MODES, do the menus feel like the game (the rule's lift, the cards, a
Track heard as it is tapped); ONE TRACK, one Track as a game of its own, and the first versus match in a room; FULL BLAST, is three
minutes of everything fun or tiring (the waves' pacing is the lever); TRACERS, is drawing a ring satisfying in the first ten
seconds; SONGLINE, does it feel like flying with the tune (the Spark's speed and the gates' size are the levers; the bell or the
pad); JAM, hand it to the youngest player in the house and watch, and Toybox by ear.

### Part 7: the whole (2026-11-16)

Done: the cards' dev-only line gone (every mode built: the release build shows all seven, TEAM / VERSUS in the room's MODES);
`scripts\sanitize.ps1 -Only modes` (a bot's run of each mode, and SONGLINE versus, JAM, POP-OFF and TRACERS versus rooms on the
loopback network, under ASan + UBSan); `--ui-scale 0-3` for pictures at the UI scale's four sizes; the documents of MODES_PLAN 9
(`PLAN.md` M8 built; `GAME_DESIGN.md` 4's pointer to the modes' runs; `TECH_SPEC.md` 7.1's modes' state and caps and 8.1.1's table of
who decides what; `design/screens.md`'s built modes; `design/look.md`'s modes' new things; `design/music/README.md`'s bonus Tracks;
CLAUDE.md's file map and commands).

Measured:
- **The sanitizers** (`sanitize.ps1 -Only tests,modes`): the four unit suites, determinism, a bot's run of ONE TRACK, HEADLINERS (all
  ten), FULL BLAST, TRACERS, SONGLINE and JAM, and the four rooms: no report, every run its full result.
- **The UI scale at 130%** (`m8_sl_groove`, `m8_jam_room4`, `m8_popoff_draft`, `m8_tr_line` with `--ui-scale 3`): every new HUD line
  fits, no layout warning.
- **Four devices, one room, each mode in TEAM and in VERSUS** (`coop-phones.ps1`, the Moto hosting, the second Moto and two tablets
  joining by address, every seat's bot playing, 1.6 minutes a room; each device woken and its lock screen dismissed first, as
  `stress-devices.ps1` does: the first try lost its rooms to three sleeping devices, "renderer: no window surface"): all fourteen
  rooms started; 138 census checks a room, 0 mismatches in every one; no crash, no error of ours in any log; 0 to 8 predicted kills
  undone a room (FULL BLAST's 8); the bands within 2 ms of the room's song.
- **The benchmark matrix of section 7** (the Moto, shared, tier low; the panel's grade):

  | Moment | Frames, missed | GPU p50 / p95 / max (ms) | Notes |
  |---|---|---|---|
  | FULL BLAST, wave 4 (`--mode blast:10`, 20 s from 107 s) | 1,198, 0 | 13.4 / 14.7 / 16.0 | 37,936 sparks a frame (68,886 at most) |
  | The same hosting three bots | 1,196, 2 | 12.4 / 14.4 / 16.2 | main thread p99 12.9 ms (the bots run in-process) |
  | TRACERS, the last Drop, four players (part 4) | 899, 0 | 12.8 / 13.8 / 14.6 | |
  | SONGLINE, the last Drop, four players (part 5) | 899, 0 | 13.3 / 14.3 / 14.8 | the gates one instanced draw |
  | JAM, every toy (part 6) | 3,594, 0 | 13.1 / 14.2 / 15.9 | |
  | The Stage at its zoom (TRACERS) against the set's | | 13.8 against 13.5 | the lattice over more of the screen: within 3% |
  | The band with the Spark's slot (part 5) | | | callback p95 0.87 ms of the 5 ms burst |

  **The set's own** (`--drop 10.1 --nohits --tier low`, today): 0 missed, GPU p95 13.5 ms (recorded at M5's end: 14.3; earlier 12.95),
  main thread p99 4.5 ms (4.9): within 10%.
- **THE SET's determinism hashes are unchanged** (seed 1, two minutes: careful `f8d9e8972c671db2`, careless `6d9e281fa6d95425`).

Not done in part 7: every way into and out of each mode driven by hand on a device (the paths are the set's own: the title's PLAY,
MODES, RETRY, REMATCH, NEXT TRACK, the room, the pause's RESTART; the four-phone rooms and the pictures cover the room's and the
result's), the transitions' strips for the new modes' cards (part 0's strips cover the frame), a haptics pass beyond the pulses each
part added. **M8 is built: the line-up's seven modes, co-op and versus in each, in release builds.**
Since done (part 7, the parts' open items, above): every way in and out, driven (`--flow-test`, `scripts\flow-m8.ps1`); the haptics of sound.md; the strips onto each new card (`m8_t_mode_<mode>`). Still: nobody has played the modes by hand.

### Part 6: JAM and POP-OFF (2026-11-16)

Built (`design/modes/jam.md`, `sound.md` "JAM" and "What the band needs" 5; `world_mode_jam.cpp`, the view `play_view_jam.cpp`):
**the room** on the Stage on the chosen Track (home Toybox), nothing hurting (the rules' god mode), four cells, the Nova back every
bar; **the 25 pads** of `modes.json` (the mixer's eight under the rig, DROP, TOYS, BOOSTS, EMPTY, SOUND, the twelve Tracks); the
mixer by a fly-over (entering it), the others by the Trio's two-beat hold; **the mixer** a bit a layer, switched on the next bar line
at least half a bar away; **DROP** the room's own Build of four and Drop of eight (`DirectorState::jamDropBar`, `phraseAt`'s JAM
branch: the Groove in eight-bar phrases), resting eight bars after; **the Track pads** the Track-change Break into any of the twelve
(a bonus Track its music and tempo over the crowd it borrows), the mixer kept; **SOUND** the band's sound stepping on from the
player's own; **the shelves** (`modes.json` `jam.shelves`: seven of toys, three of Boosts) as the Trio's ring round where the ship
stood, a beat in an orb takes it and every further beat a stack to its cap, a fifth Riff sends the oldest home, a due Fusion's gold
orb above the ring, a Boost for eight bars; the shelf stays within 300 wu and closes two bars after; **EMPTY** everything back in the
box; **presents** (a Boost orb at each Shard level); **the dancers** (twenty a layer on, twenty more a layer for each further player,
at most 160, x 1.6 in a Drop; an elite every eight bars from five layers; no threat budget); **POPS** (a pop one). **POP-OFF**
(versus; `forms.popOff`): the card's two bars, the draft (five Trios two bars each for every player), the Build (the dancers
gathering to 100), the Drop of sixteen bars kept at 200 bodies with an elite every four bars and a shared Boost orb every four; a
pop one, an elite five (the score, so the players' stats carry it to the podium). **The view**: each layer a light (KICK a ring
through the lattice on every beat, HATS star dust along the top wall, BASS a pool at each end of the floor, CLAP the walls and two
fixtures' beams on two and four, OPEN HAT two beams sweeping and crossing on the off-beats, SHAKER the floor's shimmer, CHORDS the
mirror ball's 24 glints a turn in four bars, STABS a fan of five beams from the bottom wall; the beams end at the walls), the pads
(edge, word at cap 14 wu, a mixer pad lit with its line up to its fixture or blinking NEXT BAR, the hold's fill in the player's
colour, DROP's double ring and clock, the Track pads in their accents with the playing one's name), the shelves (icon, name, pips,
the hold's fill). **The band**: `Band::setBedMask` (the bed by the mixer from the bar line; every bed layer in a Drop), the tick and
the sweep of a layer, the confirm chord, a toy's chord and its stacks' bells, EMPTY's back chord and closing sweep. The HUD's
`POPS` in the mode's colours (no Chain), POP-OFF's card and goal line (`PICK 2 OF 5`, `GET READY`, `POP!   BARS LEFT 13`), three
first-run hints. **Together**: a device asks for a room pad (`kPeJamPad`); the host puts the change on its bar line and tells all
(arena operation 10); toys, Boosts and EMPTY are the player's events (`kPeJamTake`, `kPeJamEmpty`): protocol `0x52460511`. The
wanderer (`Bot::thinkJam`). Tools: `raveform_headless --sim mode --jam [--minutes 10] [--track N] [--trace]`, `--net soak --mode
jam:12 [--versus]`, `scripts\review-m8.ps1 -Part p6`. JAM is built: every mode of the line-up is.

Measured:
- **The mixer** (the audio suite): all 256 settings on Toybox, a bar each in the Groove: none clips; the loudest (all but SHAKER)
  -15.9 dB against the Drop's -15.1 dB.
- **The wanderer**, ten minutes on each of four seeds: nothing refused by any pool, nothing left after any of 7-15 EMPTYs, 3 to 6
  DROPs, 4 to 17 Track changes, 184 to 265 toys taken, 255 to 312 bodies at most (a Brood's young past the dancers' 256).
- **The Moto** (shared; `bench_play --mode jam:12 --loadout max --tier low`, the wanderer playing, 60 s): 3,594 frames, 0 missed,
  the panel's P95 one period: the contract holds; GPU 13.1 / 14.2 / 15.9 ms, 16,193 capsules at most (the lights and the pads'
  words), worst audio callback 1.03 ms. **The room on the phone** (2400 x 1080): every pad's word reads at about 17 px.
- **Co-op and POP-OFF** (`--net soak --minutes 3 --players 4 --mode jam:12`, and `--minutes 2 --versus`): pass, census clean.
- Tests: the sim suite's JAM (every toy to its cap and emptied, the fifth Riff, a Boost's eight bars, the mixer on its bar line,
  DROP's Build and Drop, a Track pad's Break into Track 3); the audio suite's mixer. **The set's determinism hashes are unchanged.**

Decided alone:
- **The room's bar lines**: a change waits for the first bar line at least half a bar away, on every device (the host's word).
- A Track change completes on its bar line on every device (the director's state takes the new Track there), and ends any DROP's
  rest.
- HATS is the conductor's off-beat hat layer, OPEN HAT its open hat; CHORDS the pad at its full level.
- The stabs' fan flashes on the bar's first and third beats and CLAP's on two and four (the band's own steps are not read back).
- POP-OFF's pops are its score (one, an elite five), so the podium's counts come with the players' stats as the set's scores do.
- The dancers left over when a layer goes off stay (the director only fills); a Brood's young can take the room past 256.
- The wanderer: a pad at random (the mixer most often), held its beats and half a beat more; with a shelf open an orb one to four
  beats; EMPTY when the dice say so.

Not done in part 6: the moments (`--mode jam:12:drop:hold`), the pads' live signal lines, the walls' swell on the kick, the
background's breathing with the bass, `EMPTY`'s flight of icons back to the pads, the Trio-style two rising notes while DROP fills,
the dancers standing still with the kick off (they move as always), the SOUND pad's name of the next sound, the shelf's
label naming its kind (`WEAPONS 1 OF 3`: here `SHELF 2 OF 7`), POP-OFF's side-by-side count.
Since done (part 7's open items): all of it; the moment's JAM Drop, held, is benchmarked on the Moto.

### Part 5: SONGLINE (2026-11-16)

Built (`design/modes/songline.md`, `sound.md` "SONGLINE" and "What the band needs" 1 and 4; `world_mode_songline.cpp`, the view
`play_view_songline.cpp`): **the line** a bar ahead of the Spark, a point a sixteenth (`SongSpark::ahead`), turned every 8 wu by the
two slow waves (by distance flown, scaled by the section), by the walls and by rule 4, never tighter than `maxCurvature` (250 wu);
the finale's two bars a closing spiral (280 to 40 wu). **The gates** from each bar's arpeggio (7 wu a semitone about the bar's
middle, at most 56; a Build's wave shrinking to nothing by its last bar; a gate an eighth in a Groove and a Build, a sixteenth in a
Drop, a quarter in a Break (60 wu, worth three), a gold gate every 8 bars with its Boost; at most 256), threading (the ship's path
through the ring), in time (the Spark within 80 wu), a miss (the streak to the tier below, once a bar), the streak's tiers (16 score
x 1.5 and the Shard pull doubled, 32 a grid step, 64 Prism, 128 two steps and Chorus), the knock-out costing the streak and putting
the ship back beside the Spark, a Shard level giving a cell back (no Trios); **the crowd** at 0.7 of the set's along the line
(formations 600 to 1,200 wu ahead: rings round coming gates, walls across it, escorts beside it), counted whole, and every body
2,000 wu behind every ship and the Spark dissolving once a beat (the host's). **The view**: the dotted guide, the gates as one
instanced ring glyph each (the accent turned 4 degrees a semitone, snapping open in a sixteenth, a missed one fading grey, another
player's at a third, gold with its flares), the Spark (tapered arms in the scene's light, a rimmed white heart on the priority layer,
its pool, its tail over the last beat, the boss's double chevron off screen), the streak's rings, the in-time thread, the bursts,
the finale's eight rings and flash; the boss camera with the Spark as its boss. **The HUD** (`GATES n   STREAK n`, three first-run
hints), **the count** (the string lighting from the left over two beats, `GATES n OF m` counting), **the result** (`GATES 446 OF 512
87%`, `IN TIME`, `BEST STREAK`, `BEST CHAIN`; the string over the run's bar: lit, white in time, dark missed) and `songline_<track>`.
**The band**: the Spark in a slot that is nobody's (`Band::setSpark`: the beat's eighths, quarters or sixteenths of the bar's
arpeggio, the bell, the players' own level), a player's lead at any note (`Band::leadNoteAt`: each gate its note on the next
thirty-second, a third above in time), the players' slots silent and the gun a soft hat, the Build's top note held as a pad voice,
the streak's bells and glass, the come-back's saw glide, the finale's tom roll, impact, gong and bells, the count's chirps.
**Together**: a gate threaded travels as `kPeGate` (its birth, in time, the claim's ticks after it); co-op's four strings side by side
with the braid every four bars and each seat's harmony (0, 2, 4, 7 scale steps), the chord (every player's gate of a step: each Chain
one more, the step white, the stab); versus's one string, first come by the claim's tick, the host's word (arena operations 12-14:
the chord, the owner, the come-back), the underdog threading from 51 wu; the director's message carries the line a bar ahead once a
bar (a client more than 2 wu off takes it): protocol `0x52460510`. The rider (`Bot::thinkSongline`: the string pulled tight through
the next three rings, never back for a gate behind it, turning only for a body in its way). Tools: `raveform_headless --sim mode
--songline [--track N] [--policy play|slow|child] [--trace]`, `--net soak --mode songline:4 [--versus]` (each peer's gates, chords,
counts), `scripts\review-m8.ps1 -Part p5`. SONGLINE is built and in release builds.

Measured:
- **The line** (the sim suite, 12 Tracks x 10 seeds): at least 589 wu inside the walls, its tightest turn 331 wu, two worlds (the
  second ship 300 wu off the line) agree on every gate, every repeated bar of the progression the same shape (193,920 gates), 512
  gates a run at every tempo.
- **The rider** (20 seeds a Track): 91.4% (Afterglow, min 85.4) to 98.3% (Track 12) of its gates; no knock-outs. **The half-speed
  bot** is found again within 2 to 3 bars every time (the bar is 4). **The child-like bot** threads 97.2% of Aurora's gates, no
  knock-out (the bar: half, at most 2).
- **Co-op and versus** (`--net soak --minutes 2 --players 4 --mode songline:4`, and `--versus`): pass, census 0 mismatches; co-op
  159 chords on every peer, each seat's own gates 73-81%; versus: every peer counts every seat's gates as the host does; the
  clients' lines never 2 wu off the host's. TRACERS's soaks pass again (below).
- **The Moto** (shared; `bench_play`, Track 10's last Drop, four players: hosting three loopback bots, tier low; 15 s from 57 s):
  899 frames, 0 missed, the panel's P95 one period: the contract holds; GPU 13.3 / 14.3 / 14.8 ms, 139 bodies in view (282 at most),
  846 glyphs a frame (1,421 at most: the gates one instanced draw per shape). **The band with the fifth slot** (`--band full`: four
  players on the 1/16-triplet grid with Split, every Riff at 2, the Spark on every sixteenth; Track 10's first Drop): callback p50 /
  p95 / p99 / max 0.74 / 0.87 / 0.92 / 1.09 ms of the 5 ms burst (load 0.18 at p95), 56 voices at most, no steal, no underrun.
- Tests: the sim suite's line test; the audio suite's Spark (its own slot on the beat's eighths, quarters and sixteenths, each its
  step's arpeggio note); the net suite as before; the review pictures (Debug: 0 validation messages). **The set's determinism hashes
  are unchanged.**

Decided alone:
- **The wall rule picks its way round once** (the turn whose circle stays furthest inside the walls) and holds it until the line is
  clear: "toward the centre" taken afresh each step swept the line through a wall when the centre lay behind it, and in a corner
  flipped between the two walls and drove straight on (the line test found both). The waves may turn it harder that way, never
  against it.
- **Rule 4 holds one way round** while the lost ship is behind the line (a sign taken afresh flips about the ship's bearing and flies
  the Spark straight on), prefers the homing's side at a wall while that circle keeps 400 wu of room, and comes before the finale's
  spiral (a ship lost in the last Drop was never fetched); the spiral curls to the open side.
- **The guide's light** is 1.4 at the Spark falling to 0.45 a bar ahead, not the page's 0.35: drawn at 0.35 it could not be seen
  at all (screenshots brightened three times showed nothing).
- The Spark's arms stay in the scene's light; only its heart is on the priority layer (rimmed arms read as a gun sight).
- **After a timed run's last downbeat nothing lives on any device**: a spawn the host sent just before the pop could land on a client
  after it and live on (the census found 6 such in TRACERS's soak and 112 in SONGLINE's; both pass now).
- The crowd's count is every body alive (the line moves on faster than its hunters follow; the frame's count let 900 pile up).
- The versus claim's tick travels as ticks after the gate's birth (an absolute tick broke the player event's place bounds and took
  its packet with it).
- The count is drawn by the HUD (`Play::drawSongCount`), the finale's flash a white pool clamped at 0.35.

Not done in part 5: the moments (`--mode songline:10:drop2:hold`: the benchmark ran on a warm-up), the pictures the plan names beyond
these eight (`m8_streak_<1..4>`, `m8_gold_gate`, `m8_race_4`, `m8_bend_back`), the streak's ring shattering outward on a miss (a
spark burst stands in), the gold gate's Boost icon (a glow stands in), the underdog's wider ring drawn round the ship, the versus
count side by side, a remote player's gate notes in its voice on this device, the Spark's pad patch as the owner's fallback.
Since done (part 7's open items): the moments (held, four players: benchmarked), `m8_streak_<1..4>` as `m8_sl_streak_<1..4>`, `m8_gold_gate`, `m8_race_4`, `m8_bend_back`, the ring shattering, the gold gate's icon, the underdog's ring, the versus count, remote gate notes. The Spark's pad patch waits on the owner's ear (sound.md gives it only if the bell is not enough).

### Part 4: the Stage, TRACERS and the two bonus Tracks (2026-11-16)

Built (`design/modes/tracers.md`, `README.md` 4 and 11): **the Stage** (the walls as the world's state, `World::halfW/halfH`: ships,
bodies, bouncing shots, spawns and the Dash meet them; a fixed camera at the zoom that fits the Stage and its 40 wu margin; the
rig; a lattice of its own every 20 wu, allocated with the other, so every other line is a tile's side). **TRACERS**
(`world_mode_tracers.cpp`): the tracer (laid above 120 wu/s, a point every 16 wu, up to 160, living 8 beats plus one a Shard level to
16, twice under LONG, four more for the versus underdog; a hit or a knock-out puts it out; a node where each sixteenth fell); a ring
where the newest piece crosses an older one (the newest six points never count) or within the 36 wu snap, thinned to 48 corners,
catching every solid body inside (frozen, harmless, in its owner's colour), popping on the next eighth with the bonus table (an elite
loses a quarter of its health: four rings or a Nova), its tiles lit (a lit tile stays lit); a full house at 813 tiles (the chase on the
next bar line, the crowd's pop two beats later paying 5,000 x the house x the Chain, the floor dark a bar after it, the next house's
crowd and budget 15% higher); the floor paying 10 a tile x the Chain at the end; a knock-out costing the oldest quarter of the
player's own tiles (and back at the Stage's centre). No gun: the band plays the lead while the line is laid. The crowd: contact
species only (variant A, no Sentries, Tethers or Weavers, nothing that fires), half the Track's target (at most 200 bodies on the
Stage), half the budget, no dissolving in the Break, elites every eight bars from the second Groove (no Volatile, no Cloaked), a
Boost orb every six bars from the mode's list with the two gels (LONG, ECHO: every ring pops again a beat later, half as large
again). **Together**: rings go out as their corners (`Msg::Ring`, protocol `0x5246050F`), every device catching, popping and lighting
from the same corners; the host decides the houses (a ring of kind 1) and sends its floor every two seconds (`Msg::Floor`, a nibble a
tile, taken unless older than the client's own last ring); a ring's deaths are its owner's claims, a client's verdicts paying the
ring's bonus; a teammate's line crossed twice closes a duet ring (both owners, a check of the two colours, twice the bonus, the
partner paid with its own Chain); in versus a rival crossing your line cuts it (`kPeCut`), a rival inside your ring as it pops is spun
out (pushed 300 wu, its line out, two beats with none: `kPeSpin`), the tiles are the count (crown, underdog, podium, the turf bar).
**The view** (`play_view_modes.cpp`): the floor tinted in the lattice by tile, the lit regions' stepped edges, the tracer (deep to white,
dashed and flickering in its last two beats, node beads), the closing hint, the lock's white flash, caught bodies in the owner's colour
with a white dot, the pop's shock ring, pool and lattice impulse, `RING X5`, `FULL HOUSE` with `X n`, the tiles' light coming in and
blinking out, the count's sweep. The HUD (`FLOOR 62%   HOUSE 2`, `LAST CALL`, `TRACER 9 BEATS`, the gels' cards, no Riff slots), the
result (FLOOR, FULL HOUSES, BEST RING, BEST CHAIN, the floor drawn small, a gold star a house and a white tick a ring of x 5 on the
run bar; a timed mode's run behind its result at a fifth), the `tracers_<track>` record, the sounds from the band's recipes (a glass
lock, a woodblock fizzle, the pop's stab larger by the bonus with a tom from x 5 and the sub drop at x 8, the house's tom roll into the
impact, the gong and the stab). **The bonus Tracks** in the band (`audio::kMaxTracks` 12; Mirrorball and Toybox embedded from
`design/modes/music/`), calibrated, their palettes exported; FULL BLAST and TRACERS take Tracks 11 and 12 (the music and tempo over
the borrowed crowd); MODES's strip shows twelve. Tools: `raveform_headless --sim mode --tracers [--track N] [--policy child]
[--trace]`, `--net soak --mode tracers:3 [--versus]`, `scripts\review-m8.ps1 -Part p4`.

Measured:
- **The ring-flier** (`--sim mode --tracers --seeds 20`, floor-aware circles): fills a house in 20 of 20 runs on Tracks 1-4, 8 and 11
  (Mirrorball), 19 of 20 on 5, 6, 7 and 9, 17 of 20 on 10 (the fastest tempo: 1:15 a run); 0.9 to 2.2 houses a run; knock-outs 0 to
  0.55 a run (at most 2).
- **The child-like bot** (avoidance half a second late, no Dash): knocked out **2.45 times a run on Mirrorball** (2.35 on Reef's own
  tempo; at most 5), over the page's bar of 2. Its hits a run: drifters 4.7, caroms 4.2, shoals 2.8. For the lead: the crowd for a
  five-year-old (the Stage's crowd scale, or no Caroms on the Stage, or the first house's crowd lower) is the lever; nothing changed.
- **Co-op and versus** (`--net soak --minutes 1.5 --players 4 --mode tracers:3`, and `--versus`): pass; census clean, every predicted
  kill confirmed, the floors equal at the end, houses 3 / 3; 3 of 119 snapshots set a few tiles right (rings overlapping in flight).
  THE SET's soak passes as before.
- **The Moto** (shared; `bench_play`, Track 10's last Drop, four players: hosting three loopback bots, tier low; 15 s from 57 s):
  899 frames, 0 missed, the panel's P95 one period (16.7 ms): the contract holds; GPU 12.8 / 13.8 / 14.6 ms (p50/p95/max), 179 bodies
  in view (207 at most), 8,237 capsules a frame.
- **Mirrorball against the prototype** (`render-audio.ps1 -Only track11_*`): all three sounds and Toybox in club pass, loudness and
  every band and section within half a dB, onsets exact.
- Tests: the sim suite's TRACERS tests (a square both ways, a figure of eight, the snap at 36 and not 37, a three-tile fizzle, 48-corner
  thinning, two worlds alike, no shots or bullets; a duet, a cut, a spin-out); the net suite's ring and floor round trips and
  refusals; the audio suite at twelve Tracks. **The set's determinism hashes are unchanged.**

Decided alone:
- **A fizzle keeps the line** (rule 6 does not say): one dry tick a beat at most, and the line flies on, so a wiggle never costs the
  ring being flown round it (with the cut, the bots closed half as many rings).
- The hint offers no point of the line just flown (the newest six and as many more as 1.5 x its reach holds).
- **The tablet's zoom is 0.641**, under the page's floor of 0.75: the whole room wins (at 0.75 the Stage is 1,536 wu across on a
  16:10 screen, 90 wu short at each side). Its pixels a world unit (1.07) are close to the phone's at 0.784 (1.17).
- A Track's tonic is its key's (`Track::tonic`): Mirrorball's progression starts on its two chord, so the first chord's bass (which
  stood in for the tonic and is it in the ten and Toybox) would have put its effects, Shard runs and lead degrees in E.
- The bonus Tracks stay in `design/modes/music/` (the plan moves them to `design/music/`; `design/tools/bonus_tracks.py` reads them
  there).
- The floor's snapshot every two seconds (not only on a census mismatch); a house a client's floor fills waits for the host's.
- A duet's lines both start again from their ships (the page cuts loose only the pieces used).
- A knock-out costs a quarter of the player's own tiles rounded down.
- TRACERS is in release builds from this part (the owner's first play is its gate).

Not done in part 4: the moments (`--mode tracers:10:drop2:hold`: the benchmark ran on a warm-up instead), the pictures the plan lists
beyond these (`m8_ring_x8`, `m8_lights_out`, `m8_duet`, `m8_turf_4`, `m8_cut`, `m8_spin`, the nine Boosts' moments), the pop's
stagger over a thirty-second, the edges' glow pools, the rig's beams (x 5 and FULL HOUSE), FULL HOUSE's flash and confetti, the
sounds' exact notes (the pop's chord by its note count, the count's chirp a column, the gels' own), the HUD's first-run hints, the
versus count at the end (the sweep's tally side by side), the tablet's and the four-player benchmarks beyond the one above.
Since done (part 7's open items): the moments (held, four players, the Moto and a tablet), `m8_lights_out`, `m8_turf_4`, `m8_ring_x8` (as `m8_tr_ring_big`), `m8_duet`, `m8_cut`, `m8_spin` and the nine Boosts' pictures (as `m8_tr_*`), the pop's stagger, the edges' glow, the rig's beams, FULL HOUSE's flash and confetti, the sounds by the bonus (x 2 the chord's triad), ECHO, a duet's two leads, the lights, the cut and the spin, the count's chirps, the three hints, the versus count: all of it.

### Part 3: FULL BLAST (2026-11-16)

Built (`design/modes/full_blast.md`): a timed mode's fixed form in the director (`DirectorState::formCount`, `formKind`, `formBars`
from `modes.json` `forms.blast`: the opening and four waves, each a Build 4 and a Drop 16, at the Track's second cycle; `Phrase::wave`,
`barsLeft`; past the last bar the run is over: every body alive pops, no score, no Shards, and `RunEnd`); every power on from the first
bar as a kit (`World::dealFullBlast`: every Mod's stacks in the table's order, four Riffs to level 5 drawn by the seed (each seat the
next four of one shuffle), their Fusions: 109 landings over two bars, three and four a sixteenth); the waves (`fullBlast.waves`: the
crowd from its first scale to its last over the Drop of the Track's cycle-2 target, at most 400; elites in waves 2 and 4 on any Track,
not waiting for the bank; wave 3's mini-boss (Track 1 borrows Track 2's pair); a Boost orb every 4, 3, 2, then 1 bars); nothing ends
the run (the knock-out, the power cut, in the timed modes too); no Extra Lives fall. The game: the run's card (`FULL BLAST`, the Track,
`EVERY POWER IS ON`), each Drop's wave name on its downbeat, the goal line (`WAVE 2 OF 4   BARS LEFT 51`, `LAST CALL`), the result
(BEST CHAIN, KILLS, BOOSTS, HITS TAKEN), the `blast_<track>` record. `raveform_headless --sim mode --blast [--track N]`.

Measured (`--sim mode --blast --seeds 3`, the assist bot): every Track's run ends on its 84th bar (3:05 on Track 1, 2:11 on Track
10), no pool refused anything, the most shots alive 651 to 756, no knock-outs.

Not done in part 3: the torrent's look beyond the load-in (the Riffs drawn larger, the 700 wu ring), the power cut's look and sound,
the wave names' sound, LAST CALL's run bar, the count (the load-out), the result's nine-section bar and TONIGHT'S BAND, the Moto
benchmarks (wave 4 held, solo and hosting three bots), versus's underdog rule (a hit does not halve its Chain).
Since done (part 7's open items): all of it (the benchmarks held for 60 s, solo and hosting three bots).

### Part 2: HEADLINERS (2026-11-16)

Built (`design/modes/headliners.md`): the acts' form in the director (`DirectorState::actForm`: act 1 a Groove 4 and a Build 4 before
Kaleidos's Drop, every later act the set's Track-change Break into its Track, a Build 4 and its boss's Drop held; no cycles, no
mini-boss, no elites or arena events; the Build's fodder at half the Track's crowd); the kit as Kaleidos enters (`kit.atBoss[1]`) and
the top-up on each act's Break (what a player is short of the next boss's `kit.atBoss`, landed over its first two bars, the act's
own stream); the fallen boss's bonus Trio on the Break's third bar for its six bars; one Extra Life held at the start and one from the
bosses of acts 2, 4, 6 and 8; after act 10 the outro and the run's end. The game: the act's card (`ACT 3 OF 10`, the boss's name,
the Track and its tempo), the goal line (`ACT 3 OF 10   4:12`, not under a boss's bar), `HEADLINERS COMPLETE` with the time, the
result (the time in gold or `ACT n OF 10`; BOSSES or TIME, HITS TAKEN, FASTEST with the boss's name, SCORE), the `headliners` record
(acts, then time). `raveform_headless --sim mode --headliners`; pictures `review-m8.ps1 -Part p2`.

Measured (`--sim mode --headliners --seeds 20`): the assist bot and the novice play all ten acts in 20 of 20, the run 6.6 and 7.0
minutes (the page's 10 to 12). The fights (median seconds, acts 1 to 10), the novice: 35, 11, 10, 16, 6.9, 12, 12, 15, 16, 27;
the assist bot: 35, 11, 8.6, 14, 5.9, 9.8, 9.8, 15, 14, 25: short of 30 to 60 from act 2 on with the kit as measured, as in ONE
TRACK (the same question for the owner: the kit's size).

Not done in part 2: versus's damage race (each act to whoever did the boss the most damage, the damage bar under the boss's, `ACT
3: P2`, the underdog's damage bonus), the bill of ten stars under the act's card, the card's and HEADLINERS COMPLETE's sounds.
Since done (part 7's open items): all of it; the exit's race measured (the aiming bot against the assist bot, help on and off).

Not done in part 1 yet: the podium's arrival on the music (the table counting up, the winner named on the next bar line with its
bell and confetti) and LOBBY (back to the lobby with the room standing: the session has no way back to its lobby yet); the
two-phone versus check; the load-in's bed layers (the engine has no bed mask yet: `sound.md`'s `setBedMask`) and the
outro's held chord and reversed cymbal (the band plays a Break under it).
Since done (part 7's open items): the podium on the music, LOBBY, the load-in's bed, the outro's held chord and reversed cymbal; the two-phone check (part 7's open items, above).

Not done in part 0: `m8_modes_bonus` (the bonus Tracks come with part 4: the strips stop at ten chips; TRACERS's and JAM's homes
fall back to Zenith and Arcadia); the crowns' picture (nobody wins a match until versus, part 1); the built parts of
`design/modes/screens.md` are not moved into `design/screens.md` (`design/` is the lead's).
Since done (part 7's open items): `m8_modes_bonus`; the crowns in the lobby (`m8_lobby_back`). Still: `design/screens.md` (the lead's file) does not take the built parts of `design/modes/screens.md`.

## 2026-11-15, a crowded Shard field merges (no free Magnet Storm)

The owner: "Shards come to me even without a magnet storm power up ... That's a free magnet storm can we fix that?" Above
kShardsCrowded (1,536 out) the loose Shards older than a bar flew to their player. Now they merge (`World::mergeCrowdedShards`):
each player's in the same 60 wu cell become one Shard worth them all (`Shards::value`), where the first of them lies, living as
long as the youngest, to be collected by hand; a death in a crowded field drops one Shard worth all of its own (it dropped one
and the rest were lost). Collected, a Shard adds its worth to the Chain, the pickups and the level meter. The owner: "the value
of the shard should be reflected by color and size": bigger by a third a doubling (to 2.6x), golder to 8 and then towards white,
pulsing from 4 (`play_view.cpp`). Test `shard crowd`: 1,836 Shards far from the ship a bar and a half on: worth all 1,836 in
fewer than 1,536, none homing, the Chain unmoved; the biggest collected adds its worth.

## 2026-11-15, the owner's buffs and Wave's rework (after the table below)

The owner: "buff: Seeker, Fork, Pierce, Burn, Last Light, Rear Guard, Prospector, Afterparty, Crescendo, Comet, Gravity
Rounds. Wave needs a re-work ... Slipstream can be buffed slightly and rank 1 be max rank"; for Wave the owner chose the wave
front of three offered. The numbers (`tuning.h`, the old ones in brackets there and in GAME_DESIGN 6.2-6.3):

- **Wave**: the shots fly straight; on every beat a front rolls out along the aim, an arc of 90 degrees to 400 wu in 0.45 s,
  2 damage to every body it passes and each boss part once a front (seven rays); rank 2 140 degrees, 550 wu, 3. Drawn as a
  crescent (`play_view_upgrades.cpp`); the band's vibrato stays its sound. Helix Array keeps its strands.
- Seeker turns 5, 9, 14 rad/s within 420 wu and its shots pass through 1, 2, 3 bodies; in a fight a homing shot keeps to the
  boss unless a body is within 80 wu (the crowd drew them off). Fork's two carry 0.75 of the shot, home in, fly 0.9 of a life
  (Fractal's halves stay halves). Pierce +3 a stack and a quarter heavier after each body. Burn 3 beats at 2 a beat a stack,
  spreading 80 wu. Last Light once in 4 bars, 600 wu. Rear Guard with every volley. Prospector half a Shard a stack (whole
  ones sure). Afterparty four bars, rank 2 the whole Drop (the band's `leadGridIndex` too). Crescendo +10% a bar, a hit
  halves it. Comet 5 damage every sixteenth. Gravity Rounds crush 1 then 2 within 130 then 180 wu. Slipstream +20%, one rank.

Each alone at its cap on the base build (Split 1 + Double-Time 1; the novice, 40 seeds; the base alone: 117 bosses won, 10.2
minutes, the Choir in 88 s):

| | Before | After |
|---|---|---|
| Wave | 104 bosses, 9.1 min, the Choir 118 s (24 of 40 won it) | **125, 12.1 min, 37 s (40)** |
| Gravity Rounds | 111, 8.9 min, 90 s | **120, 13.0 min, 61 s** |
| Seeker | 114, 11.0 min, 95 s | **120, 11.7 min, 61 s** |
| Comet, Crescendo, Afterparty (the Choir) | 67, 75, 81 s | **47, 50, 58 s** |
| Rear Guard | 115, 10.1 min, 83 s | 119, **11.3 min**, 67 s |
| Fork, Pierce, Burn, Last Light (the Choir) | 73, 76, 79, 84 s | 71, 69, 81, 75 s (small) |
| Slipstream | 106 bosses (two ranks) | 116 (one rank: about neutral) |
| Prospector | 116 | 112 (its worth is the Trios it speeds, which this test leaves out) |

## 2026-11-15, each upgrade alone over whole runs (the owner: "measure each upgrade in isolation")

`scripts\isolate-upgrades.ps1` (`raveform_headless --sim isolate`, 16 processes at once): every Mod and Riff that can be dealt,
at its cap from the first tick, the loadout frozen (no Trio, no Boost), the novice bot from Track 1 to its first death (at most
40 minutes), 40 seeds, the same for every row; `-Base split,double_time` puts the first Trio's build under every row. Noise
(the bare ship on four seed sets): Track +-0.1, bosses won +-4, minutes +-0.5.

- **A wall at Track 4**: with the bare ship or the base build and one pick, every row but Pulse Ring stops at Event Horizon (the
  base build alone: Track 3.9, 117 of the first three bosses won in 40 runs, its runs ending at 10.2 minutes). The bosses' health
  is tuned for a real build (about 15 upgrades by Track 4), so past Track 3 these tables measure survival, not progress; the
  first three bosses' seconds measure damage.
- **Strongest**: Pulse Ring (the only row past Event Horizon: alone Track 4.9 against 2.8; on the base build 5.6 and 182 bosses
  against 117, the Choir in 48 s against 88). **Orbitals and Regen** at their caps keep the novice alive: alone 35 and 36 of 40
  runs alive at 40 minutes (the bare ship: none, 10.4 minutes), on the base build +12.5 and +14.3 minutes, but they kill nothing
  (stuck in Track 4's fight). Damage on the bosses (the Choir on the base build): Overcharge 30 s, On-Beat Crit 36, Heavy Step 37,
  Split 41, Swarm 50, Flak 54, Sentinel 55, Echo 57, Lancer 57. Survival through the crowd: Sweeper, Storm Coil, Ricochet, Extra
  Cell, Magnet, Aegis, Arc, Kick Pulse (+1.6 to +2.9 minutes on the base build).
- **Little or nothing**: Seeker (alone 2.8 Tracks, the bare ship's; on the base build 3 bosses fewer and the Choir slower: the
  assist already aims), Fork, Pierce, Burn, Last Light, Rear Guard, Nova Stock, Prospector, Afterparty, Crescendo, Comet,
  Minelayer, Singularity (alone). Not measurable here (they act on Trios and Boosts): Lucky Break, Collector, Encore.
- **Worse than nothing**: Wave (alone Track 2.5 and 60 bosses against 2.8 and 73; on the base build 104 against 117, the Choir
  won in 24 runs of 40 against 37, in 118 s against 88: the wave carries the assist's aimed shots off the boss), Slipstream (106
  against 117, the Choir won in 26: the novice runs into more, 4.06 hits a minute against 3.82), Gravity Rounds (111, -1.3 min).

## 2026-11-15, the upgrades for a young player (2): what the owner chose

- **The assist on the fast grids** (`core/lead_rule.h`, one rule for `World::spawnShots`, `Band::computeShots` and the engine's
  lead): on the 1/32 grid and faster it skips one step in four instead of every other one, so 2, 4 and 6 shots a beat as
  Double-Time grows (Hyper 9 and 12). Double-Time's second rank for the assist on the Choir: **0.06 -> 0.96** of an Overcharge
  stack. The reference itself got quicker (29.2 -> 26.5 s): the boss's held Drop starts with the Surge, which the assist now
  feels too.
- **Seeker on a boss** (`steerShots`): a seeking shot bends only toward a part within 35 degrees of its heading
  (`kSeekerBossConeCos`), the weak point first (the part a hit multiplies most, then a full hit, then armour). Steering to the
  weak point alone was not enough (-0.61, as before): a shot turned across the Choir toward the singing Voice met a silent hood
  on the way. With the cone: assist **-0.61 -> 0.00**, careful **-0.38 -> +0.03** (no gain on a boss the aim already finds; no
  loss).
- **The Dash Mods removed** ("just remove the dash mods entirely and buff the base dash"): Afterimage, Reserve and Shockstep
  have a cap of 0, so they are never dealt (their numbers kept: the wire, the band's Mods and the icons are indexed by them);
  the decoy is gone (`chaseTarget`, its fields, its picture and sound). **The base Dash**: two charges (Reserve's) and a shock
  ring where every Dash ends, 140 wu, 3 damage, x 2 on a Perfect Dash (Shockstep's first rank; `kDashCharges`, `kDashRingWu`,
  `kDashRingDamage` in `sim_types.h`; `kFxDashRing`). Test `dash`: two Dashes in half a second, the third refused, the ring's
  kill.
- **Regen** ("buff the regen upgrade, like remove the lowest rank"): a cell every 8 bars, then every 4; cap 2 (16, 8, 4 before).
- Not measured: what the stronger Dash and Regen do to the bots' runs (the novice dashes out of half the hits it sees; the
  child-like bot never dashes, so only Regen reaches it).

## 2026-11-15, the upgrades for a young player (the owner: "lets look at the upgrades next")

`--sim picks --policy assist` (new): the no-trap table for a player who lets the auto-aim fire (the careful bot's feet). Both
tables on 8 seeds; the boss figure (seconds to kill the Choir alone, against an Overcharge stack's gain) is the one to read: the
crowd figure is noise for the assist (a stack of Split measured 1,488 to 1,169 kills a minute) and good to +-0.5 at best for the
careful bot.

| On the Choir (gain / Overcharge's) | Careful (aims) | Assist (auto-aim) |
|---|---|---|
| Reference, Split 1 + Double-Time 1 | 22.2 s | 29.2 s |
| Split | 1.00 | 0.99 |
| Double-Time (its 2nd rank) | 0.57 | **0.06** |
| On-Beat Crit, Crescendo, Heavy Step | 0.49, 0.59, 0.85 | 0.73, 0.77, 0.65 |
| Seeker | **-0.35** | **-0.42** |
| Orbitals | **-0.49** | **-0.84** |
| Afterparty (the Surge, so not in this fight) | 0 (crowd 1.33) | 0.03 |
| Downbeat Cannon | 1.45 | 1.42 |

- **Double-Time's second rank gives the auto-aim nothing.** The assist fires every other step of the grid and always on the
  off-beat's (H3), so 1/16 is 2 shots a beat, 1/16 triplet 4 and 1/32 also 4. The same flat top takes the Drop's second Surge
  gear and Afterparty from an auto-aim player. Rule in three places: `World::spawnShots`, `Band::computeShots`, the engine's lead.
- **Seeker steers into armour on a boss**: `steerShots` curves a shot to the nearest part that can be hurt at all, usually a
  silent hood or a closed pillar (a quarter), not the weak point the shot was aimed at.
- **Orbitals** lengthen the Choir for both bots; not traced (no damage path slows a boss: likely the bots' feet, the ring taking
  bullets off them).
- **Not offered by the numbers**: Afterimage, Reserve and Shockstep do nothing for a player who never dashes (the child-like
  bot); Nova Stock and Last Light are worth more than they look with Auto Nova on (every stocked Nova is a saved last cell);
  Crescendo resets on every hit, so a young player rarely has it.

## 2026-11-15, the Hive's darts on co-op clients

The default seed's clean loopback soak had failed by a hair (on-screen enemy error 12.10 wu RMS against 12), nearly all of it the
Hive's darts on the clients (376 wu RMS). Two causes, both fixed (`world_miniboss.cpp`, `world_net.cpp`):

- After the half a dart's hive was its rank among the live darts (alternately), so one death a client had not heard of yet, or had
  predicted, moved every later dart into the other hive, hundreds of wu away, for the whole bar; on the host too, a kill sent half
  the cloud across to the other hive at the next bar line. Now a dart's hive is its spawn number's parity (`World::hiveOf`): the
  same on every device, for life. Each batch hatches its spawn numbers in a row, so the two hives still split it evenly. Within a
  hive the slots are still ranked by spawn number among the live darts, so a death not yet heard moves the later darts of that
  hive by one slot (a few wu) until the host's correction.
- A dart the host spawned reached a client with slot 0 (spawnEnemy's), so it flew to the first slot of hive 0's shape until the
  next bar line, where the host's flies to its Queen (slot -1, `hiveHatch`); a client's spawn of a Hive dart now has -1 too.

| Clean loopback soak, 4 players, 10 min | Before | After |
|---|---|---|
| Default seed: on-screen enemy error | 12.10 wu (fail) | **3.61** |
| Its Hive's darts (the called bodies) | 376 wu | **5.67** |
| Seeds 2, 3, 4 (other runs: the change moves the host's fight too) | 2.1, 3.1, 4.7 | 3.16, 6.06, 3.10 (darts 6.7, 6.4, 8.3) |

Seed 3's 6.06 is knocked-back bodies (78% of its error²), not the darts. `scripts\test.ps1` passes (its clean 6-minute soak 1.73 wu,
the impaired one 4.59).

## 2026-11-15, balance for young players (2): fewer enemies in Tracks 3-5, slower Caroms

The owner: "tone down the number of enemies 10% in tracks 3-5 and slower Caroms". `tuning::kTrackEnemyScale` (0.9 for the
set list's Tracks 3-5, in `baseBudget` and `crowdTarget`: the threat budget, the crowd target, and so the Drop's mega-wave and
the elites the budget buys; the loop as written); Caroms (species.json) speed 180 to 150 and the bounces' cap x 2.0 to x 1.5
(their top speed at tier 5 from 504 to 315 wu/s). The child-like bot over 20 runs of 60 minutes:

| | Before today | An Extra Life a Track, three bosses x 0.9 | And these |
|---|---|---|---|
| The first run finished the ten Tracks | 3 of 20 | 2 of 20 | **9 of 20** |
| First death (median) | 6.0 min | 5.8 min | **17.2 min** |
| First deaths before minute 10 | 12 of 20 | 14 of 20 | 9 of 20 |

The default seed's clean loopback soak (`test.ps1`) now fails by a hair: on-screen enemy error RMS 12.10 wu against its limit of
12, nearly all of it the Hive's darts on the clients (its "called bodies": RMS 376 wu over 96 samples; Shoals 1-2 s after their
last correction 1,116 wu). It is not this change: this run's timing now meets the Hive at a bad moment (seeds 2, 3 and 4 pass,
at 2.1, 3.1 and 4.7 wu, two of them with the Hive). The cause: `World::hiveReslot` runs on every device at each bar line and
ranks the live darts by spawn number; a dart's slot (`e.a`, `e.b`) comes with the host's corrections, but the count each hive's
spacing uses (`MiniState::slots`) is each device's own, so when a client's live darts differ from the host's at the bar line (a
kill predicted here, or one the host has not told yet) every dart of that hive stands somewhere else for the whole bar. Fixed the
same day (the entry above).

## 2026-11-15, balance for young players: an Extra Life every Track; Cadence, Fata Morgana and Event Horizon x 0.9

The owner: "This game should be 'easy' and beatable most of the time. (playable by 5-6 year olds)". Measured over 20 runs of
60 minutes (`--sim crowd --seeds 20 --minutes 60`), before the changes:

| Bot | Finished the ten Tracks on the first run | First death (median) | Deaths an hour | First meetings lost |
|---|---|---|---|---|
| Novice (a first week: assist only, 0.25 s late, dashes half the time) | 10 of 20 | 20.1 min | 3.0 | Feedback 4, Event Horizon 2 |
| Reckless (aims perfectly, never dodges) | 12 of 20 | 21.0 min | 2.5 | Fata Morgana 8 |
| Child-like (the novice with 0.5 s reactions and no Dash; a temporary build) | 3 of 20 | 6.0 min | 6.0 | Cadence 5 of 18, Fata Morgana 4 of 15, Horizon 3 |

The child-like bot's hits come from bodies (Caroms 1,207, Shoals 714, Drifters 675), not the bosses; Tracks 1-3's bosses were won
at every first meeting by every bot. The owner chose: an Extra Life every Track (`World::extraLifeDrop`: Track 1 from a kill in
its first Drop, from Track 2 on from the mini-boss; it was Tracks 1, 3, 5, 7 and 9) and the three bosses' health x 0.9
(`kHorizonHealthTune`, `kCadenceHealthTune`, `kMorganaHealthTune`). The child-like bot after them: deaths an hour 5.0 (6.0),
Extra Lives taken 238 (187), Cadence lost 2 of 17 (5 of 18), Fata Morgana 2 of 16 (4 of 15), Event Horizon 5 of 20 (3; its
fights 64 s, were 86), the first run finished 2 of 20 (3) with the first death at a median 5.8 min (6.0): a run's early deaths,
to bodies in Tracks 2-3, are what these do not reach (20 runs move a count by about 2). Proposed and not chosen yet: CONTINUE
from the Track reached after a game over; fewer hits from bodies in Tracks 1-4 (slower Caroms, a longer safe time after a hit,
5-6 starting cells).

## 2026-11-15, four-phone co-op runs (the owner: "please do several co-op runs to find any issues")

`scripts\coop-phones.ps1` (new): the first serial hosts a room (`--coop host --coop-start N --bot`), the others join by address,
room code or the list of rooms heard (`--bot`); a lock per device, the foreground checked, our APK deployed first; optional HOME
and back on one device, a force-stop of one app, and per-seat arguments (`--coop-pause-at`, `--coop-quit-at`, `--coop-down-at`);
every device's log read every 30 s (the OnePlus's log buffer rolled over within a run) and scanned (`captures\coop4\<run>\`).
Devices on the PC's hotspot (192.168.137.x): the Moto `ZY22BXMJG3` and a second Moto `ZY22BZ8BDZ` (both on wireless adb), the
Galaxy Tab `R5GL362AKGK` (SM-X230, Android 16) and the OnePlus 12R `a0e622a5` (its GitHub build replaced by the dev build, as the
owner allowed). Four runs of 3.5-4 minutes, four seats, every seat's bot playing, commit 3a1e49a's dev APK:

| Run | Host | Join | Also | Result |
|---|---|---|---|---|
| 1 | Moto | address | — | to Track 3; Kaleidos, Alpha Hound, Longwave fell at the same run second on all four |
| 2 | OnePlus | the room list | the Tab away 11 s (HOME, back) | each joiner heard the room in 1.5 s; the room paused for all and resumed |
| 3 | Tab | room code | the host's shared pause at 60 s; a seat leaves at 120 s | the pause held (4.8 s of wall time over the run time); "seat 1 left the run" on all three, the run went on |
| 4 | Moto 2 | address | a ship downed at 60 s; the OnePlus's app killed at 103 s; the host quits at 160 s | revived (alive 1011 then 0111); the killed seat dropped 4.5 s later; the clients ended "the host is gone" 1.5 s after the host left |

No crash, no census mismatch (0 of 48-118 on every client), no unknown ids or late spawns, kills confirmed (none undone), rtt
22-37 ms, loss 0-0.6%, the band within 0.2-0.8 ms RMS of the room's song (1-2 re-seeks), on-screen corrections 0.2-4.5 wu RMS.
Noted, not changed:
- **The host's load in a four-seat Track 2**: 1,000-1,200 enemies alive (the co-op budget, x 2.8 at four players, as GAME_DESIGN
  asks); on the Moto host 2.1% of frames ran two or more ticks and the longest frame was 41-50 ms (the OnePlus host 43 ms). The
  Moto host sends up to 720 kbit/s (about 2,800 corrections a second with 456 enemies), each client takes about 240 kbit/s.
- One audio underrun on a Moto client at its run's start (its buffer raised to 720 frames).
- The OnePlus's panel ran at 60 Hz: the game voted 120 Hz and a system or user setting overruled it.
- The shared pause (`--coop-pause-at`, `coopPause`) writes no log line; the run time against the wall time shows it.

## 2026-11-15, the updater: an UPDATE pill on the title when GitHub has a newer build

The owner: "a small button in the main menu that shows if an update is available, and tapping it goes to the github".
docs/RELEASE_PIPELINE.md "The updater" has the whole of it: GitHub builds carry their build number (the commit count, as their
tag), ask GitHub's API for the latest release once a launch (Java, a thread of its own; a local build never asks), and the title
shows a gold `UPDATE: BUILD <n>` pill over the note while that is newer; tapping it opens the release page (`SDL_OpenURL`). The
version line shows the build number. Picture `screen_title_update` (`captures\updater\`). Checked: the desktop build and the
picture; the Android dev build compiles with both JNI functions exported; GitHub's API answers `tag_name` `build-288` without an
account. Not checked on a phone (the Moto had just been given a build, perhaps by the owner, and a second device was unknown):
the first GitHub build with the updater shows the pill once a newer one is published.

## 2026-11-15, Singularity holds what it catches; Kaleidos and Polyrhythm easier

- **Singularity** (the owner: "i'm wondering if it fails to suck in some enemies"). The well's pull (260 wu/s) was added to each
  body's own motion, so whatever moved hard on its own got out. Measured with the new `--probe-singularity L` (the crowd tool:
  player 0 carries the Riff at level L; every well's catch followed to the tick before it blows, by family), level 3, 6 runs of
  30 minutes with `--god`: of the bodies still alive at the blow, within 80 wu of the centre were Drifters 98%, Shoals 88%,
  Hounds 82%, but Caroms 65%, Moths 60% (20% ended farther than they began: they home to their place round the player), Rams
  45%, Blinks 0 of 2. Now `pullBodies` (Singularity, Black Hole, Collapse Field) undoes a caught body's own motion of the tick,
  once a tick whatever the wells (`World::held_`), and the pull alone moves it: every family that lives to the blow ends within
  80 wu 89-100% of the time, none farther. A Blink's hop is still a jump no well holds. `testSingularityHolds`: a tier-3 Carom
  heading away (216 wu/s) ends within 40 wu of the centre. The same run's one Shard-pool overflow (114, deep in the loop, god mode)
  is the review's known low finding.
- **Kaleidos** x 0.8 health (the owner: "lower the health of the first track boss by 20%"), `kKaleidosHealthTune`.
- **Polyrhythm** x 0.5 health (the owner: "Polyrhythm needs 50% less health"), `kPolyHealthTune`.

## 2026-11-15, Longwave's death shatters what is left of it

The owner: "i think it dies without destroying all its tail". Measured with the novice bot (`--sim crowd --seeds 8 --minutes 12
--policy novice --trace`, the `[boss]` lines' last `parts` before each death): of 9 fights, 8 ended with segments alive (23,
21, 17, 12, 6, 5, 3 and 1 of 24); two ended 11 s in. That is rule 5 ("kill the head and it is over"), with the assist aiming at
the open jaw. The death the page describes ("the rest of the body unravels tail-first, one segment per sixteenth, each a medium
kill; then the head as a boss break ... then the Shards") was built only as segments taken off the drawing, silently, with the
boss's fall played at the kill. The owner chose to keep the head kill and shatter the leftovers.

- **Sim** (`World::longwaveUnravel`, `longwaveUnravelFirst` in `sim_types.h`): the dying boss emits `BossCue::LwShatter` for each
  segment left, tail first, on each sixteenth line from the first one after the death (b: its rank, f: its heading, where it
  froze), then `LwBreak` for the head on the next line. The Shards are paid then (`payoutTick`), the boss stays a quarter bar
  more, and the Track ends on the first bar line after the break (at least half a bar after the kill, as before).
- **View** (`play_view_m5.cpp`): the kill is the jaw locked open with the clamped white flash (0.9, 0.12 s); each `LwShatter` is
  a medium kill (`killRecipe`) in its place's colour along the body; `LwBreak` is the boss break (`fightBreak`). The drawing
  takes segments off on the same lines. The kill's own sound, gong, haptic pulse and 50 ms hit-stop stay at the kill.
- **Sound** (`pending_sounds.cpp`): `LwShatter` the medium kill (0.7), `LwBreak` the large kill (1.0).
- **Checks:** `testLongwaveUnravel` (15 segments left: 15 shatters in order on consecutive sixteenths where they froze, one
  break, the payout at the break, the Track's end after it). The picture `m5_lw_unravel` (a strip from the kill, a frame a beat;
  `review-m5.ps1 -Part part1`): `captures\lwfix\lw_unravel_sheet.png`. The ship holds fire there and keeps near the body, so all
  24 segments are left.

## 2026-11-15, the review's batch 2 dropped

The owner: "No need batch 2". These are not built, and their specs in `design/screens.md`, `design/arena.md` and
`docs/GAME_DESIGN.md` (Settings) are marked so:

- the structures' first-run hints and their names over them;
- CONTROLS' stick size and position, button size, the touch layout editor and gamepad mapping;
- the SCORES screen with its co-op tables and lifetime stats;
- the game-over medley.

Left as found: with the maximum loadout, a Trio orb's words can run off the screen's right edge (batch 1's
`captures\fix1\hud\loadout_max_2400x1080.png`, CONFETTI).

## 2026-11-15, the release pipeline on GitHub (docs/RELEASE_PIPELINE.md)

The owner asked for "a GitHub repo and a release pipeline that automatically builds a release apk available for download".
The repository is `rockenrooster/raveform` (`origin`; created private, public the same day at the owner's "Make it public",
after a scan of the whole history found no key, token, password or keystore), and `.github/workflows/release.yml` builds the
release variant on every push to `main` and publishes it as release `build-<commit count>` with `raveform.apk`, its SHA-256
and the symbols. Anyone can download the APK without a GitHub account (checked: the latest-download link answers 200).

- **The first build (`build-284`, ad6c6fe):** 3 min 39 s for the whole job, 2 min 56 s of it the Gradle build (NDK and CMake
  from `sdkmanager` 11 s, the sources 6 s). The APK is 7.19 MB (libmain.so 3.07 MB), the symbols zip 14.1 MB. Builds cost nothing on
  a public repository (while private: about 500 a month in GitHub's free 2,000 minutes).
- **What Linux needed:** `scripts/fetch-deps.sh` runs there too. There is no Vulkan SDK, so the headers come from Khronos's
  Vulkan-Headers at the SDK's tag, and `vk_enum_string_helper.h`, which the SDK takes from Vulkan-Utility-Libraries, comes from
  that repository's tag. Both commits are checked. They are the SDK's files: `vk_enum_string_helper.h` has the same blob,
  and `vulkan_core.h` is the same after the SDK's CRLF become LF. No `#include` or CMake path differs in case from its file,
  which a case-sensitive file system would refuse (checked with a script).
- **Signing:** `android/app/build.gradle` signs the release with the keystore `RF_KEYSTORE` names (the workflow decodes it
  from the `RF_KEYSTORE_BASE64` secret, with three more secrets), else the debug key as before. No secrets are set, so the
  first builds carry a debug key made by their run. Which key to use is the owner's decision.
- **Not verified:** the APK on a phone. It cannot install over the phones' builds, which have this PC's debug key, without an
  uninstall that would take their settings and scores.

## 2026-11-15, the review's fixes (batch 1)

The owner chose fixes from `docs/REVIEW_2026-11-15.md`; the lead's brief, item by item.

- **Screens fit every shape** (H2, M11 for the Trio). One layout scale, `layoutScale(W, H) = min(H / 1080, W / 2222)`
  (`play_hud.h`), wherever a layout scale is made: `Ui::begin`, `drawHud`, `drawRunHud`, `pauseButtonCentre`,
  `TouchControls::layout`, the HUD keep-out (`Play::render`), `AudioScene::layout` (the sync screen) and the pictures that tap
  PLAY or scroll the credits. 2400x1080 is unchanged (W / 2222 > 1 there). Top-anchored rows stay at the top and bottom-anchored
  ones at the bottom, so a 4:3 screen has room between them. Then what still collided: a long room name in the join list is cut
  short with `...` 36 px before the seat count; the game over's score (solo, and the co-op team's) scales down to fit inside the
  margins (with room for NEW BEST's star), by the final score's width so it does not shrink as it counts; the Nova stock's pitch
  closes up, and its stars shrink once they would touch, so seven never meet `DROP IN n` (`novaStarPitch`, also the chevrons'
  keep-out); the touch controls are not drawn under the pause; CONTROLS' `EVERY HINT WILL SHOW AGAIN` sits 24 px under RESET
  HINTS (it was 12: the layout rule's only warning, at every shape). Checked in Debug at 2400x1080, 1920x1200, 1920x1080 and
  1600x1200 (`captures\fix1\screens_<shape>`, `coop_<shape>`, `sync`): notice, title, SOUND, CONTROLS, LOOK, credits, pause,
  game over and ENDED BY, the HUD with the touch controls at 100% and 130%, the co-op notice, join (empty, list, code, address),
  lobby (2 and 4 seats), the co-op pause, game over (host and client), the co-op HUD and the sync screen: nothing off screen or
  overlapping, no `ui:` layout warning, 0 validation messages in 92 Debug runs. `review-m3b.ps1` and `review-m4b.ps1` take
  `-Res` (m3b now prints `ui:` layout warnings too); new
  pictures `m6_uiscale_100_full`, `m6_uiscale_130_full` (seven Novas, two Extra Lives, the touch controls).
- **Syncopation and Rear Guard on the assist** (H3). The assist's half-rate grid never skips the off-beat's step (tick % 96 ==
  48), in the sim (`World::spawnShots`), the band (`Band::computeShots`) and the engine's lead: one rule. At Double-Time 1 (the
  16-tick grid) the assist fires 16 shots a bar, 4 of them on the off-beat (a new check in the sim's lead rule test); grids 24
  and 12 and Hyper's already had the off-beat on an even step.
- **Nova against bosses** (H4): every Nova's blast (pressed, Auto Nova's, Black Hole's, the Extra Life's: all reach
  `World::bossNova`) is x 0.33 on a boss and x 0.66 on a mini-boss (`kBossNovaScale`, `kMiniNovaScale`, `tuning.h`). Enemies and
  the arena's bodies take it whole.
- **Feedback and Novas** (H5). (a) Only Novas the player pressed go on the tape: `PlayerState::novaPressed` (`pressNova`'s new
  `pressed`; false for Auto Nova's, an Extra Life's and Black Hole's; Last Light was never a Nova event), carried by the NovaPress
  and NovaBlast events (`b`) and to teammates by the Nova player event (`MsgPlayerEvent::b`, which the parser now accepts as 0-1:
  it refused any `b` on a Nova, so the first build of this dropped every client's Nova: 333 census mismatches in the clean soak).
  (b) Each Null is announced a beat before it grows: its place fixed then (where the Echo stands), a dashed see-through tell
  closing on it from 360 wu to the Echo's size over the beat (`telegraphLine`), and the telegraph tick (FbNull with f = 1; the
  burst stays on the growth). (c) A recorded Nova is a small four-point star on the tape at its step, in the player's colour with
  a white core, until it comes back. Pictures `m5_fb_tape`, `m5_fb_null`, `m5_fb_null_strip` (the ship presses a Nova every five
  seconds of the fight).
- **Co-op SYNC** (H6): drawn disabled in co-op, opening nothing; solo unchanged. While the shared count-in runs, a device in
  SETTINGS (saved, as DONE does) or CREDITS opened from the pause is brought back to the pause screen, so it sees the 3-2-1.
- **The arena's bodies' health** (H7): `World::trackScale` is now the Track's mini-boss health as the game gives it: the page's
  number (Track 1: 300) x the mean `healthFactor x kMiniHealthTune` over the mini-bosses that Track can bring
  (`World::trackMiniFactor`, pickMini's lists: Track 8 counts Deep Well and both of Track 7's pair; Tracks 9, 10 and the loop all
  twelve; Track 1 takes Track 2's). Measured with `--sim crowd --seeds 10 --minutes 20 --policy assist` (the crowd tool now prints
  each kind's broken of placed and its mean health). Before: Vault 9 of 67 broken (13%, mean health 1,374), Rift 21 of 109
  (19%, 1,219), Vault Drop 0 of 21 (0%, 2,221), Comet 1 of 40 (2%, 512), carriers 4 of 69 (6%, 1,251). After: Vault 22 of 60
  (37%, 292), Rift 31 of 100 (31%, 269), Vault Drop 3 of 22 (14%, 470), Comet 5 of 40 (12%, 118), carriers 4 of 54 (7%, 333).
  (Both builds already had the assist, Nova and shelter fixes.) Both runs exit 1 only for Shards dropped by the full Shard pool:
  3 before, 24 after (two seeds; more bodies broken, more Shards), the review's low finding, not touched here.
- **Shelter** (H8): `World::hitPlayer` turns every hit away while `sheltered(p)`, so the meteor, the Storm Cell, a Volatile
  blast and any later source respect it; `bossHitPlayer` checks it before counting a boss hit, and `haloBlocks` spends no Halo
  hit on a sheltered ship.
- **Cadence's turn** (H9): announced a bar ahead (`CadenceState::turnWarn`), on the bar line before the song comes back to I:
  CdTurn is sent then for the turn's bar line (its reversed cymbal now four beats long, `pending_sounds.cpp`: it swells over the
  warning bar and ends on the turn), and the view draws each pillar's quarter-turn sweep as a dashed see-through arc over that
  bar, rising over its last beat. An announced turn happens even if the phase changes within that bar; none is unannounced.
  Pictures `m5_cd_turn`, `m5_cd_turn_strip`.
- **Haptics** (M45): medium on this device's player losing a cell and on a Perfect Dash; heavy on its Nova (pressed or Auto
  Nova's; a free one has its own moment) and on its death (solo) or going down (co-op). `Play::pulse` never fires for a picture,
  a benchmark (the run alone) or the title's demo (a control tapped on the title still does: `pulse(kind, ui)`), and at most once
  in 80 ms, a heavier one replacing a lighter one within that time.
- **Co-op, your ship going down** (M46): the hit sound, the PlayerHit cue (Crescendo falls back) and the heavy pulse; a
  teammate's going down is the hit sound at 0.45, panned by where it happened, on the next sixteenth.
- **The boss hint line** (M16): the boss's name and its hint line in the boss bar at 14 px (were 11), each on the priority
  layer's dark rim (`threatRims`, 2 px past the stroke) as the first-run hints' world text is.
- **The HUD shows the build** (M20): every Fusion taken heads the Mod row, its icon in a gold frame; every Mod held is shown:
  when one row cannot hold them it wraps onto rows above (up to four), every icon at 0.85 then, and the Boosts' labels and the
  Trio hint above them move up with the extra rows; a Mod's pips show its cap (held filled, the rest empty outlines). With
  `--loadout max` (3 Fusions, 36 Mods): two full rows and one icon more at 2400x1080, three rows at 1920x1200.
- **The Trio's top orb's words** (M11): a top orb whose block would reach into the HUD's top band (`HudKeepOut::band`: the
  Track bar and the boss bar, 150 px at 1080) has its words beside it, on its right, as the side orbs.
- **The recipe line** (M26): shown only when the other half is held or can still be taken (`Loadout::canTake`: a Riff not held,
  with four Riffs held, cannot).
- **The 3-2-1 ticks**: the telegraph tick on each of the three rings' beats as the band hears them (solo: sent by
  `beginCountIn` on the pause screen; co-op: `Play::countInFrame` once the shared count-in's resume is known, on the band's
  timeline), and the pause's dip and low-pass lift over the count-in's first beat (`countInLift`, `applyPauseMix`;
  `Band::setMuffle` takes a ramp), so the player counts in with the music.
- Desktop Debug and dev build; `scripts\test.ps1` passes (audio 2,724 checks, sim 6,120, net 131,368, save 60; determinism;
  the four loopback soaks, 0 census mismatches); `scripts\sanitize.ps1` (tests, the 3-minute bot run, the fights in Tracks
  4, 7 and 10, the 3-minute net soak): no report. Not heard (PC runs are silent) and not tried on a phone or the tablet:
  the 3-2-1 ticks and the lift, the Null and turn sounds, the haptics.

## 2026-11-15, Fusions shown; a Nova a Shard level; the count-in 3-2-1

The owner, on the Fusion findings below: "They come too late. Its fine, no change needed" (the pace stays); "The game never
shows which upgrades combine. fix please"; "A Fusion's orb looks like every other orb. fix please"; "the resume countdown
should be 3-2-1 instead of 4-3-2-1"; "leveling up via shards should also give a nova".

- **The Fusion's gold orb** (screens.md "The Trio", designed in M3, never built: it was dealt as one more orb evenly round the
  ring): it stands above the ring at 240 wu (`kTrioFusionWu`; screens.md's 230 put its thin ring over the Trio's), its icon
  and its thin ring x1.45 (`kTrioFusionScale`; `trioOrbScale` gives the sim's hold and shelter rings and the view the same
  size), a four-arm gold flare and a two-arm white one swelling on the beat, a gold pool, its ring in gold, two gold lines
  up to it from the ring, `FUSION: SPLIT + WAVE` over its name and words in gold to its right. The other orbs turn point down
  (`trioOrbPlace(..., fusionLast)`: lower centre, upper right, upper left; Lucky Break's spread over the ring's lower 240
  degrees, at most 120 apart). The sim and every co-op peer place them alike (`placeTrioOrbs`, also on a Trio heard from
  another device); with a Fusion the Trio's centre keeps the gold orb inside the arena.
- **Recipes:** an orb that is half of a Fusion not yet taken shows `+ WAVE 1/2 = HELIX ARRAY` in gold under its pips (the
  other half, its rank of its max, the Fusion). Words now sit by where an orb stands (above a top orb, beside the others, a
  bottom orb's to its right, every orb's beside with a Fusion on offer), so any layout reads. Pictures: `--picture
  trio_fusion` (Split 4 + Wave 2 held, Echo 1: Accent, Orbitals, Magnet and Helix Array's orb) and `trio_fusion6` (six orbs).
- **A Nova a Shard level** (`World::collectShard`): one at once, up to the capacity; none in the loop, as the Breaks
  (`kLoopBreakRefills`). The LevelUp event says so (`c`); the HUD's newest Nova star swells and flashes gold for 0.9 s (half
  in reduce flashing) and the log line adds `+1 Nova`.
- **The count-in 3-2-1** (`kCountInBeats` 3, solo and the shared co-op one: `Play::beginCountIn`, `Session::hostPause`, both
  pause screens' rings).
- **Found on the way:** the co-op Stats message refused more than 6 Novas, but Nova Stock's three ranks hold 7: a player
  holding seven had every Stats packet dropped (teammates' cells, Novas, Chain and score froze on the others' screens). The
  check is `kNovaCapacityMax` (7) now. GAME_DESIGN 5 said "up to 6 with Mods": 7.

## 2026-11-15, Kick Pulse two ranks; why no Fusion reached the owner's play

- "Limit kick pulse to 2": **cap 2** (`kMods`; it was 3). The first rank shoves, the second hurts (`THE SHOVE HURTS`); the third
  rank's wider ring (240 wu) is gone (`kKickRingWu` 160 / 160). GAME_DESIGN 6.2 row 35, screens.md's words. GAME_DESIGN 6.1's
  limits line also says three Boosts at once now (6.4 and `kBoostSlots` since 2026-11-14; the line still said two).
- "I don't think I ever got any [Fusion] in my playtesting": measured with `--sim crowd --seeds 16 --minutes 40 --god` (hits
  cost nothing, so every run goes the distance); the crowd tool now also prints the first run's first Fusion (when it came
  due, when it was taken, the upgrades taken by then, which one). The careful picker (`--policy assist`: the careful bot's
  movement and picks, the assist's fire): first Fusion due in 15 of 16 runs, median minute 17.9, 33.4 upgrades in (Tracks
  3-11, most in 6-10); a Fusion held as Track 5 ends in 4 of 16 (M5 part 3's "in every run that got there" slipped as the
  Tracks shortened). The novice (`--policy novice`: the first orb half the time, else any, as a player who does not know the
  recipes): due in 12 of 16, median minute 18.8, 35.2 upgrades in; held as Track 5 ends in 1 of 16. Upgrades taken by minute
  10: 23.5 (careful), 18.9 (novice). And nothing shows a recipe, and a due Fusion's orb is drawn as one more orb evenly round
  the ring like the others: screens.md's Fusion on offer (above the ring at 230 wu, its icon x1.45, the gold flares, gold words
  and lines) was never built. Both runs exit 1 only for pool overflows deep in the loop (god mode, Tracks 17 and 19: 58
  Shards, 196 bullets dropped), past any run a person plays.
- sim, net, audio and save tests, determinism and the four soaks pass.

## 2026-11-15, the audit: critical and high-priority defects

"Do an audit of the project and look for anything critical and high priority." Five read-only reviews (audio, network,
simulation, platform/rendering/Android, game layer and saves), each finding then checked against the code, and the sanitizer
suite (ASan + UBSan: the unit tests, determinism, a 3-minute bot run, fights in Tracks 4, 7 and 10, the 3-minute net soak, and
the three away cases): no sanitizer report anywhere. Fixed (all confirmed by reading; the first two also on the Moto):

- **Critical: a pause in a run's first two beats could never be left.** The drop-in's arrival ends on the run's clock, which a
  solo pause holds, and while it lasts the screens take no input: PAUSED stayed up for good (the pause ring is live while the HUD
  fades in). `pauseRun` now ends the arrival. On the Moto: a pause at 0:00, then RESUME, and the run went on.
- **A solo run did not pause when the app left the screen** (a call, home): it went on the moment the app came back. It pauses
  now (`appBackground`). On the Moto: home at 0:16, back to PAUSED.
- **CREDITS opened from the pause's settings unfroze the run**: it played on behind the licences, and a death there left a dead
  ship with no game over (no way out but killing the app). From the title, the demo stopped. (`underPauseMenu`, `attract`.)
- **Audio on a route not at 48 kHz**: the stream was opened at the device's rate, and the band writes silence at any rate but
  48 kHz, which also stops its heard clock and so the game (a USB DAC, Bluetooth on the legacy path, or a route change mid-run).
  48 kHz is asked for now; Oboe resamples. **A failed first open was never retried**; it is, every half second.
- **The warp swells, the enemy-shot blips and the Nova's blast** went to the band in run ticks without the song offset: after any
  solo pause, and in co-op, they sounded early, or late and (the warp's) silent, or queued minutes ahead and pushed other
  sounds out (after the sync screen).
- **Co-op: game over while a pause was coming trapped the host** (the run held on the pause's tick, RETRY's drop-in waited for
  good, HOME disabled). The host lifts the pause behind the game over.
- **Co-op: a client's Rifts never opened** (only the host set the open tick and nothing told the clients): a client's shots went
  through an open Rift. Clients open them by the same phrase rule.
- **Pause-on-leave hardening**: the away notice's backup copies leave in datagrams of their own (one lost datagram took both),
  the host's notice has copies too; an away player heard again (pings, heartbeats) restarts the 60 s, so a return near the end
  of the wait no longer loses the room; leaving a room mid re-align no longer silences the next room's song for 12 s.
- **A lost GPU device or a failed submit froze the game for good** (the submit's fence never signalled; a lost device spun on a
  frozen screen): the app now closes instead.
- **Cadence's death bursts and the Arpeggiator's death run never played** (the dying boss's update returned before them).
- Held keys stuck into the next run (keyboard); a gamepad's A on the game over restarted the world behind it (A / Start are now
  the screens' main control, B is back, Start pauses); an arena operation's event kind is bounds-checked.

**Open, not fixed (recommended next):** Windows saves fail under a non-ASCII profile path (`fopen`/`MoveFileExA` read UTF-8 as
ANSI; desktop only); the band's command queue drops commands once full while the engine is not rendering and nothing re-sends
the state (rare now that the stream re-opens); three rare Android/Vulkan paths (a render-target rebuild that fails half-way under
memory pressure; the surface hand-off's narrow race on a fast home-after-resume; a dead surface after the 2 s release timeout
during first-launch init); the release build is still signed with the debug key (Play will refuse it).

## 2026-11-14 (late night), pause-on-leave; the music in step between phones

"Yes build it" (pause-on-leave) and "make sure that the music stays in sync between devices since it is a music focused game".

**Pause-on-leave** (net/session.cpp `goAway`, `comeBack`, `hostReturned`, `hostSetAway`; protocol `0x5246050B`: `kAwayAsk`,
`kBackAsk`, `kAwayMask`, `MsgPause::away`). When a player's app leaves the screen (Android: home, a call, the lock; App's
background event, on the main thread before the pump blocks) the session says so at once (a client: reliable and a second
unreliable copy, since nothing lost then is ever resent) and the host pauses the room on the next sixteenth. The room waits
60 s (`kAwayWaitMs`, counted by the session: ENet re-checks a silent peer only as its resends back off, so its own limit is set
past it). Every screen shows `WAITING FOR <NAME>` in place of RESUME; nobody can resume or retry meanwhile. Back, a device first
holds its run until the room's pause and (a client) a fresh song anchor have reached it, catches up to the pause tick in ticks
of its own (paused, the tick cap is 600 and no time is dropped), re-settles its clock on fresh samples (a client; the answer to
a ping sent before it left is not a sample, and no sample with a round trip over a second counts) or brings its band onto the
song the room kept (the host: as a client's servo does, then it publishes again whole bars off, `hostBandShift_`), and the room
counts in. Past the wait the host drops the seat and plays on; the returning device goes home with `AWAY TOO LONG: THE RUN WENT
ON WITHOUT YOU` (a host: `... THE ROOM CLOSED`); clients give up on a host away past it. The net clock is the boot clock on
Android (CLOCK_MONOTONIC stops while a locked phone sleeps). The loopback soak has `--away-at S --away-for D [--away-seat N]`;
`scripts\test.ps1` runs a client away 5 s and the host away 10 s (2 minutes each). Soaks: a client away 2, 5, 30, 45 s and the
host 10, 20 s all pass (the room held on one tick, played on 3.4-4.8 s after the return, nobody dropped, 0 census
mismatches); away 70 s: a client is dropped at 60 s and the room plays on; the host's clients give up and go home.

**The music between phones** (play_net.cpp `heardSongAt`, `kNetLeadUs`; BeatView::heardNs). The shared song was each
device's band as heard at its own display time (two refresh periods ahead), so a 120 Hz phone's music sat 17 ms off a 60 Hz
one's; now the host publishes, and every client steers to, the song as heard 33 ms after a frame begins, the same true moment
on every device (the 60 Hz phones as before), placed by the band's own heard time on the audio clock (read beside the net
clock), not the frame's. Measured with an outside clock (each phone's wall clock against this PC's over USB, `echo
$EPOCHREALTIME` in a persistent adb shell: round trips 0.6-1.8 ms, repeat measurements within 0.1 ms; each phone logs every
20 s the song its speaker plays at a moment of its wall clock, `net: music:`): the Moto hosting and the Tab joining play
**within 0.4-0.8 ms of each other**, before and after each away. The client's band against the room's song frame by frame:
0.24-0.34 ms RMS, 0.5-0.6 ms max. The phone runs of the away found three faults on the return, all fixed: a re-seek taken
before the reopened stream's timestamps came held the wrong length (a second re-seek followed: 341 then 803 ms; the servo now
waits a second); the host's re-align gave up at 8 s and then published its band without its whole-bar shift (the song jumped
six bars back for everyone; now 12 s, and the shift is always the servo's); and the re-align counted the servo's last error
while the servo was still waiting (it "settled" in 0.45 s on a stale 0 ms and the clients re-seeked 1 s; now only frames the
servo measured count).

## 2026-11-14 (late night), "multiplayer is a little buggy still": a crash in the audio thread

Co-op on the two phones here (the Moto hosting, the Galaxy Tab SM-X230 joining, both bots; our APK, the locks): the Moto's
process died at Track 3's second Drop, the moment the Choir came in, and the Tab's run ended with "THE HOST LEFT". The
tombstone: SIGSEGV (SEGV_ACCERR) on the AudioTrack thread in `Engine::renderBlock` (engine.cpp, a mono one-shot at rate 1:
`ldr q20, [x26]`, the sample's data). The cause, reproduced on the desktop: a recipe that *ends* on its tick (the reversed
cymbal, the swelling crackle) asked for after that tick has been rendered was started past its sample's end (`from` = the
sample's length plus the lateness), and the render's `frames - pos` wrapped (u32), so the voice read a block past the buffer:
garbage in the mix (a click or burst) or, where the buffer ends on a guard page, the crash. A cue at the sim's own tick is
always that late (the render runs ~5 ticks ahead of it): the arena's Rift closing at a cycle-2 Drop and a Rift destroyed
(`BossCue::RiClose`, "a reversed cymbal cut short, then glass") send one, in solo play as in co-op. Fixed in
`Engine::playSample` (a start at or past the end plays nothing) with a guard in the render; test `testLateEndingRecipes`
(audio suite: before the fix the late cymbal left energy 0.003 and the crackle 0.22 where silence was due).

"Make it audible" (the owner): as sent, the Rift's reversed cymbal could never be heard. Now (`pending_sounds.cpp`, the
`RiClose` pair) it swells from the close to the next eighth at least a sixteenth on (125-375 ms at 120 BPM) and the glass
breaks there. The fights sheet has it (`--render fights`, "Rift destroyed", posted 6 ticks late as the game hears it): the
effects bus swells for ~290 ms, 0.029 to 0.041 RMS, into the glass on the eighth. (A Cloaked elite's cymbal ends a beat after
its cue: heard as written.)

Also: the 20 s co-op log line `net: world:` (what each peer holds and draws, for the earlier "bullets on p2 and p3" report)
now has the clients' kill predictions (confirmed, undone), late spawns and unknown ids. On these phones the bullets drew on
both peers in every 20 s window; a 2.5 s trip to the background on the client came back with one visible correction (65 wu on
screen for that window), the session kept.

## 2026-11-14 (late night), the Reprise's implosion quick, a big shake and a flashbang

"Implosion needs to be quick and big camera shake on explosion", then "the explosion should also have a borderline flashbang
visual": the Reprise's payout (its Shards) now comes on the first beat line at least three quarters of a beat after its death
(`sim::repriseBurstTick`, 0.75 to 1.75 beats; until now the bar line at least half a bar on, up to a bar and a half), the
implosion running to it; the reversed cymbal is as long as the implosion (`BossCue::RpDeath` carries its length in f) and the
impact lands on that beat. The burst: a flashbang (the HUD's additive layer, white for two frames, then a 0.18 s fall; a longer whiteout tried after it was
undone: "Reprise death v2 was what i wanted") and a
big shake channel (`PlayView::bigShake_`, 26 ref px past the look's 6 px cap, ~0.45 s). Video `captures\video\reprise_death.mp4`
(the `m6_fx_rp_death` picture with a long strip: every frame from before the kill, the kill on a bar's third beat). "Is there a
stutter on the reprise death?": there was, the boss hit-stop (3 x 50 ms for the Reprise: the run held, then caught up three ticks a
frame) on top of the short implosion; the Reprise has no hit-stop now (no frozen frames in the video but the flashbang's two).

## 2026-11-14 (late night), the Vault's layers, a hit and its safe time, the Reprise's implosion

"Missing visual effects for breaking vault layers and vault death; visual effects when you are damaged are not visible enough or
missing, it's not clear when you are damaged and when you can be damaged again; Reprise needs an implosion before the explosion"
(look.md §5 "Feedback"):

- **Vault**: each quarter lost, its glyph's layer that goes flies apart stroke by stroke (`PlayView::shatterShape`, from
  `ArenaGlyph`'s own strokes; `vaultLayer`), a ring, sparks, a jolt; broken, all that stood flies apart (`vaultShatter`) under a
  burst of its own (`vaultBurst` on `BossCue::VaBreak`). The layers used to vanish without a sign (the view ignored `VaHit`).
- **A hit**: the danger ring, sparks, the hull white, the frame's edges flashing red, a harder jolt; the time the ship cannot be
  hurt (`invulS`, 2.5 s after a hit) shows as a draining bubble round it and a blink of the hull on the sixteenths, and its end
  as the bubble snapping shut. Until now nothing showed it.
- **Kaleidos Reprise**: the kill starts an implosion that runs to the bar line its Shards come on (the reversed cymbal's end:
  the BossDeath event's tick), where the explosion and the ten-colour shockwave now come (`repriseBurst`, `drawRepriseWave`,
  both from `drawFight`, so the wave outlives the boss's own drawing); its impact moved to that bar line too (play.cpp).
- Pictures `m6_fx_vault_layer`, `m6_fx_vault_break`, `m6_fx_hit`, `m6_fx_rp_death`; Debug validation 0 messages on the first three.

## 2026-11-14 (late night), the title's demo plays the whole game

"Can you make the demo AI actually get powerups? also it should never die so you can see all the tracks eventually", then
"instead of dying it gets a nova every time a death would happen": `Rules::demoRevive` (the title's demo only, set each frame
by Play): the hit that would end its run brings it back with two cells, a free Nova blast and a bar of invulnerability
(`World::demoRevive`). The bot's demo mode (`Bot::setDemo`): Boost orbs and Extra Lives pull from 1,100 wu at twice the pull,
Shards harder; `Bot::precise()` while it closes on a Trio's orb or a pickup, when the demo's eased stick answers in 0.06 s
instead of 0.35 (eased, it sailed past the orb and the hold emptied). Any bot now ignores a taken orb and a teammate's. Once
the demo has played the set it starts over from Track 1 on the next bar line. Measured with `--scenario play --fast-forward
2400` (the title, simulated): the set finished once, the second pass at Track 9 at minute 40, 0 deaths, 108 hits, 20 Novas, 40
upgrades, 26 Boosts and 2 Extra Lives taken in the second pass. The music and palette follow its Track behind the title.

## 2026-11-14 (late night), Title v2; more feedback on nine things; the Extra Life every other Track

"Things that need more visual feedback: on beat crit, crescendo, blackhole (powerup and upgrade), syncopation, stormguard, the
meteor crash, it just eases and disappears, it doesn't need to ease, its a meteor impact, the fire burn upgrade, extra life
powerup every other track (i'm not seeing one after the first one), event horizon deatth; also implement this (minus the title
reflection): design/title.md", with the answers to title.md §10: the ship under PLAY removed, the intro on every cold start but
much faster, the cut kept when leaving the app, the font's own letter spacing, the demo behind as now, the captions kept.

- **Feedback** (`src/game/play_view_feedback.cpp`; look.md §5 "Feedback"): **On-Beat Crit**: the shot's star 16 / 22 / 28 wu
  with a gold glow; its hit a gold star with a white core, a ring to the burst's 80 wu, a gold pool, sparks and a small lattice
  push. **Crescendo**: ten ticks round the ship lit gold to white as it climbs (the newest flaring), a ring and a warm glow at
  its cap pulsing on the beat, a hit shattering it; its owner's shots brighten and warm toward gold. **Syncopation**: an
  off-beat shot carries a ">" ahead of its head, a doubled tail and a brighter head (`Shots::accent`, set by `fireShot`; the
  simulation never reads it). **Burn**: a flame on each burning body drawn as line-art fire (a teardrop outline to a swaying tip,
  an inner flame, side flames from two stacks), embers and a warm glow; a burning death's spread bursts in fire. **Black Hole
  and Singularity**: the lattice falls into them (per-well reach, `LatticeWell`), an accretion disc of streaks on spirals, rings
  falling in, a white photon ring round a dark core, all quickening to the blast; placed, a ring folds in; blown, a white
  flash, debris of light, a ring, the lattice thrown out, a shake. **"Stormguard"** (no upgrade has that name: Storm Coil and
  Aegis both got it): Storm Coil's zaps (and Chain Storm's) held 140 ms, brighter and thicker, with a pooled glow along each
  and a star where each strikes (a first reach ring was dropped: "the ring for storm coil isn't needed, just a more visible
  lightning zap"); Aegis's arc glows, and a block (`kFxAegis`, never drawn before) flashes white where it struck and flares the arc.
  **The meteor** falls faster and faster (its speed tripling over the fall) and smashes: its rock in pieces, a white star, a
  crater rim glowing for a bar and more, a harder shake. **Event Horizon's death**: its burst never played (the boss went on the
  very tick it was due, before the frame that would have shown it: `PlayView::drawFight` plays it now); over the death bar light
  is torn in toward the point and the lattice falls in after it, the lattice goes dark through the beat of black with one white
  point, then two jets of light along its axis, rings in its hues, debris. Pictures `m6_fx_*` (`captures\fx\*.sheet.png`).
- **Extra Life** (GAME_DESIGN 5): every other Track (1, 3, 5, 7, 9), for a player holding fewer than two; from Track 3 the
  mini-boss drops it whether it dies or warps out (`World::miniGone`); two may be held (`tuning::kMaxExtraLives`), both shown
  on the HUD. The cause of "not seeing one after the first": an unspent one stopped every later drop.
- **Title v2** (screens.md "Title", "Transitions"; `src/game/title_v2.cpp`, `play_transitions.cpp`): the fan, the word at cap
  240 (the font's own advance) with depth toward the rule's centre and a pooled halo, the rule whose ends are the music and
  which carries the tagline, the motes, PLAY with its beat dots and rings (hover and pressed states; every pill has a hover
  state with a mouse now), the captions. The **intro** on every cold start (a bar and a beat: the point, the rule's throw, the
  word played a letter a sixteenth with eight lead notes, the controls; any input skips it); **home** builds the same way; the
  **drop** gathers the title into one point (the fan folds, the tubes run hot, the word falls into the rule, the rule draws in
  to the frame's centre with PLAY) and the downbeat bursts it (flash, eight rays, the ship). The quiet title leaves in the new
  order. Not built: the reflections (the owner), lights out (the cut kept), the optional falling-noise cue. The word's notes are
  `Recipe::SawGlide` held still (a saw pair through the lead's low-pass), not the player's own lead voice. Pictures
  `screen_title`, `m6_t2_intro`, `m6_t2_drop`, `m6_home`, `m6_title_idle` (`captures\t2\`).

## 2026-11-14 (night), the iOS port: written, not built

"And then your last task will be porting to IOS." Apple's compilers, SDK and signing run only on macOS, so the port was
written on this PC and **has not been compiled or run**; docs/IOS.md is how to build it on a Mac and what to check first.

- **Build**: `CMakeLists.txt` takes `CMAKE_SYSTEM_NAME=iOS` (`RF_IOS`): the app bundle `raveform_ios` (bundle id
  `dev.raveform.game`, iOS 15+, iPhone and iPad, Xcode's automatic signing with `RF_IOS_TEAM`), **MoltenVK** linked statically
  from the Vulkan SDK for macOS 1.4.363.0 (its entry point kept for SDL's Vulkan loader, which looks in the app's own image),
  the frameworks; no headless tools or tests. `scripts/fetch-deps.sh` (the same pins as `fetch-deps.ps1`, the headers from the
  SDK, MoltenVK's licence) and `scripts/build-ios.sh` (generate, build, `--install`, `--run "<app args>"`, `--open`).
- **Bundle**: `ios/Info.plist.in` (landscape, full screen, no status bar, a black `UILaunchScreen` for native resolution,
  ProMotion's 120 Hz allowed, the local network's prompt, Documents shared), `ios/Assets.xcassets` (the icon: `make_icon.py
  --ios-only`, one 1024 px square, the adaptive icon's visible part; the launch colour), `ios/PrivacyInfo.xcprivacy` (no
  tracking or data collected; the reasons for the time-since-boot, file-timestamp and user-defaults APIs).
- **Code**: `src/core/platform.h` (`RF_IOS`, `RF_MOBILE`); `src/platform/ios/ios_glue.h/.mm` (UIKit's impact haptics, the
  device's model, the Documents folder); the app's iOS window (full screen, borderless), hints (the home indicator auto-hides;
  the audio session is playback), touch at 163 points an inch, captures and results in Documents; first-run hints read touch on
  every phone or tablet. The Vulkan instance asks for portability devices and the device enables `VK_KHR_portability_subset`
  where it is listed (both harmless elsewhere). The credits list MoltenVK on iOS (libc++ is iOS's own there); the Apache
  text comes from MoltenVK's licence where Xcode's toolchain has no LLVM licence file. The build's name follows the
  configuration (`$<CONFIG>`), so Xcode's Debug and Release each get theirs.
- **Checked here**: desktop dev and Debug build (Debug validation 0 messages, the names `dev` and `debug`), the APK builds.
  **Not verified**: everything on iOS (docs/IOS.md's list: compile and link, start, formats, timestamps, touch, audio,
  background, 120 Hz, co-op by code with an Android phone, haptics). The room list on iOS needs Apple's multicast entitlement;
  joining by code or address does not.

## 2026-11-14 (night), the wordmark, the meteors, the demo's movement

- "Make the title of the game at the main menu look way more polished", then (of a first version in letters of its own,
  extended and leaning) "i like the previous font": the game's font's strokes (`VectorFont::glyphSegments`, new) drawn as
  chrome neon in `Play::drawTitleLogo`: a pooled halo (the HUD is drawn after the bloom, so its glow is drawn here), a glowing
  extrusion, the tube in a chrome split at each letter's horizon, a white-hot core, flat joins where strokes run straight on,
  a sheen every second bar (on the run's music clock, so pictures show it), two glints, a laser underline (screens.md
  "Title"). Video `captures\video\title_logo.mp4`.
- "Meteor projectiles need to be bigger so that it makes sense with the hit area size and impact force": a burning rock 88 wu
  across at the ground (40% of its ring; 55% of that as it starts high up), white-hot facing the ground, a cone of fire,
  embers, a warm glow (arena.md). Picture `captures\pt5\meteor_*`.
- "The demo bot also needs to have smooth movement, not erratic at all": the title's demo steers with its stick eased over
  0.35 s and never dashes (`Play::attractInput`, also in the title's pictures). Video `captures\video\title_demo_smooth.mp4`.

## 2026-11-14 (night), the polish batch: tells, arena bodies, the leftovers, the Beam's swing, VIVID colour

The owner, on the list of gaps: "lets work on these too: 3, 4, 5, 7, wide color".

- **3, the two tells drawn as threats**: a Volatile bomb's and a meteor's filling front were solid lines on the priority
  layer; now `PlayView::tellRing`, a solid 1.5 px danger line in the scene light at the tells' level, rising over the last
  beat, as the dashed tells are (look.md 5 "Hurts").
- **4, what passed through arena bodies** (M6 A2's rest): Chain Storm's and Storm Coil's bolts strike a Vault, Rift, gold
  Vault, Comet or carrier in their reach as they strike enemies (Chain Storm: the two nearest, a bolt each from the ship;
  Storm Coil: its zaps go to the nearest targets, a body as an enemy is), Colossus rams them every tick of the touch, Last
  Light's mini-Nova hits them with the Nova's blast in proportion to its reach (420 of 1,400). `World::arenaInReach`.
- **5, the leftovers**: Swift turns 1.3 times as fast (`World::turnK_` on every family's turn); a Wyrm's head and a Tether's
  pair or triad can be elites (traits.md "Who"; no Twin or Splitting on them: their extra bodies would be loose members;
  `spawnEliteCreature`, `spawnCreature` returns the group); Fata Morgana's wind whips the drawn lattice 10 wu along it in
  gusts travelling downwind, the oasis at rest in the accent's tint, the whole lattice tinted as she dies (morgana.md;
  `LatticeView` wind and calm); **Feedback's Echo carries its player's Orbitals, Pulse Rings and Sweeper** a delay behind,
  round the Echo, the first Echo's point-reflected (feedback.md rule 4; `World::feedbackEchoRiffs`): shards, ring fronts and
  arms hurt as echo shots do (a cell), drawn as danger shards and lethal lines. New picture `m6_fb_riffs`.
- **7, the Beam under the assist**: the drawn Beam swings to a new aim over about 45 ms instead of jumping (the assist
  changes target between ticks, and a teammate's Beam follows its poses a few ticks late; one picture had two players'
  views of one Beam a hundred degrees apart). The hits are as they were.
- **Wide colour** (TECH_SPEC 5.2, PLAN M6): SETTINGS, LOOK, COLOUR: STANDARD / VIVID, VIVID presenting in Display-P3 where
  the screen offers it (the Moto does, PLAN 3.2; this PC does not: the row says so). `--vivid` for a run.
- Ran: sim 6,114 / save 60 / net 131,368 / audio 2,718 checks, 0 failures; determinism PASS; the clean 4-minute loopback
  soak (2 players) PASS, 0 census mismatches. Pictures `captures\pt5\` (Morgana's wind, the Volatile tell, Meteor Rain),
  `captures\m6\m6_fb_riffs*`, `settings_look_colour.png`.

## 2026-11-14 (night), the quiet title

- "Like maybe after waiting at the main menu for a while the menu fades away so you can see the demo until you tap the screen
  and the menu fades back in?": after 8 bars without an input the title leaves on the next bar line, home's order backwards a
  beat apart, and the demo comes up from a third of its light to full; any input (a tap, the pointer moving more than 4 px, a
  key, a button, a stick) brings it back over a beat and does nothing else (screens.md "Title"; `Play::titleIdleFrame`,
  `titleGone`, `uiActivity_`). Picture `m6_title_idle` (new): the title left over its bar and, after a tap on PLAY's place two
  and a half bars later, back as the title (the run did not start; the picture logs it). Video `captures\video\title_idle.mp4`.

## 2026-11-14 (evening), the Reprise's homecoming: one shockwave of thin lines with a thick glow; the co-op report

- "They should all come out 'at once' like a real exp, just a thick transparent layer of each color sandwiched together" (230d128:
  ten see-through bands side by side, leaving the core together), then "each color line should be very thin but have a very
  thick glow": ten lines 1.5 ref px wide in each Track's full colour, a gap apart (16 wu growing to 36), each glowing over 1.6
  gaps on either side (glass strips between the lines, each edge carrying the summed glow there) and 3 gaps past the front and
  the back; the front at 80 + 1,420 x (1 - (1 - u)^2.5) wu over 1.8 s (`drawReprise`, look.md 5). The glass batch takes 12,288
  vertices (6,144: the wave alone draws up to about 3,700). Debug run of `m5_rp_death`: 0 validation messages. Video
  `captures\video\boss_death_reprise_v4.mp4`.
- Co-op, "bullets on p2 and p3 are not rendering", then "intermittent ... killed and relaunched on the 3 devices and it's fixed":
  not reproduced on this PC (three peers over UDP: a fresh run into Kaleidos at High and at Low, a retry after everyone went
  down, the shared pause and its count-in; every peer draws the shots, the enemy bullets (74 = 74 in the fight) and the boss).
  The clients get Kaleidos two ticks after the host (a picture on the host's entrance tick had looked like a client without
  it). The Moto's log has no run since the 16:52 install and the other devices were not on this PC, so there is no log of
  the failing session. Not verified on phones.

## 2026-11-14 (evening), the Reprise's rings slow as they spread; the title's demo aims as the owner does

- "I like the rings in every Track's colour but it just races across the screen instead of the normal explosion with resistance
  like the first one": the ten rings spread as a shock ring does (radius x 0.25 + 0.75 x the square root of its age's share,
  fading over 1.2 s, to 1,100 wu and 120 more for each; the white one to 3,200 over 1.8 s); they had raced at 2,400 wu/s. Lines
  in each Track's colour end to end (the shock ring's glyph, tried first, has a white core that washed them out).
- "The demo camera doesn't seem to have the same easing as the auto aim camera movement": the title's attract bot aimed by hand,
  its aim jumping between targets, and the camera leans along the aim; it now steers and the auto-aim assist fires for it (the
  aim held on a target), as the owner plays (`Play::input`; `--bot` and `--autoplay` keep their own aim).
- Video `captures\video\boss_death_reprise_v2.mp4`.

## 2026-11-14 (evening), a boss falls; the Reprise falls hardest (look.md 5)

- "Give the bosses a little bit for epic deaths, but not as epic as the track 10 boss": `PlayView::bossFall` for every boss
  (Kaleidos under its tumbling facets; the generic death; the late breaks of Event Horizon, Polyrhythm and Feedback): a white
  flash to 800 wu held 0.2 s (420, 0.12), three rings to 1,100 and two more to 1,600 and 2,000 (the second colour, gold), 300
  sparks and 120 gold, distortion to 1,200, shake 7, a white ripple to 2,600 wu; the gong at 0.55 over its own sound (play.cpp).
- "Make sure the reprise one is even more epic": a 2,000 wu flash held 0.4 s and a gold one after it, six arm-coloured rings,
  400 gold sparks and 100 in each arm's colour, two distortion waves, shake 10, the gong at 0.9 with the impact (reprise.md had
  promised them; they had not been wired), the action held three times as long; then drawn from its death (the view keeps six
  shock rings at once): a ring in each of the ten Tracks' colours one after another, and a white ripple to 6,000 wu.
- sim 6,114 checks, 0 failures. Pictures `captures\pt4\strip_boss_death_sheet` (Kaleidos), `captures\m5\part6\rp_death`.

## 2026-11-14 (evening), the bug check pass; the version on the title

- "Include a version # with build date at the bottom right at the main menu": `cmake/build_stamp.cmake` writes
  `generated/build_stamp.h` on every build (the version from `CMakeLists.txt`'s project VERSION, now 0.9.0; the commit read from
  `.git` without git; the date and minute), rewritten only when it changes; `src/core/build_info.cpp` makes the line and the
  title draws it under HEADPHONES RECOMMENDED (cap 11, white at 0.4: `V0.9.0  7775A64  2026-11-14 16:29`; screens.md "Title").
  The APK's versionName 0.9.0 (0.0.1-m0).
- "Now do a bug check pass", over this evening's work (f0624e5..7775a64): the diff read through (the output stage's two gains
  and their reset, the Shards' homing and its guard, Black Hole on its owner's device only, the fork flag's bit, three Boost slots
  and the HUD row, the tempo and health factors, the Reprise's cues and placement, the lethal-line helper's batches and their
  capacities: every batch drops past its capacity, never overruns). Ran: audio 2,718 / sim 6,114 / net 131,368 / save 60 checks,
  0 failures; calibration check, determinism, the clean loopback soak, nova-check, a 12-minute bot soak (Shards peaked at 1,729,
  past the crowded line: the homing rule ran) PASS; Vulkan validation on the Reprise, Event Horizon and Track 5 at Low: 0 messages;
  `scripts\sanitize.ps1` (ASan + UBSan: the four test suites, determinism, a 3-minute bot run, fights in Tracks 4, 7 and 10, the
  3-minute net soak): all clean, 0 reports. No bug found.

## 2026-11-14 (evening), a substantial glow on every red projectile; Weaver Prime's lines

- "Do a pass on any red enemy projectiles (lines, dots, anything) and make sure they have a substantial glow": enemy bullets had
  their glow (a sparkle) only from High up, so on Low and Medium, the phones' tiers, none at all. Now every bullet has a pooled
  glow on every tier (26 px, 0.32, never under 0.6 of it in a crowd; quarter resolution), the sparkle on top from High up;
  `RendererDesc::maxPools` 1,024 -> 4,096 for them. A Skitter's mine: a pooled glow at its core (blinking) and over its reach.
- Weaver Prime ("red lines that were too thick and have no glow"): a lethal line's core is never over 4 px (`kLethalMaxPx`),
  so its 12 wu laps are thin lasers whose glow shows their width; the cage's burst edges are lethal lines with their glow (1.2 px
  lines with none before).
- The Moto (shared; Track 10's first Drop, Low, 30 s, `--drop 10.1 --nohits`): 0 missed frames (panel), GPU 12.19 / 12.95 / 14.24
  ms (p50/p95/max), the pools pass 0.09 ms; the CPU-side present interval p95 17.7 ms (its contract line fails there, as Swappy
  paces; the panel's passes). Pictures `captures\pt4\glow_arp_low`, `weaver_prime_low`.

## 2026-11-14 (evening), Kaleidos Reprise: 1.5x bigger, faster, tougher, and epic

- "The track 10 Boss needs to be more epic visual wise and audio wise also make it 1.5x bigger also 1.5x faster ... and 1.5x more
  hp": the wheel 1.875 times Kaleidos (1.25): core 120 (80), arms 450 (300), the arms' hit circles 51 (34); it turns once in
  5 1/3 bars (8) and keeps 675-975 wu from the nearest player at 45 wu/s (Kaleidos's 450-650 at 30); health x 1.5
  (`kRepriseHealthTune`, so about 90 s for the novice where it was 60: not measured); the camera holds 0.75 (0.85; at 0.7 the
  lattice's edge showed). Its bullets keep their speeds.
- Epic: twelve rays of light behind the wheel in the arms' colours (white and gold in Last Light), flaring on the kick, and a
  wide glow of the soloist's colour; every bar's One booms (new `BossCue::RpBoom`: the impact at 0.45, a ring off the wheel, a
  core flash, a lattice push), every fourth bar and each change of phase rings the gong (`RpGong`: two rings across the frame,
  the distortion ring, a shake); the death is the run's biggest burst (a 1.4 km white flash, a ring in each arm's colour, 400
  sparks, distortion, a shake of 9) before the ripple and the ENCORE card (reprise.md "Death", "Sound").
- sim 6,114 checks / audio 2,718, 0 failures. Pictures `captures\pt4\reprise_a` (Reprise), `reprise_b` (Medley).

## 2026-11-14 (evening), Collector one rank, three Boosts at once, Meteor Rain's landing

- Collector "too op late game": a Boost every **500** Shards, **one rank** (250, then 150 at two; `kCollectorEvery`, the cap in
  `kMods`). Its words `A BOOST PER 500 SHARDS`.
- "Can only seem to have 2 temp powerups active at a time": **three** at once (`kBoostSlots` 3; a fourth replaces the oldest).
  The HUD's Boost row puts a gap between each pair (it counted one gap for any number).
- "Meteor rain needs more of a visual impact on where it lands": the meteor falls onto its ring over the two beats (a white-hot
  head and a gold tail from the upper right, faster as it comes); the landing a 1.4 flash the size of the ring, three rings,
  a danger crater glowing for 0.9 s, 36 sparks, a distortion ring, a lattice dent of 200 and a shake of 5 (arena.md).
- sim 6,114 checks / net 131,368 / audio 2,718, 0 failures; determinism PASS. Picture `captures\m5\part5\ev_meteor_rain`.

## 2026-11-14 (evening), thinner lethal lines with more glow; what an area means (look.md 5)

- "Can you make them thinner and make the glow more to compensate?": `lethalBeam`'s core is half the kill width, never under
  2 px (the whole kill width, at least 3 px), its axis line at most half the core; the glow sigma 0.6 of the kill width, at
  least 8 wu, 0.55 on the axis (reduce flashing 0.4) (0.5, 6 wu, 0.35). Echo shots back to their thin core with a stronger glow
  at the head and down the middle.
- "Some areas aren't clear if they are safe or not (like the big circles in track 5?)": the Sentry Array's (Track 5's first
  Drop). Look.md 5 now gives every area one of five looks: hurts (a lethal-line edge and a faint danger fill), holds (a solid
  line in the enemy's hue), shields (a dotted ring in its hue), pulls (its hue, rings drifting in), shelters (white or gold).
  Changed: the Sentry Array's fence blue (white) and its domes dotted in its hue (white), its turrets filled faintly as bodies
  (touching one hurts); Sentry C's domes dotted (a solid ring with white); the **Storm Cell** (Tracks 5, 9, 10; it takes a
  cell from a ship that stays inside) a danger edge and a 0.05 danger fill (an accent ring); **Event Horizon's** black disc a
  danger edge (touching it costs a cell).
- sim tests 6,112 checks, 0 failures. Pictures `captures\pt4\t5_drop2`, `red_t7b`, `red_horizon`, `captures\m5\part5\ev_storm_cell`.

## 2026-11-14 (evening), the owner's red-line pass (look.md 5 "A lethal line reads at a glance")

- The owner on f0cd14f: "Track 9 red projectiles: no these were thin red lines streaking across the screen": Fata Morgana's
  sand, not the bullets. **Undone: the 0.25 on Tracks 9 and 10's bullets** (`kBulletsLate` gone; Tracks 8-10 as written again;
  the bullets stay drawn larger). **The sand**: fifteen streaks (sixty) at 0.8 (0.5), each drawn as a lethal line (4 wu) and
  flaring on the kick, when the storm hurts (`boss_prose.h` `kMgSand*`; morgana.md, its tool `bosses_7_10.py`).
- "Do a pass on any red line enemy stuff, make sure that they are all visible enough, have enough glow": every lethal line goes
  through `PlayView::lethalBeam`: the priority capsule as wide as it kills but never under 3 px, the rim, the white-pink axis,
  and a glow of sigma max(half the kill width, 6 wu) at 0.35 on the axis (0.20 at half the kill width: a 6 wu Tether glowed 3
  wu), its discs centred on equal pieces so polylines glow evenly. Now drawn by it too: the Weaver's trails and Ram B's wakes
  (scene-only lines fading with their age before; now whole while they kill, 0.6 in their last tenth), the Battering Ram's
  wake, Weaver Prime's laps, Polyrhythm's walls (3 px; now their 10 wu kill width), the edges of Feedback's Howl and its Nulls
  (4 wu). Echo shots: a core of at least 3 px and a glow at the head. Tells stay see-through (0449eb3).
- sim tests 6,112 checks, 0 failures; determinism PASS. Pictures `captures\pt4\red_*` (Morgana, Polyrhythm, Feedback, Track 7).

## 2026-11-14 (evening), the owner's play of f0624e5: the mix, Arcadia's blocks, Track 9's bullets, Boosts that show, the grade, the first level, Shards, the tempo

Done by the lead alone (the owner: "fix these yourself with no subagents"); design and code in one commit.

- **"Annoying attenuation when tons of sfx are playing."** Measured first (`raveform_headless --render duck-check`, new: the
  Drop held, 4 s of the music alone, then 8 s of a crowd dying: three small kills and a medium a frame, a large every fifth, a
  Shard every second frame, a hit every two seconds; every 32 frames the effects' dip on the music and the limiter's gain): the
  music lost **2.2 dB on average, 4.0-5.3 dB in its worst 5%**: large kills' dips (sfxproto's 0.25 a large kill) 1.9 dB of it, the
  limiter the rest, since a crowd dying took the effects bus to 0 dBFS (a large kill alone is -8.0, a hit -5.2, a Nova's blast
  -3.1). Now: a large kill dips the music no more, a hit 0.25 (0.5) and a Nova's blast 0.22 (0.45); and the effects bus has
  its own gain in the output limiter's delay line (`Engine::kFxCeiling` -4 dBFS: a pile of effects is quieted on its bus, the
  music is not), the sum's gain planned from the music and the effects at their own request (engine.h). The music now loses
  **0.66 dB on average, 2.1-2.6 dB in its worst 5%**. `nova-check` fits a gain for the music and one for the effects (two
  gains now move): THD+N 0.24-1.05% (pass under 3%).
- **Arcadia's (Arpeggiator's) red pills** "much smaller with the glow making up the rest of the size": a block is a lethal
  capsule 36 x 12 (72 x 28) drawn as a lethal beam is (priority capsule, white-pink axis) with a danger glow out to about the old
  size that hurts nobody (`arpeggiator.json` `blockWidth` 36, new `blockDepth` 12; `World::arpBlockHit` reads both).
- **Track 9's "red line enemy projectiles"** (the enemy bullets: dozens of small outlined diamonds in its Drop, Tracks 8-10 were
  as written): `kBulletsLate` 0.25 in Tracks 9 and 10 (`World::bulletTrackScale`; Track 8 and the loop as written), and the
  bullet glyph half a standard enemy across (0.38), its hit radius 5 wu as before.
- **Boosts that don't seem to work.** Black Hole waited for a Nova press, which a player on the assist never makes: it goes off by
  itself the tick after it is taken, as a free Nova (no Nova spent; `World::updateBoosts`; the sim test now presses nothing).
  **Fractal** ("i still don't know what it does"): a split shows: a Y of light at the hit along the halves' paths (`kFxFork`
  draws two arms, bolt styles 4-5), the halves drawn as Prism lanes turned 60 / 120 degrees round the hue wheel
  (`kShotForked`, the last free shot flag); Fork's halves the same. Words: `PULLS ALL IN, THEN BLOWS`, `HITS SPLIT SHOTS IN TWO`.
- **Colour pop** ("like we did for the Prism powerup"): a grade after the tone map on the scene only (look.md 3 "Grade",
  `look.json` `grade`: saturation 1.12, vibrance 0.45, contrast 0.3; `composite.frag`, the UBO's new `grade`). The threat layer
  and the HUD are drawn after it. The M1 acceptance stills predate it (their compare will differ).
- **The first Shard level** 100 Shards (170; `kLevelShardsFirst`), the later ones as before.
- **Shards that vanish.** Over 1,536 out, every loose Shard older than a bar went out at once at full brightness (the crowded
  rule of play test 1); now it flies to its player as under Magnet Storm and is collected (a homing one is not lost to age while
  its player lives; the view draws a homing Shard whole).
- **The tempo** "start at 110 bpm and scale up slightly slower": 110 to 155, five a Track (`kTrackBpm`, the Track files; the loop
  holds 155). Every boss's and mini-boss's health factor x (its Track's tempo now / then) (`tempoHealth`), so each fight lasts
  the seconds it was tuned to (the players' damage a second follows the tempo). Loudness calibrated again (gains +0.2-0.4 dB).
  Not measured again with the novice (the scaling is analytic; the owner's rule is short runs): a fight that is held by its bars
  rather than its health may run up to a tenth longer in seconds.
- Checked: sim tests 6,112 checks, 0 failures (the director's tempi, bullets by Track with Track 8 as written and Track 9 at
  0.25, the Arpeggiator's 36 x 12, the first level 100, Black Hole with no Nova pressed); audio tests 2,718, 0 failures (the data
  check at 110 + 5 a Track; the engine's timing tests run on the music at the tempi they were written at, 120 + 6, with their own
  bank: `testUseWrittenTempi`); calibration check, net tests, save tests, determinism, the clean 6-minute loopback soak pass
  (the impaired soak not run: the owner's rule). The bot's Shard levels (3 seeds, 4 minutes): Track 1 a level every 44 s, Track
  2 every 18 s; three level-ups in Track 1. Pictures in `captures\pt4\` (before / after: `endgame`, `arp`, `t9`) and
  `captures\m5\part3\boost_fractal_stretch`, `boost_black_hole`.

## 2026-11-14, Kaleidos Reprise about 60 s (design bccb4eb; GAME_DESIGN 7.6, "the finale is the longest")

- `kRepriseHealthTune` 0.347 -> **0.659** (x 1.9, from the length ratio 60 / 35). The novice from Track 10's start
  (`--handover 10`, seeds 1-20, the build as committed): **20 of 20**, won in a **median 60 s**, mean 74 s: four runs took
  two to three and a half minutes (127, 128, 149, 154, 202 s: the weakest builds the careful bot hands over), the rest 22-77 s.
  Measured on the way: x 1.71 19 of 20, 51 s; x 1.9 as a lever on 0.347 19 of 20, 61 s; x 2.1 20 of 20, 65 s (means of the won
  fights; one long win more or less moves a 20-seed mean by 5-10 s here, hence the median).
- Its pace reference (`kBossPaceNovice`, the novice's first eight bars a player) measured again at 0.659: 1,097 -> **1,708**
  (20 fights). No fight's handover runs through the Reprise, so nothing else moved.
- sim tests 6,108 checks, 0 failures; determinism PASS. The dev APK on the Moto (installed, not launched).

## 2026-11-14 (afternoon), the owner's play of af4b101 (design 0449eb3, 3283249, a09dd2c-02c28ab): enemy bullets by Track, see-through tells, Event Horizon's pushes, every fight 30-60 s

The engineer before me was stopped in the middle of this job; its diff (the bullets by Track, 1b) is kept and finished: what it
had and what changed in it is under 1b.

### 1a. Event Horizon at half health (0449eb3; horizon.md's note)

- `kHorizonHealthTune` 2.178 -> **1.089** (x 0.5: "way too much hp, needs like 0.5x"). With everything below in, the novice
  from Track 4's start wins **19 of 20 in 53 s** (it was 27 of 40 in 75 s; the lead's estimate 38 s: the fight is not linear in
  its health). Inside 30-60, so nothing more (GAME_DESIGN 7.6). Its pace reference: under 3.

### 1b. Enemy bullets by Track (0449eb3; GAME_DESIGN 8)

- **The rule** (`World::trackCount`, `bulletTrackScale`, `world_upgrades.cpp`): a volley asks for its page's n and fires the
  Track's share: x 0.5 in Tracks 1-3, x 0.75 in 4-7 (`tuning::kBulletsEarly`, `kBulletsMiddle`), as written from Track 8 and in
  the loop. Integers only (in 64ths), so every device decides alike. A pattern loses bullets evenly (each caller spaces its
  bullets by the count it gets, so a fan stays centred and a ring even), never below `least`, its page's smallest; where n x f
  is not whole the volleys alternate between the counts either side of it, so they average n x f exactly (a fan of five at 0.5:
  2, 3, 2, 3; three at 0.75: 2, 2, 3, 2). A volley that cannot thin (one bullet, a mirrored pair, a turning spiral's arms, a
  pattern at its smallest) fires whole on that share of its volleys: every second at 0.5, three in four at 0.75, on its beat
  (a Bresenham count over the volley's own index, salted by the enemy's serial so neighbours do not skip together). A fight's
  patterns go through `bossCount`: the lever and the loop (`bossBulletDensity`, as before), then the Track's.
- **Every source** (each `spawnBullet` caller): the crowd's Hound fan (1, 2, 1, 2 at 0.5: its smallest an aimed shot, so every
  glint still fires), the Sentry's turning arms (whole, on every second volley: two arms would be another pattern), the
  Flanker's side shot, the Moth's petal pair and its closing burst, the Blink's path bullets (thinned along the hop), the
  Drifter's aimed shot (a volley that will not fire shows no glint), the Carom's bounce fan, the Wyrm's mirrored side pair (the
  whole snake on the same volleys; its head tells only those), the Well's ring and burst, the Mimic's answer; Kaleidos's tips
  and core rings, Longwave's troughs (by the wave's pass: every second trough of a wave fires, so the wave keeps its shape), its
  whip and the bare head's rings, the Choir's fans, streams and solo rings (each pattern by its own volleys), Event Horizon's
  eruption (the arms on three sixteenths in four; the spiral as long and as turned), Feedback's echo shots (by tape slot: three
  recorded shots in four), Cadence's fans and rosettes, the Ram's wall fan, the Carom King's bounce, the Moth Queen's rosettes
  and court bursts, the Blink Star's decoy fans, the Sentry Array's volleys, the Mirror Mimic's answers. Fata Morgana, the
  Arpeggiator, Deep Well and the Reprise live in Tracks 8-10 (as written). Polyrhythm, Twin Tether and Weaver Prime fire no
  bullets; elites and arena events have no bullets of their own. Beams, bodies, the Arpeggiator's blocks and trails untouched.
- **What the stopped engineer's diff had**: the helpers and every call site above. Changed: its thinning rounded (a fan of
  three gave 2 at both 0.5 and 0.75: x 0.67), now exact over the volleys; several volley indices did not step by one a volley,
  so a one-bullet pattern there fired always or never: the Choir's (a sixteenth's index for patterns on the eighths), Longwave's
  troughs (the step index, which a segment sees every eighth step: half the segments never fired), Kaleidos's core rings, the
  bare head's rings; the Sentry's three arms became two; the Drifter's and the Wyrm's tells showed for volleys that did not
  fire; `MiniState::answers` (the Mimic's count) kept.
- Co-op: alike everywhere (the Track and the volley grids are song time; Feedback's tape as each device records it, as
  before). `Rules::bulletsAsWritten` turns the rule off: the tests that count a page's pattern, and `--bullets-as-written` in
  the headless tool. Test `bullets by Track`: five shooters over eight bars of a Groove fire 340 as written, 170 in Track 2
  (x 0.50), 257 in Track 5 (x 0.76), 340 in Track 9.
- **Before / after** (`--sim crowd --god --policy reckless`: the careless bot ignores bullets, so the crowd is the same run
  with and without the rule; `captures\bullets\r_*`): the enemy bullets alive in the first run's Grooves, mean and most at once.

  | | seed 1, as written | seed 1, the rule | 10 seeds, as written | 10 seeds, the rule | fired of what the pages asked (10 seeds) |
  |---|---|---|---|---|---|
  | Track 2 Groove | 27, most 64 | 14, most 43 | 20, most 100 | 11, most 55 | 920 of 1,916 (x 0.48) |
  | Track 5 Groove | 11, most 37 | 5, most 16 | 13, most 126 | 8, most 98 | 622 of 833 (x 0.75) |

  The same seed matches in Track 2 (the pages asked 239 and 246); by Track 5 the two runs differ (the director's assist
  answers the god-mode hits), so the 10-seed means and the same run's asked / fired are the better reading there. Tracks 3, 4,
  6, 7 fired x 0.52, 0.72, 0.72, 0.67 of what was asked. The headless crowd table prints these lines now (`enemy bullets alive in
  the first run's Grooves`, `fired a Groove second by Track`).

### 1c. A tell is see-through (0449eb3; look.md 5, Telegraphs, and the priority capsule's row)

- `PlayView::telegraphLine(d, a, b, fireTick)` draws in the scene light only, never on the priority layer (no rim, no core):
  dashes 1.5 px in the danger colour at 0.35, rising to 0.6 over the beat before `fireTick` (the view's constants
  `kTellWidthRefPx`, `kTellLevel`, `kTellLevelFiring`; dash and gap 30 wu each, chosen: the beam tells' own). It dashes what it
  is given, so every caller passes the whole line and when it fires: Kaleidos's beams (the next bar line), the Choir's cage and
  held note, Event Horizon's jets, Polyrhythm's burning link (the coming burn), the Blink Star's edges (the fifth hop),
  Longwave's lunge (`lungeTick`), Alpha Hound's and the Ram's lock lines (`cueTick`), the fights' charge lines
  (`FightLine::until`), Volatile bombs' and meteors' dashed rings. A shooter's glint and a Ram's chevrons are marks, not dashes
  (`tellMark`): at the telegraph line's scene level as before, off the priority layer too. A beam's tell is no longer as wide
  as the beam (look.md v2.5's rule, replaced by this one).

### 2. A shove is a push, never a jump (3283249; horizon.md rules 6 and 7, the phase 2 row)

- **Cause**, as the lead found: `World::knockPlayer` added to `PlayerState::knock` and the controller added the difference to
  the ship's position in one update (the wave 140 wu in a tick every bar of Tidal; the throw 300).
- **Now** `PlayerState::shoves[2]` (0 the wave, 1 the throw): the push's whole way, its start and length in ticks, a serial;
  `World::shovePlayer` sets one; `PlayerController::takeShoves` carries the ship each update by what the push has done since
  (`shoveProgress`, 1 - (1 - u)^2: a velocity that starts at twice the mean and dies away to nothing), on top of its own
  steering, the whole way at any frame rate. The throw: 300 wu over half a beat from the touch; meanwhile the horizon's pull
  lets go (`wellPull(pos, false)`, `World::thrown`) and the ship is neither thrown nor hurt again. The wave: 140 wu over a beat
  from the moment the ring reaches the ship, once a wave (the push starts faster than the ring, 650 wu/s at Track 4's tempo, so
  the ring passes the ship again as it slows: without the guard it pushed again and again). Shards: an outward velocity their
  drag takes away (140 wu x `kShardDrag`: about 140 wu, 82% of it within the beat at Track 4's tempo).
- **Co-op**: each device pushes its own ships, as before (`isLocal`, from the song time); a client's ship is pushed by its
  own world and its pose carries the motion. No protocol change.
- **Anything else that set a ship's position**: nothing that shoves. The rest are placements or contacts: a run's start, a
  revive at the beacon, the host's record of where a ship went down, the pose the world takes each tick, the Dash (the
  player's own 220 wu over 0.12 s), a Bumper (the ship put on its edge with a kick of 600 wu/s, a velocity), the Pen's walls
  and the arena's edges.
- Test (`horizon`): a touch sets the throw (300 wu over half a beat); the controller carries the ship 270-330 wu, never more
  than 70 wu a tick; the pull lets go meanwhile; no second throw; the wave's push is 140 wu over a beat, 3/4 of it at half.
- Strips (`captures\af4b101\`, dev, off screen, a frame a sixteenth; `review-m3b.ps1 -Config dev -Only m6_hz_wave,m6_hz_touch`,
  scripted in `play_pictures_m6.cpp`): `strip_m6_hz_wave_sheet.png` (a ship standing 380 wu out in Tidal held, from a
  sixteenth before the ring reaches it) and `strip_m6_hz_touch_sheet.png` (a ship flown into the horizon and thrown). Tidal is
  `--fight horizon:1` (0-based like every `--fight` phase; `:2` is Collapse, which has no wave).

### 3. Every boss fight 30-60 s for the novice (a09dd2c-02c28ab; GAME_DESIGN 7.6)

The novice through the half-rate assist from the fight's Track's start (`--sim crowd --policy novice --handover N --minutes
60`), after 1b; won first meetings and their mean seconds (`captures\fights\`). Ten seeds were too few for these: a fight's
length moves 20-50 s between seed sets (the build each run inherits), so each was measured on 20-30 seeds while tuning, then
once more on the build as committed, 10 seeds each (the last column).

| Fight | af4b101 (the lead's figures) | with 1b, its factor as it was | factor | tuning (20-30 seeds) | as committed (20 seeds: 1-10 / 11-20) |
|---|---|---|---|---|---|
| Longwave | 32 of 40, 83 s (from the run's start) | 20 of 20, 88 s | 0.1925 -> **0.0886** (x 0.46) | 20 of 20, 49 s | **20 of 20, 49 s** (55 / 43) |
| Event Horizon | 27 of 40, 75 s | (1a only) | 2.178 -> **1.089** (x 0.5) | 19 of 20, 53 s | **19 of 20, 52 s** (62 / 43) |
| Polyrhythm | 36 of 40, 20 s | 18 of 20, 19 s | **0.4165**, as it is | x 1.3: 16 of 20, 21 s; x 1.58: 12 of 20, 25 s | **18 of 20, 19 s** (22 / 15) |
| Feedback | 26 of 40, 63 s | 17 of 20, 70 s (once the cuts before it were in) | 0.2925 -> **0.16** (x 0.547) | 30 of 30, 50 s | **20 of 20, 52 s** (57 / 46) |
| Cadence | 28 of 39, 91 s | 7 of 10, 115 s | 2.113 -> **0.919** (x 0.435) | 20 of 20, 52 s | **20 of 20, 39 s** (50 / 28) |
| Arpeggiator | 28 of 40, 69 s | 20 of 30, 50 s; 16 of 20, 65 s with the cuts before it | 1.863 -> **1.435** (x 0.77) | 17 of 20, 53 s | **20 of 20, 43 s** (48 / 37) |
| Kaleidos Reprise | 35 of 40, 114 s | 26 of 30, 100 s | 0.914 -> **0.347** (x 0.38) | 20 of 20, 50 s | **20 of 20, 35 s** (38 / 31) |

Kaleidos (55 s), the Choir (34) and Fata Morgana (41) untouched. The committed build's Cadence, Arpeggiator and Reprise came
out shorter than in tuning (39, 43, 35 s against 52, 53, 50): the re-measured pace references (below) are the only change
between, and they move the careful bot's own fights before the handover (it aims, so its pace is set from them): longer
fights there, more Shards, a stronger build handed to the novice. All inside 30-60.

- **Not linear in health**, as the lead warned: Longwave x 0.562 gave 58 s, x 0.46 49 s, x 0.36 36 s; Event Horizon x 0.5 75 to
  53 s. **Feedback**'s won fights run from 10 s to over 100 by the build the novice inherits (the head shuts while the
  assist fires: x 0.25): seeds 1-10 stay at 60-83 s from x 0.77 down to x 0.45; x 0.547 of its old factor (x 0.71 on 0.225)
  gives 30 of 30 in 50 s, x 0.6 the same 50 s in 29 of 30.
- **The cuts reach downstream**: a shorter fight pays fewer Shards, so the build the novice inherits for the next fights is
  weaker. With 1b alone the Arpeggiator was already 20 of 30 in 50 s; once Longwave, Event Horizon and Feedback were cut it
  took 65 s (16 of 20), hence its x 0.77; Cadence and the Reprise were measured with every cut before them in.
- **Polyrhythm** stays: x 1.3 of its health gave 16 of 20 in 21 s, x 1.58 12 of 20 in 25 s, against 18 of 20 in 19 s as it is
  (the rule: raise it only if the novice wins it about as often). Said so to the lead.
- **The pace references** (`kBossPaceNovice`) are a damage rate (the novice's first eight bars a player), so a health cut does
  not scale them: scaled, an aiming player's pace would rise by the cut and give back the health the owner took off. They were
  measured again at the new health (they fall only as far as the novice's eight bars do less: parts dead sooner, later phases'
  armour, the weaker inherited build): Longwave 126 -> 93, Event Horizon 985 -> 996, Feedback 509 -> 183, Cadence 1,615 -> 1,240,
  Arpeggiator 2,063 -> 2,450, the Reprise 1,292 -> 1,097; Polyrhythm 1,030 kept. The novice never sets a pace (only aimed fire
  counts), so none of the lengths above depend on them.

### Checks

- `scripts\test.ps1`'s parts, one by one (the owner's rule: no impaired-network runs, so its second soak, 5% loss and 80 ms
  jitter, was left out): audio 2,716 checks; the loudness calibration current; sim 6,108 checks, 0 failures (new: `bullets by
  Track`; `horizon` with the pushes; Longwave's crest and trough shots made 2 for its smaller segments); determinism PASS (hash
  cc439a7eca529334, unchanged: two minutes of Track 1 hold no shooter); net 131,368 checks; save 60; the clean 6-minute loopback
  soak PASS (0 census mismatches).
- Pictures (`captures\af4b101\`, desktop dev, off screen, each with a `.small.png`): `boss_beam_tell.png` (Kaleidos's beam tells
  over the lattice, the bullets and the glass; `review-m3b.ps1 -Config dev -Only boss_beam_tell`), the two Event Horizon strips
  above.

### Noticed, not changed

- Volatile bombs' and meteors' filling ring (inside their dashed ring) is still a solid line on the priority layer: a tell by
  look.md's new words, though not a dashed one.
- The lead's `--fight horizon:2:hold` is Collapse (the phases are 0-based); the strips use `:1`, Tidal.
- These lengths are the novice's with the build the careful bot hands it (`--handover`); they swing 10-30 s with anything
  upstream (above). Polyrhythm stays under 30 s by the rule's own condition.
- The Track's count makes a Drifter's skipped volley show no glint, so it does not pause for it: the crowd moves differently from
  the first skipped volley, which is why a run with and without the rule part ways by Track 5 even with a bot that ignores
  bullets.
- The assist bot's bar (19 of 20 through the ten Tracks) and the careful bot's line were not measured again.

## 2026-11-14, the owner's rulings (design c2f113a, a953828, 27077b6, a667116, eb10554, b12310b): two-cycle Tracks, Longwave never jumps, no Break Trio, Split and Double-Time's ranks, the transitions, the assist at half rate

### A. Every Track is two cycles, like Track 1 (c2f113a; GAME_DESIGN 4.2, 4.3, 5)

- `cyclesOfTrack` is 2 for every Track, the loop's too: 48 bars before each boss.
- **The mini-boss comes on the first cycle's Drop** from Track 2 on (`phraseAt`: the hold on cycle 1; `miniNaturalEnd` the
  first cycle's Drop's end, it counted `cycles / 2`), the boss on the second.
- **The Extra Life**: Track 1 as before (a kill in the first cycle's Drop); from Track 2 the mini-boss drops it as it dies, an
  orb for each player who holds none (`extraLifeDrop(at, fromMini)`, the host's; a mini-boss that warps out drops none).
- **The arena**: from Track 4 the second event comes in the first cycle's Groove (it was the third's); Track 1's Comet stays in
  the second. species.json's debuts are the lead's data (from Track 2 the third cycle's rows now say cycle 2).
- What counted a third or middle cycle, changed: `--drop <track>.<cycle>` takes cycles 1-2 (CLAUDE.md: Kaleidos Reprise's Drop
  is `--drop 10.2`; `10.1`, the thermal check's and the densest crowd's, is unchanged); the m5 Track pictures written for a
  third cycle (`t2_c3`, `t3_c3`) show the second (clamped); the tests (the director, the mini-boss Drop, the mini-boss in a run
  with its Extra Life). Left as they are: species.json's budget and crowd arrays keep a third value nothing reads now; the
  Rift's "closes in a third cycle" can no longer happen (it closes at the second cycle's Drop as before).

### B. Longwave never jumps and swims slower (a953828, c2f113a; longwave.md rule 6, Phases)

- **The lunge** no longer slides the whole path in a beat: the head dashes 500 wu over two beats (the data's `dashWu`,
  `dashBeats`) and place k asks for the path's anchor `k x followBeatsPerSegment` (0.25 beat) earlier
  (`LongwaveState::dashFollow`), so the move runs down the body like a whip and the figure of eight goes on from where the head
  landed. `longwaveAnchor` before a dash reaches a place gives the old anchor (it gave the new one: the investigator's one-frame
  flash of the spine).
- **A dead segment**: the body behind slides up over `segmentCloseSteps` (1) step, place and swing easing (`closeFrom`,
  `closeTick`, `closeTicks` by segment; `longwaveLag`); the crest counts the new body at once.
- **The entrance** unwinds in from the wall: place k makes the head's entrance a sixteenth a place later (it slid the whole body
  as one piece, about 2,700 wu over two bars); the tail lands six beats after the fight starts.
- **The view's spine and echoes** run through the same places (`longwaveRankLags`, interpolated between ranks), so the spine
  never runs ahead of the segments.
- Laps 24 / 20 / 16 bars are the data's. Tests: the close (a step), the dash's numbers and its whip, the laps.
- Strips (`captures\lw\`, dev, off screen): `strip_m6_lw_entrance_sheet.png` (a frame each half second over the entrance),
  `strip_m6_lw_lunge_sheet.png` (each quarter second from a Whitecap dash), `strip_m6_lw_close_sheet.png` (every second frame
  from a segment's death); `scripts\review-m3b.ps1 -Out captures\lw -Config dev -Only m6_lw_entrance,m6_lw_lunge,m6_lw_close`.

### D and E. No Trio in a cycle's Break; the run's first Trio (a667116; GAME_DESIGN 4.1, 4.2, 6.1; screens.md "The Trio")

- **D**: a cycle's Break opens no Trio; the Trios are the Shard levels' and the boss's, which opens in the Track-change Break
  with the whole Break as its clock (it had half, the other half the Break's own). A level-up Trio still open there finishes
  first and the boss's follows (`breakWaiting` 1). A level filled within four bars of a cycle's Break no longer waits for it
  (only a coming Track-change Break holds it back). The Break keeps its music, Shards, refill and spotlight.
- **E**: the first Trio of a run (each player's, `TrioState::opened == 0`) offers Split and Double-Time, each where it can still
  be taken; the third orb (and Lucky Break's, a Fusion's) is drawn as ever and is never one of those two again (my reading of
  "never a third staple twice": no staple twice; Overcharge or On-Beat Crit may be the third). Then the normal deal.
- The first-run hint `FLY INTO ONE AND STAY` fires on the first Trio that opens, now a level-up one; the pictures that opened a
  Trio at a fixed moment (`trio`, `trio_answer`, `trio_take`, `trio_locked`, `trio_hold`, `m6_hint_4`) open a level-up Trio
  there instead of a Break one. Tests: the first Trio's two staples, no Break Trio in Track 1's first 50 s, a level near a
  Break opening at once, nothing waiting on a level-up Trio in a cycle's Break.
- The headless crowd table prints the first run's Trios by Track (level-ups + the boss's).

### The follow-up (design c9218dc): aimed pace, every way in or out of a run, the band's worst case

- **Only aimed damage raises a boss's pace** (GAME_DESIGN 7.6): `BossState::paceAimed` keeps, beside the whole eight bars'
  damage (`paceDmg`, still the measured number the novice's references are made of), what each player did while its pose was
  aimed (`fire 1`); the pace is set from that. Through the assist (`fire 2`) or not firing, nothing counts, so a player who
  never aims meets the novice's boss. Co-op: per player, the host's decision as before (it holds every player's pose). Test:
  the same damage to Kaleidos sets a pace above 1.5 aimed and exactly 1 through the assist.
- **The assist bar after it** (`--policy assist`, seeds 1-20, 60 minutes, the same run as before): **18 of 20**, every pace 1.00.
  Both runs that ended inside the ten Tracks ended against the Arpeggiator in Track 8 (it won 18 of its 20 first meetings;
  5.4 hits a fight, 4.75 of them its beams); nothing else lost a first meeting. Over the hour's retries the fights took 15 of
  35 last cells, the crowd 18 (drifter bullets 7, shoals 3), events 2. Not tuned further (the coordinator's word).
- **Every other way in or out of a run** (screens.md): the pause's RESTART is RETRY's drop-in (the pause screen leaves on the
  next beat, its dim lifts from 0.2 over the wait and its filter opens over it; the run's clock comes back on the band's whole
  bars as a count-in's would, `leavePause`); QUIT RUN is home (as since b12310b); co-op's HOME and LEAVE RUN are home (the screen
  leaves in its beat while the session runs, then the session ends and the title builds from the next bar line of this
  device's own song, `homeLeaveCoop`); a peer whose host has gone (seated: in the lobby or the run) goes home at once with
  `THE HOST LEFT` under PLAY for two bars (it went to the join screen); the lobby's START is the drop-in, the lobby fading as the
  title's controls do (over the wait's first beat). The transitions' clock in co-op is the song unheld by a pause
  (`netSongRun_`). Strips: `captures\transitions\strip_m6_pause_restart_sheet.png`, `strip_m6_pause_quit_sheet.png`.
- **The band's worst case** (`--band max`, `content_sound.cpp`'s table): Split 4 and Double-Time 2.
- **The upgrades' worst case on the Moto** (shared phone; `bench_play --drop 10.1 --nohits --tier low --loadout max`, 60 s,
  `captures\bench\ZY22BXMJG3-bench_play-20261114-103235.json`): 3,594 frames, missed 0 on the panel (the M5 part 3 run: 10,
  0.28%); present intervals p95 17.7, max 19.3 ms (the CPU-side line fails at 16.9 as it always has; the panel passes); GPU
  total 12.7 / 14.0 / 16.4 ms (p50 / p95 / max; scene 3.7 / 5.2 / 7.6; it was 12.1 / 13.8 / 15.6, scene 3.2 / 3.9 / 4.7); the
  sim tick 0.67 / 1.19 / 3.1 ms with up to 903 enemies; the band (its own game mode with the loadout) callback load 0.15 / 0.21 /
  0.26 / 0.57 (p50 / p95 / p99 / max of the 5 ms burst), 159 voices at the peak, 17 steals, no underrun (worst callback 1.35
  ms). Note: with two-cycle Tracks the window does not stay in Track 10's first Drop: the Reprise falls inside it and the run
  ends in the loop's Track 11.
- arena.json's events text (two cycles) is data text the code never read: nothing to change.
- **The Arpeggiator's blocks** (design 2fbb4b2: 72 wu wide at 340 wu/s, were 90 at 420): data only. Everything reads
  `ArpData::blockWidth` / `blockSpeed`: the blocks' life down their lanes and their hit (`world_arpeggiator.cpp`), the view, the
  bell as a block passes the ship's row (`pending_sounds.cpp`, its delay from the lane distance over the speed, each frame), the
  bots' dodging; the chart and the Nova's clearing do not depend on them. Only comments and two fallback defaults said 90 and
  420 (now the data's); the test's data line reads 72 and 340. **The assist bar** (seeds 1-20, 60 minutes, the same run):
  **19 of 20**; the one run that ended inside the ten Tracks ended against the Arpeggiator (19 of 20 first meetings won, 5.9
  hits a fight, 5.1 of them its beams).

### F and G. Double-Time two ranks again; Split a lane a rank (eb10554; GAME_DESIGN 6.2 rows 1-2, screens.md's words)

- **F**: Double-Time's cap is 2: the fire grid 1/16, 1/16 triplet, 1/32 (`World::leadGrid` and the band's `leadGridIndex`
  already capped the index at 1/32, so the Surge stays capped there; Hyper halves what is left). Words `FIRE RATE X1.5`, the
  second rank `FIRE RATE X2`.
- **G**: Split's cap is 4, a lane a rank: `splitLanes(stacks) = 1 + stacks`, 2, 3, 4, 5 lanes. Neighbouring lanes stay
  `kSplitFanDeg` (5 degrees) apart and the fan stays centred on the aim, so an even count straddles the aim line (two lanes at
  +-2.5 degrees, four at +-2.5 and +-7.5; no lane down it). `volley` places lanes by half-lanes from the aim line (`off2`).
  Words `+1 SHOT LANE`. The band already played one harmony note a Split stack (conductor.json's degrees 3rd, 5th, octave,
  7th, four of them): no audio change. `kMaxShotsPerStep` (25, Split's 5 lanes x Prism 5) is unchanged.
- **Even counts, my readings** (the rules were written for odd fans): the thinning's "lanes beyond the middle three" are those
  more than one lane's spacing off the aim line (four lanes: the outer pair; two or three: none); above the second cap "the
  middle lane fires alone" is the pair either side of the aim line; Accent's lance and Heavy Step's orb take the place of the
  lane on the aim line, and an even fan has none, so they fly down the aim line between the middle pair and every lane fires
  beside them. Helix Array is fused at Split's cap (5 lanes, odd: pairs at 80 and 53 wu and the middle lane straight, as
  before); its strand levels are counted from the outside in, so an even fan would ride the same way.
- Pictures: `split2` ... `split5` (`captures\split\`, a still each and `split_sheet.png`, the four fans one above the other;
  `review-m3b.ps1 -Config dev -Only split2,split3,split4,split5`). The M5 upgrade pictures that held Split (`up_wave`,
  `up_helix`, `up_kills`, the Boost pairs) take twice the stacks, the same fans as before.
- Tests: the caps, lanes and words; each fan's angles and whether a lane lies on the aim line; Heavy Step's orb beside four
  lanes; Split 4 x Prism 25 shots; the grid 24, 16, 12 and Hyper's 6; the shot pool (four players, Split's 5 lanes,
  Double-Time 2, Prism, Hyper, 174 BPM) and the thinning now with four lanes (an even fan's outer pair at half reach).
- Left as it is (the coordinator's word): the no-trap table's yardstick (`--sim picks`: "Split 1 and Double-Time 1" as the
  reference build, the crowd gain measured "against a Split stack's", which is now one lane, not two): for the balance pass.
- Noticed, not changed: the band's own worst case (`--band max`, `content_sound.cpp`'s table, the sound engineer's) still says
  Split 2 and Double-Time 1 (now 4 and 2: four harmony notes on a 1/32 lead); `--loadout max` (the upgrades' worst case on the
  Moto) now takes Split 4 and Double-Time 2 by itself and was not measured again; the careful bot's line (a fight at least half
  the page's shorter length) and the loop were not measured again.

### The transitions (b12310b; screens.md "Transitions (M6)")

- **The drop-in** (`src/game/play_transitions.cpp`): PLAY is answered at once (the confirm chord, a light haptic tick, its
  pressed look kept, four dots under its label counting the beats left) and the run starts on the first bar line at least
  two beats away (`requestRestart`'s rule already: two to six beats). Over the wait the title's pieces leave at their beats
  (CO-OP, SETTINGS, the best score and the note over the first beat; the tagline and beat dots over the last, the waveform
  flattening), the attract run dissolves (`ViewInput::runLight` scales the run's enemies, bullets, shots, Shards, sparks and
  pools; the lattice and the background stay; its ship is gone, still and out of harm, and nothing new spawns), and nothing
  takes a tap. On the downbeat the logo's letters streak out to both sides, the straight waveform snaps out as a ring of
  light from the ship to the frame's corners and the lattice takes one impulse of 140 there (`PlayView::dropRing`), PLAY's
  outline shrinks into the ship as the halo, the title's ship glides to the run's ship, and the run's light comes up to full
  over the beat; the HUD fades in over beat 2 (`setHudFade`: everything `hudColour` gives, its glows too; the Track name card
  writes itself as always). Back in the wait cancels: PLAY lets go, everything fades back over a beat, the filter opens.
- **RETRY** is the same drop-in from game over: the game-over screen leaves on the next beat in one beat (the score and RUN
  OVER streak out to both sides, the four numbers, ENDED BY, the set list and the band fade and sink 30 px), the dead run fades
  out over the wait, and on the downbeat the ship draws itself at the centre with its halo. **Co-op**: when the host's START
  or RETRY schedules the run, every peer plays the drop-in from its own screen (the lobby or the co-op game over fading out)
  on that bar line.
- **Home** (HOME, and QUIT RUN from the pause): the tapped control keeps its pressed look, RETRY greys; the screen leaves in
  the next beat and the run behind fades out; on the first bar line after that the title's run starts (`requestRestartAt`),
  empty, and the title builds itself over its bar: the waveform outward from the centre in a beat, the logo written letter by
  letter over two, the tagline and beat dots, then PLAY (taking taps from then), CO-OP and SETTINGS, the best and the note on
  the last beat; the palette cross-fades from the run's to Ultraviolet over that bar; the run behind fades up to the title's
  light over the bar after. A tap on PLAY while the title is still building starts the drop-in from where the title is.
- **The music**: the band's own pieces, nothing added to `src/audio`. The wait's low-pass is the pause's filter (`SetMuffle`
  sent with 700 Hz and the wait's length as its ramp; home holds the bed at 900 Hz and opens it over the title's bar), the
  riser the Comet's whoosh (`Recipe::Whoosh`, stretched to end on the bar line, at most a bar long), the drums out for the last
  beat the fights' `CutBed` (drums, hats, clap, snare), the crash the Drop's impact (`Recipe::Impact`) on the downbeat (and on
  home's bar line); the run's first Groove from its first kick is `conductBand`'s new-run path as before. One thing the band
  cannot do: stop a sample, so Back lets the riser finish under the opening filter.
- **The grace in the director** (`World::directorBar`, `spawnFormation`; `tuning::kGraceQuietBars` 1, `kGraceBars` 2,
  `kGraceClearWu` 600, quoted): nothing spawns in a run's first bar (the crowd's first formations come on the second bar line,
  from the frame's edges), and for its first two bars every hostile formation keeps 600 wu from every ship (260 after). Every
  run has it, the title's too. Test: no spawn in the first bar, no arrival within 600 wu in two.
- The first-run hint waits for the run's second bar. Reduce flashing leaves out the ring and the lattice impulse; the fades
  stay. Strips (`captures\transitions\`, dev, off screen, a frame a beat from a quarter beat after the tap's next beat):
  `strip_m6_drop_title_sheet.png`, `strip_m6_drop_retry_sheet.png`, `strip_m6_home_sheet.png`.
- Not done: co-op HOME / LEAVE RUN keep their cut back to the title (`leaveCoop`); the pause's RESTART keeps its own way (the
  page names PLAY, RETRY and HOME).

### C. The assist at half rate (27077b6; GAME_DESIGN 2, 7.6), measured on the final shape (A, B, D, E, F, G and the grace)

- **Built**: the assist (aim idle) fires with `fire 2`, every second step of the fire grid (`PlayerController`): half the
  build's rate, Double-Time included (the sim's `spawnShots` and the band's lead rule already skipped odd steps for 2). A thumb
  on the aim stick, and its 400 ms after, keep the full rate; Riffs, Novas and the rest keep theirs. Co-op: each player's own
  (the pose carries it). The novice and the assist bots fire through the assist, so at half rate; the careful and careless bots
  aim. Test: the assist's pose is 2, an aimed one 1.
- **The retune** (`--sim crowd --policy novice --nopace`, 40 seeds throughout: Tracks 1-3 from the run's start over 14
  minutes, Tracks 4-10 `--handover N` over 60; files `captures\half\r7_*` to `r17_*`). Split's and Double-Time's new ranks cut
  the novice's damage again (Longwave's eight bars 206 to 183, Event Horizon's 2,005 to 1,108 at the same health), so every
  fight came down further than half (`tuning.h`, the factor and in brackets the play test 3 factor x the retune's): Kaleidos
  0.605 (x 0.5), Longwave 0.1925 (x 0.25), the Choir 0.189 (x 0.15), Event Horizon 2.178 (x 0.2475), Polyrhythm 0.4165
  (x 0.1225), Feedback 0.2925 (x 0.1625), Cadence 2.113 (x 0.2625), Arpeggiator 1.863 (x 0.135), Fata Morgana 1.7325
  (x 0.1125), the Reprise 0.914 (x 0.1625); the mini-bosses x 0.3 (0.234, 0.1575, 0.105, 0.216, 0.36, 0.39, 0.45, 0.204,
  0.1275, 0.3, 0.315 for Weaver Prime at x 0.21, 0.39), which keeps the novice's kills before they warp out about where they
  were (the mini-boss's kill now drops the Extra Life from Track 2). The lines (GAME_DESIGN 7.6, scaled to 40):

  | | line (of 40) | novice |
  |---|---|---|
  | Track 2's boss reached | 32 | 38 |
  | Kaleidos won | 36 | 40, 55 s |
  | Longwave won | 32 | 32, 83 s (at most 150) |
  | the Choir won | 32 | **29** (34 met), 34 s: misses by three |
  | Event Horizon / Polyrhythm / Feedback won | 24 each | 27, 36, 26; 75, 20, 63 s |
  | Cadence / Arpeggiator / Fata Morgana / the Reprise | a won fight at most 1.5 x the page's longer length | 28 of 39, 28, 26, 35; 91, 69, 41, 114 s (at most 150, 165, 180, 210) |

  The Choir misses because of Longwave, not its own health: only 34 runs of 40 meet it within the 14 minutes (Longwave's
  eight losses end first runs), and it wins 29 of those 34; measured from Track 3's start (`--handover 3`) it wins 37 of 40.
  Longwave's own losses stopped following its health (x 0.3 of play test 3: 32 of 40, x 0.25: 32, x 0.2: 31); the straddling
  even fan costs the novice about 14% of its damage on Longwave (a build with one lane on the aim line: 124 against 109 in eight
  bars, the same 32 wins), so it is not that either. Twenty or forty seeds are noisy here: from Track 4 on a run's build comes
  from the careful bot's run before the handover, which changes with anything upstream (Event Horizon went 31 to 24 of 40 and
  Feedback 27 to 22 between two runs at the same health).
- **The pace's references** (`kBossPaceNovice`, the novice's eight-bar damage a player from the same runs): Kaleidos 272,
  Longwave 126, the Choir 204, Event Horizon 985, Polyrhythm 1,030 (at its last health most novices kill it inside eight bars:
  745 in the 19 fights that lasted them), Feedback 509, Cadence 1,615, Arpeggiator 2,063, Fata Morgana 5,206, the Reprise
  1,292.
- **The bar, the assist bot over the whole run** (`--policy assist`, 60 minutes, pace on): **missed**. Seeds 1-20: 15 of 20
  finished the ten Tracks; seeds 21-40: 19 of 20 (34 of 40, the line's 38). Not the crowd: the six first runs that ended inside the ten
  Tracks ended in fights (Cadence 3, Polyrhythm, Feedback, Fata Morgana one each: the first meetings' losses), and over the
  hour's retries the hits that took a last cell were the fights' 27 of 36 and 18 of 34. Two rounds before (higher health there, the earlier pace
  references) gave 19 and 16 of 20: the bar sits about 17 of 20 and moves two or three either way with anything upstream. One
  cause is the pace rule: the assist bot out-damages the novice's references by more than the dead band (its paces 1.2-1.75
  from Track 4, 2.1-2.8 on Longwave and the Choir), so its fights are tougher than the novice's by design. Left for the lead:
  whether the assist alone should ever raise a boss's pace.
- **Trios in the first run** (level-ups + the boss's, a667116): the novice T1 1.0+1.0, T2 5.0+0.8, T3 4.9+0.7, T4 4.0, T5 2.5,
  T6 2.8, T7 2.3, T8 1.8, T9 1.2, T10 1.8 (Tracks 4-10 from the handover, the boss's not reached); the assist bot T1 1.2+1.0,
  T2 5.4-6.0+1.0, T3 5.0-5.4+1.0, T4 5.4+1.0, T5 3.0-3.2+1.0, T6 3.3+1.0, T7 2.5-2.8+0.9, T8 1.9-2.0+1.0, T9 0.9-1.1+1.0,
  T10 1.7-1.8+1.0.
## The night of 2026-11-14: easier fights (design commit 1bcc897) and M7's release preparation

- **Design cb2edba** (the lead's two follow-ups): the font's `;` and `@` load and draw (62 glyphs, 253 segments; the loader
  has no fixed glyph count: an ASCII table of 128 and room for 1,024 segments), retaken in `captures\m7\m6_credits.png` (the
  SDL licence's `SLOUKEN@LIBSDL.ORG` and `MISREPRESENTED; YOU`; crop `m6_credits_font_crop.png`); libc++'s credit line is the
  page's `LIBC++  (APACHE 2.0, LLVM EXCEPTION)`. RELEASE_CHECKLIST items 9 and 10 resolved; 8 and 11 stay by the lead's ruling.
- **On the phone** (the Moto, the dev build, installed 01:47, not launched; the launcher was in front): commit a0e7ea8 (A was on
  it from b6bcc8f at 01:28). `scripts\test.ps1`: all passed (audio 2,716 checks, sim 5,880, save 60, determinism, net 131,368, both
  soaks: 0 census mismatches).

### A. Easier fights (GAME_DESIGN 5; horizon.json, polyrhythm.json)

- **A boss's arrival tops a player up to three cells** (`kBossEntranceCells` 3, was 2), the arcs drawn as Regen's.
- **A boss pays for its phases**: at each phase change one Cell for each player at the boss (`kBossPhaseCells` 1), drifting to
  its player at 240 wu/s while that player is hurt (`kBossPhaseCellDriftWu`, chosen; `CellPickups::homing` 2), homing as any
  Cell inside the magnet (`World::bossPhasePay`, after `updateBoss`: `BossState::phasePaid` against `phase`, so a phase the
  host decides is paid when its verdict lands). Each device drops its own players' Cells, as the boss's payout does (no
  protocol change). Mini-bosses have no phases to pay for. Kaleidos, Longwave and the Choir pay three, the others two.
- **Polyrhythm's burning link burns on beat 4 alone** (`polyLinkBurn`: the bar's last `burnBeats`, from the data's 1) and
  **Event Horizon's jets one beat** (the data's `burnBeats`, read as before).
- Tests: the entrance rule at three (two arcs), the phase Cells (Kaleidos: a pair down, a Cell drifting to the ship; a second
  pair, a second Cell), the link's beat 4.
- **The whole-run sets** (seeds 1-20, 60 minutes; `captures\m6bal\fights_*.txt`): **the novice finishes the ten Tracks in 6 of
  20** (7 before; the same seeds play differently), 35 of its 42 run-ending hits in fights; what ends the rest: Polyrhythm
  (14 of 38 fights lost, 11 beam hits a fight), the Choir (6 lost of 57), Fata Morgana (4 of 15), Longwave (3 of 65), Event
  Horizon (3 of 46), Feedback (3 of 23), one each Kaleidos and Arpeggiator; the first runs' deaths in Tracks 5 (4), 4 (3),
  2 (2), 9 (2), 3, 6 and 8 (one each), the fourteen that did not finish.
  **The careful bot finishes 18 of 20** (its two first deaths in Tracks 5 and 9), 9,830 kills a minute. No further tuning
  (the lead: the owner's play decides the next step).

### B. Release preparation (PLAN M7, without the long soak)

- **B1, sanitizers** (`scripts\sanitize.ps1`, `build\desktop-asan`: llvm-mingw clang's AddressSanitizer and UBSan, nothing
  downloaded; logs in `captures\asan\`): all clean after one fix: a negative float converted to unsigned (the Brood Mother's hatching cue, `world_miniboss2.cpp`); ENet's own offsetof through a null pointer and `1 << 31` left as fetched (UBSan off for ENet in that build). Run: the audio, sim, net and save tests, the calibration check, determinism, a 3-minute bot run, a fight start in Tracks 4, 7 and 10 (Event Horizon, Cadence, the Reprise: fought and won), the 3-minute net soak. The sanitizer build leaves out the counted `operator new` (`RF_SANITIZE`: the runtime replaces it) and links the tools dynamically (the ASan runtime is a DLL). Not run: LeakSanitizer (not on Windows), the game's binary.
- **B2, crash-safe saves**: `core/save_file` writes every kept file to `<file>.tmp`, flushes it to the disk (`fsync` /
  `_commit`) and renames it over the old one in one step (POSIX `rename`; Windows `MoveFileEx` with replace and
  write-through: before, the old file was deleted first). Settings and hints, high scores, the tier cache (was written in
  place), the baked audio and the sync calibration use it. Loading: settings and high scores by path in `game/save_data.cpp`
  (no SDL; every value held to its range, the hints to eleven bits, a high score's Track to 1-999, broken lines skipped); the
  tier cache already refused a damaged file; the baked audio now also checks every index its tables hold against the samples
  it brought (`SampleBank::cacheTablesValid`) and resets its tables after a failed load (`resetTables`); the sync calibration
  ignores an offset outside ±500 ms. `raveform_save_tests` (60 checks, in `scripts\test.ps1`): for each file a round trip, a
  missing one, one cut short, damaged values and random bytes; the atomic write over an existing file, over a stale
  temporary, a failed write keeping the old file. Touches src/audio (bake.cpp, bake.h, band.cpp: the lead's list named the
  baked audio).
- **B3, the release variant** (`build-android.ps1 -Config release [-Bundle]`): native Release with `-g` (the unstripped
  libraries kept as `app-release-symbols.zip`, 13 MB; the APK's stripped), not debuggable, `minifyEnabled false` (SDL's
  Java glue is called by name from native code), the `args` extra ignored when the app is not debuggable
  (RaveformActivity), signed with the debug key (no keystore created). **Size: 7.01 MB** — libmain.so 2.90 MB (41.6%),
  libSDL3.so 2.04 (29.2%), libc++_shared.so 1.31 (18.8%), liboboe.so 0.32 (4.6%), resources 0.31 (4.5%, the icon's layers
  most of it), dex 0.10 (1.4%). The App Bundle: 6.18 MB. Nothing taken out (the development-only code is about 0.14 MB:
  RELEASE_CHECKLIST, open item 11).
- **B4, Play readiness**: `docs/RELEASE_CHECKLIST.md` (the open items first). Changed: Wi-Fi is no longer implied required
  (the manifest says not required). Checked: targetSdk 36 / minSdk 29; every native library 16 KB aligned (no flag change);
  six permissions, each with its reason; nothing leaves the local network; the photosensitivity notice.
- **B5, the credits** (screens.md "Credits (M7)"): `CREDITS` the last row of SETTINGS (LOOK, bottom left); the screen
  (`game/screens_credits.cpp`) scrolls by drag, sticks, arrow keys or W / S, BACK top left; RAVEFORM, BUILT ON, LICENCES (each
  in full at cap 10, wrapped), PLAY SAFE (the notice's lines, now shared: `kNoticeLines`). `cmake/Credits.cmake` reads the
  licences from the fetched sources at configure time into `credits.inc`; the configure walks the game binary's link graph and
  fails on a linked library with no entry (or no licence text). Linked: SDL, Oboe, Android Frame Pacing, ENet, volk, the
  Vulkan headers and libc++ (not in screens.md's list; RELEASE_CHECKLIST item 9). The HUD's capsule budget is 16,384 (was
  8,192) for a screenful of licence text. Pictures: `captures\m7\m6_credits.png`, `m6_credits_licences.png`.
- **B6, the thermal check**: the Moto (shared), `--drop 10.1 --drop-hold --nohits --tier low` for 300 s: 0 missed of 17,968 frames over the run and 0 of 3,593 in its last minute alone (worst interval 23.5 ms), main thread p99 5.3 ms, GPU P95 14.3 ms, 324 enemies in view on average (775 at most); battery 21.3 to 22.0 °C (`dumpsys battery`), thermal status 0 throughout.

### Decisions taken alone

- The phase Cell's drift speed (240 wu/s) and its keeping its drift when its player is at full health and hurt again.
- Where the licence text of Oboe, Frame Pacing and the Vulkan headers comes from (the toolchain's Apache 2.0 text, the
  headers' own notice) until their own files are fetched; libc++ credited; CREDITS on the LOOK tab's last row.
- ENet's own undefined idioms are left as fetched (UBSan off for ENet in the sanitizer build; ASan stays on it).
- The release build keeps the development-only code (2% of the APK); `minifyEnabled false`.
- `--drop-hold` (benchmarks): with `--drop T.C`, that Drop never ends (`DirectorState::holdDropCycle`), for the thermal check.

## M6, the first chunk (the lead's design commit cfd31c0: what M5 left that a player notices, then M6's parts that need no decision) — 2026-11-14

### A. What M5 left

- **A1, the boss hit flash is a white copy of the part's own shape** (GAME_DESIGN 7.6). While a weak point's 60 ms flash
  runs, the boss is drawn with a tap on the scene batch (`CapsuleTap`, `src/gfx/capsule.h`: a CPU copy of each line's ends and
  width; the batch itself is mapped GPU memory and is never read back) and its glyphs are counted from where it began;
  `PlayView::drawBossHitFlash` then copies, for each part hit, every line of the boss with both ends inside the part's circle
  (x 1.35) in white at 2.0 and 0.7 of its width (at least 0.5 ref px), and every glyph centred in it as a hull-only white copy
  at 2.0. The circle's outline is drawn only when nothing is found. Kaleidos keeps what it had (the pair's arms, its core ring:
  already its own shape). Two hits on one part inside 60 ms flash it once. Reduce flashing: 0.9 (it was so before).
- **A2, everything a player fires hurts an arena body with health** (a Vault, the Rift, the gold Vault, the Comet, a carrier):
  every area and line the upgrades deal through (`areaHit`, `lineHit`: Accent's lances, Beam, Shockstep, Bar Line, Standing
  Wave, Black Hole, Minelayer, Singularity, Comet, Supernova Rounds, Lancer, the Downbeat Cannon, Sentinel's and the others'
  through them) now also calls `arenaArea` / `arenaLine`; Kick Pulse (`arenaArea`), the Sweeper (`arenaSweep`, its swept
  arms), the Orbitals' lit shards (`arenaArea` each) and Pulse Ring (`arenaRing`) hit them directly; a client's hits are boss
  claims as before (`World::damageBody`). Not covered: Colossus's body, Chain Storm's and Storm Coil's bolts, Last Light.
- **A3, a mini-boss stays until it is dead or 16 bars from its arrival, the Drop held for it**: this was already the rule as
  built (it arrives on the middle Drop's downbeat, the Drop's 8 bars plus `kMiniHoldBars` 8, warping out on that bar line); a
  `static_assert` in `world_miniboss.cpp` now ties the two (the Drop's length + the hold = 16) and the picture
  `m6_mini_stays` shows Alpha Hound 13 bars after its arrival in the held Drop.
- **A4, the novice's ten Tracks** (seeds 1-20, 60 minutes; files in `captures\m6bal\`). The table of what took the novice
  outside the fights from Track 4 on put the crowd's bullets first (Drifter B's 139, then the events and blasts 103, Carom
  bodies 46, Shoal 35, Weaver 30); the hits that **ended** a run were the fights' (38 of 48). Two rounds, the crowd only:

  | | novice finished | its hits from Track 4 on: drifter bullets / events / carom bodies | runs ended in fights | careful finished | careful kills a minute |
  |---|---|---|---|---|---|
  | before | 5 of 20 | 139 / 103 / 46 | 38 of 48 | 20 of 20 | 10,320 |
  | round 1: the crowd's bullets x 0.8 (`kCrowdBulletSpeedScale`; the fights' as their pages say), a lost cell eases the budget by 0.2 to a floor of 0.6 (`kAssistEaseStep`, `kAssistEaseMin`; GAME_DESIGN 8's 0.1 and 0.8 stay for the rise) | 6 | 191 / 113 / 41 | 46 of 54 | 20 | 10,455 |
  | round 2: from Track 4 on a shooting species is picked at half weight (`kShooterEaseTrack` 4, `kShooterEaseWeight` 0.5) | **7** | 97 / 113 / 42 | 32 of 41 | 18 | 9,693 |

  **Where it landed: 7 of 20** (stopped after two rounds, as asked). The crowd is no longer what ends the novice's runs: the
  first deaths fall in the fights of Tracks 2, 5 and 6 (Longwave, Polyrhythm's beams: 9 beam hits a fight, Feedback) and
  Fata Morgana, which the instructions keep out of this pass. The careful bot's run is not empty (the same crowd in view, 7%
  fewer kills a minute; two of its twenty first runs ended in Tracks 5 and 9, both in fights).
- **A5, as time allowed**: Phasing's every other hull segment hidden off the beat (look.md 4): the hull's even segments drawn
  as lines in the hull's field (`PlayView::hullEveryOther`), the white copy likewise, the ornament and core hidden with them
  (was: the whole hull at 35%). Left: Swift's x 1.3 turn, Wyrm heads and Tether pairs as elites, Fata Morgana's wind on the
  lattice, the "family met" rule (every family has debuted by Track 6: it would choose nothing), the echoes of Riffs that fire
  nothing.

### B. M6

- **B1, first-run hints** (screens.md "First-run hints (M6)"): the eleven lines in world text 70 wu over the ship (cap 22,
  white on the priority layer with a dark rim), two bars up, fading over the last beat, each once ever on a device
  (`settings.txt` `hints`), one at a time, at least four bars apart, never in a fight or within 1.5 s of a Trio's take, never in
  co-op for a player who has ended a run (`runs`); a hint whose moment passes waits for the next (`Play::hintsTick`). The
  touch lines on a touch device, the keyboard's on the desktop. `RESET HINTS` under SETTINGS, CONTROLS.
- **B2, ENDED BY** on the game over: what took the last cell as the game names it (`Play::causeName`; the sim keeps the cause
  of each hit: a species, a boss, an event, a Volatile blast; a bullet names its shooter: `Bullets::src`); nothing for a run
  quit or finished.
- **B3, SET COMPLETE**: when the tenth Track's boss falls, SET COMPLETE (cap 44, gold) with the score in the Track card's place
  for two bars, before ENCORE; the same words where RUN OVER stands on that run's game over; a gold star after the title's BEST
  line (`highscores.txt`: a sixth field).
- **B4, the app icon** (`scripts\make_icon.py`, reading `design/glyphs.json`, `look.json` and `palettes.json`, writing nothing
  under design/): the adaptive icon's background (the Ultraviolet ground and the lattice at rest, bulging under the ship),
  foreground (ship and halo with their bloom at half strength) and monochrome layers at every density
  (`android/app/src/main/res/mipmap-*/ic_launcher_*.png`, `mipmap-anydpi-v26/ic_launcher.xml`; the placeholder drawable is
  gone); `captures\m6\icon_512.png` (+ `_round`, `_monochrome` for review). The name stays `RAVEFORM`.
- **B5, UI scale** (SETTINGS, LOOK: 90 / 100 / 115 / 130%, `settings.txt` `ui_scale`): HUD text and the touch controls (laid
  out again at once). **Reduce flashing**, checked against M5's additions: it already covered the beams and lethal beams, the
  Nova (the Extra Life's too), the boss deaths, Event Horizon's and Polyrhythm's flashes, Meteor Rain, the Volatile blasts,
  the Eclipse; it did not cover, and now does: the boss hit flash (0.9), Storm Cell's strikes (0.9 / 0.8), Arc's, Chain Storm's
  and Storm Coil's bolts (0.8), Freeze Frame's shatter (one ring), the HUD's boss bar flash.
- **B6, haptics** (HAPTICS setting's strength; `Play::pulse`): the ring breaking medium, an Extra Life spent heavy, a boss's
  death heavy, a mini-boss's arrival medium; a weak point's hit none.

### Decisions taken alone

- The flash's circle x 1.35 and width 0.7 (A1); the crowd's levers and their numbers (A4); the Phasing elite's ornament and
  core hidden with the hull's odd segments (A5).
- The icon (B4): "the ship's extent 44% of the icon" read as its span (nose to tail; the halo then spans 54%, inside the 66 dp
  every mask keeps); the lattice at the reference tier's 20 wu, the ship's pool of light left out (not in the page's list); the
  bulge a radial push of at most about 4 wu over 26 wu; the ground centred; the foreground's alpha its brightest channel after
  the tone map (the light it adds over the dark ground); the monochrome layer the same, with the faint wide bloom (under 4%)
  cut.
- Pictures and fixed-step runs no longer write `settings.txt` or `highscores.txt` (`Play::endRun`): `screen_game_over` and the
  M6 game-over pictures were replacing the PC's files.

### Checks

- **On the phone** (the Moto, installed 01:12, not launched; the launcher was in front): commit f625bbe.
- `scripts\test.ps1`: all passed (audio 2,716 checks, sim 5,875, determinism, net 131,368, both loopback soaks: 0 census
  mismatches). One test followed the crowd's slower bullets (`testPlayTest2`'s loop bullet speed x
  `kCrowdBulletSpeedScale`).
- Pictures (`captures\m6\`, 2400x1080 with `.small.png` copies; Debug with 0 validation messages, the fights in dev): every hint
  line `m6_hint_0` ... `m6_hint_10` and `m6_hints_sheet.png` (the eleven side by side); `m6_ended_by` (ENDED BY HOUND);
  `m6_set_complete` (the card in the run), `m6_set_complete_over` (the game over's title); `m6_title_star` (the BEST line's
  star); `m6_uiscale_90`, `m6_uiscale_130` (the HUD and the touch controls); `m6_mini_stays` (Alpha Hound 13 bars after its
  arrival, the HUD's Drop held); the hit flash on three fights, each also twice as large (`m6_flash_longwave`,
  `m6_flash_choir`: a singer white, `m6_flash_alpha`: the hound's hull white; the horde, bullets, sparks and Shards not drawn
  in these three so the part reads); `icon_512.png`, `icon_512_round.png`, `icon_512_monochrome.png`.
- **The Moto, one run of 60 s** (A1 draws more in every boss fight; the densest is Track 10's last Drop, Kaleidos Reprise:
  `bench_play --drop 10.3 --nohits --tier low`, shared phone, the panel at 60 Hz): 0 missed of 3,594; main thread p99 5.3 ms
  (M5's run 4.8 ms); GPU P95 14.2 ms (14.5); 180 enemies in view on average, 292 at most; audio 0 underruns, heap 0. The
  panel's grade PASS; the present call's own interval P95 17.7 ms as before (Swappy's pacing).

## M5 — done (2026-11-14): the end of the milestone

- **On the phone** (the Moto, installed 00:21, not launched): commit 2520297 (play test 3 items 1-8 61a7f09; the m4-net and
  m5-audio merges; b1d4a2f; the balance pass). Protocol 0x5246050A.
- `scripts\test.ps1`: all passed on 2520297 (audio 2,716 checks, sim 5,875, determinism, net 131,368, both soaks).
- Pictures of what changed (`captures\playtest3\`, Debug, 0 validation messages): the guard ring broken and relighting (and
  three times larger), the Extra Life's orb with one held in the HUD and its use, a Vault's name, a hit on Kaleidos's weak
  point (strip), Split's five lanes, Track 1's phrase bar.
- **The Moto, three runs of 60 s** (`bench_play --tier low --nohits`, shared phone; the panel ran at 60 Hz in all three, voted
  60 by the system: someone's setting or the phone's): Track 10's first Drop (`--drop 10.1`, the densest crowd: 187 enemies in
  view on average, 648 at most) 0 missed of 3,594, main thread p99 4.9 ms, GPU P95 14.3 ms; Track 10's last Drop (`--drop 10.3`,
  Kaleidos Reprise) 0 missed, main thread p99 4.8 ms, GPU P95 14.5 ms; the upgrades' worst case there (`--drop 10.1 --loadout
  max`: 20,000 sparks a frame on average) 0 missed, main thread p99 6.7 ms, GPU P95 13.8 ms. Audio 0 underruns, heap 0 in all.
  The panel's grade PASS in each; the present call's own interval P95 17.7 ms (the CPU-side line, Swappy's pacing at 60 Hz).
- **What M5 leaves unbuilt**, in the order a player would notice: a boss hit's white flash is an outline of the part's hit
  circles, not a copy of its hull; the Downbeat Cannon's crowd figure (x 3, its quoted shape); the careful bot's Choir and
  Polyrhythm shorter than half their pages (they die before the pace grows: the ruling); the careless bot never passes Fata
  Morgana; Phasing's every other hull segment hidden; Swift's x 1.3 turn; Wyrm heads and Tether pairs as elites; Swift on the
  mini-bosses but the Tether and the Well; the "family met" condition for Tracks 9 and 10's mini-bosses; Fata Morgana's lattice
  whipped along the wind and tinted in the oasis; Riffs other than shots and the Nova on a Vault; Feedback's and the Reprise's
  echoes of Riffs that fire nothing; the mini-bosses warp out of 5-30% of the novice's first meetings (the Drop is 8 bars: a
  win takes 10-18 s, a loss is a fight it did not engage).

## Play test 3 (the lead's design commit a008b71; GAME_DESIGN 2, 4, 5, 6.2-6.4, 7.6; arena.md, screens.md) — 2026-11-13

### What was built (items 1-8)

- **Tracks half as long** (4.1, 4.2): every Track in the Short form (8 / 4 / 8 / 4); the run's Track 1 two cycles (48 bars),
  every later Track three (72), the loop's too (`formOfTrack`, `cyclesOfTrack`); Standard is retired (the table keeps its row,
  equal to Short, for the band's own copy: `src/audio/engine.cpp`'s two literals changed with it, nothing else in src/audio;
  `Play` asks the band for Short always). Bar-written things keep their bars (the mini-boss's hold 8, the elites' spacing).
  An arena event never outlasts its Groove: announced on the Groove's first bar line, one written longer ends with the
  Groove's last (the Elite Hunt's and the Convoy's twelve bars: the Groove's eight, the announcing bar among them; the Rift
  Surge keeps its written Build-to-Drop span). Track 1's Comet comes in its second cycle's Groove.
- **The assist beats the game** (2, 7.6): the assist fires on the full grid (`PlayerPose::fire` 1; 2 is no longer set); in a
  boss or mini-boss fight it aims at the boss unless an ordinary enemy is within 250 wu (`kAssistNearWu`):
  `World::assistBossAim` takes the open weak point (a part the hit multiplies when the shot arrives, the most first), else
  the nearest part that takes its full share, else armour's share, else the nearest part, each led by the shot's flight
  (Kaleidos: the core through a dead pair's sector, else the nearest resting arm, else the nearest arm). In Feedback's fight
  it rests every second bar (Slapback and Howl); in Regeneration, whose second Echo plays two bars behind, two bars of three
  (decision: with every second bar the Head never opens there). The novice no longer aims (`kNoviceAimLagS`,
  `kNoviceWanderDeg` and its weak-point learning are gone; `kNoviceLearnBars` stays for Fata Morgana's water); new policy
  **assist** (`Bot::Policy::Assist`, `--policy assist`): the careful bot's movement, the assist's fire. Neither phrases its
  fire for the Mimic or Feedback (they never lift a thumb).
- **Caps** (6.2): Split 2 (five lanes), Double-Time 1 (the triplet grid), Fork 1 (forking again is Fractal's). Helix Array's
  two pairs at 80 and 53 wu and the middle lane; the shots-per-step ceiling 25 (5 x Prism's 5); words (Double-Time and Fork
  one line each, Arc "KILLS ARC TO 1 MORE"); tests; the `split7` picture is `split5`. The band's stress loadout
  (`src/audio/content_sound.cpp` `kMods`) is src/audio: the sound engineer moved it to Split 2, Double-Time 1, Fork 1 (c3af625,
  merged).
- **Chains at half, one step** (6.2 rows 13-15, 6.4): Arc 1 / 2 / 3 more within 160 wu, an arc's own kill arcs no further
  (`World::arcing_`; Tesla Bloom's arcs too); Burn spreads within 45 wu once (a fire caught from a death carries
  `kBurnSpread` in `Enemies::burnLevel` and spreads no further; a hit makes it the shooter's own again); Shatter 2 / 3 / 4
  pieces, a piece's kill throws none (`kShotPiece`); Supernova Rounds' blast half the shot's damage (`kSupernovaShare`);
  Freeze Frame shatters at most 12 at a time (`kFreezeChainMax`).
- **Orbitals, a guard ring** (6.3 row 1): shards 14 wu across (`kOrbitalHitWu` 7), 64 wu out, a turn a bar, cutting for 3 an
  eighth note and eating bullets. While every shard is lit a hit breaks the ring instead of taking a cell
  (`World::breakRing`, before Auto Nova: the panic pulse, 1 s of invulnerability, the Chain halved, Crescendo and Last Light
  as for a hit; no budget easing, no cell); the shards go dark and relight one by one, the ring guarding again 8 bars after
  it broke (6 at level 3, 4 at level 5: `kRingRelightBars`); dark shards neither cut nor eat bullets. 3 / 5 / 7 / 9 / 11
  shards; the words of screens.md. The bot's score for it unchanged. Drawn: dark shards faint, a dashed ring while it guards,
  the shards flying when it breaks.
- **Extra Life** (5, screens.md): its own pickup kind among the Boost orbs (`kPickupExtraLife`; never a Boost, no slot, no
  Trio). In each Track of the set list the host drops one from the first kill of the first cycle's Drop, an orb for each
  player who holds none (`DirectorState::extraLifeTrack`; none in the loop, one held is carried in); it lies 8 bars with a
  gold edge chevron. Held: one. Spent on the hit that would end the run, after Auto Nova (co-op: instead of going down): two
  cells, a free Nova (`pressNova(p, true)`: no stock), a bar of invulnerability, half the Chain, `EXTRA LIFE` over the ship
  for a bar (picked up too). The HUD: a small gold ship outline right of the Nova stock. The orb: a gold ship outline in a
  ring, the Boost orb's size.
- **The Vault says what it is** (arena.md): none in Track 1 (arena.json; the loader now accepts an empty structure list);
  from Track 2 `VAULT` over it (cap 22, gold) until its first hit, and a gold edge chevron while it is off screen within
  1,200 wu.
- **Every hit on a boss is seen and heard** (7.6): a hit taking its full share or more: the part white for 60 ms (a white
  outline of its hit circles at 2.0; Kaleidos's pair's arms or its core), six sparks in its hue back along the shot, its
  segment of the health bar flashing (the whole bar's last 40 px, or the pair's / segment's / Voice's pip); armour (a plate or
  below x 1): three grey sparks thrown sideways, no flash. Boss sparks go first into the particle pool and take an ordinary
  request's place when the request list is full (`SparkReq::prio`). Two cues, `BossCue::HitWeak` and `HitArmour`, the
  shooter's device only, at most one of each a 1/32 step with the step's gain in f (1, then x 0.7 within the beat, down to
  0.3); their `pending_sounds.cpp` rows say what is missing (a bright tick in the key, a dull low knock: the glass and a tom
  play meanwhile).
- The pace's dead band is 1.5 (`kBossPaceDeadBand`, the same commit).
- Co-op (src/net, kept small): protocol **0x5246050A**. A Hit's `opts[0]` says 1 the guard ring broke (its tick in `tick`), 2
  an Extra Life was spent; an Extra Life taken is a `kPeBoostTake` of `kPickupExtraLife`; a dropped one is a `BoostDrop` of
  `kPickupExtraLife` + its owner (the validation accepts both).
- Tests: the director's cycle and the middle Drop on the new lengths, the caps (five lanes, the triplet grid), Helix's two
  pairs, the conductor's forms (`audio_tests`: every Track 8 / 4 / 8 / 4), and `testPlayTest3`: the ring (breaks instead of a
  cell, relights one by one, 4 bars at level 5), the Extra Life (taken, spent: two cells, a free Nova, half the Chain, a bar
  of invulnerability; none held, the run ends), Arc one step, Shatter's two pieces, the assist's aim at Kaleidos.

### Acceptance (items 1-8)

- `scripts\test.ps1`: all passed (audio 2,679 checks, sim 5,875, determinism, net 131,366, both loopback soaks).
- **The test soaks now run six simulated minutes** (they were ten): with Tracks half as long, ten minutes reach Event Horizon
  in Track 4, where the clean soak showed 6 census mismatches (fight bars 8-9, the client one or two bodies short). Not this
  batch's: a008b71 itself (the arena data put back to let it load), soaked 17 minutes into Tracks 4-6, shows **435**. Six
  minutes cover the first three Tracks, what the ten covered before. The cause (the network engineer, m4-net 26823dc, merged):
  the pace decision's damage was checked on the wire as a place, so a large one refused the boss message and its packet's
  spawns and verdicts; 17 minutes now show 0 mismatches. The soaks stay at six minutes (the lead: enough).
- Pictures (`captures\playtest3\`, `review-m3b.ps1 -Config dev`): `pt3_ring_broken` (+ `_zoom`), `pt3_ring_relight` (+
  `_zoom`), `pt3_life_orb` (one held in the HUD, one lying), `pt3_life_use`, `pt3_vault`, `strip_boss_hit` (the assist's fire
  on Kaleidos's resting arm), `split5`, `hud_groove` (Track 1's phrase bar).
- A stray edit of the sound engineer's (68 lines appended to main's `src/audio/content_sound.cpp` at 23:08, from its own
  worktree's file) broke the main build; removed (main's file is a008b71's), a copy kept, the lead told.

### The merges and b1d4a2f (the second phone build)

- Merged `m4-net` 26823dc (the census fix above) and `m5-audio` 80f6870 (every fight, arena and elite cue as its page asks, the
  output limiter, a kill's sounds as one family, the boss hit tick and armour knock, the Extra Life's and the ring's sounds:
  docs/PROGRESS_m5_audio.md). Both merged cleanly.
- b1d4a2f: the six mini-bosses whose rule decides whether they can be hurt show their hint line as the bosses do (`bossHint`:
  the Hive, Battering Ram, Brood Mother, Sentry Array, Twin Tether, Deep Well; `Play::bossHintTick` now keys on a hint
  existing, not on a boss); what of a mini-boss's own body took nothing takes a quarter (`prose::kMiniArmour`: the Ram's horns,
  the Sentry Array's domed turrets, on that turret's health; the Brood Mother's dome and the Deep Well's swallowing were a
  quarter already). The Hive's decoy is light, not its body: it still stops a shot for nothing (decision). The assist aims at
  their open weak point through the same tiers as the bosses'.

### 7e, the balance pass (GAME_DESIGN 7.6; `raveform_headless --sim crowd`, seeds 1-20; files in `captures\pt3bal\`)

- **Method.** The novice now fires only through the assist, so first its eight-bar damage was measured with no pace
  (`--nopace`, new), then the bosses' and mini-bosses' health was moved in two rounds of `--health` factors from the fights'
  starts (Tracks 1-3 from the run's start, 60 minutes; Tracks 4-10 `--handover N`), toward the novice's lines and its fights
  taking about the page's length, and the pace's novice numbers were measured again at the final health
  (`kBossPaceNovice`: Kaleidos 760, Longwave 780, the Choir 1,550, Event Horizon 4,721, Polyrhythm 6,000, Feedback 3,170,
  Cadence 8,860, Arpeggiator 14,400, Fata Morgana 23,250, the Reprise 6,060).
- **Health** (`tuning.h`, the factor on the part 7 tune in brackets): Kaleidos 1.21 (x 2.2), Longwave 0.77 (x 1), the Choir
  1.26 (x 1), Event Horizon 8.8 (x 1.1), Polyrhythm 3.4 (x 2.0), Feedback 1.8 (x 0.9), Cadence 8.05 (x 1.15), Arpeggiator
  13.8 (x 0.77), Fata Morgana 15.4 (x 1.1), the Reprise 5.625 (x 1.25); the mini-bosses x 0.8, 0.7, 0.5, 0.5, 0.4, 0.65, 0.5,
  0.85, 0.85, 1, 0.6, 1 (Alpha Hound to Deep Well; the middle Drop is 8 bars now, the hold 8 more). Where a fight's length and
  the novice's wins pulled apart the wins won: Kaleidos x 2.7 was 17 of 20 (64 s), the Choir x 1.2 14 of 20, Polyrhythm x 2.6
  cost the assist bot two runs.
- **The picks** (`--sim picks --seeds 8`, the table's yardsticks): nothing above 3x but the Downbeat Cannon (x 3.1); Pulse Ring
  2.1, Ricochet 1.7, Heavy Step 1.7, Pierce 1.1 are under the lead's line and were left. The Cannon's damage (the Lancer's 6
  x On-Beat Crit's 8 = 48) is now 18 (`kDownbeatDamage`): its crowd figure did not follow (x 2.7 at 18, x 3.3 at 9 on the same
  seeds: its kills are its quoted shape's, 64 wu the screen's length every bar), its boss figure did (the Choir x 1.32 to
  1.05). **Not brought in**: the Cannon's crowd figure, without changing what the page quotes.
- **The last runs** (the final build; 20 seeds; the novice 60 minutes, the others 45):

  | | novice | assist | careful | careless |
  |---|---|---|---|---|
  | finished the ten Tracks (the first run) | 5 | **19** | 20 | 0 |
  | first death, median | 16.6 min | 42.5 min | 42.4 min | 26.6 min (Track 9) |
  | Kaleidos (won of met, s) | 20/20, 47 | 20/20, 47 | 20/20, 49 | 20/20, 38 |
  | Longwave | 19/20, 94 | 20/20, 68 | 20/20, 50 | 19/20, 36 |
  | the Choir | 18/20, 48 | 20/20, 29 | 20/20, 28 | 20/20, 25 |
  | Event Horizon | 18/20, 142 | 20/20, 74 | 20/20, 69 | 20/20, 88 |
  | Polyrhythm | 13/20, 34 | 19/20, 32 | 20/20, 31 | 20/20, 31 |
  | Feedback | 13/18, 108 | 20/20, 66 | 20/20, 78 | 19/20, 110 |
  | Cadence | 13/13, 98 | 20/20, 80 | 20/20, 75 | 20/20, 67 |
  | Arpeggiator | 13/13, 111 | 20/20, 101 | 20/20, 92 | 19/20, 87 |
  | Fata Morgana | 11/13, 113 | 20/20, 94 | 20/20, 95 | 0/18 |
  | the Reprise | 10/10, 165 | 19/19, 152 | 20/20, 127 | — |

  The novice's mini-boss first meetings: 67-95% won (10-18 s). Its lines hold (Kaleidos 18, Longwave and the Choir 16, Tracks
  4-6 12 of 20; Kaleidos 40 seeds: 40 of 40); Event Horizon from the novice's own run takes 142 s (its page's 1.5 x 80 = 120;
  from `--handover 4` 78 s). The assist bot's bar holds (19 of 20; the one: Polyrhythm in Track 5). The careful bot's line (half
  the page's shorter length) holds but for the Choir (28 s, line 40) and Polyrhythm (31 s, line 50): both die before the pace
  has grown, as in part 7 (the ruling: no second mechanism). The careless bot never passes Fata Morgana (the storm outside the
  oases; the reckless policy ignores danger).

### Decisions taken alone

- The loop's First Light plays three cycles (only the run's Track 1 is two: it is the opening).
- Events end with their Groove counted from the announcing bar line (so an eight-bar event runs seven after its announcement).
- The assist's Regeneration rest (two bars of three), above.
- The ring's break counts as a hit for the Chain, Crescendo and Last Light, not for the director's assist easing.
- An Extra Life's orb carries no name (screens.md gives its look only); the words come with the pickup.
- "A white copy of its hull": drawn as a white outline of the part's hit circles (the fights draw their hulls each in its own
  code; a hull copy for every part of 22 fights is more than this batch).

## M5 part 7 "The ten-Track whole" — 2026-11-13 (7e: the balance pass, in "Play test 3" above)

### The armour numbers from the data (7667dd7)

- Read in place of the tuning constants: arpeggiator.json `numbers.damageScale` (`loading` 1.0: the core outside its One, whose
  x 2 stays the page's prose; `console` 0.25), reprise.json `numbers.damageScale` (`armBeforeTheMedley`, `closedCore`,
  `armNotSoloing`, `waveSegmentOffTheCrest`: 0.25 each), minibosses.json Deep Well `damageScale` (`swallowing` 0.25, its fill
  counting the whole; `choked` 2.0). `kApArmour` and `kRpArmour` are gone; `kDwArmour` is only the fallback.
- The Weaver's burst (the lead's ruling): drawn as an area, two danger edge lines 40 wu apart and a fill of 0.12 between them
  for its half beat (`wv_burst` retaken).

### The bullet pool with four players (GAME_DESIGN 6.6, 286d14d)

- The director buys no shooting species while the enemy bullets alive are above 70% of their pool (`kBulletPoolShooters`;
  `Species::shoots()`: every Sentry, Drifter, Hound, Carom, Flanker, Blink and Wyrm B, Moth, Well and Mimic B and C; the
  crowd's picks, the threats' and the elites').
- A fight's own patterns come first: bullets spawned while the fights update (`World::inBossUpdate_`, and the Deep Well's ring
  on a client) are flagged `Bullets::boss`; with the pool full one takes the place of the crowd bullet nearest its end.
- The 70% rule alone did not move the refusals (the soak below: 3,992 refused on the host): the shooters already alive keep the
  pool full (by the nearest body: Caroms' bounce fans 4,519 of the host's 10,340 bullets, Drifters 2,603, Sentries 522).
  Added (decision taken alone): **the crowd's volleys are held above 95% of the pool** (`kBulletPoolCrowd`, counted as held,
  not refused), so the last 5% stays the fights'.
- `raveform_headless --net soak --fight twin_tether --minutes 3` (four players, Track 7): **0 refused** on every peer
  (host: 10,277 fired, the fights 2,190; 4,188 crowd bullets held; none in a crowd bullet's place), soak PASS. The pool does
  not grow (2,048).

### A boss keeps pace (GAME_DESIGN 7.6, 286d14d)

- `BossState::paceDmg` counts each player's damage to a boss (after armour) in its fight's first eight bars; at the end of the
  eighth bar the host (or solo) sets the pace = that damage a player over the novice's (`tuning::kBossPaceNovice`), held
  between 1 and 4, and sends it (`kGeomBossPace`); from then on the boss takes its damage divided by the pace (a client's
  claim is noted undivided: the host divides by its own). Mini-bosses keep none.
- The novice's numbers (8-bar damage a player, from the fights' starts as each was tuned: Tracks 1-3 from the run's start,
  4-10 `--handover N`): Kaleidos 234, Longwave 450, the Choir 1,334, Event Horizon 4,188, Polyrhythm 2,910, Feedback 2,792,
  Cadence 6,501, Arpeggiator 16,772, Fata Morgana 21,279, Kaleidos Reprise 6,200.
- First built at the eighth bar alone: the careful bot killed Kaleidos (10 s), the Choir (15 s), Polyrhythm (15 s) and often
  Longwave (26 s) before it. The lead's ruling (ea472ab): **on every bar line of the first eight** the pace = the damage so far
  over the novice's number in proportion (k/8 at bar k), a ratio under 1.25 counting as 1 (`kBossPaceDeadBand`), between 1 and
  4; after the eighth it stands (each bar's pace is the host's `kGeomBossPace`; the eighth's sets it for good).
- From the fights' starts with it (20 runs, first meetings): the careful bot Kaleidos **23 s** (its line 30: short by a
  little, accepted), Longwave **52 s** (line 35), the Choir **25 s** (line 40: short), Polyrhythm **20 s** (line 50: short),
  Event Horizon 58 s (line 40), Feedback 47-72 s, Cadence 74 s, Arpeggiator 97 s (the last two at the eight-bar rule). The
  Choir and Polyrhythm die before the pace has grown (mean pace 3.0 and 2.1-2.8): no second mechanism (the ruling).
- The novice with it (from the run's start): Kaleidos 18 of 20 (42 s; 19-20 of 20, 37-41 s without), Longwave 14 of 20 (102 s;
  14-15, 93-95 s without), the Choir 14 of 15 met (42 s; 16-18 met, 40-48 s without), Event Horizon 11 of 11. Its lines
  hold as before; its fights are about a tenth longer (its mean pace 1.4-1.5: its good early bars pass the dead band).

### No pick is a trap (GAME_DESIGN 6.6; `raveform_headless --sim picks [--seeds N]`)

- The reference build Split 1 and Double-Time 1 (the careful bot, hits costing nothing, the loadout frozen: `Rules::
  frozenLoadout`; no pace: `Rules::noPace`). Each weapon and rhythm Mod's stack and each Riff's level: its gain over the
  reference in kills a minute (a minute from Track 3's second Groove) against a stack of Split's, and in seconds to kill the
  Choir from its start (alone) against a stack of Overcharge's (the lead's yardsticks, ea472ab); the better of the two, band
  0.6-1.5. And the Downbeat Cannon over its two ingredients at their caps.
- **The instrument is noisy**: a minute's kills swing by a fifth from seed set to seed set (the reference 1,457, 1,361, 1,023
  kills a minute on three sets), and a stack of Split adds only about a quarter to them, so a crowd figure is good to about
  +-0.5. The boss figures are steady (the Choir 92.7-93.3 s on every set).
- Far above on every set (crowd): Burn x 7-20, Arc x 3-10, Shatter x 3-14 (their chains through the crowd), Ricochet x 2-5,
  Heavy Step x 2-7, Pierce x 1.7-5, Pulse Ring x 2-4, On-Beat Crit x 1.3-6, the Downbeat Cannon x 3.5-6 (x 1.25 the Choir over
  its ingredients). Within on its boss figure: Split 1.07, Double-Time 0.65, Crescendo 0.94, Echo 0.59, On-Beat Crit 0.50.
- **Seeker** never killed the Choir: its shots curved to the fight's `pos` (the Choir's empty centre). Fixed: they curve to
  the nearest part that can be hurt (`steerShots`); the Choir in 98.5 s with it.
- Tuning of the picks and the balance pass: **on hold** (the lead, after the owner's play test notes). A first step on Arc,
  Burn and Shatter was tried and reverted.

### The not-built lists (parts 4-6), in the order a player would notice

- Built: the mini-bosses' elite traits shown (their names under the MINI-BOSS card, a white ring swelling on the beat round
  the body, a gold second for Armored); the Rift's black disc while it is open (on the priority layer's black: the scene's
  additive light cannot draw black); a Rift Surge's three joined by a faint triangle; the loop's species one tier higher
  for each time round (species.json; `pickSpecies`, to tier 5); the Weaver's burst as an area (the ruling above).
- Left (each more than an hour, or another's): Phasing's every other hull segment hidden (the glyph is drawn whole); Swift's
  x 1.3 turn (each family turns by its own code); Wyrm heads and Tether pairs as elites; Swift on the mini-bosses but the
  Tether and the Well; the "family met" condition for Tracks 9 and 10's mini-bosses (every family has debuted by Track 6, so
  it would choose nothing); Fata Morgana's lattice whipped along the wind and tinted in the oasis (the lattice has no
  directional push and no tint by region); Riffs other than shots and the Nova on a Vault; Feedback's and the Reprise's
  echoes of Riffs that fire nothing; the sounds the rows of `pending_sounds.cpp` list (the sound engineer's branch).
- Pictures retaken: `wv_burst`, `st_rift`, `ev_rift_surge`.

## M5 part 6 "Tracks 7 to 10" — 2026-11-13

### Elites from Track 3 (the lead's design commit d59062f)

- Elites come from Track 3, cycle 1 (`kElFromTrack`), at once in 16 bars in Track 3 falling evenly to once in 8 by Track 6;
  a trait is not drawn before its `fromCycle` of its first Track (Giant and Splitting: Track 3's second cycle).
- **The novice's first Longwave: 15 of 20** (17 before elites existed, 14 with them from Track 2). Its health untouched (the
  lead's rule). The fights table now has the cells as each fight begins: the novice wins Longwave with **3.1** cells on
  average and loses it with **1.2**. What takes them in Track 2 before Longwave (20 first runs): enemy bullets **51** hits,
  Caroms 17, Shoals 15, the mini-boss fight **12**, Drifters 7, Rams 4, a Sentry 1, a Flanker 1. The structures (Vault, two
  Pylons) and Track 2's events (Vault Drop, Comet) take none: they cannot hurt.
- Given back: Event Horizon 6.0 -> **8.0** (the novice 19 of 20, 118 s: the line holds). Polyrhythm's 2.0 no longer held (11 of
  20): **1.7** (15 of 19, 25 s; 2.4 given back: 10 of 19).

### Cadence (Track 7; design/bosses/cadence.md)

- Four pillars on a 700 wu square (I, V, vi, IV clockwise from the top left), the square's centre following the players' at
  40 wu/s; the bar's chord (the song's bar, `songShift_`) is open and silent, x 2 on its One; the other three fire a fan of five
  at the nearest player on beats 1 and 3 (180 wu/s, 4 s); Modulation (below 66%) turns the square a quarter the other way in a
  beat on every bar line the song comes back to I and adds a fan of seven on the snare; Inversion (below 33%) turns the rule
  over (the sounding pillar alone fires a rosette of sixteen on the One and twelve on three; the other three open). Death: the
  four burst in turn on the last bar's beats, the Shards (300) with the tonic.
- **Armour** (GAME_DESIGN 7.6): the closed pillars (the page: "the only one that can be hurt") take a quarter; since f32a441
  read from cadence.json `numbers.damageScale` (open 1.0, openOnTheOne 2.0, closed 0.25; in Inversion the sounding pillar is
  the closed one).
- **Hint**: `SHOOT THE SILENT ONE` (the page's weak point is the open pillar, which is the silent one: it matches).
- **Health** x 7 (`kCadenceHealthTune`): the novice with the careful bot's build at Track 7 (`--handover 7`) wins **18 of
  20** in **83 s** (x 1: 14 s; x 5: 66 s); the careful bot **73 s** (x 1: 9 s). The page asks 80-100 s.
- Co-op: everything on the song's clock; the square's centre is the heartbeat's sync point. **No host decision.**
- Pictures: `captures\m5\part6\` `cd_progression`, `cd_modulation`, `cd_inversion`, `cd_death`, `cd_strip` (dev).
- Moto (shared phone; this build's APK left installed): `--fight cadence:2:hold` 0% missed, main thread P95 4.4 ms, GPU 13.7,
  405 enemies in view at most, heap 0.
- Soaks: clean 2.91, impaired 5.38 wu RMS, census 0 mismatches.
- The flaky audio check "seq latest" failed once more: its writer now goes on publishing until the reader has read (the
  lead's rule), instead of a rerun.

### The boss entrance (the lead's f32a441)

- Nobody meets a boss on their last cell: as a boss arrives (`spawnBossNow`, bosses only, not mini-bosses) each of this
  device's players with fewer than two cells grows back up to two (never past their capacity), with Regen's `CellTake` event
  (the arcs draw over the entrance bar). Test: `arpeggiator` (one cell -> two; a mini-boss leaves it at one).
- The forty-seed recount of the first pass was withdrawn by the lead (short runs only); not run.

### Arpeggiator (Track 8; design/bosses/arpeggiator.md)

- A machine (a console 900 x 120, its core of 70 behind a shutter, four ports, the 4 x 16 gold chart above it) stands 380 wu
  from the players' centre on its side, following along its axis at 120 wu/s; four lanes 200 wu wide and 900 long; the
  blocks (90 x 28 danger bars on the priority layer, 420 wu/s, `bossBeamsHit`) leave the ports on the lead's steps in the lane
  of the note (G's ranks, `kApLanesG`). Quarter notes: steps 0, 4, 8, 12; Eighth notes (below 66%): every second step, and
  every fourth bar it moves to the next side (its lanes dashed for the bar before); Two hands (below 33%): a second machine,
  white, at right angles, the stab's rhythm (steps 0, 3, 6, 9, 12, 14) in lanes 1-4 in turn. A Nova clears the lanes. Death:
  the chart reads GG, the blocks climb back up their lanes in gold, chirps up the scale over two beats, the Shards (340) on
  the One that ends the Track.
- **Floored part**: the console ("sheds shots") takes a quarter on the core's health (`tuning::kApArmour`; not in
  arpeggiator.json). Core x 2 on the One (the shutter open), x 1 at other times.
- **Hint** `SHOOT IT AS IT LOADS` (the page's weak point: it matches).
- **Health** x 18 (`kArpHealthTune`): the novice with the careful bot's build (`--handover 8`) wins **17 of 20** in **98 s**
  (x 1: 10 s; x 10: 18 of 20, 62 s; x 16: 17, 89 s); the careful bot **73 s**, 20 of 20. The page asks 90-110 s. Both bots
  step to a lane line as a block comes down their lane (a block is 90 wu of a 200 wu lane).
- Decisions taken alone: every chord uses G's lane ranks (the page gives G's); Two hands' stab lanes 1, 2, 3, 4, 1, 2; a
  block's note sounds as it leaves its port (the page: as it passes the nearest player's row); the second machine takes the
  next side clockwise.
- Co-op: the blocks follow the song's clock; the machines stand from the players' centre on every device. **No host decision.**

### Fata Morgana (Track 9; design/bosses/morgana.md)

- The oasis (260 wu) drifts along a figure of eight 900 wu wide (a lemniscate of Gerono, 450 tall) at 110 wu/s of arc from
  where the players were, a function of the song's clock (`morganaDrift`: the rest bars taken out). The storm hurts on the
  kick (steps 0, 6, 10): a ship whose centre is outside the calm loses a cell. Every fourth bar of the song the oasis rests
  and she crosses it through its middle (x 2 over the water; she does not fire then). She circles 70 wu outside its edge, a
  turn in four bars, firing a fan of five (40 degrees, 200 wu/s, 4 s) at the nearest player on each kick. Mirage (below
  66%): on the song's fourth bar lines a second oasis 500 wu away, one of them real (the seed and the bar; the real one
  moving the drift with it); the mirage is calm while shown, blinks out on the snares, is gone after two bars. Crossing (below
  33%): the drift stops; every second bar the oasis leaps 600 wu (shown dashed the bar before; the way from the seed and the
  bar, clear of the walls), the old one shrinking over the bar and a corridor 160 wu wide joining them; x 2 the beat after.
  Death: the storm drops; the Shards (380).
- No floored part (the page names none that cannot be hurt).
- **Hint**: the lead's `SHOOT WHERE THE WALL THINS` does not fit the page (it has no wall; its weak point is her crossing the
  water and the beat after a leap): `SHOOT HER OVER THE WATER`.
- Bots: both keep inside the water they go for (the novice the nearer water until it has learned the blink): nothing takes
  them out across its edge, a Dash that would goes along it.
- **Health** x 14 (`kMorganaHealthTune`): the novice (`--handover 9`) wins **15 of 20** in **61 s** (x 1: 20 of 20, 8 s;
  x 17: 9 then, with the bots' stronger hold on the water, 11 of 20, 69 s); the careful bot at x 17 20 of 20, 60 s. Shorter
  than the page's 90-120 s: the novice's line comes first (most of its lost cells are the storm's, 8 a fight).
- Not built: the lattice whipped 10 wu along the wind, the oasis's tint on the lattice and the death's tint edge to edge (the
  lattice has no directional push and no tint by region); the storm's crackle (a held voice); the 2 kHz low-pass is the band's
  bed low-pass (`lowPassBed`), not the whole music bus.
- Co-op: the drift, the rests, her orbit and the kicks follow the song's clock; the mirage and the leaps are drawn from the
  run's seed and the song's bar, acting on bar lines; each device hurts its own players. **No host decision.**

### Kaleidos, Reprise (Track 10; design/bosses/reprise.md)

- Kaleidos's wheel a quarter larger (its facets to arms of 300 wu, a core of 80), each arm in the accent of its boss's Track,
  a turn in eight bars, 450-650 wu from the nearest player. Reprise (100-70%): fans of five from the arms of the two pairs not
  resting (Kaleidos's Bloom) on the two-step's hits (steps 0, 4, 10, 12); core x 1, x 2 on the One. Medley (70-20%): each arm
  a sixth of the phase's health; solos of two bars round the wheel from Kaleidos's arm (a dead arm skipped): Kaleidos (fans
  from the soloist and the arm opposite on the eighths), Longwave (the arm lets go: eight segments 150 wu apart sweep 1,800 wu
  out toward the nearest player over the two bars, swinging 80 wu a beat; only the crests open), the Choir (a volley of seven
  on the One, answered on three by the arm opposite), Event Horizon (ships and enemy bullets drawn to the wheel at 200 wu/s;
  a ring of 24 on its second bar line), Polyrhythm (two rings of 24 each One, a gap each: the three's 120 degrees on a bar at
  240 wu/s, the four's 90 at 180, meeting on its first One), Feedback (an Echo of the nearest player plays back its last bar
  from the far side of the wheel: Feedback's tape and echo shots). Last Light (20-0%): First Light's arpeggio from
  `design/music/track01.json` (`RepriseData::arpSemis`), a bullet a sixteenth at 15 degrees a semitone above the bar's lowest,
  the figure turning 45 degrees a bar, 260 wu/s; core x 1, x 2 on the One; the palette cross-fades from Last Light to
  Ultraviolet by the health left (`Play::updatePalette`). Death: no burst, the core dims to a point, one white ripple across
  the lattice; the Shards (500) and the ENCORE card on the bar line.
- **Floored parts** (a quarter, `tuning::kRpArmour`): the arms in the Reprise ("cannot be hurt yet"); in the Medley the
  closed core (its quarter goes to the soloist's sixth) and every arm but the soloist; the wave's segments but its crests.
- **Hint** `SHOOT THE ARMS OFF` (kept: the page's weak point is the soloist in the Medley; in the Reprise and Last Light it is
  the core on the One).
- **Health** x 4.5 (`kRepriseHealthTune`): the novice (`--handover 10`) wins **20 of 20** in **131 s** (x 1: 31 s); the careful
  bot 20 of 20, **88 s**. The page asks 110-140 s.
- Co-op: the turn, the fans, the solos and the last pattern follow the song's clock and the arms alive (the host decides an
  arm's death); the wheel is the heartbeat's sync point; what reads where the players are (the wave's way, the volleys' aim,
  the Echo's player, the rings' first gap) is each device's reading. **No host decision.**

### Mini-bosses batch 3 (design/bosses/minibosses.md "Batch 3")

- **Mirror Mimic** (Track 6, always): it stands where the player nearest the mirror point (the players' centre as it arrived)
  would be through it; one bullet a step from it along that player's aim turned half round (400 wu/s, 3 s, at most 60) while
  they fire; it copies their Dash a beat late, and for that beat and the next it holds fire and takes x 2. The mirror point
  hurts (24 wu). Half: a second image across the level line through the point. Death: glass, its last volley as Shards.
- **Twin Tether** (Track 7 or 8): two nodes 520 wu apart, a turn in two bars, the middle following the nearest player at 60
  wu/s; the rope between them lethal (12 wu); a Dash through it (this device's players) goes slack for a beat (two after a
  Perfect Dash): the rope off, both nodes x 2. Each node half the health; one dead: the survivor's flail (420 wu, a turn a
  bar).
- **Weaver Prime** (Track 7 or 8): a cage round where the nearest player stood at its first knot (the host's); laps of 300,
  250, 200, 150 wu in beats 2-4 of four bars, the trail lethal (12 wu) for two bars; the knot on every One (x 2); after the
  fourth lap a new cage, the last one's third and fourth laps bursting 40 wu wide for half a beat (the page's 40, above the 12
  wu line: the lead rules). Half: a second Weaver the other way, laps 25 wu inside the first's, its knot on beat 3.
- **Deep Well** (Track 8): drifts at 30 wu/s (half: 90), bends shots within 400 wu, draws ships within 300 wu (up to 120
  wu/s). It swallows: a quarter of the damage (**floored**: the page says it takes no harm) while the bar's fill counts the
  whole; on the One what the bar fed it comes back as a ring (one bullet a 3 damage, at most 48, 220 wu/s; half: two rings,
  beats 1 and 3); fed more than 5% of its health in a bar it chokes for the next bar (gold, x 2, no ring). The bar line is
  the host's (`kGeomWellBar`). Death: one Shard for each shot swallowed, to 300.
- The picks (`World::pickMini`): Track 6 Mirror Mimic; Track 7 Twin Tether or Weaver Prime; Track 8 Deep Well or the other
  of the two; Tracks 9 and 10 any the run has not met (`DirectorState::minisMet`) with one and two elite traits (Armored: x 2
  health; Swift: x 1.6 on the Tether's and the Well's speed only; Regenerating: 8% a beat after two quiet beats, the host's);
  the loop any of the twelve but the last, with a trait a round of the loop, to three. The "family met" condition is not
  applied to Tracks 9 and 10, and the traits are not drawn on a mini-boss (its look): listed for part 7.
- Bots: the Mimic's answer is dodged as bullets and the careful bot fires in phrases (two beats on, two off); both step off the
  Tether's rope and Dash through it as it comes; both leave the Weaver's cage and cross a trail with a Dash.
- Sound rows (`pending_sounds.cpp`): what the engine lacks is listed there (the Mimic's inverted lead, the Tether's drone, the
  Weaver's crackle, the Well's reversed cymbal into the One).
- **Health** (`kMiniHealthTune[8..11]`, the novice from the Tracks' starts, `--handover 6/7/8`): Mirror Mimic x 0.5 (x 1: 8
  won, 12 warped out; x 0.5: **13 won**, 7 warped out, 18 s); Twin Tether x 1 (Track 7 **6 of 9**, 22 s; Track 8 3 of 5);
  Weaver Prime x 2.5 (x 1: 11 of 11 in 10 s; x 2.5: Track 7 **9 of 11**, 24 s; Track 8 5 of 6, 21 s); Deep Well x 1.3 (x 1: 7
  of 9, 15 s; x 1.3: **8 of 9**, 12 s). With the Track 6-8 bosses on the same runs: Feedback 17 of 20 (97 s), Cadence 18 (84
  s), Arpeggiator 17 (108 s).
- Pictures: `captures\m5\part6\` `ap_*`, `mg_*`, `rp_*` (a solo each), `mm_*`, `tt_*`, `wv_*`, `dw_*`, strips `ap_strip`,
  `mg_strip`, `rp_strip`, `wv_strip` (dev). The picture tables' shared index (`Play::m5Pic_`) passed 100 with these and the
  fights' pictures were read as the families' (out of bounds: the crash of `wv_knot` and the dead runs): widened to u16 with the
  other tables from 1000, 2000 and 3000 (a static_assert).
- **Moto** (shared phone; 3be00a8's APK left installed): `--fight reprise:0:hold` (the Reprise's fans from four arms on the
  two-step, the densest of the new fights) 60 s: panel 0% missed (PASS), main thread P95 4.5 ms, GPU 13.6 ms P95, 447 enemies
  and 98 bullets in view at most, heap 0. Not measured on the phone (short runs only): Arpeggiator, Fata Morgana and batch 3.
- Soaks (test.ps1, after the merge of m4-net f836e5e at 3be00a8): clean PASS, impaired PASS.
- The halo-burn test (play test 2) sets its set list to three: at a set list of eight it had been testing nothing (no loop
  Track in its Tracks 4-8); at ten it failed.

## M5 part 5 "The arena and the elites" — 2026-11-13

The six structures and eight events of `design/arena.md` (arena.json) and the elites with the twelve traits of
`design/traits.md` (traits.json); with them, the lethal glow of look.md `1dce5d3` (0.20 on the axis, sigma half the kill
width) and the merge of `m4-net` (d5be6ea).

### What was built

- **Data**: arena.json and traits.json embedded and read (`ArenaData`, `TraitData`; the pages' prose numbers in
  `boss_prose.h` "The arena"); their digest joins the peers' content hash.
- **Elites** (`src/sim/world_elites.cpp`): the director buys one with the threat budget (x 4 its species' threat) about once
  in 16 bars in Track 2 falling evenly to once in 8 by Track 6, from Track 2's second cycle, never in a Break or a fight; at
  most 3 at once in Tracks 2-6 and 5 from Track 7. Any species of the Track's roster but Shoals, Wyrms and Tethers; x 1.3 its
  size, x 6 its health (co-op x 1 + 0.5 a player), x 10 its score, x 4 its threat, 20 Shards and a Cell (not in the loop), a
  large kill. Traits drawn from those the run has met (one in Tracks 2-6, two in 7-10, one more each time round the loop, at
  most four; never Cloaked with Phasing). The twelve: Armored (x 2, a second hull until half, its shatter), Swift (x 1.6 its
  speed), Giant (x 1.8 and x 2.5, never shoved), Splitting (two copies at 0.6 and 30%), Volatile (a 160 wu ring bursting two
  beats on: a cell from a ship inside, 30 x the Track to every enemy), Regenerating (8% a beat after two quiet beats), Shielded
  (its front 120 degrees sheds shots, turning 1.2 rad/s), Twin (two bodies, one bar x 1.6, 200-300 wu apart, dying together),
  Magnetic (steals up to 40 loose Shards within 300 wu; they orbit it; released doubled), Commander (x 1.3 the speed of all
  within 400 wu; its death stuns them a beat), Cloaked (8% between beats, full on the flash: the view), Phasing (solid only in
  the first half of each beat). The view: look.md 3 (hull + 0.6 at 2.3, a white copy, glass, the aura at 0.22, a white edge
  chevron) and each trait's treatment of look.md 4.
- **Structures** (`src/sim/world_arena.cpp`): placed by the host on each Track's first downbeat, 1,000-1,800 wu from the
  players' centre, 700 apart, 400 from a wall, outside every frame; dissolving over the Track change's Break unless a player
  is within 600 wu. Vault (health trackScale x 0.33; a Boost and 40 Shards a player; a ring of its ornament gone each quarter);
  Pylon (four beats inside its 150 wu ring tune it for a cycle: on every One a ring to 520 wu at 700 wu/s, 6 x the Track a
  body); Rift (open through the second cycle's Groove and Build, four fodder a bar; x 0.25; a Boost and 30); Bumper (a cluster
  300-500 wu apart; ships kicked off at 600 wu/s, shots reflected for x 1.5, enemies and bullets bounced; eight hits discharge a
  ring of 260 wu, 10 x the Track); Current (240 x 1,600 wu, two crossing; +260 wu/s for ships, enemies and bullets; shots with
  it x 1.3 speed and damage, against it x 0.7); Lens (320 wu, a turn every eight bars; a shot crossing it becomes three, the
  two beside it 14 degrees out at half damage in the hues either side).
- **Events**: Track 1's Comet in its third cycle's Groove; from Track 2 one in the second cycle's Groove, from Track 4 a second
  in the third; drawn from the Track's list, never the Track before's; announced a bar ahead (gold chevron, the name at the
  place; the Comet a white chevron two beats ahead). Vault Drop (a gold Vault for eight bars: two Boosts and 80), Rift Surge
  (three Rifts on a 900 wu triangle from the Build to the Break, six a bar; all three: a Cell for the hurt, 100 Shards for the
  others), Elite Hunt (the Track's highest-threat family with two traits, three from Track 7, fleeing at 0.8; killed in twelve
  bars: a level's Trio and 150 Shards), Comet (700 wu/s through the players' centre, five Shards a beat, 60 and a Boost),
  Eclipse (eight bars: the scene x 0.15 between beats, back on the kick's flash; the lattice 0.05; kills x 2), Meteor Rain
  (four bars of three 110 wu rings a beat, one on a player, landing two beats on: a cell, 20 x the Track), Convoy (three
  carriers at 60 wu/s with eight Hounds; a Boost and 50 each, the last a Cell; twelve bars), Storm Cell (320 wu drifting in
  at 90 wu/s for eight bars; lightning on up to six bodies a beat, 15 x the Track; a ship inside two beats running loses a
  cell).
- **Sound**: one row a cue in `src/game/pending_sounds.cpp` (the traits' marks, the structures', the events'); what the engine
  lacks is in each row.
- **Bots**: both keep out of a bomb's or a meteor's ring and the Storm Cell.
- **Co-op, the host's decisions** (for the network engineer): elites and their traits (operation 1), Twins' links (2),
  structures and events' bodies placed (3), an event begun with its place (4), each meteor (5), a body broken (6) or gone (7),
  an event ended (8), the hunted elite (9); the Rift's flow, Splitting's copies and the Convoy's Hounds are the host's spawns;
  the Storm Cell's lightning, meteors and Volatile bombs on enemies are the host's; Boosts are the host's drops. Each device's
  own: a Current's push and a Bumper's kick on its ship, a meteor's, bomb's or Storm Cell's cell, its players' Shards and Cells,
  Magnetic's stealing, a Lens's split; a tuned Pylon's pulse is its tuner's device's, a Bumper's discharge its charging shot's
  owner's. A client's hits on a body are claims (boss claims on part 0x80 + slot).

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Tests | audio 2,679 checks, sim 5,787 (new: the arena test: the data, an elite's factors and its death, Twin, Splitting, Volatile, Regenerating and Shielded, a Vault broken, a Lens split, a Current, a Bumper, the Comet, Meteor Rain), net 131,366; determinism passes | all pass |
| Loopback soaks, 4 players, 10 min | clean **2.68**, impaired **4.69** wu RMS; census 0 mismatches; the worst upload second 44.5 / 49.4 kbit/s. (The first run failed: the arena's operations and body claims were refused by the wire's checks and took their packets with them; `src/net/protocol.cpp` now accepts them, protocol **0x52460506**) | < 12 wu; 60 kbit/s |
| The novice, 20 x 60 min, with elites and the arena | Track 2's boss reached in **17**; first meetings won: Kaleidos **20**, Longwave **14** (93 s), the Choir **16** (41 s), Event Horizon **19** (102 s), Polyrhythm **12** of 19 (40 s), Feedback **13** of 13 (115 s) | 16; 18; 16; 16; 12 a boss of Tracks 4-6; <= 1.5 x the page |
| The careful bot, 20 x 40 min | first deaths in Tracks 1-3: **0**; in Tracks 1-6: **1**; median 28.6 min; elites 1,237 in the 20 runs, 245 bodies broken, every event drawn (Vault Drop 75, Rift Surge 39, Elite Hunt 61, Comet 122, Eclipse 21, Meteor Rain 41, Convoy 31, Storm Cell 32) | 4 |
| The careless bot, 20 x 40 min | Tracks 1-3: **1**; Tracks 1-6: **2** | 2; 8 |
| Pictures (Debug, 0 validation messages) | `captures\m5\part5\`: 26 (`el_*` 12, `st_*` 6, `ev_*` 8; `ev_comet`, `ev_rift_surge` retaken in dev after their fixes); `hz_jets`, `pr_burn`, `bs_fire` with look.md `1dce5d3`'s glow: 59.2/85.4/**3.9**/0.052, 57.2/86.3/**3.1**/0.050, 48.8/81.8/**4.9**/0.060 (dark<48, dark<96, >=200, mean) | |
| Moto (shared phone, our APK deployed before each run, the lock; `bench_play`, `--tier low --nohits`) | `--fast-forward 640` (Track 5 with its Currents, Pylon and Rift): **0% missed**, main thread P95 4.7 ms, GPU P95 14.2 ms, 223 enemies in view at most, heap 0; `--drop 6.3` (Track 6's Lenses and Bumpers, 482 in view at most): 0.06% missed (Swappy 2.09%), panel P95 one period, 4.8 / 14.1 ms, heap 0 | P95 <= 16.9 ms, missed <= 5% |

### Not done or not verified

- **Longwave's novice line is not met with elites in the run** (14 of 20; 17 before them). Its health does not move it: x
  0.58 to x 0.77 of part 4's tune all give 13-14 wins (68-98 s): the novice reaches the fight with fewer cells after Track 2's
  elites (Armored and Swift from its second cycle). The levers are the lead's: the elites' first Track or pace, or the novice.
  Polyrhythm went 2.4 -> 2.0 and Event Horizon 8.0 -> 6.0 for the novice's lines with elites and the arena.
- Mini-bosses warp out more often with the novice now (Battering Ram 11 of 16, Moth Queen 9 of 15, Sentry Array 11 of 17).
- Not built: the Rift's black disc (the scene is additive: it is not drawn); Phasing's "every other hull segment hidden" is the
  whole hull at 35% off its half; Swift's x 1.3 turn (only its speed); Wyrm heads and Tether pairs as elites; the "faint
  triangle" joining a Rift Surge's three; the Comet's "saw glide rising over the two beats of its warning" is the announce
  row's glide; Riffs other than shots and the Nova do not hurt a Vault.
- Sounds: every arena and trait row in `pending_sounds.cpp` plays the nearest the engine has; what each lacks is in its row.
- The bots do not tune Pylons (2 in 20 runs) or seek structures.

### Decisions taken alone

- **trackScale** in co-op is x (1 + 0.5 a player) as the mini-bosses' health (the page is silent).
- Rift bodies are part of the crowd count only as bodies (the page: "part of the crowd target, not on top of it"); they are
  spawned on top of the target.
- The Bumpers' and Currents' counts are byTrack's (Track 6 has two Bumpers where the rule says three or four).
- A kill inside a Comet's lane, Vault or carrier counts for nobody but the shooter; Boosts from bodies are the host's drops
  (one orb a player).
- Elites drop no Cell in the loop (no kill drops a Cell there, GAME_DESIGN 8).
- Elites' Splitting copies are not elites (no trait, no Cell).
- The Elite Hunt's reward Trio is a level's (banked on the next bar line).
- The Pylon tunes for any player inside its ring; in co-op each device sees the tuning by its own view of the ships, and the
  pulse hurts on the tuner's device.

## M5 checkpoint "A boss's rule is a bonus, not a wall" — 2026-11-13 (before part 5)

The lead's checkpoint after part 4 (design `514e4ea`, `17a36f7`): the user never got past Longwave.

### What was built

- **Armour takes a quarter** (GAME_DESIGN 7.6). `World::damageBoss`: a multiplier below 1 is armour: the shot sparks and
  stops as before (an Impact on part 1 carrying the share) and does the share the page gives (0.25 in longwave, choir,
  polyrhythm, feedback and horizon.json, read as they are). Polyrhythm's corners and body are armour on their rotor (their
  0.25 goes to that rotor's health). Feedback's swallowed-shot hat sounds only while its Head is fully shut. Kaleidos is as
  built: only its lens plates are armour (at nothing), its firing arms' third stays a hit. The Battering Ram's horns stay at
  nothing (minibosses.json: "sheds every shot"); the Brood Mother's shut dome (a quarter already) now sparks as armour. The
  Nova's blast does armour's share on an armoured part (it skipped them).
- **The boss hint line** (screens.md "Boss hint"): under the boss's name in the HUD's small size (cap 11), white at 0.8, from
  the arrival through the fight's first four bars, the last fading; again for two bars (the last fading) when this player's
  own shots met only armour for two bars running, at most once every eight bars (the arrival's showing is the first). Kept by
  `Play::bossHintTick` from this device's player's Impact events, so each co-op player sees it by their own shots. The six
  lines of Tracks 1-6 (`bossHint`); mini-bosses have none yet (screens.md: written as each is reviewed).
- **The novice** (`--policy novice`, `Bot::Policy::Novice`, `tuning.h` `kNovice*`): it aims at the nearest body (a boss's
  nearest part, armour or not) 200 ms behind it with a 6 degree wander; from bar 16 of a fight it aims at the weak point in
  half the bars (a hash of the bar); it never rides Longwave's crest, stands inside the Choir's ring, holds Event Horizon's
  lanes, rides Polyrhythm's gaps or the Sentry Array's notch, circles to Kaleidos' open sector, or phrases its fire for
  Feedback outside the bars it uses the rule; it dashes out of half the hits it sees coming, Novas at one cell, and takes a
  Trio's first orb half the time, else any.
- **Calibration** (on the build the user played: armour at nothing, Longwave at 2.2): the first novice (its 250 ms as
  threats seen 250 ms late) won its first Longwave in 18 of 19 runs: not a novice. Two changes, mine: **its 250 ms is a
  steering delay** (the stick it holds is what it decided 15 ticks ago; seeing threats late was absorbed by the careful bot's
  1.4 s look-ahead), and **it minds only the bullets arriving within 0.6 s** (`kNoviceLookS`). Then, on those rules, it loses
  its first Longwave in **11 of 20** (9 won, 233 s when it wins; Kaleidos 15 of 20; Track 2's boss reached in 15):
  `raveform_headless --sim crowd --seeds 20 --minutes 25 --policy novice --armour0 --health kaleidos=1.818,longwave=2.857,choir=3.333`.
- **Health tuned with the novice** (seeds 1-20, 60 minutes, `--policy novice`; levers `--health id=x` until the lines held,
  then baked into `tuning.h`): the table below. Tracks 4-6 were first measured with `--handover N` (the careful bot's build
  handed to the novice at Track N), then on the novice's own runs.
- **Lethal beams are lasers** (look.md v2.5): `PlayView::lethalBeam` draws every one: the priority capsule exactly as wide
  as it kills, a 2 px white-pink line on its axis, and a danger glow (0.25 on the axis, sigma twice the kill width, reduce
  flashing 0.15) that hurts nobody. Kill widths: Event Horizon's jets **12 wu** (44), Polyrhythm's burning link **12** (32),
  the Blink Star's star **10** (16); Kaleidos' beam, the Choir's cage and held note (Kaleidos' 10) and the Tethers' beams (6)
  unchanged and drawn the same way. Every beam's warning dashes are as wide as the beam (`telegraphLine`'s width).
- **Feedback's cream discs** were **kill pools**: a crowd killed in one place (the Zenith palette's fodder is cream) summed
  a dozen pools into a disc a tenth of the screen wide (`--mute killpool` removes them). A kill inside a live kill pool now
  renews that pool instead of stacking another (`PlayView::addPool`), so no place holds more than one pool's light.
- **The Beam Boost's glow**: it burned and drew 28 wu with one lane (`kBeamHalfWu` + 4 a lane); look.md says 20, so the first
  lane adds nothing (20, 28, 36 ... wu). Glow discs under 16 ref px (look.md §3) are sparkles, not quarter-resolution pools.
  The held cores (the Beam Boost, Standing Wave) move between 1.0 and 1.2 (was 1.4).
- **Co-op**: a client's boss claims are summed per part a tick (every shot on a boss claims now): the impaired soak's worst
  upload second went 61.0 -> 48.5 kbit/s (limit 60).

### The fights with the novice (seeds 1-20, 60 minutes; the careful bot 40 minutes)

The first run reached Track 2's boss in **16 of 20** (line 16). First meetings: met / won of 20 runs, the seconds of a win;
hits a fight over all its fights (bullets / bodies / beams); the careful bot's seconds at the first meeting; the health factor
in `tuning.h` (part 4's x the novice's).

| Fight | Novice met / won | Its seconds | Hits a fight | Careful bot | Factor | Lines |
|---|---|---|---|---|---|---|
| Kaleidos | 20 / **19** | 31 | 2.8 (2.4 / 0.4 / 0) | 9 s | **0.55** (new) x 0.7 in Track 1 | won >= 18; <= 135 s; careful >= 30 s |
| Longwave | 20 / **17** | 98 | 4.3 (3.3 / 1.0 / 0) | 18 s | **0.77** (2.2 x 0.35) | >= 16; <= 150 s; careful >= 35 s |
| The Choir | 20 / **17** | 44 | 3.1 (2.8 / 0.3 / 0) | 15 s | **1.26** (4.2 x 0.3) | >= 16; <= 165 s; careful >= 40 s |
| Event Horizon | 20 / **19** | 107 | 4.1 (2.6 / 1.3 / 0.3) | 28 s | **8.0** (16 x 0.5) | >= 12; <= 120 s; careful >= 40 s |
| Polyrhythm | 19 / **12** (7 lost) | 40 | 6.1 (0 / 0.1 / 6.5) | 18 s | **2.4** (3.0 x 0.8) | >= 12; <= 180 s; careful >= 50 s |
| Feedback | 14 / **13** | 136 | 5.4 (4.1 / 0.2 / 1.1) | 43 s | **2.0** (2.5 x 0.8) | >= 12; <= 180 s; careful >= 60 s |
| Alpha Hound | 19 / 18 (1 warped out) | 19 | 0.3 | 5 s | 0.975 (6.5 x 0.15) | <= 52 s; careful >= 12.5 s |
| The Hive | 16 / 11 (5) | 25 | 0.5 | 7 s | 0.75 (5.0 x 0.15) | |
| Battering Ram | 18 / 13 (5) | 26 | 1.4 | 10 s | 0.7 (2.0 x 0.35) | |
| Carom King | 13 / 10 (3) | 24 | 1.8 | 10 s | 1.44 (4.8 x 0.3) | |
| Moth Queen | 16 / 12 (4) | 29 | 2.0 | 10 s | 3.0 (12 x 0.25) | |
| Brood Mother | 17 / 12 (5) | 18 | 1.2 | 11 s | 2.0 (10 x 0.2) | |
| Blink Star | 11 / 8 (3) | 28 | 2.4 | 16 s | 3.0 (5 x 0.6) | |
| Sentry Array | 17 / 8 (3 lost, 6) | 25 | 2.8 | 11 s | 0.8 (4 x 0.2) | |

Every novice line holds; **every careful-bot line fails, by three to five times** (the careful bot's damage on target is that
much the novice's: it leads its aim, never wanders and shoots the weak point from the first bar). Where they pull apart the
novice's line wins (the design's rule). Mini-bosses have no won line; their health was taken down until the novice won most
of its meetings within their 24 bars (warped out: the hold ran out). The guards: the careful bot's first deaths in Tracks 1-3
**0 of 20**, in Tracks 1-6 **0** (line 4); the careless bot's 0 and **3** (lines 2 and 8).

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Tests | audio 2,679 checks, sim 5,763 (armour's quarter and its spark, the calibration switch, the new health factors), net 131,366; determinism passes | all pass |
| Loopback soaks, 4 players, 10 min | clean 4.02 wu RMS; impaired 6.30, upload's worst second 48.5 kbit/s | < 12 wu; 60 kbit/s |
| Lethal beams (`scenes.py light`, 2400x1080: dark<48, dark<96, >=200, mean; before -> after) | `hz_jets` 53.0/80.5/**6.7**/0.070 -> 54.9/80.3/**3.9**/0.056; `pr_burn` 31.8/66.2/6.4/0.093 -> 32.1/67.6/4.9/0.085; `bs_fire` 40.1/72.2/6.4/0.080 -> 41.4/72.0/4.9/0.066; `bs_half` 39.3/71.0/7.9/0.092 -> 40.4/67.9/6.3/0.080 | |
| Feedback's kill pools (the same) | `fb_open` 40.8/74.3/**7.4**/0.109 -> 42.0/78.4/**4.3**/0.083; `fb_slapback` 35.8/68.7/8.9/0.135 -> 36.8/73.1/4.9/0.101; `fb_death` 54.0/82.4/4.8/0.081 -> 60.3/86.6/2.9/0.058 | |
| The Beam Boost's glow, full width at half maximum across the beam (core, glow and bloom together, the picture's luminance, everything else muted) | **13.3 wu** at its 20 wu (14.7 at the 28 it burned); at a quarter of the peak 42 wu, at a tenth 63 (the core's bloom alone: 16 and 44); the glow alone as a quarter-resolution pool measured 25 wu where its sigma gives 13 (hence sparkles under 16 ref px) | <= w |
| Pictures | `captures\m5\checkpoint\`: `hz_jets`, `pr_burn`, `bs_fire`, `bs_half`, `fb_open`, `fb_slapback`, `fb_death` (dev build; the boss hint shows on `fb_open`) | |
| Moto (shared phone, `8f5abeb` deployed before each run; `bench_play`, `--tier low --nohits`) | `horizon:3:hold` (the jets) 0% missed, 3.8 / 13.5 ms (main thread / GPU P95); `blink_star:1:hold` 0%, 4.4 / 14.0; `feedback:1:hold` 6.37% on a first run with no late CPU frame and GPU P95 14.2, 0% on the rerun (a transient on the shared phone); `feedback:2:hold` 0%, 4.4 / 14.2; `--fast-forward 640` 0.03%, 4.9 / 14.3; heap 0 in every run | P95 <= 16.9 ms, missed <= 5% |

### Decisions taken alone

- The novice's 250 ms is a steering delay and it minds bullets within 0.6 s (the calibration above).
- A Nova on armour does armour's share of its blast (the design is silent; it skipped armour).
- Kaleidos' firing arms (a third) stay a hit, not armour: "Kaleidos is unchanged" (their sparks and its hint are as before).
  For the hint, a hit below x 1 counts as armour, so on Kaleidos shots on the firing arms bring the hint back.
- The hint shows from the boss's arrival (its entrance bars) through the fight's first four bars ("as it arrives" and "the
  first four bars of the fight" both hold); a bar with no shot on the boss breaks a run of armour-only bars.
- Kill pools merge (a kill inside a live pool renews it, at the larger of the two levels) rather than going under 0.12: a lone
  pool keeps look.md's table (0.26 and 0.32); no place holds more than one.
- The Beam Boost hurts 20 wu with one lane, 4 more a lane after the first (look.md's 20).
- The careful bot's lines are left failing (the novice's win; table above); Polyrhythm stays short (40 s for the novice) as
  ruled.
- Not built: `--fight` jumps with the careful bot's median build (over an hour: the game would need a build to restore; the
  headless `--handover N` does it for the sims).

## M5 part 4 "Tracks 4 to 6" — 2026-11-13

Tracks 4 (Aurora), 5 (Furnace) and 6 (Zenith) with everything the design gives them: the five families of their rosters
(Wyrm, Tether, Weaver, Well, Mimic) and the variants they use, their bosses Event Horizon, Polyrhythm and Feedback, and
mini-bosses batch 2 (Moth Queen, Brood Mother, Blink Star, Sentry Array). The set list is now six Tracks (`kBossesBuilt`);
the loop begins after Feedback.

### What was built

- **Families** (`src/sim/world_families4.cpp`, commits `e9f1728`, `10ae503`): Wyrm chains (a head and its body, the C variant
  regrowing a head), Tether pairs and triads (the beams between them), Weaver trails (the C blast), Wells (pulling ships,
  bodies and shots; B swallows shots), Mimics (stepping to the mirror point on the beats, B firing every fourth volley, C
  copying the Dash), and the variants Tracks 4-6 use (Skitter B mines, Moth C bursts, Blink C decoys, Flanker C, Drifter C,
  Shoal, Ram C, Carom C, Skitter C).
- **Event Horizon** (`world_horizon.cpp`, `cdc1fac`): the pull (ship 260, fodder 340, bullets 180, shots 220, Shards 300 wu/s
  at the photon ring, x (136/d)^1.2; a Dash ignores it), eating (a cell a body, five Shards a body; no Shards, no score), the
  bar line's verdict (the host's `kGeomHorizonBar`: fed, the eruption of 8 + 4 a body on spiral arms and the shield x 0.15;
  starved, the gasp and x 1 for the bar), the lanes (two, three, four spiral arms of 1,300 wu, 270 degrees; the Well's own
  stream, the director's crowd off), Tidal's wave on beat 3 (shoves ships and Shards 140 wu), Collapse's drift (90 wu/s) and
  jets (shown a bar ahead along the next axis, two beats on a fed bar line), the contact throw (300 wu,
  `PlayerState::knock`, taken by the controller). The view: a true black disc on the threat layer (after tone mapping, no
  bloom, no grid inside), the rings of dashes brighter on the right half, the photon ring, the meter on the priority layer
  (the cell filled flashes white), the lanes lit by what walks them, the gold star and rim while starved, the lattice pulled
  in on every beat, the wave, the jets; its death implodes and the flash and its 300 Shards come a bar and a beat later
  (`BossState::payoutTick`). Camera 0.8.
- **Polyrhythm** (`world_polyrhythm.cpp`, `c4ef960`): Three and Four 720 wu apart on the link (the pivot follows the
  players' centre at 50 wu/s, the link turns once in 8 bars); walls from the corners on each pulse (3 and 4 a bar, arcs of
  80 and 60 degrees, a gap in front of every face, 10 thick, 260 wu/s, 2.6 s), the One with both gaps on the link; only the
  faces hurt (x 1, Solo x 1.25); Hemiola (the host's word, from the bar line after 60% joint health): the turn reverses every
  second bar, Four's walls at 300, the link burns on beats 3-4 of every second bar after a bar of dashes; Solo: the survivor
  plays both counts (its own ring and the dead one's, turning the other way, gaps together on the One) and walks at 70 wu/s.
  A wall that hits is erased there (this device's own); a Nova erases what its front reaches (on every device: it follows
  the Nova). Its death: a last bar of sparks along the link, the break and 240 Shards on the One that ends the Track.
  Camera 0.75.
- **Feedback** (`world_feedback.cpp`, `89322d5`): the tape (`World::tape_`) records every present player each tick (place
  relative to the Head, aim, alive), every shot their ship and Riffs fire (from where and which way; past 120 a bar every
  second one) and their Novas. Each Echo plays a bar back from the mirror point (220-900 wu), the shots turned, at 400 wu/s
  for 3.2 s as enemy bullets (`Bullets::echo`: a Nova clears them; at most 500), and counts as playing for two sixteenths
  after each primary shot. The Head takes x 1 only while every Echo is silent (co-op: the silent share). Regeneration's second
  Echo, two bars back, stands where the player stood; Howl runs the tape at two and four beats and burns the Loop (15 wu a
  bar line, to 300). Novas come back as Nulls (a danger ring growing as a Nova's front). Echo bodies, Nulls and the burning
  Loop take a cell. The view: the loudspeaker (its cone pushing on the kick, the shutter across or drawn back, the gold
  core), the Loop with sixteen slots, the next bar's ticks in each player's colour and the gold head on the sixteenths, the
  Echoes as their player's ship in the danger colour with three after-images and the dashed line to where the player stood,
  echo shots as streaks with the threat rim. Its death: the tape cut, the break and 320 Shards two bars on.
- **Mini-bosses batch 2** (`world_miniboss2.cpp`, `a6c2843`): the **Moth Queen** (holds 520 wu and circles once in 8 bars; a
  court of twelve Moths warps in round the player and spirals in over three bars, bursting into rings of six on the fourth
  while she is open x 2; rosettes of five on every eighth, 12 degrees apart; from half health two courts of eight turning
  apart and rosettes in pairs, open for the second half of bar four); the **Brood Mother** (drifts at 40 wu/s; six eggs on
  beat 1 that die to one shot and hatch on beat 3; her dome x 0.25 while a broodling lives, x 2 with none; from half health
  two clutches of five; her last clutch is Shards); the **Blink Star** (a 420 wu star round where the player stood, an edge
  a beat, all five lethal for a beat on bar two's beat 2, dazed x 2 at its first point; from half health decoys at the
  points throwing fans and the star a quarter larger); the **Sentry Array** (eight turrets on a 340 wu fenced ring: the fence
  keeps ships and stops shots; the turret under the hand is open x 2, the rest fire five inward on every beat; they step
  round to stand evenly after a death; with four left the clock runs backwards every second bar and the volleys come on the
  eighths). Track 4 draws from the first two, Track 5 from the others.
- **Sound** (`src/game/pending_sounds.cpp`): a row for every new cue: Event Horizon's count (the pad voice gliding down),
  fed (impact and gong, louder with the meal), starved (the reversed cymbal), the wave (the whoosh); Polyrhythm's drums on the
  Track's root and flat second rising a semitone a quarter of health (`NoteRule::KeyRoot`), the gong on the One, the link's
  ticks and impact; Feedback's echoes (each player's own lead, quieter; the second Echo 6 dB more), the held root while the
  Head opens, the swallowed shot's hat, the Null, the cut; batch 2's hats, plucks, risers, toms, chirps up the chord
  (`ChordUp`), bells, the Blink Star's pentatonic run (`PentaRun`), the Sentry's woodblock up the scale (`ScaleUp`). What the
  engine lacks is written in each row (below).
- **Co-op**: the fights' decisions that read where players stand are the host's (`kGeomHorizonBar`, `kGeomPolyHemiola`,
  `kGeomMothCourt`, `kGeomBlinkStar`); the Horizon's meal travels in the heartbeat's part slot; deaths and health steps as
  always. Protocol `0x52460505` (the enemy states `kLane`, `kCourt`, `kEgg` and the new decisions; with the network follow-up's motion corrections, merged here). `src/net` change: the
  version in `protocol.h` only.
- **Tools**: `--fight horizon|polyrhythm|feedback|moth_queen|brood_mother|blink_star|sentry_array[:phase[:hold]]` (a fight
  past the run's set list stretches it for that run); `scripts\review-m5.ps1 -Part part4` (34 pictures and 6 strips); the
  crowd sim reports first deaths in Tracks 1-3 and in the whole first pass.

### The lead's rulings of the afternoon, built with part 4's close

- **Beams v2.5** (look.md section 5, `3c110c7`): a beam is one thin line and its glow (`PlayView::drawBeam`). The core on the axis
  (2 px Accent, Bar Line, the Beam Boost, Standing Wave; 3 px Lancer; 4 px the Downbeat Cannon), 70% white and 30% the owner's
  colour at the nose running to the owner's light colour (a Fusion's half white), 1.6 of the shot level at its peak (the Cannon
  2.0); the glow a Gaussian strip of the owner's colour (a Fusion's gold), 0.25 on the axis, sigma w / 5, drawn with the pools
  as round Gaussians a sigma wide two sigmas apart (their sum along the axis is the strip, its ripple under 2%), coming up over
  the first 20 wu; under 12 wu the core alone; no edge lines, no fill. A beam on a beat peaks two frames and is gone over 110 ms
  with its glow; a held one keeps its glow and moves its core between 1.0 and 1.4 on the 1/32 grid. Reduce flashing: peak 1.0,
  no white, glow 0.15. `kDownbeatHalfWu` 60 -> 32 (no beam hurts wider than 64 wu); the Cannon's kills a minute before and
  after belong to the "no pick is a trap" table, which stays with part 7's balance pass.
- **A stream of shots reads as a line** (look.md 5, GAME_DESIGN 6.2 row 10, 6.5, `3b2f03b`): Wave is a standing curve fixed
  to the aim line and measured from the muzzle (each shot's phase by its own flight, `Shots::life0`; a shot starts on its
  curve), one wave to a beat of flight, 28 then 40 wu, every Split lane about its own direction with the same phase. Helix
  Array: the Split lanes stop fanning and ride strands in mirrored pairs (the outer 80 wu, the pairs inside 2/3 and 1/3), the
  middle lane straight, one wave to two beats; `kHelixAmp` is gone; the Wave's wider hit radius stays.
- **Thinning never flickers** (GAME_DESIGN 6.6): thinning starts above the soft cap (576) and ends below three quarters of it
  (432), only on a beat (`World::updateThinning`); while it lasts every lane fires on every step and the lanes beyond the
  middle three fly half as far; the multipliers' extras wait above the soft cap as before; above the second cap (800) the middle
  lane alone. The alternate steps are gone.
- Tests: the Wave's curve (every shot within 0.5 wu of A sin(2 pi age / beat), sideways at most 0.6 of forward), Helix Array's
  strands (no fan, mirrored pairs at three levels, the middle straight), the thinning (on and off only on a beat, half-reach
  outer lanes, every lane firing).

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Tests (`5107877`) | audio 2,679 checks, sim 5,761 (new: Event Horizon, Polyrhythm, Feedback, batch 2, the shot stream), net 131,366; determinism passes (`78fd9381c93838c6` play, `fcbe62dd9e1c369a` reckless: moved from part 3's with the six-Track set list in `b3b967f`; the shot stream's rules did not move them, the two-minute bots neither take Wave nor reach the soft cap) | all pass |
| Loopback soaks, 4 players, 10 min (test seed) | after the merge of `m4-net` (`f14beab`): clean **8.03**, impaired **5.13** wu RMS; with the shot stream's rules (`5107877`): clean **10.52**, impaired **5.21**. Clean over ten seeds (`scripts\net-seeds.ps1`): mean 4.80, median 3.81, best 2.89, worst 10.52 (the test's seed); seed 5 fails on upload (61.7 kbit/s against 60). For the network engineer; not tuned here | < 12 wu |
| Careful bot, 20 x 40 min (`--sim crowd`, final tunes) | first deaths in Tracks 1-3: **0 of 20**; in the first pass (Tracks 1-6): **4 of 20**; median 27.9 min; boss fights lost 7.2%. At home: Event Horizon **54 s**, Polyrhythm **21 s** (3.0 wall hits a fight, 4 of 25 lost), Feedback **65 s**, Moth Queen 23 s, Brood Mother 19 s, Blink Star 22 s, Sentry Array 22 s; Longwave 72 s, the Choir 69 s | the pages: Horizon about 80 s, Polyrhythm 100-120, Feedback about two minutes, mini-bosses 25-35 |
| Careless bot, 20 x 40 min | Tracks 1-3: **1 of 20** (9.8 min, T3); the first pass (Tracks 1-6): **6 of 20** (T5 1, T6 4); median 26.5 min; Feedback 124 s with 15 hits a fight (its own echo shots: it does not dodge) | the lead's line: at most 2 of 20 in the first pass (set when the first pass was Tracks 1-3) |
| `--loadout max` (desktop, the part 3 worst case) | the live shots' peak **623** of 1,024 (the old alternate steps: 593); thinned 1.7% of the ticks; the second cap never reached; nothing refused by the hard cap; the largest sideways step of any player shot **0.41** of its forward step | 0.6 |
| Beams (`scenes.py light`, 2400x1080) | `up_barline` **52.8%** dark, **1.3%** above 200; `fusion_cannon` 62.5%, **0.7%**; `up_accent` 64.3% / 0.5%; `riff_lancer` 62.0% / 0.7%; `boost_chorus_beam` 62.8% / 0.5%; `up_standing_wave` 61.7% / 0.8%; no core wider than 4 px | barline >= 50% and < 8%; cannon < 6% |
| Moto (shared phone, our APK of `5107877` deployed before each run, the lock; `bench_play`, `--tier low --nohits`) | **every run 0-0.06% missed, panel P95 one period, heap allocations 0**. Track 6's last Drop (`--drop 6.3`, the Feedback fight): up to 389 enemies in view (764 alive), 273 enemy bullets; main thread P95 4.7 ms, GPU P95 14.2 ms, tick P95 0.50 ms. `horizon:2:hold` 4.1 / 13.5 ms, 776 bullets alive at most; `polyrhythm:1:hold` 4.5 / 14.4 ms; `feedback:2:hold` 4.6 / 14.4 ms, 408 in view and 517 bullets (its echo shots), the worst frame 32 ms; `moth_queen:1:hold` 4.5 / 14.4 ms, 875 alive; `sentry_array:1:hold` 4.3 / 14.1 ms, 463 in view; `--fast-forward 640 --nohits --loadout max` (60 s) 5.1 / 13.8 ms, tick 0.37/0.78/3.47 ms, 898 alive, 0.03% missed | P95 <= 16.9 ms, missed <= 5% |
| Pools on the Moto (new: the result's `overflow_*` fields and a log line when a pool overflows) | the runs that jump to Track 5 (`--fight polyrhythm`, `--fight sentry_array`) fill the **enemy bullet pool (2,048)** in Track 5 cycle 1's Drop during their fast-forward, run bars 17-20, before the fight: 10,237 bullets refused, the same in all three runs (the phone's simulation is deterministic; the desktop's same run diverges and drops none). The jump starts the Track with no upgrades, so the crowd and its shooters pile up (as `--drop` runs do). Nothing dropped in any measured window, nor in the crowd sims' 40 runs of 40 minutes. Not changed (the lead's rule: report first) | |
| Review sets, Debug | `captures\m5\part4\`: 34 pictures and 6 strips (`.small.png`, contact sheets); `captures\m5\part3\` retaken: `up_accent`, `up_barline`, `up_standing_wave`, `riff_lancer`, `boost_chorus_beam`, `fusion_cannon`, `up_wave`, `up_helix`, and the strips `up_wave_strip`, `up_helix_strip`; 0 validation messages | 0 |

### Not done or not verified

- **Polyrhythm's length**: the bot cannot ride its gaps. Walls are thrown from where the rotors were (they swing with the
  link at about 170 wu/s), so even beyond the walls' reach it is caught; at x8 (35 s) it took 7 wall hits a fight and lost half
  its fights. The health is x3 (21 s) until a better bot or a play test says how hard the walls are for a player.
- **Event Horizon at 54 s and Feedback at 65 s** are under their pages' lengths. Their health was raised as far as the first
  pass allowed in one pass of measurements; part 7's balance pass takes them up with the ten Tracks.
- **The first pass is now six Tracks**: the careless bot dies in it in 6 of 20 runs (Tracks 1-3: 1 of 20). The lead's line
  of 2 of 20 was set for Tracks 1-3; the line for Tracks 1-6 is the lead's to set.
- **Not built**: Riff echoes that fire nothing (Orbitals, Pulse Ring, Sweeper) and what shots do after they leave, on
  Feedback's Echo; the gold eggs to touch (Brood Mother's last clutch is plain Shards); Event Horizon's entrance lean of every
  light and its death spiral of everything alive (the disc and rings implode); the Echoes flying home as Shards. Broodlings move
  at their species' speed (the page's 260 wu/s and the half's 15% are not applied).
- **Sounds** the engine lacks are written in each row of `src/game/pending_sounds.cpp`: the Horizon's drone (a sub sine), the
  bend of its count, the starved cut of pads and bass; the band's pad and filter on Feedback's open and closed Head, its
  delay feedback in Howl, the echo's octave-down low-passed copy; the Moth Queen's clap and the bed dropping to kick and pad;
  the Brood Mother's thread pad; the Blink Star's five-note stab. By ear: nothing (the PC runs are silent).
- The "no pick is a trap" table (and in it the Downbeat Cannon's kills a minute before and after its 64 wu): with part 7.
- The flaky audio check "seq latest" failed once under load on the merge (passed on three reruns and in the final run).

<!-- part 4 acceptance below -->

### Decisions taken alone

- **Event Horizon**: its eruption leaves from the outer ring (320 wu): from the photon ring the Collapse's pull on bullets
  (270 wu/s there) would take a 200 wu/s bullet straight back. The pull on bullets and Shards is a speed added to theirs (the
  page's numbers are wu/s), so slow bullets curl back as the page says. The fight's first bar line gives no verdict (nothing
  eaten yet). Collapse's jets are shown every bar along the coming axis (whether the bar is fed is known only on its line).
  Its 300 Shards come with the flash, a bar and a beat after it dies. Not built: on the entrance "every light leans toward one
  point" (the grid pinches); the death spiral of everything alive (the disc and rings implode).
- **Polyrhythm**: the link starts across the screen (it is wide). The rotors turn smoothly between pulses, exactly a step at
  each. The link's dashes run from the bar before until it burns on beat 3. Solo's second ring is the dead rotor's shape
  (its corners and arcs) from the survivor. Its break and Shards come on the bar line the Track ends at (the first at least
  half a bar after the death), so the "last bar" of sparks is half a bar to a bar and a half. Wall hits count as beams in the
  crowd sim's table.
- **Feedback**: the Echo plays back the shots as they left (straight, at 400 wu/s): what a shot does after it leaves
  (Pierce, Split, Ricochet, homing) and the Riffs that fire nothing (Orbitals, Pulse Ring, Sweeper) are not echoed; Boosts
  are echoed through the shots they change. A Null is a danger ring growing as a Nova's front does (to 1,400 wu in 0.35 s):
  a Dash through it or distance. The second Echo stays unreflected in Howl too. The phase follows the health on every device
  (as Event Horizon's). The 320 Shards come at the break (the Echoes flying home as 100 of them is not drawn). The bot fires
  in the page's phrases (two beats on and two off, swapped; a bar on and two off; two beats on and four off) and holds its
  Nova.
- **Batch 2**: the Moth Queen's two courts (half health) close a bar apart by the inner ring spiralling in over two bars and
  the outer over three; the Brood Mother's eggs land a tick after they are laid (so a shot can take them at once); her last
  clutch is sixteen clumps of ten Shards (no gold egg to touch); broodlings move at their species' speed (the page's 260 wu/s
  and the half's 15% are not applied); the Blink Star's fifth hop is on bar two's beat 2 as the star fires, its first point on
  the side the Star stood; the Sentry Array's fence is kept as circles (the ring's apothem inside, its radius outside).
- **Track 6's mini-boss** is Mirror Mimic's (batch 3, part 6): until it is built Track 6 draws from batch 2.
- **The set list is six Tracks** now (`kBossesBuilt`); two loop tests pin a set list of three to keep testing what they test.

### Numbers chosen (`src/sim/tuning.h`, part 4; the pages' prose numbers in `src/sim/boss_prose.h`)

Event Horizon: the jets 1,800 wu long, 22 wu half width; health x 1 (`kHorizonHealthTune`). Polyrhythm: the burning link 16
wu half width; health x 1. Feedback: a Null's front 30 wu; health x 1. Moth Queen: the burst at 200 wu/s, the court's step at
600 wu/s, the Queen at up to 300 wu/s. Blink Star: the decoys' fans at 220 wu/s, 15 degrees apart. Sentry Array: a step round
over a beat. Batch 2's health x 1 (`kMiniHealthTune[4..7]`).

## M5 part 3 "Every upgrade" — 2026-11-13

All of GAME_DESIGN 6.2 to 6.5 in the game: the 28 Mods, 9 Riffs (4-12), 12 Boosts (3-6, 9-16) and 8 Fusions that the first
slice did not have, and the Trio dealt by the lead's rule of 2026-11-13 (6.1). The simulation is `src/sim/world_upgrades_m5.cpp`
(hooks in `world.cpp`, `world_upgrades.cpp`, `world_enemies.cpp`), the view `src/game/play_view_upgrades.cpp` and
`play_view_shots.cpp`, the sound `src/game/upgrade_sounds.cpp` (the band's whole loadout and its cues).

### What was built

- **Weapon Mods**: Fork (two half-damage shots 20 degrees apart on the first hit; Fractal: again), Seeker (shots turn toward
  the nearest enemy, or the fight's body, within 320 wu: 3, 6, 10 rad/s), Heavy Step (every fourth volley's middle lane is a
  slow orb that blows: 90/120/150 wu), Accent (the volley on beat 1, then 1 and 3, then every beat: a 1,500 wu piercing lance
  at 3x the shot in the middle lane's place), Echo (each volley again a beat later at half damage; two echoes), Wave (a sine
  across the line, 28/40 wu, the hit radius x1.6/x2), Rear Guard (off-beat shots backward, then sideways), Arc (a kill
  jumps to 2, 3, 4 more within 220 wu), Burn (two beats alight, a beat's fire x the stacks, a burning death lights its
  neighbours), Shatter (3/5/7 shards from a kill), Gravity Rounds (a hit tugs the bodies within 110/150 wu toward it).
- **Defence and movement**: Afterimage (the enemies chase a decoy where the Dash began, 2/4 beats), Reserve (a Dash charge a
  stack), Shockstep (a ring where the Dash ends, 140-220 wu, x2 on a Perfect Dash), Aegis (an arc 30-60 wu out turning once a
  bar, 60/90/120 degrees, stops bullets), Last Light (a hit fires a free mini-Nova of 420 wu, once a cycle), Slipstream
  (+8% speed).
- **Economy**: Prospector (25% a stack of a Shard more), Nova Stock (+1 capacity and a Nova), Encore (+1 bar on every
  Boost), Lucky Break (a fourth and fifth orb), Collector (a Boost every 250, then 150 Shards).
- **Rhythm**: Syncopation (the off-beat shot +50%/+100% and +2 pierce), Bar Line (a laser cross through the ship on every
  bar line; diagonals; 1,800 wu), Crescendo (+5% a clean bar to +50%/+100%), Kick Pulse (a knockback ring on every beat
  outside the Break, damage from 2, wider at 3), Afterparty (the band's rule: the Surge a grid step faster a stack, its
  second gear held two bars longer a stack, x2 score and magnet in those bars).
- **Riffs 4-12** at levels 1-5: Lancer (a beam on beat 4, beats 2 and 4 from level 3), Minelayer (a cluster a bar, all
  blowing on the next bar line), Storm Coil (triplet-eighth zaps at the nearest 1-5), Swarm (6-16 homing missiles a bar on
  the 1/32), Singularity (a well 300 wu ahead pulling three beats, then blowing), Mirror (a wingman at 50-100%), Comet
  (boomerang discs, 1-3), Sweeper (a laser arm, two from level 3), Flak (pellets on beats 2 and 4, exploding at 5). Hyper
  doubles the Lancer's, Storm Coil's, Flak's and the Sweeper's grid.
- **Boosts**: Supernova Rounds, Colossus (x3, invulnerable, rams), Time Stretch (the enemies and their bullets at half speed),
  Freeze Frame (the frame's bodies stop for two bars and shatter in one hit, chaining), Black Hole (the next Nova pulls three
  beats, then blows 500 wu), Chain Storm (lightning to the 24 nearest each beat), Chorus (two ghost ships), Beam (the primary
  as a laser), Ricochet Fever (+5 bounces, x1.1 a bounce), Confetti (six homing sparks a kill), Fractal (forks twice),
  Carousel (six bombs, one launched each beat).
- **Fusions** (the gold orb when both ingredients are at their caps): Helix Array, Standing Wave, Tesla Bloom, Shard Halo,
  Downbeat Cannon, Collapse Field, Murmuration, Quartet.
- **How a Trio is dealt** (GAME_DESIGN 6.1, the lead's ruling): the first orb is firepower, the others the whole pool by
  weight (`tuning::kDeal*`), no two alike, the Riff guarantee; Trios hold up to six orbs (three, Lucky Break's two, a
  Fusion), on the wire too.
- **The band's clock**: Singularity pulls in the song bars whose index + 1 is a multiple of four and blows on the next One;
  the Sweeper's arm stands at twelve o'clock on the One of every second song bar (`World::setSongBarShift`, set by Play from
  the band's offset); Minelayer never blows in the first bar after it is taken. Tested.
- **Sound**: `Band::setLoadout(slot, PlayerLoadout)` with every Mod, Riff, Boost and Fusion (this device's player and the
  teammates'); the cues (Fork, Heavy Step, Arc, Burn's crackle, Shatter, Gravity, the decoy, Shockstep, Aegis, Last Light,
  Collector, Ricochet's bounce, Storm Coil, Supernova, Colossus, Freeze's shatter, Confetti, Black Hole's Nova, Crescendo's
  fall on a hit) counted a frame and posted once a kind. The fights' pending sounds now use the audio engineer's eight
  recipes and chordNote: none is pending.
- **Co-op**: whatever moves or stops enemies because of one player's upgrade is shared or the host's. The decoy and Black
  Hole's singularity reach every device by the player events (dash, Nova with its place), a Singularity's well by a new one
  (`kPeWell`), Kick Pulse and Colossus push from the poses on every device; Gravity Rounds' tug is the host's (at its own
  shots' hits and the copies of the others'), Freeze Frame is decided by the host and carried in a spare bit of the
  enemies' corrections. Damage is dealt by the owner's device (claims), as before. Protocol `0x52460503`. `src/net`
  changes: `protocol.h/.cpp` (a Trio's six orbs, `kPeWell`'s range, the correction's frozen bit, the version),
  `session.cpp` (the six orbs; `kPeWell` from the well's event).
- **Tools**: `scripts\review-m5.ps1 -Part part3` (22 scenes, `play_pictures_m5.cpp` kUpPics); `--loadout max` (every Mod
  at its cap, Swarm, Flak, Storm Coil and Sweeper at 5, their Fusions, Chorus and Fractal held: the worst case); the crowd
  sim's "first run's build" line (Mods at their cap as Track 3 ends, a Fusion as Track 5 ends).

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Tests (`5e6ffeb`) | audio 2,679 checks, sim 5,649 (new: the rules above, the shot caps with the heaviest build into a wall of enemies for eight bars, the band's three timings, the dealing's weights and firepower orb, Lucky Break's and the Fusion's orbs), net 131,340 (the frozen bit); determinism `f4df84a4414eaa0f` / `368f4ea929adbd19` | all pass |
| Loopback soaks, 4 players, 10 min | before the dealing (`e205aff`): clean **5.63**, impaired **10.05** wu RMS. With it: clean 10.33, impaired **12.48** at the test's seed (seeds 2-4: 9.8, 8.8, 7.3): the four bots now reach the Hive fight and Track 3 inside the ten minutes, and the Hive's darts (their shape guessed until the host's arrives) are the worst windows (30-70 wu) | < 12 wu |
| Careful bot, 20 x 40 min (`--sim crowd`) | median first death 16.4 min (Tracks 5-6), **1 of 20** in the first pass (9.9 min, the Choir), boss fights lost 9.7%; Longwave **70 s**, the Choir **65 s**, Alpha Hound 16 s, the Hive 18 s, the Ram 18 s, Carom King 16 s at home | the lead: Longwave about 90, the Choir about 75 |
| Careless bot, 20 x 40 min | median 15.7 min, **1 of 20** in the first pass (9.8 min, T3); seeds 21-40 (14 min): 3 of 20 | at most 2 of 20 |
| The build (careful, dealing, before the last health step) | as Track 3 ends: 5.2 Mods at their cap, two or more in 20 of 20 runs; as Track 5 ends a Fusion in 10 of 10 | two by Track 3; a Fusion in half |
| Moto, the worst case (`bench_play --fast-forward 640 --nohits --loadout max`, 60 s; our APK, the lock; shared phone; APK of `b3cacc3`) | **0% missed**, panel P95 1 period, CPU max 20.2 ms; main thread P95 5.1 ms; GPU total P95 14.3 ms; sim tick p50/p95/max 0.37/0.77/3.47 ms; up to 898 enemies (581 in view), 18,673 sparks; heap allocations 0; Track 5 (the loop) | P95 <= 16.9 ms, missed <= 5% |
| Moto, the bot's own build (`--fast-forward 640 --nohits`, 60 s) | 0% missed, main thread P95 4.5 ms, GPU P95 14.3 ms | as above |
| Review set, Debug (`captures\m5\part3\`) | 22 pictures with `.small.png`; 0 validation messages | 0 |

### Decisions taken alone

- **The dealing's weights** (the lead's starting values, moved as allowed): a new Riff 3 (not 2) and Extra Cell and Regen
  count 3 before they are held. Measured with the careless bot: with Regen at 1 it grew back 5 cells in 20 runs (107 in play
  test 2's smaller pool) and 9 of 20 runs died in the first pass on seeds 21-40.
- **The bot picks by a score table** (`sim/bot.cpp`): the staples and the defensive Riffs (Orbitals, Pulse Ring,
  Sentinel) and the cells' Mods high, a Fusion's ingredient +6 when the other is held, no bonus for a new Riff (with it the
  careless bot built Riffs before firepower and killed a sixth as fast).
- **Boss health below the lead's lengths**: Longwave's tune 2.2 (70 s) and the Choir's 4.2 (65 s). 2.7 and 4.8 gave 86 s and
  70 s but the careless bot then died in the first pass in 3 of 20 runs; the hard line won.
- **Kick Pulse** rings on every beat outside the Break (the simulation has no kick pattern; the band plays its tom on the
  kick). **Time Stretch** slows the whole world in co-op (it is one). **Slipstream** keeps the table's +8% though 6.6 asks
  no small percentage (Design's table; for the lead). **Last Light's** mini-Nova is 420 wu.
- **Accent's lance and Heavy Step's orb** take the middle lane's place; **Echo** fires from where the ship is a beat later;
  **Mirror's** wingman flies beside the ship firing along the aim; **Quartet's** three cover the sides and the back;
  **Chorus's** ghosts fly behind, 80 wu out.
- **Beam** fires no shots: each grid step burns its line, as wide and as hard as the volley's lanes. **Collector** gives its
  Boost at once. **Black Hole** takes the next Nova while the Boost is on (the Boost ends). **Freeze Frame's** "in view" is
  the player's frame. **Carousel** always shows six bombs. **Fractal**: forks +2, at most 3 deep.
- **Caps**: everything that multiplies shots (forks, echoes, wingmen, Rear Guard, shrapnel, sparks, missiles, pellets,
  bombs) stops above the soft cap of the player's live shots (576); the test fires the heaviest build into a wall for eight
  bars: no shot held back by the hard cap.

### Numbers chosen (`src/sim/tuning.h` "M5 PART 3" block)

Fork 20 degrees, half damage. Seeker 3/6/10 rad/s within 320 wu (missiles 6, Murmuration 9 within 640; sparks 8). Heavy Step:
the orb at 0.45 of the shot's speed, x3 its radius, 1.6 s; blasts 90/120/150 wu for 4/6/8. Accent: 3x, 1,500 wu, 14 wu half
width. Echo half damage. Wave 28/40 wu, 0.2 s, radius x1.6/x2; Helix x3. Arc 220 wu, 2 a jump. Burn 1 a beat x the stacks,
two beats, spreads within 90 wu. Shatter 3/5/7 pellets at 700 wu/s for 0.3 s. Gravity 110/150 wu, 70/110 wu/s a hit.
Afterimage 2/4 beats. Shockstep 140/180/220 wu for 3/5/8. Aegis 60/90/120 degrees, 30-60 wu. Last Light 420 wu. Collector
250/150. Syncopation x1.5/x2, +2 pierce. Bar Line 900/900/1800 wu, 16 wu half width, 5. Kick Pulse 160/160/240 wu, a
push of 380 wu/s, 2 from stack 2. Lancer 6 (x1.5 at 5), half widths 10/16/16/22/22, 1,600 wu. Mines 110 wu for 8. Storm
Coil 220/220/260/260/300 wu, 1.5. Swarm 6/8/10/12/16 at 900 wu/s for 2, 1.4 s (Murmuration 7 s). Singularity 300 wu ahead,
pulls 260-340 wu at 260 wu/s, blows 200-260 wu for 12/12/18/18/24. Mirror 50/60/70/80/100% at 90 wu. Comet out 520 wu,
22 wu x 1/1.5/1.5/2/2, 1/1/2/2/3 discs, 3 an eighth. Sweeper 240/300/300/360/360 wu, 6 a pass. Flak 12/16/20/24/24, at 5
blasts of 40 wu. Supernova 70 wu. Colossus x3, 20 a tick. Time Stretch 0.5. Freeze chain 90 wu. Black Hole pulls 700 wu at
300 wu/s, blows 500 wu for 50. Chain Storm 360 wu, 24, 2. Chorus 80 wu. Beam 1,400 wu, 10 + 4 a lane. Fever +5, x1.1. Six
sparks. Carousel 70 wu, bombs at 500 wu/s for 1 s, 90 wu for 6. Downbeat Cannon 60 wu half width, 2,400 wu, x8. Collapse
200 wu at 200 wu/s. Standing Wave 1.5 an eighth. The dealing: new Mod 1, held 3, new Riff 3, Riff level 3, cells' Mods 3,
Fusion ingredient x2. Bosses: Longwave 2.2, the Choir 4.2.

### Not done or not verified

- **"No pick is a trap" table** (6.6, the lead's ask): not built; with the balance pass at the end (part 7). It needs a
  headless mode that forces a loadout and sets Track 3 cycle 2's Groove and a Choir fight, and about a hundred runs a
  column (five seeds each, both measures): more than half a day with the rest of part 3.
- **The impaired soak at the test's seed is 12.48 wu RMS** (above). The five upgrade effects that move or stop enemies
  were measured one by one before the dealing (Gravity Rounds alone took it to 14-15, Freeze Frame to 14.7, until both went
  to the host: 10.05 with everything on); since the dealing the bots get further in the ten minutes, and the worst windows
  are the Hive's guessed shapes (part 1's). A client that waits for the host's shape instead was tried: 18.9, reverted.
  Not hidden: `test.ps1` fails on it at that seed.
- By ear: the new cues through the band (the PC runs are silent). The boost name cards overlap when two Boosts come in
  the same moment (`boost_supernova_chain.png`), a HUD layout matter.
- Co-op on devices with the new upgrades (the loopback soaks only). The OnePlus and the tablet: not used.
## M5 part 2 "Species: Tracks 2 and 3's own families" — 2026-11-13

`design/species_m5.json` is now `design/species.json` (`git mv`, the lead's instruction); the loader reads all sixteen
families' base numbers and motion, every variant number the game uses, the 106-entry roster, and budgets and crowd
targets for ten Tracks. Built in this part: the families **Flanker, Skitter, Moth, Brood, Blink** and the variants Tracks 1
to 3 use (**Drifter B, Hound C, Carom B, Moth B, Brood B and C, Blink B, Flanker B**; Hound B and Sentry B were the
slice's), with Track 2's and Track 3's rosters. Track 1 is untouched.

### What was built (`src/sim/world_families.cpp`, `species.*`, the view)

- **The table**: sixteen families in the enum (the slice's six first, as on the wire), 240 species and one more row, the
  **broodling** (the Brood's mini: brood.motion.mini*, fodder, chases like a Hound, drawn with `brood_1`). Families
  without behaviour yet (Wyrm, Tether, Weaver, Well, Mimic) are never picked (`familyBuilt`).
- **Flanker**: lands off the line to the player and comes in on a slant, then circles at orbitRadius 260 closing at
  closeRate 40 wu/s. B: on the orbit, a beat's tell, then a shot inward (at the player) on every beat.
- **Skitter**: chases; in a player's line of fire (within 900 wu along the aim, 36 wu plus its radius beside it) it
  sidesteps dodgeDistance 90 square to the line in 0.18 s, then not again for two beats.
- **Moth**: a clock round the player: its place turns a quarter turn a bar and steps a quarter of closePerBar 60 inward
  on every beat, down to minRadius 60; it flies to its place at up to 1.6 x its speed. B: after a beat's tell, two
  petals opposite each other on every eighth, each pair turned on 20 degrees, at 180 wu/s.
- **Brood**: a slow carrier toward the player. B: a clutch of two on every bar line while engaged, at most six of its
  own alive. C: bursts into eight broodlings on death. Hatching and the swarm are the host's (the clients hear them).
- **Blink**: does not travel: it stands on a point of a five-point star round the player, hops to every second point on
  each beat (a white blink flash), four hops, then a beat's tell (the Ram's chevrons along its line), a strike through
  where the player stood at 700 wu/s for a beat, two beats' rest, a new star. B: three bullets left along each hop,
  flying on along it.
- **Drifter B**: a slow aimed shot every second bar on its downbeat, after a beat's glint (each Drifter its own bar).
  **Hound C**: a frontal shield of 120 degrees (a white arc) turning to the player at 1.6 rad/s; a shot that meets it is
  shed. **Carom B**: three bullets fanned 50 degrees from every bounce (walls and the Pen).
- Every shooter's glint gives it the edge indicator off screen (`Species::ranged`: Hound, Sentry, Drifter, Flanker and
  Moth B).
- **The director**: the roster runs to Track 10; a Track picks what has debuted by its cycle; the loop (after the set
  list) picks everything that debuted in the set list's Tracks. Budgets and crowd targets: the set list's Tracks from the
  data, past it the last Track's last value and a step (with three Tracks in the set list: the first slice's rule,
  unchanged). Packs (chosen): Flankers 4, Skitters 3, Moths 6, Blinks 2, a Brood or a Sentry alone.
- **Co-op**: an enemy's correction carries one more byte, a family's own counter (a Blink's hops), and the new states
  (protocol `0x52460502`, 31 bytes a correction, 38 to a packet).
- **The Pen fights' crowd** (the lead's ruling after part 1): 0.35 of the Drop's, never more than 120 bodies inside
  (minibosses.json `common.crowd.penScale`, `penMax`); fodder inside beyond 120 dissolves as the Pen rises. `ram_*` and
  `carom_*` retaken.
- **Tools**: `--drop <track>.<cycle>` (benchmarks: that Track from its start, fast-forwarded to the cycle's Drop);
  `scripts\review-m5.ps1 -Part part2`.

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Tests | `scripts\test.ps1` passes: sim 5,621 checks (new: the table, Moth's clock, Blink's star and strike, Brood B's cap and C's swarm, Hound C's shield, Drifter B's tell, Flanker's orbit, Skitter's sidestep); net 131,340 (the correction's new byte); determinism `30e1c914ed5830b6` / `e2de53b4dc0ecca6`; soaks clean 6.4 wu RMS, impaired **9.97** wu RMS (was 7.9; below) | all pass, < 12 wu |
| Careful bot, 20 seeds x 40 min | median first death **31.10 min** (Tracks 8-9), none in the first pass, boss fights lost 4.6% (Kaleidos 3/79, Longwave 2/67, Choir 4/56) | 26-36, none, 1 in 10 |
| Careless bot, 20 seeds x 40 min | median **19.15 min**, **1 of 20** in the first pass (seed 4, 6.0 min, in Longwave); seeds 21-40: 17.95 min, 0 of 20 | 17-25, none |
| Crowd in view / target (careful, Groove) | Tracks 1-3: 0.50 / 0.53 / 0.50, 0.50 / 0.46 / 0.43, 0.41 / 0.41 / 0.37 (as before) | |
| The Moto, Track 3's first Drop (`--drop 3.1 --tier low --nohits`, 30 s; shared phone, our APK, the lock) | missed **0%**, P95 1 period, max 19.0 ms; up to 774 enemies in view (the jump brings no upgrades from Tracks 1 and 2, so the crowd piles up: a harder case than play); GPU P95 14.6 ms at the governor's clock; main thread P95 4.6 ms | P95 <= 16.9 ms, missed <= 5% |
| Review set, Debug (`captures\m5\part2\`) | 12 pictures, 2 strips (`fam_moth_strip`, `fam_blink_strip`) with sheets, the glyph sheets of all sixteen families (`glyphs_1`, `glyphs_2`); 0 validation messages | 0 |

### Decisions taken alone

- **A Blink's star**: hopWu (200) is the star's radius round the player (each hop to every second point is then 380 wu);
  read as a hop's length the star would be 105 wu across the player.
- **Brood A** is the carrier alone (GAME_DESIGN 7.2: B hatches, C bursts); a clutch hatches only while the Brood is
  engaged (near a player).
- **Flanker's closeRate** is wu a second while it circles (no unit given); it closes to the player's body.
- **The loop's roster**: everything that debuted in the set list's Tracks; species_m5.json's "every species comes back
  one tier higher for each time round" is not built yet (part 7, with the ten-Track loop).

### Numbers chosen (`src/sim/tuning.h` M5 part 2 block, `world_spawner.cpp` packs)

Flanker: lands 0.9 rad off the line, slant weight 0.8, circles within 1.3 x its orbit, radial reach 120 wu, turns 3 rad/s.
Skitter: turns 4 rad/s, a sidestep in 0.18 s, reads the line of fire 900 wu along and 36 wu (+ radius) beside. Moth:
catches up at 1.6 x its speed. Packs: Flanker 4 (spread 90), Skitter 3 (60), Moth 6 (120), Blink 2 (80), Brood 1;
spotlight groups Brood 1, Blink 2.

### Not done or not verified

- Variants for Tracks 4 to 10 (Skitter B and C, Moth C, Blink C, Flanker C, Drifter C, Carom C, Sentry C, Ram B and C,
  Shoal B and C): their numbers load; behaviour comes with part 4.
- **The impaired co-op soak's error rose to 9.97 wu RMS** (limit 12): the new families decide from where the players
  are (a Skitter's sidestep reads each player's aim, a Moth's clock and a Blink's star their position), and a client
  knows the others' a moment late. Not yet looked into further.
- **The enemy bullet pool filled** in 2 of the careless bot's 20 runs deep in the loop (4,297 and 4,310 bullets refused,
  Tracks 5-6): Moth B's petals on every eighth under the loop's ramps. The pool's cap held; the patterns thinned there.
- Pictures are on Track 1's palette for the family scenes (the spawner off on Track 1).
## M5 part 1 "Longwave, The Choir, mini-bosses batch 1 and the Pen" — 2026-11-13

Built on `main` (never pushed): `4c62506` (the previous content engineer's uncommitted tree, taken over: it built after two
fixes and its tests passed), `abeb273` (the bots fight the new bosses, the singing hoods, one table of pending sounds),
`f9f3ae8` (the merge of `m5-audio`: the ten Tracks), and the closing commit (`--fight`, the review set, the Moto, this
section). Designs: `design/bosses/longwave.*`, `choir.*` (with the lead's ruling `94706dc`: only a singing hood hurts),
`minibosses.*` (batch 1 and the Pen).

### What was built

- **Longwave** (Track 2's boss; `src/sim/world_longwave.cpp`, the view in `src/game/play_view_m5.cpp`). The head swims
  the figure of eight round its anchor, segment k sits k x 100 wu behind it on the path and swings with the wave
  (amplitude 150, eight segments a wavelength, tapered over the first three); the crest is the one live segment the sine
  peaks on in a step and the only one that can be hurt; troughs fire square to the body on the same steps (a fan of three
  in Chop); the jaw opens on beat 1 and for two beats after a lunge; dead segments close up and the crest skips on; a
  phase change lands on the next beat line (the rates change, never the place); the whip on the downbeat after each one;
  Whitecap and the bare head lunge (a line for a bar, a 700 wu dash on the downbeat); the bare head's rings on beats 2
  and 4. Its look: the colour running along the body, the electric-blue accent on fins, crown, eyes, the spine's core and
  the two echoes (kept on every tier), the white-hot crest with its flares and its warming neighbours, a pool under a
  trough about to fire, the lattice impulse of 140 under each crest step; the death unravels tail-first, a segment a
  sixteenth, then the head breaks.
- **The Choir** (Track 3's boss; `src/sim/world_choir.cpp`). Eight Voices on the Round (radius 380, 300 in the Fugue),
  each a note of D dorian (D4 to D5) and a hue 45 degrees from the next (kept 25 degrees off the danger colour); only a
  singing Voice can be hurt, from its step for one beat; Unison (the caller on beats 1 and 3, the Voice opposite on 2 and
  4, the caller one round each bar), Canon (the answer running round the ring an eighth apart, the cage every fourth
  bar), Fugue (two callers' streams sweeping 80 degrees, mirrored the next bar by the others, the cage every second bar),
  Solo (the last Voices circling the nearest player at 280 wu, rings of 16 on beats 2 and 4, the held note's 120-degree
  beam every fourth bar, the last Voice x 1.5). A dead Voice is a rest; the Round turns once in 16 bars, follows the
  players at 60 wu/s, stops a beat and turns back when a Voice dies. Its look: hoods, ribs, faces, lips that flare when
  it sings, halos and strings in pearl, the call string (a standing wave of three antinodes from the singer to the
  answerer, its hues to white), the answerer drawing breath, the gold core of the singer, the burning cage and the held
  note on the threat layer; **a silent hood at 0.6 of its level**. The death: the outlines sing the last chord with every
  string white for a bar, then shatter clockwise.
- **Contact with the Choir** (the lead's ruling, `94706dc`, choir.json `numbers.contact.hoodHurts`): a hood hurts by touch
  only while it sings; read from the json ("only while it sings", "always", "never").
- **Mini-bosses batch 1** (`src/sim/world_miniboss.cpp`): on the downbeat of the middle cycle's Drop from Track 2 on, in
  place of the mega-wave; drawn by the run's seed from the Track's pair, never the one met in the Track before (the loop's
  Tracks from their song's pair); the Drop holds while it lives, at most 8 bars past its 16, then it warps out and leaves
  nothing; health by Track from minibosses.json, x its factor, x 1.25 a loop Track, x (1 + 0.5 per extra player); a
  Boost orb, 60 Shards and 25 times a tier-5 body's score; three quarters of the Drop's crowd, fodder only; its name card
  for a bar and a health bar half a boss's height. **Alpha Hound** circles at 420 wu and howls a pack of six (two of five
  from opposite sides from half health) as a chevron with its danger line on beat 1, the pack charging on beat 3, its own
  lunge every fourth bar (third), winded x 2 for two beats after; at its death the pack flees for two bars with double
  Shards. **The Hive**: 120 darts and the Queen (12 hatched a bar, up to 160, counted against the enemy cap), Ring, Spear
  and Wall in turn every two bars with a beat of white flash before each, two hives of 60 with a white decoy from half
  health; every dart falls as Shards at her death. **Battering Ram** in the Pen: turns, locks on beat 3, charges on the
  next downbeat at 1,400 wu/s, cannot stop with a wall within 300 wu behind the mark: the wall rings 16 bullets and it is
  stunned x 2 (three beats, two from half health, when it also charges twice and leaves a wake); its front 120 degrees
  sheds every shot. **Carom King** in the Pen: legs trimmed so every bounce lands on a beat, 8% faster each (to x 1.6),
  five bullets fanned 40 degrees from each bounce, gold x 2 for a beat after it; a Prince shed at 75%, 50% and 25% (12%
  of its health, 1.3 x its speed, bouncing on the half beats; a dead Prince breaks into two Caroms).
- **The Pen**: a square of white light walls, 1,400 wu, round the players' centre on the downbeat; ships and bodies stop
  at it, shots pass; fodder outside dissolves; the crowd warps in inside; it falls when the fight ends.
- **The set list as data** (GAME_DESIGN 4.2): the loop begins after the set list's last Track (`World::setSetList`, the
  music's Tracks the game has content for: three until Tracks 4 to 10 have their fights); `songOfTrack`, `bpmOfTrack`,
  `loopRamp`, the halo burn, the ENCORE card and the palettes count from it. Tested for N = 1, 2, 3, 5, 10 (the loop at
  N + 1) and in a run (N = 2, 3, 4).
- **Co-op** as Kaleidos: health by the players present; every pattern from the song on every device; on the wire
  (`src/net`, the only changes there): the protocol version `0x52460501`; a fight's enter message carries which fight
  and the Pen's centre; the heartbeat carries every part's health (24 for Longwave, 8 Voices, 3 Princes) and the point
  that keeps the clients' fight in place (Longwave's anchor, the Round's centre, a mini-boss's body); the director's
  message carries the middle Drop's hold; decisions that read where the players stand (a Hive shape's anchor, a lock, a
  lunge's line) are the host's (`World::bossDecide`), a client guessing until it hears.
- **The bots** (`src/sim/bot.cpp`): every fight's parts are aimed at where damage will count when the shot arrives; the
  careful bot keeps off the bodies where they will be (against its own course, a fifth and two fifths of a second ahead)
  and dashes from one about to touch, rides beside Longwave's crest, keeps inside the Choir's Round (in through a gap,
  never back out across the ring), keeps out of every line shown and off the Pen's walls.
- **Sounds**: every fight's cue goes through one table, `src/game/pending_sounds.cpp` (one row a cue: the page, its
  words, what plays, and what the engine lacks), on the engine's recipes since the merge (below).
- **The camera**: Longwave and the Choir pull it to their pages' 0.75 for the whole fight (below look.json's zoomMin; the
  lead accepted it, GAME_DESIGN 9 now says so); Longwave without the lean toward the boss.
- **HUD**: every fight's name (a mini-boss's card small and for one bar), the whole health in one bar, pips for the
  Voices and the segments after it.
- **Tools**: `--fight <id>[:<phase>[:hold]]` (the run starts in the fight's home Track and the fast-forward plays on to
  it; the phase takes its parts down; `:hold` gives the health back every tick, for benchmarks); `scripts\review-m5.ps1`;
  `raveform_headless --sim crowd` reports each fight (fought, won, lost, warped out, hits a fight by bullets, bodies and
  beams, seconds to kill by Track) and `--trace` a line a second of each fight and one for each hit in it.

### The merge of `m5-audio` (the lead's instruction)

`f9f3ae8` merges the audio engineer's branch at `95e9e6c` (the ten Tracks, beats from the clock, `src/audio/cues.h`): no
conflict. After it the tests found `minibosses.json`'s batch 3 (the lead's commit of the same hour) unknown to the loader:
`BossKind` gained `MirrorMimic`, `TwinTether`, `WeaverPrime`, `DeepWell` (data only) and the health table holds Tracks 2
to 10. "On the beat" now reads the clock's beat (`BeatView::beat`), never the kick: the Trio's and Boost orbs' swell,
the Cells and their chevrons, the co-op chevron and beacon flare, the HUD's full level meter, and the bench stage's Pulse
Ring and Lancer; the kick stays where the kick is meant (the halo, the pump, the lattice's ripple, the logo). The fights'
sounds now use the engine's recipes (`Band::playRecipe`, `startVoice` / `stopVoice`, `lowPassBed`) where they can say
what the page asks; the audio engineer's report follows this section.

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Builds, tests | Desktop Debug and dev and the APK; `scripts\test.ps1` passes (audio 2,539 checks, sim, net; determinism; both 10-minute loopback soaks: clean 6.1 wu RMS, impaired 7.9 wu; 0 census mismatches, 0 allocations) | all pass |
| The careful bot, 20 seeds x 40 min | median first death **35.95 min** (Track 9); none in the first pass; boss fights lost **4.5%** (Kaleidos 3 of 79, Longwave 3 of 68, the Choir 3 of 56) | 26-36 min, none, at most 1 in 10 |
| The careless bot, 20 seeds x 40 min | median first death **19.91 min** (Track 6); none in the first pass; the Choir lost 3 of 43 | 17-25 min, none; the Choir at most 1 in 5 |
| Time to kill, careful bot (home Track) | Longwave **89 s** (T2), the Choir **84 s** (T3); Alpha Hound **22 s**, The Hive **29 s** (T2), Battering Ram **24 s**, Carom King **26 s** (T3) | 70-100, 80-110; 25-35 |
| Hits a fight (careful) [bullets / bodies / beams] | Kaleidos 0.41 [0.06/0.34/0], Longwave 0.50 [0.32/0.18/0], the Choir 0.32 [0.21/0.09/0.02]; Alpha 0.02, Hive 0.11, Ram 0.55 [0.23/0.20/0.12], Carom 0.59 [0.13/0.46/0] | |
| Mini-bosses that warped out (careful) | Alpha 3 of 45, Hive 11 of 56, Ram 8 of 40, Carom 7 of 39 (mostly in the loop, where their health climbs 1.25 a Track) | |
| Review set, Debug | 31 pictures and 6 strips, 0 validation messages | 0 |
| The Moto in each fight's densest moment (shared phone; our APK deployed before each run, the lock; `bench_play --tier low --nohits --fight <id>:<phase>:hold`, the fight held in that phase) | Longwave Chop (60 s): panel missed **0.64%**, P95 1 period, max 26.5 ms, enemy bullets up to 384, sim tick P95 0.27 ms; the Choir Solo, two Voices (60 s): missed **0%**, max 19.8 ms; Alpha Hound half (30 s): 0%, 19.5 ms; The Hive half: 0%, 20.1 ms; Battering Ram half: 0%, 19.3 ms; Carom King with three Princes: 0%, 19.7 ms. GPU total P95 13.1-13.6 ms at the GPU governor's clock; main thread CPU P95 4.2-4.6 ms; 0 heap allocations, 0 audio underruns | P95 <= 16.9 ms, missed <= 5%, max <= 50 ms: **all pass** |
| A crash found on the Moto | a fight's own danger lines (the Alpha's pack, the Hive's spear) carry no species; the band's telegraph tick looked one up (`play.cpp`, out of bounds): fixed before these runs | |

### Decisions taken alone

- **The Hive's x 2** (minibosses.md says every mini-boss has one, and the Hive's entry names only when the Queen is
  open): the beat of white flash before each shape, when every dart is white.
- **The loop's mini-bosses**: a loop Track draws its mini-boss from its song's pair (First
  Light's loop Tracks from all four).
- **Longwave's lunge** has a closing sweep (the page gives the lunge no sound).
- **A mini-boss's entrance** plays the elite-arrives patch under its card (not on the page).
- **The camera** at the pages' 0.75 (accepted by the lead); Longwave's fight crowd at its page's 30% (accepted).
- **The careless bot and the Choir**: with every hood hurting it lost 29 of 56 Choir fights and 10 of 20 runs in the
  first pass (median 14.0 min); the lead ruled that only a singing hood hurts, and that variant meets the targets (above).
  With the hoods harmless always (measured on the way): careless median 19.4 min, none in the first pass, the Choir 3 of 43.

### Numbers chosen (`src/sim/tuning.h` M5 block, `src/sim/boss_prose.h` for the pages' words)

- Health with the bot (the pages ask for it): Longwave x 1.4 on the page's 60 / 600 (the careful bot's build at Track 2 is
  far past the page's 30 damage a second), the Choir x 4.5 on 260 a Voice, the mini-bosses x 6.5 (Alpha), 5.0 (Hive),
  2.0 (Ram), 4.8 (Carom) on minibosses.json's 600 / 1,100 by Track.
- Shards: 4 a Longwave segment, 20 a Voice, 10 a Prince; the Voices move to their places at most 900 wu/s, the Hive's darts
  too; the Ram turns at 4 rad/s, its wall fan is 160 degrees wide, its wake 22 wu; a Prince that drifts on after the King
  keeps a quarter of its speed.
- The fights' bullet density lever at 1 (the pages' counts; Kaleidos keeps play test 1's 0.7).
- The Ram's stun low-passes the bed at 600 Hz for its beats.

### Pending sounds (`src/game/pending_sounds.cpp`; for the audio engineer, through the lead)

What the pages ask that the engine's recipes cannot say yet (each row plays the nearest it has):

1. Longwave's run: a rubber-string voice (the bass patch an octave up, twice its decay) pitched in semitones above the
   chord's root (0 2 3 5 7 8 10 12) a crest step. Now: a lead note.
2. Longwave's whip and the Choir's cage burn: the impact (crash and sine) as a one-shot, the whip's a fifth up. Now: the
   large kill.
3. A Voice's death: its pad note gliding down a fourth over a beat into the reverb. Now: SawGlide down a fourth.
4. The held note: a held pad note that rises an octave as the beam fires. Now: the pad note held for the bar.
5. The Hive's Ring: closed hats on a grid that tightens as the Ring closes. Now: the telegraph tick.
6. The Ram's lock: a tom roll that rises. Now: the snare roll. Its charge's driven kick. Now: the sub drop alone.
7. The Carom King's rim shot pitched to a chord tone (root, third, fifth, seventh), Princes an octave up. Now: the enemy shot.
8. A note relative to the chord's root for any recipe (Tom, Woodblock, Bell): the game knows scale degrees, not MIDI notes.

### Not done or not verified

- The fights in co-op on devices (the loopback soaks pass with them in the run; no device co-op run of a new fight).
- By ear: none of the fights' sounds has been heard (the PC's runs are silent).
- The tablet and the OnePlus: not attached.

## M5 audio: the ten Tracks of the set list — 2026-11-13 (the audio engineer; merged in `f9f3ae8`)

Branch `m5-audio` (worktree `Raveform-m5-audio`, cut from `737949e`), folded in here from `docs/PROGRESS_m5_audio.md` at the merge.
Phone numbers come from the **shared** Moto One 5G Ace (`ZY22BXMJG3`), under the phone lock (`m5audio`), with this
branch's APK deployed right before each run.

| Done criterion (brief) | Status | Evidence |
|---|---|---|
| Ten Tracks embedded, loaded, selectable (`--track 1..10`); every Track-sized table holds ten; ten palettes | **Met** | `MusicData` / `SampleBank` / `FdnReverb` / calibration tables at `kMaxTracks = 10`; `Palettes::kMax` 12 (an error if the file lists more); tests "music data", "palettes" |
| Nothing assumes a kick on every beat; beats and bars from the song clock; the sidechain from the pattern's kicks | **Met** (engine and band); game-side readers listed below | tests "no kick on the beat" (Tracks 7, 9, 10): beats, bars, kicks, the sidechain frame by frame, the Nova, the fill, the Build's roll, riser, swell, impact |
| Every Track x variant matches the prototype; calibration regenerated; `scripts\test.ps1` passes | **Met**: 30 of 30 pass, every onset 0 samples off | `captures\m5audio\match_table.md`; `test.ps1` all passed (audio 2519 checks) |
| Track changes at all nine boundaries; medley with its events | **Met** | test "the set list: nine Track changes"; `captures\m5audio\setlist_medley.{wav,mp3,events.csv,txt}` |
| Moto: full band at Track 10 and at the heaviest Track inside M2's limits | **Met**: P95 22.4% (Track 10), 22.6% (Track 9), 0 underruns, 0 callback allocations | below |

### What changed

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

### Match against the prototype (`scripts\render-audio.ps1 -Out captures\m5audio`, final engine)

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

### Calibration (`src/audio/loudness_calibration.json`, kDspVersion 5)

Tracks 1-3 unchanged (to 0.001 dB). New: track04 -5.216 / -4.996 / -5.387, track05 -5.336 / -5.327 / -5.642, track06
-5.581 / -5.695 / -5.558, track07 -5.258 / -5.282 / -5.078, track08 -6.631 / -6.571 / -6.926 (the loudest raw render,
-7.37 LUFS: Arcadia's square bass), track09 -5.479 / -5.532 / -5.383, track10 -5.099 / -5.115 / -4.997 dB
(club / arcade / velvet).

### Track changes and the medley

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

### Moto (`bench_idle --band full`, our APK, the lock; shared phone)

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

### Game code that reads a kick as the beat (src/game, not changed here)

`envelope(BeatKind::Kick)` is the pattern's kick (right for the kick pump, the halo, the lattice ripples, the logo:
look.md and screens.md say "kick"). These say "on the beat" and read the kick, so on Afterglow, Mirage and Last Light
they pulse on beat 1 and the and of 3 (and 2) only; `BeatView::beat(decay)` gives the clock's beat:
`play_view_b.cpp` 276 (Trio orbs "swell 6% on the beat"), 294 (Boost orbs), 317 and 320 (chevron, Cells "swell on the
beat"), `play_view_coop.cpp` 73 and 108 (chevron, beacon flare), `play_hud.cpp` 290 (the full meter's flash);
`stage.cpp` 1627 and 1644 (the bench stage's Pulse Ring and Lancer "every second beat" from `lastKickT_`);
`play_view.cpp` 706-719 (the no-band fallback synthesises a kick on every beat; fine for pictures of Tracks 1-6).

### Follow-ups after the lead's acceptance (2026-11-13; merged again after part 2)

- **Effects in key** (`95e9e6c`): the effects written in A minor (Boost bells, Upgrade chord and bells, Game over,
  Elite's fifth, Perfect Dash bell, Shield) move to the Track's root by a playback rate (-6..+5 semitones: Solar Wind
  -4, Reef +5, Aurora -5, Furnace -2, Zenith +3, Afterglow +4, Arcadia -2, Mirage +1) and take a baked major-third
  variant on Afterglow, Arcadia and Mirage. The UI ticks stay as they are. Tested on all ten Tracks.
- **Start-up** (`26696ab`): after `pm clear` the Moto baked 375 one-shots (56.8 MB) in 6.1 s before the first frame,
  over PLAN's 5 s; a normal launch read the cache in 54 ms. The risers of Tracks 2-10 are now baked on a worker while
  Track 1 plays, then cached.
- **Teammates' leads at eight notes**: accepted, on the user's co-op listening list.

### M5 content sound (sound_m5.json): state

Published: `src/audio/cues.h` (`386da70`): Mod 36, Riff 12, Boost 16, Fusion 8 (GAME_DESIGN numbering), the
`PlayerLoadout`, `Cue` (19 game events), `Recipe` (16) and 16 held-voice slots; `Band::setLoadout(slot, PlayerLoadout)`,
`cue()`, `playRecipe()`, `startVoice() / setVoice() / stopVoice()`, `lowPassBed()`. Built (`2d33052`): every recipe and
every cue of cues.h as sound_m5.json says, aggregated per 1/32; sheets `captures\m5audio\sheets\recipes.*` and `cues.*`
(`raveform_headless --render recipes|cues [--track N]`). Not built yet: Riffs 4-12's instruments, the Mods' changes to
the lead and mix, the Boosts' layers, the eight Fusions, and the Moto with all of it playing. The game's fight sounds
use the recipes since part 1 (`src/game/pending_sounds.cpp`).
### Not done or not verified

- By ear: the user has not heard the engine's ten Tracks (the match is by numbers; the medley is for that).
- The game's director still plays three songs and the loop (`kSongs = 3`, the other engineer's).
- `CLAUDE.md` still says `--track 1|2|3`.
- The one-shot bank is 48.3 MB in memory and in the cache file; the four-bar risers are half of it (int16 risers or
  baking per Track on demand would halve it). First-launch bake time on the Moto not measured (2.5 s for three Tracks
  in M2; about 2.2 times the desktop's work now).
- 12R and tablet: not attached, no numbers.

### M5 upgrades in the band: Riffs 4-12, the Mods, the Boosts, the Fusions (second engineer, 2026-11-13)

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

#### The move (design data)

`conductor.json` now holds the "mods", "riffs" and "boosts" blocks of `sound_m5.json` (f7cd516), merged verbatim into
its own blocks in GAME_DESIGN order (the first slice's entries kept as they were); the three notes merged into one each.
`sound_m5.json` is not in this worktree (it never was): on main it keeps "fusions", "recipes", "effectsInKey" and "where".
The engine reads every number of the moved sentences through ProseReader with its count checked (an edit that changes a
sentence's shape fails loudly at load). Numbers written as words are the engine's reading and are listed in
`loadUpgrades`: Lancer's charge "three beats" (a beat after the last shot), Rear Guard's "two" and "five" degrees,
Accent's beats per stack, Echo's "one beat", Carousel's beats 1 to 4. Lancer's level-3 pattern is read from the quoted
pattern in its "says". The calibration hash does not include conductor.json: no recalibration.

#### What plays (and where the engine had to read the design)

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

#### Listening sheets (`captures\m5audio\sheets\`, each `.mp3`, `.wav`, `.txt` with the times)

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

#### The content engineer's eight (cues.h, appended; ids for the content engineer)

`chordNote(amount, octave, NoteFrom::ScaleSteps | Semitones | ChordTone)` (value >= `kChordNote` = 0x10000) for any
`midi` of playRecipe / startVoice / setVoice, resolved at the tick it plays (chord tones from the chord's own notes:
Aurora's B major gives D#, not the scale's D). New recipes (`Recipe`, values 16-21): **RubberString** 16 (the bass
patch, twice its decay, p1 sixteenths; default the chord's bass an octave up), **Impact** 17 (p1 semitones up; or a
note for its 38 Hz sine), **Hat** 18 (closed; p1 = 1 open), **TomRoll** 19 (p1 beats ending on the tick, rising p2
semitones, default 2 and 12), **DrivenKick** 20 (tanh(p1 x kick), default 3), **RimShot** 21 (baked on the grid of
pitched recipes). `Band::setVoice(voice, param, gain, midi, glideSeconds)` glides a held voice (pad voice, drone) to a
note; a PadVoice's start `param` > 0 is its attack in seconds (choir.md's 20 ms). Not done: choir.md's "reverb send
0.5" (the content voices share the effects' send).

#### Moto: the worst case (`bench_idle --band max --track 10`, our APK, the lock `m5audio`; shared phone)

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

#### Cold start (`pm clear`, `am start`; shell `date +%s.%N` to the "play: first frame" line of `logcat -d -v epoch --pid`)

| Launch | First frame after am start | TOTAL PSS | Notes |
|---|---|---|---|
| Predecessor (`26696ab` measured before it) | 6.1 s | 149 MB | 375 one-shots baked before the first frame |
| This branch before `72d8a14`, cleared x2 | 6.61 / 5.99 s | 149 MB | 394 one-shots, 36.2 MB baked in 5.12 s before the first frame |
| **Final, cleared x2** | **3.20 / 3.12 s** | 141 / 143 MB | the later Tracks' kits and risers baked behind the game in 4.0 s, then cached |
| **Final, normal x2** (the cache) | **0.90 / 0.97 s** | 145 / 146 MB | |

The bake's own log now splits its time; on the desktop the thirty drum kits were half of it (1.2 of 2.2 s). Shared
phone; the screen was asleep before the second set and was woken with KEYCODE_WAKEUP (the launcher in front, no
keyguard).

#### Not done or not verified (this part)

- By ear: the recipes and cues sheets have not been heard; the upgrades' levels are the design's starting values and
  the engine's choices listed above (the user accepted the four upgrade sheets).
- The worst case's margin is 0.005 (above). Not measured: two Boosts a player, the Fusions in the worst case, the 12R
  and the tablet.
- `seq latest` (an M2 thread test) failed once under load from a parallel Gradle build and passed on its rerun.
- Main-checkout file touched by mistake: `src/audio/engine.cpp`'s modification time (a PowerShell read-and-write ran in
  the main checkout's folder; its content is byte for byte what it was: `git diff --ignore-cr-at-eol` empty).

## M4 stage B "LAN co-op: the lobby, the screens, the devices" — 2026-11-12

Built on `main` (never pushed): `988f718`, `d72f845`, `8dbcd3b` (the first engineer: the co-op picture in the owners'
colours, the lobby's protocol and discovery, the flow and screens, the co-op band), `ae637f4` (the fourth checkpoint: the
inherited tree as it stood, built and tested), and `ae637f4` (the inherited tree, built and tested), `f15e8fa` (the screens as `1d605ec` gives them, one row a room, the leave on one tick, the tools) and the closing commit (off-screen PC runs, the game over's threat layer, the device results, this section). The lead's screen rules after the review of the first
pictures are in `1d605ec` (screens.md "Co-op (M4)").

### What was built

- **Discovery and joining** (`src/net/lobby.cpp`, TECH_SPEC 8.3). The host's beacon on UDP 47777 twice a second (magic,
  protocol version, game port, room code, seats taken, seats, state, name) to the limited broadcast and to every network's
  own; the join screen's browser keeps what it heard in the last 3 s. **One row a room**: a host on several networks was
  heard once per network it shares with the joiner (four rows on this PC, `FH39 HOST 1/4` four times in the first
  pictures); a room is now its code, game port and name, and it keeps the address its code was made from. Join by the
  list, by the room code (four of the sixteen symbols `34679ACDFHJKMPQR` for the low 16 bits of the host's address; the
  address of a room heard with that code, else this device's own /16 in front), or by an address typed on the system
  keyboard (SDL text input; Android's number keyboard; a comma, a space or a dash is taken as a dot). A join nobody
  answers ends after 6 s with `NO ROOM WITH THAT CODE` / `NO ROOM AT THAT ADDRESS` (ENet alone takes 5 to 30 s). The
  handshake checks the protocol version and the content hash; a refusal says why on the join screen.
- **Android**: the Wi-Fi multicast lock is held while the join screen listens; Android 17's local network permission
  (`ACCESS_LOCAL_NETWORK`, now also in the manifest) is asked after the game's notice, and a refusal keeps the notice up
  with its last line changed and CONTINUE asking again. The target SDK is 36, so on today's phones the permission is not
  asked at all.
- **The co-op screens** (`src/game/screens_coop.cpp`, screens.md "Co-op (M4)" as updated in `1d605ec`): the local network
  notice, the join screen (rooms on the left: one row each, FULL and PLAYING disabled, a dashed `LOOKING FOR ROOMS` row
  with three dots on the beat; the message line in amber; HOST A ROOM; BACK; the room code and its sixteen 112 px keys on
  the right, DEL and JOIN under them, TYPE AN ADDRESS), the lobby (the code for every seat, four seats taken / joining /
  open, `TAP TO HEAR YOUR LEAD` under your own, START disabled until every seated player is ready, `WAITING FOR HOST` for
  the others, LEAVE), the shared pause (`FOR EVERYONE IN THE ROOM`, the team's score with separators, the shared count-in,
  LEAVE RUN), the co-op game over (TEAM / TIME / TRACK under the score, the players as a table centred as a whole, each
  player's crosses in the set list in their colour, RETRY for the host and disabled with `WAITING FOR HOST` for the
  others, the run dimmed to a fifth with no beacon or beacon sound).
- **Nothing of a run behind the setup screens**: opening CO-OP ends the title's attract run (the world emptied and
  still, no ship drawn; the band plays on); BACK brings the attract run back.
- **Every screen**: controls keep 24 px from each other and from lines of text. `Ui` keeps every control and line of a
  frame and warns once per pair that breaks the rule (`ui: <screen>: 'A' and 'B' are N px apart`); the review scripts
  fail on it. A disabled control is an outline at 0.3 with its label at 0.4 and takes no tap. Fixed under the rule: the
  join screen (HOST A ROOM and BACK overlapped by 10 px), the solo game over (HOME 15 px under RETRY), the SOUND tab (the
  tabs, the hint and the cards; the SYNC offset; the host-only control and DONE).
- **Edge chevrons keep clear of the HUD**: a chevron whose disc would come within 20 px of the score block (with the
  co-op ranking), the Track line and phrase bar, the Nova stock with the Dash bar, or the pause ring slides along the
  edge to the nearest free place (`PlayView::edgePlace`).
- **Leaving**: the host quits or drops: every client's run ends, the score is kept (the high scores), and the join screen
  says `THE HOST LEFT: THE RUN IS OVER. YOUR SCORE IS KEPT`. A client drops: the host times it out (or hears its
  goodbye), its seat opens, its ship fades where it was, and the run goes on. The leave now lands **on the same tick on
  every device**, two beats after the host knows (a timed player event before the seat mask; before, each device dropped
  the player when its message arrived, so the enemies that followed it chose again at different moments). The boss's
  health follows the players still in the run when it enters (it counted the seats).
- **The co-op band** (the first engineer, checked): the bed identical on every device (the host decides each bar's
  intensity), this device's lead loudest and the others by distance in their fixed timbres, the beacon's held root and
  the revive's notes, and the SOUND setting "music from the host's phone only".
- **Tools**: `scripts\review-m4b.ps1` (the whole review set, below), `scripts\coop-device.ps1` (the device checks, below),
  `raveform_headless --net soak --drop-at S` (a client's process dies mid-run), the UDP peers' and the game's 20 s lines
  now give the on-screen error at correction over the window next to the last second's error of all corrections, and
  `--net-capture-bar` logs what pushed the lattice (`view: lattice impulses ...`). New arguments: `--coop-quit-at S`,
  `--coop-permission-screen refused`, `--coop-demo-rooms`, the `--coop-shot` moments `address`, `beacon_off`,
  `pause_menu`, `over_client`. `--coop-down-at` now takes the hits inside the tick (`World::hitNextTick`): the first
  attempt's bullet at the ship's own position never downed it on this build.

### Acceptance

| Check | Result | Limit |
|---|---|---|
| Builds, tests | Desktop Debug and dev and the APK build at every checkpoint; `scripts\test.ps1` passes (audio 2,134 checks, sim 5,495, net 131,340; determinism hashes `4b50f64668105be3` / `f6fa477a0167baf9`, as before stage B) | all pass |
| Loopback soaks, 4 players, 10 simulated min (`test.ps1`) | clean: on-screen 5.15 wu RMS, at correction 7.41; song max 2.59 ms; down 213.1 / up 44.1 kbit/s worst second; census 900 / 0; 0 overflow, 0 allocations. 5% loss + 80 ms jitter + a pause: 7.00 / 9.85 wu; 3.46 ms; 210.9 / 44.8; 894 / 0; predicted kills undone 1,160 of 116,295 (1.0%) | < 12 wu, < 10 ms, 250 / 60, 0 |
| A client's process dies (`--net soak --players 3 --minutes 4 --drop-at 90`, seeds 1-3) | the host times it out (10 s) and the run goes on; on-screen error per 10 s 1.2-1.4 wu before, 0.3-0.9 after, run RMS 1.6-3.6 wu; 0 census mismatches, 0 allocations | as above |
| Four processes over UDP on this PC, 8 min (`--same-clock`, today's content) | on-screen error at correction 5.14 / 3.59 / 3.47 wu RMS; true song offset max 5.86 / 2.61 / 5.41 ms; down 200.6-202.8, up 41.2-51.7 kbit/s worst second; census 239 / 0 each; 0 stalls, 0 allocations | as above |
| Validation, layout | the 26 review pictures and the 7 solo screens (Debug): 0 validation messages, no control within 24 px of another or of a line (`ui:` warnings 0 after the fixes); a Debug co-op benchmark with three loopback bots: 0 validation messages, 0 heap allocations | 0 |
| Discovery (Moto and PC, Wi-Fi 192.168.137.x) | the phone hears the PC's room (`184332_..._phone_hears_pc.png`, one row); **the PC does not hear the phone's** (`184633_..._pc_hears_phone.png`: Windows Firewall, below) | both ways |
| Joining | PC to phone by the room code (`FHM4`, the PC's game) and by the address (a PC bot): seated, the run starts at three; **phone to PC by the list, the code and the address: no answer** after 6 s each (`NO ROOM ...` on the phone; the firewall) | list, code, address |
| A client drops (the PC bot killed 75 s into the Moto-hosted run) | `seat 1 left the run` on the phone and the PC's game in the same second; the run goes on; the PC game's on-screen error 1.7 wu RMS over the 20 s with the leave | handled cleanly |
| The host quits (the phone's app stopped) | the PC's game ends its run, keeps the score, shows `THE HOST LEFT: THE RUN IS OVER. YOUR SCORE IS KEPT` on its join screen (`184633_..._pc_host_left.png`) | handled cleanly |
| **15 min, the Moto hosting three PC bots over Wi-Fi** (`ZY22BXMJG3-bench_play-20261112-184949.json`, `184929_long_moto_host_pc_bot*.txt`) | Moto: 53,904 frames, **0 missed**, panel P95 1 period, worst 22.2 ms, heap allocations 0, tick p50/p95/max 0.44/0.74/4.99 ms, 803 enemies on average (1,350 at most), 268,079 kills, Track 6 reached; host 0 disconnects. Each PC bot: 0 stalls, census 461 / 0, down 209.0-230.8 kbit/s worst second (mean 147), up mean 33-34 kbit/s; **on-screen error at correction 35.5 / 33.5 / 21.8 wu RMS: FAIL**; up worst second 301-764 kbit/s (one second, see below) | P95 1 period, ≤ 5% missed, ≤ 50 ms; < 12 wu; 250 / 60 |
| 15 min, the same with 5% loss and 80 ms jitter on every peer | Moto (`...-191537.json`): 53,903 frames, 0 missed on the panel, P95 1 period, worst 28.1 ms, heap allocations 0, **PASS**; host 0 disconnects. Each PC bot: **0 stalls**, census 461 / 0, down 199.4-203.6 kbit/s worst second (mean 141), up mean 33 kbit/s (worst second 159-191, one second); on-screen error at correction **17.5 / 17.7 / 81.5 wu RMS: FAIL**. Playable and without stalls; the drift is the clean run's (below) | playable, no stalls |
| 15 min, the PC hosting the Moto and two bots (clean and impaired) | **not run**: the phone cannot reach a PC host (Windows Firewall) | |
| Song offset PC to phone | not measurable (two clocks) | < 10 ms |

**The on-screen error fails with the Moto hosting four players.** The same content holds on this PC (four processes over
UDP: 3.5-5.1 wu RMS; the loopback soaks 5.2 / 7.0 wu), and stage A's Moto-hosted Wi-Fi run with two players held 1.95 wu.
With the Moto hosting four, each PC bot's error at correction, per 10 s, is 2-4 wu in Track 1's first bars and 10-115 wu
from its denser bars on (Tracks 2-5), back to 3-6 wu in Track 6; at matching moments and enemy counts the PC-hosted run
reads 1-10 wu. The PC's game hosting the same way (its band driving the song it publishes, as the phone's does, three
headless bots, 6 min) reads 4.6-6.7 wu RMS, its 10 s windows up to 17.5: the band-driven host adds a little, the phone
over Wi-Fi the rest. Census, bandwidth and stalls are clean, so the copies are complete and on time but drift between
corrections. Not found yet: what on the phone as host makes the copies drift (suspects: poses from the phone arriving
in Wi-Fi bursts beyond the 66 ms interpolation, so enemies chasing the host's ship turn differently on each client; the
phone's tick timing against the host time it publishes). The next steps are a two-player and a three-player Moto-hosted
run to see where it starts, and the host's pose arrival gaps logged on a client. The PC bots' worst second of upload
(301-764 kbit/s) is a single second; their 10 s lines never show more than 40 kbit/s, and the phone's run had ended by
then (not checked second by second).


### The error jump after a seat left (`captures\m4b\device\174323_pc_game.txt`)

An artefact of the measure, not a jump on screen. The figure in that line ("error 38.5 wu RMS") was the RMS of every
correction of the last second, wherever its enemy was: off-screen enemies are corrected rarely, so their corrections
are large, and one second's RMS swings with what the host chose to send. Measured three ways:

- The loopback soak with a client's process dying mid-run (`raveform_headless --net soak --players 3 --minutes 4
  --drop-at 90`, three seeds; the true error of every enemy on the other client's screen against the host's world,
  each 10 s): 1.2-1.4 wu RMS in the 10 s before the death, 0.3-0.9 in the 10 s after, 1.0-2.0 in the next three
  windows; the same one-second figure swung between 0.1 and 22.1 wu over the run of seed 1 with no event at all.
- Three processes over UDP on this PC (host, A, B; B killed 75 s in, `captures`-free, scratch): A's on-screen error as
  corrections landed, per 10 s, 1.2-1.3 wu before and 0.8-1.8 wu after; the one-second figure 0.6-5.3 after.
- The Moto hosting with the PC's game and a PC bot (below, the bot killed): the PC game's on-screen error 0.9 wu RMS
  over the 20 s before and 1.7 over the 20 s with the leave; the one-second figure 15.8 wu twenty seconds before it,
  with nothing happening.

Changed anyway: the logs now print both figures ("all corrections ... wu RMS" for the last second, "on screen ... wu
RMS over N" for the 20 s window), and the leave itself now lands on the same tick on every device (above), so the
enemies that followed the player who left no longer choose again at different moments on different devices.

### The lattice in `frame_client.png` (the lead's question; nothing changed)

Measured at the same moment (bar 40, a Surge with a level-up Trio open), from both peers' logs (`view: lattice
impulses ...`, `captures\m4b\frame_host_host.log` and `frame_host_shot.log`): the crowd gain is **1.00** (no dimming at
that crowd) and the swarm gain 1.00. In the middle third of the frame 65-74% of the lattice's nodes are warm (heat
at least 5 wu) and none hot (13 wu). The heat is the springs' displacement, a mean of 5.9-6.0 wu; the kick ripples add
0.0-0.8 wu. What pushed the springs inside the frame in the last second: **the Drop's wave** (one impulse of 600 at a
400 wu radius around the ship) and **13 kills** (60 each, sum 780); no Nova, hit, down or Dash. Shots and the Trio's
shove do not push the lattice at all.

### Decisions taken alone

- **A join that nobody answers ends after 6 s** (`kConnectTimeoutUs`) with `NO ROOM WITH THAT CODE`, `NO ROOM AT THAT
  ADDRESS` or `THAT ROOM IS GONE` by how the join began; ENet alone gives up after 5 to 30 s.
- **Every session that ends lands on the join screen** with its message (screens.md lists the host-left message among
  the join screen's); BACK goes to the title. The first engineer's flow went to the title after a run.
- **A room is its code, game port and name** in the list; it keeps the address its code was made from when heard on
  several (else the first heard). Two hosts with one code, port and name on different /16s would share a row.
- **The leave lands two beats after the host knows**, on the same tick everywhere (a timed player event to the others,
  then the seats); a late event applies at once.
- **The boss's health counts the players present when it enters** (GAME_DESIGN 8's "players"), not the seats.
- The join list's rows stand **24 px apart, not 22** (screens.md gives both; the rule for every screen wins), so four
  rows end 6 px lower.
- **The SOUND tab's tabs sit 14 px higher** (top at 154, not 168) so the hint fits 24 px from them and from the cards;
  the faders moved to y 690 as screens.md says; the SYNC offset 6 px lower; the host-only control 10 and 4 px higher.
  A segmented row (OFF / ON) and a fader with its label and value count as one control each.
- **The co-op game over**: a header row over the number columns (SCORE, CHAIN, KILLS at cap 12 in the accent), the
  Chain as `X41`, the kills with separators; YOUR BAND under the set list (this device's player); RETRY at 270 px and
  HOME at 90 px from the bottom so `WAITING FOR HOST` fits between them with 24 px each side; no beacon sound either;
  the run behind at the pause's exposure (a fifth) with the threat layer (drawn after the tone map, where the
  exposure cannot reach it) left out. The enemies' hulls stay readable at a fifth of the exposure because the tone map
  compresses them (the lead's last note: an arrow crossed `P1 (YOU)`); darker would need a fade after the tone map, a
  composite parameter that does not exist yet (not done).
- **TYPE AN ADDRESS**: Android's number keyboard (SDL); a comma, a space or a dash typed is a dot; the sixteen keys
  give way to two lines of help while it is open; a computer keyboard types the code on the pad too (P is one of the
  symbols, so only Escape goes back on that screen).
- **The local network notice** stays up after a refusal with its last line changed (screens.md) and CONTINUE asking
  again; `ACCESS_LOCAL_NETWORK` is in the manifest while the target is 36 (it does nothing until the target is 37).
- The lobby's own seat says `TAP TO HEAR YOUR LEAD` on a line of its own under READY; the host's `WAITING FOR ...`
  hint sits right of START (it was 3 px under it); a ready seat's ship has its halo as two arcs (screens.py).
- **Off screen** (the user's rule): `--offscreen` creates the window hidden, as a utility window that cannot take focus,
  moves it left of every display by its width and 400 px, and shows it without activating it.
- `--coop-demo-rooms` (pictures only) adds three made-up rooms after the ones heard, so the list's layout and its
  FULL and PLAYING rows can be shown with one host on this PC; `join_list.png` has the PC's real room first.

### Numbers chosen

`src/net/session.h`: `kConnectTimeoutUs` 6 s; the leave two beats ahead (`session.cpp`). `src/game/screens.cpp`: the
layout gap 24 px (quoted, `Ui::kControlGapPx`). `src/game/screens_coop.cpp`: the join screen's rows at 265 + 114 k, the
keys' pitch 136 px (112 + 24), DEL / JOIN 180 x 80 at y 904, TYPE AN ADDRESS at y 1003, HOST A ROOM at y 850, the message
at y 712, the dashed row's dashes 22 px; the game over's table at y 384-534 (cap 22, columns 56 px apart), its line under
the score at y 342. `src/game/play_view.cpp`: the chevrons' clearance 20 px (quoted) searched in 4 px steps along the
edge; the HUD's blocks (`Play::render`): the score block to 290 px or the score's width right of the margin and down to
the Shard level (with the co-op ranking), the phrase block 280 px left and 380 px right of the centre and 70 px tall,
the Nova stock and Dash bar to 122 px, the pause ring. `src/platform/app.cpp`: the off-screen window 400 px left of the
leftmost display.

### Not done or not verified

- **The PC as host, from the phone: blocked by Windows Firewall** (the user's check). The phone hears the PC's room
  (`captures\m4b\device\184332_discovery_pc_host_phone_hears_pc.png`), but its joins by the list, by the code and by
  the address get no answer, and the PC's join screen does not hear the phone's room: there is no firewall rule for
  `raveform.exe` or `raveform_headless.exe`, so unsolicited inbound UDP is dropped (`Get-NetFirewallApplicationFilter`
  lists none; the 192.168.137.x network is Private). The prompt Windows shows to the first host was never answered on
  this PC. To allow it, in an elevated PowerShell:
  `foreach ($p in 'build\desktop-dev\raveform.exe','build\desktop-debug\raveform.exe','build\desktop-dev\raveform_headless.exe') { New-NetFirewallRule -DisplayName "RAVEFORM $p (LAN co-op)" -Direction Inbound -Action Allow -Protocol UDP -LocalPort 47777,47778 -Profile Private -Program (Join-Path 'D:\Stuff\DevFolder\Android_Games\Raveform' $p) }`
  then `scripts\coop-device.ps1 -Run discovery_pc_host` should write `phone_joined_list`, `_code` and `_address`
  pictures of the phone seated in the PC's lobby, and `-Run long_pc_host [-Impaired]` runs the 15 minutes with the PC
  hosting. Not run here: the 15-minute PC-hosted runs, the phone joining a PC by any route, the PC hearing a phone.
- The OnePlus 12R and the tablet are not attached; PLAN's exit run (PC host + Moto + tablet + 12R) is not possible.
- Nobody has listened to two devices together; the song offset between the PC and the phone cannot be measured (two
  clocks; the PC bots' clock estimate is reported instead).
- The Android 17 local network permission is coded but cannot be exercised (target SDK 36, the Moto runs an older
  Android); the notice and its refusal are pictured on the desktop only.
- Hotspot mode (one phone's hotspot) is not tried.
- The lobby's "joining" seat state lasts the clock's sync (well under a second); it is not pictured.


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

### The halo burns down (the lead's last pass on the loop, `daec945`)

On the first downbeat of every Track of the loop (not in its Track-change Break) every present ship loses two cells of
capacity, never below one (`kLoopHaloBurn` 2 from `kLoopHaloBurnFrom` 4, `World::burnHalos`, the `HaloBurn` event);
cells held above the new capacity go with it. The lost arcs shatter outward on the beat (`PlayView::shatterArcs`; the
halo glyphs are ornament, which `shatterGlyph` skips, so a lost cell's arc had not been shattering either and now
does); no panic ring, no invulnerability, the Chain untouched. In the loop no kill drops a Cell and Regen stops (the
Breaks already gave nothing back); a boss still drops two Cells (no elites or mini-bosses exist in the slice), and a
Halo, Extra Cell picks (only inside the capacity the burn left, `PlayerState::burnCap`) and the Novas in stock still
work. In co-op every device burns every present ship alike, down ones too; a revive still returns with one cell. The
Track card's last line reads `HALO n`, the viewed ship's capacity (6, 4, 2, 1 from a full halo); the loop's first card
shows both `NO MORE REFILLS` and `HALO 6` (`captures\playtest2\loop_encore.png`); two arcs shattering from an eight-cell
halo: `captures\playtest2\loop_burn.png` and `loop_burn_zoom.png` (Debug, 0 validation messages).

The knob (one or two cells a Track, from the loop's first or second Track), 8 seeds x 40 minutes each bot:

| burn | careful: median first death (its Track) | careless: median first death (its Track) |
|---|---|---|
| 2 cells from Track 4 (the design's) | 31.3 min (11-12) | 21.6 min (8) |
| 2 cells from Track 5 | 31.3 min (11-12) | 21.6 min (8) |
| 1 cell from Track 4 | 31.3 min (11-12) | 21.7 min (8) |
| 1 cell from Track 5 | 34.3 min (13) | 25.3 min (9-10) |

No setting reaches the targets; two cells from the loop's first Track is the nearest and stays. Over 20 seeds x 40
minutes:

- Careful: median first death **32.1 min, in Track 12** (target 18-30). 18 of 20 runs die, at 18.8-35.3 min (Tracks
  7-13); two live the 40 minutes. No death in the first pass. Boss fights lost 6.1%; it is hit about once in five
  minutes, 119 of its 153 hits in boss fights, so it outlives the burn to one cell (Track 7) by several Tracks.
- Careless: median first death **21.6 min, in Track 8** (target 9-16). Every run dies, at 13.6-24.7 min (Tracks 5-9).
  No death in the first pass (one in five allowed). Boss fights lost 8.2%.
- Pool overflows 0, heap allocations 0 in both. The burn changes no spawn, speed or bullet count (it removes sustain
  only), so the Moto's densest part was not measured again.
- `scripts\test.ps1` passes (`raveform_sim_tests` 5,495 checks: two ships of 8 and 6 cells burn over Tracks 4-8, the
  first to 6/4/2/1/1, nothing before each first downbeat, the cells above the capacity lost, no invulnerability, the
  Chain kept, an Extra Cell refilling inside the burnt capacity, one burn a ship a Track; no kill drops a Cell in the
  loop and Regen's period is 0 there; a boss killed in the loop drops two Cells for each ship).

The lead's ruling (`1d605ec`, GAME_DESIGN 8): the halo burn's result is accepted as measured, and the targets are now
the careful bot's median first death between minutes 26 and 36 and the careless bot's between 17 and 25, nobody in the
first pass (20 seeds of 40 minutes). `--sim crowd` prints no target bands, so nothing there changed.

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
