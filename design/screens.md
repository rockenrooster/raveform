# RAVEFORM — Screens

Layouts for the screens outside the run. Everything is drawn with the same light as the game (the
stroke font, capsule lines, pools), on the background gradient of the current palette, with no lattice.
Sizes are reference pixels at 2400×1080 and scale with screen height; keep the 132 px side margins
clear. Every control has the pressed state of `design/look.md` §6: filled disc or bar in the player
colour at 0.45, outline at 2.0 and thicker, label at 1.8.

Screens are added here as their milestone comes up. So far: sync calibration (M2); title, settings,
the Trio, game over and pause (M3); co-op lobby (M4). Pictures: `design/previews/screens.png` (tiled) and
`screen_title.png`, `screen_settings.png`, `screen_sync.png`, `screen_game_over.png`, `screen_pause.png`,
`screen_lobby.png` at full size, drawn by `design/tools/screens.py`.

Rules for every screen:

- The music never stops. Menus run on the same clock as the game, so anything that pulses, pulses on
  the beat, and leaving a menu lands on a bar line.
- One control is the obvious one: it is the largest, it sits where a thumb rests, and it carries the
  player's colour. Everything else is white outline.
- A control is a pill: outline 2 px at 0.9 and its label at 1.1. Pressed: glass fill in its colour at
  0.45, outline 3 px at 2.0, label at 1.8, a soft pool of its colour behind. Disabled or destructive
  but rarely wanted: outline at 0.3.
- Headline words are thick tubes: the stroke font at 6% of its cap height wide in a colour field, with
  a white copy a quarter as wide on top.
- No screen has more than three choices in its main row.

---

## Sync calibration (M2)

Purpose: one number per device that lines up what is heard with what is seen (TECH_SPEC §6.3). The
player gets here from settings, and once automatically if the device reports no usable audio
timestamps. It must be usable by someone who has never heard the word latency.

```
                                 SYNC

              MAKE THE FLASH AND THE CLICK HAPPEN TOGETHER

                               (  ring  )

        [ - ]   ------------------|----●-------------   [ + ]
                                    +35 MS

        [ AUTO ]                                        [ DONE ]
```

| Element | Place | Behaviour |
|---|---|---|
| Title `SYNC` | Top centre, cap height 34, palette accent | — |
| Instruction | Under the title, cap height 16, white at 1.2 | One line. On Bluetooth it changes to `BLUETOOTH ADDS DELAY - SLIDE RIGHT UNTIL THEY MEET` |
| **The ring** | Centre, 150 px radius, white at 0.5, width 2.0 | A metronome at 120 BPM. On each beat the ring flashes: a filled disc at 0.45 fading over 120 ms, the outline jumping to 2.6 and a ring expanding from it to 260 px over 200 ms. Each beat also plays the click (the interface tick of the effect set). The flash is drawn when the clock says the click is heard, using the offset being edited, so what the player sees is exactly what the game will do |
| Beat dots | Four small dots under the ring, 36 px apart | The current beat's dot is lit; beat 1 is the accent colour. They make it possible to tell early from late |
| Slider | Under the dots, 1100 px wide | −150 ms to +350 ms, zero marked with a tick. Thumb: a 44 px disc in the player colour; drag anywhere on the track. The value is shown under it in cap height 22: `+35 MS`, or `0 MS (AUTOMATIC)` when untouched |
| `−` and `+` | At the ends of the slider, 110 px discs | One tap = 5 ms; hold to repeat four times a second |
| `AUTO` | Bottom left | Clears the offset back to what the device's own timestamps give |
| `DONE` | Bottom right | Saves and returns. Back does the same |

Rules:

- The click and the flash keep running the whole time; nothing on this screen pauses them.
- The value applies at once, so the player hears and sees the result of every nudge.
- No words about buffers, frames or milliseconds beyond the number itself.
- Reduce-flashing mode: the disc fill is 0.2 instead of 0.45 and there is no expanding ring.
- Haptics: a light tick on the beat, on the same clock, so the three senses can be compared.

---

## Title (M3)

The game is already playing when it opens: the bot's run goes on behind everything at a third of its
light, in the palette of Track 1, with the music at full level.

| Element | Place | Behaviour |
|---|---|---|
| **Waveform** | Across the whole width at 48% of the height, 2 px, running cyan → white → first enemy hue → magenta | The actual music, drawn as a line: about a bar of the master bus, scrolling. A faint mirrored echo of it underneath. The logo stands on its own sound |
| **Logo** `RAVEFORM` | Centred, cap height 190, top at 216 | Tubes 11 px wide in the same colour field as the waveform, white copy 3.2 px, a pool of the first enemy hue behind, two four-arm flares on its corners. Swells 2% on the kick |
| Tagline `SOUND BECOMES LIGHT` | Under the logo, cap height 22, accent | — |
| Beat dots | Four, under the tagline | The current beat is lit; beat 1 in the accent |
| **PLAY** | Centre, 600 × 140, label 52, player colour | One tap starts a run on the next bar line. This is the control a thumb finds first |
| CO-OP, SETTINGS | Left and right of it, 330 × 104, label 30, white | — |
| The ship | Under PLAY | Idles, turning toward the last touch |
| Best | Bottom left, cap 14, gold | `BEST 1,204,880  TRACK 05  X412` |
| Note | Bottom right, cap 14, white at 0.5 | `HEADPHONES RECOMMENDED`; on first launch the photosensitivity notice takes this screen first |

## Settings (M3)

A sound check, not a form. The music keeps playing, so every change is heard as it is made. Three
groups as tabs across the top, never more: **SOUND**, **CONTROLS**, **LOOK** (video and accessibility
together). The open tab is glass in the player colour. SOUND is the one drawn; the other two follow
the same pieces (cards for a choice of a few, faders for amounts, segmented pills for off / low / high).

| Element | Place | Behaviour |
|---|---|---|
| `SETTINGS` | Top centre, cap 44, accent | — |
| Tabs | Three pills 360 × 84 at y 168, 420 apart | — |
| Hint | Under the tabs, cap 13, white at 0.7 | `THE SAME MUSIC, THREE WAYS. TAP ONE: IT CHANGES ON THE NEXT BAR` |
| **Sound cards** | Three, 620 × 300, corners 40, 48 apart, top at 286 | One per sound variant (`design/music/variants.json`). Each has its own two colours, its name as a tube in them (cap 46), three words, and its **signature line**: the same four beats drawn the way that variant sounds. The lines move with the music: they are the live master bus, shaped per card |
| Club | Amber → magenta | A fat round line with a hump and a soft pool on every kick. `WARM  ROUND  DEEP` |
| Arcade | Cyan → lime | A thin bright stepped line with a spike and a small flare on every beat. `CRISP  BRIGHT  DRY` |
| Velvet | Violet → azure | A slow wide line with two fainter echoes trailing it. `SOFT  WIDE  LONG` |
| Card states | — | **Playing:** glass in its two colours at 0.085, outline 3.4 px at 2.2, two pools, `PLAYING` with a four-bar level meter. **Tapped, waiting for the bar line:** outline at 1.5, `NEXT BAR` in its colour with four dots counting the bar out. **Idle:** outline at 0.8, line at 0.6 |
| **Faders** | Three under the cards, one per card column, at y 690 | `MASTER`, `MUSIC`, `EFFECTS`. The track is forty short bars and is also a meter: bars up to the live level of that bus are lit (player colour → accent, the top fifth amber), the rest up to the thumb are white at 0.22. Thumb: a 48 px disc in the player colour. Value in cap 22 at the right |
| `SYNC` | Bottom left, 360 × 100 | Opens sync calibration; the current offset sits under it in gold |
| `HAPTICS` | Bottom centre | A row of three small pills: `OFF`, `SOFT`, `STRONG`. Choosing one plays it once |
| **DONE** | Bottom right, 460 × 124, player colour | Saves and returns. Back does the same |

Rules:

- A sound change is never a cut: the tapped card waits for the bar line, then the whole band changes
  together. Tapping another card before the bar line replaces the choice.
- Nothing on this screen opens a second screen except `SYNC`.
- CONTROLS holds auto-aim assist, Auto Nova (on by default: `AUTO NOVA  FIRES ONE BEFORE YOUR LAST CELL GOES`), stick size and position (drawn as the real sticks, dragged into
  place), swap sides, button layout, gamepad mapping. LOOK holds effects (Auto / Low / Medium / High /
  Ultra), frame-rate target, reduce flashing, shake, vivid colour, colour-blind-safe palettes,
  high-contrast threats and UI scale, with the bot's run behind it so each change is seen at once.

## The Trio (M3)

Not a screen: the upgrade choice happens in the world, at the Break, with the run still going
(GAME_DESIGN §6.1). Pictures: `design/previews/trio.png` (both cases), `screen_trio_break.png`,
`screen_trio_fusion.png`, drawn by `design/tools/trio.py`. Sizes in world units (1.5 px at 2400×1080).

| Element | Place | Behaviour |
|---|---|---|
| **Choice ring** | A circle of 170 wu around where the ship was when the Break began; it does not follow the ship | It is also the clock: the lit arc (accent ↔ white, 2 px at 1.3, a bright bead at its end) is the Break that is left; the spent part is white at 0.16. When it runs out, the orb nearest the ship is taken |
| **Orbs** | Three on the ring: top, lower left, lower right | The upgrade's own icon in its frame (`design/icons.json`): Mods 38 wu, Riffs ×1.14. A thin white ring at 1.42 × that around it. They swell 6% on the beat |
| Words | Always outside the ring: above the top orb, left of the left one, right of the right one | Name, cap 20 wu (30 px), white at 1.3. Effect under it, cap 11 wu, palette accent: two to four words, a number where there is one (`+1 SHOT LANE`, `LEVEL 3: SECOND RING`) |
| Stack pips | Under the words | One dot per stack or level the upgrade can have: owned ones white, the one this pick adds gold with a ring, the rest empty outlines |
| **Closing on one** | Within 110 wu of an orb, or flying at it | That orb answers: icon ×1.12, its ring in the player colour at 2.0 with a dashed outer ring, a pool of the player colour, a line of light from the ship to it, its words at 1.9. The other two do not change |
| **Locked** | The Break's first two beats | The ring draws itself in one beat and the orbs fade in over the next, as outlines at 0.5 with their words already readable. Nothing can be taken yet. On the third beat they light with a chime |
| **Holding** | The ship inside the orb's thin ring (1.42 × the icon) | The thin ring fills as a solid arc in the player colour, clockwise from twelve o'clock, over two beats, with two rising notes. Flying through does nothing. Leaving empties it at once and silences it |
| **Taking it** | The arc closes | The orb folds into the ship in two beats' worth of sparks in its two colours, the icon flies to the band row of the HUD, the other two pop into sparks of their own colours, the ring closes. The pickup sound is the "upgrade taken" chord, on the next sixteenth |
| Hint | Bottom centre, cap 14 px, white at 0.7 | `STAY ON ONE TO TAKE IT`, on the first three Trios of a run |

**A level-up Trio** (GAME_DESIGN 6.1, Shard levels) is the same ring and orbs, opened round the
ship on a bar line in the middle of the fight. Differences: the fight is not dimmed; the ring is
gold instead of the accent and runs for four bars; `LEVEL 7` in gold, cap 16 wu, sits at the ring's
centre for the first bar; each orb's thin ring is drawn double, because a ship inside it cannot be
hurt and bodies that touch it are shoved off. Everything else — the two locked beats, the
two-beat hold, the nearest orb at the end — is the same.

With a **Fusion** on offer the triangle turns point down (orbs at lower centre, upper left, upper
right) and the Fusion sits above the ring at 230 wu: its icon at ×1.45 in the eight-point frame, a
four-arm gold flare and a two-arm white one, its words in gold, and two thin gold lines running up to
it from the ring. It is the only gold thing on the screen.

Rules:

- The fight steps back: the lattice rests at 0.07, enemies still alive are drawn at a fifth of their
  light, nothing spawns inside the ring. Shards still fly in.
- In co-op each player has their own ring and orbs in their own colour; another player's are drawn at
  a third and cannot be taken.
- Nothing here is a touch target. The sticks stay where they are.

Effect words (final, 2026-11-12; plain words, no music terms, so a first-time player can choose):

| Upgrade | Words by stack or level |
|---|---|
| Split | `+2 SHOT LANES` |
| Double-Time | `FIRE RATE X1.5`, then `FIRE RATE X2` |
| Pierce | `SHOTS PIERCE +2` |
| Ricochet | `SHOTS BOUNCE +2` |
| Overcharge | `+50% SHOT DAMAGE` |
| Extra Cell | `+1 CELL, ALL REFILLED` |
| Regen | `CELL REGROWS: 16 BARS`, then `8 BARS`, `4 BARS` |
| Magnet | `SHARD PULL X2`, then `X3`, `X4`, `X5` |
| On-Beat Crit | `BEAT SHOTS X3`, `X5`, `X8` |
| Orbitals | `NEW RIFF: THREE ORBITERS`, then `LEVEL 2: FIVE ORBITERS` and so on by twos |
| Pulse Ring | `NEW RIFF: BEAT RINGS`, `LEVEL 2: SECOND RING`, `LEVEL 3: WIDER RINGS`, `LEVEL 4: HEAVIER RINGS`, `LEVEL 5: TWICE AS OFTEN` |
| Sentinel | `NEW RIFF: OFF-BEAT DRONE`, `LEVEL 2: BURST OF 4`, `LEVEL 3: SECOND DRONE`, `LEVEL 4: BURST OF 5`, `LEVEL 5: THIRD DRONE` |
| Boosts (HUD) | Prism `FIVE-WAY RAINBOW FAN`, Hyper `FIRE RATE X2`, Halo `BLOCKS THREE HITS`, Magnet Storm `PULLS EVERY SHARD` |

## Track name card (M3)

A new Track announces itself like a boss does. On the first downbeat after a Track-change Break the
card writes itself across the lower third and holds for two bars, then its letters streak out to
both sides on the beat: `TRACK 2` at cap 28 in white above, the Track's name (`SOLAR WIND`) at cap
110 in the new palette's gradient, 8 px tubes, a two-arm flare sweeping along the name once as it
lands. The first Track of a run gets the same card on its first downbeat. Enemies spawn as normal
underneath; the card is drawn at 0.8 so it never hides a threat.

## Game over (M3)

The palette is the one the run ended in. The score is the headline; everything else explains it.

| Element | Place | Behaviour |
|---|---|---|
| `RUN OVER` | Top centre, cap 44, accent | — |
| **Score** | Centred under it, cap 150 | White → gold → amber top to bottom, 9 px tubes. Counts up over one bar. A new best gets a gold four-arm flare and `NEW BEST` |
| Four numbers | One row: `TIME`, `TRACK`, `BEST CHAIN`, `KILLS` | Label cap 14 in accent, value cap 40. Four, never more |
| **Set list** | A bar across the width | The run drawn as the song it was: one segment per Track, as long as the time spent in it, in that Track's accent colour with its name under it. A white star where a boss fell, a gold tick for each upgrade taken, a danger-coloured cross for each hit taken. It plays back as a ten-second medley of the run while the screen is up |
| Your band | Under the set list | The icons of the Riffs and the most-stacked Mods the run ended with |
| **RETRY** | Bottom right, 600 × 140, label 52, player colour | One tap: a new run starts on the next bar line |
| HOME | Under it, 330 × 90 | — |

In co-op the score is the team's, each player's own score sits in their colour under it, ranked, and
the set list carries each player's crosses in their colour.

## Pause (M3)

The run freezes and dims to a fifth; the music carries on under a low-pass.

| Element | Place | Behaviour |
|---|---|---|
| `PAUSED` | Top centre, cap 96 | Accent → white → accent |
| **RESUME** | Centre, 600 × 130, label 48, player colour | **Counts the player back in:** four numbered rings fill on four beats, and the run goes on from the place in its bar where it was paused, so the run's bars stay on the band's (as built in M3: a pause on a bar line resumes on a bar line). Nobody is dropped into a fight without the count |
| RESTART, SETTINGS | Left and right, 320 × 100 | Restart asks once |
| Run line | Under the count, cap 14, gold | Track, tempo, score, Chain |
| QUIT RUN | Bottom centre, 320 × 90, outline at 0.3 | Asks once |

Back toggles pause. In co-op any player can pause; it takes hold on the next beat for everyone and the
count-in is shared.

## Co-op lobby (M4)

| Element | Place | Behaviour |
|---|---|---|
| `CO-OP` | Top centre, cap 60, accent | — |
| **Room code** | Centred, cap 120, gold | Four characters. Hosts show it; joiners type it on a twelve-key pad (or pick the host from the list of those found on the network) |
| Network hint | Under the code, cap 13 | `SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT` |
| **Seats** | Four rings of 130 px in a row | A taken seat: ring in that player's colour field, glass, a pool, the ship with its halo, name and state under it. Joining: ring at 0.8, no glass, ship at a third. Open: dashed white ring |
| Band line | Under the seats, cap 12, accent | `EACH PHONE IS AN INSTRUMENT: SET THEM DOWN TOGETHER AND THE ROOM IS THE BAND` |
| **START** | Bottom centre, 600 × 130, host only | Starts when every seated player is ready; the others see `WAITING FOR HOST` |
| LEAVE | Bottom left, 300 × 90 | — |

Each player hears their own lead when they tap their seat, so a group can tell whose phone is whose
before the run starts.
