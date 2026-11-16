# HEADLINERS — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md`. Numbers: `modes.json` `headliners`, `kit`,
> `forms`. Screens: `screens.md`. Sound: `sound.md`.

The ten bosses, back to back, each on its own Track's music: Kaleidos, Longwave, The Choir, Event
Horizon, Polyrhythm, Feedback, Cadence, Arpeggiator, Fata Morgana, Kaleidos Reprise. Ten to twelve
minutes. The fights are the most made things in the game (each a page of its own in `design/bosses/`),
and in the set a player meets the tenth only after twenty minutes. Here they are the whole show.

What it is for:

1. Every boss in one sitting, in order, so the last one's medley of the others is heard as a medley.
2. A run measured by the clock: the faster the bill is played, the better.
3. In versus, ten short rounds with a winner each.

---

## The run

An **act** is one boss. The set's order, the set's music.

| Part | Bars | What happens |
|---|---|---|
| **Act 1's opening** | Groove 4 | The drop-in into First Light. The load-in brings the kit a run has when Kaleidos enters (`kit.atBoss[1]`); nothing spawns |
| **Between acts** | The set's Track-change Break, 8 | The band's own change of Track: tempo, key and palette cross-fade into the next act's (as built). Bars 1-2: **the top-up**, a load-in of whatever the player is short of `kit.atBoss[act]`. Bars 3-8: the fallen boss's bonus Trio, held as in the set, its ring the clock for those six bars. On the Break's first downbeat: one cell and one Nova back, and the Extra Life when this act owes one |
| **The Build** | 4 | The act's name card on its first downbeat (`ACT 3 OF 10`, the boss's name, the Track's under it). Fodder only, at half the Track's crowd target, so there are Shards to take and something for the gun to play. The boss's entrance on the last bar, as built |
| **The fight** | The boss's Drop, held | The boss as its page has it: phases, hint line, pace, its Cells, its crowd share |
| **The fall** | — | Its death as built (look.md 5, "A boss falls"). The next act's Break begins on the bar line the set's would |
| **After act 10** | Break 4 | `HEADLINERS COMPLETE` (cap 44, gold) and the time; every Shard flies in; the band holds Last Light's home chord. Then the result |
| **A death** | — | As in the set: Auto Nova, an Extra Life, then the run is over, with the act reached |

## Rules

1. **Each boss meets the build it was tuned against.** As an act's fight begins the player holds at
   least `kit.atBoss[act]` upgrades: what they took themselves counts first, the top-up makes up the
   rest, dealt as `README.md` 5 says. A player ahead of the kit gets no top-up.
2. **The fights are the set's.** Health (the novice's tune and the first boss's 0.7), armour's quarter,
   the pace rule, the hint lines, enemy bullets by Track, the camera's hold and lean, the 30 to 60
   seconds a fight should take: nothing is tuned again for this mode. A fight that runs long here
   says the kit number for that act is wrong.
3. **Between fights the player is looked after**: a cell and a Nova at each Break; "nobody meets a
   boss on their last cell" (up to three grow back at its entrance); each boss pays a Cell a phase and
   two at its death (GAME_DESIGN 5, 6.1).
4. **Extra Lives**: one is held at the start, and one drops from the bosses of acts 2, 4, 6 and 8 as
   they fall, for a player holding fewer than two.
5. **Shard levels run on** across the acts, from one: the Build's fodder and each boss's 200 Shards
   feed them, and their Trios open as in the set.
6. **No mini-bosses, no arena events, no elites.** The acts are the bosses'. A Track's structures
   stand as in the set (they are part of its room).
7. **The clock is the count.** The run's time is shown all run long (the goal line). The record is the
   acts cleared, then the time: ten acts in 9:42 beats ten in 10:05, and both beat nine.
8. **No encore.** The tenth boss's fall ends the run.

## Numbers

| | |
|---|---|
| Acts | 10 |
| Before a fight | 8 bars in act 1, 12 in the others: 17 to 26 seconds by tempo |
| A fight | 30 to 60 seconds for a player on the assist; shorter for one who aims, down to half the boss's page's shorter length (the pace rule) |
| The whole | About 10 to 12 minutes |
| Kit | `kit.atBoss`: 4, 12, 17, 22, 26, 30, 34, 38, 42, 46 upgrades for acts 1 to 10 (estimates, to be measured) |
| Build crowd | 0.5 of the Track's crowd target, fodder only |
| Record | `headliners`: acts cleared, seconds; a gold star for all ten |

## Look

- **The act's card.** The Track name card's place and manner (`design/screens.md`): `ACT 3 OF 10` at
  cap 28 in white, the boss's name at cap 110 in the new palette's gradient, and under it at cap 22 in
  the accent the Track and its tempo (`REEF  120 BPM`).
- **The bill.** While the card stands, ten four-point stars in a row under it (the mode's emblem laid
  flat, 44 px apart): the fallen acts' stars in their Tracks' accents, this act's white and flaring on
  the beat, the rest outlines at 0.3.
- **The top-up** is the load-in (`README.md` 5), with only the icons that are new.
- Nothing else is new: the fights are drawn as built.

## Sound

- Act 1 opens on First Light's bed; each later act's music arrives through the band's Track-change
  Break, as in the set.
- The act's card: the elite's arrival fifth in the new Track's key (the low fifth of detuned saws),
  and a bell on the home note an octave up for each fallen act's star, a sixteenth apart, climbing the
  scale: the bill being read out.
- `HEADLINERS COMPLETE`: the gong, the band's stab on Last Light's home chord at 1.2, and the lead's
  eight notes of the title's word (A3 C4 E4 A4 C5 E5 G5 A5) a sixteenth apart: the first sound of the
  game, as its last.

## HUD

The set's, with the goal line (`screens.md`): `ACT 3 OF 10   4:12`, the time running. In a fight the
boss's bar and hint line as built.

## Co-op

The set's co-op: bosses' health by the players, down and revive (a downed player revives at the next
act's Break), over when all are down at once. Each player has their own kit, top-ups and Trios. The
record is `team_headliners`.

## Versus: the damage race

`README.md` 8, V1 to V9. Ten rounds; a round is a boss.

| | |
|---|---|
| Counts | **Acts won.** A boss goes to the player who did it the most damage (after armour and after the pace, as the host counts it). Level on acts at the end: the most damage over the whole bill |
| Seen | Under the boss's health bar, a second bar as wide fills from the left in the players' colours, each player's share of the damage so far, the leader's part brightest. It is the race, drawn |
| A boss falls | `ACT 3: P2` in the winner's colour where the act's card stands, for two bars; a crown lands on that seat in the ranking (one crown an act won) |
| The pace | Each player's own, as in co-op: a player who aims is not held back by one who does not |
| The underdog | 20% more damage for each act they trail the leader by, up to double. Their shots' heads are drawn one Overcharge step hotter for each (look.md 5) |
| Down | The knock-out (`README.md` 7). Nobody is out of an act |
| Ends | After act 10's fall: the podium |

A boss's rule is worth four times the damage (armour takes a quarter), so the round goes to whoever
reads the fight. That is the point of the mode in versus: knowing the bosses wins it.

## The result

`screens.md`, "Results". All ten fell: the headline is the time (cap 150, gold), `HEADLINERS COMPLETE`
where `RUN OVER` stands. Otherwise the headline is `ACT 6 OF 10` and `RUN OVER` stands. The four numbers:
`BOSSES`, `HITS TAKEN`, `FASTEST` (a boss's name and its seconds), `SCORE`; when not all fell, `TIME`
takes `BOSSES`' place. The bar: one segment a boss fought, as long as its fight took, in its Track's
accent, its name under it; a star where it fell, a cross for each hit. Controls: **RETRY**, HOME.

## Caps and the wire

The director's form is new (`docs/MODES_PLAN.md`, "Forms"); the fights are as built, with what they
already send. The top-up is drawn from the seed, the seat and the act, on every device. In versus the
host keeps each player's damage on the boss and sends the shares with the boss's heartbeat (four bytes
more). The densest moments are the fights' own, already measured (`--fight <id>:<phase>:hold`).

## What to check

- The assist bot plays the ten acts through in at least 19 of 20 runs; the novice wins each first
  meeting as often as GAME_DESIGN 7.6's lines ask of the set.
- Each fight's length for the novice is within 30 to 60 seconds with the kit as measured; the careful
  bot's at least half the page's shorter length.
- Versus, two bots (one aiming, one on the assist): the aiming bot wins at least eight acts of ten
  with the underdog's help off, and no more than seven with it on.
