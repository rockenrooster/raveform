// M8 part 5 (design/modes/songline.md "Look"): SONGLINE's view. The dotted guide a bar ahead of the Spark, the gates (one instanced
// ring glyph each, the accent turned by pitch, snapping open as their note sounds, a missed one fading grey), the Spark (a four-point
// star on the priority layer with its pool and its tail), the streak's rings round each ship, the thread of light to the Spark when a
// gate is taken in time, the bursts, the finale's firework and the edge chevron; and the effects of SONGLINE's events.

#include <cmath>
#include <cstdio>

#include "core/args.h"
#include "game/oklch.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kStandardEnemyWu = 14.0f;  // (play_view.cpp's: look.md 4, glyph sizes relative to a 14 wu enemy)
Vec3 whiteC() { return {1.0f, 1.0f, 1.0f}; }
Vec2 lerp2(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec2 dir(float a) { return Vec2{std::cos(a), std::sin(a)}; }
Vec3 songColour(const World& w, bool to) {
    const ModesData::Info& in = w.species().modes.of(Mode::Songline);
    const float* c = to ? in.to : in.from;
    return oklchFull(c[0], c[1]);
}
Vec3 labOf(Vec3 c) {
    const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    return {0.2104542553f * l + 0.7936177850f * m - 0.0040720468f * s, 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s,
            0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s};
}
Vec3 clamp01(Vec3 v) { return {clampf(v.x, 0.0f, 1.0f), clampf(v.y, 0.0f, 1.0f), clampf(v.z, 0.0f, 1.0f)}; }
}  // namespace

Vec3 PlayView::gateHue(const World& w, u8 note) const {
    // The lead-note rule (GAME_DESIGN 11.1): the hue turns with the pitch, here 4 degrees a semitone round the accent, about the
    // middle of the Track's arpeggios.
    const Vec3 lab = labOf(palette_->accent);
    const float c = std::sqrt(lab.y * lab.y + lab.z * lab.z), h = std::atan2(lab.z, lab.y);
    const float turn = clampf(4.0f * (static_cast<float>(note) - songMid_), -60.0f, 60.0f) * (kTau / 360.0f);
    (void)w;
    return clamp01(oklabToLinear(lab.x, c * std::cos(h + turn), c * std::sin(h + turn)));
}

void PlayView::drawSongline(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    if (w.setup().mode != Mode::Songline) return;
    const ModesData::Songline& sd = w.species().modes.songline;
    const float t = static_cast<float>(in.time);
    const Vec3 W1 = whiteC(), accent = palette_->accent;
    const double tps = w.ticksPerSecond();
    const float beatS = static_cast<float>(kTicksPerBeat / tps), sixteenthS = 0.25f * beatS;
    const u32 me = w.viewPlayer();
    const SongSpark& sp = w.spark();
    const Vec2 spark = lerp2(sp.prevPos, sp.pos, in.alpha);
    {
        // The middle of the Track's tune (its gates' hues turn about it).
        const ModesData::Tune& tn = w.species().modes.tunes[(w.setup().track >= 1 && w.setup().track <= kModeTracks ? w.setup().track : 1u) - 1u];
        float sum = 0.0f;
        u32 n = 0;
        for (u32 c = 0; c < tn.chords; ++c)
            for (u32 j = 0; j < 16; ++j) {
                sum += static_cast<float>(tn.arp[c][j]);
                ++n;
            }
        songMid_ = n ? sum / static_cast<float>(n) : 72.0f;
    }
    // The Spark's tail: where it flew over the last beat (the view's own record).
    if (sparkTailN_ == 0 || length(spark - sparkTail_[(sparkTailHead_ + kSparkTail - 1u) % kSparkTail]) > 4.0f) {
        sparkTail_[sparkTailHead_] = spark;
        sparkTailT_[sparkTailHead_] = t;
        sparkTailHead_ = (sparkTailHead_ + 1u) % kSparkTail;
        if (sparkTailN_ < kSparkTail) ++sparkTailN_;
    }

    // ---- The line ahead (rule 5): dots 24 wu apart for a bar ahead of the Spark, 1.5 px at 0.35 in the accent, brightening toward
    // the Spark. In the scene's light only: a hint.
    if (shown(kMuteFx)) {
        float carry = 24.0f, along = 0.0f, total = 0.0f;
        for (u32 k = 1; k <= 16; ++k) total += length(sp.ahead[k] - sp.ahead[k - 1u]);
        Vec2 a = spark;
        for (u32 k = 1; k <= 16; ++k) {
            const Vec2 b = sp.ahead[k];
            const float seg = length(b - a);
            if (seg < 1e-3f) continue;
            float s = carry;
            while (s <= seg) {
                const Vec2 p = a + (b - a) * (s / seg);
                const float u = clampf((along + s) / std::fmax(total, 1.0f), 0.0f, 1.0f);
                // (0.35 of the guide's own scale: the page's light reads as a dot only twice as bright here; PROGRESS, M8 part 5)
                const Vec3 c = accent * (1.4f * (1.0f - 0.68f * u));
                d.scene.line(toScene(p), toScene(p), 1.0f * refPx_, c, c, kDisc);
                s += 24.0f;
            }
            carry = s - seg;
            along += seg;
            a = b;
        }
    }

    // ---- The gates: one ring glyph each (there are hundreds), 1.6 px, the accent turned by pitch at 1.8 with a faint inner ring at
    // 0.6; another player's at a third; it snaps open from a point in a sixteenth as its note sounds; its last beat fading, a missed
    // one fading grey. Under the crowd's gain (look.md 3): they are many and each needs only its place.
    const double life = static_cast<double>(sd.lifeBars) * kTicksPerBar, fade = static_cast<double>(sd.fadeBeats) * kTicksPerBeat;
    const float gain = std::fmin(1.0f, 0.4f + 0.6f * crowdGain_);
    const int ring = gRing32_ >= 0 ? gRing32_ : gRing64_;
    for (u32 k = 0; k < World::kSongGates; ++k) {
        const SongGate& g = w.songGate(k);
        if (g.born < 0.0 || g.born > in.music) continue;
        if (w.setup().versus ? g.owner != 0xFF : ((g.threaded >> me) & 1u) != 0) continue;  // (taken: gone in its burst)
        const bool mine = g.string == 0xFF || g.string == me;
        if (!visible(g.pos, 80.0f)) continue;
        const double age = in.music - g.born;
        const float open = clampf(static_cast<float>(age / tps) / sixteenthS, 0.0f, 1.0f);
        float level = mine ? 1.0f : 0.33f;
        Vec3 c;
        if (g.kind == 2) {
            c = palettes_->player[3];  // (a gold gate: P4's gold)
        } else if (g.kind == 1) {
            c = W1;  // (the Break's: white, slow)
        } else {
            c = g.string != 0xFF && g.string != me ? palettes_->player[g.string < kMaxPlayers ? g.string : 0] : gateHue(w, g.note);
            if (g.string != 0xFF && !w.setup().versus && w.playerCount() > 1 && g.string == me) c = mix(gateHue(w, g.note), palettes_->player[me], 0.35f);
        }
        if (g.missed || age > life) {
            const float u = clampf(static_cast<float>((age - life) / fade), 0.0f, 1.0f);
            c = mix(c, Vec3{0.35f, 0.35f, 0.35f}, 0.8f);
            level *= 0.6f * (1.0f - u);
        } else if (age > life - static_cast<double>(kTicksPerBeat)) {
            level *= 0.55f + 0.45f * clampf(static_cast<float>((life - age) / kTicksPerBeat), 0.0f, 1.0f);
        }
        if (level <= 0.01f) continue;
        const float r = w.gateRadius(g) * (0.15f + 0.85f * open);
        const float lv = level * gain * (g.kind == 2 ? 1.0f + 0.4f * kick_ : 1.0f);
        glyph(d.glyphs, ring, false, g.pos, 0.0f, 0.0f, r, 1.8f * lv, 0.8f, kGlyphFlat, c, c);
        glyph(d.glyphs, ring, false, g.pos, 0.0f, 0.0f, 0.72f * r, 0.6f * lv, 0.5f, kGlyphFlat, c, c);
        if (g.kind == 2 && mine) {
            // (gold: a four-arm flare swelling on the beat and a tall flare seen from afar, the Boost's icon inside at half size)
            const float s = (0.6f + 0.4f * kick_) * level;
            const Vec3 gc = palettes_->player[3] * (1.6f * s);
            d.scene.line(toScene(g.pos + Vec2{-90.0f, 0.0f}), toScene(g.pos + Vec2{90.0f, 0.0f}), 0.7f * refPx_, Vec3{}, Vec3{}, 0);
            d.scene.line(toScene(g.pos + Vec2{-70.0f, 0.0f}), toScene(g.pos + Vec2{70.0f, 0.0f}), 0.6f * refPx_, gc, gc, 0);
            d.scene.line(toScene(g.pos + Vec2{0.0f, -70.0f}), toScene(g.pos + Vec2{0.0f, 70.0f}), 0.6f * refPx_, gc, gc, 0);
            d.scene.line(toScene(g.pos) + Vec2{0.0f, -60.0f * refPx_}, toScene(g.pos) + Vec2{0.0f, 60.0f * refPx_}, 0.5f * refPx_, gc * 0.6f, gc * 0.6f, 0);
            if (g.boost < kBoostCount && boostIcon_[g.boost] >= 0 && icons_)
                d.sparkles.glow(toScene(g.pos), 0.5f * r * pxPerWu_, palettes_->player[3] * (0.5f * s));
        }
    }

    // ---- In time (rule 7): a thin line of light joins the ship and the Spark for that sixteenth.
    for (u32 p = 0; p < w.playerCount() && p < kMaxPlayers; ++p) {
        const float age = t - threadLineT_[p];
        if (age < 0.0f || age > sixteenthS) continue;
        const PlayerState& pl = w.player(p);
        if (!pl.present || !pl.alive) continue;
        const Vec2 ship = lerp2(pl.prevPos, pl.pos, in.alpha);
        const float k = 1.0f - age / sixteenthS;
        const Vec3 c = W1 * (1.6f * k), pc = palettes_->playerLight[p] * (1.6f * k);
        d.scene.line(toScene(ship), toScene(spark), 0.5f * refPx_, pc, c, 0);
    }

    // ---- The streak's rings (rule 9): one to four thin rings round the ship outside the halo, 1 px at 1.2, the mode's two colours
    // running round them; a new one snaps on at each tier (a ring leaves the ship: the event's).
    {
        const Vec3 a = songColour(w, false), b = songColour(w, true);
        const float shipScale = kStandardEnemyWu * (gShip_ >= 0 ? glyphs_->def(static_cast<u32>(gShip_)).size : 1.0f);
        const int r64 = gRing64_ >= 0 ? gRing64_ : ring;
        for (u32 p = 0; p < w.playerCount() && p < kMaxPlayers; ++p) {
            const PlayerState& pl = w.player(p);
            if (!pl.present || !pl.alive || pl.streakTier == 0) continue;
            const Vec2 ship = lerp2(pl.prevPos, pl.pos, in.alpha);
            const float rot = 0.6f * t * (p & 1u ? -1.0f : 1.0f);
            for (u32 j = 0; j < pl.streakTier && j < 4; ++j) {
                const float snap = clampf((t - streakT_[p]) / 0.08f, 0.0f, 1.0f);
                const float grow = j + 1u == pl.streakTier ? 0.6f + 0.4f * snap : 1.0f;
                const float r = shipScale * (1.55f + 0.22f * static_cast<float>(j + 1u)) * grow;
                glyph(d.glyphs, r64, false, ship, rot + 0.7f * static_cast<float>(j), rot, r, 1.2f * (p == me ? 1.0f : 0.7f), 0.5f, kGlyphRingA, a, b);
            }
        }
    }

    // ---- The Spark: a four-point star, arms 60 and 36 wu, white at 3.5 with a pool of the accent (radius 120 wu, 0.2), swelling a
    // fifth on each note it plays; its tail the line it flew over the last beat, 2.4 px, white running to the accent and out. On the
    // priority layer with the dark rim, like the ship: it must be findable in a Drop.
    if (!in.noShips && shown(kMutePriority)) {
        const float swell = 1.0f + 0.2f * std::exp(-(t - sparkNoteT_) / 0.09f);
        const float head = sp.heading;
        const Vec2 along = dir(head), across = dir(head + 0.5f * kPi);
        // The tail (newest to oldest).
        Vec2 prev = spark;
        for (u32 j = 1; j < sparkTailN_; ++j) {
            const u32 idx = (sparkTailHead_ + kSparkTail - j) % kSparkTail;
            const float age = t - sparkTailT_[idx];
            if (age > beatS) break;
            const float u = age / beatS;
            const Vec3 c0 = mix(W1, accent, std::fmin(1.0f, 2.0f * u)) * (2.6f * (1.0f - u));
            d.scene.line(toScene(prev), toScene(sparkTail_[idx]), 1.2f * refPx_, c0, c0, 0);
            prev = sparkTail_[idx];
        }
        d.pools.glow(toScene(spark), 120.0f * pxPerWu_, accent * (0.2f * swell));
        const float l1 = 60.0f * swell, l2 = 36.0f * swell;
        const Vec2 tips[4] = {spark + along * l1, spark - along * l1, spark + across * l2, spark - across * l2};
        // The arms in the scene, each tapering in three steps from the white core to its tip in the accent (the bloom makes the star).
        for (u32 j = 0; j < 4; ++j) {
            const Vec2 v = tips[j] - spark;
            static constexpr float kStep[4] = {0.0f, 0.3f, 0.62f, 1.0f}, kHalf[3] = {3.2f, 1.9f, 0.9f};
            for (u32 q = 0; q < 3; ++q) {
                const Vec3 c0 = mix(W1 * 3.5f, accent * 2.0f, kStep[q]), c1 = mix(W1 * 3.5f, accent * 1.2f, kStep[q + 1u]);
                d.scene.line(toScene(spark + v * kStep[q]), toScene(spark + v * kStep[q + 1u]), kHalf[q] * refPx_, c0, c1, 0);
            }
        }
        d.sparkles.glow(toScene(spark), 18.0f * refPx_ * swell, W1 * 2.4f);
        // The priority layer (look.md 9, as the ship): a dark-rimmed white heart, so it is found in a Drop (the arms stay in the scene's
        // light: rimmed, they read as a gun sight).
        d.threatRims.line(toScreen(spark), toScreen(spark), 6.0f * refPx_ * swell, Vec3{}, Vec3{}, kDisc);
        d.threatCores.line(toScreen(spark), toScreen(spark), 3.6f * refPx_ * swell, W1, W1, kDisc);
        // Off screen: the boss's double chevron in the accent, swelling on the beat.
        if (!visible(spark, 0.0f)) edgeChevron(d, spark, accent, true, 1.0f + 0.2f * kick_);
    }

    // ---- The finale (songline.md "The finale"): on the last downbeat eight rings in the palette's hues one after another a
    // thirty-second apart, to 900 wu (the event's), a clamped white flash (not with reduce flashing).
    {
        const float age = t - finaleT_;
        if (age >= 0.0f && age < 0.25f && !reduceFlashing_) {
            const float k = 1.0f - age / 0.25f;
            d.pools.glow(toScene(finaleAt_), 1400.0f * pxPerWu_, W1 * (0.35f * k));
        }
    }
}

void PlayView::songlineEvent(const World& w, const Event& e, float t) {
    const Vec2 at{e.x, e.y};
    const u32 owner = e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const Vec3 col = palettes_->player[owner], light = palettes_->playerLight[owner], W1 = whiteC();
    const float beatS = static_cast<float>(kTicksPerBeat / w.ticksPerSecond());
    switch (e.type) {
        case EventType::GateLay: sparkNoteT_ = t; break;
        case EventType::GateThread: {
            // A gate threaded: eight sparks along the ship's heading in the player's colour, its ring leaving as a small shock ring
            // (to 70 wu in 0.15 s), a 9 px sparkle at the ship; in time the ring is white and twice as bright and a line joins the
            // ship and the Spark for that sixteenth. The Break's ripple the lattice; gold flares.
            const PlayerState& pl = w.player(owner);
            const float heading = lengthSq(pl.vel) > 1.0f ? angleOf(pl.vel) : 0.0f;
            const bool inTime = e.c != 0;
            const u8 kind = static_cast<u8>(e.f);
            SparkReq r{};
            r.at = at;
            r.dir = dir(heading);
            r.count = kind == 2 ? 24 : 8;
            r.kind = 0;
            r.a = col;
            r.b = light;
            r.t0 = t;
            sparkRequest(r);
            if (inTime) addShockRings(at, 1, kind == 1 ? 140.0f : 70.0f, W1, W1, t, 0.15f);
            else addShockRings(at, 1, kind == 1 ? 140.0f : 70.0f, col, light, t, 0.15f);
            if (inTime && owner < kMaxPlayers) threadLineT_[owner] = t;
            if (kind == 1 && !reduceFlashing_) push(kImpKill, at, 90.0f, 160.0f, t);
            if (kind == 2) addPool(at, 60.0f, 140.0f, palettes_->player[3], 0.35f, 0.0f, 0.5f * beatS, false, 1, true, false, t);
            break;
        }
        case EventType::StreakTier:
            if (owner < kMaxPlayers) {
                streakT_[owner] = t;
                const PlayerState& pl = w.player(owner);
                const float shipScale = kStandardEnemyWu * (gShip_ >= 0 ? glyphs_->def(static_cast<u32>(gShip_)).size : 1.0f);
                const float r = shipScale * (1.55f + 0.22f * static_cast<float>(e.b + 1u));
                if (e.c) {
                    addShockRings(pl.pos, 1, r * 3.0f, songColour(w, false), songColour(w, true), t, 0.3f);  // (a ring leaving the ship)
                } else {
                    // (a step down: the outermost ring shatters outward)
                    SparkReq q{};
                    q.at = pl.pos;
                    q.dir = Vec2{1.0f, 0.0f};
                    q.count = 16;
                    q.kind = 0;
                    q.a = songColour(w, false);
                    q.b = songColour(w, true);
                    q.t0 = t;
                    sparkRequest(q);
                }
            }
            break;
        case EventType::SparkBack: break;
        case EventType::SongChord:
            // Co-op's chord: that step bursts white on every string.
            for (u32 k = 0; k < World::kSongGates; ++k) {
                const SongGate& g = w.songGate(k);
                if (g.born < 0.0 || std::fabs(g.born - e.tick) > 0.01 || g.string == 0xFF) continue;
                addShockRings(g.pos, 1, 90.0f, W1, W1, t, 0.2f);
            }
            if (!reduceFlashing_) push(kImpKill, at, 80.0f, 140.0f, t);
            break;
        case EventType::GateOwner:
            // Versus: the host gave the gate to another: it turns that player's colour as it goes.
            if (e.a != w.viewPlayer()) addShockRings(at, 1, 60.0f, col, light, t, 0.15f);
            break;
        case EventType::GateCount:
            if (e.a == w.viewPlayer() || finaleT_ < t - 1.0f) {
                // The finale's burst: eight rings in the palette's hues a thirty-second apart to 900 wu, three hundred sparks.
                finaleT_ = t;
                finaleAt_ = at;
                const float thirty2 = beatS / 8.0f;
                for (u32 j = 0; j < 8; ++j) {
                    const Vec3 c = j % 4 == 3 ? palette_->accent : palette_->hue[j % 3];
                    addShockRings(at, 1, 900.0f, c, mix(c, W1, 0.4f), t + thirty2 * static_cast<float>(j), 0.6f);
                }
                SparkReq q{};
                q.at = at;
                q.dir = Vec2{1.0f, 0.0f};
                q.count = 300;
                q.kind = 1;
                q.a = palette_->accent;
                q.b = whiteC();
                q.t0 = t;
                sparkRequest(q);
                shakeAmp_ = std::fmax(shakeAmp_, 6.0f);
                if (!reduceFlashing_) push(kImpDropWave, at, 300.0f, 900.0f, t);
            }
            break;
        default: break;
    }
}

}  // namespace rf
