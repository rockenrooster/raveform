// The view of M5 part 6: the bosses of Tracks 7 to 10 (design/bosses/*.md "Parts"). Cosmetic only.

#include <cmath>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
}  // namespace

// ---------------------------------------------------------------------------------------------- Cadence

void PlayView::drawCadence(FrameDraw& d, const ViewInput& in) {
    // cadence.md "Parts": four pillars (a ring of twelve petals, radius 90, the numeral inside, cap 60 wu), each its own hue;
    // sounding: petals spread to 120, the numeral white at 2.6, a pool of its hue; silent: closed, the numeral at 0.8. A thin
    // line in the grid's colour joins them; the side leading to the next chord brightens over the bar.
    const World& w = *in.world;
    const BossState& s = w.boss();
    const CadenceData& C = w.species().cadence;
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV();
    const Vec3 hues[4] = {palette_->hue[0], palette_->hue[1], palette_->hue[2], palette_->accent};
    static const char* const kNumerals[4] = {"I", "V", "vi", "IV"};
    const float entrance = clamp01(static_cast<float>((t - s.enterTick) / kTicksPerBar));
    const u32 chord = w.cadenceChord(t);
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    const float barU = static_cast<float>(inBar / kTicksPerBar);
    const bool inversion = s.cd.phase == 2;
    Vec2 p[4];
    for (u32 k = 0; k < 4; ++k) p[k] = s.bodyPrev[1 + k] + (s.body[1 + k] - s.bodyPrev[1 + k]) * in.alpha;
    if (!visible(s.pos, C.squareSide) && !s.dying) fightChevron(d, in, s.pos, hues[chord]);
    // The square, the side to the next chord brightening over the bar.
    for (u32 k = 0; k < 4; ++k) {
        const u32 n = (k + 1) % 4;
        const float lit = k == chord ? barU : 0.0f;
        const Vec3 col = palette_->grid * ((0.9f + 1.6f * lit) * entrance);
        d.scene.line(toScene(p[k]), toScene(p[n]), 0.6f * refPx_, col, col, 0);
    }
    for (u32 k = 0; k < 4; ++k) {
        // The death: the four burst in the song's order on the last bar's beats.
        float level = entrance;
        if (s.dying && s.payoutTick >= 0.0) {
            const double burst = s.payoutTick - kTicksPerBar + static_cast<double>(k) * kTicksPerBeat;
            if (t >= burst) continue;
        }
        const bool sounding = k == chord && !s.dying;
        const bool open = inversion ? !sounding : sounding;
        const float petal = sounding ? 120.0f : C.pillarRadius;
        const Vec3 hue = hues[k];
        for (u32 j = 0; j < 12; ++j) {
            const float a = kTau * static_cast<float>(j) / 12.0f + now * (sounding ? 0.6f : 0.15f);
            const Vec2 a0 = p[k] + dirv(a) * (petal * 0.62f), a1 = p[k] + dirv(a + 0.18f) * petal;
            d.scene.line(toScene(a0), toScene(a1), 0.9f * refPx_, hue * (1.8f * level), hue * (1.2f * level), 0);
        }
        if (open) {
            // The open one: a white ring (it can be hurt; x 2 on its One in the first phases).
            const float one = (!inversion && inBar < kTicksPerBeat) ? 1.0f - static_cast<float>(inBar / kTicksPerBeat) : 0.0f;
            const u32 n = 40;
            for (u32 j = 0; j < n; ++j) {
                const Vec2 q0 = p[k] + dirv(kTau * j / n) * (C.pillarRadius * 0.5f), q1 = p[k] + dirv(kTau * (j + 1) / n) * (C.pillarRadius * 0.5f);
                d.scene.line(toScene(q0), toScene(q1), 0.6f * refPx_, W1 * ((1.0f + 1.4f * one) * level), W1 * ((1.0f + 1.4f * one) * level), 0);
            }
        }
        if (sounding) d.pools.glow(toScene(p[k]), 2.2f * C.pillarRadius * pxPerWu_, hue * (0.12f * level));
        const char* text = kNumerals[k];
        const float cap = 60.0f * pxPerWu_ * 0.6f;
        const float wText = font_->measure(TextStyle{cap}, text);
        const Vec2 sc = toScene(p[k]);
        const Vec3 col = sounding ? W1 * (2.6f * level) : hue * (0.8f * level);
        font_->drawField(d.scene, Vec2{sc.x - 0.5f * wText, sc.y - 0.5f * cap}, cap, 1.0f * refPx_, col, col, col, text);
    }
}

// ---------------------------------------------------------------------------------------------- Arpeggiator

namespace {
Vec2 arpOut(u8 side) {
    // (as world_arpeggiator.cpp: above, right, below, left of the players' centre)
    switch (side & 3u) {
        case 0: return Vec2{0.0f, -1.0f};
        case 1: return Vec2{1.0f, 0.0f};
        case 2: return Vec2{0.0f, 1.0f};
        default: return Vec2{-1.0f, 0.0f};
    }
}
// The lane of machine m's note on step k of a bar (0: none): what the sim plays (world_arpeggiator.cpp).
u32 arpNote(const ArpState& a, u32 m, u32 k) {
    if (m == 0) return (a.phase == 1 ? (k % 2u) == 0 : (k % 4u) == 0) ? prose::kApLanesG[k] : 0u;
    for (u32 j = 0; j < 6; ++j)
        if (prose::kApStabSteps[j] == k) return 1u + (j % 4u);
    return 0u;
}
}  // namespace

void PlayView::drawArpeggiator(FrameDraw& d, const ViewInput& in) {
    // arpeggiator.md "Parts": the console (900 x 120) with its core (radius 70) behind a shutter at its middle, four ports on
    // its lower edge and the chart above it (4 x 16 gold dots, 400 x 60, the next bar's notes lit, a white cursor in time);
    // the lanes, five faint lines in the grid's colour with the lane's number at the far end; the blocks, danger capsules blockWidth
    // wide and blockDepth deep (36 by 12 since the owner's play of f0624e5) on the priority layer, with a glow. The second machine is white. Its death: the chart reads GG and every block in flight
    // turns gold and climbs back up its lane.
    const World& w = *in.world;
    const BossState& s = w.boss();
    const ArpState& a = s.ap;
    const ArpData& A = w.species().arp;
    const double t = in.music;
    const Vec3 W1 = whiteV(), gold = palettes_->player[3], danger = palettes_->danger;
    const float entrance = clamp01(static_cast<float>((t - s.enterTick) / kTicksPerBar));
    const double inBar = t - std::floor(t / kTicksPerBar) * kTicksPerBar;
    const float barU = static_cast<float>(inBar / kTicksPerBar);
    const bool loading = !s.dying && t >= s.fightTick && inBar < kTicksPerBeat;
    const float openU = loading ? std::sin(kPi * static_cast<float>(inBar / kTicksPerBeat)) : 0.0f;
    const float tps = w.ticksPerSecond();
    const float hw = 0.5f * prose::kApConsoleW, hd = 0.5f * prose::kApConsoleD;
    if (!visible(s.pos, hw) && !s.dying) fightChevron(d, in, s.pos, palette_->accent);
    const auto lanes = [&](Vec2 core, Vec2 out, float level, bool dashed) {
        // Five faint lines from the console's lower edge, 900 wu into the field.
        const Vec2 axis{-out.y, out.x}, dir = out * -1.0f;
        const Vec3 col = palette_->grid * (1.4f * level);
        for (i32 j = -2; j <= 2; ++j) {
            const Vec2 from = core + dir * hd + axis * (static_cast<float>(j) * A.laneWidth);
            if (!dashed) {
                d.scene.line(toScene(from), toScene(from + dir * A.laneLength), 0.7f * refPx_, col, col, 0);
                continue;
            }
            for (float u = 0.0f; u < A.laneLength; u += 80.0f)
                d.scene.line(toScene(from + dir * u), toScene(from + dir * (u + 40.0f)), 0.6f * refPx_, col * 1.6f, col * 1.6f, 0);
        }
        if (dashed) return;
        static const char* const kNums[4] = {"1", "2", "3", "4"};
        const float cap = 40.0f * pxPerWu_ * 0.6f;
        for (u32 lane = 1; lane <= 4; ++lane) {
            const Vec2 at = core + dir * (hd + A.laneLength + 30.0f) + axis * ((static_cast<float>(lane) - 2.5f) * A.laneWidth);
            if (!visible(at, 40.0f)) continue;
            const Vec2 sc = toScene(at);
            const float wt = font_->measure(TextStyle{cap}, kNums[lane - 1]);
            font_->drawField(d.scene, Vec2{sc.x - 0.5f * wt, sc.y - 0.5f * cap}, cap, 0.8f * refPx_, col * 1.4f, col * 1.4f, col * 1.4f, kNums[lane - 1]);
        }
    };
    for (u32 m = 0; m < (a.two ? 2u : 1u); ++m) {
        const Vec2 out = arpOut(a.side[m]);
        const Vec2 axis{-out.y, out.x};
        const Vec2 core = s.bodyPrev[m] + (s.body[m] - s.bodyPrev[m]) * in.alpha;
        const float level = m == 1 ? clamp01(static_cast<float>((t - s.phaseTick) / kTicksPerBeat)) : entrance;
        const Vec3 col = m == 0 ? palette_->accent : W1;
        lanes(core, out, level, false);
        // The console's outline and its four ports.
        const Vec2 q[4] = {core - axis * hw + out * hd, core + axis * hw + out * hd, core + axis * hw - out * hd, core - axis * hw - out * hd};
        for (u32 k = 0; k < 4; ++k) d.scene.line(toScene(q[k]), toScene(q[(k + 1) % 4]), 0.8f * refPx_, col * (1.4f * level), col * (1.4f * level), 0);
        for (u32 lane = 1; lane <= 4; ++lane) {
            const Vec2 port = core - out * hd + axis * ((static_cast<float>(lane) - 2.5f) * A.laneWidth);
            d.scene.line(toScene(port - axis * 24.0f), toScene(port + axis * 24.0f), 1.0f * refPx_, col * (2.0f * level), col * (2.0f * level), 0);
        }
        // The core behind its shutter: two blades across it that part as it loads the chart on the One (x 2 then).
        const u32 n = 32;
        const Vec3 ring = (loading ? W1 * (1.0f + 1.6f * openU) : col * 0.9f) * level;
        for (u32 k = 0; k < n; ++k) {
            const Vec2 r0 = core + dirv(kTau * k / n) * A.coreRadius, r1 = core + dirv(kTau * (k + 1) / n) * A.coreRadius;
            d.scene.line(toScene(r0), toScene(r1), 0.7f * refPx_, ring, ring, 0);
        }
        const float part = openU * 0.8f * A.coreRadius;
        for (i32 side = -1; side <= 1; side += 2) {
            const Vec2 c = core + axis * (static_cast<float>(side) * (0.25f * A.coreRadius + part));
            d.scene.line(toScene(c - out * (0.9f * A.coreRadius)), toScene(c + out * (0.9f * A.coreRadius)), 1.2f * refPx_, col * (1.2f * level),
                         col * (1.2f * level), 0);
        }
        if (loading) d.pools.glow(toScene(core), 2.4f * A.coreRadius * pxPerWu_, W1 * (0.10f * openU * level));
        // The chart above the console: 4 rows (lane 1 nearest the console) by 16 steps, 400 x 60.
        const Vec2 chart = core + out * (hd + 50.0f);
        if (s.dying) {
            const float cap = 60.0f * pxPerWu_ * 0.6f;
            const Vec2 sc = toScene(chart);
            const float wt = font_->measure(TextStyle{cap}, "GG");
            font_->drawField(d.scene, Vec2{sc.x - 0.5f * wt, sc.y - 0.5f * cap}, cap, 1.0f * refPx_, gold * 2.2f, gold * 2.2f, gold * 2.2f, "GG");
            continue;
        }
        for (u32 k = 0; k < 16; ++k) {
            const u32 note = arpNote(a, m, k);
            for (u32 r = 0; r < 4; ++r) {
                const Vec2 p = chart + axis * ((static_cast<float>(k) - 7.5f) * (400.0f / 15.0f)) + out * ((static_cast<float>(r) - 1.5f) * 20.0f);
                const bool lit = note == r + 1;
                d.sparkles.glow(toScene(p), (lit ? 4.2f : 2.0f) * refPx_, gold * ((lit ? 2.6f : 0.3f) * level));
            }
        }
        const Vec2 cur = chart + axis * ((barU * 16.0f - 8.0f) * (400.0f / 15.0f));
        d.scene.line(toScene(cur - out * 40.0f), toScene(cur + out * 40.0f), 0.6f * refPx_, W1 * (1.6f * level), W1 * (1.6f * level), 0);
    }
    // The next side, dashed for the bar before the move (where the sim puts it: from the players' centre).
    if (a.moveTick >= 0.0 && !s.dying) {
        const Vec2 out = arpOut(static_cast<u8>((a.side[0] + 1u) & 3u));
        const Vec2 axis{-out.y, out.x};
        const Vec2 core = out * (dot(a.centre, out) + prose::kApPlaceWu) + axis * dot(a.centre, axis);
        lanes(core, out, 1.0f, true);
    }
    // The blocks.
    for (const ArpState::Block& b : a.blocks) {
        if (b.tick < 0.0 || t < b.tick) continue;
        float dist = static_cast<float>(t - b.tick) / tps * A.blockSpeed;
        const bool back = s.dying && a.deathTick >= 0.0 && t > a.deathTick;
        if (back) {
            const float at = static_cast<float>(a.deathTick - b.tick) / tps * A.blockSpeed;
            if (at > A.laneLength) continue;
            dist = at - static_cast<float>(t - a.deathTick) / tps * A.blockSpeed;
        }
        if (dist < 0.0f || dist > A.laneLength) continue;
        const Vec2 c = b.from + b.dir * dist;
        if (!visible(c, 0.5f * A.blockWidth + 20.0f)) continue;
        const Vec2 axis{-b.dir.y, b.dir.x};
        const float r = 0.5f * A.blockDepth;
        const Vec2 e0 = c - axis * (0.5f * A.blockWidth - r), e1 = c + axis * (0.5f * A.blockWidth - r);
        if (back) {
            d.scene.line(toScene(e0), toScene(e1), r * pxPerWu_, gold * 1.6f, gold * 1.6f, 0);
            continue;
        }
        // arpeggiator.md (the owner's play of f0624e5: "much smaller with the glow making up the rest of the size"): the lethal
        // core drawn as a lethal beam is, its capsule exactly what kills with a white-pink line down it; round it a danger glow
        // (sigma 9 wu, 0.5 on the axis, reduce flashing 0.35) out to about the old block's 72 by 28, which hurts nobody.
        const Vec3 pink = mix(danger, W1, 0.7f);
        if (shown(kMuteBullet)) {
            d.scene.line(toScene(e0), toScene(e1), refPx_, pink * 1.6f, pink * 1.6f, 0);
            constexpr float kSigma = 9.0f;
            const float radius = kSigma * look_->glowRadiusOverSigma;
            const u32 n = static_cast<u32>(std::ceil(A.blockWidth / (2.0f * kSigma)));
            const float stepWu = A.blockWidth / static_cast<float>(n);
            const float peak = (reduceFlashing_ ? 0.35f : 0.5f) * stepWu / (kSigma * 2.5066283f);
            GlowBatch& soft = radius * pxPerWu_ < 16.0f * refPx_ ? d.sparkles : d.pools;  // (look.md §3, as drawBeam)
            const Vec2 g0 = c - axis * (0.5f * A.blockWidth);
            for (u32 k = 0; k <= n; ++k) soft.glow(toScene(g0 + axis * (stepWu * static_cast<float>(k))), radius * pxPerWu_, danger * peak);
        }
        if (shown(kMutePriority)) {
            d.threatRims.line(toScreen(e0), toScreen(e1), r * pxPerWu_ + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
            d.threatCores.line(toScreen(e0), toScreen(e1), r * pxPerWu_, danger, danger, 0);
            d.threatCores.line(toScreen(e0), toScreen(e1), refPx_, pink, pink, 0);
        }
    }
}

// ---------------------------------------------------------------------------------------------- Fata Morgana

void PlayView::drawMorgana(FrameDraw& d, const ViewInput& in) {
    // morgana.md "Parts": the storm, fifteen streaks of sand in the danger colour at 0.8 (sixty at 0.5 until the owner's red-line
    // pass) streaming across the frame, each drawn as a lethal line is, on the priority
    // layer outside the calm (never a fill); the oasis, a ring in the palette's accent at 2.2 with a second 12 wu inside it that
    // swells on the beat; Morgana, three veils (broken arcs) round a white core, turning against each other. The mirage is drawn
    // as the oasis and goes out on the snares; in the Crossing the next oasis is dashed for the bar before the leap, then the old
    // one shrinks and the corridor's edges join the two. Her death: the storm drops at once.
    const World& w = *in.world;
    const BossState& s = w.boss();
    const MorganaState& g = s.mg;
    const MorganaData& M = w.species().morgana;
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV(), accent = palette_->accent;
    const float entrance = clamp01(static_cast<float>((t - s.enterTick) / kTicksPerBar));
    const float level = s.dying ? clamp01(1.0f - static_cast<float>((t - s.deathTick) / kTicksPerBar)) : entrance;
    const float R = M.oasisRadius;
    const auto ring = [&](Vec2 c, float r, Vec3 col, float width, bool dashed) {
        if (r < 1.0f) return;
        const u32 n = 64;
        for (u32 k = 0; k < n; ++k) {
            if (dashed && (k % 2u)) continue;
            const Vec2 a = c + dirv(kTau * k / n) * r, b = c + dirv(kTau * (k + 1) / n) * r;
            d.scene.line(toScene(a), toScene(b), width * refPx_, col, col, 0);
        }
    };
    const auto oasis = [&](Vec2 c, float r, float lv) {
        if (!visible(c, r + 20.0f) || lv <= 0.0f) return;
        ring(c, r, accent * (prose::kMgEdgeLevel * lv), 0.8f, false);
        ring(c, r - prose::kMgInnerRingWu + 6.0f * beat_, accent * ((0.7f + 1.2f * beat_) * lv), 0.5f, false);
    };
    // The storm (from the entrance; it drops at once with her death).
    if (!s.dying && t >= s.enterTick) {
        const float halfW = centre_.x / pxPerWu_, halfH = centre_.y / pxPerWu_;
        const float diag = std::sqrt(halfW * halfW + halfH * halfH);
        const float wind = 0.35f + 0.25f * std::sin(now * 0.13f);
        const Vec2 wd = dirv(wind), wp{-wd.y, wd.x};
        const auto h01 = [](u32 i, u32 salt) { return static_cast<float>(hash32(i, salt) & 0xFFFFu) / 65535.0f; };
        // morgana.md (the owner's red-line pass, 2026-11-14: "thin red lines streaking across the screen ... too small, not visible
        // enough and are too frequent, tune down to like 0.25x"): fifteen streaks, each a lethal line's look (it hurts nobody: the
        // storm does), flaring on every kick, when the storm hurts.
        const float sandLevel = prose::kMgSandLevel * entrance * (1.0f + 0.6f * kick_);
        for (u32 i = 0; i < prose::kMgSandStreaks; ++i) {
            const float speed = 700.0f + 500.0f * h01(i, 0x5A1u), len = 70.0f + 110.0f * h01(i, 0x5A2u);
            const float along = std::fmod(h01(i, 0x5A3u) * 2.0f * diag + now * speed, 2.0f * diag) - diag;
            const float across = (h01(i, 0x5A4u) * 2.0f - 1.0f) * diag;
            const Vec2 head = cam_ + wd * along + wp * across;
            if (!visible(head, 20.0f) || w.morganaCalm(head, t)) continue;
            const Vec2 tail = head - wd * len;
            lethalBeam(d, tail, head, prose::kMgSandWu, sandLevel);
        }
    }
    // The oases: the real one, the mirage (out on the snares), the next one dashed, the old one shrinking and the corridor.
    const Vec2 real = w.morganaOasis(t);
    oasis(real, R, level);
    Vec2 mp;
    bool lit = false;
    if (!s.dying && w.morganaMirage(t, mp, lit) && lit) oasis(mp, R, level);
    if (!s.dying && g.nextShown && g.nextTick >= 0.0 && t < g.nextTick) ring(g.next, R, accent * (1.4f * level), 0.7f, true);
    if (!s.dying && g.leapTick >= 0.0 && t >= g.leapTick && t < g.leapTick + kTicksPerBar) {
        const float u = static_cast<float>((t - g.leapTick) / kTicksPerBar);
        oasis(g.from, R * (1.0f - u), level * (1.0f - 0.5f * u));
        const Vec2 dd = normalizeOr(g.at - g.from, Vec2{1.0f, 0.0f});
        const Vec2 side{-dd.y, dd.x};
        const float hw = 0.5f * prose::kMgCorridorWu;
        for (float sgn : {-1.0f, 1.0f}) {
            const Vec3 col = accent * (1.4f * level * (1.0f - 0.6f * u));
            d.scene.line(toScene(g.from + side * (hw * sgn)), toScene(g.at + side * (hw * sgn)), 0.6f * refPx_, col, col, 0);
        }
    }
    // Morgana: three veils round a white core; brighter while she can be hurt twice over.
    const Vec2 p = s.bodyPrev[0] + (s.body[0] - s.bodyPrev[0]) * in.alpha;
    if (!visible(p, M.bodyRadius) && !s.dying) fightChevron(d, in, p, accent);
    const bool open = !s.dying && w.morganaScale(t) > 1.0f;
    const Vec3 hues[3] = {palette_->hue[0], palette_->hue[1], palette_->hue[2]};
    for (u32 k = 0; k < 3; ++k) {
        const float r = M.bodyRadius * (1.0f - 0.24f * static_cast<float>(k));
        const float spin = (k % 2u ? -1.0f : 1.0f) * (0.9f + 0.4f * static_cast<float>(k)) * now;
        const Vec3 col = (open ? mix(hues[k], W1, 0.4f) * 2.6f : hues[k] * 1.6f) * level;
        for (u32 arc = 0; arc < 3; ++arc) {
            const float a0 = spin + kTau * static_cast<float>(arc) / 3.0f;
            const u32 n = 8;
            for (u32 j = 0; j < n; ++j) {
                const float b0 = a0 + 1.4f * static_cast<float>(j) / n, b1 = a0 + 1.4f * static_cast<float>(j + 1) / n;
                d.scene.line(toScene(p + dirv(b0) * r), toScene(p + dirv(b1) * r), 0.8f * refPx_, col, col, 0);
            }
        }
    }
    const u32 n = 20;
    for (u32 j = 0; j < n; ++j) {
        const Vec2 q0 = p + dirv(kTau * j / n) * 14.0f, q1 = p + dirv(kTau * (j + 1) / n) * 14.0f;
        d.scene.line(toScene(q0), toScene(q1), 0.7f * refPx_, W1 * (2.4f * level), W1 * (2.4f * level), 0);
    }
    d.sparkles.glow(toScene(p), 8.0f * refPx_, W1 * ((open ? 1.6f : 0.8f) * level));
}

// ---------------------------------------------------------------------------------------------- Kaleidos, Reprise

void PlayView::drawReprise(FrameDraw& d, const ViewInput& in) {
    // reprise.md "Parts": Kaleidos as built, 1.875 times as large (its facets drawn to arms 450 wu long round a core of 120, the
    // owner's 1.5x of 2026-11-14), each
    // arm in the accent of the Track whose boss it plays (Ultraviolet, Solar, Reef, Aurora, Furnace, Zenith); the soloist at 3.0
    // with its white core line and a pool of its colour, the others at 1.0; in the Reprise the arms white-edged (they cannot
    // be hurt yet). Longwave's arm let go is its wave (eight segments in Solar, the crests white); Feedback's Echo a ring in its
    // player's colour; Last Light the bare core. Death: the core dims to a point (the ripple: eventFight).
    const World& w = *in.world;
    const BossState& s = w.boss();
    const RepriseState& r = s.rp;
    const RepriseData& R = w.species().reprise;
    const BossData& b = w.bossData();
    const double t = in.music;
    const Vec3 W1 = whiteV();
    const auto accentOf = [&](u32 track) {
        for (u32 i = 0; i < palettes_->count; ++i)
            if (palettes_->list[i].track == track) return palettes_->list[i].accent;
        return palette_->accent;
    };
    const Vec3 acc = accentOf(1);
    const Vec2 pos = s.prevPos + (s.pos - s.prevPos) * in.alpha;
    const float wheel = s.prevAngle + wrapAngle(s.angle - s.prevAngle) * in.alpha;
    const float enter = static_cast<float>((t - s.enterTick) / kTicksPerBar);
    const float life = s.dying ? clamp01(1.0f - static_cast<float>((t - s.deathTick) / kTicksPerBar)) : 1.0f;
    const float hw = 0.6f * refPx_;
    const float k = R.armLength / std::fmax(b.tip.armRadius, 1.0f);
    const float kc = prose::kRpScale;  // (the core: Kaleidos's, a quarter larger)
    if (!visible(pos, R.armLength) && !s.dying) fightChevron(d, in, pos, acc);
    const double barStart = std::floor(t / kTicksPerBar) * kTicksPerBar;
    const bool downbeat = t - barStart < kTicksPerBeat && t >= s.fightTick && !s.dying && r.phase != 1;
    // reprise.md "The light" (the owner, 2026-11-14: "more epic visual wise"): twelve rays behind the wheel, two in each living
    // arm's colour through the gaps between the arms, 1,100 wu long, turning with it and flaring on every kick; in Last Light,
    // the bare core's, white and gold. A wide glow of the soloist's colour under the whole wheel in the Medley.
    if (!s.dying && enter > 0.0f) {
        constexpr float kRayWu = 1100.0f;
        const float flare = (1.0f + 1.2f * kick_) * (reduceFlashing_ ? 0.6f : 1.0f);
        const float grow = clamp01(enter);
        for (u32 j = 0; j < 6; ++j) {
            if (r.phase == 1 && !(s.alive & (1u << j))) continue;
            const Vec3 hue = r.phase == 2 ? (j % 2 ? palettes_->player[3] : W1) : accentOf(j + 1);
            for (i32 side = -1; side <= 1; side += 2) {
                const float ang = wheel + static_cast<float>(j) * kTau / 6.0f + kTau / 12.0f + static_cast<float>(side) * 0.16f;
                const Vec2 dv = dirv(ang);
                const Vec2 a0 = pos + dv * (R.coreRadius * 1.1f), a1 = pos + dv * (kRayWu * grow);
                d.scene.line(toScene(a0), toScene(a1), 2.2f * refPx_, hue * (0.6f * flare * life), hue * 0.0f, 0);
                for (u32 q = 1; q <= 4; ++q) {
                    const float u = static_cast<float>(q) / 5.0f;
                    d.pools.glow(toScene(pos + dv * (kRayWu * grow * u)), 70.0f * pxPerWu_, hue * (0.04f * flare * life * (1.0f - u)));
                }
            }
        }
        if (r.phase == 1 && r.solo < 6 && (s.alive & (1u << r.solo)))
            d.pools.glow(toScene(pos), 2.2f * R.armLength * pxPerWu_, accentOf(r.solo + 1u) * 0.07f);
    }
    // The arms.
    const bool wave = w.repriseWaveOut(t);
    for (u32 j = 0; j < 6 && !s.dying; ++j) {
        if (!(s.alive & (1u << j)) || (wave && j == 1)) continue;
        const bool solo = r.phase == 1 && r.solo == j;
        const float level = (r.phase == 1 ? (solo ? 3.0f : 1.0f) : 1.4f) * life;
        const Vec3 hue = accentOf(j + 1);
        const float ang = wheel + static_cast<float>(j) * kTau / 6.0f;
        const Vec2 dv = dirv(ang), nv{-dv.y, dv.x};
        for (u32 f = 0; f < 5; ++f) {
            const BossData::Facet& fc = f < 4 ? b.facets[f] : b.tip;
            const float u = clamp01((enter - 0.15f * static_cast<float>(f)) / 0.35f);
            if (enter < 1.0f && u <= 0.0f) continue;
            const float along = (fc.armRadius + (1.0f - u) * (1.0f - u) * 900.0f) * k;
            for (u32 sg = 0; sg < fc.segs; ++sg) {
                const u32 layer = static_cast<u32>(fc.seg[sg][4]);
                const Vec2 p0 = pos + dv * (along + fc.seg[sg][0] * k) + nv * (fc.seg[sg][1] * k);
                const Vec2 p1 = pos + dv * (along + fc.seg[sg][2] * k) + nv * (fc.seg[sg][3] * k);
                Vec3 col = hue;
                float lv = level * (layer == 0 ? 1.3f : 1.0f);
                if (layer == 0 && r.phase == 0) col = W1;               // white-edged: armoured
                if (layer == 2 && solo) col = W1;                         // the soloist's white core line
                d.scene.line(toScene(p0), toScene(p1), hw * (layer == 0 ? 1.3f : 1.0f), col * lv, col * lv, 0);
            }
            if (f < 4 && fc.segs >= 4) {
                const Vec2 c = pos + dv * along;
                const Vec2 q0 = c - dv * (fc.length * 0.5f * k), q1 = c + nv * (fc.halfWidth * k), q2 = c + dv * (fc.length * 0.5f * k), q3 = c - nv * (fc.halfWidth * k);
                const Vec3 gl = hue * (0.085f * level);
                d.glass.tri(toScene(q0), gl, toScene(q1), gl, toScene(q2), gl);
                d.glass.tri(toScene(q0), gl, toScene(q2), gl, toScene(q3), gl);
            }
        }
        if (solo) d.pools.glow(toScene(pos + dv * (0.6f * R.armLength)), 0.7f * R.armLength * pxPerWu_, hue * 0.12f);
    }
    // The core: Kaleidos's three counter-turning rings and its iris (open on the One in the Reprise and Last Light; shut in the
    // Medley), the health arc; dying, it dims to a point.
    const float spin = static_cast<float>(t / kTicksPerBar) * kTau * 0.25f;
    const float cs = kc * (0.15f + 0.85f * life);
    for (u32 q = 0; q < 3; ++q) {
        const float rad = b.coreRingRadius[q] * cs;
        const u32 sides = b.coreRingSides[q] ? b.coreRingSides[q] : 6;
        const float ph = b.coreRingPhaseDeg[q] * kTau / 360.0f + (q % 2 ? -spin : spin);
        for (u32 j = 0; j < sides; ++j) {
            const float a0 = ph + kTau * static_cast<float>(j) / sides, a1 = ph + kTau * static_cast<float>(j + 1) / sides;
            d.scene.line(toScene(pos + dirv(a0) * rad), toScene(pos + dirv(a1) * rad), hw * 1.3f, acc * (2.2f * life), acc * (2.2f * life), 0);
        }
    }
    const float irisR = (b.irisClosed + (b.irisOpen - b.irisClosed) * (downbeat ? 1.0f : 0.0f)) * cs;
    for (u32 j = 0; j < b.irisShutters; ++j) {
        const float a0 = spin + kTau * static_cast<float>(j) / b.irisShutters;
        d.scene.line(toScene(pos + dirv(a0) * irisR), toScene(pos + dirv(a0 + kTau / b.irisShutters) * irisR), hw, acc * (2.0f * life), acc * (2.0f * life), 0);
    }
    d.sparkles.glow(toScene(pos), (6.0f + (downbeat ? 12.0f : 0.0f)) * pxPerWu_, W1 * ((0.8f + (downbeat ? 1.6f : 0.0f)) * life));
    if (!s.dying && s.coreMax > 0.0f) {
        const float frac = clamp01(s.coreHp / s.coreMax);
        const u32 n = 48;
        for (u32 j = 0; j < n; ++j) {
            const float u0 = static_cast<float>(j) / n, u1 = static_cast<float>(j + 1) / n;
            if (u0 >= frac) break;
            const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + std::fmin(u1, frac) * kTau;
            const Vec3 g = palettes_->player[3] * 1.8f;
            d.scene.line(toScene(pos + dirv(a0) * (b.healthRingRadius * kc)), toScene(pos + dirv(a1) * (b.healthRingRadius * kc)), hw * 0.8f, g, g, 0);
        }
    }
    // Longwave's arm let go: its wave.
    if (wave) {
        const Vec3 solar = accentOf(2);
        const float sr = w.species().longwave.segRadius;
        Vec2 last{};
        for (u32 j = 0; j < prose::kRpWaveSegments; ++j) {
            const Vec2 p = s.bodyPrev[9 + j] + (s.body[9 + j] - s.bodyPrev[9 + j]) * in.alpha;
            const bool crest = w.repriseCrest(j, t);
            const Vec3 col = crest ? W1 * 2.6f : solar * 1.6f;
            const u32 n = 16;
            for (u32 q = 0; q < n; ++q)
                d.scene.line(toScene(p + dirv(kTau * q / n) * sr), toScene(p + dirv(kTau * (q + 1) / n) * sr), hw * 1.2f, col, col, 0);
            if (crest) d.sparkles.glow(toScene(p), 10.0f * refPx_, W1 * 1.2f);
            if (j) d.scene.line(toScene(last), toScene(p), hw, solar * 0.8f, solar * 0.8f, 0);
            last = p;
        }
    }
    // Feedback's Echo.
    if (r.echoOn && !s.dying) {
        const Vec2 p = s.bodyPrev[17] + (s.body[17] - s.bodyPrev[17]) * in.alpha;
        const Vec3 col = palettes_->player[r.echoPlayer % 4] * 1.6f;
        const float er = w.species().feedback.echoRadius;
        const u32 n = 16;
        for (u32 q = 0; q < n; ++q)
            d.scene.line(toScene(p + dirv(kTau * q / n) * er), toScene(p + dirv(kTau * (q + 1) / n) * er), hw, col, col, 0);
        const Vec2 dv = dirv(s.bodyAng[17]);
        d.scene.line(toScene(p), toScene(p + dv * (2.2f * er)), hw, col, col, 0);
    }
    d.pools.glow(toScene(pos), R.armLength * 1.4f * pxPerWu_, acc * (0.06f * life));
    // reprise.md "Death" (the owner, 2026-11-14: "make sure the reprise one is even more epic", then "I like the rings in every
    // Track's colour but it just races across the screen instead of the normal explosion with resistance like the first one"):
    // the whole set list comes home as explosion rings: from the death a ring in each of the ten Tracks' colours leaves the core
    // 0.12 s after the one before and spreads as every shock ring does, fast and then slowing (radius x 0.25 + 0.75 x the square
    // root of its age's share), to 1,100 wu for the first and 120 more for each after it, fading over 1.2 s; then a white one
    // to 3,200 wu over 1.8 s. Drawn here, past the six shock rings the view keeps at once; they raced at 2,400 wu/s until then.
    // The owner on that: "they should all come out 'at once' like a real exp, just a thick transparent layer of each color
    // sandwhiched together". So it is one shockwave: from the death, ten thick see-through bands side by side, one in each Track's
    // colour (First Light's at the front, Last Light's at the back), leave the core together and spread as an explosion does, fast
    // and then slowing (its front at 80 + 1,420 x (1 - (1 - u)^2.5) wu: 330 wu at 0.14 s, 800 at 0.45 s, 1,250 at 0.9 s, 1,500 at
    // the end), the whole growing from 160 to 360 wu thick, fading over 1.8 s.
    // The owner on those bands: "each color line should be very thin but have a very thick glow". So each Track's colour is a
    // thin line (1.5 ref px, the full colour), the ten a gap apart (16 wu growing to 36 as the wave spreads), and each line glows
    // far past its width: the glow falls off linearly over 1.6 gaps on either side, so it fills the gaps (a gap's middle holds
    // two thirds of each neighbour) and spills 3 gaps past the front and the back. The glow is drawn as glass strips between
    // the lines, each edge carrying the summed glow there; the white front line of the bands is gone (it hid First Light's).
    if (s.dying && s.deathTick >= 0.0) {
        const float since = static_cast<float>((t - s.deathTick) / std::fmax(w.ticksPerSecond(), 1.0f));
        constexpr float kWaveLife = 1.8f, kWaveWu = 1500.0f, kWaveStartWu = 80.0f, kGap0 = 16.0f, kGap1 = 36.0f;
        constexpr float kLineHalfPx = 0.75f, kLineLevel = 1.6f, kGlowLevel = 0.35f, kGlowReach = 1.6f, kGlowTail = 3.0f;
        constexpr float kSegmentWu = 100.0f;
        if (since >= 0.0f && since < kWaveLife) {
            const float rf = reduceFlashing_ ? 0.6f : 1.0f;
            const float u = since / kWaveLife;
            const float outer = kWaveStartWu + (kWaveWu - kWaveStartWu) * (1.0f - std::pow(1.0f - u, 2.5f));
            const float gap = kGap0 + (kGap1 - kGap0) * u;
            const float fade = (1.0f - u) * rf;
            // Knots from the front: the front tail's end (dark), the ten lines, the back tail's end (dark).
            constexpr float kNear = 1.0f - 1.0f / kGlowReach;  // a neighbour's share of the glow on a line
            Vec3 line[10], knot[12];
            float rad[12];
            for (u32 k = 0; k < 10; ++k) line[k] = accentOf(k + 1);
            rad[0] = outer + kGlowTail * gap;
            knot[0] = Vec3{};
            for (u32 k = 0; k < 10; ++k) {
                rad[k + 1] = outer - gap * static_cast<float>(k);
                Vec3 g = line[k];
                if (k > 0) g = g + line[k - 1] * kNear;
                if (k < 9) g = g + line[k + 1] * kNear;
                knot[k + 1] = g * (kGlowLevel * fade);
            }
            rad[11] = rad[10] - kGlowTail * gap;
            knot[11] = Vec3{};
            const u32 n = static_cast<u32>(clampf(std::ceil(kTau * rad[0] / kSegmentWu), 32.0f, 96.0f));
            for (u32 j = 0; j + 1 < 12; ++j) {
                const float ro = rad[j], ri = std::fmax(rad[j + 1], 0.0f);
                if (ro <= 0.0f) break;
                const Vec3 co = knot[j], ci = knot[j + 1];
                for (u32 q = 0; q < n; ++q) {
                    const float a0 = kTau * static_cast<float>(q) / n, a1 = kTau * static_cast<float>(q + 1) / n;
                    const Vec2 o0 = pos + dirv(a0) * ro, o1 = pos + dirv(a1) * ro;
                    if (!visible((o0 + o1) * 0.5f, 0.5f * length(o1 - o0) + (ro - ri))) continue;
                    const Vec2 i0 = pos + dirv(a0) * ri, i1 = pos + dirv(a1) * ri;
                    d.glass.tri(toScene(i0), ci, toScene(o0), co, toScene(o1), co);
                    d.glass.tri(toScene(i0), ci, toScene(o1), co, toScene(i1), ci);
                }
            }
            for (u32 k = 0; k < 10; ++k) {
                const float r = rad[k + 1];
                if (r <= 0.0f) break;
                const Vec3 c = line[k] * (kLineLevel * fade);
                for (u32 q = 0; q < n; ++q) {
                    const Vec2 p0 = pos + dirv(kTau * static_cast<float>(q) / n) * r, p1 = pos + dirv(kTau * static_cast<float>(q + 1) / n) * r;
                    if (!visible((p0 + p1) * 0.5f, 0.5f * length(p1 - p0) + 4.0f)) continue;
                    d.scene.line(toScene(p0), toScene(p1), kLineHalfPx * refPx_, c, c, 0);
                }
            }
        }
    }
}

}  // namespace rf
