# RAVEFORM — Modes: the screens

> **A plan (2026-11-15), not built.** This adds to `design/screens.md` and keeps every rule on that page: the
> music never stops, one control is the obvious one, a control is a pill with its pressed state, no more
> than three choices in a main row, controls never touch (24 px), nothing cuts, lengths are in beats, a
> tap is answered at once. Sizes are reference pixels at 2400 x 1080, scaling with the screen's height;
> the 132 px side margins stay clear. When a part of this is built it moves into `design/screens.md`.

What is new: the title's **mode pill**, the **MODES** screen, the lobby's mode line, the HUD's **goal
line** and **run bar**, the modes' **name cards**, **the count**, the **results**, the versus HUD and
**the podium**, nine first-run hints, and the transitions between all of them.

---

## The title: the mode pill

PLAY stays what it is: one tap, and the player is in a run (PLAN 1: playing within five seconds). What
it starts is now written above it.

| Element | Place | Look and behaviour |
|---|---|---|
| **Mode pill** | Centre (1200, 600), 480 x 84 | A pill as every pill (white outline; hover and pressed as built). Inside: the mode's emblem at its left end (56 px tall, moving on the beat), the mode's name (cap 30), a small `>` at its right end. Tapping it opens MODES |
| Its caption | 28 px under it, cap 12, white at 0.5 | The mode's caption and, for a mode that takes a Track, the Track's name: `DRAW RINGS OF LIGHT. NO AIMING   MIRRORBALL` |
| **PLAY** | As built: (1200, 800), 640 x 150 | Starts the mode on the pill, on its remembered Track, by the drop-in. On a first launch the pill reads `THE SET` |
| Best | As built, bottom left | The best of the mode on the pill: THE SET's line as built; ONE TRACK `BEST 412,300  TRACK 04`; HEADLINERS `BEST 10 ACTS  9:42`; the timed modes `BEST 1,204,880  MIRRORBALL`; JAM nothing |

The pill leaves 55 px over PLAY and 115 px under the rule's tagline; the main row is still three (CO-OP,
PLAY, SETTINGS). In the title's own transitions the pill and its caption go with CO-OP and SETTINGS:
they arrive a quarter beat after PLAY in the intro and home, sink with them in the drop-in's wait, and
leave on the quiet title's first beat. The demo behind the title is the set's run, whatever the pill says.

---

## MODES

One screen: the mode, its Track, PLAY. From the title's pill; in a room, from the lobby (below). The
demo run goes on behind at a fifth of its light, as behind SETTINGS. Picture:
`design/previews/modes_screen.png` (TRACERS at the centre, the bonus Track Mirrorball chosen and its
palette on the screen), drawn by `design/tools/modes.py` from the numbers below.

```
                                   MODES
  ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~   the rule, y 150
        +-----------+      +===================+      +-----------+
        |  emblem   |      |      emblem       |      |  emblem   |
        |           |      |                   |      |           |
        | FULL BLAST|      |     TRACERS       |      | SONGLINE  |
        +-----------+      | DRAW RINGS OF ... |      +-----------+
                           | 1-4 PLAYERS  ...  |
                           | BEST 412,300      |
                           +===================+
                                 . . . o . .
                     TRACK 11   MIRRORBALL   122 BPM
              (1)(2)(3)(4)(5)(6)(7)(8)(9)(10)((11))(12)
   [ BACK ]                    [   PLAY   ]
```

| Element | Place | Look and behaviour |
|---|---|---|
| `MODES` | Top centre, baseline y 104, cap 44, accent | Sits on the rule |
| **The rule** | y 150, edge to edge | The title's rule (2.4 px in the field, its 10 px copy, its ends the music at half the title's height: up to 33 px), moved up. It is what the cards hang from |
| **The cards** | Three in view. Centre: 720 x 440 at (1200, 440). Sides: 520 x 340 at (500, 440) and (1900, 440), at 0.6 of their light | Corners 40. Each is one mode, in the line-up's order, the row wrapping round. Tap a side card, drag the row more than 80 px, or left / right on keys and pad: that card comes to the centre, the row sliding over half a beat |
| The centre card | — | Glass in the mode's two colours at 0.085, outline 3.4 px at 2.2 in them, two pools (the settings' sound card, playing). **Emblem** (560 x 190 about (1200, 330), `README.md` 10, moving on the beat). **Name**: a tube in the mode's colours, cap 64, baseline y 492. **Caption**: cap 18, white at 1.2, baseline 540. **Facts**: cap 13, accent, baseline 580: `1-4 PLAYERS   TEAM OR VERSUS   ABOUT 3 MINUTES`. **Best**: cap 14, gold, baseline 622, with the gold star when the mode has been cleared |
| A side card | — | Outline 2 px at 0.8 in its colours, its emblem (360 x 130) and its name (cap 36); no glass, no words |
| Dots | y 700, seven, 36 px apart | Which card is at the centre: that dot in its mode's colour at 1.9, the rest white at 0.32 (PLAY's beat dots' look) |
| **Track line** | Baseline y 752, cap 18, the chosen Track's accent | `TRACK 11   MIRRORBALL   122 BPM`; a bonus Track: `BONUS   MIRRORBALL   122 BPM`. For THE SET: `ALL TEN TRACKS, IN ORDER`; HEADLINERS: `THE TEN BOSSES, IN ORDER` |
| **Track strip** | Centre y 820 | For a mode that takes a Track: one chip a Track it can play, 96 px apart, centred (twelve: x 672 to 1728; ONE TRACK's ten: 768 to 1632). A chip: a disc of 30 px in its Track's accent (outline 2 px, its number at cap 16); the two bonus chips carry a small four-point star. **The chosen chip**: 42 px, glass in its accent, a ring in the player colour, its number at cap 22. ONE TRACK: a gold dot over each chip whose boss has fallen. For THE SET and HEADLINERS the strip is the set list instead: ten bars 100 x 14, 8 apart, in the Tracks' accents, numbers under them at cap 12; it takes no tap |
| BACK | (282, 968), 300 x 90 | To the title (to the lobby, in a room). Back does the same |
| **PLAY** | (1200, 968), 600 x 130, the player colour, label 52, the four beat dots as on the title | The drop-in into that mode and Track. In a room it reads **DONE** and goes back to the lobby |

In a room only (the host's), bottom right:

| Element | Place | Behaviour |
|---|---|---|
| `TEAM` / `VERSUS` | A segmented pill, 420 x 80 at (1958, 930) | The room's game: together, or each for themselves (`README.md` 8) |
| `HELP ON` / `HELP OFF` | A segmented pill, 420 x 64 at (1958, 1026); only with VERSUS | The underdog's help (V7); on unless switched off |

Rules:

- **A Track is heard as it is chosen.** Tapping a chip marks it at once (its ring, and `NEXT BAR` with
  four dots counting the bar out, as a sound card does); on the bar line the whole band changes to that
  Track with a crash, and the screen's palette cross-fades to that Track's over the bar. Tapping
  another chip before the line replaces the choice. With THE SET or HEADLINERS at the centre the band
  plays First Light, as the title does.
- **A mode's own Track is remembered**: bringing a mode to the centre brings its last Track's chip
  with it, and the music follows on the next bar line.
- The card at the centre is the mode; there is no other "select". PLAY plays what is in the middle.
- A mode's emblem only moves at the centre; a side card's is still.
- Keys and pad: left / right change the mode, up / down move between the cards and the strip (the
  strip then takes left / right), Enter or A is PLAY, Esc or B is BACK.
- Nothing on this screen opens another screen.
- Reduce flashing: the cards' strike (below) is a fade.

---

## The lobby and the join screen

The room has one mode, the host's choice, and every seat sees it.

| Element | Place | Behaviour |
|---|---|---|
| **MODE** (the host) | A pill 560 x 90 at (1988, 968): the lobby's bottom right, as LEAVE is its bottom left | Its label is the room's mode (cap 28). Tapping it opens MODES for the room (PLAY reads DONE; TEAM / VERSUS and the help switch are there). A room opens in THE SET, TEAM |
| The room's line | 28 px over that pill, cap 13, accent, right-aligned to the pill | The Track where the mode takes one, then `TEAM` or `VERSUS`, then `HELP OFF` when it is off: `MIRRORBALL   VERSUS` |
| The same, for the others | Right-aligned at x 2268: `MODE` (cap 12, white at 0.5, baseline 930), the mode's name (cap 28, white at 1.1, baseline 972), the room's line (cap 13, accent, baseline 1004) | Text, not a control: it takes no tap |
| A change | — | When the host changes the mode, the Track or the game, the mode's name is struck white-hot for 60 ms on every device with a glass tick, so nobody starts a game they did not see chosen. Everyone's ready state stands |
| The band line | As built | TEAM: as built (`EACH PHONE IS AN INSTRUMENT ...`). VERSUS: `EVERY PHONE FOR ITSELF. NOBODY CAN HURT A FRIEND` |
| **Crowns** | Under each seat's state, cap 12, the pickup's white-gold | In VERSUS: one small crown for each match that seat has won in this room; past five, a crown and `X7` |
| START | As built | Starts the room's mode: the drop-in on every device on the same bar line, as built |
| The join screen's rows | The row's right end, cap 14, white at 0.7 | The room's mode, and `VS` in a versus room: `K7QM   TYLER   2/4          TRACERS  VS` |

A device whose game data lacks the room's mode or Track is refused as any content mismatch is, with
the same line (`design/screens.md`, "Join").

---

## In a run

### The name card

On a run's first downbeat, where the Track's name card stands and in its manner (written across the
lower third, held two bars, its letters streaking out on the beat, drawn at 0.8):

| Line | Size | What |
|---|---|---|
| Above | Cap 28, white | The Track: `TRACK 11  MIRRORBALL` (HEADLINERS: `ACT 3 OF 10`) |
| **The name** | Cap 110, 8 px tubes, a two-arm flare along it as it lands | The mode's name in its two colours (ONE TRACK: the Track's name in its palette's gradient, as the set's card; HEADLINERS: the boss's name) |
| Under | Cap 22, accent | The mode's words (`README.md` 10): `RING THEM. LIGHT THE FLOOR`. FULL BLAST adds its Riffs under that: `TONIGHT: SWARM  FLAK  LANCER  MIRROR` |

THE SET's card is as built. In co-op every device shows the same card.

### The goal line and the run bar

| Element | Place | Look |
|---|---|---|
| **Goal line** | Top centre, 22 px under the phrase bar's `DROP IN n` | One line, cap 14 (the HUD's small size, with the UI scale): its words in the mode's two colours running across the line, its numbers white at 1.4. `README.md` 10 has each mode's. A number that changes swells 15% for a beat |
| **Run bar** | 14 px under the goal line, 520 px wide, 3 px tall; the timed modes only | The whole run: one segment a section, sized by its bars, at 0.3 (Drops at 0.6), a white dot for now. In the last eight bars it blinks on the beat and `LAST CALL` (cap 28, the mode's colours) stands under it for two bars |

Neither is drawn under a boss's bar: in ONE TRACK and HEADLINERS a fight's HUD is the set's.

### First-run hints

Nine more, by the rules of `design/screens.md` "First-run hints" (world text over the ship, once ever
on a device, RESET HINTS brings them back; `hints_seen` grows from eleven bits to twenty).

| Mode | When | Hint |
|---|---|---|
| TRACERS | The run's first bar | `MOVE TO DRAW A LINE` |
| | The first body within 300 wu | `CLOSE A RING ROUND THEM` |
| | The first tiles lit | `LIGHT THE WHOLE FLOOR` |
| SONGLINE | The run's first bar | `FOLLOW THE SPARK` |
| | The first gate in view | `FLY THROUGH THE RINGS` |
| | The streak's first tier | `KEEP THE STREAK: MORE POWER` |
| JAM | The room's first bar | `FLY OVER A PAD` |
| | Four bars later | `STAY ON THE BIG ONE` |
| | The first shelf open | `STAY ON A TOY TO TAKE IT` |

### The versus HUD

| Element | Place | Look |
|---|---|---|
| Your count | Where the score stands (top left, cap 34) | The number the mode counts, in your colour running to white, with its word after it at cap 14 (`TILES`, `GATES`, `ACTS`, `POPS`; a score has none) |
| **The ranking** | Under it, where the Chain and the level bar stand in the set (they move to its right, at cap 16) | One row a player, best first, cap 16, each in its colour: place, name, count. The leader's row carries the crown; the underdog's `UNDERDOG` at cap 10. A row swells for a beat when it moves up |
| TRACERS' share bar | Under the run bar, 520 x 8 | The floor's tiles as one bar in the players' colours, dark for the unlit |
| HEADLINERS' damage bar | Under the boss's health bar, as wide, 6 px | Each player's share of the damage so far, in their colours |
| The crown | On the leader's ship, in the world | `README.md` 8, V5 |
| A knocked-out player | Where the ship stood | `BACK IN 2`, `BACK IN 1` (cap 22 wu, the player's colour) |

### The pause

As built, with the run line naming the mode (`TRACERS  MIRRORBALL  122 BPM`, then the mode's count).
In JAM, QUIT RUN reads `LEAVE` and is drawn as a full control, not at 0.3: leaving is the way out of
a room that has no end. In a versus match RESTART is not offered (as in co-op).

---

## The count

A timed run's last bar (`README.md` 6): after the last downbeat's pop, before the result, each mode
counts what was made, on the music.

| Mode | The count, over one bar |
|---|---|
| FULL BLAST | **The load-out**: every icon of the band row flies out of the ship on a sixteenth and away to the frame's edges (the load-in, backwards), the halo's arcs last; the score's digits in the HUD roll to their final value |
| TRACERS | A line of light, the rule stood upright (2.4 px in the mode's colours, a white core), crosses the Stage from the left wall to the right in two beats. Each column of tiles it passes flashes and pays; the goal line's percentage and, in versus, the share bar count with it. For the next two beats the floor holds, pulsing on the beat |
| SONGLINE | The string as it was flown is drawn across the lower third as one line of marks (one a gate, 1,600 px wide): it lights from the left in two beats, each threaded gate a tick of the player's colour, each one in time white; `GATES 446 OF 512` counts beside it |
| A versus match | The same, with every player's colour counted side by side |

Then, on the bar line, the result or the podium arrives as game over arrives: the run behind dims to
a fifth over a beat.

---

## Results

One screen for every mode that ends, laid out as game over is (`design/screens.md`): a word at the top,
the headline number, a row of four, the run drawn as a bar, the band, RETRY and HOME. What changes:

| Mode | The top word (cap 44) | Headline (cap 150) | The four | The bar | Controls |
|---|---|---|---|---|---|
| ONE TRACK | `TRACK COMPLETE` in gold when the boss fell; else `RUN OVER` with `ENDED BY` | Score | `TIME`, `BEST CHAIN`, `KILLS`, `BOSS` | The Track's eight sections; the set list's marks | RETRY, NEXT TRACK, HOME |
| HEADLINERS | `HEADLINERS COMPLETE` in gold; else `RUN OVER` | The time, in gold; else `ACT 6 OF 10` | `BOSSES` (or `TIME`), `HITS TAKEN`, `FASTEST`, `SCORE` | One segment a boss, as long as its fight | RETRY, HOME |
| FULL BLAST | `FULL BLAST` in the mode's colours | Score | `BEST CHAIN`, `KILLS`, `BOOSTS`, `HITS TAKEN` | Nine sections, the four waves named | RETRY, HOME |
| TRACERS | `TRACERS` | Score | `FLOOR`, `FULL HOUSES`, `BEST RING`, `BEST CHAIN` | Seven sections; stars for houses, ticks for big rings; and the floor as it ended, small | RETRY, HOME |
| SONGLINE | `SONGLINE` | Score | `GATES`, `IN TIME`, `BEST STREAK`, `BEST CHAIN` | Seven sections, and the string as flown | RETRY, HOME |

- **A new best**: the gold four-arm flare and `NEW BEST`, as built. Otherwise one line under the
  headline, cap 14, gold: `TRACK BEST 412,300` (the best for that mode and Track).
- **RETRY** is where it is on game over (bottom right, 600 x 140): the same mode and Track, a new seed,
  by RETRY's drop-in. **HOME** under it as built. **NEXT TRACK** (ONE TRACK only): 460 x 110 at
  (1390, 880), a white pill, 48 px left of RETRY.
- **Co-op**: the set's co-op game over (the team's number, the table of players in their colours,
  RETRY the host's), with the mode's top word and `TEAM BEST 2,104,880  3P` under the headline.
- JAM has no result. Leaving it is home.

---

## The podium

A versus match ends here, on every device, in place of a result.

| Element | Place | Look |
|---|---|---|
| `WINNER` | Top centre, baseline y 104, cap 44, accent | `A DRAW` when the count is level at the top |
| **The winner** | A ring of 150 px about (1200, 330) | The lobby's seat, grown: the ring in the winner's colour field, glass, a pool of their colour (radius 500 at 0.06), their ship at three times the seat's size with its halo, turning slowly; **the crown** over the ring, 120 px wide, in the pickup's white-gold at 2.0 |
| Name and count | Under the ring: the name at cap 34 in their colour (baseline 540), the count at cap 60 white running to their colour (baseline 620): `412 TILES` | |
| **The table** | Centred, rows 46 px apart from y 700, cap 22 | Every player, best first, each row in its colour: place, name (`P1 (YOU)`), the count, the mode's second number, and their crowns in this room. Names left-aligned, numbers right-aligned, as the co-op table |
| **REMATCH** | (1200, 968), 600 x 130, the host's, the player colour | The same match again on a new seed: the drop-in on every device. The others see `WAITING FOR HOST` in its place |
| LOBBY | (1988, 968), 330 x 90, the host's | Back to the lobby, the room standing, to change the mode |
| LEAVE | (282, 968), 300 x 90, everyone's | Leaves the room (home) |

How it arrives, on the music:

| When | What |
|---|---|
| The count's bar | The mode's count, every colour side by side (above) |
| The bar line | The run dims to a fifth. The table writes itself, the rows in seat order, each row's number counting up over the bar with a tick a step; the rows slide into their places as the numbers pass each other |
| The next bar line | **The winner is named**: their row flashes and rises to the ring; the ring draws itself; the ship arrives in it; the crown drops onto it from above over a beat and lands with a bell; `WINNER` is struck. A clamped flash (a pool of the winner's colour, radius 1,000 at 0.3 for 120 ms), a shower of confetti in the winner's colour (the Drop's recipe), the lattice ringing once |
| A beat later | The controls rise into place |

A draw: both rings side by side (300 px apart), no crown. Reduce flashing: no flash; the confetti stays.

---

## Transitions

Every one obeys `design/screens.md` "Transitions": nothing cuts, lengths are in beats, a tap is
answered at once, the music carries it.

### Title to MODES: the rule lifts

| When | What is seen | What is heard |
|---|---|---|
| The tap | The mode pill takes its pressed state and keeps it; a ring leaves it (64 px in half a beat) | The confirm chord |
| The next beat | PLAY, CO-OP, SETTINGS, their captions, the best and the notes fade and sink 30 px. The fan folds flat; the word fades; the tagline goes and the rule closes its gap. The demo behind dims from a third to a fifth | A one-beat sweep, opening |
| The beat after | **The rule rises** from y 432 to y 150, easing out, its ends still moving with the music. `MODES` is played onto it: five letters, a sixteenth apart, each struck as the title's letters are | Five notes of the title's eight (A3 C4 E4 A4 C5) |
| Its second half | **The cards are thrown down from the rule**: each card's outline draws itself from the rule downward in a quarter beat, the centre first, the sides a sixteenth after; each is struck white-hot for 60 ms as it closes | The centre card's note, then the sides': the next three of the eight (E5 G5 A5) |
| The next beat | Emblems and words fade up; the dots, the Track line and strip, BACK and PLAY rise 14 px into place. PLAY takes a tap from here | — |

Two and a half beats. Any input during it skips to its end within a beat, as the title's intro does.

### Inside MODES

- **Another mode**: the row slides over half a beat, starting at once; the card that arrives at the
  centre is struck (60 ms, white) on the next sixteenth with its own note: the seven cards are the
  first seven notes of the title's eight, in the line-up's order, so going along the row plays the
  scale. Its emblem starts moving; the Track line and strip change with a one-beat cross-fade.
- **Another Track**: the rules above (heard on the bar line). The palette's cross-fade is the Track
  change's, over one bar.

### MODES to the title (BACK)

The cards fold up into the rule over half a beat, sides first; `MODES` streaks out to both sides; the
rule sinks to y 432 over a beat; then the title builds as home builds it from its rule (`design/screens.md`
"Home": the fan opens and the word is played over two beats, then the tagline, PLAY, the pill, CO-OP
and SETTINGS). The band returns to First Light on the next bar line if a chip had changed it. About a bar.

### MODES to a run (PLAY)

The drop-in (`design/screens.md`), from this screen: over the wait the cards, the strip and BACK fade and
sink; the tubes of `MODES` run hot; in the last beat the rule draws in to its centre and slides to the
frame's centre, PLAY shrinks into the same point, and the downbeat is the same burst, its eight rays
in the mode's two colours. The run that arrives is the mode's, on the chosen Track: when the Track is
not the one playing, the band's change to it is the drop-in's bar line (it enters with the crash the
drop-in already has).

### The lobby and MODES

The lobby leaves as a screen leaves (its pieces fade and sink 30 px over a beat). MODES arrives as
from the title without the lift: the rule is thrown out of its centre at y 150 in half a beat and the
cards down from it. DONE is the same backwards, and the lobby's mode name is struck as it returns.

### A run to its result, and on

- A timed run's end is `README.md` 6, then the count, then the result or the podium arriving as game
  over does.
- RETRY, REMATCH and NEXT TRACK are RETRY's drop-in. NEXT TRACK's wait holds the band's change of
  Track: the next Track enters on the downbeat with the crash.
- HOME, LEAVE and LOBBY are home (`design/screens.md`), to the title or to the lobby.
- Leaving JAM (the pause's LEAVE) is home.

---

## Checks for the engineer's review set

Every screen above at 2400 x 1080 with its `.small.png`, in Debug with no validation messages and no
layout warning (two controls, or a control and a line, closer than 24 px): the title with each of the
seven modes on its pill; MODES with each mode at the centre, with a bonus Track chosen, and in a room
with VERSUS; the lobby as the host and as a joiner in TEAM and in VERSUS with crowns; the join list with
three made-up rooms in three modes; each mode's name card; the goal line and run bar in each timed
mode; the versus HUD with four players; each count as a strip; each result; the podium with two and
with four players and a draw; each transition as a strip a quarter beat apart. The picture names are
in `docs/MODES_PLAN.md`.
