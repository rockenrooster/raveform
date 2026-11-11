# RAVEFORM — Progress

## Contract change

- 2026-11-10: at the user's request the frame-rate contract moved from the 99th to the 95th percentile:
  P95 present-to-present interval within the device's limit (16.9 ms at 60 Hz, 11.3 at 90, 8.6 at 120),
  at most 5% of frames missing vsync, no frame over 50 ms (PLAN §5). P99, the missed-vsync percentage
  and the maximum are still recorded.
- 2026-11-10: the contract is judged on the panel side (when each frame reached the display, counted in
  whole refresh periods); the present-call interval is reported, not graded (PLAN §5).

## M1 Look and renderer — status 2026-11-11 (evening)

Step 1 (stills) **done and accepted** (re-run below, unchanged). Step 2 (motion) **built** and now on
look.md v2.1: the afterglow is the soft tail of §7, the light rules of §3 hold, the endgame carries the
load of scenes.md §4 in view. Step 3 (proof): **`bench_endgame` meets the contract on the Moto at Low,
Medium and High** (10-minute soak, then 120 s: 0 missed frames on the panel in each; GPU work at the top
clock 11.05 / 12.21 / 14.28 ms at P95 against the 12 ms budget). **Open:** the endgame
frame 600 still misses the light targets of scenes.md §5 after look.md v2.2 (14.0% dark, luminance
0.1645 against at least 15% and at most 0.16; a quiet bar meets both); reported to the lead with the
breakdown, nothing else tuned. The 12R and the tablet are not attached: every item for them is pending.
The look is Signal (user decision 2026-11-11); Laser and Haze are developer switches and were not re-shot.

### Look bible v2.2 built (2026-11-11, evening; the lead's `39d63a0`)

- **Afterglow on the light budget** (look.md §3, §7; `look.json` `afterglow.gain = "swarm"`, read at
  start-up): the composite adds `0.7 × swarmGain × max(previous × e^(−dt/τ) − source, 0)`; knee, τ and the
  source level unchanged. At frame 600 (swarm gain 0.338) it now adds 0.0076 of luminance (was 0.020).
- **Shot streaks** (look.md §3 player shots; `look.json` `shotStreak`): every shot (player, wingman,
  Sentinel) is a capsule from its head back over speed × 1/60 s + 8 wu, never under 16 wu; the player's
  shots fly at GAME_DESIGN §4's 1,400 wu/s (was 1,500): 31 wu capsules, Sentinel shots 27 wu. Behind
  every shot the afterglow is now one even streak: `captures/m1/afterglow_shot_tail_mid600.png` (top:
  frame 600; bottom: without the afterglow), against `afterglow_shot_beads_mid600_lanes.png`, the same
  crop before v2.2, where each lane is a row of dots. Correction: the earlier
  `afterglow_shot_beads_mid600.png` crop (left of the ship) shows mostly a stream of **Shards** flying to
  the ship, not shots; Shards still leave soft dots (`afterglow_shard_beads_mid600.png`, same crop as that
  one, on this build). v2.2 does not cover them.
- **Drop confetti as a shower** (look.md §5; `look.json` `dropConfetti`): each spark starts 0-200 wu
  beyond the view edge (spread along the edges by their length) and is born 0-250 ms after the Drop
  (stateless birth time in the future), 300-700 wu/s toward the ship, 1.0-1.6 s. The streak direction was
  checked before the change and was already the spark's own velocity (the particle shader streaks along
  `v0·e^(−drag·t)`; at the top-left edge the streaks ran at about 31° down to the right, at the top-right
  about 37° down to the left, on the left edge level: each toward the ship); the "same slant" was the
  sparks all born on the edge line in one frame. The new `strip_bench_endgame_879_*` shows no border in
  any frame; the shower comes in over the following half second.

| `scenes.py light` (desktop, Signal, Reef) | Required | Before v2.2 | **v2.2** |
|---|---|---|---|
| `bench_endgame` frame 600, High | ≥ 15%, ≤ 0.16 | 13.3% / 0.1725 | **14.0% / 0.1645** (misses both) |
| `bench_endgame` quiet bar, frames 400-407 | ≥ 15%, ≤ 0.16 | 14.2-15.0% / 0.161-0.165 | 15.6-16.2% / 0.154-0.157 (meets) |
| `bench_endgame` frame 600, Low | | 28.1% / 0.149 | 30.6% / 0.140 |
| `bench_mid` frame 600 | ≥ 25%, ≤ 0.11 | 35.1% / 0.083 | 36.1% / 0.081 (meets) |

Breakdown of the v2.2 frame 600 (luminance each class adds, one off at a time): hulls 0.052, pools 0.020,
kick pump 0.014, player 0.013 (the longer shots: +0.002), afterglow 0.0076, background 0.008, lattice
0.007, bullets 0.006, Shards 0.006, sparkles 0.004, sparks 0.003, bloom 0.071. Without the afterglow
16.0% / 0.157; without the pump 15.8% / 0.151. Frame 600 sits on a kick (all pumps at their peak).

Acceptance stills: identical (0.38 / 1.27 / 1.11 mean error, luminance 0.995 / 0.983 / 0.982, worst
probe 4). Desktop Debug `bench_endgame` 60 s at Ultra and Low: valid, 0 validation, 0 loader messages, 0
heap allocations (`captures/desktop_m1_endgame_*_60s_20261111-1748.log`). Moto, the lead's short check
(Low, 20 s warm-up, 30 s, deployed right before, lock taken): 0 missed of 1,797 on the panel, max 19.1 ms,
main thread 4.53 / 4.74 ms (P50 / P95), GPU at the governor's clock 13.36 / 13.68 ms (before v2.2 in the
contract run: 4.59 / 4.81 and 13.35 / 13.72), 0 heap allocations, thermal 0
(`ZY22BXMJG3-bench_endgame-20261111-174725.json`). Nothing got slower.

### This session (2026-11-11, afternoon)

- **Afterglow v2.1** (look.md §7) was already in the code the previous coder left (renderer, `afterglow.frag`,
  committed in `2976716`); the pictures that showed ghost rows (`captures/m1` of the morning, now in
  `captures/archive/m1-2026-11-11-morning/`) were older than it. Checked against §7 line by line: source =
  the bloom level of σ 6 ref px (level 3, W/8) less the 0.25 knee, history at that size re-aimed by the
  camera, `0.7 × max(previous × e^(−dt/τ) − source, 0)` added. Strips show soft tails and no line-sharp
  ghosts, but sparse fast shots still leave rows of soft dots (see "Light at endgame").
- **Light rules** (look.md §3) checked one by one against the code: small kills sparkle-flash only, no pool,
  no ring; ≤ 24 kill pools (oldest fades early); no tier auras; bullet and Shard sparkles High and up; ≤ 6
  shock rings; warp-in rings from 2.5 × the glyph's extent (elites 90 wu); gains as in the §3 table. Fixed:
  the boss stand-in drew an aura on each of its 48 parts (the target frame has none: they are ordinary
  glyphs, not a boss design); kill pools lived 1.2 s for every kill (now the recipe's longest spark, 0.9 /
  1.2 s, as in the target); the ship was 16 wu (glyphs.json: 14); Lancer and Sweeper as the target draws them.
- **Load in view** (scenes.md §4): the endgame had about 515 enemies and 560 bullets in view (rings of
  480–600 wu and ±900 × ±450 scatters do not fit a 720 wu tall view). Formations are now fitted to the 20:9
  screen (rings squashed vertically as the target frame does, packs charging edge to edge, scatters and
  boxes on screen, the boss 600 wu across and 180 wu up and down): **706 enemies and 961 bullets in view**
  on average over the Moto run (max 709 / 1,247; the target frame holds 699 / 1,137).
- **Boss stand-in** in the Caroms' hue (hue 3, violet in Reef). **Enemy bullets** upright diamonds
  (`enemy_bullet`: points on the axes, `facesHeading` false); see the deviation below.
- **`--cuts N`** applies look.md §10's cuts in order on top of a tier; Low is back to TECH_SPEC's Low with
  glass and 24 kill pools (dropping them are cuts 1 and 7). **`--reduce-flashing`** (key R): every amount of
  the §7 beat table × `beat.reduceFlashingScale`, no Drop burst. **First-launch tier probe** (TECH_SPEC 5.4,
  below). **`--mute sparkles|pools`** and `scripts\light-breakdown.ps1` for the light table;
  `scripts\review-m1.ps1` re-shoots the review set.

### Light at endgame (scenes.md §5; `python -I design\tools\scenes.py light`)

Desktop, 2400×1080, Signal, Reef, High (the picture default), frame 600 (fixed 1/60 s steps):

| Picture | Dark (< 48) | Mean luminance |
|---|---|---|
| Target `design/previews/frame_endgame_ref.png` | 16.7% | 0.148 |
| Required | at least 15% | at most 0.16 |
| Before (HEAD `2976716`, the lead's measurement reproduced) | 8.5% | 0.193 |
| **After** (this build) | **13.3%** | **0.1725** |
| After, a quiet bar (frames 400–407, `strip_bench_endgame_407_*`) | 14.2–15.0% | 0.161–0.165 |
| After, Low tier (desktop) | 28.1% | 0.149 |
| `bench_mid` frame 600 (required ≥ 25%, ≤ 0.11) | 35.1% | 0.083 |

Frame 600 is exactly on a kick (beat 22): the exposure (+8%), bloom (+10%) and enemy-swell pumps are at
their peak, and the beat's kills (ten small, two medium, one large) and the enemies killed two beats
earlier (finishing their warp-in) all flash on it. The target frame is a still at a quiet instant,
without pumps or afterglow.

Breakdown, frame 600, one class of light off at a time (`scripts\light-breakdown.ps1`, pictures in
`captures/light/`; before = HEAD `2976716`):

| Off | Before: dark / luminance | After: dark / luminance | After: luminance it adds |
|---|---|---|---|
| nothing | 8.5% / 0.193 | 13.3% / 0.1725 | — |
| lattice | 10.3% / 0.185 | 16.0% / 0.1656 | 0.007 |
| hulls (horde, elites, boss) | 11.8% / 0.142 | 18.0% / 0.1168 | 0.056 |
| enemy bullets' scene light | 13.4% / 0.187 | 17.5% / 0.1658 | 0.007 |
| sparks | 8.7% / 0.188 | 13.3% / 0.1690 | 0.004 |
| pools (all soft light ≥ 16 ref px) | 11.0% / 0.156 | 16.8% / 0.1527 | 0.020 |
| sparkles (all soft light < 16 ref px) | 9.4% / 0.186 | 14.1% / 0.1672 | 0.005 |
| Shards | 8.6% / 0.181 | 13.3% / 0.1647 | 0.008 |
| player (ship, shots, Riffs, trail, pool) | 9.0% / 0.182 | 16.4% / 0.1611 | 0.011 |
| **afterglow** | 12.1% / 0.166 | **16.7% / 0.1523** | **0.020** |
| background | 13.0% / 0.185 | 17.7% / 0.1643 | 0.008 |
| bloom | 32.3% / 0.097 | 40.3% / 0.0921 | 0.080 |
| priority layer | 7.0% / 0.198 | 10.8% / 0.1758 | −0.003 |
| kick pump (frame 600 is on the kick) | 10.0% / 0.178 | 14.8% / 0.1588 | 0.014 |

Before, the auras (20 elites' and the boss parts' 48) added 0.010 and the kill pools 0.023; before, the
endgame also held only about 515 enemies and 560 bullets in view. Without the afterglow the frame meets
both targets (16.7%, 0.152); without the pump it meets the luminance (0.159) but not the darkness (14.8%).
The lead's own model of the v2.1 afterglow (`crowd.py study`, the whole horde drifting 5 px a frame) adds
0.0085 to the target (15.4%, 0.157); in the engine it adds 0.020 because much of the light moves much
faster than that (shots 37 px a frame, the Sweeper arm, Shards, charging packs, the boss's fans spawning
on its turning arm tips) and the tail of each flash on the beat. Where its 0.020 comes from (afterglow
on minus off, with one class muted): the player's light 0.005, the horde 0.003, Shards 0.002, sparkles
0.002, the boss 0.001, bullets 0.001, sparks 0.000; the remaining 0.006 only exists where several classes
overlap. Blurred to σ 6 the dense middle of the endgame sits above the 0.25 knee almost everywhere, so
the tail is not limited to hot light at this density: 36% of the frame's pixels gain more than 8 levels
from it (`captures/light/afterglow_adds_x3.png`: what it adds, ×3). The knee is absolute while the horde's
light follows the crowd gain; whether to change the knee, the strength or the gains is the lead's call.
At Low (lattice every 32 wu, so its lines survive the σ 6 blur) the afterglow adds 0.020 too (28.1% /
0.149 with it, 34.5% / 0.129 without) and the lattice shows through its haze as a faint grid, cut out
where the static lines already hold the light (`captures/light/afterglow_adds_low_x3.png`).

**Ghost dots are not fully gone** (v2.1; see "Look bible v2.2 built" for what changed). A shot moved
37.5 px a frame and the source is blurred to σ 6 px, so where shots are sparse (`bench_mid`) each shot
left a row of soft dots about 37 px apart (`captures/m1/afterglow_shot_beads_mid600_lanes.png`). The
crop first offered as evidence, `afterglow_shot_beads_mid600.png`, shows mostly a stream of Shards flying
to the ship, which beads the same way. In the endgame the shots are dense and the dots merge into a haze
along the lanes. Anything moving faster than about 2σ a frame (shots, young sparks, Shards near the
ship) does it.

### Stills acceptance (`design/scenes.md`, `design/tools/scenes.py compare`)

Engine at 2400×1080, `--still --no-overlay`, desktop Debug with validation: 0 validation messages, 0
loader messages. References regenerated by the lead on 2026-11-11 with soft light, clouds and the
background gradient at pixel centres (the `.rfscene` files did not change).

| Still | Mean abs error (/255) | p95 / p99 / max | Linear luminance ratio | Soft-light ratio p10 / p50 / p90 | Worst probe | Pixels off by ≥ 16 |
|---|---|---|---|---|---|---|
| `look_card`, bloom off | 0.34 | 1 / 1 / 24 | 0.997 | 0.99 / 1.00 / 1.01 | 3 (`pool_16`) | 72 |
| `look_card`, bloom on | 1.22 | 4 / 8 / 24 | 0.984 | 0.93 / 0.98 / 1.00 | 3 (`tone_p1_2`) | 6981 |
| `frame_reef` | 1.01 | 3 / 4 / 20 | 0.984 | 0.97 / 0.98 / 0.99 | (no probes) | 1 |

Re-run after the motion work (background at W/4, interleaved-gradient dither, half-precision
composite): 0.35 / 1.23 / 1.05 mean error, luminance 0.997 / 0.984 / 0.983, soft light unchanged, probes
within ±3, 0 validation messages (2026-11-11 09:20). After the low-resolution terms moved into the last
bloom pass (below): 0.38 / 1.27 / 1.11, luminance 0.995 / 0.983 / 0.982, soft light p10-p90 0.98-1.00 /
0.93-1.00 / 0.96-0.99, worst probe 4 (`pool_16` blue), 0 validation messages (2026-11-11 09:45). All
inside the look.md §9 targets. **Re-run on this session's build (2026-11-11 17:00, `scripts\review-m1.ps1`):
0.38 / 1.27 / 1.11 mean error, luminance 0.995 / 0.983 / 0.982, soft light p10-p90 0.98-1.00 / 0.93-1.00 /
0.96-0.99, worst probe 4 (`pool_16`), 0 validation messages: unchanged, inside every target.**

History of these numbers:

1. First acceptance, against the first references (soft light and background sampled at integer pixel
   coordinates, so the engine reproduced that half-pixel offset): 0.34 / 1.22 / 1.01, probes within ±5.
2. The lead's run of that build against the regenerated references: 0.39 / 1.25 / 1.08, probes ±4.
3. Half-pixel reproduction removed from the engine (`shaders/glow.frag`, `shaders/composite.frag`): 0.34 /
   1.22 / 1.01 against the regenerated references.
4. Threat layer changed from straight alpha with the colour clamped to 1 to premultiplied "over" (the
   reference mixes and then clips, so a display-linear core colour above 1 still brightens the
   anti-aliased edge): threat-core edges were up to 24 levels too dark in red. Pixels off by ≥ 16 fell
   from 1030 to 72 (card, bloom off), 7939 to 6981 (card, bloom on) and 2167 to 1 (reef); the `kill_core`
   probe went from −5 to 0 / −1. Means unchanged. This is the table above.

What is left: with bloom on, smooth differences in the bloom's shape (the pyramid approximates the six
Gaussians of `look.json`; at most about 20 levels on a soft ramp next to the brightest bars), and single
pixels at threat edges where the reference keeps about 0.3% coverage of a very hot colour. Files:
`captures/m1/<still>.png`, `<still>.cmp.png` (engine, reference, difference ×4), `<still>.compare.txt`.
Command: `scripts\run-desktop.ps1 -NoBuild -Res 2400x1080 -Frames 4 -Screenshot captures\m1\<still>.png
-AppArgs '--scene design/scenes/<scene>.rfscene --still --bloom on|off --no-overlay'`, then
`python -I design\tools\scenes.py compare captures\m1\<still>.png <scene> [--nobloom]`.

Watch item from the lead's review (no action yet): thin hot lines at shallow angles (the SHOT 4.5 row of
the card, 7° lines) look slightly ropey in the engine and in the reference, because the tone map hardens
the one-pixel anti-aliasing ramp at high intensity. The frame strips will show whether rotating glyphs or
shots crawl; if they do, the lead gets a strip and decides whether hot lines get a wider ramp. The
coverage formula stays as it is until then (the stills are matched to it).

### How the renderer is built (M1)

| # | Pass | Size | Content |
|---|---|---|---|
| 1 | Scene | W × H | Additive into `B10G11R11_UFLOAT` (fallback `R16G16B16A16_SFLOAT`), cleared: capsules (lines, discs, text, lattice, shots, trails, beams), instanced glyphs, stateless sparks, soft lights under 16 reference px ("sparkles"), triangles (glass) |
| 2 | Pools | W/4 | Soft lights of 16 reference px and up (flashes, kill pools, auras, player and well pools), times the look variant's pool level |
| 3 | Background | W/4 (W/2 on Ultra) | Radial gradient and up to 8 nebula clouds at the pixel centres they cover (TECH_SPEC 5.2 pass 0) |
| 4 | Bloom down | W/2 … W/2^L | Level k from level k−1 (level 1: the scene plus the pools) with 4 bilinear taps at ±a_k source texels. a_k is solved at start-up so that the spread accumulated at level k, plus what the upsampling adds, equals the look's σ_k (`look.json`, reference px × H/1080). L = 6 / 5 / 4 by tier, weights renormalised |
| 5 | Bloom up | … W/2 | Each level = bilinear coarser level × its weight + this level × w_k. The last one (W/2) also gathers the pools, the afterglow excess and the background, each divided by the bloom strength, so the composite reads one low-resolution texture (`bloom_final.frag`) |
| 6 | Afterglow | W/8 (the bloom level of σ 6 ref px) | look.md v2.1 §7. Source = bloom level 3 (σ 6 ref px) less the knee: `s × max(m − 0.25, 0) / m`. Two outputs: history = max(source, previous × e^(−dt/τ)) with the previous history re-aimed by the camera (scale and shift) and faded over 4% at its edges; excess = max(previous × decay − source, 0), added × 0.7 through the last bloom pass. No line-sharp history anywhere. Every tier with the afterglow on (off on Low only as cut 6) |
| 7 | Distortion | W/8 (W/4 on Ultra) | `R16G16_SFLOAT` displacement in logical px from up to 8 rings (Gaussian profile, "wide" = full width at half maximum) and 4 wells, evaluated analytically |
| 8 | Composite | swapchain | scene(uv + displacement) (three taps with chromatic aberration on High and up) + strength × gathered bloom (bloom + pools + afterglow + background) → drain toward white (the Build) → vignette → exposure → hue-preserving tone map with white bleed → ±½ code value of dither (interleaved gradient noise, moved to linear light through the sRGB slope on an sRGB target). Then, in the same render pass: the threat layer (all dark rims, then all cores, premultiplied over; capsules and instanced glyphs) and the HUD (additive) |

- Passes 1-7 are one command buffer submitted without waiting; pass 8 is a second command buffer that
  waits for the acquired image, so GPU timestamps separate work from the wait for the panel.
- Every HDR pass is in logical landscape; only the swapchain pass applies the pre-rotation (composite
  UVs, threat and HUD clip space).
- Instance data: one persistently mapped buffer with a region per primitive per frame in flight, plus a
  static region for a replayed `.rfscene`; glyph instances are staged on the CPU and grouped by shape
  when the frame is recorded (one instanced draw per shape in use); sparks live in a persistent ring
  whose slots are reused only after every frame that could draw them has finished. No allocation per
  frame.
- Design data: `design/look.json` and `design/look_variants.json` are embedded at build time and parsed
  once at start (`src/core/json_doc.*`, `src/content/look.*`); `--look <id>` picks the variant (default
  `signal`). `.rfscene` replay for the stills: `--scene`, `src/game/rfscene.*`.
- Primitives and formats: `src/gfx/capsule.h`. Shaders: `capsule.*`, `glow.*`, `tri.*`, `fullscreen.vert`,
  `bloom_down.frag`, `bloom_up.frag`, `composite.frag`.

### Deviations from TECH_SPEC §5.2 / §5.3 (so far)

- **Capsule instance:** endpoints (f32×4), then two f16×4 words: start colour RGB + half width, end
  colour RGB + flags (flat start, flat end, disc). Still 32 bytes. Colours are linear HDR with the
  intensity folded in, so there is no separate intensity, no RGBA8 colour and no skirt. Reason: look.md
  v2 draws colour gradients along a stroke and flat joints on polygons, and the glow comes from bloom (the
  reference has no skirt).
- **Two more primitives:** soft light (centre, radius, f16 colour; 24 bytes; Gaussian with σ = r/2.45,
  square cut at 1.25 r) for sparkles and kill pools, and triangles (16 bytes per vertex, colour per vertex)
  for glass fills. Reason: look.md v2 and the stills use both. Pools are drawn at quarter resolution, so a
  large pool costs 1/16 of its full-resolution fill (§5.1 principle 2).
- **Bloom:** 4-tap down with solved offsets and 1-tap bilinear up at every tier, matched to the six
  Gaussians of `look.json`. The 13-tap prefilter and tent upsampling listed for High are not built (the
  solved 4-tap pyramid already matches the reference); the tiers change the level count (4 / 5 / 6).
- **Glyph instance:** position (f32×2), two rotations (f16×4: layers 0 and 2, layer 1), scale, hull
  intensity, hull half width and colour mode (f16×4), two RGBA8 linear colours: 32 bytes, not 24. The
  colour fields of look.md §2 need two colours per instance; the modes cover enemies, white copies and
  marks, the player hull, the two ring fields and flat colours (Shards, warp-in rings, bullets, the
  threat layer). Glyph meshes are flipped to y down at load, layer 0 first (the low-detail LOD and the
  white copies draw that prefix), with look.md §4 joint flags per layer. Three unit circles (32, 64 and
  128 segments) are added for rings.
- **Spark:** birth position, birth time (f32×3), velocity, life, drag (f16×4), intensity and half width
  (f16×2), RGBA8 colour, 4 spare bytes: 32 bytes. Space-warp of sparks by wells (TECH_SPEC 5.3 details)
  is not built. Embers (10 per medium or large kill) are CPU-drawn sparkles; shatter pieces are CPU
  capsules; both are few.
- **Afterglow** (v2.1): the source is bloom level 3 as look.md asks (the pools are part of the bloom's
  input, so they are part of the source too). It runs only when the bloom does (stills turn both off).
  TECH_SPEC 5.4's "quarter res or off" on Low is now W/8 on every tier.
- **Composite precision:** colour maths in half precision, positions in full; the dither is
  interleaved gradient noise (TECH_SPEC 5.2 allows it) and its amplitude in linear light uses
  max(1.82·√x, 1/12.92) for the sRGB slope (within −20% / +30% of exact).
- **Gathered low-resolution terms:** pools, the afterglow excess and the background reach the composite
  through the last bloom pass instead of three more full-resolution taps (measured: each tap costs
  about 0.27 ms on the Moto). They go through one more bilinear step and the `B10G11R11` W/2 target; the
  Build's drain now also touches the background. Stills: table above.
- **Threat layer:** capsules and instanced glyphs, a dark rim and a core per segment, premultiplied
  over. The Moto's and the desktop's swapchains are `R8G8B8A8_SRGB` / `B8G8R8A8_SRGB`; with a UNORM
  swapchain (fallback only) the threat layer and HUD would blend in encoded space.
- **Governor v1** steps down after 0.5 s with more than 5% of the last 120 frames missing a refresh, the
  GPU saturated (work above 97% of the period) or thermal status SEVERE; it steps up after 10 s with no
  missed frame and GPU work under 75% of the period. TECH_SPEC 5.4's inputs (P95 interval, GPU time
  against a fixed budget) do not work as written on these phones: the present-call P95 sits 1 ms above
  the period with no frame late, and the GPU's clock governor keeps the GPU work near 85% of the period
  whatever the load. Spark density moves first (in steps of 0.25), then the tier; refresh-rate steps
  (120 → 90 → 60) are not built yet. A tier change waits for the GPU and rebuilds the targets (one missed
  frame on the desktop). Off in benchmarks.
- **Bench scenes** follow `design/scenes.md`; where it leaves freedom: shoal streams follow an
  arc-length-parametrised figure of eight (650 × 260 wu); shots fly at 1,500 wu/s; kill pools live as
  long as the recipe's longest spark (medium 0.9 s, large 1.2 s, as `crowd.py` draws the target);
  medium and large shock rings reach 80 and 120 wu; the bass envelope decays in 180 ms; the
  background's 8° hue drift toward the accent is a small mix toward the accent's colour; the boss
  phase break falls on bar 16k + 12 and the Drop on every 8th bar; the far pool (3,072 − 709 slots)
  orbits 1,500-4,200 wu from the ship with far-mode updates only.
- **Endgame formations fitted to the screen** (2026-11-11 pm), so the counts in view are scenes.md §4's:
  the table's distances do not fit a 720 wu tall view (a 480–560 wu ring is 1,000 wu tall). Rings keep
  their width and are squashed vertically (Sentries 0.55, Moths 0.82, Tethers 500 × 200, Weavers
  0.45), as the target frame squashes its rings; the shoal's figure of eight and the Wyrms' paths are
  softly fitted into ±680 × ±300 wu (tanh per axis); the spear and the Hound packs run to the edge of
  that box's ellipse; Drifters and Caroms are scattered in that box; the boss stand-in is 600 wu across
  and 180 wu up and down; Sentry arms have 16 wu between bullets (330 wu arms). `bench_mid` keeps its
  explicit ±900 × ±450 and ring geometry: about 290 of its 500 enemies are in view at frame 600.
- **Enemy bullets are drawn upright** (points on the screen's axes, never turning). Confirmed by the lead
  and written into look.md v2.2 §3 (the first brief had asked for bullets turned to their travel).
- **Boss stand-in:** no aura on its 48 parts (the target frame has none; look.md's boss aura would be
  one aura for a real boss).
- **Background:** the engine shows fewer nebula clouds than look.md's "about six on screen" (8 clouds on
  a 3,600 × 1,800 wu tile, 1-3 on screen; the composite has 8 cloud slots and six on screen would need
  about 34 on the tile) and about 270 star-dust sparkles on screen instead of 420 (tile 2,000 × 900 wu).
  Not changed this session: both are faint (the whole background adds 0.008 luminance at frame 600).
- **Tiers:** Low is TECH_SPEC's Low (4 bloom levels, no distortion, aberration or clouds, sparks × 0.35,
  lattice 32 wu, simplified LOD) and keeps glass and 24 kill pools; look.md §10's cuts are `--cuts N` on
  top of any tier. The afterglow is W/8 on every tier.
- **First-launch tier choice** (TECH_SPEC 5.4): the probe runs on screen under a "SETTING UP FOR THIS
  DEVICE" label, not off screen, and takes 6.3 s on the Moto (four tiers from Ultra down, 1-2 s each under
  a 20-draw saturating load) instead of two seconds; the GPU name is not used as a hint (the cache is keyed
  by GPU and driver). Budget: GPU work at the top clock ≤ 72% of the refresh period (PLAN §5's 12 ms at
  60 Hz). Pictures (`--screenshot`, `--scene`) and tests default to High so they stay comparable. The
  lead accepted the on-screen probe for M1; **from M3 it should run behind the title screen.**
- **Reduce-flashing mode** halves the amounts of look.md §7's beat table (exposure, bloom, zoom, enemy
  swell, aberration, lattice ripple and shimmer, background bass and hue drift) and removes the Drop's
  bloom burst; the Drop's white front, confetti and zoom snap stay (scenes.md events, not in the table).
  The Nova (§5) is not in M1.

### Corrections to M0 found in M1

1. **Resolution.** The Moto rendered at 2174×1020, not 2400×1080, through M0 and until 2026-11-11 09:05.
   On Android 11 `RaveformActivity` hid the system bars but did not call `setDecorFitsSystemWindows(false)`,
   so the window kept the bar and cut-out insets (2400 − 106 − 120, 1080 − 60). The app had logged
   "surface 2174x1020 differs from the display mode 2400x1080". Fixed; the log now says "matches". All
   M0 `bench_idle` numbers were measured at 86% of the native pixel count. The 12R (Android 14) is likely
   affected the same way: re-check when it is attached.
2. **Panel-side grading with Swappy.** Swappy counts a frame late only against the time it asked for:
   when the GPU needs two periods per frame, it reports 0 late frames (a 30 fps run graded PASS).
   `bench-android.ps1` now also counts present intervals above 1.5 periods (the app's `missed`, far
   outside the 1-2 ms present-call jitter) and floors the panel grade at the CPU-side median in whole
   periods. M0's clean runs had no such intervals, so their grades stand.

### GPU timing method

- Timestamps after every pass (`gpu_ms` in the result; `total` is work only, `wait` the gap before the
  swapchain pass). The GPU's frequency governor picks the clock that keeps the GPU about 85% busy, so in
  a normal run the timestamps sit near 14 ms whatever the load (a run whose composite wrote a constant
  still read 14 ms in total). The shell user cannot read the GPU clock (`/sys/class/kgsl` is denied).
- Cost at the top clock: `--gpu-burn 40` adds 40 untimed full-screen draws before the timed passes, so
  the GPU is saturated (frames take 60-290 ms) and runs at its highest clock; the timed passes then show
  what they cost there. This is the number to hold against the 12 ms budget.

### Moto contract runs (2026-11-11 pm; PLAN §5: 10-minute soak, then 120 s, graded on the panel at P95)

`bench_endgame`, 2400×1080 (surface matches the display mode), Signal, Reef, Swappy at 60 Hz, overlay
on, main thread on the big cores, this session's build deployed right before each run (shared phone,
quiet: 0.86-0.99 foreign cores on average, mostly SurfaceFlinger, the audio server and the composer). In view on
average: 706 enemies, 961 bullets, 2,345 glyph instances; crowd gain 0.479, swarm 0.331. 0 heap
allocations on our thread in every run; 0 audio underruns.

| Tier | Panel: P95 / missed / max (CPU side) | Verdict | Present call P95 / P99 (ms) | Main thread P50 / P95 (ms) | GPU at the governor's clock P50 / P95 (ms) | GPU at the top clock P50 / P95 (ms) | Thermal, battery | Result file (`captures/bench/ZY22BXMJG3-bench_endgame-…`) |
|---|---|---|---|---|---|---|---|---|
| Low | 1 period (16.67 ms) / 0 of 7,188 / 24.9 ms | **PASS** | 17.72 / 18.58 | 4.59 / 4.81 | 13.35 / 13.72 | **10.74 / 11.05** | 0 → 0, 26.5 → 28.8 °C | `20261111-164531`, top clock `-171046` |
| Medium | 1 period (16.67 ms) / 0 of 7,188 / 19.0 ms | **PASS** | 17.68 / 18.37 | 5.52 / 5.83 | 12.16 / 14.82 | **11.99 / 12.21** | 0 → 0, 28.8 → 29.7 °C | `-165823`, top clock `-171128` |
| High | 1 period (16.67 ms) / 0 of 7,188 / 20.7 ms | **PASS** | 17.61 / 18.07 | 6.46 / 6.84 | 14.05 / 14.52 | **14.03 / 14.28** | 0 → 0, 29.7 → 30.6 °C | `-171253`, top clock `-171209` |

GPU passes at the top clock (P50, ms): Low scene 3.15, pools 0.15, background 0.14, bloom (4 levels)
2.98, afterglow 0.05, composite + threat + HUD + overlay 4.29; Medium 3.41 / 0.16 / 0.49 / 3.01 (5) /
0.05 / distortion 0.04 / 4.79; High 4.37 / 0.17 / 0.49 / 3.06 (6) / 0.05 / 0.04 / 5.82. Against PLAN §5's
12 ms: Low fits (11.05 at P95), Medium is at the line (12.21), High is over (14.28) and still holds 60 Hz
with the GPU at about 86% of the period at its top clock (no headroom left for a hotter phone or a
heavier scene). The three runs ran back to back over 40 minutes; thermal status stayed 0 (none). The v2.1 afterglow
costs 0.05 ms (W/8) where the first version cost 0.33 (Low) and 0.72 (Medium). The load in view is
up from about 500 enemies and 660 bullets this morning (Low then: 10.30 / 10.58 ms at the top clock).
No cuts were needed: Low passes as TECH_SPEC defines it.

In-app captures (`capture-android.ps1 -InApp`, upright, 2400×1080): `captures/m1/frame_reef_moto.png`
compares with the reference at mean error 1.16, luminance ratio 0.982, soft light 0.96 / 0.98 / 0.99
(desktop 1.11 / 0.982); `bench_endgame_600_moto_low.png` 28.1% dark, luminance 0.149 (the desktop's Low
frame: 28.1%, 0.149); `bench_endgame_600_moto_medium.png` 19.8%, 0.163.

First-launch tier probe on the Moto (`--tier auto`): GPU work at the top clock Low 10.81, Medium 12.10,
High 14.01, Ultra 15.99 ms (the `--gpu-burn` runs above: 10.74 / 11.99 / 14.03); it chose **Low** in 6.3 s
(Medium is 0.1 ms over the 12 ms line) and cached it (`files/tier.txt`, read with `run-as`). A first
version measured the stage's first half second (load not yet built up) and chose Medium; the probe now
pre-rolls two seconds of the stage and follows real time.

### Moto results earlier on 2026-11-11 (bench_endgame, 2400×1080, Signal, Reef, Swappy 60 Hz, cold, 30 s unless noted)

Per frame in view: about 500 enemies (max 660), 660 enemy bullets (max 870), 1,800 glyph instances,
1,700 live sparks, 3,000 capsules; 3,072 enemy slots alive. Main thread pinned to the big cores.

| Tier | Panel missed | CPU-side P95 / max (ms) | Main thread P50 / P95 (ms) | GPU total at the governor's clock P50 / P95 (ms) | GPU at the top clock P50 / P95 (ms) | Result file |
|---|---|---|---|---|---|---|
| Low | 0 / 1797 | 17.66 / 20.27 | 4.25 / 4.50 | 14.16 / 14.89 | 11.69 / 12.05 | `ZY22BXMJG3-bench_endgame-20261111-092633.json`, top clock `-093031` |
| Medium | 0 / 1797 | 17.65 / 20.51 | 5.22 / 5.53 | 13.64 / 14.02 | 13.46 / 13.68 | `-092726`, top clock `-093115` |
| High | 3 / 1794 (0.17%) | 17.62 / 33.60 | 6.10 / 6.58 | 15.64 / 16.70 | not measured | `-092820` |

GPU passes at the top clock, Low: scene 2.63, pools 0.29, background 0.14, bloom (4 levels) 2.80,
afterglow (1/4) 0.33, composite + threat + HUD 5.51 ms. Medium: scene 2.87, pools 0.53, background 0.49,
bloom (5) 2.85, afterglow (1/2) 0.72, distortion 0.04, composite 5.94 ms. Heap allocations in the
measured window: 0 on our thread in every run. These are short, cold runs on a shared phone, with the
morning's lighter load in view and the first afterglow; the contract runs are above.

Where the composite's time went (Low, top clock, by switching parts off): a constant output with the
threat layer and the overlay 2.25 ms (the overlay 0.33 of it), the pools/bloom/afterglow taps 0.8 ms,
the rest the colour maths. Then:

| Change (top clock, Low) | GPU total P50 / P95 (ms) | Composite (ms) |
|---|---|---|
| Before (above) | 11.69 / 12.05 | 5.51 |
| Tone-map reciprocals precomputed, cheaper dither slope, profiling switches removed | 11.01 / 11.38 | 4.85 |
| Pools, afterglow and background gathered into the last bloom pass | **10.30 / 10.58** | 3.85 (bloom 3.05) |

Medium at the top clock after both: 12.04 / 12.35 ms (scene 2.82, pools 0.52, background 0.49, bloom
3.17, afterglow 0.72, distortion 0.04, composite 4.26). Files: `ZY22BXMJG3-bench_endgame-20261111-093631.json`
and the two burn runs after it.

Before the background moved to W/4, Medium and High ran at 30 fps (GPU 26.5 ms, composite 16.9 ms:
eight nebula clouds evaluated per full-resolution pixel).

### Desktop validation (Debug, validation and synchronization validation on)

- This session's build (2026-11-11 17:00): `bench_endgame` 60 s at Ultra and at Low, 2400×1080: valid,
  0 validation and 0 loader messages, 0 heap allocations in the window, 2 missed of 3,600 (desktop
  timing is informational), 4 Drops and 2 boss breaks inside the run; 706 enemies and 953 / 962 bullets
  in view on average, crowd gain 0.479 (`captures/desktop_m1_endgame_ultra_60s_20261111-1651.log`,
  `..._low_60s_20261111-1653.log`). Every review picture, strip and still: 0 messages. The tier probe
  (`--tier auto`) and `--reduce-flashing`: 0 messages.
- Earlier build (morning): the same two runs clean; `lifecycle_test` 20 cycles: 0 messages (not re-run:
  no lifecycle code changed).

### Pictures for review (`captures/m1/`, 2400×1080, desktop unless noted)

Re-shot 2026-11-11 17:50 on the v2.2 build with `scripts\review-m1.ps1` (Signal only, per the user's
decision; Moto captures from 17:25, before v2.2); the
morning's set is in `captures/archive/m1-2026-11-11-morning/`. `*.small.png` are 1600 px review copies;
`light.txt` holds the `scenes.py light` line of every moving-scene picture.

- `look_card_nobloom.png`, `look_card.png`, `frame_reef.png`, each with `.cmp.png` and `.compare.txt`.
- `glyph_sheet_1.png`, `_2.png`, `_3.png` (families 1-9, 10-17, then marks, elites, the ship with every
  halo in the four player colours, a Shard and an enemy bullet).
- `bench_mid_600_signal_reef.png`, `bench_endgame_600_signal_reef.png` (and `_low.png`: the Low tier),
  `bench_endgame_600_signal_ultraviolet.png`, `_furnace.png`.
- Strips of eight consecutive frames: `strip_bench_endgame_879_0..7.png` across the first Drop (bar 8,
  frame 873) and `strip_bench_endgame_407_0..7.png` in a quiet bar; `_sheet.png` tiles each at half size.
- Moto in-app captures: see "Moto contract runs".
- Light analysis: `captures/light/` (one class off at a time, now on the v2.2 build; `afterglow_adds_x3.png`
  and `afterglow_adds_low_x3.png` are from before v2.2).
- Afterglow behind fast things: `afterglow_shot_tail_mid600.png` (v2.2, shot lanes),
  `afterglow_shot_beads_mid600_lanes.png` (the same lanes before v2.2), `afterglow_shard_beads_mid600.png`
  (v2.2, a Shard stream: still beads), `afterglow_shot_beads_mid600.png` (before v2.2, mostly that Shard
  stream).

Checked by eye against scenes.md §5's list (v2.2 build): no line-sharp ghost rows (shots leave one even
tail; Shard streams flying to the ship still leave rows of soft dots, mostly visible in `bench_mid`; the
boss's fans leave soft red smears where seven bullets spawn on one turning arm tip); the Drop strip has
no border of streaks; no beads on rings; the danger diamonds and the ship (priority layer) read over the brightest
light; the boss stand-in is violet. The Drop strip jumps in brightness at frame 873 (bloom burst, zoom
snap, white front with an RGB fringe from the aberration of its distortion ring).

### Not built in M1 (or not verified)

- Space-warp of sparks by wells (not in M1); refresh-rate steps in the governor (120 → 90 → 60; needs
  the 12R); the 13-tap bloom prefilter for High. Reduce-flashing mode and the first-launch tier choice
  are built (2026-11-11 pm); reduce-flashing has no on-device switch yet (argument or key R).
- The light targets of scenes.md §5 for `bench_endgame` are not met (above): the lead's decision.
- 12R and tablet: not attached; every M1 item for them is pending (including the surface-size check,
  the contract at 120 / 90 Hz and the in-app captures).
- Audio is still the M0 test tone: beat envelopes come from the scripted 132 BPM clock (M2 brings the
  audio engine, in parallel on `m2-audio`).

### Waiting on the user (carried from M0 and the lead)

Audible test tone, 180° rotation, lock/unlock, the 12R's refresh setting (High) and the 12R itself, the
tablet. Decided 2026-11-11: the look is Signal; the sound variants ship as a player setting; M2 runs in
parallel on its own branch.

## M0 Bootstrap — status 2026-11-10 (evening): device checks done except the tablet and the user checks

All device numbers come from **shared phones**: other sessions use both phones (CLAUDE.md "Shared test
devices"). Each result file in `captures/bench/` carries the phone's CPU load during the run
(`device_load`), so a disturbed run can be told apart. `captures/` is not in git; the tables below are
the record.

### How a run is measured and graded (since this session)

- `bench_idle`: 5 s warm-up, then 60 s measured. `bench-android.ps1` sends no adb command to the phone
  during the measured window (see "Moto pacing" for why), samples the phone's CPU load once a second
  with shell builtins plus `grep`/`sleep`, and pulls the app's full result file.
- **Panel side (graded):** what reached the display. Plain FIFO: VK_GOOGLE_display_timing present
  times for every frame. The panel only changes image at a vsync, so intervals are counted in refresh
  periods; the raw present-fence timestamps scatter by about ±0.25 ms around the period (fence
  signalling, not display timing): on the Moto the raw P95 is 16.92-16.95 ms in runs with **zero**
  missed vsyncs, and the panel's measured period is 16.695 ms (59.9 Hz). Swappy: Swappy owns the
  extension, so its own statistics are used (frames shown one refresh period or more later than it
  asked). That counter also counts a steady extra frame of latency as "late" (seen on the 12R, below),
  so FIFO display timing is the exact measure and Swappy's is a conservative one.
- **CPU side (reported, not graded):** when the present call returned. It jitters by 1-2 ms around the
  period: on the Moto its P95 is 17.5-17.9 ms in every configuration, also with zero missed vsyncs (the
  12R at 60 Hz: 17.6-17.9 ms), so it fails 16.9 ms in every run while the panel shows every frame on time.
- Every late frame is attributed (`src/tools/frame_probe.h`): the interval split into the main
  thread's phases, its CPU time, run-queue wait (runnable but not running), context switches, page
  faults and CPU number, the worst 16 frames with their breakdown, and a per-second timeline that the
  script lines up with the phone's foreign CPU load and new processes.

### M0 exit criteria (PLAN §7)

| Criterion | Status | Evidence |
|---|---|---|
| One command builds and runs the desktop app; one command builds, installs and launches on a serial | Verified | `run-desktop.ps1`, `deploy-android.ps1` (used throughout) |
| Desktop 60 s with zero validation messages; screenshot written and inspected | Verified | `captures/desktop_validation_60s_20261110-2047.log`: 0 validation, 0 loader messages, heap 0; `lifecycle_test` 20 cycles 0 messages (`captures/desktop_lifecycle_20261110-2048.log`). Desktop screenshot inspected in the previous session; in-app phone captures inspected now |
| Moto and tablet: `bench_idle` holds the panel rate with P95 inside the contract | **Moto: met on the panel side** (FIFO display timing: 0 missed in every clean run, P95 = one refresh period, panel max 17.1-17.4 ms; Swappy: 0 late). CPU-side P95 17.5-17.9 ms does not meet 16.9 (jitter, see above): the user decides which measure counts. **Tablet: not attached, not run** | Tables below |
| 20 background/foreground cycles, 180° rotation, lock/unlock: no crash, no leak, swapchain recovers | **20 cycles verified on the Moto and the 12R** (final build). 180° rotation and lock/unlock: need the user. Tablet: pending | `lifecycle-android.ps1`: 20/20 backgrounds, 20/20 resumes presenting, 20/20 surface released in time, no problem or crash lines, on both phones |
| Test tone on both devices, zero underruns over 60 s; the user confirms it is audible | **0 underruns over 60 s on the Moto and the 12R** (final build). Audible: needs the user. Tablet: pending | `ZY22BXMJG3-bench_idle-20261110-212330.json`, `a0e622a5-bench_idle-20261110-205932.json` |

**Correction (2026-11-11, M1):** every Moto number in this M0 section was measured with a 2174×1020
surface (the window kept the system-bar and cut-out insets), not the native 2400×1080; see "Corrections
to M0 found in M1" above. The 12R is likely affected the same way and needs a re-check.

Also verified: in-app captures (`capture-android.ps1 -InApp`) upright and readable on both phones
(`captures/ZY22BXMJG3-20261110-212649-inapp.small.png`, `captures/a0e622a5-20261110-210252-inapp.small.png`).
The focus-loss validity flag fired on a genuine interruption on the desktop (window lost focus:
`"valid": false, "interruptions": 1`); no phone run was interrupted, so it is not confirmed on a phone.

### Moto results (bench_idle 60 s, Moto One 5G Ace, 60 Hz; 3 runs per row)

`os` = the scheduler places the main thread (it lives on the little cores 0-5); `big` = pinned to the
big cores 6-7 (now the default). P95/P99/max = CPU side; panel = graded measure.

| Configuration (quiet phone) | CPU P95 | CPU P99 | CPU max | CPU missed | Panel missed | Panel verdict | Worst run-queue wait |
|---|---|---|---|---|---|---|---|
| Swappy, os, overlay | 17.50-17.52 | 17.97-18.16 | 19.8-22.4 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 4.6-6.0 ms |
| Swappy, os, no overlay | 17.46-17.49 | 17.94-18.04 | 21.1-22.3 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 4.8-8.5 ms |
| FIFO, os, overlay | 17.81-17.85 | 18.84-18.88 | 22.9-39.1 | 2 / 10780 (*) | 2 / 10768 (*) | 3/3 PASS | 4.1-7.2 ms |
| FIFO, os, no overlay | 17.76-17.78 | 18.89-18.99 | 22.2-24.3 | 0 / 10782 | 0 / 10770 | 3/3 PASS | 4.7-5.1 ms |
| Swappy, big, overlay | 17.67-17.74 | 18.32-18.36 | 20.3-22.2 | 0 / 10782 | 0 (Swappy) | 3/3 PASS | 2.5-4.3 ms |
| FIFO, big, overlay | 17.59-17.62 | 18.08-18.17 | 19.7-20.8 | 0 / 10782 | 0 / 10770 | 3/3 PASS | 1.3-3.3 ms |

(*) Both misses in one run, in the second in which I sent `dumpsys activity` and `pidof` to the phone
by mistake (7 foreign process starts in that second; both were compositor-side: our frame was ready).
FIFO panel: P50 16.695-16.70 ms, raw P95 16.92-16.95 ms, panel max 17.1-17.4 ms in clean runs.

| Configuration, with the old script's adb polling (`pidof` + `logcat -d` every 2 s) | CPU P95 | CPU P99 | CPU max | CPU missed | Panel missed | Worst run-queue wait | Own-work max |
|---|---|---|---|---|---|---|---|
| Swappy, os | 17.67-17.77 | 25.2-25.4 | 39.8-43.1 | 36, 36, 36 (1.01%) | 2.84-2.90% (Swappy) | 13.6-21.5 ms | 13.2-27.5 ms |
| Swappy, big | 17.89-17.95 | 20.6-20.8 | 31.7-34.4 | 18, 15, 14 (0.39-0.50%) | 3.43-4.87% (Swappy) | 4.1-5.1 ms | 2.0-4.6 ms |
| FIFO, os | 18.35-18.42 | 28.2-33.8 | 47.0-47.9 | 42-49 (1.18-1.37%) | 28-30 (0.79-0.84%) | 8.2-12.4 ms | 9.7-17.7 ms |
| FIFO, big | 17.83-17.85 | 28.7-32.0 | 42.9-46.1 | 44-48 (1.24-1.35%) | 30-36 (0.84-1.01%) | 2.8-6.4 ms | 1.8-2.3 ms |

FIFO panel misses under the polling: 187 of 188 had our frame produced on time (normal CPU-side
interval, positive present margin): the compositor missed. All runs above stay inside the contract
on the panel side (P95 one refresh period, under 5% missed, no frame over 50 ms).

Final build, default configuration (Swappy, big): `212044` 0 missed, CPU P95 17.73, max 19.7; FIFO:
`212207` 0 missed on the panel, panel max 17.3; with `--tone`: `212330` 0 missed, 0 underruns.

Earlier runs (old build or old script), CPU side: run A (Swappy) P95 17.57, P99 19.52, max 41.7, 0.61%
missed; FIFO 15 s P95 17.87, P99 29.29, max 47.1, 1.12%; run C (Swappy + SurfaceFlinger polling every
0.7 s) P95 20.07, P99 35.13, max 44.8, 3.34% (SurfaceFlinger: 124 of 3311 missed); this session's first
baseline (old build, old script, quiet phone) P95 17.71, P99 26.03, max 43.6, 1.04%.

### OnePlus 12R (stands in for the tablet; panel held at 60 Hz by the phone's setting)

- 120 Hz: the refresh-rate vote never reached the system (`ANativeWindow_none -> -1`: `dlsym(RTLD_DEFAULT)`
  cannot see libnativewindow.so). Fixed: it is opened explicitly; now `setFrameRateWithChangeStrategy -> 0`.
  The panel still stays at 60 Hz because the phone's refresh-rate setting is Standard
  (`peak_refresh_rate=60.000004`; DisplayModeDirector vote `PRIORITY_USER_SETTING_PEAK_REFRESH_RATE`
  max 60). The 120 Hz check (P95 ≤ 8.6 ms) needs the user to set Display → Screen refresh rate → High.
- While the vote is overruled: the target and the missed count now follow the live panel rate (before:
  target 8.333 ms, "100% missed"), and Swappy's swap interval follows it too (before: Swappy aimed at
  8.33 ms presents and showed 82.5% of frames one refresh later than asked; after: 0).
- 9 runs × 60 s (os ×3, big ×3, tone ×2, earlier ×1): 0-1 CPU-side missed, CPU P95 17.58-17.88, max
  18.9-27.3 ms; panel 0 late with the fix. Tone: 0 underruns. The main thread runs on the mid cores
  (3-6) either way. The system gives the app 2376×1080: the phone's resolution setting is FHD+
  (panel native 1264×2780).
- Lifecycle: 20/20 PASS; it rebuilt the swapchain 79 times (about 4 per cycle: the launcher is portrait,
  so every cycle rotates the surface) and counted 4 audio underruns across the 20 stream reopens (the
  buffer restarts at 2 bursts and grows after an underrun); 0 underruns in steady state.

### Moto pacing (Part B): cause, evidence, changes

**Cause: foreign load on the phone, mostly from our own benchmark tooling.** The old `bench-android.ps1`
ran `adb logcat -d` and `pidof` every 2 s for the whole run and `dumpsys display` inside the window
(run C also polled `dumpsys SurfaceFlinger --latency` every 0.7 s). Each poll starts processes on the
phone and makes logd, system_server or SurfaceFlinger work. Left to the scheduler, our light main thread
lives on the little cores 0-5 (big cores 6-7 sat at 1% load), next to SurfaceFlinger, the composer and
the audio server, so a burst there makes it wait for a CPU, or delays the compositor.

Evidence:
1. Same build, same phone, quiet, script silent during the window: 0 missed in 17 of 18 runs (the 18th:
   my own two adb commands, see above). Re-adding only the old 2 s polling: 36 missed in each of 3 runs
   (1.01%), P99 25.2-25.4, max 39.8-43.1 — the same as the old-script baseline of this session (37,
   1.04%, P99 26.0, max 43.6). Every late second of those runs (26, 23, 26) had foreign process starts.
2. The intermediate script (polling `pidof` from just before the window ended): 11 of the 12 late seconds
   in its 8 runs fell at t = 57-60 s.
3. Attribution of every late frame: the main thread was runnable but waiting for a CPU (run-queue wait
   ≥ 2 ms in 97 of 108 late frames of the disturbed unpinned runs, up to 21.5 ms), or the compositor
   missed with our frame ready (FIFO: normal CPU interval, positive present margin). Never a blocking
   call outside the waits, no major page faults; own work above half a frame only when preempted
   inside it.
4. The `cpu_ms.max` lead (17.8-18.8 ms in the old runs): `cpu_ms` excludes the three waits but not
   preemption. Under the polling it returns (own-work max 13-27 ms), and those frames show 9-21 ms of
   run-queue wait inside scene build, recording or submit: preemption, not a stall in our code. Quiet:
   own-work max 2.3-7.1 ms (unpinned, 12 runs), 0.7-3.7 ms (pinned, 11 runs).

Changes (ours):
- Bench tooling: no adb traffic during the measured window, 5 s warm-up, device-load sampler (foreign
  CPU, new processes, top processes, late seconds joined with foreign activity), full result file,
  panel-side measurement, P95 grading. This removes the disturbance that produced most earlier misses.
- Main thread pinned to the non-little cores by default (`--affinity big`; `--affinity all` restores
  the scheduler's choice), re-applied after every resume. Quiet: worst run-queue wait 1.3-5.8 ms instead
  of 4.1-8.5, own work about 3× faster, audio callback worst 0.07-0.16 ms instead of 0.36-1.0 ms. Under the
  polling: CPU-side late frames halved (14-18 instead of 36) and our thread no longer preempted (worst
  4.1-5.1 ms instead of 13.6-21.5); the remaining misses are outside our thread. On the panel (FIFO
  display timing) the polling costs 0.8-1.0% of frames with or without pinning, all compositor-side, so
  at M0's idle load pinning neither helps nor hurts the panel (Swappy's late counter rose, 2.9% →
  3.4-4.9%, but it also counts latency shifts). It stays the default for the CPU headroom it gives M1
  (own work 3× faster, no preemption inside it); M1's `bench_endgame` is measured both ways.
- Not ours, unchanged: compositor-side misses under foreign load; Swappy allocates on our thread inside
  its calls (≈4 per frame) and on its own threads (≈1 per frame) — our code: 0 in every run.

Remaining risk for M1: the idle scene does not load the GPU or the CPU; a busy phone (other sessions,
background apps, adb) costs frames that no in-app change prevents, so benchmarks must keep the phone
quiet and report its load. With the main thread on the two big cores, M1's worker pool on the Moto
should start with at most one worker on the big cores (PLAN §5 "big cores minus one"). Thermals: only
idle runs so far.

### Done this session (code and scripts)

- `src/tools/frame_probe.*` (new): per-frame attribution described above.
- `src/platform/cpu_affinity.*` (new): pins the calling thread to the non-little cores.
- `src/gfx/frame_pacer.*`: vote fixed (dlopen libnativewindow.so) and recorded; target and Swappy swap
  interval follow the live panel rate; VK_GOOGLE_display_timing measurement in plain FIFO; Swappy
  statistics; allocations inside Swappy counted on our thread only.
- `src/gfx/renderer.*`: phase timestamps; Swappy frame start.
- `src/platform/app.*`: probe, affinity (applied at start and every resume), overlay graded on P95 and
  refreshed only while shown, summary `RFBENCH` line plus full result file, allocations per thread.
- `src/core/json.h` (arrays), `src/core/alloc_counter.*` (per-thread count), `src/core/args.*`
  (`--affinity`), `CMakeLists.txt` (new sources, Ninja job pool).
- Scripts: `env.ps1` (4-core limit), `build-desktop.ps1`, `build-android.ps1` (job limits, own Gradle
  daemon, `-Offline`, `-Clean`), `bench-android.ps1` (above), `lifecycle-android.ps1` (collects our log
  after every cycle: the 12R's log buffer rotates within seconds).
- PC CPU limit (user rule): see CLAUDE.md. Clean desktop Debug build 200.8 s wall, 237.4 s CPU (1.18
  cores average), 1235 processes all at mask `0x55000000`, at most 8 compiler processes (4 jobs × driver
  + cc1); clean Android build 46.8 s wall, 135.1 s CPU (2.89 cores), 286 processes all at `0x55000000`,
  at most 4 compilers, in a fresh daemon with `-XX:ActiveProcessorCount=4`.

### Not verified / needs the user

- Tablet (`R5GL362BEHX`): not attached. All tablet items pending (90/60 Hz contract, lifecycle, tone).
- 12R at 120 Hz: set Display → Screen refresh rate → High, then
  `scripts\bench-android.ps1 -Serial op12r -Scenario bench_idle`.
- 180° rotation, lock/unlock, audibility of the test tone: user checks (commands in the M0 report).
- Contract measure: the user decides whether the panel-side measure (graded here) or the CPU-side one
  counts; on the Moto the CPU-side P95 cannot reach 16.9 ms at idle.

### Deviations

- `c++_shared` (Oboe prefab), no VMA, no ImGui (lead's direction). Network permissions deferred to M4.
- `VK_LAYER_PATH` set per process: the registry lists missing SDK 1.1.92.1 layers.
- Benchmarks warm up 5 s (was 2 s); the scripts stay silent during the measured window.
- `heap_allocs` now counts our main thread outside Swappy; `heap_allocs_in_pacer` (Swappy's calls on our
  thread) and `heap_allocs_other_threads` are reported separately.
- The main thread is pinned to the non-little cores by default (measured above; `--affinity all` to
  compare).

### All runs (bench_idle; 60 s unless noted; result files in `captures/bench/`)

P50-Max = CPU side (present call), ms. Panel = graded measure: P95 in ms after counting intervals in refresh
periods, and missed %. Verdict against the P95 contract on each measure. `old script` = 2 s adb polling
during the run; `adb polling 2 s` = that polling re-added on purpose (disturbance test). The 12R row
`195918` predates the target fix (its "100% missed" is against an 8.33 ms target the panel never ran at).

| Result file | Device | Config | Panel Hz | P50 | P95 | P99 | Max | Missed (CPU) | Panel: P95 graded / missed | Verdict | Underruns | heap_allocs | Valid |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| `ZY22BXMJG3-bench_idle-20261110-190609` | Moto | swappy, old build, old script | 60 | 16.70 | 17.57 | 19.52 | 41.7 | 22 (0.61%) | not measured | -, FAIL cpu | 0 | 17905 | n/a |
| `ZY22BXMJG3-bench_idle-20261110-191030` | Moto | fifo, old build, old script | 60 | 16.69 | 17.87 | 29.29 | 47.1 | 10 (1.12%) | not measured | -, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-191340` | Moto | swappy, old build, old script | 60 | 16.72 | 20.07 | 35.13 | 44.8 | 118 (3.34%) | not measured | -, FAIL cpu | 0 | 1796 | True |
| `ZY22BXMJG3-bench_idle-20261110-195923` | Moto | swappy, old build, old script | 60 | 16.71 | 17.71 | 26.03 | 43.6 | 37 (1.04%) | not measured | -, FAIL cpu | 0 | 1604 | True |
| `ZY22BXMJG3-bench_idle-20261110-201854` | Moto | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.55 | 18.23 | 36.6 | 3 (0.08%) | 16.67 / 0.17% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-202741` | Moto | fifo, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.84 | 19.16 | 36.7 | 1 (0.03%) | 16.70 / 0.03% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-202903` | Moto | swappy, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.50 | 18.07 | 24.3 | 0 (0.00%) | 16.67 / 0.03% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203026` | Moto | fifo, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.79 | 18.93 | 41.6 | 2 (0.06%) | 16.70 / 0.03% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203149` | Moto | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.70 | 17.51 | 18.05 | 34.9 | 1 (0.03%) | 16.67 / 0.06% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203312` | Moto | fifo, os, script polled pidof from ~1 s before the end | 60 | 16.71 | 17.83 | 18.99 | 27.3 | 1 (0.03%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203435` | Moto | swappy, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.69 | 17.46 | 17.95 | 32.8 | 1 (0.03%) | 16.67 / 0.06% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203559` | Moto | fifo, os, script polled pidof from ~1 s before the end, no overlay | 60 | 16.70 | 17.73 | 19.08 | 45.5 | 2 (0.06%) | 16.70 / 0.06% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-203911` | Moto | swappy, big | 60 | 16.71 | 17.67 | 18.32 | 19.0 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204035` | Moto | fifo, big | 60 | 16.70 | 17.60 | 18.16 | 20.1 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204308` | Moto | swappy, os | 60 | 16.70 | 17.50 | 17.97 | 22.4 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204433` | Moto | fifo, os | 60 | 16.70 | 17.85 | 18.88 | 39.1 | 2 (0.06%) | 16.70 / 0.06% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204557` | Moto | swappy, big | 60 | 16.71 | 17.67 | 18.34 | 22.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204721` | Moto | fifo, big | 60 | 16.70 | 17.59 | 18.08 | 19.7 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-204845` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.46 | 17.99 | 21.1 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205009` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.78 | 18.99 | 22.2 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205133` | Moto | swappy, os | 60 | 16.71 | 17.52 | 18.16 | 19.8 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205257` | Moto | fifo, os | 60 | 16.71 | 17.84 | 18.87 | 22.9 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205422` | Moto | swappy, big | 60 | 16.71 | 17.74 | 18.36 | 22.1 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205546` | Moto | fifo, big | 60 | 16.72 | 17.62 | 18.09 | 20.4 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205711` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.49 | 17.94 | 22.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-205836` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.78 | 18.89 | 24.3 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210001` | Moto | swappy, os | 60 | 16.70 | 17.52 | 18.07 | 22.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210125` | Moto | fifo, os | 60 | 16.70 | 17.81 | 18.84 | 23.5 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210250` | Moto | swappy, big | 60 | 16.71 | 17.68 | 18.32 | 20.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210415` | Moto | fifo, big | 60 | 16.70 | 17.61 | 18.17 | 20.8 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210539` | Moto | swappy, os, no overlay | 60 | 16.70 | 17.48 | 18.04 | 21.6 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210704` | Moto | fifo, os, no overlay | 60 | 16.70 | 17.76 | 18.89 | 23.4 | 0 (0.00%) | 16.70 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-210845` | Moto | swappy, os, adb polling 2 s | 60 | 16.70 | 17.76 | 25.36 | 40.8 | 36 (1.01%) | 16.67 / 2.87% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211346` | Moto | swappy, big, adb polling 2 s | 60 | 16.71 | 17.91 | 20.57 | 34.4 | 18 (0.50%) | 16.67 / 3.43% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211509` | Moto | swappy, os, adb polling 2 s | 60 | 16.71 | 17.77 | 25.32 | 39.8 | 36 (1.01%) | 16.67 / 2.84% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211633` | Moto | swappy, big, adb polling 2 s | 60 | 16.71 | 17.89 | 20.84 | 33.4 | 15 (0.42%) | 16.67 / 4.30% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211756` | Moto | swappy, os, adb polling 2 s | 60 | 16.71 | 17.67 | 25.18 | 43.1 | 36 (1.01%) | 16.67 / 2.90% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-211920` | Moto | swappy, big, adb polling 2 s | 60 | 16.70 | 17.95 | 20.72 | 31.7 | 14 (0.39%) | 16.67 / 4.87% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212044` | Moto | swappy, big | 60 | 16.70 | 17.73 | 18.42 | 19.7 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212207` | Moto | fifo, big | 60 | 16.70 | 17.60 | 18.15 | 23.8 | 0 (0.00%) | 16.69 / 0.00% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212330` | Moto | swappy, big, tone | 60 | 16.71 | 17.68 | 18.30 | 21.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212731` | Moto | fifo, os, adb polling 2 s | 60 | 16.70 | 18.35 | 33.75 | 47.0 | 49 (1.37%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-212855` | Moto | fifo, big, adb polling 2 s | 60 | 16.71 | 17.85 | 32.05 | 44.9 | 48 (1.35%) | 16.70 / 1.01% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213019` | Moto | fifo, os, adb polling 2 s | 60 | 16.71 | 18.35 | 31.89 | 47.9 | 46 (1.29%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213142` | Moto | fifo, big, adb polling 2 s | 60 | 16.71 | 17.84 | 28.73 | 42.9 | 44 (1.24%) | 16.70 / 0.96% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213306` | Moto | fifo, os, adb polling 2 s | 60 | 16.71 | 18.42 | 28.23 | 47.3 | 42 (1.18%) | 16.70 / 0.79% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `ZY22BXMJG3-bench_idle-20261110-213429` | Moto | fifo, big, adb polling 2 s | 60 | 16.70 | 17.82 | 31.61 | 46.1 | 46 (1.29%) | 16.70 / 0.84% (display timing) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-195918` | 12R | swappy, old build, old script | 60 | 16.59 | 17.58 | 18.06 | 19.4 | 3618 (100.00%) | not measured | -, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-201900` | 12R | swappy, os, script polled pidof from ~1 s before the end | 60 | 16.58 | 17.65 | 18.12 | 18.9 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204310` | 12R | swappy, os | 60 | 16.59 | 17.60 | 18.00 | 19.3 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204435` | 12R | swappy, big | 60 | 16.60 | 17.88 | 18.32 | 20.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204559` | 12R | swappy, os | 60 | 16.57 | 17.65 | 18.11 | 19.2 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204723` | 12R | swappy, big | 60 | 16.60 | 17.84 | 18.34 | 19.8 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-204846` | 12R | swappy, os | 60 | 16.59 | 17.59 | 18.02 | 19.7 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205010` | 12R | swappy, big | 60 | 16.61 | 17.83 | 18.34 | 27.3 | 1 (0.03%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205248` | 12R | swappy, big, tone | 60 | 16.61 | 17.69 | 18.24 | 20.8 | 0 (0.00%) | 33.33 / 82.50% (Swappy) | FAIL panel, FAIL cpu | 0 | 0 | True |
| `a0e622a5-bench_idle-20261110-205932` | 12R | swappy, big, tone | 60 | 16.59 | 17.88 | 18.32 | 19.5 | 0 (0.00%) | 16.67 / 0.00% (Swappy) | PASS panel, FAIL cpu | 0 | 0 | True |
