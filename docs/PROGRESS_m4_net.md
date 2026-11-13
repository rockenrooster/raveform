# RAVEFORM — M4 network follow-up (branch `m4-net`)

The network engineer's work on the three open M4 problems (the lead's brief, 2026-11-13): the on-screen error with the
Moto hosting four players, the PC bots' one-second upload spike, and the headroom of the impaired loopback soak.
Numbers taken on the shared Moto (`ZY22BXMJG3`) are marked as such. Files under `captures\m4net\` (ignored by git).

(Work in progress: this file is filled in as the work goes.)

## (2) The one-second upload spike — explained and fixed

**What it was**: real traffic, after the host had gone. When the phone's benchmark ended, its app left without a
goodbye that reached the PC bots; each bot's ENet link kept the host as a peer until ENet's timeout (5 s at least) and
resent everything still unacknowledged (its hit claims of the last moments) at the capped resend timeout of
`link.cpp` (one round-trip timeout, 50 ms at least), every 50 ms, until the link ended. Measured on this PC (a host
over UDP killed 60 s into a run, `captures\m4net\vanish\client.txt`): the client's worst second of upload was the
last, 128.9 kbit/s against a mean of 30.9, with the host silent for 3.7 s; with four players and the bots' claims of a
dense Track the same seconds read 301-764 kbit/s on the Moto run.

**Fix** (`src/net/link.cpp`): the resend cap applies only while the peer has been heard within 500 ms (every live peer
sends 30 datagrams a second); a silent peer gets ENet's own backoff. The same experiment after the fix
(`captures\m4net\vanish_fixed\client.txt`): worst second 29.3 kbit/s (mean 24.0), not at the end. The detection of a
dead host is unchanged (ENet's 5 s minimum).

The measure now also says when the worst second was, how long the peer had been silent then and how many reliable
bytes were unacknowledged (`raveform_headless --net join`'s summary).
