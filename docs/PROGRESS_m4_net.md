# RAVEFORM — M4 network follow-up (branch `m4-net`)

The network engineer's work on the three open M4 problems (the lead's brief, 2026-11-13): the on-screen error with the
Moto hosting four players, the PC bots' one-second upload spike, and the headroom of the impaired loopback soak.
Numbers taken on the Moto (`ZY22BXMJG3`) are from a phone shared with other sessions (our APK deployed right before
each run, the main checkout's lock `m4net`). Files under `captures\m4net\` (ignored by git).

## The instruments (what was added to see)

- **The client's error at correction, broken down** (`Session::diag()`, `NetDiag`; the game logs it every 20 s as
  `net: diag`, `raveform_headless --net join/host` every 10 s and for the whole run): by the player the host's copy
  follows (this client's, the host's, another client's), whether this copy followed another player or was in another
  state, the time since its last correction, along and across the host's velocity (a time offset or a different
  path), and by family.
- **The other players' poses as they arrive**: per seat, new samples against all heard, misstamped (dropped),
  relayed (host), the gaps between arrivals, their age on arrival, how often and how far a tick reached past the
  newest; bad packets; the gap between correction packets; the host's song anchors against the model they replace.
- **This device's frames and ticks**: the longest frame, ticks a frame, ticks skipped (the song had not moved), time
  dropped, pose sends over 50 ms apart; on the host each new anchor of its band's heard clock against the model.
- **The soak's true error broken down** the same way (on-screen samples against the host's world in the same process),
  with the error of the simulated position alone (without the drawn offset), the error's sizes, and Shoals' age bins.
- **The worst upload second's makeup** (payload by channel, datagrams, how long the peer had been silent, reliable bytes
  unacknowledged then) and when it happened.
- `scripts\net-seeds.ps1`: the soak over ten seeds (one seed's error swings with what its run happened to do).
- `raveform_headless --net-tune down=B,famN=W,motion=0|1` and `--remote-poses interp|predict` (experiments).
- `scripts\coop-device.ps1`: `-LockRoot/-LockName` (the main checkout's lock), `-Players 2..4`, `-PhoneArgs`,
  `-BotArgs`; the phone's log of the run is saved (`*_phone.logcat.txt`); the APK is deployed as built (no Gradle
  run); the PC bots run a private copy of the headless tool (a build can go on meanwhile).

## (1) The Moto hosting four: the cause, found and fixed

**Cause: every PC client saw the other two clients' ships frozen where they stood in the lobby, for the whole run.**

- A client's first pose left before its clock had a sample, so it was stamped on the client's own clock as "host
  time". The PC's clock read 6.5 days (560,397 s) ahead of the Moto's.
- That pose sorted as the newest of its seat in the host's ring for good. The host relays each seat's newest pose at
  every pose frame, so it relayed that lobby pose again and again; the other clients dropped it as a repeat and never
  saw that player move (`posesHeard` 116, new 0 per 10 s).
- The host's own view was right (its interpolation landed next to the legitimate newest sample), so enemies the host
  sent after another client's ship were chased, on every other client, toward a ship standing still elsewhere.
- Evidence (`captures\m4net\device\120658_*`, 6 min, and `121756_*`, 2 min; shared phone): the phone received every
  client's poses at 30 Hz (age 10 ms, gaps <= 67 ms), its ticks and anchors were clean (0 ticks skipped, 0 time
  dropped, anchors 0.02 ms RMS); in its first 20 s it logged one pose per client aged -560,397,120 ms. On each PC
  client the copies the host sent after another client erred 25-110 wu RMS against ~2 wu for those after itself, and
  28-31% of the copies on screen followed another player than the host's copy.
- Why only there: one PC's processes share one clock (PC-hosted runs never showed it); two players need no relay
  (stage A's Moto run held 1.95 wu); the error grew with the crowd that followed teammates.

**Fix** (`src/net/session.cpp`): a client sends poses only once its clock has settled (`synced()`); any receiver drops
a pose stamped more than 1 s ahead of or 5 s behind its own host time, and a previous state older than 1 s. Net test
`testRelayClocks` (one client 6.5 days ahead): without the fix the others never hear it move (a teammate's copy
1,835 wu from the teammate); with it they do.

## (2) The one-second upload spike — explained and fixed

**What it was**: real traffic after the host had gone. When the phone's benchmark ended, its app left without a goodbye
that reached the PC bots; each bot's ENet link kept the host until ENet's timeout (5 s at least) and resent everything
unacknowledged (its last claims) at our capped resend timeout (one round-trip timeout, 50 ms at least), every 50 ms.

**Fix** (`src/net/link.cpp`, `session.cpp`): a peer silent for more than 250 ms (every live peer sends 30 datagrams a
second; silence is now our own count of what arrived, since ENet's `lastReceiveTime` moves only with acknowledgements)
is taken as gone or cut off until heard again: its resends get ENet's own backoff (each waits twice as long) instead of
the cap, and a client holds its poses and claims. Detection of a dead host is unchanged (ENet's 5 s; the links end 7 s
after the host's process is killed).

Measured on this PC (a host over UDP killed mid-run): two players, `captures\m4net\vanish\client.txt` before: the worst
up second was the last, 128.9 kbit/s (mean 30.9), the host silent 3.7 s; after (`vanish_fixed`): 29.3 (mean 24.0), not at
the end. Four players killed at 100 s (`vanish_v4`, final code): worst up seconds 34.0-36.2 kbit/s, all in play; the
links end 7 s after. On the Moto, the run with the first form of the fix (`122828_*`, a resend timeout floored at the
silence) still read 73.9-82.2 kbit/s in the last second (the claims the bots went on making were in flight), which is
what the client's holding of its poses and claims now stops; that form also never let a dead link end (the floor moved
the deadline on), found on this PC and replaced by ENet's backoff.

## (3) Headroom: what drives the soak's error, and what was done

### The starting point

`main` at `9bf5696` (the lead's numbers): the impaired soak fails at its default seed, 12.48 wu RMS (limit 12); seeds
2-4 9.8, 8.8, 7.3; the clean soak 10.33 (5.63 a day before). Merged into `m4-net` with the fixes of (1) and (2), ten
seeds each (`scripts\net-seeds.ps1`, `captures\m4net\seeds\merged_*`):

| | mean | median | worst | passed | down worst | up worst |
|---|---|---|---|---|---|---|
| impaired (5% loss, 80 ms jitter, a pause) | 9.33 | 9.17 | 12.94 | 8 of 10 | 233.1 | 53.4 |
| clean | 8.28 | 7.69 | 13.90 | 9 of 10 | 225.6 | 56.4 |

### What the error is made of (the soak's true error, broken down; 20 seeds)

- **Shoals: 57-95% of the squared error** (36% of the on-screen samples). Boids drift apart fastest: in the worst seed
  (10, impaired; `captures\m4net\diag_seed10.txt`) a Shoal copy is 11 wu off within 0.25 s of its last correction, 25 wu
  within a second, 46 wu within two; every other family 5, 6 and 11 wu. Carom is most of the rest (2-26%).
- **The tail**: errors of 50 wu and more are 50-80% of the squared sum from 1-2% of the samples (copies that went
  another way within one to four seconds of their last correction).
- **The kind**: a copy following another player than the host's copy: 10-35% of the squared sum from 1-2% of the
  samples; knockback (upgrades' pushes, the panic pulse) 2-7%; Freeze Frame's frozen state differing 0-8%; a
  different state (tell, dash) under 2%. The drawn offset adds little (the simulated position alone: 12.28 against
  12.94 wu in the worst seed).
- **The player the host's copy follows**: this client's own 48-85% (most samples), the host's 7-50%, another client's
  7-38%: no class stands out per sample, so the remote players' delay is not the main driver.
- **The Hive's darts** (the bodies a mini-boss calls): 0-9% across the merged seeds; once the Shoals are corrected
  well they become 15% at the default seed (61.6 wu RMS on few samples). See below.

### Experiments (ten seeds each, impaired / clean means; `captures\m4net\seeds\`)

| Change | impaired mean (worst) | clean mean (worst) | note |
|---|---|---|---|
| merged baseline | 9.33 (12.94) | 8.28 (13.90) | |
| remote players predicted to the tick time (`--remote-poses predict`) | 9.50 (15.04) | 7.34 (12.91) | old content; not kept: the relayed poses are too old to carry |
| Shoals' priority x2 (`fam2=2`) | 8.26 (10.17) | | |
| Shoals' priority x4 | 11.69 (21.08) | | starves the rest |
| corrections' budget +30% (`down=26000`) | 7.14 (10.48) | | down worst 260 kbit/s: over the limit |
| motion corrections | 8.45 (12.71) | 6.13 (7.84) | |
| motion corrections + Shoals x2 | 7.08 (9.78) | 5.55 (10.74) | |
| + the pause's lead, claims capacity | 6.82 (11.56) | 5.55 (10.74) | 10 of 10 pass each way |
| + the Hive's Spear carried on | 6.47 (8.69) | 5.39 (10.53) | one impaired seed failed its upload (62 kbit/s) |
| + remote players predicted on direct links (final) | **6.03 (8.16)** | **5.19 (8.03)** | 10 of 10 pass each way; up worst 59.8 / 55.3 |
| (tried, dropped) a byte budget on plain hit claims | identical | | the upload's worst seconds are claims of kills, which cannot wait |

### What changed (all in `src/net` but one constant)

- **Motion corrections** (`Msg::Motions`, `NetMotion`, protocol `0x52460504`): a moving Shoal's correction is its
  place, heading, the player it follows and its knockback, 8-12 bytes against 31 (the serial a varint step from the one
  before); every fourth correction of it, and the first after it took damage, is still a full one; the client keeps its
  copy's musical state, health and random stream and applies the rest as any correction (`netCorrect`).
- **Shoals gain priority twice as fast** (`SessionConfig::familyWeight`).
- **The pause** takes hold at least 1.5 of the worst client's resend timeouts ahead (120 ms let a client under 80 ms of
  jitter and 5% loss run past the beat before it heard: the soak's "every peer on the same tick: NO" on some seeds).
- **`NetLog::kClaims`** (src/sim/world.h) 512 -> one per enemy slot: a client's claims of one tick passed 512 with M5
  part 3's upgrades (95 lost in one soak: kills the host never heard of).
- **The Hive's Spear** (src/sim/world_boss.cpp, `bossGeom`, clients only): the host's word on a Spear reaches a client
  after the moment it was made; the client used to put the Spear back where it stood then, though the host's had
  charged on since (700 wu/s). It is now carried on by the charge made since. The darts were 15% of the default seed's
  squared error (61.6 wu RMS) before; the impaired seeds' worst fell from 11.56 to 8.69 wu.
- **Remote players on direct links are predicted** (`RemotePoses::Direct`, the default; `--remote-poses interp|predict|
  direct`): the host takes each client, and a client the host, where it is at the tick's time, carried on from its
  newest pose (10 ms old on a LAN, at most 150 ms carried) with a new pose's jump spread over 100 ms; the relayed
  players (a client's view of the other clients) stay 66 ms behind, interpolated. Before, the host's enemies chased a
  client's ship where it had been 66-80 ms earlier and the client's copies where it was: the "this client's own
  player" class was 48-95% of the squared error, and on the Moto one bot's copies were 15 wu RMS off through it.
  Predicting every player (relays too) was tried and was worse: a relayed pose is 45-110 ms old and carries badly.
  This is a deviation from TECH_SPEC 8.1 ("shown with about 66 ms interpolation"): those ships are drawn predicted and
  smoothed. Not looked at on a screen.

### What in the simulation would have to become the host's (not changed; for the lead)

What each device decides from its own view of the other players, and so cannot agree on (measured in the soaks):

- **Kick Pulse's ring** (world_upgrades_m5.cpp, every beat, a shove to every body in a ring round the player): on the
  default seed with Direct it was 73% of the clean soak's squared error (knocked bodies 13% of the samples, 18.9 wu RMS).
  Host-only like Gravity Rounds' tug (the knockback reaches clients in the corrections) would remove it.
- **Colossus's ram shove**, **Black Hole's and Singularity's pulls** (`pullBodies`), **the panic pulse** of a teammate's
  hit: the same kind (bodies moved by where a remote player is); smaller in these runs (knocked 2-7% elsewhere).
- **Enemies' choice of player** (`targetOf`: the nearest, with stickiness): a copy following another player than the
  host's was 10-35% of the squared error from 1-2% of the samples. The correction carries the host's choice, but the
  next look (every fourth sixteenth) can choose again on the client's view. Making the choice the host's (sent with the
  corrections and kept until the next) would remove the class; it is a simulation change, not done.
- **Shoal flocking** stays chaotic whatever is sent (boids amplify any difference); motion corrections and their weight
  hold it now, but a crowd of Shoals larger than today's will need more of the downlink.

(The lead ruled on the first four in round 2, below; they are now changed.)

## Round 2 (the lead's points after the merge was accepted)

### The lag at an enemy's first correction: the hit's dip (point 1)

On the Moto runs one bot's copies were 50-100 ms of motion behind the host's along their velocity, most at an enemy's
first correction, worst in the late Tracks. Cause: **a player's hit slows that player's own device's whole simulation to
85% for 200 ms** (`dipS_`, look.md 5's dip) and no other device's: each hit put that device's entire crowd 30 ms of
motion behind the host's, and the bot hit most lagged most. Late spawns were not it (0 on the clean runs, 41-57 in 15
minutes under the impaired profile). **Fix** (`src/sim/world.cpp`, co-op only): the dip slows only the drawn lattice, not
the simulation. Solo play and the determinism hashes are unchanged.

### Shoves and pulls, and the player an enemy follows (points 3 and 4, as ruled)

(Taken out again after round 3 with the hit's dip, by the lead's ruling: on today's content the three cost about 1 wu;
see "A fresh baseline on main 1dce5d3" below. What follows is what was tried.)

- `World::firstHand(p)` (`src/sim/world.h`): true on the host and solo, and on a client for its own player and the
  host's. Kick Pulse's ring, Colossus's ram shove, Black Hole's, Singularity's and Collapse Field's pulls, and a
  teammate's panic pulse move bodies only where `firstHand` (`world_upgrades_m5.cpp`, `world_net.cpp`); the owner
  still sees its own on the beat. A relayed player's shove reaches a client in the host's corrections, and the host
  now gives a body knocked faster than 30 wu/s 6 more weight (`session.cpp`).
- `targetOf` on a client keeps the player an enemy follows until the host says otherwise; it chooses again only when
  that player is down or gone. `netCorrect` takes the host's choice whatever lies between. On the host a changed
  choice goes to the top of that client's queue (+200 priority). Clients only: host, solo and hashes unchanged.
- Measured: the seed where knockback was 73% of the squared error now has it at 4% (clean, seed 1). Ten seeds each
  (`captures\m4net\seeds\r2_*`): impaired mean 6.63 wu (worst 10.18; before round 2 6.03 / 8.16), clean 4.40 (worst
  8.33; before 5.19 / 8.03); 20 of 20 pass; the default seed 9.13 / 3.15. A copy following another player than the
  host's copy is now the largest class (61% of the clean default seed's squared error, 2% of the samples): the
  client waits for the host's word, which under loss and jitter comes late.

### Remote players predicted on direct links, measured (point 2)

The soak now compares each viewer's copy of every other player with that player's own record at the same song time
(`remote ships` and `remote aims` lines). Default seed, ten minutes (`captures\m4net\r2\`):

| | host's view of a client | client's view of the host | client's view of a relayed client |
|---|---|---|---|
| clean, interp: RMS / P95 / max (wu) | 32.4 / 41.0 / 150 | 34.9 / 38.8 / 137 | 33.4 / 41.8 / 200 |
| clean, direct | **9.2 / 16.0 / 115** | **11.2 / 18.5 / 112** | 32.5 / 40.8 / 180 (interpolated) |
| clean: unexplained step max, frames over 14 wu (half a hull), interp | 40.1, 190 of 40,114 | 39.2, 370 of 22,900 | 76.5, 854 of 94,028 |
| clean, direct | 40.0, 139 of 41,623 | 40.4, 162 of 21,012 | 58.6, 874 of 94,228 |
| impaired, interp: RMS; step max, frames over 14 wu | 31.5; 62.5, 355 of 39,738 | 35.8; 132, 495 of 25,801 | 40.2; 344, 6,827 of 86,525 |
| impaired, direct | 19.6; 278, 444 of 38,937 | 23.4; 284, 775 of 25,086 | 38.9; 324, 6,342 of 86,574 |

On a clean LAN direct is three times closer to the truth and steps no more than interp; the largest unexplained steps
(40 wu in both modes) are a dash's first frames, which a copy sees one pose late in any mode. **Not met**: "no frame's
unexplained step above half a hull" fails in both modes (0.3-1.6% of frames on a clean LAN). Under the impaired profile
both modes jump (to 278-344 wu); direct jumps further on direct links. The fallback the lead asked for (a link past an
age or jitter threshold interpolating by itself) is **not built yet**; the threshold from these numbers would be a pose
age above about 60 ms or its spread above 30 ms (clean links: 8-11 ms, impaired: 26-213 ms).

## Round 3 (after main 3b2f03b was merged back: `f92ab51`)

### A direct link falls back to interpolation by itself (point 2)

`Session::fillRemote` keeps, per directly linked seat, an average of the pose's age when it lands and of its spread (EMA).
A link interpolates (66 ms behind, as relayed seats do) while the age is above 50 ms or the spread above 15 ms, and
predicts again once both are back under 35 ms and 10 ms (`kFreshAgeMs`, `kFreshDevMs`, `kFreshAgeBackMs`,
`kFreshDevBackMs`; clean links measure 8-11 ms of age, the impaired profile 44-109 ms). `net: diag` counts the ticks
each direct link spent interpolated. Clean runs never fall back (the clean seeds are bit-identical to round 2's).
Default seed, impaired, ten minutes (`captures\m4net\seeds\r3_imp` without, `r3b_imp` with):

| impaired, seed 1 | host's view of a client | client's view of the host | client's view of a relayed client |
|---|---|---|---|
| direct, no fallback: RMS / P95 / max; unexplained step max, frames over 14 wu | 20.7 / 38.5 / 231; 227, 528 of 40,059 | 24.1 / 43.5 / 248; 264, 675 of 23,411 | 40.2 / 57.5 / 277; 296, 6,715 of 85,970 |
| direct with the fallback | 31.5 / 40.3 / 196; **62.5**, 357 of 40,446 | 35.2 / 40.8 / 198; **114**, 470 of 22,911 | 40.7 / 58.5 / 281; 344, 7,068 of 86,428 |

The fallback trades closeness (RMS 21-24 -> 31-35 wu, interpolation's) for steadiness on the links it covers (largest
unexplained step 227-264 -> 62-114 wu). A relayed seat was already interpolated; its jumps are the relay's jitter
(80 ms on two hops) and are unchanged. Ten seeds: impaired mean 7.21 wu (worst 10.38; without the fallback 5.95 /
7.43), clean 4.87 (8.33), 20 of 20 pass. The impaired means differ within the seeds' spread (one soak's error swings
with what its run did). TECH_SPEC 8.1 deviation, recorded: remote players are drawn predicted (extrapolated from their
newest pose, its offset smoothed over 0.1 s) on direct links, interpolated on relayed ones and on a direct link that is
late or uneven.

### Point 5: Tracks 4-6 and the new fights in co-op (40-minute soaks, `captures\m4net\part5\`, round 3 code)

The soak plays the set list of six Tracks, then the loop; the on-screen error (true, at the same song time) by the
host's Track and by fight, seeds 1 and 2, in wu RMS (limit 12 for the whole run):

| | clean s1 | clean s2 | impaired s1 | impaired s2 |
|---|---|---|---|---|
| Track 4 / 5 / 6 | 5.4 / 5.8 / 8.3 | 8.5 / 5.3 / 8.0 | 15.2 / 9.3 / 11.1 | 15.9 / 11.7 / 14.3 |
| loop, Tracks 7-11 | 13.7-20.9 | 10.1-15.9 | 12.1-19.4 | 12.1-25.9 |
| Feedback | 6.7 | 9.9 | 12.0 | 13.7 |
| Polyrhythm | 10.3 | 8.1 | 13.7 | 16.2 |
| **Event Horizon** | 14.2 | 26.6 | **77.5** | **82.1** |
| Moth Queen | 13.5 | 13.9 | 14.5 | 18.9 |
| Brood Mother | not reached | not reached | not reached | not reached |
| Blink Star | 9.9 | 15.2 | 13.8 | 28.3 |
| Sentry Array | 8.4 | 6.0 | 11.2 | 19.1 |
| whole run (RMS; at correction) | 9.35; 12.3 FAIL | 7.88; 11.0 FAIL | 11.1; 21.7 FAIL | 14.9; 21.4 FAIL |
| worst second down / up (kbit/s; limits 250 / 60) | 224 / **89.4** | 226 / **77.4** | 235 / **93.5** | **255** / **84.2** |

- **Event Horizon** was the worst by far. Broken down (the soak's new `Event Horizon:` line: lane walkers, the rest,
  copies whose Well is in another phase, the Well's place against the host's), three causes, each fixed on clients only
  (`world_net.cpp` `netCorrect`, `world_horizon.cpp` two helpers, `session.cpp` the heartbeat); solo, the host and the
  determinism hashes are unchanged:
  1. **Lane walkers** (95-107 wu RMS): `horizonLane` draws a walker every tick from its lane and its distance along it
     (`e.a`, `e.b`), so a correction's x/y was undone the next tick; the distance came only when no beat lay between
     and was never carried to the client's time (the walk is 100-500 wu/s near the Well). Now a walker always takes the
     host's lane and distance, the distance carried by the walk (speed plus pull, a tick at a time): 95-107 -> 4-16 wu.
  2. **The phase change** (550-630 wu RMS while it lasts): the Well's lanes go from two to three to four with its phase
     and every walker turns with them; a client changed phase on its own copy of the health, up to a heartbeat (250 ms)
     plus the latency away from the host. The host now sends its heartbeat (the boss's health) at once when a boss's
     phase changes: no sample left with the phases apart.
  3. **The pull on the rest** is not in a body's velocity, so a correction is now carried by the pull too (small).
  The Well itself agrees (its place 0-3 wu RMS from the host's). 25-minute soaks, the Horizon fight's error: impaired
  seed 1 79.9 -> 76.7 (lanes) -> **7.1** (phase); clean seed 2 52.3 (lanes fixed, phase not) -> **9.8**; impaired
  seed 2 96.9 -> **9.4** (lanes; its phase changes did not fall apart). `captures\m4net\part5c` .. `part5f`.
- After the Horizon fixes, the boss claims merged and main 17a36f7 merged (`captures\m4net\part5g\`, same seeds,
  40 minutes; the runs take other courses, so fight by fight they move both ways):

  | | clean s1 | clean s2 | impaired s1 | impaired s2 |
  |---|---|---|---|---|
  | Track 4 / 5 / 6 | 5.0 / 4.5 / 5.3 | 8.6 / 4.8 / 5.9 | 9.1 / 9.5 / 10.0 | 9.4 / 7.0 / 10.0 |
  | loop, Tracks 7-11 | 5.9-14.1 | 6.3-15.4 | 13.6-21.7 | 7.5-17.9 |
  | Feedback / Polyrhythm / Event Horizon | 7.4 / 6.2 / 11.9 | 8.7 / 6.7 / 7.0 | 9.0 / 9.0 / 25.6 | 15.2 / 10.1 / 30.1 |
  | Moth Queen / Blink Star / Sentry Array | 11.7 / 6.1 / 5.8 | 15.3 / 8.2 / 6.9 | 24.4 / 18.4 / 14.6 | 15.4 / 14.8 / 8.3 |
  | whole run (RMS; at correction) | 6.50; 8.64 | 6.98; 8.68 | 11.7; 15.3 FAIL | 9.42; 16.2 FAIL |
  | worst second down / up | 230 / **68.1** | 229 / **72.1** | 231 / **79.8** | 222 / **70.9** |

  Brood Mother is still never drawn by these seeds. Event Horizon under the impaired profile is down from 78-82 to
  26-30 (lanes 16-29, the rest 36-38: its pulled crowd under 80 ms of jitter); clean 7-12. Moth Queen is the worst
  fight left (impaired 15-24).
- **The uplink in the loop** still fails over 40 minutes: 61-80 kbit/s in the loop's densest seconds (Tracks 9-11,
  past 25 minutes of play), 30-36 of it reliable payload: 1,100-1,400 claims a second after merging (one an enemy),
  two in three carrying a damage. Grouping the claims by damage was tried (the damage then changes on one item in
  three) and made no measurable difference to the worst seconds; it was taken out. Left: claims at 15 Hz (more hits
  merge per enemy; kills confirmed 33 ms later), acknowledgements only with the 30 Hz pose (about 8 kbit/s of the
  ~21 kbit/s of ENet and IP overhead), or a ruling on the loop's density. The 10-minute soaks (Tracks 1-4) stay
  under 52 on every seed.
- **Feedback** (the lead's first): no mechanism of its own shows. Its error is the crowd's in a dense fight (17% of
  the impaired seed 2 samples): Shoals 19.1 and family 4 20.1 wu RMS, a copy following another player 60.8 (9% of the
  squared error), knocked bodies 25.8 (7%); its own states never differ. Clean 8.7-9.7, impaired 13.3-17.3.
- The loop's Tracks (7-11, past 15-25 minutes of play) carry most of the whole run's error and all of the uplink's
  worst seconds: kill claims at loop densities. Tracks 1-6 alone stay at 1.1-8.5 clean and 2.7-15.9 impaired.
- Feedback, Polyrhythm, Moth Queen, Blink Star and Sentry Array sit at 6-19 wu, near or above the limit under the
  impaired profile; not yet broken down.

### The uplink's worst second: hits on the boss, one claim each (point 3)

The soak now counts each client's claims (items, hits, the most in one second) and boss claims. The test seed's
worst up second (client 3, 82.7 kbit/s, 51.1 of it reliable payload) held **920 boss claims in one second** (up to
48 in a tick), six bytes each: every shot that hit the boss was claimed on its own, unmerged, while hits on enemies
were already merged per enemy. Now a client keeps one claim a part and pair until the pose takes them (30 Hz; the
host adds them up as before, `damageBossFinal` is linear in the damage): the most in one second fell to 50-117, the
test seed's worst up second to **42.8** (the error is bit-identical: the host's health comes out the same). Ten
seeds after main 17a36f7 was merged (`captures\m4net\seeds\r4_*`): worst up second **38.9-49.6 clean, 42.3-51.3
impaired**, every seed under 60 (seed 5: 44.1; main measured 61.7 there).

### Boss claims after main 8f5abeb (armour takes a quarter) and the boss's health on clients

- On the wire: main sums a tick's hits on one part and pair in the client's log (`damageBossFinal`); this branch then
  keeps one claim a part and pair until the pose takes them (30 Hz, `Session::writeBossClaims`). Both add the damage
  as it is (the claimant's multiplier, armour's quarter included, already applied), and `damageBossFinal` on the host
  is linear in it, so the host's health comes out the same: summing is what we want. One claim a part and pair a pose,
  six bytes each; a parser cap of 1e6 per claim is far above any sum (`kMaxBossClaimSum` 1e5 splits a larger one).
- The boss's health on each client against the host's (the soak's new `boss health` line, every 0.1 s of a fight,
  a share of the whole): RMS 3.1-5.2%, at most 25-52% for a moment, and a client is above the host 27-40% of the time
  (its bar waits for the others' damage, which reaches it in the 4 Hz heartbeat). The fights now end in seconds, so a
  quarter second of four players' damage is a large share of a short fight. After a fight the copies agree (the host's
  death decides; a client's boss was still alive 0.3-1.8 s in all while the host's was dying, the death's latency).
  A 10 Hz heartbeat during fights was tried (`--net-tune bossbeat=1`): no clear gain in ten seeds, so it is off.

### A fresh baseline on main 1dce5d3, and what round 2's rules cost now

Main's own tree (exported with `git archive`, built in the scratchpad; main holds this branch's round 1 network code,
not round 2's) against this branch after merging it, ten seeds, ten minutes (`captures\m4net\seeds\main1dce_*`,
`r9_*`, `exp*_*`):

| ten seeds, mean (worst) | clean | impaired |
|---|---|---|
| main 1dce5d3 | **4.07** (6.68) | **6.05** (8.28) |
| this branch | 5.07 (6.91) | 7.06 (10.12) |
| this branch, round 2's three client rules off (the dip, `firstHand`, the host's target as law) | 4.20 (5.49) | 6.00 (6.72) |
| only `firstHand` off | 3.94 (4.83) | 6.76 (8.73) |
| only the target rules off | 4.33 (6.90) | |
| only the dip's co-op rule off | 4.50 (6.89) | |

Each of round 2's rules, off on its own, lands at 3.9-4.5 clean; all three off match main. Measured in round 2 (on
the content of that day) they cut the clean mean from 5.19 to 4.40; on today's content (Kick Pulse on the kick, armour
taking a quarter, the novice retune) **together they cost about 1 wu**, clean and impaired. The ten-seed means swing
by ±0.7 wu between builds that differ in nothing the runs reach (the 10 Hz heartbeat in fights: 4.86 against 5.73), so
the single rules are not told apart; the three together are.

**Ruled and done (the lead, after 7061fb0): the three rules are out.** `world.cpp` and `world_upgrades_m5.cpp` are
main's again; `World::firstHand`, the client's `targetOf` law and `netCorrect`'s target-whatever-lies-between are gone
from `world.h` / `world_net.cpp`. Every device applies every player's shove and pull from its own view, as before
round 2, and the owner still sees its own on the beat. The host's side stays (a changed target and a knocked body
still weigh more in its queue). Ten seeds (`captures\m4net\seeds\r10_*`), with the rules in -> out:

| seed | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 | 10 | mean |
|---|---|---|---|---|---|---|---|---|---|---|---|
| clean | 5.20 -> 3.21 | 4.00 -> 5.49 | 3.48 -> 4.07 | 6.91 -> 5.34 | 4.09 -> 5.22 | 6.36 -> 3.53 | 5.88 -> 2.72 | 4.80 -> 4.37 | 4.58 -> 2.91 | 5.43 -> 5.18 | 5.07 -> **4.20** |
| impaired | 7.04 -> 5.28 | 5.63 -> 5.88 | 9.12 -> 5.87 | 6.55 -> 6.72 | 6.44 -> 6.14 | 5.34 -> 6.17 | 6.26 -> 4.93 | 6.67 -> 6.37 | 7.43 -> 6.38 | 10.12 -> 6.25 | 7.06 -> **6.00** |

The guard (a seed over 10 wu now that was under it with the rules in): none; the worst is 6.72. Seed 5 clean fails on
the uplink, **60.8 kbit/s** (client 3, 1,032 claims in one second): the uplink item below. `scripts\test.ps1` passes
(the soaks 3.21 / 5.28; hashes unchanged).

### Main 5107877 and the test seed's clean figure (point 3)

Measured on this branch after the merge: the test seed (seed 1) clean **4.55** wu, seed 5 clean 3.83; ten seeds
clean mean 5.13 (median 4.42, worst 9.31), impaired 7.77 (median 7.29, worst 10.88); 20 of 20 pass. What 5107877
could decide from another player's state, measured: a copy's thinning against its owner's differs in **59 of 69,600
looks** (0.08%; the owner thinned in 84) on the test seed; Wave's and Helix Array's curves are a function of each
shot's own flight (`life0 - life`) and its muzzle, so they depend on nothing remote beyond where the copy fires from.
The live shot counts of a copy do differ from the owner's (45.9 RMS; thinning reads them on a beat), which matters
only near the soft cap. So nothing in 5107877 shows a systematic effect: every shot path changed, so every run took
another course, and one seed's figure swings with what its run met (on this branch the test seed's error is mostly
the Hive fight: 56% of its squared error from 7% of its samples). If thinning ever needs to agree, the owner's live
count can ride in its pose (one byte, count / 4).

## Round 4 (after the lead's rulings on 7061fb0; round 2's rules out at `d5be6ea`)

### (a) The impaired Moto run on the final APK (`182905`, nothing else of mine on the PC)

Moto hosting three PC bots, 15 minutes, 5% loss and 80 ms jitter on every peer's sends (the phone's too), APK and bots
from `d5be6ea`. Shared phone; our APK deployed first. **0 PC stalls** on every bot.

| bot | whole run (limit 12) | outside Event Horizon | Event Horizon's 20 s (bars 371-386) | worst up / down second |
|---|---|---|---|---|
| 1 | 11.86 | 11.46 | 29.0 (242 samples) | 48.1 / 222.6 |
| 2 | **12.20** | **12.16** | 19.1 (113) | 51.5 / 203.5 |
| 3 | **13.30** | **12.84** | 57.8 (98) | 43.4 / 228.0 |

**Not met** for bots 2 and 3, and not because of Event Horizon: outside its 20 s they are 12.2 and 12.8. The windows
over 20 wu are in Track 5 (cycle 2 to the boss, 30-50 wu a window) and Track 6; the soaks put Track 5 impaired at
7-10. The Event Horizon windows fell from 170-182 to 19-58 on the same measure.

Repeated on round 4's APK (`190641`, `d531af0`: claims at 10 Hz in dense seconds, the new at-correction measure;
0 PC stalls, Moto 53,904 frames, 0 missed, P95 17.8 ms, heap 0):

| bot | whole run | outside Event Horizon | Event Horizon (bars 368-383) | worst up / down |
|---|---|---|---|---|
| 1 | 12.06 | 12.13 | 5.1 | 45.2 / 209.0 |
| 2 | **23.52** | 23.68 | 9.4 | 54.8 / 226.0 |
| 3 | **18.38** | 17.17 | 61.5 | 44.9 / 211.9 |

**Not met.** Bot 2's figure is one 10-second window at 300 s (Track 2, 211 wu over a few hundred samples); bot 3 is
25-57 wu through Tracks 4 and 5. Two runs of the same profile differ this much (12-13 against 12-24), so one 15-minute
phone run does not tell builds apart; the soaks' ten seeds do (impaired 6.0-6.2). Event Horizon on the new measure:
5-62 wu. What the device's Track 4-5 windows are made of (bot 3's 841 s line on `182905`): copies **not corrected for
over 4 s** carry 94% of the squared error, and enemies following another (relayed) player 85%: on the phone host, under
the impaired profile, its corrections do not reach every enemy on a client's screen in time. That is the next thing
to measure (the host's queue on the phone: what it sends a client a second against what the client's screen holds).

### (b) The device-only spike in Event Horizon: the measure

The device runs' `scr` (a client's drawn copy against the host's sample as each correction lands) carried the
host's sample to now **by its velocity alone**. A lane walker on its spiral (100-500 wu/s, curving hard near the
Well) and a body in the pull (the pull is not in its velocity) are not where that straight line puts them, so the
measure itself read tens to hundreds of wu while the copies stood where the host's were; the soak's true error (both
worlds at the same song time) does not carry anything. Now `World::netHostPlace` carries the sample the way
`netCorrect` places the copy (velocity, pull, lane), and the session's measure uses it; for every other enemy it is
unchanged. The soak prints this measure by fight (`at correction ... by fight`) beside the true error by fight.

Measured in the soak (40 minutes, `captures\m4net\long\r11_*`, `r12_*`), Event Horizon: at correction 55.7-59.5 wu
where the true error is 14.4-42.5 (impaired seed 2: 14.8 against 13.7). So the soak shows the same inflation on the
device runs' measure, even carried as `netCorrect` carries: as a correction lands, a fast walker's drawn copy still
holds the last correction's offset (it decays over 150 ms) while its simulated place is right. **The device run has
nothing the soak lacks**: the phone's 19-58 wu in that fight is the measure's reading of a fight the soak puts at
14-43 true. The other fights read 1.3-2x their true error at correction (Moth Queen 23-29 against 13-18).

### (c) The loop's uplink

Two changes, measured over 40 minutes (seeds 1 and 2; `captures\m4net\long\`):

- **Claims at 10 Hz in a dense second**: when a client's world has made more than 400 claims in the current or the
  last second, its claims (and boss claims) leave with every third pose instead of every pose: more hits on one enemy
  merge, fewer reliable datagrams go. The claimant's own kills show at once as before; a teammate sees one at most
  67 ms later (the lead's rule: 100 ms). `--net-tune dense15=0` switches it off.
- **Acknowledgements only with the pose** (`ackpose=1`, off by default): a running client services its link on pose
  frames only (every frame while a ping is out, so the clock's pongs are timed as they land). It saved only 1.6 kbit/s
  on the mean and moved the host's worst down second from 220 to 242-258 (over 250): the acknowledgements arriving up
  to 33 ms later make the host send in bursts. Kept as a switch, off.

| 40 minutes, worst up second (limit 60) | clean s1 | clean s2 | impaired s1 | impaired s2 |
|---|---|---|---|---|
| before (part5g, round 3) | 68.1 | 72.1 | 79.8 | 70.9 |
| neither change (`r11base`) | 64.3 | 68.1 | 81.8 (worst of the two) | |
| claims at 15 Hz only (`r11dense`) | 58.4 | 62.2 | 67.6 | 71.6 |
| both, 15 Hz (`r11`) | 56.2 | 59.8 | 71.5 (worst of the two) | |
| **claims at 10 Hz (`r12`, default)** | **58.5** | **59.3** | **67.2** | **67.0** |

Clean is now under 60 in the loop; **impaired is not (67)**. In those seconds the payload is 29-46 kbit/s (claims
16-35, poses 11) and ENet's own share (UDP/IP and ENet headers of ~55 datagrams, acknowledgements of the host's
reliable stream, resends under 5% loss) is the other 21-37. Ten seeds (`r12_*`, `r13_clean`): clean 4.24 (worst 5.69),
impaired 6.22 (8.05), worst up second 44.4 / 47.4 (seed 5: under 60 now). Left: fewer acknowledgements need fewer
reliable messages from the host (the reliable stream is spawns, deaths, events and claims' verdicts), or the limit's
reading for the loop under loss.

### (d) Relayed seats under jitter

The interpolated seats now spread a pose's jump over 100 ms as a predicted one's (`relaysmooth=1`). Measured (impaired
default seed, `captures\m4net\r4\relay*.txt`): a client's view of a relayed client, frames with an unexplained step
over half a hull 6,487 -> **2,320** of 85,000, but drawn against the truth RMS 39.6 -> 49.1 wu, and the simulation
takes that place too: enemies chasing a relayed player drift from the host's, impaired ten seeds **6.22 -> 7.59**
(worst 8.05 -> 10.83). Off by default; the steps above 120 wu are snaps either way (a revive, a respawn). The way to
have both is an offset in the view only (the ship drawn smoothed, the simulation's pose unsmoothed): in the game's
view code, not done.

### After merging main d59062f (M5 part 5: the arena and the elites; protocol 0x52460506)

`b708c50` (the merge took main's tip, d59062f, though its message names 5f5d672). `scripts\test.ps1`: net tests 0
failures, both soaks pass (2.86 / 4.60), determinism hashes now 9a5a34b5b7459ba8 / d5d6d0ed37d0c355 (main's new
content); **one simulation test fails**, `[arena]` at sim_tests.cpp:2248 (the elites' Tracks and two traits'
numbers), which main's design commit d59062f changed ahead of its code: the same on main. (The audio suite's `seq
latest` failed once while soaks ran beside it and passed on a quiet PC.)

40 minutes, seed 1 (`captures\m4net\long\r14\`): clean 7.40 (pass; up 53.4, down 220), **impaired 17.2 FAIL** (at
correction 21.6; up 62.1, down 235.6). The loop now carries Track 6 and on at 18.7 clean (40% of the squared error
from 6% of the samples) and 21.7-28.8 impaired: the arena and the elites, new this merge, not yet broken down. Census
0 mismatches, no overflow.

## Round 5 (the lead's rulings on 7458156)

### The budgets (TECH_SPEC 8.4, the lead's ruling of 2026-11-13)

New limits per client, worst second: **500 kbit/s down, 120 up** (were 250 / 60; written into TECH_SPEC 8.4 with the
reason; `net_headless.cpp`'s pass limits moved with them). The extra downlink is spent where the measurements point:

- Corrections go to a client **30 times a second** (15 before; one packet a time, so the rate needs the cadence) out of
  **30,000 B/s** of that client's downlink (20,000 before), `SessionConfig::downBytesPerS` / `corrEveryUs`.
- An enemy on a client's screen that follows **a relayed player** (another client, whom this client sees late) gains
  three times the priority (`relayedFollowWeight`); the clients' requests for on-screen copies uncorrected for 0.75 s
  (the attention list, 5 Hz) keep their boost. Off-screen weights are as before.
- `--net-tune down=B,corrhz=H,relayw=W` for the experiments.

Ten seeds, after main d59062f (the arena and elites from Track 3), impaired unless said:

| corrections' budget a client | error, mean (worst) | down, mean / worst second (kbit/s) |
|---|---|---|
| 20,000 B/s at 15 Hz (before) | 6.61 (8.63) | 141.7 / 222.2 |
| 26,000 at 30 Hz | 5.05 (10.50) | 183.5 / 257.9 |
| **30,000 at 30 Hz (default)** | **4.58 (9.35)**; clean **2.52 (4.27)** | **210.3 / 288.4**; clean 220.5 / 300.6 |
| 40,000 at 30 Hz | 2.83 (3.76); clean 2.30 (5.42) | 272.3 / 362.3 |

40,000 was the closest but its mean (272) is over half the limit; the lead wants the mean well under half, so 30,000
(mean 210-220, 42-44% of 500). Uplink unchanged (worst second 44-49, mean 32).

40 minutes, seed 1 (`captures\m4net\long\b2\`): **clean 7.32, impaired 7.08, both pass** (after the merge, before
this: 7.40 and **17.2 FAIL**); Track 6 (the arena and elites) 8.2 clean / 7.5 impaired, was 18.7 / 21.7. Worst
second down 296-308, mean 211-220; up 55-64. `scripts\test.ps1`: net tests pass, soaks 2.52 / 3.40 wu; the one failing
simulation test is the lead's `[arena]` (design ahead of code on main).

The phone as host with the larger budget (its frame contract, the main thread's P95, the session's time) is measured
in the three impaired runs below. The session runs on the main thread: its time is inside the bench's `cpu_ms`
(before, `190701`: P95 6.14 ms, `work_ms` P99 6.89).

### The user's decisions (2026-11-13, through the lead)

- "Skip all the bad network runs, what we have should be adequate." The impaired phone runs, the impaired device
  criterion (three runs, medians under 12), impaired seed sweeps, 40-minute impaired soaks and tuning toward impaired
  figures are dropped. The phone-hosted criterion is the clean run alone (passed: 8.45 / 6.39 / 9.25, `174451`). The
  impaired soak inside `scripts\test.ps1` stays as a regression guard (limit 12). Dropped with it: the relayed seats'
  view-only smoothing and Moth Queen's impaired figure.
- "Only short targeted runs so we can get this project complete." What is left: merge main as part 6 lands; for each
  new fight one clean soak of at most 3 simulated minutes started at that fight (over 12 wu gets fixed); one clean
  5-minute Moto-hosted run with the new budgets (frame contract, main thread P95, every client). No seed sweeps, no
  40-minute soaks, no 15-minute phone runs, no breakdown of the arena by structure, event and trait (Track 6 is 8.2
  clean).
- The three impaired runs on `5db7488` were stopped: the first finished (`194333`: 6.04 / 6.44 / 6.86, 0 stalls; the
  Moto 53,903 frames, 1 missed, P95 17.76 ms, heap 0, main thread CPU P95 6.07 ms against 6.14 before the budget), the
  second was cut (not a result), the third never ran.

### Fights measured from their start (clean, 3 simulated minutes; `captures\m4net\fights\`)

`raveform_headless --net soak --fight <id> --minutes 3 [--seed N]`: every world plays the fight's home Track from the
run's start (the host draws the mini-boss named), and the soak measures from the fight's first bar for the minutes
given (about 12 s of wall time). After merging main f32a441, with the new budgets:

| fight | clean, wu RMS in the fight | note |
|---|---|---|
| Cadence (Track 7, part 6) | **5.0** | at correction 6.5 |
| Brood Mother (first time measured) | **11.4** | under 12, close |
| Moth Queen | 6.4 | |
| Event Horizon | 4.6, 3.0, **39.6, 41.5** (seeds 1-4); 20.4-37.7 when reached after the mini-bosses | over 12 on two seeds of four |

Event Horizon's clean excess is its **phase change**: the Well's lanes go from two to three to four with its health,
and every walker turns with them at once. A client changes phase on its own copy of the health, a few ticks before
the host when its own claims cross the line, a few after when another player's do (the heartbeat sent at once on the
host's change still takes a pose frame and the transit). For those ticks the client draws every walker on the other
lane set, hundreds of wu away; three changes a fight make 30-45 wu of RMS. The 30 Hz corrections, the relayed
weight and the phase heartbeat each change the seeds' figures but not the cause (`hz_s3_*`, `hz_s4_*`). Fixing it
needs the phase change to land on the same tick everywhere: the host deciding it for a tick ahead (on the next beat,
as thinning and the bar lines' verdicts already are), which changes when the phase turns on the host too. That is a
rule of the fight (horizon.md: phases by health), so it is the lead's and the content engineer's to decide.

**Ruled (the lead, 2026-11-13) and built:** in co-op Event Horizon's phase turns on the next beat (at least a quarter
beat ahead) after its health crosses the line, decided by the host and sent ahead (`bossDecide kGeomHorizonPhase`,
protocol **0x52460507**); a client turns only when the host says. Solo and the determinism hashes unchanged
(`test.ps1` passes; hashes 6d23b6e1c53af52d / a6a9479e47ca796c as after the merge). src/sim:
`world_horizon.cpp` (`updateHorizon`, co-op only), `world_boss.cpp` (`bossGeom`: the new decision), `boss.h`
(`kGeomHorizonPhase`, `HorizonState::phaseNext` / `phaseAt`, not in the hash). The same four seeds, clean, in the
fight:

| seed | 1 | 2 | 3 | 4 |
|---|---|---|---|---|
| before | 4.6 | 3.0 | 39.6 | 41.5 |
| phase on the beat | **4.3** | **3.3** | **33.3** | **20.1** |

The phase now never differs between devices (no sample with the phases apart). **Seeds 3 and 4 are still over 12**:
seed 4's walkers are fixed (lanes 4.3) and its excess is the pulled bodies (36.5) while the Well's place differs by
more than 10 wu (in Collapse the Well drifts toward the players' centre, each device's own, synced at 4 Hz); seed 3's
is a few walkers placed 100-200 wu off right after a correction (lanes 38.2; 82% of the squared error from errors over
96 wu): the cause is not found yet.

### The phone host with the new budgets (clean, 5 minutes, `202359`)

Moto hosting three PC bots, `5c17cb7`'s APK, our APK deployed first, shared phone: **1.50 / 2.99 / 2.15 wu** (limit
12), 0 stalls, census 0 mismatches; worst second down 264-280 kbit/s (mean 218), up 38-42 (mean 31). The Moto: 17,969
frames, **0 missed**, interval P95 17.73 ms, heap 0, main thread CPU P95 **5.61 ms** (P99 6.01; before the budget,
`190701`: 6.14 / 6.83). The session runs on the main thread; its time is inside that figure. (The bots' FAIL flag is
the run's end: the phone quits, one disconnect each.)

### (b) note: the earlier device figures in pulls and on spirals

The device runs before `d531af0` measured a copy at correction against the host's sample carried by its velocity
alone. Bodies in Event Horizon's pull and walkers on its spirals are not where that line puts them, so those figures
(the 170-182 wu of `171023`/`172745`, the 19-58 of `182905`) read high for that reason; from `d531af0` the measure
carries the sample as the copy is placed.

### (e) Moth Queen and Brood Mother

Not done this round. Moth Queen in the 40-minute soaks: true 13-18 impaired (at correction 23-29). Brood Mother is
still not drawn by the soaks' seeds; `--fight brood_mother` is a game argument, the soak has no such start yet.

## The jumpy player shots the user saw on the Moto (the lead's question)

The phone's runs this afternoon (PC local times, `captures\m4net\device\<tag>_*`, bench results `captures\bench\`):

| Tag | When | Profile | Build |
|---|---|---|---|
| 120658 | 12:07-12:15 (6 min) | clean | diagnostics; relay bug present (teammates frozen) |
| 121756 | 12:18-12:21 (2 min) | clean | relay counters; bug present |
| 122828 | 12:28-12:45 | clean | relay fixed, interpolation (PC stalls in that run) |
| 130551 | 13:06-13:22 | clean | motion corrections, interpolation |
| 133303 | 13:33-13:49 | clean | direct prediction, the Hive's Spear |
| 135034 | 13:50-14:08 | **impaired**: 5% loss and 80 ms jitter on every peer's sends, the phone's own included | as 133303 |
| (this round) | 14:13-14:30 | clean | round 2 |

What can make player shots jump on the host's screen, with the evidence:

- **(d) The bots' aim flicking between targets** (behaviour): every player's own aim turns more than 30 degrees in one
  frame in 6.2% of its frames (8,689 of 139,184, the soak's bots; the phone's bot is the same code). A shot leaves along
  the aim, so the stream swings with it, on the owner's own screen as on the others. A remote copy shows each flick a
  pose later; its per-frame aim turn beyond the owner's own is 30-37 degrees RMS in both modes (interp and direct alike):
  not the prediction.
- **(a) The impaired profile** (the 13:50-14:08 run): 80 ms of jitter on the phone's own sends and the bots' poses makes
  remote ships, and so where their shots start, step by up to 278-344 wu in a frame in either mode (1-7% of frames over
  half a hull). No real LAN looks like that. If the user watched that run, this is what they saw on remote players'
  shots.
- **(b) Direct mode's muzzle**: on a clean LAN direct steps no more than interpolation (above); it does not explain it.
- **(c) The shot pool and soft cap**: no shot was ever refused by the pool in the soaks (overflow 0); the soft cap's
  thinning (pairs of Split lanes taking turns above 576 live shots of a player) is not counted anywhere and was **not
  measured**; the phone's result does not report live shots.
- **(e) The frame's time base**: co-op ticks and interpolates on CPU frame time (`wireNowUs()` at the frame's start);
  solo play's band clock is the same CPU time plus a constant lead, so a solo `bench_play` would show the same.
  **Not measured on the phone.**
- **Split by owner on the host's screen** (round 3: `net: diag shots` every 20 s in the game's log, `play_net.cpp`):
  frames whose drawn step is more than a quarter off the panel's period, and live player shots whose last tick bent more
  than a quarter from a straight step at their speed, own player's against the remote players'. A remote player's shot
  has **no late start and no carry** by construction: every device spawns it on its own tick from its copy of that
  ship and its aim, so what can differ from the owner's screen is where the copy is (the remote ships above), not the
  shot's flight. Desktop host with three bots, Track 1 Drop (bar 40), 4 x 20 s each:

  | | frames off the period | own shots bent | remote shots bent | most alive own / remote |
  |---|---|---|---|---|
  | clean, loopback bots | 7-47 of 1,200 (PC timing) | 0 of 3,557-12,970 | 0 of 10,935-84,936 | 26 / 123 |
  | impaired (5% loss, 80 ms jitter on all four; three headless joiners over UDP) | 9-38 of 1,200 | 0 of 5,108-11,268 | 0 of 16,654-34,303 | 23 / 67 |

  On a clean LAN the host draws remote players' shots no worse than its own. Wave, Helix Array and Seek were not in
  these loadouts (minute 1-2): the phone run below covers 15 minutes.
- **Strips** (`captures\m4net\strip\coop_clean_*.png`, `coop_impaired_*.png`, `*_sheet.small.png`): eight frames from
  bar 40 on the host's screen (`--net-capture-bar 40 --strip 8`, new: the bar's picture and the next seven frames).
  Co-op runs on the wall clock, so saving each 2400x1080 PNG skips one or two ticks between strip frames (ticks 4483,
  4486, 4487, ...); the steps between frames are not one tick's.

## Numbers before and after (the Moto's are from a shared phone; our APK deployed before each run)

| | Before | After | Files |
|---|---|---|---|
| Moto hosting 3 PC bots, 15 min, clean: on-screen error at correction per bot (limit 12) | 35.5 / 33.5 / 21.8 | relay fix: 10.5 / 10.7 / 9.4 (`122828`); + motion corrections: 10.2 / 15.1 / 7.2 (`130551`); + direct, Hive: 5.8 / 14.5 / 11.6 (`133303`); round 2: **4.84 / 6.32 / 6.12** (`141328`); final code (reaches Track 5 and Event Horizon): **8.45 / 6.39 / 9.25** (`174451`) | `captures\m4net\device\` |
| the same, impaired (5% loss, 80 ms jitter, every peer) | 17.5 / 17.7 / 81.5 | direct, Hive: **11.45 / 6.92 / 6.59** (`135034`); round 2 merged: 12.14 / 11.26 / 13.35 FAIL (`163522`); round 3 before the lane fix: 24.6 / 29.3 / 27.2 FAIL (`171023`; the Horizon fight 124-150 wu, without it 10.0 / 15.4 / 11.6); final code: **20.3 / 15.8 / 14.3 FAIL** (`172745`; the 20 s round Event Horizon's end 170-182 wu, without them 13.2 / 12.4 / 10.7; 13-17 PC stalls a bot, my own idle soaks and builds on the same four cores) | |
| PC bots' worst up second (limit 60) | 301-764 (the host gone) | clean runs 51-68 (the end, host gone) then held; impaired 46-80 in play (`135034`, bot 2: 80.3); boss claims merged: impaired 51.0 / 55.0 / 54.1 (`172745`) | |
| The Moto's frames (contract: P95 one period, missed <= 5%) | 53,904, 0 missed | 53,903-53,904 frames, 0-1 missed, P95 1 period, max 21.0-25.6 ms, heap 0, every run; `171044`, `172805`: 53,904, 0 missed, P95 17.7 ms, max 21.1-23.9, heap 0 | `ZY22BXMJG3-bench_play-20261113-*.json` |
| Loopback soak, default seed (limit 12) | main: impaired 12.48 FAIL, clean 10.33 | round 2: impaired 9.13, clean 3.15; after main 17a36f7: impaired 7.73, clean 4.55 | `test.ps1` |
| Ten seeds each | merged: impaired 9.33 (worst 12.94, 8 of 10 pass), clean 8.28 (13.90, 9 of 10) | round 2: impaired 6.63 (10.18), clean 4.40 (8.33); after main 17a36f7: impaired 7.77 (10.88), clean 5.13 (9.31); 20 of 20 pass | `captures\m4net\seeds\r2_*`, `r4_*` |
| Soak bandwidth, worst second (limits 250 / 60) | 233 / 56 (main 17a36f7, seed 5: 61.7 up FAIL) | 10 minutes, ten seeds: 223 / 51.3, all pass; 40 minutes (the loop): 231 / 68-80 **FAIL** | |
| The Event Horizon fight (40-minute soaks, clean / impaired) | 14.2-26.6 / 77.5-82.1 | 7.0-11.9 / 25.6-30.1 | `captures\m4net\part5\`, `part5g\` |

## Changes outside src/net (for the merge)

- `src/game/play_net.cpp`: diagnostics only (`noteFrame`, `noteTickSkipped`, `noteTimeDropped`, the `net: diag` lines,
  round 3's `net: diag shots`).
- `src/platform/app.cpp`, `app.h` (round 3): `--net-capture-bar B --strip N` takes the bar's picture and the next N-1
  frames (`<name>_<k>.png`); without `--strip` as before.
- `src/sim/world.h`: `NetLog::kClaims` 512 -> `kMaxEnemies` (already on main).
- `src/sim/world.cpp`, `src/sim/world_upgrades_m5.cpp`: none any more (round 2's dip and `firstHand` taken out; main's).
- `src/sim/world_boss.cpp`: `bossGeom`, clients only: a late Spear decision carried on by the charge since (on main).
- `src/sim/world_net.cpp`: `netCorrect` (clients only) carries the host's sample by Event Horizon's pull as well as by
  its velocity, gives a lane walker the host's lane and carried distance, and lets a walker off the lane when the
  host's has left it. Round 2's `targetOf` law, target-always and the panic pulse's `firstHand` are out.
- `src/sim/world_horizon.cpp`, `world.h` (round 3): `horizonLanePoint`, `horizonLaneCarry` (const helpers for the
  clients' corrections; nothing on the host or in solo calls them).
- `src/tools/net_headless.cpp`, `tests/net_tests.cpp`, `scripts/coop-device.ps1`, `scripts/net-seeds.ps1` (new).

## Not verified, not done

- **The Moto-hosted criterion under the impaired profile is not met** on the current code (`172745`: 20.3 / 15.8 /
  14.3). The runs now reach Track 5 in 15 minutes, so Event Horizon is in every run, and its 20 s on the phone measure
  170-182 wu, far above the soaks' 26-30: the device's Horizon fight is not explained yet (real time over UDP with a
  phone host differs from the soak in the fight's length, 23 s, and in when the boss dies against the clients' copies).
  Without those 20 s two bots are still at 12.4-13.2 (Track 2's mini-boss, Track 4's mini-boss). That run also had
  13-17 PC stalls a bot from my own idle soaks and builds sharing the four cores. The clean run on the final code is
  `174451`: **8.45 / 6.39 / 9.25**, every bot under 12, 0 stalls, worst up second 41-50, the Moto 53,904 frames, 0
  missed, P95 17.7 ms, heap 0 (the bots' FAIL flag is the run's end: the phone quits, one disconnect each). The
  phone's own shot diagnostics in that run: straight shots bent in their last tick, own 1,980 of 6.19 M (1,739 in
  their first tick), remote **0 of 6.06 M**; frames off the period 19 of 54,837.
- Remote ships: a relayed seat under 80 ms of jitter still jumps (unexplained step up to 344 wu); nobody has looked
  at the remote ships on a screen.
- The phone's shot numbers before the curving shots were left out (`171023`, `172745`): remote 4.6-9.0% of steps
  bent, own 0.04-12.5%, both dominated by Wave and Helix Array; the final APK's diagnostics separate them.
- Brood Mother has not been drawn in any soak; Event Horizon's impaired 26-30, Moth Queen's 15-24.
- The loop's uplink (above); the 40-minute soaks fail at correction under the impaired profile (15-16 wu).
- `scripts\test.ps1` after merging main 17a36f7: the net tests pass (131,366 checks), the determinism hashes are
  unchanged (78fd9381c93838c6, fcbe62dd9e1c369a), both soaks pass; **7 simulation tests fail** (longwave, choir,
  horizon, polyrhythm, feedback): they check the bosses' armour at 0 damage and horizon.json's numbers, which main's
  design commit 514e4ea changed ahead of its code (the lead's "coming on main next"). Not this branch's.
- Uplink headroom: with the boss claims merged the 10-minute soaks' worst second is 38.9-51.3 kbit/s and the impaired
  Moto run's 51-55; the loop's (40-minute soaks) is 61-80: enemy claims (1,100-1,400 a second after merging), the
  ENet and IP overhead of ~55-65 datagrams a second and, impaired, resends. Sending acknowledgements only with the
  30 Hz pose would save about 8 kbit/s; claims at 15 Hz would merge more (not done).
- The PC host from the phone (Windows Firewall), the OnePlus 12R and the tablet: as in M4 stage B.

## Lines for CLAUDE.md

- `scripts\net-seeds.ps1 [-Seeds 1..10] [-Impaired] [-Extra '--net-tune','fam2=2'] [-Low] [-Out captures\m4net\seeds\x]`:
  the loopback soak over ten seeds (one seed's error swings with its run).
- `scripts\coop-device.ps1 -Run long_moto_host [-Players 2..4] [-Minutes 15] [-Impaired] -LockRoot <main checkout>
  -LockName <name> [-PhoneArgs '...'] [-BotArgs ...]` (saves the phone's log; deploys the APK as built).
- `raveform_headless --net soak|host|join ... [--remote-poses interp|predict|direct] [--net-tune down=B,famN=W,motion=0|1]`;
  the soak prints the true error broken down (by Track, by fight, Event Horizon's and Feedback's own lines), the
  remote ships and aims against the truth, the worst up second's makeup, each client's claims (the most in a second),
  thinning against the owner's and the player shots on the host's screen; `join`/`host` print `corr`, `poses`, `frames` diagnostics every 10 s; the game logs `net: diag` every 20 s.
- Protocol `0x52460505` (main's part 4 with this branch's motion corrections).
- `--net-capture-bar B --strip N`: the picture at bar B and the next N-1 frames (`<name>_<k>.png`; co-op runs on the
  wall clock, so a strip's frames are one to three ticks apart).
- The game's log: `net: diag shots` every 20 s (frames off the panel's period; player shots bent, own and remote).
