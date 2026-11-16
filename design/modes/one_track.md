# ONE TRACK — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md`. Numbers: `modes.json` `oneTrack`, `kit`,
> `loadIn`. Screens: `screens.md`. Sound: `sound.md`.

Pick a Track of the set and play it whole: its two cycles, its mini-boss, its boss. Two to three
minutes. It is the owner's "one mode can be playing a specific track 1-10", and it is the mode a player
opens to hear a favourite Track again, to meet one boss again, or to play for five minutes.

What it is for:

1. Every Track and every boss can be reached in one tap, by anyone, from the first launch.
2. A short run with an end: the boss falls, the Track resolves, a score stands against that Track's best.
3. It is the frame the other modes are built on: a run that starts somewhere other than Track 1's
   first bar, with a kit, and ends somewhere other than death.

---

## The run

| Part | What happens |
|---|---|
| **Start** | The drop-in, from the MODES screen or the title (`design/screens.md` "Transitions"), into the chosen Track's first Groove. The name card: `TRACK 4` over `AURORA`, and under it `BEAT ITS BOSS` (`screens.md`) |
| **The load-in** | Bars 1 and 2: the kit rains into the ship (`README.md` 5). Track 1 has no kit and no load-in |
| **The Track** | As the set plays it (GAME_DESIGN 4.2): two Short cycles, 48 bars. Its roster, threat budget, crowd target, elites, arena structures and event; from Track 2 a mini-boss on the first Drop; Shard levels and their Trios; the Break's refill |
| **The boss** | On the second Drop, held while it lives; its hint line, its pace, its Cells at each phase, "nobody meets a boss on their last cell" (GAME_DESIGN 5, 7.6), all as in the set |
| **The outro** | The boss falls (its death as built). Then four bars of Break, not the set's eight-bar Track change: no Trio opens; every Shard on the field flies to its player as under Magnet Storm; the band's pads resolve to the Track's home chord; `TRACK COMPLETE` (cap 44, gold) with the score under it stands where the name card stood, for the four bars |
| **The end** | On the outro's last bar line: the result |
| **A death** | As in the set: Auto Nova, an Extra Life, then the run is over (`RUN OVER`, `ENDED BY`), with RETRY |

## Rules

1. **The Track is the set's Track.** `DirectorState::track` is the chosen Track with the set list at
   ten: its budget and crowd by Track and cycle, `kTrackEnemyScale`, enemy bullets by Track, the
   mini-boss by `World::pickMini` (nothing met yet), the elites and their traits by Track, the arena's
   structures and event for that Track. Nothing of the loop applies.
2. **The kit** is `kit.atTrackStart[track]` upgrades, dealt and loaded in as `README.md` 5 says. A
   Fusion the dealing reaches is taken as a run would take it.
3. **The teacher's boss is Track 1's only.** The set's first boss has 0.7 of its health (GAME_DESIGN 8).
   Here that factor applies when the Track is 1 and never otherwise: bosses 2 to 10 were tuned as
   later bosses.
4. **The Extra Life** drops as in the set for that Track (Track 1: from a kill in the first Drop; from
   Track 2: from the mini-boss).
5. **Shard levels start at one.** The first level takes 100 Shards and the curve runs from there
   (GAME_DESIGN 6.1), whatever the kit: a short run should still deal a few Trios by hand.
6. **The score** is the set's: enemy value times the Chain, the Chain from one. It is this Track's
   score only, so it is kept against this Track's best and never against the set's table.
7. **NEXT TRACK** on the result (when the boss fell and the Track is not the tenth) starts the next
   Track as a new run with its own kit: nothing carries over. It is a short cut, not a campaign.

## Numbers

| | |
|---|---|
| Length | 48 bars and the fights: about 105 seconds of Track at 110 BPM, 74 at 155; a mini-boss holds its Drop at most eight bars past it; a boss 30 to 60 seconds (GAME_DESIGN 7.6) |
| Kit | `kit.atTrackStart`: 0, 6, 14, 19, 23, 27, 31, 35, 39, 43 upgrades for Tracks 1 to 10 (estimates, to be measured) |
| Outro | 4 bars |
| Record | `one_<track>`: best score, cleared, seconds. A cleared Track has a gold star on its chip |

## Look

Nothing new in the world but the load-in (`README.md` 5) and the outro's card. The outro: the palette
does not change; the lattice rests at 0.07 as in a Trio's Break; the boss's Shards and every other
Shard fly in on the first bar; on the fourth bar's last beat the card's letters streak out to both
sides as a name card's do.

## Sound

- The load-in's run of chirps and the chord that closes it (`sound.md`, "The load-in").
- The outro: the Break's pads on the Track's home chord for four bars (the band holds the first chord
  of its progression), the lead silent, a gong on its first downbeat under the boss's own death sound;
  on its third bar a reversed cymbal four beats long ends on the last bar line as the result arrives.
- `TRACK COMPLETE`: the "upgrade taken" chord on the Track's home chord, an octave up, as the card
  finishes writing itself.

## HUD

The set's, unchanged. The phrase bar already shows where the boss comes (`DROP IN n`).

## Co-op

The set's co-op for one Track: one arena, the crowd and the fights scaled by the players (GAME_DESIGN 8,
10), loot instanced, down and revive, the run over when all are down at once. Each player has their own
kit and their own load-in, in their own colour, at the same time. The result is the team's (`screens.md`);
the record is `team_one_<track>`.

## Versus: the score race

`README.md` 8, rules V1 to V9. What this mode adds:

| | |
|---|---|
| Counts | Each player's own score when the boss falls |
| The crowd | One crowd, shared. A kill's score goes to the player whose shot, Riff or Nova made it (the claim's player), as in co-op |
| The boss | Its 200 Shards go to every player, as in co-op. Its health and its pace are co-op's (the pace is each player's own aimed damage) |
| Boosts | Shared: first come (V4) |
| Down | No beacon and no revive: the knock-out (`README.md` 7) |
| The underdog | Their Shards count double for the Chain until the next bar line |
| Ends | With the outro: the podium takes the result's place |

THE SET takes versus by the same rules: the count is the score when the tenth boss falls, and the match
ends there (`SET COMPLETE`), without the encore.

## The result

`screens.md`, "Results". Headline: the score. The four numbers: `TIME`, `BEST CHAIN`, `KILLS`, `BOSS`
(its seconds, or a dash when the run ended first). The set-list bar is this Track's eight sections in
its accent, with the star, ticks and crosses as built. `YOUR BAND` as built. Controls: **RETRY** (the
same Track, a new seed), **NEXT TRACK** (rule 7; not on Track 10, not after a death), HOME.

## Caps and the wire

No new state but the kit (a loadout) and the run's setup (`docs/MODES_PLAN.md`, "RunSetup"). The kit is
drawn on every device from the seed and the seat, so nothing of it is sent. The densest moment is the
set's own for that Track: no new benchmark.

## What to check

- The assist bot clears every Track in at least 19 of 20 runs of ONE TRACK, and each boss's fight
  lasts within 30 to 60 seconds for the novice: the same lines as the set (GAME_DESIGN 7.6). A Track
  that misses has its kit number looked at before anything else.
- THE SET is untouched: its determinism hash for a given seed is the same before and after this mode
  is built.
