# MINI-BOSSES — batch 1

Four of the twelve in GAME_DESIGN 7.5: **Alpha Hound**, **The Hive**, **Battering Ram** and
**Carom King**. A mini-boss is one idea, taught in under a minute. It is its family grown large:
whatever the player already knows about Hounds is true of the Alpha, and what the Alpha adds is
the fight.

Data: `design/bosses/minibosses.json` (rules common to all, then one entry each — load it, do not
retype it). Picture: `design/previews/minibosses_1.png`. Tool: `design/tools/minibosses.py`.

---

## Rules for every mini-boss

- **When.** On the downbeat of the middle cycle's Drop from Track 2 on, in place of the mega-wave.
  A small name card holds for one bar.
- **Which.** The run's seed draws one whose family the run has already met, never the one met in
  the Track before. In batch 1, Track 2 draws from Alpha Hound and The Hive, Track 3 from Battering
  Ram and Carom King.
- **How long.** The Drop holds while it lives, for at most 8 bars past its 16. Then it warps out
  and leaves nothing.
- **Health** by Track: 600 · 1,100 · 1,700 · 2,400 · 3,200 for Tracks 2 to 6, × 1.25 for each Track
  of the loop, × the mini-boss's own factor, × (1 + 0.5 per extra player). That is 25 to 35 seconds
  of fire with the build of that Track's middle cycle. Tune with the bot.
- **Crowd.** Three quarters of the Drop's crowd, fodder only, on top of what the mini-boss calls.
- **Reward.** One Boost orb and 60 Shards for each player, and 25 times the score of a tier-5 body
  of its family.
- **Body.** The family's tier-5 glyph enlarged, one more ring of ornament, the white equipment
  marks. Off screen it gets the boss's double chevron. Its health is a thin bar under the section
  bar, half the height of a boss's.
- **Everything lands on the grid**, and every one has a moment when it takes × 2.

### The Pen

Two of the four need walls, and the arena's own are too far apart. On the downbeat a square of
white light walls, 1,400 wu on a side, rises round the players' centre in one beat. Walls stop
ships and bodies without hurting; shots pass through; fodder caught outside dissolves; the crowd
warps in inside. It falls when the fight ends. The Pen is one of M5's structures.

---

## Alpha Hound (Hounds)

*It does not chase. It circles and sends the pack.*

| | |
|---|---|
| Body | `hound_5`, radius 64 |
| Moves | Circles the nearest player at 420 wu, 170 wu/s |
| Beat 1, every bar | **The howl.** Six Hounds of the Track's tier warp in as a chevron 500 wu from the player, with a danger line from the chevron's tip through where the player stood at the howl |
| Beat 3 | The pack charges along the line at 520 wu/s for two beats, through and past. Afterwards they are ordinary Hounds |
| Every fourth bar | The Alpha lunges itself: danger line on beats 3 and 4, then 900 wu/s for one beat |
| **× 2** | **Winded** for the two beats after its lunge |
| Half health | Two packs of five a bar, from opposite sides. The lunge comes every third bar |
| Death | The pack scatters: every Hound alive turns tail for two bars and drops double Shards |
| Sound | The howl is a saw note gliding up a fifth; the charge is a snare roll into beat 3; the lunge is the sub drop |
| Cap | 30 called Hounds alive |

## The Hive (Shoal)

*A cloud that becomes shapes. Only the Queen has health.*

| | |
|---|---|
| Body | 120 darts and one gold Queen (`shoal_5`, radius 18). Health factor 0.8 |
| Armour | A dart dies to one shot and stops it. The Queen hatches 12 a bar, up to 160 |
| Shapes | A new one every two bars, on the bar line, after a beat of every dart flashing white: Ring, Spear, Wall, and round again |
| Ring | Radius 300 round the player, closing to 120 over the two bars; one gap of 60° that turns half a turn a bar. The Queen rides the ring opposite the gap |
| Spear | A tight column through the player's position on beats 1 and 3 at 700 wu/s, danger line a beat ahead. The Queen is its last dart |
| Wall | 900 wu long, crossing the frame at 160 wu/s, two gaps of 90 wu. The Queen flies behind its middle |
| **Open** | Ring: she is in plain view on the far side from the gap, so every shot at her is time not spent leaving. Spear: from behind, after it passes. Wall: from behind, once through a gap |
| Half health | Two hives of 60 taking different shapes; one carries the Queen, the other a white decoy |
| Death | Every dart falls at once as Shards: the biggest single drop of the Track |
| Sound | The cloud is the shaker; a change of shape is a reverse cymbal into the bar line; closed hats tighten with the Ring, two toms for the Spear, a noise sweep for the Wall |
| Cap | 160 darts, counted against the enemy cap: the crowd gives up that many places |

## Battering Ram (Rams) — in the Pen

*A bullfight. Make it hit the wall.*

| | |
|---|---|
| Body | `ram_5`, radius 80. Health factor 1.2. Its front 120° sheds every shot; sides and back take × 1 |
| Bar one | Beats 1 and 2 it turns to face the nearest player. Beat 3 it **locks**: a danger line from its horns through the player to the wall. Beat 4 it holds |
| Bar two, beat 1 | It charges along the line at 1,400 wu/s to where the player stood at the lock, and skids to a stop |
| The wall | If a wall is within 300 wu behind that spot it cannot stop. The wall rings — 16 bullets fan from the point of impact — and the Ram is stunned |
| **× 2** | **Stunned** for three beats after hitting a wall: horns down, from every side |
| Half health | After a skid it locks again in one beat and charges a second time. The stun is two beats. Its charge leaves a wake in the danger colour for a bar |
| Death | It charges once more with nobody aiming it, breaks through the Pen, and the walls fall outward as Shards |
| Sound | The lock is a tom roll rising through beats 3 and 4; the charge is the sub drop and a driven kick; the wall is the gong; the bed is low-passed while it is stunned |

## Carom King (Caroms) — in the Pen

*It plays the Pen like an instrument, and every quarter of its health is another player in the band.*

| | |
|---|---|
| Body | `carom_5`, radius 80, then 68, 56 and 44 |
| Moves | Straight lines at about 300 wu/s, reflecting off the Pen. Each leg's speed is trimmed so that **every bounce lands on a beat**; each bounce is 8% faster, up to × 1.6 |
| Every bounce | Five bullets from the point of impact, fanned 40° either side of its new heading |
| 75%, 50%, 25% | It sheds its outer square as a **Prince**: radius 40, 12% of the King's full health, 1.3 times its speed, bouncing on the half beats. The King is a size smaller and 15% faster. A dead Prince breaks into two ordinary Caroms |
| **× 2** | Its core is gold for one beat after each bounce: hit it as it comes off the wall |
| Death | The last square bursts on a beat and every Prince alive loses its speed and drifts |
| Sound | Each wall is a note of the Track's chord — left the root, top the third, right the fifth, bottom the seventh — played as a rim shot, so its bounces are an arpeggio that speeds up. Princes play the same notes an octave up on the half beats |
| Cap | One King, three Princes, six Caroms, 60 bullets |

---

## Still to design (batches 2 and 3)

Moth Queen, Brood Mother, Blink Star, Sentry Array (batch 2); Twin Tether, Weaver Prime, Deep Well,
Mirror Mimic (batch 3).
