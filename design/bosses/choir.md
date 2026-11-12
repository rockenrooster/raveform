# THE CHOIR — boss of Track 3

Eight hooded Voices standing on a ring around the players, each one a note of the scale and a colour
of its own, joined by strings of light. They do not chase: they surround, and they sing at you. The
Choir arrives on the last Drop of Track 3 "Reef" (132 BPM, D dorian) and stays until the last Voice
is silent; the music holds its Drop section for the whole fight.

Kaleidos taught that the quiet part is the part to hit. Longwave taught moving with the music. The
Choir teaches listening ahead:

1. **Whoever is singing can be hurt.** A Voice opens its mouth to fire, and only an open mouth takes
   damage. You attack into the attack.
2. **The song tells you who sings next.** A call is always answered: first by the Voice opposite,
   later by each Voice round the ring in turn. Turn to the answer before it comes.
3. **You edit the song.** Every Voice you kill takes its note and its beat out of the music for good.
   Which ones you remove decides where the rests and the gaps are.

Data: `design/bosses/choir.json` (parts, numbers, patterns — load it, do not retype it).
Picture: `design/previews/boss_choir.png`. Tool: `design/tools/boss_choir.py`.

---

## Anatomy (world units)

| Part | Shape | Size |
|---|---|---|
| **Voice** (×8) | A pointed hood seen from above, its point outward, three ribs inside it, a ten-sided face with a core at its centre, and two lips at the front that flare into a horn when it sings | Radius 66. 260 health each |
| **Halo** | An arc behind each hood, pearl white, that brightens with the Voice's own note | Radius 99 |
| **The Round** | The ring the Voices stand on, Voice k at k × 45° | Radius 380 (300 in the Fugue) |
| **Neighbour strings** | A thin string between each pair of neighbouring live Voices, shaded from one Voice's hue to the other's: the octagon | 1.6 px, a 5 wu shimmer |
| **Call string** | A standing wave across the Round between the Voice that is singing and the Voice that will answer it: three antinodes, drawn as its two envelopes. Light only: it never hurts | 2.6 px, 44 wu swing, one beat to fade |

## Rules

1. **The ring is the scale.** Going clockwise the Voices own D, E, F, G, A, B, C and the D above, so
   opposite Voices are always a fifth apart. A Voice sings its own note every time it fires.
2. **Only a singing Voice can be hurt.** Its mouth is open from the step it fires until one beat
   later. Shots ring off a closed hood.
3. **Every call gets its answer.** The pattern of who answers, and when, is the phase.
4. **A dead Voice is a rest.** Its beat stays in the bar and nothing happens on it: no shots, no note.
   A dead caller is skipped; a dead answerer leaves the call unanswered.
5. **The Round turns** once every 16 bars and follows the players' centre at 60 wu/s. It stops for
   one beat when a Voice dies and then turns the other way.
6. **The cage** (from the second phase): the neighbour strings show danger dashes for one bar, then
   burn as beams for two beats from the next downbeat. Be inside or outside, not across. A string
   needs both its Voices, so every dead Voice opens two gaps.
7. **Everything is a function of song time**, as for every boss. Only health, and the times at which
   Voices died, are shared in co-op.

## Phases

The phase is how many Voices are left.

| Phase | Voices | Call | Answer | Also |
|---|---|---|---|---|
| 1 **Unison** | 8–7 | The caller, beats 1 and 3: a fan of five at the nearest player, 11° apart | The Voice opposite, beats 2 and 4: the same fan | The caller moves one Voice clockwise every bar. Fire comes from in front, then from behind: turn with the song |
| 2 **Canon** | 6–5 | The caller, beat 1: the fan of five | Every other live Voice clockwise from the caller, each an eighth after the one before: a fan of three, 14° apart | The cage every fourth bar. The answer runs round the ring like a wave: move with it |
| 3 **Fugue** | 4–3 | Two callers as far apart as the live Voices allow: one bullet every eighth, a stream that sweeps 80° across the nearest player over the bar, the two sweeping opposite ways | The other live Voices play the same streams back, mirrored, in the next bar | The Round shrinks to 300 over two bars. The cage every second bar. The two sweeps cross once a bar, and the crossing is the gap |
| 4 **Solo** | 2–1 | Each live Voice, beats 2 and 4: a ring of 16, alternate rings turned half a step | — | The ring is gone: the last Voices circle the nearest player at 280 wu, a lap every four bars. Every fourth bar the **held note**: a dashed line for a bar, then a beam from the open mouth that sweeps 120° over beats 1 and 2. The last Voice takes ×1.5 |

Bullets fly at 220 wu/s and live five seconds. The most alive at once is about 180 (Solo with two
Voices); the other phases stay near 55.

## Numbers

| | Value |
|---|---|
| Voice health | 260 each, 2,080 in all (× 1 + 0.5 per extra player, as for every boss) |
| Damage scale | Singing × 1 · silent 0 · the last Voice × 1.5 |
| Contact | A hood hurts; strings do not, except while the cage burns |
| Camera | Pulls back to 0.75 for the fight so the whole Round is in the frame from inside it, eased over the entrance bar |
| Other enemies during the fight | The crowd drops to half and only fodder families spawn, outside the Round |
| Reward | A bonus Trio and 220 Shards per player; the Track ends |

In Unison one of two opposite Voices is always open, so a player who turns with the song is always
doing damage and one who camps a single Voice does half. With the build a player has by the end of
Track 3 (about 45 damage a second on target) expect 80–110 seconds. Tune the Voice health with the
bot once it can turn to the answer.

## Sound

- **The Voices.** The pad patch used as a voice (attack 20 ms, release one beat, reverb send 0.5),
  one note per sung step, each Voice its own note. In Unison every bar is root, fifth, root, fifth,
  and the root climbs one degree of the scale each bar. In the Canon the answer runs up the scale in
  eighths. The fight is a melody the players can hear thinning as they win.
- **A death** is that Voice's note falling a fourth over one beat into the reverb; it is never heard
  again.
- The cage ticks on every beat of its telegraph bar (the telegraph tick) and lands with the impact.
- The held note is the last Voice's note held for the whole telegraph bar, rising an octave as the
  beam fires.

## Look

- **Every colour.** Eight hues 45° apart round the OKLCH wheel (lightness 0.80, chroma 96% of the
  gamut), starting at the Track's first enemy hue; a hue within 25° of the danger colour moves to the
  nearer edge of that band, so no Voice can be mistaken for a bullet. No other boss wears more than
  two colour families: this one is the rainbow.
- **Pearl** (OKLCH lightness 0.93, hue 300, 22% chroma) for halos, faces and strings at rest, so the
  hues stay the Voices' own.
- A singing Voice is white-hot: a white copy of its hood at 2.7, glass at 0.16, lips flared, a
  two-arm flare across the mouth, three arcs of its own hue leaving the mouth, and its core gold.
  **Gold is the weak point**, as everywhere in the game.
- The Voice that will answer next draws breath: lips a third open, a small four-arm glint on its
  face, the far end of the call string in its hue. This is the telegraph for rule 2.
- The call string is the picture of the fight: singer's hue at one end, white in the middle,
  answerer's hue at the other, its nodes glinting. Keep it on the lowest tier (five polylines).
- The lattice answers the song: an impulse of 120 under each Voice as it sings, and a line of
  impulses along the call string.
- Bullets and the burning cage are the danger colour on the priority layer, like every threat.
- The boss is never dimmed by the light budget.
- **Entrance:** on the Drop's downbeat the Voices warp in clockwise, one every eighth, each singing
  its note as it lands: a rising scale. On bar 2 all eight sing together and the name card holds for
  two bars. Firing starts on bar 3.
- **Voice death:** the hood splits along its axis and falls outward; its two strings snap back to
  the neighbours with a white flash.
- **Death:** the eight hoods return as outlines in their own hues for one bar and sing the last
  chord (D, A, D) with every string white; then they shatter outward clockwise, one every eighth,
  each a large kill, the last as a boss break — three rings, 300 sparks, the 30 px distortion ring,
  50 ms of hit-stop — then the Shards.
