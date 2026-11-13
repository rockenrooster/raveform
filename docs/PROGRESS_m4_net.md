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
is taken as gone or cut off until heard again: a resend waits at least as long as the silence has lasted, and a client
holds its poses and claims. Detection of a dead host is unchanged (ENet's 5 s).
