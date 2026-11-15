# RAVEFORM — notes for Claude sessions

Read `docs/PLAN.md` first (locked decisions, performance contract, milestones, working rules), then
`docs/PROGRESS.md` (milestone status, benchmark numbers, deviations, open questions). Read
`docs/TECH_SPEC.md` and `docs/GAME_DESIGN.md` sections as the milestone names them. Creative content
(font, palettes, shapes, music data) arrives from the lead under `design/`; code never redesigns it.

## Commands (PowerShell 7, repo root; every script dot-sources `scripts\env.ps1`)

| Task | Command |
|---|---|
| Fetch pinned SDL / volk / Vulkan headers / ENet 1.3.18 (no-op when present; tag and commit checked) | `scripts\fetch-deps.ps1` |
| iOS (on a Mac only; docs/IOS.md; written on this PC, never built): the same pins, the Vulkan headers from the Vulkan SDK for macOS 1.4.363.0, MoltenVK's licence; the app (CMake's Xcode generator, MoltenVK static, arm64, iOS 15+) | `scripts/fetch-deps.sh`, then `scripts/build-ios.sh --team <id> [--config Release] [--install] [--run "<app args>"] [--open] [--clean]` |
| The iOS app icon (one 1024 px square, `ios/Assets.xcassets`) | `python -I -B scripts\make_icon.py --ios-only` (about seven minutes) |
| Build desktop (`debug` = Debug + validation, `dev` = RelWithDebInfo) | `scripts\build-desktop.ps1 [-Config debug\|dev] [-Clean]` |
| Build and run desktop (silent unless `-Sound`: the app gets `--silent`) | `scripts\run-desktop.ps1 [-Config dev] [-NoBuild] [-AppArgs '<app args>'] [-Sound]` |
| Desktop screenshot (+ review copy ≤ 1600 px) | `scripts\run-desktop.ps1 -Screenshot captures\x.png -Res 2400x1080 -Small` |
| Desktop 60 s validation + timing run | `scripts\run-desktop.ps1 -AppArgs '--scenario bench_idle --seconds 60'` |
| Desktop lifecycle test (surface release/restore + resizes) | `scripts\run-desktop.ps1 -AppArgs '--scenario lifecycle_test --cycles 20'` |
| Build APK (`dev` = native RelWithDebInfo, debuggable, debug-signed; `debug` = native Debug; `release` (M7) = native Release, stripped (the unstripped libraries beside it as `app-release-symbols.zip`), not debuggable, no app arguments, debug-signed, its size by part printed; `-Bundle` also the App Bundle; `-Offline` = no network) | `scripts\build-android.ps1 [-Config dev\|debug\|release] [-Bundle] [-Offline] [-Clean]` |
| Build, install, launch on one device | `scripts\deploy-android.ps1 -Serial <serial\|moto\|op12r\|tablet> [-NoBuild] [-NoLaunch] [-AppArgs '--tone']` |
| Benchmark on a device (saves `captures\bench\*.json` + `*.load.txt`; 5 s warm-up; samples the phone's CPU load) | `scripts\bench-android.ps1 -Serial <serial> -Scenario bench_idle [-Seconds 60] [-Warmup 5] [-AppArgs '--tone'] [-Deploy] [-NoLoad] [-SfLatency]` |
| Screenshot of the panel (adb screencap; RAVEFORM must be in front) | `scripts\capture-android.ps1 -Serial <serial> [-Small]` |
| Screenshot read back by the app (pre-rotation undone) | `scripts\capture-android.ps1 -Serial <serial> -InApp [-Small]` |
| 20 background/foreground cycles | `scripts\lifecycle-android.ps1 -Serial <serial> [-Cycles 20]` |
| Review copy of any PNG | `python scripts\shrink_png.py <png> [--max-width 1600]` |
| Acceptance still (desktop) + compare | `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 4 -Screenshot captures\m1\frame_reef.png -AppArgs '--scene design/scenes/frame_reef.rfscene --still --bloom on --no-overlay'`, then `python -I design\tools\scenes.py compare captures\m1\frame_reef.png frame_reef` (`look_card` also with `--bloom off` / `--nobloom`) |
| Scene picture or strip (desktop, fixed 1/60 s steps) | `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 600 -Screenshot captures\m1\x.png -AppArgs '--scenario bench_endgame --seconds 600 --warmup 0 --no-overlay [--look haze] [--palette furnace] [--strip 8]'` |
| M1 contract run (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_endgame -Seconds 120 -Warmup 600 -AppArgs '--tier low' -Deploy` (10-minute soak, then 120 s) |
| GPU cost at the top clock | `scripts\bench-android.ps1 -Serial moto -Scenario bench_endgame -Seconds 15 -Warmup 4 -AppArgs '--tier low --gpu-burn 40' -Deploy` |
| The M1 review set (stills + compare, glyph sheets, frame-600 pictures, both strips, `scenes.py light` lines) | `scripts\review-m1.ps1 [-Out captures\m1]` (writes `captures\m1\` unless `-Out`) |
| Where a frame's light comes from (one class off at a time) | `scripts\light-breakdown.ps1 [-Frame 600] [-Out captures\light]` |
| Unit tests (audio suite: queues, tempo map, sequencer timing, lead rule, variants, music data, no allocation; loudness calibration check; M3 sim suite: grid, pools and handles, Chain and score, lead rule, Nova, species, no allocation; the determinism test; M4 net suite: message round trips, the packet parsing fuzz, the wire and its condition simulator, clock and song sync, a loopback session; the 10-minute loopback soak clean and with 5% loss + 80 ms jitter and a pause) | `scripts\test.ps1` (exit 0 = all passed) |
| Audio match against the prototype: every Track × variant, match table, onset check, listening files | `scripts\render-audio.ps1 [-SkipRef] [-Only track03_club] [-NoListening]` (writes `captures\m2\`, `match_table.md`) |
| Prototype references (musicproto.py as a library; writes nothing under `design/`) | `python -I -B scripts\audio_ref.py captures\m2\ref [track03 [club]]` |
| Loudness calibration (after any music data or DSP change; `test.ps1` fails while it is stale) | `scripts\calibrate-audio.ps1 [-Check]`, then rebuild |
| Offline renders of the band (headless, the same engine) | `build\desktop-dev\raveform_headless.exe --render staged\|mock\|sfx\|leads\|full\|surge\|medley [--track 1..10] [--sound club\|arcade\|velvet] --out x.wav [--pcm16] [--events x.csv] [--meta x.json] [--stems dir] [--random-bursts] [--profile] [--seconds S]` |
| The output stage's checks (headless): the Nova's distortion through the limiters; what a barrage of effects costs the music (the owner's play of f0624e5: the effects bus has its own gain under -4 dBFS) | `raveform_headless.exe --render nova-check` (exit 1 at 3% THD+N) and `--render duck-check` |
| One-shots against the prototype's synthesis | `raveform_headless.exe --dump-samples captures\m2\samples`, then `python -I -B scripts\audio_samples.py captures\m2\samples` |
| Audio scenes (desktop) | `scripts\run-desktop.ps1 -AppArgs '--scenario audio_test'` / `'--scenario sync'` |
| Audio device jobs under the phone lock (deploys the last built APK) | `scripts\audio-device.ps1 -Run bench\|screenshot\|soak [-Seconds 60] [-Scenario bench_idle] [-AppArgs '--band full']` |
| Play the game (M3; the default with no scenario) | `scripts\run-desktop.ps1 -Config dev` (`-AppArgs '--bot'`: the bot plays); on a phone `scripts\deploy-android.ps1 -Serial moto` |
| Simulation, headless (determinism, 10-minute bot soak, stress) | `build\desktop-dev\raveform_headless.exe --sim determinism\|soak\|crowd\|stress [--minutes M] [--seconds S] [--seed N] [--policy play\|reckless]` (exit 0 = pass; one `RFSIM` JSON line) |
| M3 stress contract run (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_stress -Seconds 60 -Warmup 60 -AppArgs '--tier low' -Deploy` (`bench_play`: the bot plays the normal game, measured) |
| The M3 review set (scripted strips, bot frames at 30 s / 2 / 5 min, stress) | `scripts\review-m3.ps1 [-Out captures\m3] [-Config debug\|dev] [-SkipStrips] [-SkipFrames]` |
| The M3 stage B review set (screens, Trio, HUD per section, Riffs, Boosts, edge chevrons, Kaleidos, Track change, bot at 3 / 6 / 10 / 15 min) | `scripts\review-m3b.ps1 [-Out captures\m3b] [-Config debug\|dev] [-Only trio,boss_beam]` |
| The M3 stage C review set (bot at 3 / 6 / 10 / 15 min, the Drop with the crowd, edge chevrons with Kaleidos, Prism's fan, the Track name card, the boss's death strip) | `scripts\review-m3b.ps1 -Out captures\m3c -Only bot_3,bot_6,bot_10,bot_15,hud_drop,edge,boost_prism,track_change,boss_death` (`-Config dev` for the bot frames: their fast-forward is slow in Debug) |
| The M3 stage C2 review set (the careful bot's real run and the no-hits run at 3 / 6 / 10 / 15 min, the Drop with the crowd, Prism's rainbow) | `scripts\review-m3b.ps1 -Out captures\m3c2 -Config dev -Only bot_3,bot_6,bot_10,bot_15,bot_3_nohits,bot_6_nohits,bot_10_nohits,bot_15_nohits,hud_drop` and `-Only boost_prism` (Debug) |
| Play test 1's pictures (the Trio locked and half held, Split's fan at 2, 3, 4 and 5 lanes since eb10554 (`split2` ... `split5`), Orbitals at level 3, the bot at minute 10) | `scripts\review-m3b.ps1 -Out captures\playtest1 -Only trio_locked,trio_hold,split2,split3,split4,split5,riff_orbitals` and `-Config dev -Only bot_10` |
| Play test 2's pictures (a level-up Trio in a fight with the ship sheltered in an orb, a Cell in a crowd, the halo at eight cells with one growing back, Overcharge 0-5 in one sheet, Prism, the HUD's Shard level) | `scripts\review-m3b.ps1 -Out captures\playtest2 -Only pt2_level_trio,pt2_cell_crowd,pt2_halo8,pt2_overcharge_0,pt2_overcharge_1,pt2_overcharge_2,pt2_overcharge_3,pt2_overcharge_4,pt2_overcharge_5,boost_prism` and `-Config dev -Only pt2_hud_level` (also writes `overcharge_sheet.png` and `pt2_halo8_zoom.png`; `prism_compare.png` sets `prism_before.png`, from the play test 1 build, beside `boost_prism.png`); the loop's card and the halo burning down: `-Only loop_encore,loop_burn` (also `loop_burn_zoom.png`) |
| Play test 3's pictures (the Orbitals' guard ring broken and relighting, the Extra Life's orb and its use, a Vault's name, a hit on Kaleidos's weak point as a strip, Split at five lanes, Track 1's phrase bar) | `scripts\review-m3b.ps1 -Out captures\playtest3 -Only pt3_ring_broken,pt3_ring_relight,pt3_life_orb,pt3_life_use,pt3_vault,boss_hit,split5,hud_groove` (scripted in `src/game/play_pictures.cpp`) |
| The crowd and the difficulty over many seeds (M3 stage C / C2 acceptance) | `build\desktop-dev\raveform_headless.exe --sim crowd [--seeds 20] [--minutes 30] [--seed N] [--policy reckless] [--god] [--trace]` (per Track and cycle: enemies in view with the game's camera during Groove against the target, the counted frame, alive, the Drop's peak in view; first death, Tracks reached, kills a minute, hits by cause, boss fights (and the share lost), the most shots alive; `--god`: hits cost nothing, so every cell is measured; `--trace`: a line a minute, a bar and a second of each boss fight; play test 2: the Shard level pace by Track with the Shards collected a second, the first run's level and upgrades at minutes 3, 6 and 10, Cells taken and cells grown back) |
| The densest part of the game, measured (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Warmup 3 -Seconds 60 -AppArgs '--drop 10.1 --nohits --tier low' -Deploy` (since play test 3, Track 10's first Drop: the set list's densest crowd; its last Drop, Kaleidos Reprise: `--drop 10.2` (every Track is two cycles since c2f113a: `.3` no longer exists)) |
| Bot, 30 simulated minutes with a minute table | `build\desktop-dev\raveform_headless.exe --sim soak --minutes 30 [--seed N] [--policy reckless]` |
| The real game at minute 10, measured (Moto) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Warmup 600 -Seconds 120 -Deploy` |
| M4 co-op soak: host + 3 bots on the in-process loopback network, simulated time (TECH_SPEC 8.4 numbers; exit 0 = pass, one `RFNET` JSON line) | `build\desktop-dev\raveform_headless.exe --net soak [--minutes 10] [--players 4] [--seed N] [--netsim loss=0.05,jitter=80[,delay=D,dup=P,reorder=P]] [--pause] [--drop-at S]` (`--drop-at S`: the last client's process dies S s into the run, without a word; the true on-screen error is printed for each 10 s around it) |
| M4 co-op peers without a screen over real UDP (one process each; each prints an `RFNET` line) | `raveform_headless.exe --net host [--players 4] [--bind 127.0.0.1] [--port 47778] [--minutes 15] [--netsim ...]`, then per client `raveform_headless.exe --net join <ip> [--bind <ip>] [--same-clock --clock-offset-ms X --clock-ppm P] [--seed N] [--minutes 15]` (`--same-clock`: one PC, so the true song offset is measured) |
| M4 co-op in the game (desktop; no lobby screens) | `scripts\run-desktop.ps1 -AppArgs '--host --players 2 [--bot] [--net-overlay]'` and `-AppArgs '--join <ip> [--bot]'`; bots in the host's process: `'--host --loopback-bots 3 --bot'` |
| M4 co-op benchmark (Moto hosting three in-process bot clients) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Warmup 600 -Seconds 120 -AppArgs '--host --loopback-bots 3' -Deploy` (the result's `net` object: the session, the bots' worker frame time) |
| M4 stage B review set (every co-op screen and moment of `design/screens.md` "Co-op (M4)": the local network notice and its refusal, the join screen empty / with rooms / a code / an address, the refusals, the lobby from the host / a joiner / four seats, one moment from the host and a client, beacon on and off screen, revive, co-op HUD, shared pause and count-in, co-op game over from host and client, the host leaving; 2400x1080 with `.small.png`; exit 1 on validation messages or controls closer than 24 px) | `scripts\review-m4b.ps1 [-Out captures\m4b] [-Config debug\|dev] [-Only lobby_host,hud]` |
| M4 stage B device checks (the phone and this PC on one Wi-Fi; lock name `m4b`; our APK deployed first): discovery and joining both ways (list, code, address), a client drop and the host quitting; the 15-minute runs | `scripts\coop-device.ps1 -Run discovery_pc_host\|discovery_moto_host\|long_pc_host\|long_moto_host [-Serial moto] [-Minutes 15] [-Impaired]` (`-Impaired`: 5% loss and 80 ms jitter on every peer; results in `captures\m4b\device\`) |
| M4 net follow-up: the loopback soak over many seeds (one seed's error swings with its run; `docs/PROGRESS_m4_net.md`) | `scripts\net-seeds.ps1 [-Seeds 1..10] [-Minutes 10] [-Impaired] [-Extra '--remote-poses','predict'] [-Out captures\m4net\seeds\x] [-Low]` (each seed's RFNET figures and the mean, median and worst on-screen error; at most 4 soaks at once) |
| M4 net experiments (headless) | `raveform_headless.exe --net soak ... [--net-tune down=B,famN=W,motion=0\|1] [--remote-poses interp\|predict]` (the soak's and `--net join/host`'s 10 s lines break the true error down by player, family and age; `direct`, remote players predicted on direct links, is the default; since the merge of `d5be6ea`: by Track and fight with Event Horizon's and Feedback's own lines, the remote ships and aims against the truth, the worst up second's makeup, each client's claims, thinning against the owner's; `join`/`host` print `corr`, `poses`, `frames` diagnostics every 10 s; the game logs `net: diag` and `net: diag shots` every 20 s) |
| M4 net co-op device runs, the follow-up's options | `scripts\coop-device.ps1 ... [-LockRoot <checkout> -LockName <name>] [-Players 2..4] [-PhoneArgs '...'] [-BotArgs '...']` (the phone's log is saved as `*_phone.logcat.txt`; the APK is deployed as built; the PC bots run a private copy of the headless tool) |
| M5 review sets (one folder a part: `captures\m5\<part>\`, `.small.png` copies, a contact sheet a strip; Debug: exit 1 on validation messages) | `scripts\review-m5.ps1 -Part part1\|part2\|part3\|part4\|part5\|part6 [-Config debug\|dev] [-Only lw_chop,ch_cage]` (part 6: Cadence `cd_*`, Arpeggiator `ap_*`, Fata Morgana `mg_*`, Kaleidos Reprise `rp_*` (a solo each: `rp_kaleidos` ... `rp_feedback`), mini-bosses batch 3 `mm_*`, `tt_*`, `wv_*`, `dw_*`, five strips (`-Config dev`); part 5: each elite trait ahead of a still ship `el_*`, each structure `st_*`, each event `ev_*`; part 1: Longwave, the Choir and mini-bosses batch 1, every phase and moment; part 2: each new family at its moment, Tracks 2 and 3 in play, the glyph sheets; part 3: every new upgrade round a still ship, 22 scenes (`up_*`, `riff_*`, `boost_*`, `fusion_*`); part 4: Event Horizon `hz_*`, Polyrhythm `pr_*`, Feedback `fb_*` and batch 2 `mq_*`, `bm_*`, `bs_*`, `sa_*`, each phase and its death, six strips (`-Config dev`: the fast-forward to Tracks 4-6 is slow in Debug); the pictures are scripted in `src/game/play_pictures_m5.cpp`) |
| The upgrades' worst case on the Moto (M5 part 3) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Warmup 3 -Seconds 60 -AppArgs '--drop 10.1 --nohits --tier low --loadout max' -Deploy` (`--loadout max`: every Mod at its cap, Swarm, Flak, Storm Coil and Sweeper at 5 with their Fusions, Chorus and Fractal always on) |
| A fight, played or measured (M5): the run starts in the fight's home Track and the fast-forward plays on to it | `-AppArgs '--fight <id>[:<phase>[:hold]]'` (`kaleidos`, `longwave`, `choir`, `alpha_hound`, `the_hive`, `battering_ram`, `carom_king`; M5 part 4: `horizon`, `polyrhythm`, `feedback`, `moth_queen`, `brood_mother`, `blink_star`, `sentry_array`; part 6: `cadence` (phases 1 Modulation, 2 Inversion, by health), `arpeggiator` (1 Eighth notes, 2 Two hands), `morgana` (1 Mirage, 2 Crossing), `reprise` (1 Medley, 2 Last Light: every arm off; 10-15 the Medley for that arm's solo's picture), `mirror_mimic`, `twin_tether` (1: a node dead), `weaver_prime`, `deep_well`; phase 1-3 takes its parts down to that phase (Horizon and Feedback by health, Polyrhythm's 2 kills Four, the Sentry Array's 1 four turrets), a mini-boss's 1 is half health; `:hold` gives its health back every tick so it stays there) |
| A Track's Drop, measured (M5) | `-AppArgs '--drop <track>.<cycle> --nohits --tier low'` with `bench_play` (that Track from its start, fast-forwarded to the cycle's Drop; no upgrades from earlier Tracks, so the crowd piles up) |
| A fight's densest moment on the Moto (M5) | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Seconds 60 -Warmup 3 -AppArgs '--tier low --nohits --fight longwave:1:hold' -Deploy` (the Choir `choir:3:hold`; a mini-boss `alpha_hound:1:hold` ... `carom_king:3:hold` with `-Seconds 30`: its Drop's hold ends it; M5 part 4: `horizon:2:hold`, `polyrhythm:1:hold`, `feedback:2:hold`, `moth_queen:1:hold` ... `sentry_array:1:hold`) |
| Every fight's numbers with the bots (M5) | `build\desktop-dev\raveform_headless.exe --sim crowd --seeds 20 --minutes 40 [--policy reckless] [--trace]` (the `fights` table: fought, won, lost, warped out, hits a fight by bullets / bodies / beams, seconds to kill by Track; `--trace`: a line a second of each fight and one a hit) |
| The fights tuned for a person (GAME_DESIGN 7.6): the novice's first meetings | `build\desktop-dev\raveform_headless.exe --sim crowd --seeds 20 --minutes 60 --policy novice` (play test 3: the novice never aims, it fires only through the auto-aim assist; `--policy assist`: the careful bot's movement with the assist's fire, the bar "finished the 10 Tracks" in 19 of 20; the `first meetings` table: met, won, lost, warped out and the seconds of each fight's first meeting in each run, and how many first runs reached Track 2's boss; the lines: Track 2's boss in 16, Kaleidos won in 18, Longwave and the Choir 16, Tracks 4-6 12; `--armour0`: armour takes nothing, as on the build the user played (the calibration: the novice must lose to Longwave there with `--health kaleidos=1.82,longwave=2.86,choir=3.33`); `--health id=x,...`: a factor on a fight's health on top of `tuning.h`; `--handover N`: the careful bot plays to Track N with hits costing nothing, then the policy asked for takes its build on to Track N's boss) |
| The no-trap table (GAME_DESIGN 6.6; M5 part 7) | `build\desktop-dev\raveform_headless.exe --sim picks [--seeds 10] [--seed N]` (on the reference build Split 1 and Double-Time 1, each weapon and rhythm Mod's stack and each Riff's level: its gain in kills a minute from Track 3's second Groove against a Split stack's, and in seconds to kill the Choir against an Overcharge stack's; the Downbeat Cannon over its ingredients; a crowd figure is good to about +-0.5) |
| M4 two peers' views of one moment (desktop, two windows started together) | `run-desktop.ps1 -NoBuild -Res 2400x1080 -Screenshot captures\m4\host.png -AppArgs '--host --players 2 --bind 127.0.0.1 --bot --net-capture-bar 40 [--net-overlay] [--net-view-seat 0]'` and `-Screenshot captures\m4\client.png -AppArgs '--join 127.0.0.1 --bind 127.0.0.1 --bot --net-capture-bar 40 ...'` (`--net-view-seat 0`: both cameras on the host's ship) |
| M6 review pictures (`src/game/play_pictures_m6.cpp`; `captures\m6\`) | `scripts\review-m3b.ps1 -Out captures\m6 -Only m6_hint_0,...,m6_hint_10,m6_ended_by,m6_set_complete,m6_set_complete_over,m6_title_star,m6_title_idle,m6_uiscale_90,m6_uiscale_130` (Debug) and `-Config dev -Only m6_fb_riffs` (Feedback's Echo with the player's Riffs) and `-Config dev -Only m6_mini_stays,m6_flash_longwave,m6_flash_choir,m6_flash_alpha` (the fast-forward to a fight is slow in Debug); M7: `-Out captures\m7 -Only m6_credits,m6_credits_licences`; the transitions (b12310b, strips of eight a beat apart): `-Config dev -Out captures\transitions -Only m6_drop_title,m6_drop_retry,m6_home,m6_pause_restart,m6_pause_quit`; also writes `m6_hints_sheet.png` and `m6_flash_*_zoom.png`. Pictures and fixed-step runs save no settings or high scores |
| The owner's feedback pass and Title v2 (2026-11-14; `play_view_feedback.cpp`, `title_v2.cpp`): strips per moment | `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Screenshot captures\fx\x.png -AppArgs '--scenario play --picture m6_fx_crit --strip 8'` (`m6_fx_crit`, `fx_cresc` (5 of 10 ticks), `fx_cresc_full`, `fx_cresc_break`, `fx_sync`, `fx_burn`, `fx_well`, `fx_hole`, `fx_coil`, `fx_aegis`, `fx_meteor`, `fx_hz_death` (dev: the fast-forward to Track 4)); the title: `screen_title`, `m6_t2_intro` and `m6_t2_drop` (strips an eighth apart), `m6_home`, `m6_title_idle` |
| Enemy bullets by Track (0449eb3, GAME_DESIGN 8): the crowd's Grooves with and without the rule | `build\desktop-dev\raveform_headless.exe --sim crowd --seeds 10 --minutes 16 --god --policy reckless [--bullets-as-written]` (the careless bot ignores bullets, so the crowd stays the same run; the table prints the enemy bullets alive in each Track's first-run Grooves and what the Track's count fired of what the pages asked; `--bullets-as-written` = `Rules::bulletsAsWritten`) |
| Event Horizon's pushes (3283249), strips a sixteenth apart in Tidal held | `scripts\review-m3b.ps1 -Out captures\af4b101 -Config dev -Only m6_hz_wave,m6_hz_touch` (a `--fight` phase is 0-based: Tidal is `horizon:1`) |
| Sanitizers (M7): the unit tests (audio, sim, net, save), determinism, a 3-minute bot run, a fight start in Tracks 4, 7 and 10, the 3-minute net soak under ASan + UBSan (llvm-mingw; `build\desktop-asan`, logs in `captures\asan\`; exit 0 = no report) | `scripts\sanitize.ps1 [-Only tests,soak,fights,net] [-NoBuild]` |
| The thermal check (M7, Moto): Track 10's first Drop held for 5 minutes | `scripts\bench-android.ps1 -Serial moto -Scenario bench_play -Seconds 300 -Warmup 5 -AppArgs '--drop 10.1 --drop-hold --nohits --tier low' -Deploy` |
| The app icon (screens.md "App icon and name (M6)") | `python -I -B scripts\make_icon.py` (reads `design/glyphs.json`, `look.json`, `palettes.json`; writes the adaptive icon's layers into `android/app/src/main/res/mipmap-*` and `captures\m6\icon_512*.png`; a few minutes) |

The version is `CMakeLists.txt`'s project VERSION (and `android/app/build.gradle`'s versionName, kept the same); every build writes
`generated/build_stamp.h` (`cmake/build_stamp.cmake`: version, commit, date) and the title screen shows it bottom right.

Desktop exit codes: 0 ok, 1 init failed, 2 bad arguments, 3 validation messages, 4 benchmark aborted,
5 screenshot failed. Device serials: Moto `ZY22BXMJG3`, OnePlus 12R `a0e622a5`, Galaxy Tab `R5GL362BEHX`.

## App arguments (desktop argv; Android: the `args` string intent extra)

`--scenario play|bench_play|bench_stress|touch_test|bench_idle|bench_mid|bench_endgame|glyph_sheet|lifecycle_test` (the
first three: the game, below; `bench_mid` / `bench_endgame`:
the scripted M1 stages of `design/scenes.md`; `glyph_sheet --page 1|2|3`: a still), `--tier
low|medium|high|ultra|auto` (effect tier, TECH_SPEC 5.4; without it pictures and tests use high, everything else
the tier the first-launch probe chose for this GPU, cached in `SDL_GetPrefPath()/tier.txt`; `auto` measures
again), `--cuts N` (the first N cuts of look.md §10 on top of the tier), `--reduce-flashing` (look.md §7; key R),
`--governor on|off` (governor v1; default on for interactive stages, off in benchmarks), `--look
signal|laser|haze` (Signal is the game's look; the others are developer switches), `--palette <id>` (default
reef), `--mute <names>` (analysis pictures: `background, grid, heat, enemy, elite, boss, aura, bullet, priority,
sparks, flash, killpool, fx, warp, shard, player, afterglow, distortion, aberration, vignette, pump, sparkles,
pools`; `core/args.h`), `--scene <file.rfscene> --still --bloom on|off` (acceptance stills), `--strip N` (also
capture the N-1 frames before the `--screenshot` frame: `<name>_<k>.png`), `--fixed-step` (1/60 s per frame;
implied by `--screenshot`), `--gpu-burn N` (profiling only: a saturating GPU load before the timed passes,
so they show their cost at the GPU's top clock),
`--seconds N` (measured, after `--warmup S`, default 2),
`--frames N`, `--screenshot <png>` (relative paths go to the app's external files dir on Android),
`--res WxH` (desktop window), `--validation` / `--no-validation`, `--tone` (hold the test tone),
`--no-swappy` (plain FIFO), `--fps HZ` (Android target refresh; default = panel maximum), `--no-overlay`,
`--affinity big|all` (Android: pin the main thread and the threads it starts to the non-little cores, or
leave placement to the scheduler), `--vivid` (M6: COLOUR VIVID, Display-P3 where the screen offers it, for this run), `--cycles N`, `--drop-hold` (M7, with `--drop`: that Drop never ends; the thermal check); M4 co-op (stage A, without the lobby): `--host` (`--players N`
seats 2-4, `--bind <ip>`, `--port N`), `--join <ip>`, `--loopback-bots N` (host: N bot clients in this process on
the loopback network), `--netsim loss=..,jitter=..,delay=..,dup=..,reorder=..` (the condition simulator on what
this device sends), `--net-overlay` (the overlay on its network page), `--net-view-seat N` (pictures: the camera on seat N's ship), `--net-capture-bar B` (with `--screenshot`, and `--strip N` for the next N-1 frames as `<name>_<k>.png`:
the picture as the run reaches bar B, the same musical moment on every peer, then quit; it also logs `view: lattice impulses ...`,
what pushed the lattice in the last second and the crowd gain). M4 stage B, the co-op screens from the command line (the title's
CO-OP does the same by taps): `--coop host` (host a room at once) or `--coop join[:<code>|:<ip>|:list]` (the join screen, then
type the code on the pad, join the address, or take the first room heard), `--coop-name NAME` (the seat's name), `--coop-start
[N]` (the host starts once N, default 2, are seated and ready, and retries 3 s after all are down), `--coop-down-at S`,
`--coop-pause-at S` (pause for everyone S s into the run, resume 3 s later), `--coop-quit-at S` (leave the room S s into the run;
the host: the run ends for all), `--coop-permission-screen [refused]` (the local network notice on the desktop), `--coop-demo-rooms`
(three made-up rooms join the list: the layout's picture), `--net-content-salt N` (another content hash: the refusal's test),
`--coop-shot permission|join|code|address|message|lobby|lobby2..4|beacon|beacon_off|revive|hud|pause_menu|pause|over|over_client`
(with `--screenshot`: the picture at that moment, then quit). On the join screen a keyboard types the code; the address is typed
on the system keyboard (SDL text input, a number keyboard on Android). Keys: F1 overlay, F2 the
overlay's network page (co-op), P pause for everyone (co-op), F12 screenshot, T tone hold, L next look, P next
palette, K next tier, R reduce flashing, Esc / Back quit (in the game with its screens: pause and back; Enter is the screen's main control). Taps (decided when the last finger lifts): two fingers next look, three
the overlay, four next palette. Raw launch: `adb -s <serial> shell "am start -n dev.raveform.game/.RaveformActivity --es args '--scenario bench_idle --seconds 60'"`.

The game (M3, `game/play.h`, `src/sim/`; stage B adds the director, upgrades, Kaleidos and the screens): with no
`--scenario` (or `--scenario play`) the game opens on the first-launch notice (once), then the title over the bot's
attract run; PLAY, pause (the ring top right, Esc / Back / P), RESUME with a count-in, game over, RETRY, HOME and
SETTINGS (SOUND, CONTROLS, LOOK). Settings go to `settings.txt`, scores to `highscores.txt` and the baked one-shots
to `baked_audio.bin`, all in `SDL_GetPrefPath()` (`adb shell pm clear dev.raveform.game` brings the first launch
back). `--autoplay`: the screens with the bot playing each run (tap PLAY and RETRY); `--reckless-after S` makes that bot
careless after S seconds of a run, so a loop test reaches game over. Stage B pictures:
`--picture screen_notice|screen_title|screen_settings_sound|screen_settings_controls|screen_settings_look|
screen_pause|screen_game_over|trio|trio_answer|trio_take|trio_locked|trio_hold|split2..5|hud_groove|hud_build|hud_drop|hud_break|riff_orbitals|
riff_pulse|riff_sentinel|boost_prism|boost_hyper|boost_halo|boost_magnet_storm|edge|boss_bloom|boss_refraction|
boss_spiral|boss_naked|boss_beam_tell|boss_beam|boss_iris|boss_pair_death|boss_death|track_change|bot_3|bot_6|
bot_10|bot_15|bot_3_nohits|bot_6_nohits|bot_10_nohits|bot_15_nohits|pt2_level_trio|pt2_cell_crowd|pt2_halo8|pt2_hud_level|
pt2_overcharge_0..5|loop_encore|loop_burn` with `--strip 1` (8 for the three strips;
`bot_N` is the careful bot's real run at minute N, retrying after a death; `bot_N_nohits` and `hud_drop` run with hits
costing nothing, so the Tracks come on time). Stage C: the run's Tracks are First Light, Solar Wind and Reef, then the
loop; stage C2: the crowd is counted in each player's frame (`firstSlice.crowd`, `tuning::kFrameHalfWWu/HWu`).
`--nohits`: hits cost nothing for the whole run (a measured run deep in the game). The band follows the director (`Play::conductBand`);
benchmarks and `--bot` are the run alone. `--scenario touch_test` is M0's touch scene (formerly the default). `bench_play` (the bot plays the normal
game) and `bench_stress` (about 700 enemies and 1,500 enemy bullets around a bot-driven, invulnerable ship)
are measured like the other benchmarks and add a `sim` object to the result (tick time per 60 Hz tick
[p50, p95, max], entity counts, kills, deaths, pool overflows). `--bot` (the bot plays), `--seed N`,
`--fast-forward S` (S seconds of play before the first frame, for pictures), `--picture <name>` with `--strip 8
--screenshot <png>` (scripted review scenes: `fire, kill_small, kill_medium, kill_large, shards, hit, nova,
nova_front, dash, perfect_dash, warp, warp_land, sentry`; the strip starts at the scripted moment). The game
runs on the band's heard clock (Groove held, intensity from the threat on screen) and uses its Track's
palette (Track 1: Ultraviolet) unless `--palette`; pictures run on the simulated clock without the band.
Controls: WASD or arrows move, mouse aims with the left button held to fire, E or the right button Nova,
Space Dash, Enter / E / Space / click retry after death; gamepad left stick, right stick (fires beyond 30%),
RB Nova, LB Dash, A or Start retry; touch: floating sticks (left half moves, right half aims and fires),
NOVA and DASH buttons, tap to retry. With the aim idle the auto-aim assist fires at the nearest enemy on the
half-rate grid. The game starts with the overlay off (F1) and has no tap gestures.

M5 (`design/bosses/*`, `src/sim/world_longwave.cpp`, `world_choir.cpp`, `world_miniboss.cpp`, part 4's `world_horizon.cpp`,
`world_polyrhythm.cpp`, `world_feedback.cpp`, `world_miniboss2.cpp`, the view in `src/game/play_view_m5.cpp` and `play_view_m5b.cpp`): each
Track ends in its own boss (Track 1 Kaleidos, 2 Longwave, 3 The Choir, 4 Event Horizon, 5 Polyrhythm, 6 Feedback, 7 Cadence,
8 Arpeggiator, 9 Fata Morgana, 10 Kaleidos Reprise (M5 part 6: `world_cadence.cpp`, `world_arpeggiator.cpp`, `world_morgana.cpp`,
`world_reprise.cpp`, the view `play_view_m6.cpp`); round again in the loop) and from Track 2 on the middle cycle's Drop brings a
mini-boss (Alpha Hound or The Hive in Track 2, Battering Ram or Carom King in Track 3, the last two in the Pen; Moth Queen or Brood
Mother in Track 4, Blink Star or Sentry Array in Track 5; Mirror Mimic in Track 6; Twin Tether or Weaver Prime in Track 7; Deep
Well or the other in Track 8; Tracks 9 and 10 any the run has not met with one and two elite traits (Armored, Swift,
Regenerating), the loop any (`World::pickMini`, `DirectorState::minisMet`; batch 3 in `world_miniboss3.cpp`)). The set list is
the Tracks the game has fights for (`World::setSetList`, `kBossesBuilt`: all ten since part 6) and the loop starts after it.
Kaleidos Reprise's last pattern is First Light's arpeggio, read from `design/music/track01.json` (`RepriseData::arpSemis`). A boss whose Shards come late (Event Horizon's flash,
Polyrhythm's break on the One, Feedback's two bars) sets `BossState::payoutTick`. Feedback records every player on
`World::tape_` and its echo shots are enemy bullets flagged `Bullets::echo`. A player's shot stream (look.md v2.5, GAME_DESIGN
6.6): Wave and Helix Array are standing curves from the muzzle (`World::waveAt`, `Shots::life0`); above the soft cap the
thinning (`World::updateThinning`, on a beat) halves the reach of the lanes beyond the middle three. A beam is a core and a
pooled glow (`PlayView::drawBeam`). Every fight's sound goes through `src/game/pending_sounds.cpp` (one row a cue; what the engine lacks is
listed there and in PROGRESS). `--fight <id>[:<phase>[:hold]]` jumps a run to a fight (pictures, benchmarks). Species (M5 part 2):
`design/species.json` (was `species_m5.json`) holds all sixteen families, 240 species (+ the broodling) and the ten Tracks'
rosters, budgets and crowd targets; families with behaviour so far: the slice's six and Flanker, Skitter, Moth, Brood, Blink
(`src/sim/world_families.cpp`, `familyBuilt`). Upgrades (M5 part 3): all 36 Mods, 12 Riffs, 16 Boosts and 8 Fusions of GAME_DESIGN 6.2-6.5
(`src/sim/world_upgrades_m5.cpp`, the view `play_view_upgrades.cpp`, the band's loadout and cues `game/upgrade_sounds.cpp`); a Trio is
dealt (6.1: the first orb firepower, the others by `tuning::kDeal*` weights; up to six orbs with Lucky Break and a Fusion's gold
orb); the bot picks by `kBotUpgradeScore` (`sim/bot.cpp`). Co-op: what moves or stops enemies because of one player's upgrade is
shared by player events or decided by the host (Gravity Rounds, Freeze Frame in the corrections). `World::setSongBarShift` (Play,
each tick) keeps Singularity's and the Sweeper's phase on the band's song bars. A boss's rule is a bonus, not a wall (GAME_DESIGN
7.6, since 2026-11-13): armour (a multiplier below 1; Kaleidos as built) sparks and stops a shot and does its share
(`World::damageBoss`), the HUD shows each boss's hint line under its name (`bossHint`, `Play::bossHintTick`: by this player's own
shots), the bosses' and mini-bosses' health factors in `tuning.h` are tuned with the novice bot (`Bot::Policy::Novice`), and every
lethal beam is drawn by `PlayView::lethalBeam` (its kill width, at most 12 wu, a white-pink axis and a danger glow).
Play test 3 (a008b71, built in 61a7f09; docs/PROGRESS.md "Play test 3"): every Track Short (8 / 4 / 8 / 4; the run's Track 1 two cycles,
the others three); the assist fires on the full grid and aims at a fight's weak point (World::assistBossAim, an enemy within 250 wu
first; it rests in Feedback's fight), the novice fires only through it and Bot::Policy::Assist is the careful bot's movement
with it; Split 2, Double-Time 1, Fork 1 (since eb10554 Split 4, a lane a rank to 5 lanes, an even fan astride the aim line, and Double-Time 2 again); Arc, Burn and Shatter half and one step; Orbitals a guard ring (World::breakRing,
orbitalsLit); the Extra Life (kPickupExtraLife among the Boost orbs, 	akeExtraLife, spendExtraLife); no Vault in Track 1,
VAULT over the others; every boss hit seen (PlayView::bossHit, drawBossHitFlash) and heard (BossCue::HitWeak, HitArmour).
The arena (M5 part 5, `design/arena.md`, `arena.json`; `src/sim/world_arena.cpp`, the view `play_view_arena.cpp`): six structures stand
for a Track (placed on its first downbeat by the host, dissolving over the Track change) and an event runs in a Track's Groove
(`World::arenaBar`, `arenaBeat`, `updateArena`); bodies with health (Vault, Rift, the gold Vault, the Comet, carriers) are
`World::arena_` slots, hit by shots and Novas, a client's hits claimed as boss claims on part `kArenaPartBase` + slot; the host's
decisions travel as boss operations from `kArenaOpBase` (`World::logArena` / `applyArenaOp`). Elites (`design/traits.md`;
`src/sim/world_elites.cpp`): `Enemies::traits` (`kEliteBit` and the twelve trait bits), bought by the director with the threat
budget (`World::eliteBar`), their traits' rules in `updateElites`, `eliteSheds`, `eliteHit`, `eliteDeath`; the prose numbers
in `boss_prose.h` (`kAr*`, `kEl*`), the traits' own in traits.json (`TraitData`).

Audio (M2, `src/audio/audio_args.h`): `--scenario audio_test|sync` (the band on heard time with its
controls; the sync calibration screen of `design/screens.md`), `--band off|play|staged|full|stress|metronome`
(default: `play` in audio_test, `metronome` in sync, `full` in every `bench_*` unless pictures are taken, else
off; `full` = the load test, every layer playing: Drop held, four players firing on the 1/16-triplet grid with
one Split harmony, every Riff at level 2, kills every frame; `stress` = Split ×2), `--silent` (everything plays and is measured, the output is zeroed at the device: PC runs), `--no-audio` (no band in
any scenario: M0's silent stream, as M1 measured; for comparison), `--track 1..10` (default 1; `bench_*`:
the palette's Track, Reef = 3 at the stage's 132 BPM), `--sound club|arcade|velvet`, `--intensity X`,
`--section groove|build|drop|break`, `--hold`, `--fire`, `--haptics on|off` (default on in the audio scenes),
`--sync-offset MS` (this run only). In real time the stages run on the band's heard clock and heard beat
events (`Stage::setBeat`); `--screenshot` / `--fixed-step` runs keep the scripted 132 BPM clock and play no
band unless `--band` asks (the stage still keeps the script), so pictures stay repeatable. Keys in
audio_test: Space (hold) fire, V sound, N next section, H hold, X next Track, Up/Down intensity, 1/2/3 Riffs,
M staged/conductor, C sync screen, G haptics; mouse or finger held anywhere else = fire, on-screen buttons
for all of them. Sync screen: Left/Right ±5 ms, A auto, Enter/Esc/Back done; touch: drag the slider, − / +
(hold repeats), AUTO, DONE. Benchmarks with the band add `audio_engine` to the result (callback wall and
CPU time, load = engine time / callback period, cores, callback heap allocations, voices, latency).

Log tags: `RAVEFORM` (everything), `RFBENCH` (one JSON line per benchmark or test, also on desktop
stdout), `RFLIFE` (background / foreground / first present after resume). On Android the `RFBENCH`
line is a summary (logcat entries stop at 4068 bytes); the full result, with the worst-frame table and
the per-second timeline, is `<scenario>.json` in the app's external files dir, which `bench-android.ps1`
pulls. Reading a result: `interval_ms` is present call to present call on the CPU; `display` is the
panel side (plain FIFO: VK_GOOGLE_display_timing present times; Swappy: Swappy's own statistics, where
`missed` = frames shown one refresh period or more later than asked; GPU-bound frames that take two periods
are not "late" to Swappy, so `bench-android.ps1` also counts present intervals above 1.5 periods);
`phases_ms`, `sched`, `late` and `worst` attribute every interval (`src/tools/frame_probe.h`); `device_load`
is the phone's CPU load. M1 adds `tier`, `look`, `palette`, `gpu_ms` (per pass [p50, p95, max] from
timestamp queries; `total` is work only, `wait` is the gap before the swapchain pass) and `scene_counts`.
GPU times follow the GPU's own frequency governor (it keeps the GPU about 85% busy, and the shell user
cannot read its clock): compare passes at the top clock with `--gpu-burn 40`.

## Conventions

- C++20, no exceptions, no RTTI, `-Wall -Wextra -Werror` on our code. Layout per PLAN §4 (`src/core`,
  `src/gfx`, `src/audio`, `src/platform`, `src/game`, `src/tools`, and since M3 `src/sim`: the simulation,
  a static library without SDL, Vulkan or audio that the game, `raveform_headless` and the tests link).
- The simulation is deterministic (TECH_SPEC 7.3): no wall clock, no unordered iteration, its own random
  streams, no allocation after `World::init`; `World::hash()` digests the state and `scripts\test.ps1` runs
  the determinism test. Gameplay numbers come from design data or the docs; numbers no document gives are
  in `src/sim/tuning.h` (and the view's constants block in `src/game/play_view.cpp`), listed in PROGRESS.md.
- No heap allocation in the steady-state frame: global `operator new` is counted and benchmarks report
  `heap_allocs` for the measured window (must stay 0). Fixed-size arrays, persistently mapped buffers.
- Vulkan 1.1 core only (the Moto is 1.1.128 without debug_utils, sync2, dynamic rendering, timeline
  semaphores). SPIR-V via `glslc --target-env=vulkan1.1`, embedded at build time. No optional features.
- Desktop Debug runs validation + synchronization validation; it must report 0 messages. Loader
  messages are counted separately (`VK_LAYER_PATH` in `env.ps1` hides the stale SDK 1.1.92.1 registry entries).
- Android lifecycle lives in the SDL event watch (`App::eventWatch`): the pump blocks while paused.
  Background destroys the swapchain + surface and closes audio; `RaveformActivity.surfaceDestroyed`
  waits for that. The next `beginFrame` rebuilds them.
- Pre-rotation: `Swapchain::rotationDeg` drives both the clip-space matrix (`renderer.cpp`) and the
  PNG un-rotation (`png.cpp`); change them together.
- Native resolution always: the surface must match the display mode (2400x1080 on the Moto). The log
  says "surface ... matches the display mode"; a "differs" warning is a bug (the M0 Moto runs were at
  2174x1020 until `RaveformActivity` called `setDecorFitsSystemWindows(false)`).
- Design data (`design/look.json`, `look_variants.json`, `palettes.json`, `glyphs.json`, `vector_font.txt`)
  is embedded at build time and read at start-up; numbers are never retyped into code.
- The audio callback never allocates, locks, logs or calls JNI. The music (`design/music/*.json`) is
  embedded like the other design data; the loudness calibration (`src/audio/loudness_calibration.json`)
  carries a hash of it and of `kDspVersion` (bump it when the DSP changes a render's loudness).
- Python prototypes run as libraries with `python -I -B` and never write under `design/`.
- Only phone numbers count for performance; desktop timing is informational.
- Git: `Invoke-RfGit` (Git for Windows + schannel). Commit only at working checkpoints, never push, never
  commit `design/` files you did not write. Build output, `captures/` and fetched `third_party/` are ignored.

## PC runs are silent (user rule)

Nothing run on this PC makes sound: the user is at the machine. `--silent` keeps everything running and measured
(the band, its limiter, the callback's timing, the song clock) and zeroes the output at the device, after the
limiter; `--no-audio` drops the band where a silent stream is enough. `scripts\run-desktop.ps1` and the review
scripts pass `--silent` unless `-Sound` is given (the user's opt-in to listen); a desktop process started by hand
gets `--silent` too. `raveform_headless` opens no audio device; offline renders write WAV files only. Phones are not
covered: their runs keep their sound.

## PC runs are off screen (user rule)

No game window appears on this PC's desktop during test, picture, review or benchmark runs: the user is at the
machine. `--offscreen` creates the window hidden, as a utility window (no taskbar button) that cannot take focus, moves
it left of every display and shows it without activating it, so the Vulkan surface, the swapchain, screenshots, strips
and validation behave as on screen (the log says `window: off screen at (x, y)`). `scripts\run-desktop.ps1`, the review
scripts (`review-m1`, `review-m3`, `review-m3b`, `review-m4b`, `light-breakdown` through `run-desktop`) and
`coop-device.ps1` pass it unless `-Watch` is given (the user's opt-in to see the window); processes they start with
`Start-Process` also get a hidden console. A desktop process started by hand gets `--offscreen` too. `scripts\test.ps1`
and `raveform_headless` open no window. Phones are not covered.

## PC CPU limit (user rule)

Anything this project runs on the PC uses at most 4 CPU cores: builds above all, also shader compiles,
offline tools and test runs.

- `scripts\env.ps1` confines its PowerShell process to one logical CPU on each of 4 physical cores (read
  from the processor topology, all in one L3 domain; on the 5950X: logical CPUs 24, 26, 28, 30, mask
  `0x55000000`). Every process it starts inherits that. Run builds, tools and tests through the scripts,
  or dot-source `env.ps1` first. Per process only: no system settings, no other process touched.
- Job counts too: `cmake --build --parallel 4`, Gradle `--max-workers=4`, and a Ninja job pool in
  `CMakeLists.txt` (`RF_MAX_JOBS`), which also holds for the Ninja that the Android Gradle plugin runs.
- Gradle: `build-android.ps1` gives this project's daemon its own immutable JVM options
  (`-XX:ActiveProcessorCount=4`, a project-local `-XX:ErrorFile`), so it never reuses a daemon that another
  session or an older build started without the limit (`-D` options do not separate daemons). Never run
  `gradlew --stop`: other sessions share the Gradle user home.

## Shared test devices

Both phones are used by other Claude sessions at the same time. The device scripts follow these rules;
do the same for any manual adb command.

1. Nothing device-wide: no `adb kill-server` / `start-server`, no reboot, no `adb logcat -c` (read with
   `-d` and filter by tag, time or our PID: `logcat -d --pid=<pid>`), no settings changes (`settings put`,
   `wm size`, rotation locks), and no clear, uninstall or force-stop of any package but `dev.raveform.game`.
2. Before launching, benchmarking, sending key events or taking a screenshot, check the foreground
   (`adb -s <serial> shell dumpsys activity activities | grep -E "mResumedActivity|topResumedActivity"`).
   If another app (neither the launcher nor ours) is in front, someone else is using the phone: wait and
   re-check, try the other phone meanwhile. Never send HOME or start our activity over someone else's app.
3. Keep device runs short and leave the phone tidy: benchmarks exit by themselves; after any other run,
   `adb -s <serial> shell am force-stop dev.raveform.game`.
4. A benchmark is valid only if RAVEFORM kept focus for the whole run. The app logs focus loss and
   background events (`RFLIFE`), aborts on background and reports `"valid": false` on focus loss;
   `bench-android.ps1` retries such runs later. Numbers are taken on shared phones: say so when reporting.
5. Background/foreground cycles send HOME: run them in one short burst only while the launcher or our
   app is in front; if the phone stays busy, skip the check and report it as not verified.
6. Every adb command needs `-s <serial>`.
7. Between this project's coders (lead's rule, 2026-11-11): before using a phone write
   `captures\locks\<serial>.lock` holding your name (e.g. `m1`) and the time, and delete it when the run ends;
   a lock under 30 minutes old with another name means the phone is busy (refresh yours on runs past 30
   minutes). Another coder installs its own `dev.raveform.game`: deploy your APK right before every device run
   (`-Deploy`) and check which build ran before debugging a strange result.
