# EVENT HORIZON — boss of Track 4

> **M5, 2026-11-13.** Armour takes a quarter: the fed Well (0.15 before) takes 0.25 of a shot's damage, where this
> page says none (`damageScale` in the data; GAME_DESIGN 7.6, "A boss's rule is a bonus, not a wall").
> Hint line (screens.md): `STARVE IT: KILL ITS FOOD`. The health is tuned with the novice bot, and then
> halved on the owner's play (2026-11-14: "way too much hp, needs like 0.5x"; GAME_DESIGN 7.6: what a
> person says after playing outranks the bots).

A well: a disc of true black 240 wu across, ringed by light, that pulls in everything near it — the
crowd, bullets, Shards, the players' own shots and the grid itself. It arrives on the last Drop of
Track 4 "Aurora" (138 BPM, E minor) and stays until it is dead; the music holds its Drop section for
the whole fight.

Kaleidos: hit the quiet part. Longwave: move with the music. The Choir: listen ahead. Event Horizon
teaches the last thing the crowd has to teach:

1. **What you leave alive feeds it.** The crowd streams into the Well along spiral lanes. Every body
   it eats is counted, and on the bar line it erupts with everything it ate.
2. **Starve it and it can be hurt.** A bar in which it eats less than its hunger ends in a gasp, and
   for the next bar it takes full damage. Fed, it barely feels you.
3. **The bar is the unit.** Cut the lanes first, then turn the guns on the Well: the fight is won a
   bar at a time, and the meter round the Well shows how this bar is going.

Data: `design/bosses/horizon.json` (parts, numbers, patterns — load it, do not retype it).
Picture: `design/previews/boss_horizon.png`. Tool: `design/tools/boss_horizon.py`.

---

## Anatomy (world units)

| Part | Shape | Size |
|---|---|---|
| **Horizon** | A black disc. Anything that crosses its edge is eaten | Radius 120. 3,200 health |
| **Photon ring** | One white line round the horizon; gold while the Well is starved | Radius 136, 2 px |
| **Accretion rings** | Three rings of dashes, each turning at its own rate and direction, in the Track's three enemy hues | Radii 190, 250, 320 |
| **Meal meter** | Twelve cells just outside the photon ring, filling clockwise in the danger colour as it eats; emptied on every bar line | Radius 152 |
| **Core** | A gold four-point star in the middle of the disc, shown only while starved: the weak point | 46 wu |
| **Lanes** | Spiral arms 1,300 wu long that wind 270° into the Well. The fight's whole crowd arrives along them | 2, 3, then 4 |

## Rules

1. **The pull.** Everything within 1,400 wu is drawn toward the Well, harder the nearer it is. At
   the photon ring the ship is pulled at 260 wu/s, half its own speed, so flying away always works
   and a Dash ignores the pull. Fodder, bullets, Shards and player shots are pulled too.
2. **Your shots bend.** Aim ahead of the curve; a shot fired past the Well swings round it.
3. **Eaten is gone.** A body that crosses the horizon gives no Shards and no score and fills one
   cell. Five uncollected Shards that fall in count as one body, so leaving Shards near the Well
   feeds it.
4. **The bar line decides.** Fed (the meal reached the phase's hunger): it erupts, 8 bullets and 4
   more for each body eaten, at most 96, in as many spiral arms as there are lanes, and its shield
   holds for the bar (damage × 0.15). Starved: it gasps, the rings thin, the rim turns gold, the
   core shows, and it takes full damage until the next bar line.
5. **Its own bullets fall back.** Slow ones curl round and are eaten; they do not fill the meter.
6. **Touching the horizon** costs a Shield cell and throws the ship 300 wu out.
7. **Everything is a function of song time**, as for every boss, except the meal: the meter and
   health are shared in co-op.

## Phases

| Phase | Health | Lanes | Hunger | Stream per bar | Adds |
|---|---|---|---|---|---|
| 1 **Accretion** | 100–66% | 2, opposite | 5 | 10 bodies | — One player can hold both lanes by circling |
| 2 **Tidal** | 66–33% | 3 | 7 | 18 | Pull × 1.25. On beat 3 of every bar a **tidal wave**: a ring leaves the Well at 600 wu/s and shoves ships and Shards 140 wu outward. It does no damage; it breaks your position |
| 3 **Collapse** | 33–0% | 4 | 9 | 26 | Pull × 1.5. The Well drifts toward the players at 90 wu/s. A fed bar line also fires the **jets**: two opposite beams along the Well's axis for two beats, shown as danger dashes for the bar before; the axis turns 45° every bar |

Bullets fly at 200 wu/s and live six seconds. The most alive at once is about 250 (three fed bars
running in phase 3).

## Numbers

| | Value |
|---|---|
| Health | 3,200 (× 1 + 0.5 per extra player); hunger + 2 per extra player |
| Damage scale | Starved × 1 · fed × 0.15 |
| Pull at the photon ring | Ship 260 · fodder 340 · enemy bullets 180 · player shots 220 · Shards 300 wu/s, falling off as (136 / distance)^1.2 |
| Camera | 0.8 for the fight, the Well kept in the frame |
| Crowd | Fodder only, and only along the lanes; the threat budget is off for the fight |
| Reward | A bonus Trio and 300 Shards per player ("everything it ate comes back"); the Track ends |

A starved bar at 138 BPM is 1.74 s. With the build a player has by the end of Track 4 (about 60
damage a second) one starved bar is worth about 100 health if every shot lands, so the fight is some
30 starved bars: with two bars in three starved, about 80 seconds. A player who only shoots the Well
does 15% of that and is buried by eruptions of 50 bullets and more. Tune hunger and the stream with
the bot once it can hold a lane.

## Sound

- **The drone.** A sub sine on the Track's root that sinks a semitone for every three cells on the
  meter and springs back on the bar line: the fuller the meal, the lower the room.
- **The count.** Each body eaten plays the next note of the Track's arp an octave down, bending
  down a fifth. The meal can be counted with the eyes shut.
- **Fed:** the impact and the gong on the bar line, louder the bigger the meal.
- **Starved:** the pads and the bass cut out for the first beat under a reversed cymbal. That
  silence is the signal to attack.
- The tidal wave rides in on the riser's last half-beat.

## Look

- **True black.** The disc is drawn last: no bloom, no afterglow, no grid inside it. It is the only
  black thing in the game, and against the brightest fight of the run it reads at once.
- The rings are dashes in the Track's hues, each ring at its own speed; the half turning toward the
  right of the screen is 1.8× brighter, so the disc looks tilted and alive.
- The lattice bends into the Well (a standing impulse of 400 at the horizon) and its lines stop at
  the edge. This is the fight the grid was built for.
- Everything on a lane leaves a trail in its own hue, so the lanes are spiral arms of light. A lane
  the players have cut goes dark from the cut inward.
- The meter is the danger colour on the priority layer; the cell being filled flashes white.
- Starved: rings at a third, rim and core gold, a gold four-arm flare from the core. **Gold is the
  weak point**, as everywhere.
- Player shots draw their bent path: the streak follows the curve.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat every light on screen leans toward one point and the grid
  pinches there over two beats; the disc opens black on beat 3 and the rings spin up; the name card
  holds for two bars.
- **Death:** it implodes. For one bar everything left alive, every bullet, every spark and the
  lattice spiral into a point while the music thins to the drone; one beat of black and silence;
  then the clamped white flash on the downbeat, three rings, 400 sparks in the Track's hues, the
  30 px distortion ring, 50 ms of hit-stop, and a fountain of 300 Shards.
