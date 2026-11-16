# FULL BLAST — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md`. Numbers: `modes.json` `fullBlast`, `forms.blast`.
> Screens: `screens.md`. Sound: `sound.md`.

Every power on, from the first bar: every Mod at its cap, four Riffs at level 5, their Fusions, and
Boosts falling like rain, through four long Drops on the Track of your choice. About three minutes.
It is the end of a long run without the long run: the power fantasy as a single.

What it is for:

1. The game's peak, on demand. The screen-filling light storm PLAN 1 promises is twenty minutes into
   the set; here it is twenty seconds in.
2. A score to chase that is all skill at staying unhit: the build is given, the Chain is the game.
3. For the youngest player: everything explodes and nothing ends the run.

The Moto has already been measured there: the upgrades' worst case in the set's densest Drop held the
contract with no frame missed, and that Drop held for five minutes stayed cool (PROGRESS, M5 part 3's
worst case and M7's thermal check). This mode is those two benchmarks with a score.

---

## The run

84 bars: one opening, four waves. A wave is a Build of four bars and a Drop of sixteen.

| Bars | Part | What happens |
|---|---|---|
| 1-4 | **The opening** (Groove) | The drop-in. The name card: `FULL BLAST`, the Track, `EVERY POWER IS ON`, and the four Riffs dealt (`TONIGHT: SWARM  FLAK  LANCER  MIRROR`). **The load-in is a torrent**: over a hundred icons in two bars, three and four landing on every sixteenth, the chirps running up the scale without a gap, the halo drawing all eight arcs, the Riffs lighting one after another. Nothing hostile in these four bars; a first flood warps in on bar 4 |
| 5-24 | **Wave 1, FLOOD** | The Build draws the room in as every Build does; the Drop lands with the Track's crowd at 1.6 of its target and nothing but fodder and the Track's hunters. A Boost orb every four bars |
| 25-44 | **Wave 2, ELITES** | Four elites arrive on the Drop's downbeat, one more every four bars, with the crowd. A Boost every three bars |
| 45-64 | **Wave 3, THE MINI** | A mini-boss enters on the downbeat (the Track's own; Track 1 and the bonus Tracks borrow one of Track 2's pair) with the crowd at 1.2. If it lives to the Drop's end it warps out, as in the set. A Boost every two bars |
| 65-84 | **Wave 4, EVERYTHING** | The crowd at twice its target falling to 1.6, two elites and one more every two bars, **a Boost orb every bar** |
| The last downbeat | **The end** | `README.md` 6: the impact, everything pops, the count, the result |

## Rules

1. **The loadout** (drawn from the seed, the same on every device):
   - every Mod that can be dealt, at its cap (GAME_DESIGN 6.2: all 33; the three Dash Mods stay
     retired);
   - **four Riffs at level 5**, drawn from the twelve by the run's seed, so no two runs play alike and
     the band has four instruments, as its slots allow (6.1);
   - every Fusion whose two halves are held: Helix Array and Standing Wave always, and whichever of
     the other six the four Riffs bring;
   - eight cells, Regen's second rank, seven Novas.
2. **Boosts.** Three at once, the oldest giving way (6.4). Orbs fall where the director puts them as
   in the set, at the wave's pace instead of the set's. Collector's and Encore's stacks apply, so the
   rain is thicker and lasts longer than the numbers above say.
3. **Shard levels** deal a Trio of three Boosts, as the set does once nothing is left to take (6.1),
   and a Nova each.
4. **Surge** runs in every Drop as in the set, and Afterparty's two stacks make it twelve of the
   sixteen bars, its grid a step faster.
5. **Nothing ends the run.** Hits cost cells and half the Chain as always; with no cell left, Auto
   Nova, then the knock-out (`README.md` 7). No Extra Lives fall. In this mode the knock-out is **the
   power cut**: while the ship is out its HUD band row is dark; it comes back on its bar line with the
   Drop's impact and every power lit.
6. **The crowd is the Track's**, at its second cycle's budget and target (`design/species.json`), scaled
   by the wave. The bullet rules of the set hold: bullets by Track (GAME_DESIGN 8), and the director
   buys no shooters while the bullet pool is above 70% (6.6).
7. **Thinning holds** (6.6): above a player's soft cap the outer lanes fly half as far and the
   multipliers' extras wait; it starts and ends on a beat. In this mode it will be seen in most Drops,
   and it must look as it does in the set: a fan that keeps its shape.
8. **The score** is the set's (value times the Chain; Surge doubles it). Second number: the best Chain.

## Numbers

| | |
|---|---|
| Length | 84 bars: 3:03 at 110 BPM, 2:10 at 155, 3:22 on Toybox |
| Waves | 4, each Build 4 + Drop 16 |
| Crowd scale by wave | 1.6 to 1.2; 1.6 to 1.2; 1.2; 2.0 to 1.6 (of the Track's cycle-2 target, never above the crowd's 400) |
| Elites | Wave 2: four, then one every 4 bars. Wave 4: two, then one every 2 bars. Traits by the Track, as in the set |
| Boost orbs | One every 4, 3, 2, then 1 bars |
| Riffs | 4 of 12 at level 5 |
| Record | `blast_<track>`: best score, best Chain |

## Look

- **The torrent.** The load-in of `README.md` 5 at its fullest: the icons come from the whole circle at
  once in the order of the Mods' table, then the Riffs last, each Riff's landing drawn larger (icon
  at 0.8) with its ring appearing as it lands. The last landing is the fourth Riff, on the last
  sixteenth of bar 2; the closing ring on bar 3's downbeat goes out to 700 wu.
- **A wave's name.** On each Drop's downbeat the wave's name (`FLOOD`, `ELITES`, `THE MINI`,
  `EVERYTHING`) at cap 110 in the mode's colours where a Track's name card stands, for one bar, its
  letters streaking out on the next downbeat. Drawn at 0.8 as every card, so it hides nothing.
- **The power cut** (the knock-out): the ship shatters; for its bars out, the player's pool of light
  on the lattice is gone and the band row is at 0.3. Back: a ring from where the ship re-forms (90 to
  600 wu in half a beat), every icon of the band row flashing in turn over one beat.
- **The light budget does the rest** (look.md 3): the crowd dims as it fills, the ship and its shots
  never do. Nothing in this mode may add a full-screen light that the set does not have. Reduce
  flashing clamps it as it clamps the set's Drops.

## Sound

- The torrent: the load-in's chirps (`sound.md`), three and four voices a sixteenth, aggregated like
  kills (one sound a sixteenth, its gain by the count), so a hundred landings are a run, not a roar.
- A wave's name: the Drop's impact is already there; under it a reversed cymbal four beats long ends
  on that downbeat, and the name's letters are struck with the band's stab on the bar's chord.
- The power cut: the music bus low-passed at 600 Hz while the ship is out (the pause's filter), opened
  over one beat as it returns, with the Drop's impact as a one-shot on that bar line.
- Everything else is the set's: four Riffs' instruments, the Mods' changes to the lead, the Fusions'
  sounds, the Boosts' layers. The band's fullest arrangement plays in every Drop (`--band full` is the
  load test of exactly this).

## HUD

The set's, with the goal line `WAVE 2 OF 4   BARS LEFT 51` and the run bar (`screens.md`): its four
Drops are its four lit segments. The band row shows the four Riffs and the Fusions; the Mods are too
many to show and are not shown (the row's own rule, as built).

## Co-op

Two to four players in the same storm, the crowd and the elites scaled by the players (GAME_DESIGN 8).
Each player's four Riffs are their own draw, and no Riff is dealt twice until all twelve are out (four
players hold sixteen, so four repeat). Loot instanced. The score is the team's; the record
`team_blast_<track>`.

## Versus

`README.md` 8, V1 to V9.

| | |
|---|---|
| Counts | Score at the last downbeat |
| Boosts | Shared, first come: with an orb every bar in wave 4, the rain is the contest |
| The underdog | A hit does not halve their Chain |
| Ends | The podium takes the result's place |

## The result

`screens.md`, "Results". Headline: the score. Four numbers: `BEST CHAIN`, `KILLS`, `BOOSTS`, `HITS TAKEN`.
The bar: the run's nine sections in the Track's accent, the four Drops tall, each wave's name under its
Drop; a cross for each hit. `TONIGHT'S BAND`: the four Riffs and the Fusions. Controls: **RETRY** (the
same Track, new Riffs), HOME.

## Caps and the wire

Nothing new in the world: a loadout, a form, the waves' numbers. The loadout is drawn on every device
from the seed and the seat. The densest moment is wave 4's first four bars with four players. Its
benchmarks (`docs/MODES_PLAN.md`): solo on the Moto, Track 10, wave 4 held; and the Moto hosting three
loopback bots there. If the second misses the contract, the co-op crowd's scale in this mode comes
down before anything else does (wave 4 at 1.6 falling to 1.2 for three and four players); the loadout
and the resolution stay.

## What to check

- The Moto: 60 FPS P95 in wave 4 held for 60 seconds, solo, on Track 10; zero heap allocations; the
  audio callback inside its budget with the band's fullest arrangement and four Riffs.
- The shot pool: no player above the hard cap; the thinning's share of ticks reported.
- A bot's run on each of the twelve Tracks ends on its bar with no pool refusing anything.
- The child-like bot (0.5 s reactions, no Dash) is knocked out no more than three times a run on
  Track 10.
