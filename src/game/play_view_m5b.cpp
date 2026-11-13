// The view of M5 part 4: Event Horizon (design/bosses/horizon.md "Look"), Polyrhythm, Feedback and mini-bosses batch 2.
// Shapes and numbers from the boss pages' json through the species table; cosmetic only, nothing here touches the
// simulation.

#include <cmath>
#include <initializer_list>
#include <utility>

#include "game/play_view.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 lerpv(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
}  // namespace

// ---------------------------------------------------------------------------------------------- Event Horizon

void PlayView::horizonFlash(Vec2 at, float t) {
    // The death's payoff (horizon.md Look): the clamped white flash on the downbeat, three rings, 400 sparks in the Track's
    // hues, the 30 px distortion ring.
    addPool(at, 60.0f, 420.0f, whiteV(), reduceFlashing_ ? 0.45f : 0.9f, 0.0f, 0.12f, false, 1, false, false, t);
    addShockRings(at, 3, 1100.0f, palette_->hue[0], palette_->hue[1], t, 0.6f);
    for (u32 k = 0; k < 2; ++k) {
        SparkReq r{};
        r.at = at;
        r.count = 200;
        r.kind = 1;
        r.a = palette_->hue[k];
        r.b = palette_->hue[k + 1];
        r.t0 = t;
        sparkRequest(r);
    }
    addDistortion(at, 900.0f, 0.6f, 30.0f, 28.0f, t);
    shakeAmp_ = std::fmax(shakeAmp_, 6.0f);
    push(kImpBoss, at, 600.0f, 500.0f, t);
}

void PlayView::drawHorizon(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const HorizonData& H = w.species().horizon;
    const HorizonState& h = s.hz;
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV();
    const Vec3 danger = palettes_->danger;
    const Vec3 gold = palettes_->player[3];
    const Vec2 c = lerpv(s.prevPos, s.pos, in.alpha);
    const float tps = std::fmax(in.bpm, 60.0f) * static_cast<float>(kTicksPerBeat) / 60.0f;
    if (!visible(c, H.ringRadius[2] + 60.0f) && !s.dying) fightChevron(d, in, c, palette_->hue[0]);
    // The entrance: the grid pinches at its point over two beats; the disc opens black on beat 3 and the rings spin up.
    const float open = clamp01(static_cast<float>((t - (s.enterTick + 2.0 * kTicksPerBeat)) / kTicksPerBeat));
    // The death: everything spirals into the point over a bar, a beat of black, then the flash.
    float scale = 1.0f, spinExtra = 0.0f;
    if (s.dying) {
        const float u = clamp01(static_cast<float>((t - s.deathTick) / kTicksPerBar));
        scale = 1.0f - u * u;
        spinExtra = 3.0f * u * u;
        if (deathShown_ == 0 && t >= s.deathTick + kTicksPerBar + kTicksPerBeat) {
            deathShown_ = 1;
            horizonFlash(c, now);
        }
        if (scale <= 0.0f) return;
    }
    const bool fight = t >= s.fightTick && !s.dying;
    const bool starved = fight && !h.shield;
    const u8 ph = h.phase < 3 ? h.phase : 2;
    // The lattice bends into the Well: a standing pull at the horizon, renewed on every beat (look.md's grid answers it).
    {
        const double beat = std::floor(t / kTicksPerBeat);
        if (beat != hzBeat_ && t >= s.enterTick && !s.dying) {
            hzBeat_ = beat;
            push(kImpBoss, c, -prose::kHzLatticeImpulse * (t < s.fightTick ? 0.5f : 1.0f), H.horizonRadius * 1.2f, now);
        }
    }
    const float hw = 0.6f * refPx_;
    // The lanes: spiral arms 1,300 wu long winding 270 degrees in. Everything on a lane leaves a trail in its own hue, so
    // the lanes are arms of light; a lane cut goes dark from the cut inward.
    if (fight) {
        constexpr u32 kBins = 52;
        constexpr float kTrailWu = 380.0f;
        float lit[4][kBins] = {};
        const Enemies& e = w.enemies();
        const u32 lanes = H.phases[ph].lanes < 4 ? H.phases[ph].lanes : 4u;
        for (u32 i = 0; i < e.count; ++i) {
            if ((e.flags[i] & kFlagDead) || e.state[i] != kLane) continue;
            const u32 lane = static_cast<u32>(e.a[i]) % 4u;
            const float b = e.b[i];
            for (u32 k = 0; k < kBins; ++k) {
                const float r = prose::kHzLaneWu * (static_cast<float>(k) + 0.5f) / kBins;
                if (r < b || r > b + kTrailWu) continue;
                lit[lane][k] += 1.0f - (r - b) / kTrailWu;
            }
        }
        const float wind = prose::kHzLaneWindDeg * kTau / 360.0f;
        for (u32 l = 0; l < lanes; ++l) {
            const float lane0 = kTau * static_cast<float>(l) / static_cast<float>(lanes);
            const Vec3 hue = palette_->hue[l % 3];
            const auto at = [&](float r) { return c + dirv(lane0 + wind * (1.0f - r / prose::kHzLaneWu)) * r; };
            for (u32 k = 0; k < kBins; ++k) {
                const float r0 = prose::kHzLaneWu * static_cast<float>(k) / kBins, r1 = prose::kHzLaneWu * static_cast<float>(k + 1) / kBins;
                if (r1 < H.horizonRadius) continue;
                const Vec2 p0 = at(r0), p1 = at(r1);
                if (!visible(p0, 60.0f) && !visible(p1, 60.0f)) continue;
                const float level = 0.12f + 1.5f * std::fmin(lit[l][k], 1.0f);
                d.scene.line(toScene(p0), toScene(p1), hw * 1.4f, hue * level, hue * level, 0);
            }
        }
    }
    // The rings: dashes in the Track's hues, each at its own speed; the half turning toward the right of the screen is
    // 1.8 x brighter, so the disc looks tilted and alive. Starved: at a third.
    {
        const float bars = static_cast<float>((t - s.enterTick) / kTicksPerBar) - 0.75f;
        const float spin = bars > 0.0f ? bars : 0.5f * bars * bars;  // (spinning up from rest)
        for (u32 k = 0; k < 3; ++k) {
            const float r = H.ringRadius[k] * scale;
            const u32 n = H.ringDashes[k] ? H.ringDashes[k] : 1u;
            const float turn = (H.ringTurns[k] * (spin + spinExtra)) * kTau;
            const float level = (starved ? 1.0f / 3.0f : 1.0f) * open;
            for (u32 j = 0; j < n; ++j) {
                const float a0 = turn + kTau * static_cast<float>(j) / n, a1 = a0 + kTau / n * H.dashFraction;
                const float mid = 0.5f * (a0 + a1);
                const float bright = std::cos(mid) > 0.0f ? 1.8f : 1.0f;
                const Vec3 col = palette_->hue[k % 3] * (1.5f * level * bright);
                const float am = 0.5f * (a0 + a1);
                const Vec2 q0 = c + dirv(a0) * r, q1 = c + dirv(am) * r, q2 = c + dirv(a1) * r;
                d.scene.line(toScene(q0), toScene(q1), hw * 1.3f, col, col, 0);
                d.scene.line(toScene(q1), toScene(q2), hw * 1.3f, col, col, 0);
            }
        }
    }
    // The photon ring: white-hot, gold while it gasps.
    {
        const float r = H.photonRadius * scale;
        const u32 n = 56;
        const Vec3 col = (starved ? gold * 2.6f : W1 * 2.2f) * open;
        for (u32 j = 0; j < n; ++j) {
            const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
            d.scene.line(toScene(c + dirv(a0) * r), toScene(c + dirv(a1) * r), 0.5f * H.photonWidthPx * refPx_, col, col, 0);
        }
    }
    // True black: the disc, after tone mapping (no bloom, no afterglow, no grid inside it).
    const float R = H.horizonRadius * scale * open;
    if (R > 1.0f) d.threatRims.line(toScreen(c), toScreen(c), R * pxPerWu_, Vec3{}, Vec3{}, 0);
    if (!fight) return;
    // Starved: the rim gold and the core shown, a gold four-point star with a four-arm flare (gold is the weak point).
    if (starved) {
        const u32 n = 48;
        for (u32 j = 0; j < n; ++j) {
            const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
            d.threatCores.line(toScreen(c + dirv(a0) * R), toScreen(c + dirv(a1) * R), 1.0f * refPx_, gold, gold, 0);
        }
        const u32 pts = H.starPoints ? H.starPoints : 4u;
        const float spinStar = static_cast<float>(t / kTicksPerBar) * 0.25f * kTau;
        for (u32 j = 0; j < 2 * pts; ++j) {
            const float a0 = spinStar + kPi * static_cast<float>(j) / pts, a1 = spinStar + kPi * static_cast<float>(j + 1) / pts;
            const float r0 = j % 2 ? H.starInner : H.starOuter, r1 = j % 2 ? H.starOuter : H.starInner;
            d.threatCores.line(toScreen(c + dirv(a0) * r0), toScreen(c + dirv(a1) * r1), 1.4f * refPx_, gold, gold, 0);
        }
        for (u32 j = 0; j < 4; ++j) {
            const Vec2 dv = dirv(spinStar + kTau * static_cast<float>(j) / 4.0f + 0.25f * kPi);
            d.threatCores.line(toScreen(c + dv * (H.starInner * 0.5f)), toScreen(c + dv * (H.starOuter * 1.6f)), 0.7f * refPx_, gold, gold, 0);
        }
    }
    // The meter: twelve cells round the photon ring, the danger colour on the priority layer; the cell being filled flashes
    // white.
    {
        const u32 cells = H.meterCells ? H.meterCells : 12u;
        const u32 full = h.meal < cells ? h.meal : cells;
        const float slot = kTau / static_cast<float>(cells);
        const float wpx = 0.5f * H.meterWidthPx * refPx_;
        const bool flash = now - hzEatAt_ < 0.12f;
        for (u32 j = 0; j < cells; ++j) {
            const float a0 = -0.5f * kPi + slot * (static_cast<float>(j) + 0.5f * (1.0f - H.meterCellFraction));
            const float a1 = a0 + slot * H.meterCellFraction;
            const Vec2 q0 = c + dirv(a0) * H.meterRadius, q1 = c + dirv(0.5f * (a0 + a1)) * H.meterRadius, q2 = c + dirv(a1) * H.meterRadius;
            if (j >= full) {
                d.scene.line(toScene(q0), toScene(q1), wpx * 0.6f, danger * 0.25f, danger * 0.25f, 0);
                d.scene.line(toScene(q1), toScene(q2), wpx * 0.6f, danger * 0.25f, danger * 0.25f, 0);
                continue;
            }
            const Vec3 col = flash && j + 1 == full ? W1 : danger;
            for (const auto& seg : {std::pair<Vec2, Vec2>{q0, q1}, std::pair<Vec2, Vec2>{q1, q2}}) {
                d.threatRims.line(toScreen(seg.first), toScreen(seg.second), wpx + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                d.threatCores.line(toScreen(seg.first), toScreen(seg.second), wpx, col, col, 0);
            }
        }
    }
    // The tidal wave: a ring leaving the Well at 600 wu/s (no damage: not the danger colour).
    {
        const float r = prose::kHzWaveSpeed * static_cast<float>((t - h.waveTick) / tps);
        if (r > H.photonRadius && r < H.pullRange) {
            const float fade = 1.0f - r / H.pullRange;
            const u32 n = 72;
            const Vec3 col = mix(palette_->hue[1], W1, 0.4f) * (1.8f * fade);
            for (u32 j = 0; j < n; ++j) {
                const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
                const Vec2 q0 = c + dirv(a0) * r, q1 = c + dirv(a1) * r;
                if (visible(q0, 40.0f) || visible(q1, 40.0f)) d.scene.line(toScene(q0), toScene(q1), hw * 2.0f, col, col, 0);
            }
        }
    }
    // Collapse's jets: danger dashes along the coming axis for the bar before (it turns 45 degrees a bar), then two
    // opposite beams for two beats on a fed bar line.
    if (H.phases[ph].jets) {
        const float next = h.axis + prose::kHzAxisTurnDeg * kTau / 360.0f;
        for (float sgn : {1.0f, -1.0f}) {
            const Vec2 dv = dirv(next) * sgn;
            for (float u = H.photonRadius; u < tuning::kHzJetLengthWu; u += 60.0f) {
                const Vec2 q0 = c + dv * u, q1 = c + dv * std::fmin(u + 30.0f, tuning::kHzJetLengthWu);
                if (visible(q0, 40.0f) || visible(q1, 40.0f)) telegraphLine(d, q0, q1, 1.0f);
            }
        }
        if (t >= h.jetFrom && t < h.jetUntil) {
            const float bw = tuning::kHzJetHalfWu * pxPerWu_;
            for (float sgn : {1.0f, -1.0f}) {
                const Vec2 dv = dirv(h.axis) * sgn;
                const Vec2 p0 = c + dv * H.photonRadius, p1 = c + dv * tuning::kHzJetLengthWu;
                d.scene.line(toScene(p0), toScene(p1), bw, danger * 3.0f, danger * 2.0f, 0);
                d.scene.line(toScene(p0), toScene(p1), bw * 0.35f, W1 * 2.5f, W1 * 1.5f, 0);
                d.threatRims.line(toScreen(p0), toScreen(p1), bw + 2.0f * refPx_, Vec3{}, Vec3{}, 0);
                d.threatCores.line(toScreen(p0), toScreen(p1), bw, mix(danger, W1, 0.3f), mix(danger, W1, 0.3f), 0);
            }
        }
    }
}

}  // namespace rf
