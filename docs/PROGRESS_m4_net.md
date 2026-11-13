# RAVEFORM — M4 network follow-up (branch `m4-net`)

The network engineer's work on the three open M4 problems (the lead's brief, 2026-11-13): the on-screen error with the
Moto hosting four players, the PC bots' one-second upload spike, and the headroom of the impaired loopback soak.
Numbers taken on the Moto (`ZY22BXMJG3`) are from a phone shared with other sessions (our APK deployed right before
each run, the main checkout's lock `m4net`). Files under `captures\m4net\` (ignored by git).

(Being written as the work goes; the final state is at the end of the file.)

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
