// M8 part 6 (design/modes/jam.md "Look"): JAM's room. Each layer the mixer has on is a light in the room (KICK a ring through the
// lattice on every beat, HATS star dust along the top wall, BASS a low pool at each end of the floor, CLAP two fixtures' short beams
// on two and four, OPEN HAT two long beams sweeping, SHAKER a shimmer over the floor, CHORDS the mirror ball's 24 glints, STABS a fan of
// five beams from the bottom wall); the pads drawn by their edge with their words (the mixer's lit with a line up to its fixture, the
// hold pads' rings filling, DROP's clock, the Track pads in their Tracks' accents); the shelves of free toys round the ship.

#include <cmath>
#include <cstdio>

#include "core/args.h"
#include "game/oklch.h"
#include "game/play_view.h"
#include "content/icons.h"
#include "gfx/vector_font.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec3 whiteC() { return {1.0f, 1.0f, 1.0f}; }
Vec2 dirOfA(float a) { return Vec2{std::cos(a), std::sin(a)}; }
float frac01(double v) { return static_cast<float>(v - std::floor(v)); }
u32 hash2(u32 a, u32 b) {
    u32 h = a * 0x9E3779B1u ^ (b + 0x7F4A7C15u) * 0x85EBCA77u;
    h ^= h >> 15;
    h *= 0x2C1B3C6Du;
    return h ^ (h >> 12);
}
}  // namespace

void PlayView::jamBeam(FrameDraw& d, Vec2 from, Vec2 to, float level) {
    // jam.md "A beam": one 2 px line at 0.6 and a glow of 6 wu at 0.2, the accent running to white at the fixture; scene light only.
    if (level <= 0.01f || !jamData_) return;
    const ModesData::Jam& jd = *jamData_;
    const Vec3 W1 = whiteC(), a = palette_->accent;
    {
        // (the room's light stays in the room: the beam ends where it meets a wall)
        const Vec2 dv = to - from;
        float u = 1.0f;
        const float hx = jamHalf_.x, hy = jamHalf_.y;
        if (dv.x > 1e-3f) u = std::fmin(u, (hx - from.x) / dv.x);
        if (dv.x < -1e-3f) u = std::fmin(u, (-hx - from.x) / dv.x);
        if (dv.y > 1e-3f) u = std::fmin(u, (hy - from.y) / dv.y);
        if (dv.y < -1e-3f) u = std::fmin(u, (-hy - from.y) / dv.y);
        to = from + dv * clampf(u, 0.0f, 1.0f);
    }
    d.scene.line(toScene(from), toScene(to), 0.5f * jd.beamCorePx * refPx_, W1 * (jd.beamLevel * 2.0f * level), a * (jd.beamLevel * level), 0);
    d.scene.line(toScene(from), toScene(to), jd.beamGlowWu * pxPerWu_, W1 * (jd.beamGlow * level), a * (0.5f * jd.beamGlow * level), 0);
}

void PlayView::drawJam(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    if (w.setup().mode != Mode::Jam) return;
    const ModesData::Jam& jd = w.species().modes.jam;
    jamData_ = &jd;
    jamHalf_ = Vec2{w.halfW(), w.halfH()};
    const World::JamState& js = w.jam();
    const float t = static_cast<float>(in.time);
    const Vec3 W1 = whiteC(), accent = palette_->accent;
    const ModesData::Info& mi = w.species().modes.of(Mode::Jam);
    const Vec3 modeA = oklchFull(mi.from[0], mi.from[1]), modeB = oklchFull(mi.to[0], mi.to[1]);
    const float hw = w.halfW(), hh = w.halfH();
    const double beats = in.music / kTicksPerBeat;
    const float beatIn = frac01(beats);
    const u32 beatNo = static_cast<u32>(std::floor(beats));
    const u8 mixer = w.phrase().section == Section::Drop ? static_cast<u8>(0xFF) : js.mixer;  // (the Drop: every layer, every light)
    const auto fixture = [&](u32 k) { return Vec2{jd.pad[k < 8 ? k : 0].at.x, -hh}; };
    const bool fx = shown(kMuteFx);

    // ---- The lights (jam.md "Look": the room's light show is exactly as full as its music).
    if (fx) {
        // KICK: a ring through the lattice from the room's centre on every beat (one impulse of 140).
        if ((mixer & 1u) && beatNo != jamBeatSeen_) {
            if (!reduceFlashing_) push(kImpKill, Vec2{}, 140.0f, 0.9f * hw, t);
        }
        jamBeatSeen_ = beatNo;
        // HATS: star dust along the top wall, twinkling on the eighths.
        if (mixer & 2u) {
            const u32 eighth = static_cast<u32>(std::floor(beats * 2.0));
            for (u32 k = 0; k < 28; ++k) {
                const u32 h = hash2(k, eighth);
                const float x = -hw + (static_cast<float>(k) + 0.5f) / 28.0f * 2.0f * hw + static_cast<float>(h % 40u) - 20.0f;
                const float y = -hh + 18.0f + static_cast<float>((h >> 8) % 50u);
                const float tw = 0.4f + 0.6f * static_cast<float>((h >> 16) % 100u) / 100.0f;
                d.sparkles.glow(toScene(Vec2{x, y}), (1.6f + 1.4f * tw) * refPx_, W1 * (0.9f * tw * (0.6f + 0.4f * hat_)));
            }
        }
        // BASS: a low pool of the grid's colour at each end of the floor, breathing with the bass.
        if (mixer & 4u)
            for (int sgn = -1; sgn <= 1; sgn += 2)
                d.pools.glow(toScene(Vec2{static_cast<float>(sgn) * 0.82f * hw, 0.55f * hh}), 260.0f * pxPerWu_, palette_->grid * (0.12f + 0.16f * bass_));
        // CLAP: on two and four the walls flash and two fixtures each throw a short beam at the floor.
        if (mixer & 8u) {
            const bool clap = (beatNo % 2u) == 1u;
            const float k = clap ? std::exp(-beatIn * 6.0f) : 0.0f;
            if (k > 0.02f) {
                const float lv = reduceFlashing_ ? 0.5f * k : k;
                jamBeam(d, fixture(2), fixture(2) + Vec2{-120.0f, 0.55f * 2.0f * hh}, lv);
                jamBeam(d, fixture(5), fixture(5) + Vec2{120.0f, 0.55f * 2.0f * hh}, lv);
                const Vec3 wc = accent * (1.4f * lv);
                const Vec2 c4[4] = {{-hw, -hh}, {hw, -hh}, {hw, hh}, {-hw, hh}};
                for (u32 e = 0; e < 4; ++e) d.scene.line(toScene(c4[e]), toScene(c4[(e + 1u) % 4u]), 1.6f * refPx_, wc, wc, 0);
            }
        }
        // OPEN HAT: two long beams from the rig's ends sweeping the floor, crossing on the off-beats.
        if (mixer & 16u) {
            const float swing = std::sin(static_cast<float>(beats) * kPi);  // (zero, the crossing, on each half beat)
            for (int sgn = -1; sgn <= 1; sgn += 2) {
                const Vec2 from = fixture(sgn < 0 ? 0u : 7u);
                const float a = 0.5f * kPi + static_cast<float>(sgn) * (-0.55f + 0.45f * swing);
                jamBeam(d, from, from + dirOfA(a) * (2.0f * hh * 1.25f), 0.9f);
            }
        }
        // SHAKER: the floor shimmers (a fine dust of light that moves every sixteenth).
        if (mixer & 32u) {
            const u32 six = static_cast<u32>(std::floor(beats * 4.0));
            for (u32 k = 0; k < 36; ++k) {
                const u32 h = hash2(k + 97u, six);
                const Vec2 p{-hw + static_cast<float>(h % 1000u) / 1000.0f * 2.0f * hw, -hh + static_cast<float>((h >> 10) % 1000u) / 1000.0f * 2.0f * hh};
                d.sparkles.glow(toScene(p), 1.4f * refPx_, mix(W1, accent, 0.5f) * 0.55f);
            }
        }
        // CHORDS: the mirror ball: 24 glints circling the floor, one turn in four bars, their colour stepping round the palette as
        // the chord changes (each bar).
        if (mixer & 64u) {
            const float turn = static_cast<float>(beats / (4.0 * jd.turnBars)) * kTau;
            const u32 barNo = static_cast<u32>(std::floor(beats / 4.0));
            const Vec3 hue = (barNo % 4u) == 3u ? accent : palette_->hue[barNo % 3u];
            for (u32 k = 0; k < jd.glints; ++k) {
                const float a = turn + kTau * static_cast<float>(k) / static_cast<float>(jd.glints);
                const float r = 0.35f + 0.6f * static_cast<float>(hash2(k, 5u) % 100u) / 100.0f;
                const Vec2 p{std::cos(a) * r * 0.92f * hw, std::sin(a) * r * 0.85f * hh};
                d.pools.glow(toScene(p), jd.glintWu * pxPerWu_, mix(W1, hue, static_cast<float>(k % 3u) / 2.0f) * jd.glintLevel);
            }
        }
        // STABS: a fan of five beams from the middle of the bottom wall, flashing on the stab (the bar's first and third beats).
        if (mixer & 128u) {
            const bool stab = (beatNo % 2u) == 0u;
            const float k = stab ? std::exp(-beatIn * 4.0f) : 0.0f;
            if (k > 0.02f)
                for (u32 b = 0; b < 5; ++b) {
                    const float a = -0.5f * kPi + (static_cast<float>(b) - 2.0f) * 0.32f;
                    jamBeam(d, Vec2{0.0f, hh}, Vec2{0.0f, hh} + dirOfA(a) * (1.7f * hh), reduceFlashing_ ? 0.5f * k : k);
                }
        }
    }
    if (w.setup().versus || !font_) return;  // (POP-OFF: no pads)

    // ---- The pads (jam.md "A pad"): a disc drawn by its edge (1.5 px), its word inside (cap 14 wu).
    const u32 me = w.viewPlayer();
    const Vec3 player = palettes_->player[me < kMaxPlayers ? me : 0];
    const float cap14 = 14.0f * pxPerWu_;
    const auto ring = [&](Vec2 c, float r, float hwPx, Vec3 col, float from = 0.0f, float to = 1.0f) {
        const u32 n = 48;
        for (u32 s = 0; s < n; ++s) {
            const float u0 = static_cast<float>(s) / n, u1 = static_cast<float>(s + 1) / n;
            if (u1 <= from || u0 >= to) continue;
            const float a0 = -0.5f * kPi + kTau * std::fmax(u0, from), a1 = -0.5f * kPi + kTau * std::fmin(u1, to);
            d.scene.line(toScene(c + dirOfA(a0) * r), toScene(c + dirOfA(a1) * r), hwPx, col, col, 0);
        }
    };
    const auto words = [&](Vec2 c, float cap, Vec3 col, const char* text, float dyWu = 0.0f) {
        const float wt = font_->measure(TextStyle{cap}, text);
        const Vec2 sc = toScene(c + Vec2{0.0f, dyWu});
        font_->drawField(d.scene, Vec2{sc.x - 0.5f * wt, sc.y - 0.5f * cap}, cap, 0.06f * cap + 0.4f * refPx_, col, col, col, text);
    };
    const bool blinkOn = beatIn < 0.5f;
    for (u32 k = 0; k < ModesData::Jam::kPads; ++k) {
        const ModesData::Jam::Pad& pad = jd.pad[k];
        const Vec2 c = pad.at;
        if (k < ModesData::Jam::kMixer) {
            const bool on = (js.mixer >> k) & 1u;
            const bool waiting = js.mixerAt >= 0.0 && (((js.mixerNext ^ js.mixer) >> k) & 1u);
            Vec3 col = on ? mix(modeA, modeB, static_cast<float>(k) / 7.0f) * 1.6f : W1 * 0.5f;
            if (waiting && blinkOn) col = mix(modeA, modeB, 0.5f) * 1.9f;
            ring(c, pad.radiusWu, 0.75f * refPx_, col);
            words(c, cap14, col, jd.layer[k]);
            {
                // (jam.md "A pad": its layer's live signal as a short line across it under the word, the layer's own rhythm: KICK a
                // bump on every beat, HATS ticks on the eighths, BASS a slow swell, CLAP two and four, OPEN HAT the off-beats, SHAKER
                // the sixteenths, CHORDS a smooth wave, STABS one and three; flat when the layer is off)
                const double b = beats;
                const float b1 = frac01(b), b2 = frac01(b * 2.0), b4 = frac01(b * 4.0);
                float amp = 0.0f;
                switch (k) {
                    case 0: amp = std::exp(-b1 * 6.0f); break;
                    case 1: amp = std::exp(-b2 * 9.0f); break;
                    case 2: amp = 0.5f + 0.5f * std::sin(static_cast<float>(b) * kPi); break;
                    case 3: amp = (beatNo % 2u) == 1u ? std::exp(-b1 * 6.0f) : 0.0f; break;
                    case 4: amp = b2 >= 0.0f && std::fmod(b * 2.0, 2.0) >= 1.0 ? std::exp(-b2 * 6.0f) : 0.0f; break;
                    case 5: amp = 0.4f + 0.6f * std::exp(-b4 * 8.0f); break;
                    case 6: amp = 0.6f + 0.4f * std::sin(static_cast<float>(b) * 0.5f * kPi); break;
                    default: amp = (beatNo % 2u) == 0u ? std::exp(-b1 * 5.0f) : 0.0f; break;
                }
                if (!on) amp = 0.0f;
                const float half = 0.55f * pad.radiusWu, y0 = c.y + 0.42f * pad.radiusWu, h = 0.18f * pad.radiusWu * amp;
                Vec2 prev{c.x - half, y0};
                const Vec3 sc = on ? col * 0.9f : W1 * 0.3f;
                for (u32 j = 1; j <= 10; ++j) {
                    const float u = static_cast<float>(j) / 10.0f;
                    const float wave = std::sin(u * kTau * (k == 2 || k == 6 ? 1.0f : 2.5f + static_cast<float>(k)) + static_cast<float>(k)) * std::sin(u * kPi);
                    const Vec2 q{c.x - half + 2.0f * half * u, y0 - h * wave};
                    d.scene.line(toScene(prev), toScene(q), 0.35f * refPx_, sc, sc, 0);
                    prev = q;
                }
            }
            if (waiting) words(c, 0.7f * cap14, W1 * 0.9f, "NEXT BAR", pad.radiusWu + 14.0f);
            if (on) {
                d.pools.glow(toScene(c), 1.6f * pad.radiusWu * pxPerWu_, mix(modeA, modeB, 0.5f) * 0.18f);
                const Vec3 lc = mix(modeA, modeB, 0.5f) * 0.6f;
                d.scene.line(toScene(c + Vec2{0.0f, -pad.radiusWu}), toScene(fixture(k)), 0.4f * refPx_, lc, W1 * 0.8f, 0);
            }
            continue;
        }
        // A hold pad: its ring, its word, the ship's hold filling the ring in the player's colour (the Trio's).
        const float held = pad.holdBeats > 0.0f ? clampf(js.hold[me < kMaxPlayers ? me : 0][k] / pad.holdBeats, 0.0f, 1.0f) : 0.0f;
        char label[32] = {}, sub[32] = {};
        Vec3 col = W1 * 0.8f;
        float capW = cap14;
        if (k == ModesData::Jam::kPadDrop) {
            // DROP: the largest thing on the floor: a double ring, its word at cap 30 wu, four beat dots; resting, its outer ring the
            // clock filling over the rest; ready, it swells on every beat.
            const u32 bar = w.phrase().bar;
            const bool busy = w.director().jamDropBar != kNoBar && bar < w.director().jamDropBar + jd.dropBuildBars + jd.dropBars;
            const bool resting = busy || bar < js.dropReadyBar;
            const float swell = resting ? 1.0f : 1.0f + 0.08f * std::exp(-beatIn * 5.0f);
            std::snprintf(label, sizeof label, "DROP");
            capW = 30.0f * pxPerWu_ * swell;
            col = resting ? W1 * 0.4f : mix(modeA, modeB, beatIn) * 1.8f;
            ring(c, pad.radiusWu * swell, 1.0f * refPx_, resting ? W1 * 0.5f : col);
            const float outer = pad.radiusWu * swell + 10.0f;
            if (resting) {
                const float total = static_cast<float>(jd.dropBuildBars + jd.dropBars + jd.dropRestBars);
                const float from = static_cast<float>(js.dropReadyBar) - total;
                const float u = clampf((static_cast<float>(in.music / kTicksPerBar) - from) / total, 0.0f, 1.0f);
                ring(c, outer, 0.8f * refPx_, W1 * 0.25f);
                ring(c, outer, 1.0f * refPx_, mix(modeA, modeB, u) * 1.4f, 0.0f, u);
            } else {
                ring(c, outer, 1.0f * refPx_, col);
                d.pools.glow(toScene(c), 2.2f * pad.radiusWu * pxPerWu_, mix(modeA, modeB, 0.5f) * (0.15f + 0.1f * std::exp(-beatIn * 5.0f)));
            }
            for (u32 b = 0; b < 4; ++b) {
                const Vec2 dot{c.x + (static_cast<float>(b) - 1.5f) * 16.0f, c.y + 0.55f * pad.radiusWu};
                const bool lit = (beatNo % 4u) == b;
                d.sparkles.glow(toScene(dot), (lit ? 3.0f : 2.0f) * refPx_, (lit ? W1 * 1.6f : W1 * 0.5f));
            }
        } else if (k == ModesData::Jam::kPadToys || k == ModesData::Jam::kPadBoosts) {
            const bool toys = k == ModesData::Jam::kPadToys;
            std::snprintf(label, sizeof label, toys ? "TOYS" : "BOOSTS");
            const u8 next = toys ? js.toysNext[me < kMaxPlayers ? me : 0] : js.boostsNext[me < kMaxPlayers ? me : 0];
            // (jam.md rule 5: the label names the shelf the next visit opens: WEAPONS 1 OF 3, RHYTHM, UTILITY, RIFFS 2 OF 2, BOOSTS n OF 3)
            if (toys) {
                static const char* const kGroup[7] = {"WEAPONS", "WEAPONS", "WEAPONS", "RHYTHM", "UTILITY", "RIFFS", "RIFFS"};
                static const u8 kFirst[7] = {0, 0, 0, 3, 4, 5, 5}, kCount[7] = {3, 3, 3, 1, 1, 2, 2};
                const u32 n = next < 7u ? next : 0u;
                std::snprintf(sub, sizeof sub, "%s %u OF %u", kGroup[n], static_cast<unsigned>(n - kFirst[n] + 1u), static_cast<unsigned>(kCount[n]));
            } else {
                std::snprintf(sub, sizeof sub, "BOOSTS %u OF %u", static_cast<unsigned>(next + 1u), static_cast<unsigned>(jd.boostShelves));
            }
            col = toys ? modeA * 1.4f : palettes_->player[3] * 1.4f;
            ring(c, pad.radiusWu, 0.8f * refPx_, col);
        } else if (k == ModesData::Jam::kPadEmpty) {
            std::snprintf(label, sizeof label, "EMPTY");
            ring(c, pad.radiusWu, 0.75f * refPx_, col);
        } else if (k == ModesData::Jam::kPadSound) {
            std::snprintf(label, sizeof label, "SOUND");
            // (its next sound named under it: the band's whole sound steps on from the player's own)
            static const char* const kSounds[3] = {"NEXT: CLUB", "NEXT: ARCADE", "NEXT: VELVET"};
            std::snprintf(sub, sizeof sub, "%s", kSounds[(jamBaseSound_ + js.soundNext + 1u) % 3u]);
            ring(c, pad.radiusWu, 0.75f * refPx_, col);
        } else {
            // A Track pad: in its Track's accent with its number inside; the one playing a white ring and its name over it.
            const u32 track = k - ModesData::Jam::kPadTrack0 + 1u;
            Vec3 tc = accent;
            for (u32 q = 0; q < palettes_->count; ++q)
                if (palettes_->list[q].track == track) tc = palettes_->list[q].accent;
            std::snprintf(label, sizeof label, "%u", static_cast<unsigned>(track));
            col = tc * 1.5f;
            const bool playing = w.director().jamTrack == track;
            const bool coming = w.director().jamChangeBar != kNoBar && w.director().jamNext == track;
            ring(c, pad.radiusWu, 0.75f * refPx_, col);
            if (playing || coming) ring(c, pad.radiusWu + 6.0f, 0.9f * refPx_, playing ? W1 * 1.6f : (blinkOn ? W1 * 1.2f : W1 * 0.3f));
            if (playing) words(c, 0.8f * cap14, W1 * 1.2f, songName(w.phrase().song), -pad.radiusWu - 16.0f);
        }
        words(c, capW, col, label);
        if (sub[0]) words(c, 0.62f * cap14, col * 0.8f, sub, pad.radiusWu + 14.0f);
        if (held > 0.0f) {
            ring(c, pad.radiusWu + 3.0f, 2.2f * refPx_, player * 2.2f, 0.0f, held);
            d.sparkles.glow(toScene(c + dirOfA(-0.5f * kPi + held * kTau) * (pad.radiusWu + 3.0f)), 8.0f * refPx_, player * 1.6f);
        }
    }
}

void PlayView::drawJamShelves(FrameDraw& d, const ViewInput& in) {
    // jam.md rule 5: a shelf is the Trio's ring (170 wu round where the ship stood, six orbs evenly round it, icon, name and pips as
    // on a Trio's orb); one beat in an orb takes it, every further beat a stack; a Fusion whose halves are full the gold orb above.
    const World& w = *in.world;
    if (w.setup().mode != Mode::Jam || w.setup().versus || !icons_ || !font_) return;
    const ModesData::Jam& jd = w.species().modes.jam;
    const Vec3 W1 = whiteC(), gold = palettes_->player[3];
    for (u32 p = 0; p < w.playerCount() && p < kMaxPlayers; ++p) {
        const World::JamState::Shelf& s = w.jam().shelf[p];
        if (s.pad == 0xFF || !w.player(p).present) continue;
        const bool mine = p == w.viewPlayer();
        const float lv = mine ? 1.0f : 0.4f;
        const Vec3 player = palettes_->player[p], light = palettes_->playerLight[p];
        const float closing = s.closeAt >= 0.0 ? clampf(static_cast<float>((s.closeAt - in.music) / (static_cast<double>(jd.closesAfterBars) * kTicksPerBar)), 0.0f, 1.0f) : 1.0f;
        for (u32 seg = 0; seg < 96; ++seg) {
            const float a0 = kTau * static_cast<float>(seg) / 96.0f, a1 = kTau * static_cast<float>(seg + 1u) / 96.0f;
            const Vec3 c = (s.pad == 0 ? player : gold) * (0.5f * lv * closing);
            d.scene.line(toScene(s.centre + dirOfA(a0) * jd.ringWu), toScene(s.centre + dirOfA(a1) * jd.ringWu), 0.6f * refPx_, c, c, 0);
        }
        for (u32 k = 0; k < 7; ++k) {
            const u8 u = w.jamShelfOrb(p, k);
            if (u == kNoUpgrade) continue;
            const Vec2 at = w.jamShelfOrbPos(p, k);
            if (!visible(at, 120.0f)) continue;
            const Vec2 sp = toScene(at);
            const float orbWu = tuning::kTrioOrbWu;
            const i32 icon = orbIcon(u);
            const bool boostOrb = isBoostOrb(u), fusionOrb = isFusionOrb(u);
            const u32 level = u < kUpgradeCount ? w.player(p).loadout.level(u) : 0u;
            const u32 capN = u < kUpgradeCount ? upgradeInfo(u).cap : 0u;
            const bool full = capN && level >= capN;
            if (icon >= 0) icons_->draw(d, icon, sp, orbWu * pxPerWu_ / icons_->size(icon), (full ? 0.5f : 1.1f) * lv * closing);
            if (fusionOrb) d.pools.glow(sp, orbWu * 2.8f * pxPerWu_, gold * (0.25f * lv));
            const float ringR = tuning::kTrioHoldRing * orbWu;
            const float hold = s.orbHold[k] > 0.0f ? frac01(s.orbHold[k] / jd.takeBeats) : 0.0f;
            for (u32 seg = 0; seg < 48; ++seg) {
                const float u0 = static_cast<float>(seg) / 48.0f, u1 = static_cast<float>(seg + 1u) / 48.0f;
                const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + u1 * kTau;
                const bool filled = mine && u0 < hold;
                const Vec3 c = filled ? player * 2.3f : (fusionOrb ? gold * 1.4f : W1 * (0.55f * lv * closing));
                d.scene.line(toScene(at + dirOfA(a0) * ringR), toScene(at + dirOfA(a1) * ringR), (filled ? 2.6f : 0.6f) * refPx_, c, filled ? light * 2.0f : c, 0);
            }
            if (!mine) continue;
            // The name under the orb and its pips (stacks of its cap), as on a Trio's orb.
            const char* name = boostOrb ? boostInfo(boostOfOrb(u)).name : (fusionOrb ? fusionInfo(fusionOfOrb(u)).name : upgradeInfo(u).name);
            const float capName = 11.0f * pxPerWu_;
            const float wt = font_->measure(TextStyle{capName}, name);
            const Vec3 nc = (fusionOrb ? gold : W1) * (1.3f * closing);
            font_->drawField(d.scene, Vec2{sp.x - 0.5f * wt, sp.y + (ringR + 8.0f) * pxPerWu_}, capName, 0.6f * refPx_, nc, nc, nc, name);
            for (u32 q = 0; q < capN && q < 12; ++q) {
                const float x = sp.x + (static_cast<float>(q) - 0.5f * static_cast<float>(capN - 1u)) * 9.0f * refPx_;
                const Vec2 pp{x, sp.y + (ringR + 8.0f) * pxPerWu_ + capName + 9.0f * refPx_};
                d.sparkles.glow(pp, 2.4f * refPx_, q < level ? player * 1.8f : W1 * 0.35f);
            }
        }
    }
}

}  // namespace rf
