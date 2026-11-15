# RAVEFORM — Screens

Layouts for the screens outside the run. Everything is drawn with the same light as the game (the
stroke font, capsule lines, pools), on the background gradient of the current palette, with no lattice.
Sizes are reference pixels at 2400×1080 and scale with screen height; keep the 132 px side margins
clear. Every control has the pressed state of `design/look.md` §6: filled disc or bar in the player
colour at 0.45, outline at 2.0 and thicker, label at 1.8.

Screens are added here as their milestone comes up. So far: sync calibration (M2); title, settings,
the Trio, game over and pause (M3); the co-op screens (M4). Pictures: `design/previews/screens.png` (tiled) and
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
- Controls never touch: at least 24 px between any two, and between a control and a line of text.
- A control that cannot be used yet is drawn disabled (outline at 0.3, label at 0.4) and takes no tap:
  JOIN before four characters are typed, START before every seated player is ready.
- Numbers of four digits and more carry their thousands separators everywhere (`27,175`, never `27175`).

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

## Title (M3; Title v2, 2026-11-14)

Title v2, "First Light", is built from the plan of 2026-11-14 (folded in here, its file deleted; its pictures are
`design/previews/title_v2*.png`) with the owner's answers of
2026-11-14: the ship under PLAY is gone; the intro plays on every cold start but much faster than the plan (a bar and a beat); leaving
the app keeps its cut; the word keeps the font's own letter spacing (6 units a letter, not 5.25); the run behind stays at a third of
its light; the captions under CO-OP and SETTINGS stay; and there is **no reflection** (the word's under the rule and the fan's).
The code is `src/game/title_v2.cpp` (the frame) and `src/game/play_transitions.cpp` (the intro, home, the drop); its numbers are
the named block at the top of `title_v2.cpp`. "Sound becomes light": the word stands over a line of light whose ends are the music
and whose middle is a laser rule carrying the tagline.

The game is already playing when it opens: the bot's run goes on behind everything at a third of its light, in the palette of
Track 1, with the music at full level. Reference pixels at 2400 x 1080, scaling with the screen's height. "The field" is one colour
run used by the word, the rule, the fan and the motes: P1 cyan -> white -> the palette's first enemy hue -> its third.

| Element | Place | Look |
|---|---|---|
| **The fan** | From (1200, 432), behind the word | Seven laser beams at 0, +-23, +-46, +-69 degrees from straight up, each from 40 px out, 1,040 px long, a 2 px line at 0.40 fading to nothing along its length with an 18 px copy at 0.028, coloured by its place across the field. One pair lights on each beat (1.0, the rest 0.62): the centre on beat 1, +-23 on 2, +-46 on 3, +-69 on 4. It sways +-3 degrees over four bars |
| **The word** `RAVEFORM` | Cap height 240, baseline 378, at the font's own advance (about 1,840 px wide; narrower screens fit it between the 132 px margins) | Chrome neon: the tube 14 px (above each letter's horizon, 56% down, the field with white mixed in toward the top; below it the field and the third hue, deepening to the foot with the second), a white-hot core 4 px; strokes that run straight on meet flat. Its depth: copies of the tubes shrinking about (1200, 432) to 6.6% smaller (twelve half steps; six on the low tier) in the second and third hues, so the word recedes toward the rule's centre from both sides. Its halo: pools along every stroke and one pool of the first hue, radius 850 at 0.035. Swells 1.5% on the kick about its baseline's centre. Every second bar a sheen crosses it in a beat and a half |
| Glints | The R's top corner; the M's right foot | Four-point stars, arms 96 and 70 px, flaring on the kick and on the backbeat (reduce flashing: 0.35) |
| **The rule** | y 432, edge to edge | A 2.4 px line in the field at 2.1 with a 10 px copy at 0.16. Under the word it is straight with a 1.2 px white core at 1.6 and two 5 px white flare arms from the tagline's gap fading to nothing 1,150 px out; beyond the word its ends are the music (the master bus, rising from nothing to +-66 px over 300 px) with a mirrored echo in the second hue. On the kick its straight part brightens and a bump runs along its ends in half a beat. Its pieces meet flat (no beads) |
| **Tagline** | Set into the rule's gap, centred on y 432 | `S O U N D   B E C O M E S   L I G H T`, cap 22, 55% accent + 45% white at 2.1; the rule opens a gap for it (its width and 44 px each side) |
| **Motes** | Rising off the rule | Up to 110 dots (40 on the low tier) in the field's colour at their place, each rising 300 px over four beats with a +-30 px sway, fading as it rises; louder music sheds more |
| **PLAY** | (1200, 800), 640 x 150 | Glass in P1 at 0.08, an outline of 80% P1 and 20% white (3 px at 1.8), a P1 pool of radius 350; its label a tube (cap 60) 11 px above its centre; **the four beat dots inside it**, 47 px under its centre (the beat's lit at 1.9, beat 1 in the accent, the rest 0.32). Every beat a copy of its outline leaves it, growing 40 px over the beat (1.0 on beat 1, 0.7 on the others; half with reduce flashing). Hover (a mouse): fill 0.18, outline at 2.3, the label's core at 2.2; pressed: fill 0.45, outline 4 px at 2.6, label 1.5 / 2.4, pool 0.10 |
| CO-OP, SETTINGS | (640, 800) and (1760, 800), 340 x 104, label 30 | As every pill (since Title v2 every pill has a hover state with a mouse: a white fill at 0.10, the outline 3 px at 1.6, the label at 1.5). A caption 28 px under each, cap 12, white at 0.5: `2-4 PLAYERS   ONE WI-FI` and `SOUND   CONTROLS   LOOK` |
| Notes | y 940, cap 18, danger | `THE HOST LEFT` and the co-op messages |
| Best | Bottom left, cap 14, gold | `BEST 1,204,880  TRACK 05  X412` |
| Note | Bottom right, cap 14, white at 0.5 | `HEADPHONES RECOMMENDED`; on first launch the photosensitivity notice takes this screen first |
| Version | Bottom right under the note, cap 11, white at 0.4 | The version, the build's commit and its date and minute: `V0.9.0  F0CD14F  2026-11-14 16:40` (the owner, 2026-11-14: "include a version # with build date at the bottom right at the main menu"); the version is `CMakeLists.txt`'s project version |

**The intro** (every cold start of the app, after the notice on a first launch; not when the app comes back from the background,
not on home, not in pictures, benchmarks, fixed-step or `--autoplay` runs; picture `m6_t2_intro`, a strip an eighth apart). The
screen is black until the band runs (a second at most). On the next beat line **a point of light** at the rule's centre (flashing on
the kick), under a one-beat riser with the band's low-pass closed; on the next beat **the rule is thrown out of it** to both edges in
half a beat with a crash and a clamped flash (a white pool of radius 1,000 at 0.3 for 120 ms), its ends waking over the beat, the
background and its stars coming up from black and the filter opening. A beat later the fan snaps open (6% past and back) and **the
word is played**, a letter a sixteenth (R first, M last): each letter is struck white-hot for 60 ms (tube x 1.2 at 3.5) while the
rule leaps under it (a notch 92 px wide, 34 up and 12 down, gone in an eighth), dark for 40 ms, then lit with its depth and halo;
each with one note of the lead, A3 C4 E4 A4 C5 E5 G5 A5. Two beats later the tagline opens the rule from the centre and PLAY rises
14 px into place over the beat (it takes a tap from then); CO-OP and SETTINGS with their captions a quarter beat later, the best,
the note and the version a quarter after that. The run behind comes up over the next bar. A bar and a beat in all. **Any input
skips it** (and is eaten): everything not yet on screen arrives inside a beat, and the word's notes not yet placed are not played.
Reduce flashing: no flash, letters fade on over an eighth instead of being struck, the glints at 0.35.

**The quiet title** (the owner, 2026-11-14: "after waiting at the main menu for a while the menu fades away so you can see
the demo until you tap the screen and the menu fades back in"). The title at rest (no transition, no co-op session) that hears
no input for **8 bars** (17.5 s at Track 1's 110 BPM) leaves on the next bar line, as home builds it but backwards, each piece
over a beat: the best, the note and the version with CO-OP and SETTINGS on the bar's first beat, PLAY on the second, the
tagline, the rule's gap and the motes half a beat later, the fan folding flat and the word fading on the third, the rule drawing
back into its centre on the fourth (Title v2: the word fades, it does not fall: the fall belongs to the drop); over the same bar
the demo comes up from a third of its light to its full light. Any input brings it all back over a beat from where it stood:
a tap anywhere, the pointer moving (a mouse, more than 4 px), a key, a gamepad button, a stick or trigger past half. That input
does nothing else (a tap where PLAY was does not start a run), and nothing takes a tap until the title is back; the count of
8 bars starts again from it. The demo plays as a person would (the owner: "the demo bot also needs to have smooth movement,
not erratic at all"): it steers with its stick eased over 0.35 s (0.06 s while it closes on a Trio's orb or a pickup, so it
holds the orb instead of sailing past it), the auto-aim assist fires for it, and it never dashes. It plays the whole game (the
owner: "make the demo AI actually get powerups ... it should never die so you can see all the tracks eventually ... instead of
dying it gets a nova every time a death would happen"): it goes for Boost orbs, Extra Lives and Shards from farther and harder,
takes its Trios, and the hit that would end its run brings it back with two cells and a free Nova; once it has played the whole
set (about twenty minutes) it starts over from Track 1. The music and the palette follow the Track it is in.
Picture `m6_title_idle` (a strip a beat apart from a beat before it leaves; `--strip 600` every
frame, with the tap on PLAY's place two and a half bars after it left).

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
- **COLOUR** (M6, 2026-11-14; TECH_SPEC 5.2), beside REDUCE FLASHING: `STANDARD` / `VIVID` under `COLOUR (VIVID: THE
  SCREEN'S WIDE GAMUT)`. VIVID presents the frame in Display-P3, so the screen maps the game's colours into its wider gamut
  (every colour more saturated, white and greys as they were); it changes at once (the swapchain is rebuilt) and is saved.
  Where the screen offers no Display-P3 the row reads `STANDARD (THIS SCREEN HAS NO WIDE GAMUT)` and has no control.

## The Trio (M3)

Not a screen: the upgrade choice happens in the world, at a Shard level or after a boss, with the run still
going (GAME_DESIGN §6.1; the cycle's Break deals no Trio since 2026-11-14). Pictures: `design/previews/trio.png` (both cases), `screen_trio_break.png`,
`screen_trio_fusion.png`, drawn by `design/tools/trio.py`. Sizes in world units (1.5 px at 2400×1080).

| Element | Place | Behaviour |
|---|---|---|
| **Choice ring** | A circle of 170 wu around where the ship was when the Trio opened; it does not follow the ship | It is also the clock: the lit arc (accent ↔ white, 2 px at 1.3, a bright bead at its end) is the time that is left (a level's four bars, a boss's Break); the spent part is white at 0.16. When it runs out, the orb nearest the ship is taken |
| **Orbs** | Three on the ring: top, lower left, lower right | The upgrade's own icon in its frame (`design/icons.json`): Mods 38 wu, Riffs ×1.14. A thin white ring at 1.42 × that around it. They swell 6% on the beat |
| Words | Always outside the ring: above the top orb, left of the left one, right of the right one | Name, cap 20 wu (30 px), white at 1.3. Effect under it, cap 11 wu, palette accent: two to four words, a number where there is one (`+1 SHOT LANE`, `LEVEL 3: SECOND RING`) |
| Stack pips | Under the words | One dot per stack or level the upgrade can have: owned ones white, the one this pick adds gold with a ring, the rest empty outlines |
| **Closing on one** | Within 110 wu of an orb, or flying at it | That orb answers: icon ×1.12, its ring in the player colour at 2.0 with a dashed outer ring, a pool of the player colour, a line of light from the ship to it, its words at 1.9. The other two do not change |
| **Locked** | The Trio's first two beats | The ring draws itself in one beat and the orbs fade in over the next, as outlines at 0.5 with their words already readable. Nothing can be taken yet. On the third beat they light with a chime |
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
| Split | `+1 SHOT LANE` |
| Double-Time | `FIRE RATE X1.5`, the second rank `FIRE RATE X2` |
| Pierce | `SHOTS PIERCE +2` |
| Ricochet | `SHOTS BOUNCE +2` |
| Overcharge | `+50% SHOT DAMAGE` |
| Extra Cell | `+1 CELL, ALL REFILLED` |
| Regen | `CELL REGROWS: 16 BARS`, then `8 BARS`, `4 BARS` |
| Magnet | `SHARD PULL X2`, then `X3`, `X4`, `X5` |
| On-Beat Crit | `BEAT SHOTS X3`, `X5`, `X8` |
| Orbitals | `NEW RIFF: A GUARD RING`, `LEVEL 2: FIVE SHARDS`, `LEVEL 3: RELIGHTS FASTER`, `LEVEL 4: NINE SHARDS`, `LEVEL 5: RELIGHTS FASTER` |
| Pulse Ring | `NEW RIFF: BEAT RINGS`, `LEVEL 2: SECOND RING`, `LEVEL 3: WIDER RINGS`, `LEVEL 4: HEAVIER RINGS`, `LEVEL 5: TWICE AS OFTEN` |
| Sentinel | `NEW RIFF: OFF-BEAT DRONE`, `LEVEL 2: BURST OF 4`, `LEVEL 3: SECOND DRONE`, `LEVEL 4: BURST OF 5`, `LEVEL 5: THIRD DRONE` |
| Boosts (HUD) | Prism `FIVE-WAY RAINBOW FAN`, Hyper `FIRE RATE X2`, Halo `BLOCKS THREE HITS`, Magnet Storm `PULLS EVERY SHARD` |

The rest of the upgrades (M5, 2026-11-13). The engineer wrote words for all of them in this style
while building them; these are those words, read as a first-time player would read them. Changed:
no word a player must know music or mathematics for (`SINE`, `TRIPLET`, `ON 2 AND 4`), nothing that
names a thing the game never names on screen (`TRIO`), and no word that is also another upgrade's
name (`ARC` for Aegis, `SINGULARITY` for the Black Hole). At most 26 characters.

| Mod | Words by stack |
|---|---|
| Fork | `SHOTS FORK ON A HIT` |
| Seeker | `SHOTS HOME IN`, `TIGHTER HOMING`, `SHOTS HUNT` |
| Heavy Step | `EVERY 4TH SHOT EXPLODES`, `BIGGER BLASTS`, `HEAVIER BLASTS` |
| Accent | `A LANCE EACH BAR`, `TWO LANCES A BAR`, `A LANCE EVERY BEAT` |
| Echo | `SHOTS ECHO A BEAT LATER`, `TWO ECHOES` |
| Wave | `WAVY SHOTS HIT WIDER`, `WIDER WAVES` |
| Rear Guard | `ALSO FIRE BACKWARD`, `AND SIDEWAYS` |
| Arc | `KILLS ARC TO 1 MORE`, then the number that stack reaches |
| Burn | `HITS IGNITE`, `HOTTER FIRE`, `FIERCE FIRE` |
| Shatter | `KILLS THROW SHRAPNEL`, `MORE SHRAPNEL` |
| Gravity Rounds | `HITS PULL ENEMIES IN`, `A STRONGER PULL` |
| Afterimage | `DASH LEAVES A DECOY`, `DECOY LASTS LONGER` |
| Reserve | `+1 DASH CHARGE` |
| Shockstep | `DASH: A DAMAGING RING`, `A BIGGER RING` |
| Aegis | `A SHIELD BLOCKS BULLETS`, `A WIDER SHIELD` |
| Last Light | `WHEN HIT: A FREE NOVA` |
| Slipstream | `+15% SPEED` |
| Prospector | `+25% EXTRA SHARDS`, then `+50%`, `+75%`, `+100%` |
| Nova Stock | `+1 NOVA` |
| Encore | `BOOSTS LAST +1 BAR` |
| Lucky Break | `ONE MORE CHOICE` |
| Collector | `A BOOST PER 500 SHARDS` |
| Syncopation | `OFF-BEATS PIERCE, +50%`, `OFF-BEATS +100%` |
| Bar Line | `A LASER CROSS EACH BAR`, `AND DIAGONALS`, `LONGER LASERS` |
| Crescendo | `DAMAGE CLIMBS WHILE UNHIT`, `CLIMBS TWICE AS HIGH` |
| Kick Pulse | `KICK DRUM SHOVES ENEMIES`, `THE SHOVE HURTS`, `A WIDER SHOVE` |
| Afterparty | `LONGER, FASTER SURGE` |

| Riff | Level 1 | Levels 2 to 5 |
|---|---|---|
| Lancer | `NEW RIFF: A BEAM EACH BAR` | `LEVEL 2: WIDER BEAM`, `LEVEL 3: TWO BEAMS A BAR`, `LEVEL 4: WIDER STILL`, `LEVEL 5: HEAVIER BEAM` |
| Minelayer | `NEW RIFF: A MINE A BAR` | `LEVEL 2: TWO MINES`, `LEVEL 3: THREE MINES`, `LEVEL 4: FOUR MINES`, `LEVEL 5: FIVE MINES` |
| Storm Coil | `NEW RIFF: RAPID ZAPS` | `LEVEL 2: TWO TARGETS`, `LEVEL 3: THREE, WIDER`, `LEVEL 4: FOUR TARGETS`, `LEVEL 5: FIVE, WIDER` |
| Swarm | `NEW RIFF: HOMING MISSILES` | `LEVEL 2: EIGHT MISSILES`, `LEVEL 3: TEN MISSILES`, `LEVEL 4: TWELVE MISSILES`, `LEVEL 5: SIXTEEN MISSILES` |
| Singularity | `NEW RIFF: A GRAVITY WELL` | `LEVEL 2: WIDER WELL`, `LEVEL 3: HEAVIER BLAST`, `LEVEL 4: WIDER STILL`, `LEVEL 5: HEAVIER STILL` |
| Mirror | `NEW RIFF: A WINGMAN AT 50%` | `LEVEL 2: WINGMAN 60%`, `LEVEL 3: WINGMAN 70%`, `LEVEL 4: WINGMAN 80%`, `LEVEL 5: WINGMAN 100%` |
| Comet | `NEW RIFF: A BOOMERANG DISC` | `LEVEL 2: BIGGER DISC`, `LEVEL 3: TWO DISCS`, `LEVEL 4: BIGGER STILL`, `LEVEL 5: THREE DISCS` |
| Sweeper | `NEW RIFF: A LASER ARM` | `LEVEL 2: LONGER ARM`, `LEVEL 3: SECOND ARM`, `LEVEL 4: LONGER STILL`, `LEVEL 5: FASTER ARMS` |
| Flak | `NEW RIFF: PELLET BURSTS` | `LEVEL 2: MORE PELLETS`, `LEVEL 3: MORE STILL`, `LEVEL 4: TWICE AS MANY`, `LEVEL 5: PELLETS EXPLODE` |

| Boost | Words | | Fusion | Words |
|---|---|---|---|---|
| Supernova Rounds | `SHOTS EXPLODE` | | Helix Array | `SHOTS BRAID A HELIX` |
| Colossus | `GIANT, RAMS THROUGH` | | Standing Wave | `LANCES STAY A BAR` |
| Time Stretch | `THE WORLD AT HALF SPEED` | | Tesla Bloom | `RINGS ARC LIGHTNING` |
| Freeze Frame | `FREEZE AND SHATTER` | | Shard Halo | `ORBITALS FLING SHRAPNEL` |
| Black Hole | `PULLS ALL IN, THEN BLOWS` | | Downbeat Cannon | `A GIANT BEAM EACH BAR` |
| Chain Storm | `LIGHTNING EVERY BEAT` | | Collapse Field | `MINES PULL IN` |
| Chorus | `TWO GHOST SHIPS` | | Murmuration | `A HOMING FLOCK` |
| Beam | `A SWEEPING LASER` | | Quartet | `THREE WINGMEN` |
| Ricochet Fever | `+5 BOUNCES, FASTER` | | | |
| Confetti | `KILLS BURST INTO SPARKS` | | | |
| Fractal | `HITS SPLIT SHOTS IN TWO` | | | |
| Carousel | `A BOMB EVERY BEAT` | | | |

**Boost cards (HUD).** An active Boost shows its icon, its name and its words above the Riff slots.
Two can be active, and their cards are laid out from what they say, not at fixed places: side by
side, centred as a pair, with 48 px between the nearer ends of their text (the name or the words,
whichever is wider). Part 3's pictures had `SUPERNOVA ROUNDS` running into `CHAIN STORM` and
`FREEZE FRAME` into `RICOCHET FEVER`. A pair wider than 900 px drops its words and keeps icons and
names.

**Boss hint (HUD, M5).** Every boss has one line that says its rule in plain words (GAME_DESIGN 7.6).
It sits under the boss's name in the HUD's small size, in white at 0.8, for the first four bars of
the fight, fading over the last bar; it comes back for two bars whenever this player's shots have
met only armour for two bars running, at most once every eight bars. At most 26 characters, the
words' rules as for upgrades. In co-op each player sees it by their own shots.

| Boss | Hint | | Boss | Hint |
|---|---|---|---|---|
| Kaleidos | `HIT THE ARMS NOT FIRING` | | Feedback | `STOP FIRING TO OPEN IT` |
| Longwave | `SHOOT THE WHITE SEGMENT` | | Cadence | `SHOOT THE SILENT ONE` |
| The Choir | `SHOOT THE ONE THAT SINGS` | | Arpeggiator | `SHOOT IT AS IT LOADS` |
| Event Horizon | `STARVE IT: KILL ITS FOOD` | | Fata Morgana | `SHOOT HER OVER THE WATER` |
| Polyrhythm | `SHOOT THROUGH THE GAPS` | | Kaleidos, Reprise | `SHOOT THE ARMS OFF` |

A mini-boss whose rule decides whether it can be hurt gets a line the same way (2026-11-13). The other six
(Alpha Hound, Carom King, Moth Queen, Blink Star, Mirror Mimic, Weaver Prime) can be hurt anywhere at any
time and need none.

| Mini-boss | Hint |
|---|---|
| The Hive | `SHOOT THE QUEEN BEHIND IT` |
| Battering Ram | `SHOOT IT AFTER IT CRASHES` |
| Brood Mother | `KILL HER BROOD FIRST` |
| Sentry Array | `SHOOT THE ONE WITH NO DOME` |
| Twin Tether | `DASH THROUGH THE BEAM` |
| Deep Well | `OVERFEED IT, THEN STRIKE` |

**Extra Life (HUD, play test 3).** A held Extra Life is a small gold ship outline to the right of the
Nova stock. Picking it up and spending it both show `EXTRA LIFE` as world text at the ship (cap 22,
gold) for one bar. Its orb is a gold ship outline in a ring, the Boost orb's size.

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
| What ended it (M6) | One line under the four numbers, cap 14 | `ENDED BY` in the accent and the name of what took the last cell in white, as the game names it (`CAROM`, `LONGWAVE`, `A VOLATILE BLAST`, `METEOR RAIN`); an enemy's bullet names the enemy. Nothing when the run ended by finishing the set |
| **Set list** | A bar across the width | The run drawn as the song it was: one segment per Track, as long as the time spent in it, in that Track's accent colour with its name under it. A white star where a boss fell, a gold tick for each upgrade taken, a danger-coloured cross for each hit taken. It plays back as a ten-second medley of the run while the screen is up |
| Your band | Under the set list | The icons of the Riffs and the most-stacked Mods the run ended with |
| **RETRY** | Bottom right, 600 × 140, label 52, player colour | One tap: a new run ("Transitions": the drop-in, from game over) |
| HOME | Under it, 330 × 90 | Back to the title ("Transitions": home) |

**A finished run (M6).** When the tenth Track's boss falls the run has been won, and the game says so
before the loop begins: the Track name card's place shows `SET COMPLETE` (cap 44, gold) with the score
so far under it for two bars, the band plays the tonic over the boss's death, and the loop's own card
(`ENCORE`) follows. If that run later ends, its game over reads `SET COMPLETE` in gold where `RUN
OVER` stands, and the high-score table marks it with a gold star.

In co-op the score is the team's, each player's own score sits in their colour under it, ranked, and
the set list carries each player's crosses in their colour. In detail:

- Under the score one line, cap 14: `TEAM`, then `TIME` and `TRACK` with their values (labels in the
  accent, values white). It stands in for the row of four numbers, which a team does not have.
- The players are a table, not centred lines: rank, name (`P1 (YOU)`), score, Chain, kills, each a
  column; names left-aligned, numbers right-aligned, cap 22, every row in its player's colour. The
  table is centred as a whole.
- The run behind the screen is dimmed to a fifth, as in the pause, and no beacon or beacon flare is
  drawn: every ship is down, and their rings would stand in the middle of the scores.
- RETRY is the host's. The others see it disabled with `WAITING FOR HOST` under it; HOME is everyone's.

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
count-in is shared. Under `PAUSED` a line in cap 16 says so: `FOR EVERYONE IN THE ROOM`. The run line
reads the team's score, the Track and the time; RESTART is not offered and QUIT RUN reads `LEAVE RUN`.

**Pause-on-leave (co-op, 2026-11-14).** When a player's app leaves the screen (home, a call, the lock), the room pauses
for them on the next sixteenth and waits up to a minute. Every screen shows the pause with, in place of RESUME,
`WAITING FOR <NAME>` (cap 30, the player's colour) over `THEY LEFT THE GAME. THE RUN GOES ON WHEN THEY ARE BACK` (cap 16);
nobody can resume meanwhile. The one who left comes back to `BACK: GETTING IN TIME WITH THE ROOM` while their clock and
music settle (about a second), then the shared count-in starts by itself. Away longer than the wait, the room drops them
and plays on; back, they go home with `AWAY TOO LONG: THE RUN WENT ON WITHOUT YOU` under PLAY (the host:
`AWAY TOO LONG: THE ROOM CLOSED`). A pause someone asked for before the away stays theirs to lift.

## Co-op (M4)

Three screens lead into a co-op run: the local network notice (once), the join screen, the lobby.
**Nothing of a run is drawn behind them** — no attract ship, enemies, shots, rings or Shards; the
lattice at rest and the background only. The title has the attract run because it has four words on
it; these screens are read, typed on and tapped.

### Local network notice

Shown once, before the first join or host, and again if the permission was refused.

| Element | Place | Behaviour |
|---|---|---|
| `LOCAL NETWORK` | Top centre, cap 60, accent | — |
| The reason | Centred, three lines, cap 18, white at 1.2 | `TO PLAY TOGETHER, RAVEFORM TALKS TO THE OTHER PHONES ON YOUR WI-FI OR HOTSPOT: IT LISTENS FOR THEIR ROOMS AND SENDS THE GAME TO THEM. NOTHING LEAVES YOUR NETWORK. ANDROID WILL ASK YOU NEXT.` |
| **CONTINUE** | Centre, 600 × 130, player colour | Opens the system's question. Refused: the reason's last line becomes `RAVEFORM CANNOT SEE OTHER PHONES WITHOUT IT. YOU CAN ALLOW IT IN ANDROID'S SETTINGS.` and CONTINUE asks again |
| BACK | Bottom left, 300 × 90 | — |

### Join

| Element | Place | Behaviour |
|---|---|---|
| `CO-OP` | Top centre, cap 60, accent | With the network hint under it (below) |
| **Rooms on this network** | Left column, 132 to 1140 px; up to four rows of 1008 × 90, 22 px apart | **One row for each room, however many times it was heard.** Code (gold), the host's name, seats taken (`2/4`), left-aligned with 40 px of padding. A row's outline is the host's colour. A full room or one that has started is drawn disabled with `FULL` or `PLAYING`. No room heard: one dashed row, `LOOKING FOR ROOMS` with three dots filling on the beat. A room not heard for 3 s leaves the list |
| Message line | Under the list, cap 16 | Refusals and errors, in amber (the danger colour is for what can kill you): `THAT ROOM'S GAME DATA DIFFERS FROM THIS ONE'S: UPDATE BOTH PHONES`, `THAT ROOM IS FULL`, `NO ROOM WITH THAT CODE`, `THE HOST LEFT: THE RUN IS OVER. YOUR SCORE IS KEPT` |
| **HOST A ROOM** | Under the message line, centred on the list column, 600 × 130, player colour | The obvious control |
| BACK | Bottom left, 300 × 90, at least 24 px under HOST A ROOM | — |
| **Room code** | Right column, cap 80, gold; four dashes while empty | Typed on the pad |
| Pad | Under it: sixteen keys in four rows, 112 px discs, cap 40 | The sixteen characters a code is made of: `3 4 6 7 9 A C D F H J K M P Q R` (none that reads as another). `DEL` and **JOIN** under the pad; JOIN is disabled until four characters are in |
| TYPE AN ADDRESS | Under them, 380 × 70, outline | For networks that carry no discovery: opens the system keyboard for an address |

### Lobby

| Element | Place | Behaviour |
|---|---|---|
| `CO-OP` | Top centre, cap 60, accent | — |
| **Room code** | Centred, cap 120, gold | Four characters, with `ROOM CODE` in cap 14 above it. Every seat sees it, so a fourth player can be told it across the room |
| Network hint | Under the code, cap 13 | `SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT` |
| **Seats** | Four rings of 130 px in a row | A taken seat: ring in that player's colour field, glass, a pool, the ship with its halo, name and state under it. Joining: ring at 0.8, no glass, ship at a third. Open: dashed white ring |
| Band line | Under the seats, cap 12, accent | `EACH PHONE IS AN INSTRUMENT: SET THEM DOWN TOGETHER AND THE ROOM IS THE BAND` |
| **START** | Bottom centre, 600 × 130, host only | Starts when every seated player is ready (disabled until then); the others see `WAITING FOR HOST` in its place |
| LEAVE | Bottom left, 300 × 90 | — |

Each player hears their own lead when they tap their seat, so a group can tell whose phone is whose
before the run starts. Your own seat says so under its state: `TAP TO HEAR YOUR LEAD`.

### In the run

- The HUD's edge chevrons (teammates, beacons, Cells, events) keep clear of the HUD: where a chevron
  would land on the score block, the phrase bar, the Nova stock or the pause ring, it slides along the
  edge to the nearest free place, 20 px clear.

## First-run hints (M6)

The game explains itself once, in the world, while the player plays. No tutorial screen, no pause.

- A hint is one line of world text 70 wu above the ship (cap 22, white at 0.85, the dark rim of
  the priority layer), up for two bars, fading over its last beat. At most 28 characters, plain
  words, the upgrade words' rules.
- Each hint shows once ever on a device (remembered with the settings; `RESET HINTS` under
  SETTINGS, CONTROLS brings them back). One at a time, at least four bars apart, never in a boss
  or mini-boss fight, never while a Trio's words or a boss's hint line are up, never in co-op for
  a player who has finished a run.
- A hint whose moment passes waits for the next such moment.

| When | Hint |
|---|---|
| The first run's first bar, touch | `LEFT THUMB MOVES` |
| The same, keyboard or gamepad | `WASD OR LEFT STICK MOVES` |
| The first enemy in view | `IT AIMS AND FIRES BY ITSELF` |
| Four bars later, touch | `RIGHT THUMB AIMS IF YOU LIKE` |
| The first Trio | `FLY INTO ONE AND STAY` |
| The first hit taken | `YOUR RING IS YOUR HEALTH` |
| Twelve enemies within 300 wu and a Nova in stock, once | `NOVA CLEARS THE SCREEN` |
| The first bullets in view | `DASH GOES THROUGH BULLETS` |
| The first Boost orb in view | `GOLD ORBS ARE SHORT POWERS` |
| The first Extra Life in view | `EXTRA LIFE: ONE MORE TRY` |
| The first Vault in view | `BREAK IT FOR A PRIZE` |

## App icon and name (M6)

- **Icon:** the player's ship glyph, nose up and to the right at 30 degrees, in P1 cyan with its
  white core line, inside its halo of four cell arcs, on the Ultraviolet palette's `bg1` to `bg0`
  radial ground with the lattice's lines at rest behind it, bulging a little under the ship. No
  text. Adaptive icon: the ground and lattice are the background layer, ship and halo the
  foreground, the ship's extent 44% of the icon so every mask keeps the halo. Drawn from
  `design/glyphs.json` and `design/palettes.json` by a script, with the bloom of look.md section 7
  at half strength; a monochrome layer (ship and halo, no ground) for themed icons.
- **Name:** `RAVEFORM` under the icon as the system draws it. The title screen is the name screen.

## Credits (M7)

`CREDITS` is the last row of SETTINGS. One screen in the title's style, scrolling by drag or stick,
BACK top left. Cap 22 for headings in the accent, cap 14 for lines in white.

| Heading | Lines |
|---|---|
| `RAVEFORM` | `MUSIC, PICTURES AND CODE MADE FOR THIS GAME` |
| `BUILT ON` | `SDL 3  (ZLIB LICENCE)`, `OBOE  (APACHE 2.0)`, `ANDROID FRAME PACING  (APACHE 2.0)`, `ENET  (MIT)`, `VOLK  (MIT)`, `VULKAN HEADERS  (APACHE 2.0, MIT)`, `LIBC++  (APACHE 2.0, LLVM EXCEPTION)` |
| `LICENCES` | Each library's licence text in full, cap 10, as the library ships it (read from the fetched sources at build time, never retyped) |
| `PLAY SAFE` | The first-launch notice's photosensitivity text again |

The list is whatever the build really links: the build fails if a linked third-party library has no
entry.

## Transitions (M6)

(2026-11-14, the owner: going from the title into the first Track and from game over back to the title
"are a little jarring and sudden with poor UX". As built, PLAY cut the title away in one frame and left
the bot's run on screen at full light until the bar line, which then cut to an empty new run; HOME put
the title over the dead run, which cut to the bot's new run a bar later.)

Rules for every transition:

- **Nothing cuts.** Everything that leaves fades, slides or streaks out; everything that arrives
  draws itself in. No frame shows two screens' controls at once, and no frame is a jump.
- **Lengths are in beats**, so they follow the tempo, and every change of what is on screen lands on
  a beat. The run itself only ever starts or ends on a bar line.
- **A tap is answered at once** (pressed state, sound, haptic tick) even though what it asks for waits
  for the beat. A second tap on the same control does nothing. The control that was tapped stays lit
  until its screen has gone.
- **The music carries it**, as a DJ would: the bed never stops, and a new section enters on the
  downbeat with a crash, never mid-bar.
- **Reduce flashing:** the flashes and the lattice ring below are left out; the fades stay.

### The drop-in (title → the run's first bar)

PLAY is tapped. The run starts on the first bar line at least two beats away (two to six beats, one to
three seconds at 120 BPM): **the wait**.

| When | What is seen | What is heard |
|---|---|---|
| The tap | PLAY takes its pressed state and keeps it; a ring leaves it at once (64 px in half a beat): the tap is answered. Its four beat dots become the count (as many lit as beats are left). | The confirm chord |
| The wait's first beat | CO-OP, SETTINGS, their captions, the best score and the note fade and sink 30 px; the motes fall back into the rule. The bot's run behind dissolves: its enemies and shots fade out over the wait and its ship is gone; nothing new spawns. | The band's high end filters down (a low-pass closing over the wait) and a one-bar riser starts under it, timed to end on the bar line |
| The beats between | Title v2: the fan folds down flat onto the rule by the last beat (in a two-beat wait over the first), and **the tubes run hot**: core 4 -> 7 px, tube 14 -> 17.5 px, level + 40%; the rule's ends stand at 1.5 x. | The riser |
| The last beat, first half (from here Back does nothing) | The tagline goes and the rule closes its gap in an eighth. **The word falls into its line**: every point moves toward the rule, its height 1 -> 0, easing in, its depth with it; the rule takes its light (1 -> 2.2, white) and its ends flatten. | The drums drop out for the last beat, as in a Build's last bar |
| The last beat, second half | The rule draws in from both edges to its centre (easing in) while it slides from y 432 to the frame's centre; PLAY, still lit, shrinks to nothing and rises into the same point. On the bar line one point of light stands at the frame's centre. | The riser's tail |
| **The downbeat** | **The point bursts:** a clamped flash pool (radius 1,000 at 0.3 for 120 ms), eight rays, one a letter in that letter's colour in the field, leaving it from 60 to 1,360 px over half a beat; the lattice lights from the ship to the edges over one beat, one impulse of 140; the ship draws itself at the point with its halo over the beat. Reduce flashing: no flash, no rays, no lattice ring. | A crash, and the run's first Groove from its first kick (Track 1's bar 1, never mid-phrase) |
| Beat 2 | The HUD fades in over one beat. The Track name card (`TRACK 1  FIRST LIGHT`) writes itself as it always does. The first-run hint, if it is due, waits for bar 2. | — |
| The first two bars | **A grace**: nothing spawns in the first bar, and for two bars nothing hostile spawns within 600 wu of the ship. The first formations come in from the edges. | — |

The ship can be moved from the downbeat. Back during the wait cancels it: PLAY lets go, everything
fades back in over a beat, the riser stops and the filter opens.

**RETRY** from game over is the same drop-in. In its wait the game-over screen leaves as in "Home"
below (step 1), the dead run is already dimmed; in the last half beat the lit control (RETRY, the pause's RESTART, the lobby's
START) shrinks into the point at the frame's centre, and the downbeat is the same burst (Title v2). **Co-op:** when the host's START or RETRY lands,
every peer plays the drop-in from its own screen on the same bar line.

### Home (game over → title)

HOME is tapped (or Back). The title is built over the next bar; the bot's run behind it starts on the
bar line after.

| When | What is seen | What is heard |
|---|---|---|
| The tap | HOME takes its pressed state and keeps it; RETRY fades to its disabled look. | The back chord |
| The next beat | **The game-over screen leaves in one beat:** the score and `RUN OVER` streak out to both sides; the four numbers, `ENDED BY`, the set list and the band fade and sink 30 px. The dead run behind fades from its fifth to nothing, leaving the background gradient. | The set list's medley ends on this beat. The bed carries on filtered |
| The bar line | The palette cross-fades to Track 1's (Ultraviolet) over one bar, as a Track change does. Title v2: the rule is thrown from its centre to the edges in half a beat and its ends wake; on the bar's second beat the fan opens and the word is played, a letter a sixteenth (two beats), each struck as in the intro, with the same eight notes. | Track 1's title bed enters with a crash, its filter opening over the bar |
| The bar's last beat | The tagline opens the rule; PLAY rises into place (taking a tap from then), then CO-OP and SETTINGS with their captions, the best score and the note. | — |
| The next bar line | The bot's new run starts empty behind the title and fades up to the title's third of its light over a bar. | — |

A tap on PLAY while the title is still building starts the drop-in from where the title is; its wait
is still at least two beats.

### Every other way in or out of a run

Each takes one of the two above, so nothing cuts anywhere: the pause's RESTART is RETRY's drop-in
(the pause's dim and filter lift over its wait); QUIT RUN, co-op's HOME and LEAVE RUN are home; a
peer whose host has left goes home, and the host's message (`THE HOST LEFT`) stands under PLAY
for two bars. The co-op lobby's START is the drop-in from the lobby, whose screen leaves as the
title's controls do.

### Death → game over (unchanged, for the record)

The last hit, the ship's break and the game-over sound; the run goes on behind, dimmed to a fifth
over the next beat; the score starts counting up after a breath. This one is not changed by the rule
above; it is named here so that every way in and out of a run is on this page.
