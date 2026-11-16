# JAM — a mode

> **A plan (2026-11-15), not built.** Shared rules: `README.md` (the Stage, versus). Numbers: `modes.json` `jam`.
> Screens: `screens.md`. Sound: `sound.md`.

A play room. Nothing can hurt the ship. The band's layers lie on the floor as pads: fly over one and it
joins in on the next bar, and its light comes on in the room. Every upgrade in the game is on the
shelf, free. In the middle of the floor is a big button, and it does what a big button should: it
drops the beat. No score, no clock, no end.

What it is for:

1. The youngest player. The owner's bar for the whole game is a five-year-old; this is the mode a
   five-year-old opens first and a three-year-old can play.
2. The curious player. "Fractal ... I still don't know what it does" (the owner, 2026-11-14): here
   any upgrade can be picked up and tried on a crowd that does not fight back.
3. The first pillar as a toy: each layer of sound is a light in the room, switched on by hand.

Home Track: Toybox (`README.md` 11), written so that no combination of its layers can clash. The room
can change to any of the twelve from inside (the Track pads).

---

## The room

The Stage (`README.md` 4), called here the room. Places are in wu from its centre, y down.

| Pad | Place | Size | How it is used | What it does |
|---|---|---|---|---|
| **The mixer**: eight pads | A row under the rig's eight fixtures: y -340, x from -665 every 190 | Discs of 44 | **Fly over it**: it changes on the next bar line | Switches one layer of the band on or off: `KICK`, `HATS`, `BASS`, `CLAP`, `OPEN HAT`, `SHAKER`, `CHORDS`, `STABS`. Only KICK is on as the room opens |
| **DROP** | (0, 200) | A ring of 70 | Stay on it two beats | A Build of four bars, then a Drop of eight, then the Groove again. It rests eight bars before it can be pressed again |
| **TOYS** | (-560, 60) | A ring of 56 | Stay two beats | Opens the next shelf of six upgrades round the ship |
| **BOOSTS** | (560, 60) | A ring of 56 | Stay two beats | Opens the next shelf of Boosts |
| **EMPTY** | (-760, 300) | A ring of 36 | Stay two beats | Everything the ship holds goes back in the box |
| **SOUND** | (760, 300) | A ring of 36 | Stay two beats | The next of the three sounds (Club, Arcade, Velvet), on the next bar |
| **The Tracks**: twelve pads | A row along the bottom: y 350, x from -605 every 110 | Discs of 30 | Stay two beats | The band changes to that Track through its Break, as the set changes Track: tempo, key, palette |

"Stay two beats" is the Trio's hold (`design/screens.md`, "The Trio"): the pad's ring fills while the
ship is inside it and empties when it leaves. A choice is never an accident. The mixer's pads alone
answer to a fly-over, because switching a layer by mistake costs nothing and sounds nice.

## Rules

1. **Nothing hurts.** No body, bullet, beam or blast takes a cell here. A body that touches the ship
   is pushed off it with a few sparks. The halo shows four cells and never changes. Nova and Dash
   work as always (a Nova is a toy too) and come back every bar.
2. **The mixer is the band's bed.** Each pad is one of the conductor's bed layers (`conductor.json`),
   held on or off by the room instead of by the run's intensity. A pad flown over changes on the next
   bar line, never mid-bar (the sound cards' rule, `design/screens.md` "Settings"). With every pad off
   the room is silent but for the ship's own lead: that is allowed, and the first sound a child makes
   in it is then their own.
3. **Each layer is a light.** Switching a layer on lights its part of the room (the table under
   "Look"); switching it off puts that light out. The room's light show is exactly as full as its music.
4. **The dancers.** The Track's fodder (variant A, tiers I and II) comes to the music: twenty bodies
   for each layer that is on, up to 160. They wander and flock as their families do and hurt nobody.
   Shot, they pop as they always do, with their Shards. With five layers on, an elite walks in every
   eight bars: a big pop.
5. **The shelves.** TOYS has seven shelves of six: the thirty Mods that mean something here, then the
   twelve Riffs (`modes.json` `jam.shelves`; left out: Extra Cell, Regen, Lucky Break and the Halo
   Boost, which have nothing to do in this room). BOOSTS has three shelves: fifteen Boosts. Each visit
   to the pad opens the next shelf; the pad's label names it (`WEAPONS 1 OF 3`, `RIFFS 2 OF 2`).
   - A shelf is the Trio's ring (170 wu round where the ship stood, six orbs evenly round it, icon,
     name, words and pips as on a Trio's orb).
   - **Stay in an orb one beat** to take it. The orb is not used up: every further beat inside adds
     a stack or a level, up to its cap.
   - The shelf stays while the ship is within 300 wu, and closes two bars after it leaves.
   - Four Riffs at once; a fifth sends the oldest back to the box. A Fusion whose two halves are full
     appears as the gold orb above the ring, as in a Trio.
   - A Boost from the shelf lasts eight bars, three at once as always.
6. **DROP.** The room's own phrase: on the next bar line a Build (riser, snare roll, the lights
   draining to white, the dancers gathering, the last bar dark and slow), then the Drop: the impact,
   every layer of the band on for its eight bars whatever the mixer says, the dancers at 1.6 of their
   number, Surge for the first four bars. Then the Groove and the mixer as it was left. The pad's own
   ring is the clock: it fills over the eight bars it rests.
7. **Shard levels give presents**: at each level a Boost orb drops beside the ship (and a Nova).
   There are no Trios.
8. **The Tracks.** A Track pad changes the room's Track through the band's Track-change Break (8 bars:
   pads only, the dancers dissolving into Shards, the palette cross-fading), and the mixer keeps its
   settings into the new Track.
9. **Leaving** is the pause's LEAVE, or Back twice. The room has no result: it goes home.
10. **Nothing is kept.** No score is shown or stored. The one number in the HUD is `POPS`, because
    counting is fun.

## Numbers

| | |
|---|---|
| Dancers | 20 a layer on, at most 160; x 1.6 in a Drop; tiers I and II, variant A |
| Elite | One every 8 bars with 5 or more layers on |
| A shelf | 6 orbs on a ring of 170 wu; one beat to take; stays within 300 wu; closes after 2 bars |
| Boosts from the shelf | 8 bars |
| DROP | Build 4, Drop 8; rests 8 bars |
| Mirror ball | 24 glints, one turn in 4 bars |

## Look

The room is the Stage with its rig of eight fixtures along the top wall. Each mixer pad sits under its
own fixture: the pad is the switch, the fixture is its lamp.

| Layer | Its light in the room |
|---|---|
| `KICK` | On every kick a ring runs through the lattice from the room's centre (one impulse of 140) and the walls swell |
| `HATS` | Star dust along the top wall: sparkles of 1.6 to 3 px twinkling on the hat's steps |
| `BASS` | The background's centre glow breathes with the bass's envelope (up to 40% over its rest), and a low pool of the grid's colour lies at each end of the floor |
| `CLAP` | The walls flash on two and four, and two fixtures each throw a short beam at the floor |
| `OPEN HAT` | Two long beams from the rig sweep the floor, crossing on the off-beats |
| `SHAKER` | The lattice shimmers: its fine shimmer of GAME_DESIGN 11.1, doubled |
| `CHORDS` | **The mirror ball**: 24 glints (pools of 26 wu at 0.22, white to the accent) circling the floor, one turn in four bars; on each change of chord their colour steps round the palette |
| `STABS` | A fan of five beams from the middle of the bottom wall, flashing on every stab (the title's fan, in the room) |

- **A beam** of the rig is a laser as look.md 5 has it: one 2 px line at 0.6 and a glow of 6 wu at
  0.2, in the palette's accent running to white at the fixture. It is scene light only (never on the
  priority layer), it is never the danger colour and never a player's: it is the room's. It hurts
  nobody and stops nothing.
- **A pad**: a disc drawn by its edge (1.5 px), its word inside (cap 14 wu) and, for a mixer pad, its
  layer's live signal as a short line across it (the settings' sound cards' signature lines, small).
  Off: white at 0.5. Waiting for the bar line: blinking on the beat with `NEXT BAR` under it. On: its
  edge and word in the mode's colours at 1.6, a pool under it, and a thin line of light up to its
  fixture. A hold pad's ring fills in the player's colour as a Trio's does.
- **DROP** is the largest thing on the floor: a double ring, its word at cap 30 wu, four beat dots
  under it as PLAY has on the title. Resting: its outer ring is the clock, filling over eight bars,
  the word at 0.4. Ready: it swells on every beat.
- **The Track pads** are each in their Track's accent with its number inside; the one playing has a
  white ring and its name over it.
- **The dancers** are the Track's fodder as always. They swell on the kick as every enemy does: with
  the kick switched off they stand still, which a child will find out.
- **EMPTY**: everything the ship holds flies out of it as the load-in flies in, backwards, in a
  beat, to the TOYS and BOOSTS pads.
- Reduce flashing: the CLAP's wall flash and the STABS' fan at half, the mirror ball as it is.

The light budget holds (look.md 3): the room's lights are few and soft, the dancers at most 160.

## Sound

- The band, as the mixer has it. Each pad flown over: the interface tick at once, and the layer comes
  in or goes out on the bar line under a one-beat sweep (`sound.md`).
- DROP is the band's own Build and Drop; held: two rising notes as a Trio's hold, then the confirm
  chord, then the riser.
- A toy taken: the "upgrade taken" chord, as in the set; each further stack a bell a scale step up.
- EMPTY: the back chord and a closing sweep.
- SOUND: the settings' rule: the whole band changes together on the bar line.
- With every layer off, the room is quiet enough to hear what each upgrade adds to the lead: that is
  the other use of the mixer.

## HUD

The score block is gone. Top left: `POPS 1,204` in the mode's colours. Top centre: the Track and tempo
and the phrase bar, which in the Groove shows one long segment and, after DROP, the Build and the Drop
running. The Nova stock, the pause ring, the Riff slots and Boost cards as in the set. Hints on a first
visit (`screens.md`): `FLY OVER A PAD`, then `STAY ON THE BIG ONE`, then at the TOYS pad `STAY ON A TOY
TO TAKE IT`.

## Co-op

Two to four players in the room: it is a band practice. The pads are the room's: anyone's fly-over or
hold counts, and every device hears and sees the same room on the same bar. Toys, shelves and Boosts
are each player's own. The dancers scale by the players (twenty more a layer for each extra player, to
the same 160 at most). The lobby's line holds here more than anywhere: each phone is an instrument.

## Versus: POP-OFF

A match, not a room: `README.md` 8, V1 to V9. Thirty-two bars, about a minute.

| Bars | Part | What happens |
|---|---|---|
| 1-2 | The card | `POP-OFF` and `FIVE PICKS, THEN POP THE MOST`; every ship empty; every layer of the band on; the pads are not there |
| 3-12 | **The draft** | Five Trios in a row for each player, two bars each, dealt by the game's own rules (GAME_DESIGN 6.1: the first offers Split and Double-Time), held to be taken as always, the nearest orb at the end. Five picks make a build; no two players are dealt the same |
| 13-16 | The Build | The dancers gather |
| 17-32 | **The Drop** | Sixteen bars: the floor is kept at 200 bodies, refilled every bar; an elite every four bars. Surge for the first eight. **A pop is one; an elite is five** |
| The last downbeat | The count | Each colour's pops tick up side by side. Then the podium |

Nothing hurts here either, so there is no knock-out and no underdog. The shove (V3) works. Boost orbs
fall one every four bars of the Drop, shared (V4). A rematch deals new Trios: it is the draft that
makes each match its own.

## Caps and the wire

| State | Size |
|---|---|
| The mixer | A bit a layer, and the mask waiting for the bar line |
| Pads | 25 fixed places; a hold's progress a player |
| Shelves | A shelf's number a player for each of the two pads |
| The DROP | Its bar line, the bar it may be pressed again |

| On the wire | Who decides | How |
|---|---|---|
| The mixer, DROP, a Track change, SOUND | The host, on what the players' devices ask | A device that flies over or holds a pad asks (reliable); the host puts the change on a bar line at least half a bar away and tells all, as it does the bed's level (`MsgBed`) |
| Toys and Boosts taken, EMPTY | The player's own device | That player's events, as a Trio's take is |
| The dancers | The host's director | The set's spawns |
| POP-OFF's pops | The claims' player; the host counts | The set's claims; the counts in the players' stats |

The densest moment is a Drop in the room with four players and every toy: at most 256 bodies and the
shots of four maxed builds. It is lighter than FULL BLAST's wave 4 and needs no benchmark of its own
beyond one 60-second check (`docs/MODES_PLAN.md`).

## What to check

- Every one of the 256 mixer settings on Toybox, rendered a bar each by the headless band: no bar
  clips, none is louder than the Track's own Drop.
- A bot that wanders the room for ten minutes crossing pads at random: no pool refuses anything, the
  band's callback stays in its budget, nothing is left sounding after EMPTY.
- Every toy can be taken to its cap and emptied again; a fifth Riff sends the oldest home.
- The room on a phone at arm's length: every pad's word can be read (cap 14 wu is about 16 px at the
  Stage's zoom on the Moto: if it cannot, the words grow before the pads shrink).
- Pictures: the room with one layer on, with four, with all eight (the mirror ball, the fan, the
  beams), a shelf open, DROP ready and resting, POP-OFF's draft and its Drop.
