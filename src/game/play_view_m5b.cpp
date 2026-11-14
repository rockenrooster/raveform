// The view of M5 part 4: Event Horizon (design/bosses/horizon.md "Look"), Polyrhythm, Feedback and mini-bosses batch 2.
// Shapes and numbers from the boss pages' json through the species table; cosmetic only, nothing here touches the
// simulation.

#include <cmath>
#include <initializer_list>
#include <utility>

#include "core/args.h"
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

void PlayView::fightBreak(Vec2 at, float t) {
    // A boss break played late (horizon.md, polyrhythm.md Look): the clamped white flash on the downbeat, three rings, 400
    // sparks in the Track's hues, the 30 px distortion ring.
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
            fightBreak(c, now);
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
            telegraphLine(d, c + dv * H.photonRadius, c + dv * tuning::kHzJetLengthWu, (std::floor(t / kTicksPerBar) + 1.0) * kTicksPerBar);
        }
        if (t >= h.jetFrom && t < h.jetUntil) {
            for (float sgn : {1.0f, -1.0f}) {
                const Vec2 dv = dirv(h.axis) * sgn;
                lethalBeam(d, c + dv * H.photonRadius, c + dv * tuning::kHzJetLengthWu, 2.0f * tuning::kHzJetHalfWu);
            }
        }
    }
}

// ---------------------------------------------------------------------------------------------- Polyrhythm

namespace {
// The OKLCH hue (degrees) of a linear sRGB colour.
float hueOf(Vec3 c) {
    const float l = std::cbrt(0.4122214708f * c.x + 0.5363325363f * c.y + 0.0514459929f * c.z);
    const float m = std::cbrt(0.2119034982f * c.x + 0.6806995451f * c.y + 0.1073969566f * c.z);
    const float s = std::cbrt(0.0883024619f * c.x + 0.2817188376f * c.y + 0.6299787005f * c.z);
    const float a = 1.9779984951f * l - 2.4285922050f * m + 0.4505937099f * s;
    const float b = 0.0259040371f * l + 0.7827717662f * m - 0.8086757660f * s;
    const float h = std::atan2(b, a) * 360.0f / kTau;
    return h < 0.0f ? h + 360.0f : h;
}
float hueDist(float a, float b) {
    float d = std::fabs(a - b);
    return d > 180.0f ? 360.0f - d : d;
}
}  // namespace

void PlayView::polyWalls(FrameDraw& d, const ViewInput& in) {
    // The walls: arcs in the danger colour on the priority layer, 3 px with the threat rim, thinning to 2 px in their last
    // half second; the gaps (and what a hit or a Nova erased) plainly empty.
    const World& w = *in.world;
    const BossState& s = w.boss();
    const PolyData& P = w.species().poly;
    const Vec3 danger = palettes_->danger;
    const double t = in.music;
    const float tps = std::fmax(in.bpm, 60.0f) * static_cast<float>(kTicksPerBeat) / 60.0f;
    for (const PolyRing& r : s.pr.rings) {
        if (!w.polyRingAlive(r, t)) continue;
        const float rad = w.polyRingRadius(r, t);
        const float age = static_cast<float>((t - r.tick) / tps);
        const float px = (P.wallLife - age < prose::kPrWallThinS ? 2.0f : 3.0f) * 0.5f * refPx_;
        const u32 n = r.sides ? r.sides : 3u;
        const float half = 0.5f * r.arcDeg * kTau / 360.0f;
        const u32 segs = static_cast<u32>(std::ceil(r.arcDeg / 6.0f));
        for (u32 j = 0; j < n; ++j) {
            const float corner = r.face0 + kPi / static_cast<float>(n) + kTau * static_cast<float>(j) / static_cast<float>(n);
            for (u32 q = 0; q < segs; ++q) {
                const float a0 = corner - half + 2.0f * half * static_cast<float>(q) / segs, a1 = corner - half + 2.0f * half * static_cast<float>(q + 1) / segs;
                if (!w.polyWallAt(r, 0.5f * (a0 + a1))) continue;  // erased there
                const Vec2 p0 = r.at + dirv(a0) * rad, p1 = r.at + dirv(a1) * rad;
                if (!visible(p0, 30.0f) && !visible(p1, 30.0f)) continue;
                if (shown(kMuteBullet)) d.scene.line(toScene(p0), toScene(p1), px * 1.4f, danger * 0.9f, danger * 0.9f, 0);
                if (!shown(kMutePriority)) continue;
                d.threatRims.line(toScreen(p0), toScreen(p1), px + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                d.threatCores.line(toScreen(p0), toScreen(p1), px, danger, danger, 0);
            }
        }
    }
}

void PlayView::drawPoly(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const PolyData& P = w.species().poly;
    const PolyState& pr = s.pr;
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV();
    const Vec3 gold = palettes_->player[3];
    const Vec3 danger = palettes_->danger;
    if (polyHueFor_ != palette_) {
        // Three: the palette's cyan (its colour nearest 200 degrees); Four: its amber (nearest 70).
        polyHueFor_ = palette_;
        const Vec3 cand[7] = {palette_->accent, palette_->hue[0], palette_->hue[1], palette_->hue[2], palette_->partner[0], palette_->partner[1], palette_->partner[2]};
        float best[2] = {1e9f, 1e9f};
        for (const Vec3& c : cand) {
            const float h = hueOf(c);
            if (hueDist(h, 200.0f) < best[0]) {
                best[0] = hueDist(h, 200.0f);
                polyHue_[0] = c;
            }
            if (hueDist(h, 70.0f) < best[1]) {
                best[1] = hueDist(h, 70.0f);
                polyHue_[1] = c;
            }
        }
    }
    const float a = in.alpha;
    const float hw = 0.6f * refPx_;
    const Vec2 rot[2] = {lerpv(s.bodyPrev[0], s.body[0], a), lerpv(s.bodyPrev[1], s.body[1], a)};
    if (!s.dying && !visible(lerpv(s.prevPos, s.pos, a), 0.5f * P.span + 200.0f)) fightChevron(d, in, lerpv(s.prevPos, s.pos, a), polyHue_[0]);
    polyWalls(d, in);
    const double bar = kTicksPerBar;
    const double inBar = t - std::floor(t / bar) * bar;
    // The rotors: a polygon with a smaller one turning the other way inside, a node on each corner (it glints in the danger
    // colour the twelfth before it fires), a pane of glass with a gold bar behind it on each face.
    for (u32 k = 0; k < 2; ++k) {
        const bool alive = (s.alive & (1u << k)) && !s.dying;
        const double land = s.enterTick + (k == 0 ? 0.0 : 2.0 * kTicksPerBeat);  // Three on beat 1, Four on beat 3
        float level = clamp01(static_cast<float>((t - land) / (0.5 * kTicksPerBeat)));
        float spin = 0.0f;
        if (!alive) {
            // The first to die spins down over a bar and bursts on the One; the second goes with the boss's break.
            const bool first = pr.firstDead == k;
            const double burst = first ? (std::floor(pr.firstDeathTick / bar) + 1.0) * bar : (s.payoutTick >= 0.0 ? s.payoutTick : s.deathTick + bar);
            if (first && t >= burst) {
                if (polyBurst_ != burst) {
                    polyBurst_ = burst;
                    addShockRings(rot[k], 2, 600.0f, polyHue_[k], W1, now, 0.5f);
                    SparkReq r{};
                    r.at = rot[k];
                    r.count = 160;
                    r.kind = 1;
                    r.a = polyHue_[k];
                    r.b = W1;
                    r.t0 = now;
                    sparkRequest(r);
                    push(kImpBoss, rot[k], 300.0f, 260.0f, now);
                }
                continue;
            }
            if (!first && !s.dying) continue;
            const double since = first ? t - pr.firstDeathTick : t - s.deathTick;
            const float u = clamp01(static_cast<float>(since / std::fmax(burst - (first ? pr.firstDeathTick : s.deathTick), 1.0)));
            level *= 1.0f - 0.6f * u;
            spin = 4.0f * u * u;
        }
        if (level <= 0.0f || !visible(rot[k], P.rotorRadius + 40.0f)) continue;
        const Vec2 c = rot[k];
        const u32 n = P.sides[k] ? P.sides[k] : 3u;
        const float face = w.polyFace(k, t) + spin;
        const float toOther = w.polyLinkAngle(t) + (k == 0 ? 0.0f : kPi);
        const Vec3 hue = polyHue_[k];
        const double pulse = bar / static_cast<double>(P.pulses[k] ? P.pulses[k] : 1u);
        const bool glint = alive && t >= s.fightTick - bar / 12.0 && pulse - std::fmod(inBar, pulse) < bar / 12.0;
        const auto corner = [&](float f0, float r, u32 j) {
            return c + dirv(f0 + kPi / static_cast<float>(n) + kTau * static_cast<float>(j) / static_cast<float>(n)) * r;
        };
        const float inner = 2.0f * toOther - face;  // the inner copy turns the other way
        for (u32 j = 0; j < n; ++j) {
            const Vec2 p0 = corner(face, P.rotorRadius, j), p1 = corner(face, P.rotorRadius, j + 1);
            d.scene.line(toScene(p0), toScene(p1), hw * 2.4f, hue * (2.4f * level), hue * (2.4f * level), 0);
            d.scene.line(toScene(p0), toScene(p1), hw * 0.9f, W1 * (1.3f * level), W1 * (1.3f * level), 0);
            const Vec2 q0 = corner(inner, P.rotorRadius * P.innerScale, j), q1 = corner(inner, P.rotorRadius * P.innerScale, j + 1);
            d.scene.line(toScene(q0), toScene(q1), hw * 1.6f, hue * (1.8f * level), hue * (1.8f * level), 0);
            // The corner's node.
            d.sparkles.glow(toScene(p0), P.cornerNodeRadius * pxPerWu_ * 1.4f, glint ? danger * 2.6f : W1 * (1.4f * level));
            // The face: a pane of glass, the gold bar behind it (inset), the weak point.
            const Vec2 mid = (p0 + p1) * 0.5f;
            const Vec2 inward = normalizeOr(c - mid, Vec2{1.0f, 0.0f});
            const Vec2 along = (p1 - p0) * (0.5f * P.faceBarFraction);
            const Vec2 bc = mid + inward * P.faceBarInset;
            if (alive) {
                const Vec3 gl = hue * (0.09f * level);
                d.glass.tri(toScene(p0), gl, toScene(p1), gl, toScene(bc), gl);
                d.scene.line(toScene(bc - along), toScene(bc + along), hw * 1.5f, gold * (2.2f * level), gold * (2.2f * level), 0);
            }
        }
        d.pools.glow(toScene(c), P.rotorRadius * 2.2f * pxPerWu_, hue * (0.08f * level));
    }
    // The link: a white line from Three to Four ruled into twelve (Three's ticks its colour, Four's its colour, tick 0 gold);
    // the playhead runs along it once a bar; the next tick to fire glows a beat ahead. It draws itself over the entrance's
    // second bar, tick by tick.
    if (s.alive == 3u || (s.dying && pr.firstDead == 0xFF)) {
        const Vec2 A = rot[0], B = rot[1];
        const Vec2 dv = normalizeOr(B - A, Vec2{1.0f, 0.0f}), nv = perpOf(dv);
        const float drawn = clamp01(static_cast<float>((t - (s.enterTick + bar)) / bar));
        const u32 ticksShown = static_cast<u32>(std::floor(drawn * static_cast<float>(P.linkTicks) + 0.999f));
        const u8 burn = w.polyLinkBurn(t);
        const Vec2 L1 = lerpv(A, B, drawn);
        if (drawn > 0.0f) {
            if (burn == 2) {
                lethalBeam(d, A, B, 2.0f * tuning::kPrLinkBurnHalfWu);
            } else {
                // On the One a white flare runs the link.
                const float one = inBar < bar / 12.0 && t >= s.fightTick ? 1.0f - static_cast<float>(inBar / (bar / 12.0)) : 0.0f;
                d.scene.line(toScene(A), toScene(L1), 0.5f * P.linkWidthPx * refPx_, W1 * (1.4f + 2.0f * one), W1 * (1.4f + 2.0f * one), 0);
                if (burn == 1) {
                    // (it burns for the bar's last burnBeats every second bar: when the coming burn begins)
                    const double barAt = std::floor(t / kTicksPerBar) * kTicksPerBar;
                    const bool odd = ((static_cast<i64>(std::floor(t / kTicksPerBar)) - s.pr.hemiolaBar0) & 1) != 0;
                    const u32 burns = w.species().poly.burnBeats < 4u ? w.species().poly.burnBeats : 4u;
                    telegraphLine(d, A, B, barAt + (odd ? 0.0 : kTicksPerBar) + (4.0 - static_cast<double>(burns)) * kTicksPerBeat);
                }
            }
        }
        for (u32 j = 0; j < P.linkTicks && j < ticksShown; ++j) {
            const Vec2 p = lerpv(A, B, static_cast<float>(j) / static_cast<float>(P.linkTicks));
            const bool three = j % (P.linkTicks / (P.pulses[0] ? P.pulses[0] : 3u)) == 0, four = j % (P.linkTicks / (P.pulses[1] ? P.pulses[1] : 4u)) == 0;
            if (!three && !four) {
                d.scene.line(toScene(p - nv * (0.25f * P.tickLength)), toScene(p + nv * (0.25f * P.tickLength)), hw * 0.7f, W1 * 0.6f, W1 * 0.6f, 0);
                continue;
            }
            Vec3 col = j == 0 ? gold : (three && four ? W1 : (three ? polyHue_[0] : polyHue_[1]));
            const double at = static_cast<double>(j) * bar / static_cast<double>(P.linkTicks);
            double ahead = at - inBar;
            if (ahead < 0.0) ahead += bar;
            const float glow = ahead < kTicksPerBeat ? 1.0f - static_cast<float>(ahead / kTicksPerBeat) : 0.0f;
            const float len = 0.5f * (j == 0 ? P.oneTickLength : P.tickLength);
            d.scene.line(toScene(p - nv * len), toScene(p + nv * len), hw * 1.2f, col * (1.6f + 1.6f * glow), col * (1.6f + 1.6f * glow), 0);
            if (glow > 0.0f) d.sparkles.glow(toScene(p), 10.0f * refPx_, col * (1.4f * glow));
        }
        if (drawn >= 1.0f) {
            // The playhead.
            const Vec2 ph = lerpv(A, B, static_cast<float>(inBar / bar));
            d.sparkles.glow(toScene(ph), 9.0f * refPx_, W1 * 2.0f);
            d.scene.line(toScene(ph - nv * 14.0f), toScene(ph + nv * 14.0f), hw, W1 * 2.2f, W1 * 2.2f, 0);
            // The One: two faint rails mark the aisle through both sets of rings, for a beat.
            if (inBar < kTicksPerBeat && t >= s.fightTick && !s.dying) {
                const float f = 1.0f - static_cast<float>(inBar / kTicksPerBeat);
                const float reach = P.rotorRadius + P.ringSpeed * P.wallLife;
                for (float sg : {1.0f, -1.0f}) {
                    const Vec2 o = nv * (sg * 0.5f * P.rotorRadius);
                    d.scene.line(toScene(A - dv * reach + o), toScene(B + dv * reach + o), hw * 0.6f, W1 * (0.35f * f), W1 * (0.35f * f), 0);
                }
            }
            // The death: the playhead's last bar fires a ring of sparks on every tick, then the boss break on the One.
            if (s.dying) {
                const double tw = std::floor(t / (bar / 12.0));
                if (tw != polySpark_) {
                    polySpark_ = tw;
                    SparkReq r{};
                    r.at = ph;
                    r.count = 24;
                    r.kind = 1;
                    r.a = polyHue_[static_cast<u32>(tw) % 2u];
                    r.b = W1;
                    r.t0 = now;
                    sparkRequest(r);
                }
            }
        }
    }
    if (s.dying && deathShown_ == 0 && s.payoutTick < 0.0 && t >= s.deathTick) {
        // (the break: when the Shards come, on the One)
        deathShown_ = 1;
        fightBreak((rot[0] + rot[1]) * 0.5f, now);
    }
}

// ---------------------------------------------------------------------------------------------- Feedback

void PlayView::drawFeedback(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    const FeedbackData& F = w.species().feedback;
    const FeedbackState& f = s.fb;
    const FeedbackTape& tape = w.tape();
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV();
    const Vec3 gold = palettes_->player[3];
    const Vec3 danger = palettes_->danger;
    if (fbHueFor_ != palette_) {
        // The Head: the palette's electric blue (its colour nearest 255 degrees).
        fbHueFor_ = palette_;
        const Vec3 cand[7] = {palette_->accent, palette_->hue[0], palette_->hue[1], palette_->hue[2], palette_->partner[0], palette_->partner[1], palette_->partner[2]};
        float best = 1e9f;
        for (const Vec3& c : cand) {
            const float dd = hueDist(hueOf(c), 255.0f);
            if (dd < best) {
                best = dd;
                fbHue_ = c;
            }
        }
    }
    const Vec2 c = s.pos;
    const float hw = 0.6f * refPx_;
    if (!s.dying && !visible(c, F.loopRadius + 40.0f)) fightChevron(d, in, c, fbHue_);
    // The entrance: the Loop draws itself once round over the second bar and the Head assembles inside it.
    const float drawn = clamp01(static_cast<float>((t - (s.enterTick + kTicksPerBar)) / kTicksPerBar));
    // The death: the tape is cut; the Loop opens at the head and unspools outward.
    const float cut = s.dying ? clamp01(static_cast<float>((t - s.deathTick) / kTicksPerBar)) : 0.0f;
    if (s.dying && deathShown_ == 0 && s.payoutTick < 0.0) {
        deathShown_ = 1;
        fightBreak(c, now);
    }
    const auto ang = [&](double stepFrac) { return -0.5f * kPi + kTau * static_cast<float>(stepFrac); };  // step 0 at twelve, clockwise
    const double bar = kTicksPerBar;
    const double headStep = std::floor(std::fmod(t, bar) / kTicksPerStep);
    const float headA = ang(headStep / static_cast<double>(F.loopSteps ? F.loopSteps : 16u));
    // ---- The Loop: two thin white lines a band apart with sixteen faint slots (Howl: the danger colour, priority layer).
    {
        const float R = f.loopR * (1.0f + 0.6f * cut);
        const bool hot = f.phase == 2 && !s.dying;
        const u32 segs = 96;
        const float level = (1.0f - cut) * 1.0f;
        for (u32 j = 0; j < segs; ++j) {
            const float u0 = static_cast<float>(j) / segs, u1 = static_cast<float>(j + 1) / segs;
            if (u0 >= drawn) break;
            const float a0 = headA + kTau * u0 + 0.6f * cut, a1 = headA + kTau * u1 + 0.6f * cut;  // (cut: it opens at the head)
            for (float side : {-1.0f, 1.0f}) {
                const float r = R + side * 0.5f * F.loopBand;
                const Vec2 p0 = c + dirv(a0) * r, p1 = c + dirv(a1) * r;
                if (!visible(p0, 30.0f) && !visible(p1, 30.0f)) continue;
                if (hot) {
                    d.scene.line(toScene(p0), toScene(p1), hw * 1.6f, danger * 1.6f, danger * 1.6f, 0);
                    d.threatRims.line(toScreen(p0), toScreen(p1), 0.5f * F.loopWidthPx * refPx_ * 2.0f + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                    d.threatCores.line(toScreen(p0), toScreen(p1), 0.5f * F.loopWidthPx * refPx_ * 2.0f, danger, danger, 0);
                } else if (level > 0.0f) {
                    d.scene.line(toScene(p0), toScene(p1), 0.5f * F.loopWidthPx * refPx_, W1 * (1.3f * level), W1 * (1.3f * level), 0);
                }
            }
        }
        for (u32 j = 0; j < F.loopSteps && drawn >= 1.0f && !s.dying; ++j) {
            const Vec2 dv = dirv(ang(static_cast<double>(j) / F.loopSteps));
            d.scene.line(toScene(c + dv * (R - 0.5f * F.loopBand)), toScene(c + dv * (R + 0.5f * F.loopBand)), hw * 0.6f, W1 * 0.35f, W1 * 0.35f, 0);
        }
        // The ticks: what the Echoes will play in the coming bar, each player's lane in their colour (the tape is always
        // the next bar of echo, drawn before it happens).
        if (!s.dying && t >= s.fightTick) {
            const double D = w.feedbackDelay(0);
            for (u32 p = 0; p < kMaxPlayers; ++p) {
                const FeedbackTape::Lane& l = tape.lanes[p];
                if (!l.shotCount || !w.player(p).present) continue;
                const float lane = R + F.loopBand * (-0.35f + 0.7f * (static_cast<float>(p) + 0.5f) / static_cast<float>(kMaxPlayers));
                double lastStep = -1.0;
                for (u32 k = 0; k < l.shotCount; ++k) {
                    const FeedbackTape::Shot& r = l.shots[(l.shotHead + FeedbackTape::kShots - 1 - k) % FeedbackTape::kShots];
                    const double when = r.tick + D;
                    if (when < t) break;
                    if (when > t + bar || !r.primary) continue;
                    const double step = std::floor(when / kTicksPerStep);
                    if (step == lastStep) continue;
                    lastStep = step;
                    const Vec2 dv = dirv(ang(std::fmod(step, static_cast<double>(F.loopSteps)) / F.loopSteps + 0.5 / F.loopSteps));
                    const Vec3 col = palettes_->player[p];
                    d.scene.line(toScene(c + dv * (lane - 0.5f * F.tickLength * 0.4f)), toScene(c + dv * (lane + 0.5f * F.tickLength * 0.4f)), hw * 1.6f,
                                 col * 2.4f, col * 2.4f, 0);
                }
            }
            // The gold head on the song's sixteenths, with a small flare.
            const Vec2 hp = c + dirv(headA) * R;
            d.sparkles.glow(toScene(hp), 9.0f * refPx_, gold * 2.2f);
            flare(d.scene, hp, headA + 0.5f * kPi, 26.0f, 2, gold * 2.0f, 0.5f);
        }
    }
    // ---- The Head: surround, ribbed cone (it pushes out 15% on every kick), coil, and a six-blade shutter over a gold core
    // (drawn back while open; across while closed).
    {
        const float level = clamp01(static_cast<float>((t - (s.enterTick + kTicksPerBar)) / kTicksPerBar)) * (s.dying ? 1.0f : 1.0f);
        const float push = 1.0f + prose::kFbConePush * kick_;
        const bool open = !s.dying && t >= s.fightTick && f.open >= 0.999f;
        const auto circle = [&](float r, Vec3 col, float width) {
            const u32 n = 40;
            for (u32 j = 0; j < n; ++j)
                d.scene.line(toScene(c + dirv(kTau * j / n) * r), toScene(c + dirv(kTau * (j + 1) / n) * r), width, col, col, 0);
        };
        circle(F.headRadius, fbHue_ * (2.2f * level), hw * 1.6f);
        circle(F.coneRadius * push, fbHue_ * (1.4f * level), hw);
        circle(F.coilRadius, fbHue_ * (1.2f * level), hw * 0.8f);
        for (u32 j = 0; j < F.ribs; ++j) {
            const Vec2 dv = dirv(kTau * j / F.ribs);
            d.scene.line(toScene(c + dv * F.coilRadius), toScene(c + dv * (F.coneRadius * push)), hw * 0.6f, fbHue_ * (0.9f * level), fbHue_ * (0.9f * level), 0);
        }
        d.sparkles.glow(toScene(c), F.coreRadius * pxPerWu_, open ? gold * 2.4f : fbHue_ * 0.25f);
        const u32 blades = F.blades ? F.blades : 6u;
        const float spin = static_cast<float>(t / bar) * 0.3f;
        for (u32 j = 0; j < blades; ++j) {
            const float a0 = spin + kTau * j / blades;
            if (open) {
                // Drawn back: short blades at the coil.
                d.scene.line(toScene(c + dirv(a0) * (F.coilRadius * 0.85f)), toScene(c + dirv(a0 + 0.5f) * F.coilRadius), hw * 1.2f, W1 * (1.2f * level),
                             W1 * (1.2f * level), 0);
            } else {
                // Across: each blade from the centre to the core's rim and round.
                d.scene.line(toScene(c), toScene(c + dirv(a0) * (F.coreRadius * 1.3f)), hw * 1.4f, fbHue_ * (2.0f * level), fbHue_ * (2.0f * level), 0);
                d.scene.line(toScene(c + dirv(a0) * (F.coreRadius * 1.3f)), toScene(c + dirv(a0 + kTau / blades) * (F.coreRadius * 1.3f)), hw,
                             fbHue_ * (1.6f * level), fbHue_ * (1.6f * level), 0);
            }
        }
        if (open) d.pools.glow(toScene(c), 2.0f * F.headRadius * pxPerWu_, gold * 0.12f);
    }
    if (s.dying || t < s.fightTick) return;
    // ---- The Echoes: the player's own ship in the danger colour with a white core and three after-images, on the priority
    // layer; a faint dashed line in the player's colour from the Echo through the Head to a small ring where the player
    // stood a bar ago.
    const float shipScale = 14.0f * glyphs_->def(static_cast<u32>(gShip_)).size;  // (play_view.cpp kStandardEnemyWu: look.md 4)
    const Look::PriorityShip& ph = look_->priorityHull;
    for (u32 e = 0; e < FeedbackState::kEchoes; ++e) {
        if (!(f.active & (1u << e))) continue;
        const u32 p = e / 2u, k = e % 2u;
        const Vec2 at = lerpv(s.bodyPrev[1 + e], s.body[1 + e], in.alpha);
        const float head = s.bodyAng[1 + e];
        const double D = w.feedbackDelay(k);
        // After-images: where it was half a beat apart.
        for (u32 j = 1; j <= F.afterImages; ++j) {
            Vec2 rel, aim;
            bool alive = false;
            if (!w.feedbackPose(p, t - D - static_cast<double>(j) * F.afterImageBeats * kTicksPerBeat, rel, aim, alive)) continue;
            const float dist = clampf(length(rel), prose::kFbMinWu, prose::kFbMaxWu);
            const Vec2 q = c + normalizeOr(rel, Vec2{1.0f, 0.0f}) * ((k == 0 ? -1.0f : 1.0f) * dist);
            const float lv = 0.5f / static_cast<float>(j);
            glyph(d.glyphs, gShip_, true, q, head, head, shipScale, lv, 0.5f * look_->lines.playerHull.width, kGlyphFlat, danger, danger);
        }
        if (visible(at, 40.0f)) {
            glyph(d.glyphs, gShip_, true, at, head, head, shipScale, 1.6f, 0.5f * look_->lines.playerHull.width, kGlyphFlat, danger, danger);
            if (shown(kMutePriority)) {
                glyph(d.threatGlyphRims, gShip_, true, at, head, head, shipScale, 1.0f, 0.5f * ph.width + ph.rim, kGlyphFlat, Vec3{}, Vec3{});
                glyph(d.threatGlyphCores, gShip_, true, at, head, head, shipScale, 1.0f, 0.5f * ph.width, kGlyphFlat, mix(danger, W1, 0.25f), mix(danger, W1, 0.25f));
            }
            d.sparkles.glow(toScene(at), 6.0f * refPx_, W1 * 1.6f);  // its white core
        }
        if (k == 0) {
            Vec2 rel, aim;
            bool alive = false;
            if (w.feedbackPose(p, t - D, rel, aim, alive)) {
                const Vec2 was = c + rel;
                const Vec3 col = palettes_->player[p] * 0.45f;
                const Vec2 dv = normalizeOr(was - at, Vec2{1.0f, 0.0f});
                const float len = length(was - at);
                for (float u = 0.0f; u < len; u += 40.0f) {
                    const Vec2 q0 = at + dv * u, q1 = at + dv * std::fmin(u + 20.0f, len);
                    if (visible(q0, 20.0f) || visible(q1, 20.0f)) d.scene.line(toScene(q0), toScene(q1), hw * 0.6f, col, col, 0);
                }
                const u32 n = 16;
                for (u32 j = 0; j < n; ++j)
                    d.scene.line(toScene(was + dirv(kTau * j / n) * 14.0f), toScene(was + dirv(kTau * (j + 1) / n) * 14.0f), hw * 0.7f, col * 2.0f, col * 2.0f, 0);
            }
        }
    }
    // ---- The Nulls: a danger ring the size of the Nova it echoes.
    for (const FeedbackTape::Null& n : tape.nulls) {
        const float r = w.feedbackNullRadius(n, t);
        if (r <= 0.0f) continue;
        const u32 segs = 72;
        for (u32 j = 0; j < segs; ++j) {
            const Vec2 p0 = n.at + dirv(kTau * j / segs) * r, p1 = n.at + dirv(kTau * (j + 1) / segs) * r;
            if (!visible(p0, 30.0f) && !visible(p1, 30.0f)) continue;
            d.scene.line(toScene(p0), toScene(p1), hw * 3.0f, danger * 2.0f, danger * 2.0f, 0);
            if (shown(kMutePriority)) {
                d.threatRims.line(toScreen(p0), toScreen(p1), 2.0f * refPx_ + look_->threat.telegraphRim * refPx_, Vec3{}, Vec3{}, 0);
                d.threatCores.line(toScreen(p0), toScreen(p1), 2.0f * refPx_, danger, danger, 0);
            }
        }
    }
}

}  // namespace rf
