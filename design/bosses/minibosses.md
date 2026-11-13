# MINI-BOSSES — batches 1 and 2

Eight of the twelve in GAME_DESIGN 7.5. Batch 1: **Alpha Hound**, **The Hive**, **Battering Ram**
and **Carom King**. Batch 2: **Moth Queen**, **Brood Mother**, **Blink Star** and **Sentry Array**.
A mini-boss is one idea, taught in under a minute. It is its family grown large: whatever the
player already knows about Hounds is true of the Alpha, and what the Alpha adds is the fight.

Data: `design/bosses/minibosses.json` (rules common to all, then one entry each — load it, do not
retype it). Pictures: `design/previews/minibosses_1.png`, `minibosses_2.png`. Tool:
`design/tools/minibosses.py`.

---

## Rules for every mini-boss

- **When.** On the downbeat of the middle cycle's Drop from Track 2 on, in place of the mega-wave.
  A small name card holds for one bar.
- **Which.** The run's seed draws one whose family the run has already met, never the one met in
  the Track before. In batch 1, Track 2 draws from Alpha Hound and The Hive, Track 3 from Battering
  Ram and Carom King. In batch 2, Track 4 draws from Moth Queen and Brood Mother, Track 5 from
  Blink Star and Sentry Array.
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

# Batch 2

Batch 1 taught four verbs: dodge the charge, find the Queen, use the wall, hit it off the bounce.
Batch 2 is four ways of being surrounded, and each one makes its safe place out of the beat.

## Moth Queen (Moths)

*Her court is a clock that closes on you. She is open while she winds it again.*

| | |
|---|---|
| Body | `moth_5`, radius 70 |
| Moves | Holds 520 wu from the nearest player, drifting round them one turn every eight bars. She never closes in herself |
| Bar one, beat 1 | **The court.** Twelve Moths of the Track's tier warp in as a ring of radius 420 round the nearest player and begin to turn, a quarter turn a bar, stepping inward on every beat |
| Bars one to three | The ring spirals in from 420 to 140 wu. On every eighth note the Queen fires a **rosette**: five petals 72° apart, each rosette turned 12° from the last, 240 wu/s. A petal ends 700 wu from her |
| Bar four, beat 1 | **The court closes.** Every Moth of it still alive bursts into a ring of six bullets and is gone. The Queen stops firing and spreads her wings for the bar while the next court gathers round her |
| **× 2** | **Open** for the whole fourth bar: wings spread, core gold. In bars one to three she takes × 1, but her petals are thickest near her |
| Half health | Two courts of eight, the inner turning one way and the outer the other, closing a bar apart. Rosettes in pairs: ten petals an eighth note. Open only for the second half of the fourth bar |
| Death | The court loses its hold: every Moth alive flies straight out from her and falls as Shards |
| Sound | The court is the open hat, stepping round the stereo field with the ring's turn; each rosette is a plucked note of the Track's arpeggio; bar three is a riser; the burst is the clap on the downbeat; while she is open the bed drops to kick and pad |
| Cap | Twelve court Moths (sixteen from half health), counted against the enemy cap; 240 bullets |

What the player learns: every court Moth killed before the fourth bar is six bullets that never
fly, and the bar after the burst is the time to go to her.

## Brood Mother (Brood)

*Her children hold her shell shut. Keep the nest clean and she is yours.*

| | |
|---|---|
| Body | `brood_5`, radius 90. Health factor 1.1 |
| Moves | Drifts toward the players' centre at 40 wu/s. Touching her hurts |
| Beat 1, every bar | **The clutch.** Six eggs in a ring 170 wu round her. An egg is white, pulses on the beats and dies to one shot |
| Beat 3 | The eggs still whole **hatch** into broodlings (`brood_1`, radius 8, the health of a tier-1 dart), which chase the nearest player at 260 wu/s |
| The dome | A thread of light runs from every living broodling to her. While there is one thread her dome is shut and she takes a quarter of the damage |
| **× 2** | **Exposed.** With no broodling alive the dome breaks with a flash and she takes × 2 until the next egg hatches. Shooting the eggs before beat 3 keeps her exposed for the whole bar |
| Half health | Two clutches of five a bar, on beats 1 and 3, each hatching two beats after it is laid. Broodlings are 15% faster |
| Death | **Her last clutch:** sixteen gold eggs scatter across the frame and lie for two bars. Each one touched or shot breaks into ten Shards |
| Sound | The laying is a low tom on beat 1; the hatching is a chirp for each egg, arpeggiated up the chord on beat 3. The threads are a pad with one voice for each living broodling, so **the dome can be heard**: when the pad falls silent she is open, and a bell marks the break |
| Cap | 36 broodlings alive, counted against the enemy cap; a clutch that would pass it lays fewer |

She is never immune: a build that ignores the nest still kills her, four times slower and with
thirty-six broodlings on its heels.

## Blink Star (Blinks)

*It draws a star round you, one edge a beat. Then the star fires. It ends where it began.*

| | |
|---|---|
| Body | `blink_5`, radius 60. Health factor 0.9 |
| Moves | It does not travel. It blinks from point to point of a five-point star of radius 420 wu, centred where the nearest player stood on the first beat |
| Bar one, beats 1 to 4, and bar two, beat 1 | It appears at the first point, then **blinks to the next point in star order** (every second point) on each beat. The fifth hop brings it back to where it began. Each hop leaves its edge behind as a dashed danger line |
| Bar two, beat 2 | **The star fires.** All five edges are lethal beams 16 wu wide for one beat. Off the lines is safe: the pentagon in the middle (about 260 wu across), the corners of the five points, or outside the star |
| Bar two, beats 3 and 4 | It rests at the first point, dazed. Then the next star, centred on the player again |
| **× 2** | **Dazed** for the two beats after the star fires, at the point where it began. While it draws it can be hit at each point for the beat it stands there, × 1 |
| Half health | Every point it leaves keeps a white **decoy**; when the star fires each decoy also throws a fan of three bullets at the nearest player. The star is drawn a quarter larger |
| Death | The last star stays lit in gold for a bar, harmless, and its edges run to the players as Shards |
| Sound | Each hop is the next note of a five-note pentatonic run on the Track's root, so the star is a rising phrase the player learns by ear; the firing is all five notes at once as a stab with a zap; dazed is the stab's tail with the bed low-passed |
| Cap | One body, five beams, five decoys, fifteen bullets |

The first point is marked from the first beat. A player who notes it can stand beside it, outside
the lines, and be waiting when the Star comes home.

## Sentry Array (Sentries)

*You are inside a clock of guns, and the hand is the only safe place.*

| | |
|---|---|
| Body | Eight `sentry_5`, radius 44, sharing the Array's health evenly |
| Placed | They warp in evenly on a ring of radius 340 wu round the players' centre and stand. **Domes of light join each to its neighbours into a fence:** ships cannot cross it and shots stop on it. The crowd warps in inside |
| The hand | A white line from the centre that points at one turret and moves on by 45° on every beat: one turn every two bars |
| Every beat | Every turret **but the one under the hand** fires a fan of five bullets inward across its own share of the ring, 220 wu/s. Together they are a closing ring with one notch. The bullets end at the centre |
| The notches | Each ring's notch is one step round from the last. The player rides them round the clock |
| A turret dies | The others step round on the next bar line to stand evenly again. The hand keeps its time, so each turret is silent for longer |
| **× 2** | **The turret under the hand has no dome.** It is the only one that can be hurt, and it takes × 2 |
| Half health | With four left the clock runs backwards on every second bar: the hand flashes white a beat ahead and turns the other way. Volleys come on the eighth notes at half the fan (three bullets, then two) |
| Death | The last dome turns inside out: every bullet in flight turns gold and falls to the players as Shards, and the fence falls outward |
| Sound | The hand is a woodblock on every beat, panned to where it points and one step up the scale for each step round — an octave a turn. The volleys are closed hats. A turret's death is a tom and a gap in the scale. The turn back is a reverse cymbal into its bar line |
| Cap | Eight turrets, 200 bullets |

It is not The Choir. The Choir's eight stand far apart and answer each other across the arena;
the Array is small enough to see whole, it has one rule, and the rule is a clock.

---

## Still to design (batch 3)

Twin Tether, Weaver Prime, Deep Well, Mirror Mimic.
