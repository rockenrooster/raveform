# RAVEFORM — Screens

Layouts for the screens outside the run. Everything is drawn with the same light as the game (the
stroke font, capsule lines, pools), on the background gradient of the current palette, with no lattice.
Sizes are reference pixels at 2400×1080 and scale with screen height; keep the 132 px side margins
clear. Every control has the pressed state of `design/look.md` §6: filled disc or bar in the player
colour at 0.45, outline at 2.0 and thicker, label at 1.8.

Screens are added here as their milestone comes up. So far: sync calibration (M2).

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
