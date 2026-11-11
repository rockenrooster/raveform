# RAVEFORM — Scenes for milestone M1

What the renderer milestone puts on screen, and why. `docs/TECH_SPEC.md` §9 fixes the loads the
benchmark scenes must carry; this fixes what they are made of, so that the numbers are measured on a
picture that looks like the game and so that the picture can be judged. The engine has no simulation
yet: every scene here is scripted motion.

All scenes use the Reef palette (Track 3) unless `--palette` says otherwise, the Signal look
(`design/look.md` §8) unless `--look` says otherwise, and a zoom of 1 (1600 × 720 wu on a 20:9 screen).

| Scene | Kind | Purpose |
|---|---|---|
| `frame_reef`, `look_card` | Still, replayed from `design/scenes/*.rfscene` | Is the renderer's picture right? (`design/look.md` §9) |
| `glyph_sheet` | Still, drawn by the instanced glyph path | Are the shapes and their colour rules right? |
| `bench_mid` | Moving, scripted | Minute five: the load the game carries most of the time |
| `bench_endgame` | Moving, scripted | The worst minute the game can produce: the performance contract is measured here |

Stills are drawn with every time-driven effect off. Moving scenes advance by real time when they are
benchmarked and by a fixed 1/60 s per frame when a screenshot or frame strip is asked for, so pictures
are repeatable.

---

## 1. `glyph_sheet`

Every glyph in `design/glyphs.json` in rows by family: tier I to V left to right at 2.5× size (35 wu
radius), and under each one the same glyph at game size (14 wu × the glyph's `size`). Hull, ornament
and core mark follow `design/look.md` §2 and §3 with hue 1 for the first six families, hue 2 for the
next six and hue 3 for the rest. After the families: the three
variant marks on a Hound and on a Drifter, one elite of each of four families (white core, aura,
glass), the ship with each halo from one to six cells in all four player colours, a Shard and an
enemy bullet (scene and threat layer). Family names in cap height 12 at the left. No lattice, plain
background gradient. Compare by eye with `design/previews/glyphs_1.png` and `glyphs_2.png`.

---

## 2. The stage shared by both moving scenes

- **Clock.** A simulated song at 132 BPM (beat 0.4545 s, bar 1.818 s). Kick on every beat, snare on
  beats 2 and 4, hats on the off-beat eighths, bass on the off-beat eighths. These drive the beat
  envelopes of `design/look.md` §7 exactly as the audio engine will.
- **Ship.** Flies a slow figure of eight through the middle of the arena:
  `centre + (1400 × sin(2πt / 41), 700 × sin(4πt / 41))` wu. It turns to face its velocity; its aim
  swings ±35° around its heading every 7 s. Three Shield cells (`halo_3`), trail, pool.
- **Camera.** Follows the ship with 0.25 s of look-ahead along its velocity. Zoom breathes
  1.0 ± 0.1 over 29 s. Kick zoom punch and shake as in the look bible.
- **Lattice.** World-locked, so it scrolls under everything; every kill and every kick disturbs it.
- **Formations are anchored to the ship**, with their own motion on top, so the number of things in
  view stays what the table says while the world moves underneath.
- **Kills.** A killed enemy bursts by the recipe for its size (`design/look.md` §5), drops Shards that
  fly to the ship in 0.6 s, and is replaced at once: a warp-in ring (sized to the glyph,
  `design/look.md` §5) at a fresh place in its formation for exactly two beats, then the enemy. Counts
  therefore hold.
- **Seed.** Everything random comes from one generator seeded with 1.

---

## 3. `bench_mid` — 500 enemies, moderate effects

| Family | In view | Formation (wu, relative to the ship) | Tiers | Hue |
|---|---|---|---|---|
| Shoal | 220 | Two streams of 110, each flowing along its own slowly turning figure of eight across the view at 190 wu/s, 18 wu apart along the curve with ±20 wu of scatter | I–II | 1 |
| Drifters | 110 | Scattered over ±900 × ±450, each wandering a small loop of 30–60 wu every 5–9 s | I 60%, II 30%, III 10% | 1 |
| Hounds | 90 | A ring 260–620 wu out, spiralling inward at 60 wu/s and re-entering at the rim | I–III | 2 |
| Flankers | 40 | Two arcs of 20 circling at 420 wu in opposite directions | I–II | 2 |
| Rams | 20 | Pairs crossing the view in straight dashes: one beat of telegraph chevrons, then 0.6 s at 900 wu/s | II–III | 2 |
| Caroms | 20 | Bouncing inside a 1500 × 650 box at 260 wu/s with a short motion streak | I–III | 3 |

Two of them are elites (one Hound, one Carom). Three tier-III Sentries stand at 120° intervals 520 wu
from the ship, each turning three arms of 16 bullets once every four bars: **150 enemy bullets**.

Per beat, on sixteenth-note steps: 8 small kills and 1 medium; one large kill every second bar.

The player: shots on the sixteenth grid in three lanes; Orbitals ×3; a Pulse Ring every second beat
growing to 250 wu in 0.4 s; a Sentinel shot on each off-beat. One gravity well drifts 300 wu from the ship.

---

## 4. `bench_endgame` — the worst sustained minute

About **700 enemies and 1,500 enemy bullets in view**, with the other enemy slots up to the pool cap
(3,072) alive outside the view so that culling and far-mode updates carry their real cost.

| Family | In view | Formation | Tiers | Hue |
|---|---|---|---|---|
| Shoal | 260 | Three streams; on the first beat of each bar they snap into a spear aimed at the ship, then scatter back | II–IV | 1 |
| Hounds | 110 | Two packs charging through the ship's position in waves, one wave per bar | II–IV | 2 |
| Drifters | 90 | Scattered, wandering | II–V | 1 |
| Wyrms | 72 | Six chains of a head and eleven segments on sine paths through the view | III | 2 |
| Moths | 36 | Four rings of nine spiralling around the ship between 300 and 360 wu | II–IV | 2 |
| Caroms | 30 | Bouncing, with streaks | II–IV | 3 |
| Tethers | 24 | Twelve pairs 160 wu apart, each pair joined by a lethal beam (danger colour, scene and threat layer), slowly rotating | III | 3 |
| Blinks | 16 | Jumping on the beat along a five-point star 400 wu across; flares | III–IV | 3 |
| Weavers | 12 | Curving paths, each leaving a lethal light trail two bars long (40 segments, scene and threat layer) | III | 3 |
| Sentries | 10 | A ring 480–560 wu out, variant B marks, turning bullet arms | III–IV | 3 |
| Boss stand-in | 48 glyphs | A wheel of eight mirrored arms 420 wu across, each arm six nested squares shrinking outward (`carom_3`), turning once every four bars, 600 wu from the ship. It is assembled from existing glyphs and is not a boss design |

Twenty of the enemies are elites (white core, aura, glass, the charging ones with flares).

Enemy bullets: Sentry arms 10 × 3 × 20 = 600; boss fans of seven from each arm on every eighth note,
about 450 alive; Moth petals on eighth notes, about 180; Hound bursts of three on beat 1, about 120;
loose spirals 150.

Per beat: 40 small kills, 6 medium; 2 large kills per bar. Three gravity wells.

The player has a full build: shots on the thirty-second grid in five lanes plus a mirrored wingman;
Orbitals ×5; a double Pulse Ring every beat; Sentinel bursts of three on off-beats; a Lancer beam
across the whole view every second beat (6 px wide, intensity 4, 150 ms); a Sweeper arm 600 wu long
turning once every two bars. About 600 Shards in flight.

Events, to keep the peaks in the measurement:

- **Drop burst every 8 bars:** bloom +60% decaying over 400 ms, a white front from the ship to 1400 wu
  in 0.35 s with its distortion ring, the zoom snapping 1.1 → 0.9 and easing back over a bar, 2,000
  confetti sparks falling in from the screen edges, the palette accent swapped with the third enemy hue.
- **Boss phase break every 16 bars:** 300 sparks, the clamped white flash, the 30 px distortion ring,
  6 px of shake.
- **Build bar before each Drop:** colours drain 40% toward white, the vignette tightens to 0.40, the
  lattice stiffens (impulses halved), then the burst.

---

## 5. What to hand back for art direction

For each stage of M1 the lead reviews stills; nobody on this project can see motion, so motion is
reviewed as strips. All at 2400×1080, Signal look unless noted, written under `captures/m1/`:

| Picture | From |
|---|---|
| `look_card`, with bloom and with bloom off, plus the `compare` output for each | Desktop |
| `frame_reef` plus its `compare` output | Desktop, and the Moto's in-app capture |
| `glyph_sheet` | Desktop |
| `bench_mid` and `bench_endgame` at frame 600, in each of the three looks | Desktop |
| `bench_endgame` at frame 600 in the Ultraviolet and Furnace palettes | Desktop |
| A strip of eight consecutive frames (every frame, not every Nth) of `bench_endgame` across a Drop burst, and one across a quiet bar | Desktop |
| `bench_endgame` at frame 600 | The Moto's in-app capture, and the 12R's |

What will be looked for: threats legible over the brightest thing on screen; the ship findable in
under a second; darkness and luminance as below; no banding in dark glows; no beads on rings; no
shimmer or crawl between consecutive frames in a strip; the three looks clearly different from each
other.

Light, measured with `python design/tools/scenes.py light <png> ...` on Signal frames outside a Drop
burst ("dark" = brightest channel under 48/255):

| Frame | Dark pixels | Mean luminance |
|---|---|---|
| `frame_reef` (the target frame, for scale) | 35% | 0.076 |
| `bench_mid` | at least 25% | at most 0.11 |
| `bench_endgame` | at least 15% | at most 0.16 |

`design/previews/frame_endgame_ref.png` is what `bench_endgame` should resemble; it measures 17% and
0.148. An earlier version of this page asked for 40% dark at every density. That number was written
before anyone had drawn 700 enemies and 1,500 bullets: their lines and the lattice alone cover more
than that.
