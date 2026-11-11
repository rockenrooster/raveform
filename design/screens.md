# RAVEFORM — Screens

Layouts for the screens outside the run. Everything is drawn with the same light as the game (the
stroke font, capsule lines, pools), on the background gradient of the current palette, with no lattice.
Sizes are reference pixels at 2400×1080 and scale with screen height; keep the 132 px side margins
clear. Every control has the pressed state of `design/look.md` §6: filled disc or bar in the player
colour at 0.45, outline at 2.0 and thicker, label at 1.8.

Screens are added here as their milestone comes up. So far: sync calibration (M2); title, game over
and pause (M3); co-op lobby (M4). Pictures of the last four: `design/previews/screens.png` (tiled) and
`screen_title.png`, `screen_game_over.png`, `screen_pause.png`, `screen_lobby.png` at full size, drawn by
`design/tools/screens.py`.

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
| **RESUME** | Centre, 600 × 130, label 48, player colour | **Counts the player back in:** four numbered rings fill on four beats, and the run starts on the bar line after them. Nobody is dropped into a fight mid-phrase |
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
