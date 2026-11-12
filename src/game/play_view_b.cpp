// The view of M3 stage B: the phrase cycle on screen (the Build drains and zooms in, the Drop snaps out
// with a shockwave, the Break cools), the Trio (design/screens.md), the Riffs, the Boosts and Kaleidos
// (design/bosses/kaleidos.md). Cosmetic only; nothing here touches the simulation.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "content/icons.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kArmStep = kTau / 6.0f;
// Numbers chosen for the view (docs/PROGRESS.md M3 stage B): the look bible gives none for these.
constexpr float kBuildDrain = 0.45f;       // the Build drains colour toward white, up to this
constexpr float kBuildZoomIn = 0.10f;      // ... and zooms in by this much
constexpr float kBuildLastBarExposure = 0.78f;  // its last bar darkens
constexpr float kDropZoomOut = 0.08f;      // the Drop snaps the zoom out by this, easing back over a bar
constexpr float kBreakDrain = 0.22f;       // the Break: cool, desaturated
constexpr float kTrioAnswerWu = 110.0f;    // screens.md (quoted): an orb answers within 110 wu
constexpr float kBoostOrbWu = 20.0f;       // a Boost lying in the arena: its icon frame radius
// M3 stage C, kaleidos.md death: the clamped white flash (0.9, as the M1 boss break's) peaks for 0.12 s.
constexpr float kBossFlash = 0.9f, kBossFlashS = 0.12f;

Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 lerpv(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// OKLCH -> linear sRGB (kaleidos.json accent: lightness 0.83, hue 76, chroma as a fraction of ~0.16).
Vec3 oklch(float L, float hueDeg, float chroma) {
    const float h = hueDeg * kTau / 360.0f;
    const float a = chroma * std::cos(h), b = chroma * std::sin(h);
    const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return Vec3{std::fmax(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, 0.0f),
                std::fmax(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s, 0.0f),
                std::fmax(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, 0.0f)};
}
}  // namespace

void PlayView::sectionLook(const ViewInput& in) {
    const World& w = *in.world;
    const Phrase& ph = w.phrase();
    const double barStart = w.tickOfBar(ph.bar);
    const float inBar = clamp01(static_cast<float>((in.music - barStart) / kTicksPerBar));
    const float u = ph.sectionBars ? clamp01((static_cast<float>(ph.sectionBar) + inBar) / static_cast<float>(ph.sectionBars)) : 0.0f;
    float drain = 0.0f, zoom = 1.0f;
    exposureSection_ = 1.0f;
    switch (ph.section) {
        case Section::Build:
            // GAME_DESIGN 4.1: colours drain toward white, slow zoom-in; the last bar darkens.
            drain = kBuildDrain * u;
            zoom = 1.0f + kBuildZoomIn * u;
            if (ph.buildLastBar()) exposureSection_ = kBuildLastBarExposure;
            break;
        case Section::Drop:
            if (!ph.bossDrop || ph.sectionBar < 2) {
                // The zoom snaps out on the downbeat and eases back.
                const float since = static_cast<float>(ph.sectionBar) + inBar;
                zoom = 1.0f - kDropZoomOut * std::exp(-since * 2.5f);
            }
            break;
        case Section::Break: drain = kBreakDrain; break;
        default: break;
    }
    if (reduceFlashing_) drain *= 0.5f;
    const float k = 1.0f - std::exp(-in.dt / 0.35f);
    drain_ += (drain - drain_) * k;
    if (ph.section == Section::Drop && zoom < sectionZoom_) {
        sectionZoom_ = zoom;  // the snap
    } else {
        sectionZoom_ += (zoom - sectionZoom_) * k;
    }
    // The Trio: the fight steps back (screens.md): the lattice rests, enemies at a fifth of their light.
    const bool trio = w.trio(0).open;
    trioRest_ += ((trio ? 1.0f : 0.0f) - trioRest_) * (1.0f - std::exp(-in.dt / 0.2f));
    enemyLight_ = 1.0f - 0.8f * trioRest_;
}

void PlayView::eventB(const World& w, const Event& e, float t) {
    const Vec3 player = palettes_->player[0], light = palettes_->playerLight[0];
    const Vec2 at{e.x, e.y};
    switch (e.type) {
        case EventType::Section:
            if (e.a == static_cast<u8>(Section::Drop)) {
                // GAME_DESIGN 4.1: bloom burst, a shockwave from every player, the zoom snaps out.
                const Vec2 p = w.player(0).pos;
                addShockRings(p, 2, 900.0f, player, light, t, 0.45f);
                if (!reduceFlashing_) novaPump_ = std::fmax(novaPump_, 0.5f);
                lattice_.impulse(p, 600.0f, 400.0f);
            }
            break;
        case EventType::TrioTake: {
            // The orb folds into the ship in two beats of sparks in its two colours.
            SparkReq r{};
            r.at = at;
            r.dir = Vec2{};
            r.count = 90;
            r.kind = 6;  // inhale: toward the ship
            r.a = player;
            r.b = light;
            r.t0 = t;
            r.extra = 2.0f * 60.0f / std::fmax(w.bpm(), 60.0f);
            sparkRequest(r);
            addPool(at, 30.0f, 90.0f, player, 1.6f, 0.0f, 0.5f, false, 1, false, false, t);
            break;
        }
        case EventType::TrioPop: {
            SparkReq r{};
            r.at = at;
            r.count = 40;
            r.kind = 1;
            const u8 u = e.b;
            r.a = isRiff(u) ? palette_->hue[1] : palette_->accent;
            r.b = whiteV();
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        case EventType::BoostTake: {
            addShockRings(at, 1, 160.0f, palettes_->player[3], whiteV(), t, 0.3f);
            addPool(at, 20.0f, 120.0f, palettes_->player[3], 1.4f, 0.0f, 0.4f, false, 1, false, false, t);
            break;
        }
        case EventType::HaloBlock:
            addShockRings(at, 1, 120.0f, palettes_->player[3], whiteV(), t, 0.25f);
            break;
        case EventType::BossPair: {
            // kaleidos.md: both arms shatter outward along their axis in mirror image; the lens plates
            // crack and fall inward; the wheel flinches.
            const Vec3 hue = palette_->hue[e.a % 3], partner = palette_->partner[e.a % 3];
            for (u32 m = 0; m < 2; ++m) {
                const Vec2 d = dirv(e.f + static_cast<float>(m) * kPi);
                for (u32 k = 0; k < 5; ++k) {
                    SparkReq r{};
                    r.at = at + d * (100.0f + 40.0f * static_cast<float>(k));
                    r.dir = d;
                    r.count = 26;
                    r.kind = 1;
                    r.a = hue;
                    r.b = partner;
                    r.t0 = t;
                    sparkRequest(r);
                }
                addShockRings(at + d * 264.0f, 1, 140.0f, hue, partner, t, 0.3f);
            }
            addDistortion(at, 420.0f, 0.35f, 14.0f, 18.0f, t);
            shakeAmp_ = std::fmax(shakeAmp_, 4.0f);
            break;
        }
        case EventType::BossDeath: {
            // Iris wide open, the clamped white flash, facets tumbling outward in six-fold symmetry, three
            // rings, 300 sparks, the 30 px distortion ring; Play holds the action 50 ms. M3 stage C: the white
            // flash is clamped (0.9, like the M1 boss break) and peaks for 0.12 s; the accent glow stays after it.
            const BossData& b = w.bossData();
            const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
            addPool(at, 60.0f, 420.0f, whiteV(), reduceFlashing_ ? 0.45f : kBossFlash, 0.0f, kBossFlashS, false, 1, false, false, t);
            addPool(at, 120.0f, 520.0f, acc, reduceFlashing_ ? 0.3f : 0.6f, 0.0f, 0.45f, false, 1, false, false, t);
            addShockRings(at, 3, 1100.0f, acc, whiteV(), t, 0.6f);
            SparkReq r{};
            r.at = at;
            r.count = 300;
            r.kind = 1;
            r.a = acc;
            r.b = whiteV();
            r.t0 = t;
            sparkRequest(r);
            addDistortion(at, 900.0f, 0.6f, 30.0f, 28.0f, t);
            shakeAmp_ = std::fmax(shakeAmp_, 6.0f);
            const float wheel = w.boss().angle;
            for (u32 k = 0; k < 6; ++k) {
                for (u32 f = 0; f < 4; ++f) {
                    if (tumbleCount_ >= kTumbles) break;
                    Tumble& tb = tumble_[tumbleCount_++];
                    const float a = wheel + static_cast<float>(k) * kArmStep;
                    tb.p = at + dirv(a) * b.facets[f].armRadius;
                    tb.v = dirv(a) * (260.0f + 120.0f * static_cast<float>(f)) + dirv(a + 1.5708f) * rng_.range(-60.0f, 60.0f);
                    tb.ang = a;
                    tb.spin = rng_.range(-6.0f, 6.0f);
                    tb.facet = static_cast<u8>(f);
                    tb.pair = static_cast<u8>(k % 3);
                    tb.born = t;
                }
            }
            break;
        }
        case EventType::Impact:
            if (e.species == kBossSpecies) {
                SparkReq r{};
                r.at = at;
                r.dir = Vec2{-e.x2, -e.y2};
                r.count = static_cast<u16>(e.c == 1 ? 3 : 4);
                r.kind = 3;
                const BossData& b = w.bossData();
                const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
                r.a = e.c == 2 ? palette_->hue[e.a % 3] : acc;
                r.b = e.f > 1.5f ? whiteV() : r.a;
                r.t0 = t;
                sparkRequest(r);
            }
            break;
        case EventType::PulseRing: break;
        case EventType::Restart: tumbleCount_ = 0; break;
        default: break;
    }
}

void PlayView::drawStageB(FrameDraw& d, const ViewInput& in, float W, float H) {
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    const float a = in.alpha;
    const Vec3 W1 = whiteV();
    const Vec3 player = palettes_->player[0], light = palettes_->playerLight[0], deep = palettes_->playerDeep[0];
    const Vec3 gold = palettes_->player[3];
    const Vec3 accent = palette_->accent;
    const PlayerState& me = w.player(0);
    const float swell = 1.0f + 0.06f * kick_;  // screens.md: orbs swell 6% on the beat
    (void)W;
    (void)H;
    (void)deep;

    // ---- Boosts lying in the arena: the icon in its frame, a gold glow; they fade over their last bar.
    {
        const BoostPickups& bp = w.boostPickups();
        for (u32 i = 0; i < bp.count; ++i) {
            const Vec2 p{bp.x[i], bp.y[i]};
            const float left = static_cast<float>((bp.born[i] + tuning::kBoostLifeBars * kTicksPerBar - in.music) / kTicksPerBar);
            const float fade = clamp01(left);
            if (!visible(p, 60.0f)) {
                edgeChevron(d, p, gold, false, 1.0f);  // look.md §6: gold for events
                continue;
            }
            const Vec2 sp = toScene(p);
            const float r = kBoostOrbWu * pxPerWu_ * (1.0f + 0.08f * kick_);
            if (icons_) icons_->draw(d, boostIcon_[bp.type[i] % kBoostCount], sp, r, fade * 1.1f);
            glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, kBoostOrbWu * 1.5f, 1.4f * fade, 0.5f, kGlyphFlat, gold, W1);
            d.pools.glow(sp, r * 2.6f, gold * (0.18f * fade));
            // M3 stage C: its name under the orb, never across it: gold, cap 11 wu, 6 wu under the ring.
            const char* name = boostInfo(bp.type[i]).name;
            const float cap = 11.0f * pxPerWu_, top = sp.y + (kBoostOrbWu * 1.5f + 6.0f) * pxPerWu_;
            const Vec3 g = gold * (1.3f * fade);
            font_->drawField(d.scene, Vec2{sp.x - 0.5f * font_->measure(TextStyle{cap}, name), top}, cap, 0.8f * refPx_, g, g, g, name);
        }
    }

    // ---- The local player's Riffs (rings made by a player's Riffs use that player's colours, look.md §6).
    if (me.alive) {
        const Vec2 ship = in.shipPos;
        const Loadout& l = me.loadout;
        const bool hyper = me.boost(kHyper);
        if (const u8 lv = l.riffs[riffIndex(kOrbitals)]) {
            const u32 n = lv + 1u;
            const double turn = hyper ? kTicksPerBar * 0.5 : kTicksPerBar;
            const double ph = std::fmod(in.music - w.director().startTick, turn) / turn;
            const float shardScale = 9.0f;
            for (u32 k = 0; k < n; ++k) {
                const float ang = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(n) - 0.25f * kTau;
                const Vec2 o = ship + dirv(ang) * tuning::kOrbitalRadiusWu;
                glyph(d.glyphs, gShard_, false, o, ang * 2.0f, 0.0f, shardScale, 2.2f, 0.6f, kGlyphFlat, light, player);
                d.sparkles.glow(toScene(o), 7.0f * refPx_, player * 0.6f);
            }
        }
        for (const PulseWave& pw : me.pulses) {
            if (!pw.active || pw.age < 0.0f) continue;
            const float age = pw.age + a * in.dt;
            const float u = clamp01(age / tuning::kPulseGrowS);
            const float r = pw.reach * u;
            if (r < 4.0f) continue;
            glyph(d.glyphs, gRing128_, false, pw.centre, 0.0f, 0.0f, r, 2.0f * (1.0f - u * u), 0.6f + 0.6f * (1.0f - u), kGlyphRingW, player, light);
        }
        if (l.riffs[riffIndex(kSentinel)]) {
            const Vec2 s = lerpv(me.prevSentinel, me.sentinel, a);
            glyph(d.glyphs, gMarkC_ >= 0 ? gMarkC_ : gRing32_, false, s, t * 2.0f, t * 2.0f, 8.0f, 2.4f, 0.6f, kGlyphFlat, light, player);
            glyph(d.glyphs, gRing32_, false, s, 0.0f, 0.0f, 11.0f, 1.2f, 0.5f, kGlyphFlat, player, player);
            d.sparkles.glow(toScene(s), 9.0f * refPx_, player * 0.7f);
        }
        // Halo: a gold ring around the ship, one arc per hit it can still take.
        for (const ActiveBoost& b : me.boosts) {
            if (b.type != kHalo || !b.hits) continue;
            const float r = 30.0f;
            for (u32 k = 0; k < b.hits; ++k) {
                const float a0 = -0.5f * kPi + static_cast<float>(k) * kTau / 3.0f + 0.12f, a1 = a0 + kTau / 3.0f - 0.24f;
                const u32 segs = 8;
                for (u32 s = 0; s < segs; ++s) {
                    const float u0 = a0 + (a1 - a0) * static_cast<float>(s) / segs, u1 = a0 + (a1 - a0) * static_cast<float>(s + 1) / segs;
                    d.scene.line(toScene(ship + dirv(u0) * r), toScene(ship + dirv(u1) * r), 1.3f * refPx_, gold * 2.2f, W1 * 1.6f, 0);
                }
            }
        }
    }

    // ---- Kaleidos.
    drawBoss(d, in);

    // ---- The Trio (screens.md): the ring is the clock; three orbs with icon, name, effect words, pips.
    const TrioState& tr = w.trio(0);
    if (tr.open && me.alive && icons_ && font_) {
        const float span = static_cast<float>(tr.closeTick - tr.openTick);
        const float left = span > 1.0f ? clamp01(static_cast<float>((tr.closeTick - in.music) / span)) : 0.0f;
        const Vec2 c = tr.centre;
        const float R = tuning::kTrioRingWu;
        const u32 segs = 96;
        for (u32 s = 0; s < segs; ++s) {
            const float u0 = static_cast<float>(s) / segs, u1 = static_cast<float>(s + 1) / segs;
            const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + u1 * kTau;
            const bool lit = u0 < left;
            const Vec3 col = lit ? mix(accent, W1, 0.5f + 0.5f * std::sin(u0 * kTau * 2.0f)) * 1.3f : W1 * 0.16f;
            d.scene.line(toScene(c + dirv(a0) * R), toScene(c + dirv(a1) * R), (lit ? 1.0f : 0.7f) * refPx_, col, col, 0);
        }
        if (left > 0.0f) {
            const Vec2 bead = c + dirv(-0.5f * kPi + left * kTau) * R;
            d.sparkles.glow(toScene(bead), 8.0f * refPx_, W1 * 1.6f);
        }
        // Which orb answers: the nearest within 110 wu, or the one the ship flies at.
        i32 answer = -1;
        float best = kTrioAnswerWu;
        const Vec2 ship = in.shipPos;
        for (u32 k = 0; k < tr.orbs; ++k) {
            const float dd = length(tr.orbPos[k] - ship);
            if (dd < best) {
                best = dd;
                answer = static_cast<i32>(k);
            }
        }
        if (answer < 0 && length(in.shipVel) > 120.0f) {
            const Vec2 v = normalizeOr(in.shipVel, Vec2{1.0f, 0.0f});
            float bestCos = 0.97f;
            for (u32 k = 0; k < tr.orbs; ++k) {
                const float cs = dot(normalizeOr(tr.orbPos[k] - ship, v), v);
                if (cs > bestCos) {
                    bestCos = cs;
                    answer = static_cast<i32>(k);
                }
            }
        }
        if (tr.homing >= 0) answer = tr.homing;
        for (u32 k = 0; k < tr.orbs; ++k) {
            const u8 u = tr.upgrade[k];
            const bool riff = isRiff(u);
            const bool ans = static_cast<i32>(k) == answer;
            const Vec2 p = tr.orbPos[k];
            const float orbWu = tuning::kTrioOrbWu * (riff ? tuning::kTrioRiffScale : 1.0f) * (ans ? 1.12f : 1.0f) * swell;
            const Vec2 sp = toScene(p);
            icons_->draw(d, upgradeIcon_[u], sp, orbWu * pxPerWu_ / icons_->size(upgradeIcon_[u]) * (riff ? tuning::kTrioRiffScale : 1.0f),
                         ans ? 1.35f : 1.0f);
            // The thin ring at 1.42x; answering: in the player colour at 2.0 with a dashed outer ring and a pool.
            const float ringWu = 1.42f * orbWu;
            const u32 n = 64;
            for (u32 s = 0; s < n; ++s) {
                const float a0 = kTau * static_cast<float>(s) / n, a1 = kTau * static_cast<float>(s + 1) / n;
                const Vec3 col = ans ? player * 2.0f : W1 * 0.7f;
                d.scene.line(toScene(p + dirv(a0) * ringWu), toScene(p + dirv(a1) * ringWu), (ans ? 1.2f : 0.6f) * refPx_, col, col, 0);
                if (ans && s % 2 == 0) {
                    const float r2 = ringWu * 1.18f;
                    d.scene.line(toScene(p + dirv(a0) * r2), toScene(p + dirv(a1) * r2), 0.8f * refPx_, player * 1.4f, player * 1.4f, 0);
                }
            }
            if (ans) {
                d.pools.glow(sp, ringWu * 2.2f * pxPerWu_, player * 0.22f);
                d.scene.line(toScene(ship), toScene(p - normalizeOr(p - ship, Vec2{1.0f, 0.0f}) * ringWu), 1.0f * refPx_, player * 0.4f,
                             light * 1.6f, 0);
            }
            // Words, always outside the ring: above the top orb, left of the left one, right of the right one.
            const UpgradeInfo& info = upgradeInfo(u);
            const char* words = upgradeWords(u, tr.level[k]);
            const float capName = 20.0f * pxPerWu_, capWords = 11.0f * pxPerWu_;
            const float wName = font_->measure(TextStyle{capName}, info.name), wWords = font_->measure(TextStyle{capWords}, words);
            const float gap = (ringWu + 14.0f) * pxPerWu_;
            Vec2 org;  // the name's baseline, left end
            const float lift = ans ? 1.9f : 1.3f;
            float align = 0.0f;  // 0 left-aligned at org.x, 1 right-aligned, 0.5 centred
            if (k == 0) {
                org = Vec2{sp.x, sp.y - gap - capWords - 18.0f * refPx_};
                align = 0.5f;
            } else if (k == 1) {
                org = Vec2{sp.x - gap, sp.y - 4.0f * refPx_};
                align = 1.0f;
            } else {
                org = Vec2{sp.x + gap, sp.y - 4.0f * refPx_};
                align = 0.0f;
            }
            const Vec3 nameCol = W1 * lift, wordCol = accent * (ans ? 1.9f : 1.3f);
            // (The font's origin is the top of its caps; org and wy are baselines.)
            font_->drawField(d.scene, Vec2{org.x - align * wName, org.y - capName}, capName, 0.9f * refPx_, nameCol, nameCol, nameCol, info.name);
            const float wy = org.y + capWords + 12.0f * refPx_;
            font_->drawField(d.scene, Vec2{org.x - align * wWords, wy - capWords}, capWords, 0.7f * refPx_, wordCol, wordCol, wordCol, words);
            // Pips: one per stack or level the upgrade can have; owned white, this pick gold with a ring.
            const u32 cap = info.cap;
            const float pip = 5.0f * refPx_, step = 15.0f * refPx_;
            const float rowW = static_cast<float>(cap - 1) * step;
            const float px0 = org.x - align * rowW, py = wy + 16.0f * refPx_;
            const u32 owned = tr.level[k] > 0 ? tr.level[k] - 1u : 0u;
            for (u32 j = 0; j < cap; ++j) {
                const Vec2 q{px0 + static_cast<float>(j) * step, py};
                if (j < owned) {
                    d.sparkles.glow(q, pip, W1 * 1.2f);
                    d.scene.line(q, q, pip * 0.6f, W1 * 1.4f, W1 * 1.4f, 0);
                } else if (j == owned) {
                    d.scene.line(q, q, pip * 0.6f, gold * 2.0f, gold * 2.0f, 0);
                    for (u32 s = 0; s < 12; ++s) {
                        const float a0 = kTau * static_cast<float>(s) / 12.0f, a1 = kTau * static_cast<float>(s + 1) / 12.0f;
                        d.scene.line(q + dirv(a0) * pip * 1.7f, q + dirv(a1) * pip * 1.7f, 0.5f * refPx_, gold * 1.4f, gold * 1.4f, 0);
                    }
                } else {
                    for (u32 s = 0; s < 8; ++s) {
                        const float a0 = kTau * static_cast<float>(s) / 8.0f, a1 = kTau * static_cast<float>(s + 1) / 8.0f;
                        d.scene.line(q + dirv(a0) * pip, q + dirv(a1) * pip, 0.45f * refPx_, W1 * 0.5f, W1 * 0.5f, 0);
                    }
                }
            }
        }
    }

    // ---- Facets of a dead boss, tumbling outward.
    for (u32 i = 0; i < tumbleCount_; ++i) {
        const Tumble& tb = tumble_[i];
        const float age = t - tb.born;
        if (age < 0.0f || age > 1.6f) continue;
        const BossData& b = w.bossData();
        const BossData::Facet& f = b.facets[tb.facet];
        const Vec2 p = tb.p + tb.v * age;
        const float ang = tb.ang + tb.spin * age;
        const float fade = 1.0f - age / 1.6f;
        const Vec3 col = mix(palette_->hue[tb.pair], W1, 0.4f) * (2.2f * fade);
        const float cs = std::cos(ang), sn = std::sin(ang);
        for (u32 s = 0; s < f.segs; ++s) {
            if (f.seg[s][4] != 0.0f) continue;
            const Vec2 p0 = p + Vec2{f.seg[s][0] * cs - f.seg[s][1] * sn, f.seg[s][0] * sn + f.seg[s][1] * cs};
            const Vec2 p1 = p + Vec2{f.seg[s][2] * cs - f.seg[s][3] * sn, f.seg[s][2] * sn + f.seg[s][3] * cs};
            d.scene.line(toScene(p0), toScene(p1), 1.2f * refPx_, col, col, 0);
        }
    }
}

void PlayView::drawBoss(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    if (!s.active) return;
    const BossData& b = w.bossData();
    const float a = in.alpha;
    const Vec3 W1 = whiteV();
    const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
    const Vec3 danger = palettes_->danger;
    const Vec2 pos = lerpv(s.prevPos, s.pos, a);
    const float wheel = s.prevAngle + wrapAngle(s.angle - s.prevAngle) * a;
    if (!visible(pos, b.radius * 0.5f) && !s.dying) {
        // Edge indicator (look.md §6): the boss's double chevron in its accent, swelling on every beat.
        const float tps = std::fmax(in.bpm, 60.0f) * static_cast<float>(kTicksPerBeat) / 60.0f;
        const float sinceBeat = static_cast<float>(std::fmod(in.music, kTicksPerBeat)) / tps;
        const float beat = std::exp(-sinceBeat * 1000.0f / std::fmax(look_->kickDecayMs, 1.0f));
        edgeChevron(d, pos, acc, true, 1.0f + look_->edge.bossBeatSwell * beat);
    }
    if (!visible(pos, b.radius * 2.6f)) return;
    const float tEnter = static_cast<float>((in.music - s.enterTick) / kTicksPerBar);  // bars since the entrance began
    const double barStart = std::floor(in.music / kTicksPerBar) * kTicksPerBar;
    const bool downbeat = in.music - barStart < kTicksPerBeat && in.music >= s.fightTick && !s.dying;
    const float dying = s.dying ? clamp01(static_cast<float>((in.music - s.deathTick) / (kTicksPerBar * 0.25))) : 0.0f;
    if (dying >= 1.0f) return;
    const float life = 1.0f - dying;
    const float hw = 0.6f * refPx_;

    // One arm's shape at angle `ang`, scaled `k` (reflections), with its colours.
    const auto arm = [&](u32 pair, float ang, float k, float level, bool hullOnly, bool resting, float enter) {
        const Vec3 hue = palette_->hue[pair % 3], partner = palette_->partner[pair % 3];
        const Vec2 dv = dirv(ang), nv{-dv.y, dv.x};
        for (u32 f = 0; f < 5; ++f) {
            const BossData::Facet& fc = f < 4 ? b.facets[f] : b.tip;
            // The entrance: inner to outer, each facet streaks in along its axis and locks.
            const float lockAt = 0.15f * static_cast<float>(f);
            const float u = clamp01((enter - lockAt) / 0.35f);
            if (enter < 1.0f && u <= 0.0f) continue;
            const float out = (1.0f - u) * (1.0f - u) * 900.0f;
            const float along = (fc.armRadius + out) * k;
            const float g = clamp01((fc.armRadius - 78.0f) / (274.0f - 78.0f));
            const Vec3 base = mix(mix(hue, W1, 0.5f), partner, g);
            for (u32 sg = 0; sg < fc.segs; ++sg) {
                const u32 layer = static_cast<u32>(fc.seg[sg][4]);
                if (hullOnly && layer != 0) continue;
                const Vec2 p0 = pos + dv * (along + fc.seg[sg][0] * k) + nv * (fc.seg[sg][1] * k);
                const Vec2 p1 = pos + dv * (along + fc.seg[sg][2] * k) + nv * (fc.seg[sg][3] * k);
                Vec3 col = f == 4 || layer == 2 ? acc : base;
                float in0 = level * (layer == 0 ? 2.0f : 1.4f);
                if (resting && layer == 0) {
                    col = W1;
                    in0 = level * 2.4f;
                }
                d.scene.line(toScene(p0), toScene(p1), hw * (layer == 0 ? 1.3f : 1.0f), col * in0, col * in0, 0);
            }
            // Glass: the facet's kite filled at a low level (white-hot: 0.16).
            if (!hullOnly && f < 4 && fc.segs >= 4) {
                const Vec2 cpos = pos + dv * along;
                const Vec2 q0 = cpos - dv * (fc.length * 0.5f * k), q1 = cpos + nv * (fc.halfWidth * k), q2 = cpos + dv * (fc.length * 0.5f * k),
                           q3 = cpos - nv * (fc.halfWidth * k);
                const Vec3 gl = (resting ? W1 * 0.16f : hue * 0.085f) * level;
                d.glass.tri(toScene(q0), gl, toScene(q1), gl, toScene(q2), gl);
                d.glass.tri(toScene(q0), gl, toScene(q2), gl, toScene(q3), gl);
            }
        }
        if (resting && !hullOnly) {
            // White-hot: a two-arm flare along the arm at the tip.
            const Vec2 tip = pos + dv * (b.tip.armRadius * k);
            flare(d.scene, tip, ang, 46.0f, 2, W1 * 2.2f, 0.8f);
            d.sparkles.glow(toScene(tip), 12.0f * refPx_, W1 * 0.9f);
        }
    };

    // Reflections behind the wheel: arm hulls only, larger and turned (the kaleidoscope).
    for (u32 r = 0; r < 2; ++r) {
        for (u32 k = 0; k < 6; ++k) {
            if (!(s.alive & (1u << (k % 3)))) continue;
            arm(k % 3, wheel + static_cast<float>(k) * kArmStep + b.reflectTurnDeg[r] * kTau / 360.0f, b.reflectScale[r],
                b.reflectIntensity[r] * 0.5f * life, true, false, tEnter);  // hull lines at the data's intensity
        }
    }
    // The arms, the resting pair white-hot.
    for (u32 k = 0; k < 6; ++k) {
        const u32 pr = k % 3;
        if (!(s.alive & (1u << pr))) continue;
        const bool rest = w.bossResting(pr, in.music);
        arm(pr, wheel + static_cast<float>(k) * kArmStep, 1.0f, life, false, rest, tEnter);
        // Beams: telegraphed for the bar before as a thin dashed line, then one beat along the arm to the wall.
        const u8 beam = w.bossBeamState(pr, in.music);
        if (beam) {
            const Vec2 dv = dirv(wheel + static_cast<float>(k) * kArmStep);
            const Vec2 from = pos + dv * b.tip.armRadius;
            float reach = 12000.0f;
            if (dv.x > 1e-4f) reach = std::fmin(reach, (kArenaHalfW - from.x) / dv.x);
            if (dv.x < -1e-4f) reach = std::fmin(reach, (-kArenaHalfW - from.x) / dv.x);
            if (dv.y > 1e-4f) reach = std::fmin(reach, (kArenaHalfH - from.y) / dv.y);
            if (dv.y < -1e-4f) reach = std::fmin(reach, (-kArenaHalfH - from.y) / dv.y);
            const Vec2 to = from + dv * reach;
            if (beam == 1) {
                for (float u = 0.0f; u < reach; u += 60.0f) {
                    const Vec2 p0 = from + dv * u, p1 = from + dv * std::fmin(u + 30.0f, reach);
                    if (visible(p0, 40.0f) || visible(p1, 40.0f)) telegraphLine(d, p0, p1, 1.0f);
                }
            } else {
                const float bw = 0.5f * b.phases[1].beamWidth * pxPerWu_;
                d.scene.line(toScene(from), toScene(to), bw, danger * 3.0f, danger * 2.0f, 0);
                d.scene.line(toScene(from), toScene(to), bw * 0.35f, W1 * 2.5f, W1 * 1.5f, 0);
                d.threatRims.line(toScreen(from), toScreen(to), bw + 2.0f * refPx_, Vec3{}, Vec3{}, 0);
                d.threatCores.line(toScreen(from), toScreen(to), bw, mix(danger, W1, 0.3f), mix(danger, W1, 0.3f), 0);
            }
        }
    }
    // Lens plates: two arcs per living pair on the ring around the core, centred on its arms.
    const float lensHalf = b.lensHalfAngleDeg * kTau / 360.0f;
    for (u32 k = 0; k < 6; ++k) {
        if (!(s.alive & (1u << (k % 3)))) continue;
        const float c = wheel + static_cast<float>(k) * kArmStep;
        const u32 n = 8;
        for (u32 j = 0; j < n; ++j) {
            const float a0 = c - lensHalf + 2.0f * lensHalf * static_cast<float>(j) / n, a1 = c - lensHalf + 2.0f * lensHalf * static_cast<float>(j + 1) / n;
            for (float r : {b.lensRadius, b.lensInner})
                d.scene.line(toScene(pos + dirv(a0) * r), toScene(pos + dirv(a1) * r), 0.5f * b.lensWidthPx * refPx_ * (r == b.lensRadius ? 1.0f : 0.5f),
                             acc * (1.8f * life), acc * (1.8f * life), 0);
        }
    }
    // The core: three counter-turning hexagons, the iris, the seed of light, the gold health arc.
    const float spin = static_cast<float>(in.music / kTicksPerBar) * kTau * 0.25f;
    for (u32 r = 0; r < 3; ++r) {
        const float rad = b.coreRingRadius[r];
        const u32 sides = b.coreRingSides[r] ? b.coreRingSides[r] : 6;
        const float ph = b.coreRingPhaseDeg[r] * kTau / 360.0f + (r % 2 ? -spin : spin);
        for (u32 j = 0; j < sides; ++j) {
            const float a0 = ph + kTau * static_cast<float>(j) / sides, a1 = ph + kTau * static_cast<float>(j + 1) / sides;
            d.scene.line(toScene(pos + dirv(a0) * rad), toScene(pos + dirv(a1) * rad), hw * 1.3f, acc * (2.2f * life), acc * (2.2f * life), 0);
        }
    }
    const float irisOpen = s.dying ? 1.0f : (downbeat ? 1.0f : 0.0f);
    const float irisR = b.irisClosed + (b.irisOpen - b.irisClosed) * irisOpen;
    for (u32 j = 0; j < b.irisShutters; ++j) {
        const float a0 = spin + kTau * static_cast<float>(j) / b.irisShutters;
        const Vec2 p0 = pos + dirv(a0) * irisR, p1 = pos + dirv(a0 + kTau / b.irisShutters) * irisR;
        d.scene.line(toScene(p0), toScene(p1), hw, acc * 2.0f, acc * 2.0f, 0);
        d.scene.line(toScene(p0), toScene(pos + dirv(a0 + 0.6f) * (irisR + 7.0f)), hw * 0.8f, acc * 1.4f, acc * 0.6f, 0);
    }
    d.sparkles.glow(toScene(pos), (6.0f + 10.0f * irisOpen) * pxPerWu_, W1 * (0.8f + 1.6f * irisOpen) * life);
    if (irisOpen > 0.0f) d.pools.glow(toScene(pos), 70.0f * pxPerWu_, acc * 0.25f);
    if (!s.dying && s.coreMax > 0.0f) {
        const float frac = clamp01(s.coreHp / s.coreMax);
        const u32 n = 48;
        for (u32 j = 0; j < n; ++j) {
            const float u0 = static_cast<float>(j) / n, u1 = static_cast<float>(j + 1) / n;
            if (u0 >= frac) break;
            const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + std::fmin(u1, frac) * kTau;
            const Vec3 g = palettes_->player[3] * 1.8f;
            d.scene.line(toScene(pos + dirv(a0) * b.healthRingRadius), toScene(pos + dirv(a1) * b.healthRingRadius), hw * 0.8f, g, g, 0);
        }
    }
    d.pools.glow(toScene(pos), b.radius * 1.4f * pxPerWu_, acc * (0.06f * life));
}

}  // namespace rf
