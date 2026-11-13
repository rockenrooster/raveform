# RAVEFORM — Elites and their traits

An elite is one body the player has to deal with on purpose (GAME_DESIGN 7.3). It is a species of
the Track's roster made dangerous, and it carries a **trait**: one more rule, shown on its body and
heard as it arrives. Twelve traits, two new ones a Track from Track 2 to Track 7.

Data: `design/traits.json` (load it, do not retype it). Tool: `design/tools/traits.py`. Looks:
`design/look.md` section 3 (the elite's light) and section 4 (each trait's treatment).

---

## Every elite

| | |
|---|---|
| Who | Any species of the Track's roster except Shoal darts and Wyrm segments (a Wyrm's head may be elite: its chain follows) |
| Size, health, score, threat | × 1.3, × 6, × 10, × 4 of its species |
| Drops | 20 Shards; one Cell, whatever the players' health (GAME_DESIGN 6.1), and its Shards |
| Co-op | Health x (1 + 0.5 x (players - 1)), as mini-bosses and bosses |
| Look | Look.md 3: hull + 0.6, 2.3 px, a white copy of the hull, glass, an aura at 0.22; a white edge chevron while it is off screen; never dimmed by the light budget |
| Arrival | Warps in like any enemy but from 90 wu and with the elite's low fifth (root and fifth of the Track's key); its trait's mark sounds a beat later |
| From | Track 2, cycle 2 |
| How often | The director buys an elite with the threat budget about once in 16 bars in Track 2, falling evenly to once in 8 bars by Track 6 and staying there; never in a Break, a boss fight or a mini-boss fight |
| At once | 3 in Tracks 2 to 6, 5 in Tracks 7 to 10, for all players together |
| Traits carried | One in Tracks 2 to 6, two in Tracks 7 to 10; in the loop one more for each time round, to four at most. Drawn by the seed from the traits the run has met; never Cloaked and Phasing on one body |

---

## The twelve

| Trait | From Track | What it does | Heard as it arrives |
|---|---|---|---|
| **Armored** | 2 | Health x 2 again. While its outer hull holds (the first half of its health) shots spark off it without the hit flash; at half health the outer hull shatters and it is an ordinary elite | A gong, short |
| **Swift** | 2 | Moves at 1.6 times its speed and turns 1.3 times as fast | An upward sweep |
| **Giant** | 3 | 1.8 times the size (on top of the elite's 1.3), health x 2.5, cannot be shoved; its death is a large kill | The sub drop |
| **Splitting** | 3 | When it dies two copies leave it, 0.6 of its size, each with 30% of its health and no trait; they do not split again | A glass crack |
| **Volatile** | 4 | Its death is a bomb with a warning: a danger ring of 160 wu fills over two beats and bursts on the beat, taking a cell from any ship inside and dealing 30 x the Track's number to every enemy inside (other Volatiles go off in turn) | A crackle |
| **Regenerating** | 4 | After two beats without being hit it heals 8% of its health on every beat | Two bells rising |
| **Shielded** | 5 | Its front 120 degrees sheds every shot; it turns to keep the shield on the nearest player at 1.2 radians a second | Two woodblocks |
| **Twin** | 5 | Arrives as two bodies with one health bar (x 1.6): damage to either counts for both, they keep 200 to 300 wu apart, and they die together | The fifth again, a beat later |
| **Magnetic** | 6 | Steals loose Shards within 300 wu, up to forty; they orbit it, and its death releases them doubled | Shard chirps falling |
| **Commander** | 6 | Enemies within 400 wu of it move 30% faster; when it dies they are stunned for one beat | A snare roll of one beat |
| **Cloaked** | 7 | Drawn at 8% between beats and in full on each beat's flash (the 110 ms envelope); it can be hit at any time, and whatever it fires is on the priority layer as always | A short reversed cymbal |
| **Phasing** | 7 | Solid only in the first half of each beat: in the second half shots pass through it and its touch does not hurt | The fifth, gated on the eighths |

The two that hide come last, in Track 7, when the player knows everything else an elite can be.
A trait changes what the player does, not only how long the elite lasts: flank the Shielded, stop
firing at the Phasing on the off-beat, walk away from the Volatile, kill the Commander first.
