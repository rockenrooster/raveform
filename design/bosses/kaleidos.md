# KALEIDOS — boss of Track 1

A turning kaleidoscope wheel: six arms in three mirrored pairs around a shuttered core. It arrives on
the last Drop of Track 1 "First Light" (120 BPM) and stays until it is dead; the music holds its Drop
section for the whole fight.

It is the first boss, so it teaches the game's rhythm language and nothing else:

1. Attacks land on the beat, and you can hear them coming.
2. The thing that is not firing is the thing to hit.
3. A phrase has a shape: dodge, then punish.

Data: `design/bosses/kaleidos.json` (parts, numbers, patterns — load it, do not retype it).
Picture: `design/previews/boss_kaleidos.png`. Tool: `design/tools/boss_kaleidos.py`.

---

## Anatomy (world units, boss space)

| Part | Shape | Size |
|---|---|---|
| **Arms** | Six, at 0°, 60° … 300°. Each is four kite facets shrinking outward and a four-point emitter star at the tip | Facet centres at 104, 157, 202, 240; tip at 264. Whole wheel 270 radius |
| **Pairs** | Arms opposite each other are one pair: A (0°, 180°), B (60°, 240°), C (120°, 300°). A pair shares one health pool and shatters together | 400 health per pair |
| **Lens plates** | Two arcs per pair on a ring around the core, centred on that pair's arms. They stop shots. They shatter with their pair | Radius 66, 52° each |
| **Core** | Three counter-turning hexagons, six shutters (the iris), a seed of light, and a thin gold arc that is its health bar | Radii 44, 31, 18. 600 health |
| **Reflections** | Two faint copies of the arms behind the wheel, larger and turned: the kaleidoscope | 1.6× turned 30° at 0.22; 2.4× at 0.10 |

## Rules

1. **One pair rests each bar.** While three pairs live they rest in turn, A, B, C; the other pairs fire.
   The resting pair glows white-hot, its tips flare, and it takes **triple damage**. Firing pairs are
   armoured and take a third. So damage is always possible and timing triples it.
2. **Kill a pair, open the core.** A dead pair's lens plates are gone, so the core can be hit from that
   pair's direction — which is also the direction no fans come from any more. Cut a hole in the
   kaleidoscope, then shoot through it.
3. **The iris blinks on the downbeat.** For the whole first beat of every bar the shutters open and the
   core takes triple damage through any open sector. A deep gong marks it.
4. **Kill the core and it is over**, whatever arms remain. Careful players strip all three pairs first;
   bold ones go for the core as soon as one pair is down.
5. **Everything is a function of song time.** The wheel's angle, who rests, every bullet and every beam
   are computed from the musical clock, so every device in a co-op game sees the same fight without any
   of it being sent over the network. Only health is shared.

## Phases

The phase is the number of pairs alive. The wheel reverses direction at each change.

| Phase | Pairs | Wheel turns once every | Attack | Per bar |
|---|---|---|---|---|
| 1 **Bloom** | 3 | 8 bars | Each firing tip: a fan of 3 on every eighth note | Two pairs fire, one rests |
| 2 **Refraction** | 2 | 4 bars, reversed | Fans of 5 on every eighth. The pair that just rested opens its bar with a **beam** along each arm out to the arena wall, lasting one beat and sweeping with the wheel. The beam is telegraphed for the whole bar before as a thin dashed line | The two pairs alternate bars |
| 3 **Spiral** | 1 | 2 bars | Each tip: one bullet on every sixteenth, so the turning wheel draws a double spiral. The core adds a ring of 24 on beat 1 | Fires for three beats, rests on beat 4 |
| **Naked core** | 0 | 2 bars, reversed | The core fires a ring of 16 on every beat, alternate rings turned half a step. From the ninth bar: on every eighth | The iris rule still applies |

Bullets fly at 200 wu/s and live five seconds: slow, so the patterns are read, not reacted to. The most
alive at once is about 290 (phase 1), far inside the pool. Fans spread 14° between bullets.

Because the wheel turns, the gaps between arms are safe lanes that move: 22.5° a second in phase 1,
which is 157 wu/s at 400 wu out — a third of the ship's speed. Circling with the wheel is the first
thing the fight teaches.

## Numbers

| | Value |
|---|---|
| Pair health / core health | 400 / 600 (× 1 + 0.5 per extra player, as for every boss) |
| Damage scale | Firing pair × ⅓ · resting pair × 3 · lens plate 0 · core through an open sector × 1 · core on beat 1 × 3 |
| Contact | Arms and core both hurt; the core's radius is 46 |
| Position | Appears 500 wu from the nearest player toward the arena's centre; drifts at 30 wu/s to stay 450–650 wu from the nearest player |
| Other enemies during the fight | The director's budget drops to 30% and only fodder families spawn |
| Reward | A bonus Trio and 200 Shards per player; the Track ends |

With a starting loadout (8 damage a second) aimed at whichever pair is resting, a pair falls in about 17
seconds; expect the whole fight to take 60–90 seconds at the build a player has by then.

## Sound

- **Bells.** The boss owns a layer of glass bells (the P3 bell patch), one voice per pair: A plays the
  chord's root, B its third, C its fifth. A pair's bell sounds on every step it fires — the fans are an
  ostinato, and the resting pair is the hole in it. You hear who is safe to hit.
- A pair's death removes its voice for good and brings in a low growl an octave down (the
  elite-arrives patch).
- The iris opening is a deep gong on beat 1.
- Beams: a rising tone through the telegraph bar, a hard stab on the beat they fire.

## Look

- Pairs A, B, C take the run's three enemy hues with their partners, drawn as elites (white copy of
  the hull, glass, flares). The gradient runs along each arm, from a whitened hue at the core to the
  partner at the tip.
- **A boss has a second colour family.** Kaleidos's is amber-gold (OKLCH lightness 0.83, hue 76), the
  complement of the Ultraviolet palette: core rings, lens plates, tip stars and facet spines. Nothing
  else on screen has it, so the eye goes to the boss.
- The resting pair is white-hot: a white copy of its hulls at 2.4, glass at 0.16 and a two-arm flare
  along the arm at each tip.
- Bullets and beams are the danger colour, in the scene and on the priority layer, like every threat.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat six facets streak in along the mirror axes and lock together
  over one bar, inner to outer; the name card holds for two bars.
- **Pair death:** both arms shatter outward along their axis in mirror image; their lens plates crack
  and fall inward; the wheel flinches, stops for one beat and reverses.
- **Death:** iris wide open, the clamped white flash, every facet tumbling outward in six-fold
  symmetry, three rings, 300 sparks, the 30 px distortion ring, 50 ms of hit-stop — then the Shards.
