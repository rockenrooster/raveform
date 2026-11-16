// Title v2, "First Light" (screens.md "Title"; built 2026-11-14 with the owner's answers: the ship under PLAY gone, the intro on every
// cold start but much faster, the cut kept when leaving the app, the font's own letter spacing, the run behind at a third as
// before, the captions kept, and no reflection). "Sound becomes light": the word stands over a line of light whose ends are the
// music and whose middle is a laser rule carrying the tagline; a fan of seven beams behind the word; motes rising off the rule;
// PLAY with the beat in it. Everything moves on the music's clock. All of it is HUD (drawn after the bloom), so every glow is a
// pool; the levels come from TitleLook (play_transitions.cpp: the intro, home, the drop, the quiet title).

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "audio/band.h"
#include "game/play.h"
#include "game/play_hud.h"
#include "game/screens.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kTwoPi = 6.28318530718f;
constexpr float kDeg = kTwoPi / 360.0f;
Vec2 polarT(float a, float r) { return Vec2{std::cos(a) * r, std::sin(a) * r}; }
float clampT(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
float fractT(float x) { return x - std::floor(x); }
float hashT(u32 n) {  // 0..1 from an integer (a mote keeps its place, phase and size)
    n = (n ^ 61u) ^ (n >> 16);
    n *= 9u;
    n ^= n >> 4;
    n *= 0x27d4eb2du;
    n ^= n >> 15;
    return static_cast<float>(n & 0xFFFFu) / 65535.0f;
}

// screens.md "Title": reference px at 2400x1080, scaling with the screen's height.
constexpr float kRuleY = 432.0f;                                   // the rule (and the fan's and the depth's centre)
constexpr float kWordCap = 240.0f, kWordBase = 378.0f;             // cap height; baseline (the caps' top at 138)
constexpr float kHorizon = 0.56f;                                  // the chrome's horizon in each letter, down from its top
constexpr float kTubeHalf = 7.0f, kCoreHalf = 2.0f;                // the tube 14 px, its white-hot core 4 px
constexpr float kHotTube = 0.25f, kHotCore = 0.75f, kHotLevel = 0.4f;  // the drop's heat: 17.5 px, 7 px, +40%
// The depth: the plan's six copies each 1.1% smaller about the rule's centre, drawn as twelve half steps (six steps of 10 px at
// the word's ends showed as separate tubes, a comb, without the mock's bloom to melt them together); six on the low tier.
constexpr u32 kDepth = 12, kDepthLow = 6;
constexpr float kDepthSpan = 0.066f, kDepthLevel = 0.17f;
constexpr float kHaloShare = 3.0f;                                 // the halo's discs: the tube x 3 x 2.2 (as built)
constexpr float kWordPoolR = 850.0f, kWordPool = 0.035f;           // one pool of the first hue behind the word
constexpr float kGlintR = 96.0f, kGlintM = 70.0f;                  // the glints' arms: the R's top corner, the M's right foot
constexpr float kSwell = 0.015f;                                   // the kick's swell about the baseline's centre
constexpr float kRuleHalf = 1.2f, kRuleLevel = 2.1f, kRuleGlowHalf = 5.0f, kRuleGlow = 0.16f;
constexpr float kRuleCoreHalf = 0.6f, kRuleCore = 1.6f, kRuleKick = 0.5f;
constexpr float kFlareHalf = 2.5f, kFlareLevel = 0.75f, kFlareReach = 1150.0f;
constexpr float kEndsAmp = 66.0f, kEndsRise = 300.0f, kEchoHalf = 0.65f, kEchoLevel = 0.42f, kBump = 0.6f, kBumpWin = 100.0f;
constexpr float kTagCap = 22.0f, kTagPad = 44.0f, kTagLevel = 2.1f;
constexpr const char* kTagline = "S O U N D   B E C O M E S   L I G H T";
constexpr u32 kFanBeams = 7;
constexpr float kFanDeg[kFanBeams] = {-69.0f, -46.0f, -23.0f, 0.0f, 23.0f, 46.0f, 69.0f};
constexpr float kFanStart = 40.0f, kFanLen = 1040.0f, kFanHalf = 1.0f, kFanLevel = 0.40f, kFanGlowHalf = 9.0f, kFanGlow = 0.028f;
constexpr float kFanRest = 0.62f, kFanSway = 3.0f;
constexpr u32 kMotes = 110, kMotesLow = 40;
constexpr float kMoteRise = 300.0f, kMoteSway = 30.0f;
constexpr float kPlayY = 800.0f, kPlayW = 640.0f, kPlayH = 150.0f, kPlayCap = 60.0f, kPlayLabelUp = 11.0f, kPlayLift = 14.0f;
constexpr float kPlayTube = 4.5f, kPlayCoreHalf = 1.6f, kPlayPoolR = 350.0f, kPlayDotsDown = 47.0f, kPlayDotGap = 32.0f, kPlayDotR = 5.0f;
constexpr float kPlayRing = 40.0f, kPlayTapRing = 64.0f;
constexpr float kNotchW = 92.0f, kNotchUp = 34.0f, kNotchDown = 12.0f;
constexpr float kStrikeHotS = 0.06f, kStrikeDarkS = 0.04f, kStrikeLevel = 3.5f, kStrikeTube = 1.2f;

struct Seg {
    Vec2 a, b;
    float ya, yb;  // down the letter (0 top .. 1 foot) at each end
    u32 flags;     // flat ends where the stroke runs straight on into the next (look.md 4 joints: no beads)
};
}  // namespace

void Play::drawTitle(Ui& ui, const TitleLook& tl) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 c1 = palettes_->player[0], h0 = palette_->hue[0], h1 = palette_->hue[1], h2 = palette_->hue[2], acc = palette_->accent;
    // The field: P1 cyan -> white -> the palette's first enemy hue -> its third, in equal thirds.
    const auto field = [&](float u) {
        u = clampT(u);
        if (u < 1.0f / 3.0f) return c1 + (white - c1) * (u * 3.0f);
        if (u < 2.0f / 3.0f) return white + (h0 - white) * (u * 3.0f - 1.0f);
        return h0 + (h2 - h0) * (u * 3.0f - 2.0f);
    };
    const float fade = hudFade();
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    const double now = transNow();
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm), eighthS = 0.5f * beatS;
    const float beats = static_cast<float>(std::fmax(now, 0.0) / kTicksPerBeat);
    const float inBeat = fractT(beats);
    const u32 beatInBar = static_cast<u32>(std::fmod(beats, 4.0f)) % 4u;
    const float k = view_.kick();
    const float meter = band_ ? band_->meter() : 0.5f;
    // (M8: MODES's rule stands at its own height, and rises there from the title's in the lift: tl.ruleAt)
    const float ruleBase = (tl.ruleAt >= 0.0f ? tl.ruleAt : kRuleY) * s;
    const float Y = ruleBase + (0.5f * H - ruleBase) * tl.ruleY;

    // The word's strokes (font units: x 0..6 across an advance, y 0..6 up from the baseline), set at the font's own advance
    // (the owner, 2026-11-14: "closer letter spacing: font's own"), centred on its ink and fitted between the side margins.
    static constexpr const char* kWord = "RAVEFORM";
    Vec2 fa[8][48], fb[8][48];
    u32 fn[8];
    float minX[8], maxX[8];
    for (u32 j = 0; j < 8; ++j) {
        fn[j] = ui.font().glyphSegments(kWord[j], fa[j], fb[j], 48);
        minX[j] = 6.0f;
        maxX[j] = 0.0f;
        for (u32 q = 0; q < fn[j]; ++q) {
            minX[j] = std::fmin(minX[j], std::fmin(fa[j][q].x, fb[j][q].x));
            maxX[j] = std::fmax(maxX[j], std::fmax(fa[j][q].x, fb[j][q].x));
        }
    }
    const float margin = std::fmax(132.0f * s, std::fmax(ui.insetMin().x, ui.insetMax().x));
    const float inkUnits = 7.0f * VectorFont::kAdvanceUnits + maxX[7] - minX[0];
    const float cap = std::fmin(kWordCap * s, (W - 2.0f * margin) * VectorFont::kCapUnits / inkUnits);
    const float unit = cap / VectorFont::kCapUnits;
    const float inkW = inkUnits * unit, inkX0 = cx - 0.5f * inkW;
    const float x0 = inkX0 - minX[0] * unit;
    const float base = kWordBase * s, top = base - cap;
    const float swell = 1.0f + kSwell * k;
    const auto place = [&](u32 j, Vec2 f) {
        Vec2 p{x0 + (VectorFont::kAdvanceUnits * static_cast<float>(j) + f.x) * unit, base - f.y * unit};
        p = Vec2{cx, base} + (p - Vec2{cx, base}) * swell;
        p.y = Y + (p.y - Y) * (1.0f - tl.fall);  // (the drop: the word falls into its line)
        return p;
    };
    float letterX[8];
    for (u32 j = 0; j < 8; ++j) letterX[j] = place(j, Vec2{0.5f * (minX[j] + maxX[j]), 0.0f}).x;
    // Each letter's strike (screens.md "Title"): white-hot for 60 ms (tube x 1.2 at 3.5), dark for 40 ms, then lit; reduce flashing
    // fades it on over an eighth instead.
    float letterLevel[8];
    bool letterHot[8];
    for (u32 j = 0; j < 8; ++j) {
        const float ageS = (tl.struck - tl.strikeStep * static_cast<float>(j)) * beatS;
        letterHot[j] = false;
        if (ageS < 0.0f) letterLevel[j] = 0.0f;
        else if (calm) letterLevel[j] = clampT(ageS / eighthS);
        else if (ageS < kStrikeHotS) {
            letterLevel[j] = 1.0f;
            letterHot[j] = true;
        } else letterLevel[j] = ageS < kStrikeHotS + kStrikeDarkS ? 0.0f : 1.0f;
    }

    // ---- The fan: seven laser beams from the rule's centre behind the word, each a thin line fading along its length with a wide
    // faint copy, coloured by its place across the field; one pair lights on each beat (the centre on beat 1); it sways +-3 degrees
    // over four bars; it opens from flat (and folds flat in the drop).
    if (tl.fanLevel > 0.0f && tl.rule > 0.0f) {
        const float sway = kFanSway * kDeg * std::sin(kTwoPi * beats / 16.0f);
        for (u32 i = 0; i < kFanBeams; ++i) {
            const float deg = kFanDeg[i], sgn = deg < 0.0f ? -1.0f : (deg > 0.0f ? 1.0f : 0.0f);
            const float ang = sgn * (90.0f - (90.0f - std::fabs(deg)) * tl.fan);  // (degrees from straight up)
            float lv = tl.fanLevel * tl.rule * fade;
            if (sgn == 0.0f) lv *= clampT(tl.fan);
            const u32 pair = static_cast<u32>(std::lround(std::fabs(deg) / 23.0f));
            const float lit = pair == beatInBar ? 1.0f : kFanRest;
            if (lv <= 0.001f) continue;
            const float a = ang * kDeg + sway;
            const Vec2 d{std::sin(a), -std::cos(a)};
            const Vec2 p0 = Vec2{cx, Y} + d * (kFanStart * s), p1 = Vec2{cx, Y} + d * ((kFanStart + kFanLen) * s);
            const Vec3 c = field((deg + 69.0f) / 138.0f);
            ui.draw().hud.line(p0, p1, kFanHalf * s, ui.col(c, kFanLevel * lit * lv), Vec3{}, 0);
            ui.draw().hud.line(p0, p1, kFanGlowHalf * s, ui.col(c, kFanGlow * lit * lv), Vec3{}, 0);
        }
    }

    // ---- The word. Its strokes, split at each letter's horizon, flat where they run straight on.
    static Seg segs[8][96];  // (the frame's own: drawn on the main thread only)
    u32 segN[8] = {};
    const float sheenBeats = static_cast<float>(std::fmod(std::fmax(now, 0.0), 2.0 * kTicksPerBar) / kTicksPerBeat);
    const float sheenAt = -0.15f + 1.3f * (sheenBeats / 1.5f);
    const auto sheen = [&](float u) {
        if (sheenBeats > 1.5f) return 0.0f;
        const float d = (u - sheenAt) / 0.09f;
        return std::exp(-d * d);
    };
    const auto across = [&](Vec2 p) { return clampT((p.x - inkX0) / inkW); };
    const float wordLv = tl.word * (1.0f - tl.fall);  // (the drop: the rule takes the word's light as it falls into it)
    if (wordLv > 0.0f) {
        for (u32 j = 0; j < 8; ++j) {
            u32 n = 0;
            Seg* sg = segs[j];
            for (u32 q = 0; q < fn[j]; ++q) {
                const float ya = 1.0f - fa[j][q].y / VectorFont::kCapUnits, yb = 1.0f - fb[j][q].y / VectorFont::kCapUnits;
                if ((ya - kHorizon) * (yb - kHorizon) < 0.0f && n + 2 <= 96) {
                    const float t = (kHorizon - ya) / (yb - ya);
                    const Vec2 m = fa[j][q] + (fb[j][q] - fa[j][q]) * t;
                    sg[n++] = Seg{place(j, fa[j][q]), place(j, m), ya, kHorizon, 0u};
                    sg[n++] = Seg{place(j, m), place(j, fb[j][q]), kHorizon, yb, 0u};
                } else if (n < 96) {
                    sg[n++] = Seg{place(j, fa[j][q]), place(j, fb[j][q]), ya, yb, 0u};
                }
            }
            const float near = 0.5f * s;
            for (u32 q = 0; q < n; ++q)
                for (u32 r = 0; r < n; ++r) {
                    if (q == r) continue;
                    const Vec2 dq = normalizeOr(sg[q].b - sg[q].a, Vec2{1.0f, 0.0f}), dr = normalizeOr(sg[r].b - sg[r].a, Vec2{1.0f, 0.0f});
                    if (lengthSq(sg[q].b - sg[r].a) <= near * near && dot(dq, dr) >= 0.94f) {
                        sg[q].flags |= kFlatEnd;
                        sg[r].flags |= kFlatStart;
                    }
                    if (lengthSq(sg[q].b - sg[r].b) <= near * near && dot(dq, dr) <= -0.94f) {
                        sg[q].flags |= kFlatEnd;
                        sg[r].flags |= kFlatEnd;
                    }
                    if (lengthSq(sg[q].a - sg[r].a) <= near * near && dot(dq, dr) <= -0.94f) {
                        sg[q].flags |= kFlatStart;
                        sg[r].flags |= kFlatStart;
                    }
                }
            segN[j] = n;
        }
        const float tube = kTubeHalf * s * (1.0f + kHotTube * tl.heat), core = kCoreHalf * s * (1.0f + kHotCore * tl.heat);
        const float heatLv = 1.0f + kHotLevel * tl.heat;
        CapsuleBatch& hud = ui.draw().hud;
        // Its depth: copies of the tubes, each 1.1% smaller about the rule's centre, so the word recedes toward it from both sides.
        const u32 depth = lowTier_ ? kDepthLow : kDepth;
        const Vec3 deepC = mix(h1, h2, 0.4f);
        const Vec2 dc{cx, Y};
        for (u32 d = depth; d >= 1; --d) {
            const float sc = 1.0f - kDepthSpan * static_cast<float>(d) / static_cast<float>(depth);
            for (u32 j = 0; j < 8; ++j) {
                if (letterLevel[j] <= 0.0f || letterHot[j]) continue;
                const Vec3 c = ui.col(deepC, kDepthLevel * (1.0f - static_cast<float>(d) / (depth + 1.0f)) * wordLv * letterLevel[j]);
                for (u32 q = 0; q < segN[j]; ++q)
                    hud.line(dc + (segs[j][q].a - dc) * sc, dc + (segs[j][q].b - dc) * sc, tube * sc, c, c, segs[j][q].flags);
            }
        }
        // Its halo: soft pooled light along every stroke (the sheen adds to it only while it crosses), and one wide pool behind it.
        const float haloR = tube * kHaloShare * 2.2f;
        for (u32 j = 0; j < 8; ++j) {
            if (letterLevel[j] <= 0.0f) continue;
            const float lv = wordLv * letterLevel[j] * (letterHot[j] ? 2.0f : 1.0f) * fade;
            for (u32 q = 0; q < segN[j]; ++q) {
                const Vec2 a = segs[j][q].a, b = segs[j][q].b;
                const u32 discs = 1u + static_cast<u32>(length(b - a) / (0.6f * haloR));
                for (u32 t = 0; t <= discs; ++t) {
                    const Vec2 p = a + (b - a) * (static_cast<float>(t) / static_cast<float>(discs));
                    const float u = across(p), sh = sheen(u);
                    ui.draw().pools.glow(p, haloR * (1.0f + 0.6f * sh), mix(field(u), white, 0.5f * sh) * ((0.022f + 0.11f * sh) * lv * heatLv));
                }
            }
        }
        ui.draw().pools.glow({cx, top + 0.5f * cap}, kWordPoolR * s, h0 * (kWordPool * wordLv * fade));
        // The tubes: above each letter's horizon the field with white mixed in toward its top; below it the field and the third hue,
        // deepening to the foot with the second; a white-hot core. A struck letter is white-hot for its moment.
        const auto chrome = [&](float u, float y, float& lv) {
            if (y < kHorizon) {
                lv = 1.5f;
                return mix(field(u), white, 0.38f * (1.0f - y / kHorizon));
            }
            const float f = (y - kHorizon) / (1.0f - kHorizon);
            lv = 1.6f - 0.7f * f;
            return mix(mix(field(u), h2, 0.55f), h1, 0.30f * f);
        };
        for (u32 j = 0; j < 8; ++j) {
            if (letterLevel[j] <= 0.0f) continue;
            const float lv = wordLv * letterLevel[j] * heatLv;
            for (u32 q = 0; q < segN[j]; ++q) {
                const Seg& g = segs[j][q];
                if (letterHot[j]) {
                    const Vec3 c = ui.col(white, kStrikeLevel * wordLv);
                    hud.line(g.a, g.b, tube * kStrikeTube, c, c, g.flags);
                    continue;
                }
                const float ua = across(g.a), ub = across(g.b), sa = sheen(ua), sb = sheen(ub);
                float la = 0.0f, lb = 0.0f;
                const Vec3 ca = chrome(ua, g.ya, la), cb = chrome(ub, g.yb, lb);
                hud.line(g.a, g.b, tube, ui.col(mix(ca, white, 0.6f * sa), (la + 1.2f * sa) * lv), ui.col(mix(cb, white, 0.6f * sb), (lb + 1.2f * sb) * lv),
                         g.flags);
                const float ka = 1.3f - 0.8f * g.ya + 2.0f * sa, kb = 1.3f - 0.8f * g.yb + 2.0f * sb;
                hud.line(g.a, g.b, core, ui.col(white, ka * lv), ui.col(white, kb * lv), g.flags);
            }
        }
        // The glints: four-point stars at the R's top corner (flaring on the kick) and the M's right foot (on the backbeat).
        if (letterLevel[0] > 0.0f || letterLevel[7] > 0.0f) {
            Vec2 rTop{6.0f, 0.0f}, mFoot{0.0f, 6.0f};
            float rBest = 1e9f, mBest = -1e9f;
            for (u32 q = 0; q < fn[0]; ++q)
                for (const Vec2 f : {fa[0][q], fb[0][q]})
                    if (f.x - f.y < rBest) {
                        rBest = f.x - f.y;
                        rTop = f;
                    }
            for (u32 q = 0; q < fn[7]; ++q)
                for (const Vec2 f : {fa[7][q], fb[7][q]})
                    if (f.y < 0.5f && f.x > mBest) {
                        mBest = f.x;
                        mFoot = f;
                    }
            const float backbeat = (beatInBar % 2u == 1u) ? std::exp(-6.0f * inBeat) : 0.0f;
            const auto glint = [&](Vec2 at, float arm, float amount, float lv) {
                if (lv <= 0.01f) return;
                const float a = calm ? 0.35f : amount;
                for (u32 q = 0; q < 4; ++q) {
                    const float len = (q % 2u ? 0.35f : 1.0f) * arm * s * (0.6f + 0.6f * a);
                    const Vec2 dv = polarT(kTwoPi * 0.125f * static_cast<float>(q), len);
                    const Vec3 c = ui.col(white, (0.6f + 1.2f * a) * lv);
                    ui.draw().hud.line(at - dv, at, 0.8f * s, Vec3{}, c, 0);
                    ui.draw().hud.line(at, at + dv, 0.8f * s, c, Vec3{}, 0);
                }
            };
            glint(place(0, rTop), kGlintR, 0.35f + 0.65f * k, wordLv * letterLevel[0]);
            glint(place(7, mFoot), kGlintM, 0.35f + 0.65f * backbeat, wordLv * letterLevel[7]);
        }
    }

    // ---- The rule: edge to edge, straight under the word (a white core and a flare), its ends the music (the master bus rising
    // from nothing to +-66 px over 300 px, with a mirrored echo); it opens a gap for the tagline; a struck letter makes it leap under
    // it; on the kick its straight part brightens and a bump runs out along its ends; in the drop it takes the word's light.
    const float tagW = ui.measure(kTagCap * s, kTagline);
    const float gapHalf = tl.gap * (0.5f * tagW + kTagPad * s);
    if (tl.rule > 0.0f && tl.ruleLen > 0.0f) {
        const u32 n = band_ && band_->active() ? band_->scope(scope_, kScopeN) : 0;
        const float straightHalf = 0.5f * inkW + 24.0f * s;
        const float reachL = cx * tl.ruleLen, reachR = (W - cx) * tl.ruleLen;
        const float bumpAt = inBeat < 0.5f ? straightHalf + (cx - straightHalf) * (inBeat / 0.5f) : -1e9f;
        const float ruleLv = tl.rule * (1.0f + 1.2f * tl.ruleHeat);
        const u32 pts = 240;
        Vec2 prev{}, prevE{};
        bool have = false, prevStraight = true;
        // (its pieces meet flat, so the line has no beads where they overlap; only its two ends and the gap's edges are round)
        const auto joins = [&](float xa, float xb) {
            u32 f = 0;
            if (xa > cx - reachL + 1.0f && std::fabs(xa - cx) > gapHalf + 1.0f) f |= kFlatStart;
            if (xb < cx + reachR - 1.0f && std::fabs(xb - cx) > gapHalf + 1.0f) f |= kFlatEnd;
            return f;
        };
        for (u32 q = 0; q <= pts; ++q) {
            const float u = static_cast<float>(q) / pts, x = W * u, dx = x - cx, adx = std::fabs(dx);
            if (dx < -reachL - W / pts || dx > reachR + W / pts) {
                have = false;
                continue;
            }
            const float xc = std::fmin(std::fmax(x, cx - reachL), cx + reachR);
            float off = 0.0f;
            const bool straight = adx <= straightHalf;
            if (!straight) {
                float v = 0.0f;
                if (n) {
                    const i32 c = static_cast<i32>(u * static_cast<float>(n - 1));
                    float sum = 0.0f;
                    for (i32 j = -2; j <= 2; ++j) {
                        const i32 qq = c + j < 0 ? 0 : (c + j >= static_cast<i32>(n) ? static_cast<i32>(n) - 1 : c + j);
                        sum += scope_[qq];
                    }
                    v = sum * 0.2f;
                } else {
                    v = 0.25f * std::sin(u * 40.0f + beats * 1.5f);
                }
                v = std::fmax(-1.0f, std::fmin(1.0f, v * 2.2f));
                const float rise = clampT((adx - straightHalf) / (kEndsRise * s));
                const float bd = (adx - bumpAt) / (kBumpWin * s);
                const float bump = 1.0f + kBump * std::exp(-bd * bd);
                off = -v * kEndsAmp * s * tl.endsAmp * tl.ends * rise * bump;
            } else {
                for (u32 j = 0; j < 8; ++j) {
                    const float ageS = (tl.struck - tl.strikeStep * static_cast<float>(j)) * beatS;
                    if (ageS < 0.0f || ageS >= eighthS || calm) continue;
                    const float sn = (x - letterX[j]) / (0.5f * kNotchW * s);
                    if (std::fabs(sn) > 1.6f) continue;
                    const float e = 1.0f - ageS / eighthS;
                    const float dn = (std::fabs(sn) - 0.95f) / 0.3f;
                    off += e * (-kNotchUp * s * std::exp(-(sn / 0.45f) * (sn / 0.45f)) + kNotchDown * s * std::exp(-dn * dn));
                }
            }
            const Vec2 p{xc, Y + off}, pe{xc, Y - 0.5f * off};
            if (have) {
                const float mx = 0.5f * (prev.x + p.x);
                if (std::fabs(mx - cx) >= gapHalf) {
                    const bool st = straight && prevStraight;
                    const Vec3 fc = mix(field(u), white, tl.ruleHeat);
                    const float lv = ruleLv * (st ? 1.0f + kRuleKick * k : 1.0f);
                    const Vec3 c = ui.col(fc, kRuleLevel * lv), g = ui.col(fc, kRuleGlow * lv);
                    const u32 f = joins(prev.x, p.x);
                    ui.draw().hud.line(prev, p, kRuleGlowHalf * s, g, g, f);
                    ui.draw().hud.line(prev, p, kRuleHalf * s, c, c, f);
                    if (st) {
                        const Vec3 w = ui.col(white, kRuleCore * lv);
                        ui.draw().hud.line(prev, p, kRuleCoreHalf * s, w, w, f);
                    } else if (tl.ends > 0.0f) {
                        const Vec3 e = ui.col(h1, kEchoLevel * tl.rule * clampT(tl.ends));
                        ui.draw().hud.line(prevE, pe, kEchoHalf * s, e, e, f);
                    }
                }
            }
            prev = p;
            prevE = pe;
            prevStraight = straight;
            have = true;
        }
        // The flare: two white arms from the tagline's gap, fading to nothing 1,150 px from the centre.
        const float flareTo = std::fmin(kFlareReach * s, std::fmin(reachL, reachR));
        const float from = std::fmax(gapHalf, 0.0f);
        if (flareTo > from) {
            const Vec3 fw = ui.col(white, kFlareLevel * tl.rule * (1.0f + tl.ruleHeat));
            for (float sg : {-1.0f, 1.0f})
                ui.draw().hud.line(Vec2{cx + sg * from, Y}, Vec2{cx + sg * flareTo, Y}, kFlareHalf * s, fw, Vec3{}, 0);
        }
    }
    // The tagline, set into the rule's gap.
    if (tl.tag > 0.0f && tl.rule > 0.0f) ui.text({cx, Y + 0.5f * kTagCap * s}, kTagCap * s, mix(acc, white, 0.45f), kTagLevel * tl.tag * tl.rule, kTagline);

    // ---- The motes: dots in the field's colour at their place, rising 300 px off the rule over four beats with a sway, fading as
    // they rise; louder music sheds more. In the drop's wait they fall back into the rule.
    if (tl.motes > 0.0f && tl.rule > 0.0f) {
        const u32 count = lowTier_ ? kMotesLow : kMotes;
        const float density = 0.35f + 0.65f * clampT(meter * 1.5f);
        for (u32 m = 0; m < count; ++m) {
            const float hx = hashT(m * 3u + 1u), hp = hashT(m * 7u + 2u), hs = hashT(m * 11u + 3u), hd = hashT(m * 13u + 5u);
            if (hd > density) continue;
            const float u = 0.04f + 0.92f * hx;
            const float ph = fractT(beats / 4.0f + hp);
            const float rise = ph * (1.0f - tl.moteFall);
            const Vec2 p{W * u + kMoteSway * s * std::sin(kTwoPi * (ph * 1.5f + hs)), Y - kMoteRise * s * rise};
            ui.dot(p, (1.8f + 1.4f * hs) * s, field(u), (1.0f - ph) * 1.4f * tl.motes * tl.rule);
        }
    }

    // ---- The point of light (the intro's first beat; the drop's bar line) and the intro's clamped flash.
    if (tl.point > 0.0f) {
        const Vec2 c{cx, Y};
        const float lv = tl.point * (0.6f + (calm ? 0.0f : 1.4f) * k);
        ui.dot(c, 5.0f * s, white, 2.2f * lv);
        for (u32 q = 0; q < 4; ++q) {
            const float len = (q % 2u ? 14.0f : 40.0f) * s;
            const Vec2 dv = polarT(kTwoPi * 0.125f * static_cast<float>(q), len);
            const Vec3 g = ui.col(white, 1.4f * lv);
            ui.draw().hud.line(c - dv, c, 0.7f * s, Vec3{}, g, 0);
            ui.draw().hud.line(c, c + dv, 0.7f * s, g, Vec3{}, 0);
        }
        ui.draw().pools.glow(c, 70.0f * s, white * (0.12f * lv * fade));
    }
    if (tl.flash > 0.0f && !calm) ui.draw().pools.glow({cx, Y}, 1000.0f * s, white * (0.3f * tl.flash * fade));
}

bool Play::titlePlay(Ui& ui, const TitleLook& tl) {
    // screens.md "Title": PLAY at (1200, 800), 640 x 150: glass in P1, an outline of 80% P1 and 20% white, a wide P1 pool, its label a
    // tube 11 px above its centre, and the four beat dots inside it (the beat's lit, beat 1 in the accent). Every beat a ring leaves
    // it (1.0 on beat 1). Hover and pressed states as the table gives them. In the drop's wait it stays pressed, a ring leaves it
    // at the tap, its dots count the beats left, and in the last half beat it shrinks into the point.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f}, p1 = palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], acc = palette_->accent;
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    const float g = tl.playGather, keep = 1.0f - g;
    const Vec2 home{cx, (kPlayY + kPlayLift * tl.playLift) * s};
    const Vec2 c = home + (Vec2{cx, 0.5f * H} - home) * g;
    const Vec2 half = Vec2{0.5f * kPlayW * s, 0.5f * kPlayH * s} * keep;
    const Vec2 a = c - half, b = c + half;
    bool pressed = false, over = false;
    const bool tapped = g <= 0.0f && ui.button(10, a, b, "PLAY", pressed, over);
    pressed = pressed || tl.pressed;
    if (half.y < 0.5f * s) return tapped;
    const float r = half.y;
    const float fill = pressed ? 0.45f : (over ? 0.18f : 0.08f);
    const float outline = pressed ? 2.6f : (over ? 2.3f : 1.8f);
    const float labelTube = pressed ? 1.5f : 1.1f, labelCore = pressed ? 2.4f : (over ? 2.2f : 1.9f);
    const Vec3 gc = ui.col(p1, fill);
    ui.draw().hud.line({a.x + r, c.y}, {b.x - r, c.y}, r, gc, gc, 0);
    ui.draw().pools.glow(c, kPlayPoolR * s * keep, p1 * ((pressed ? 0.10f : 0.035f) * hudFade()));
    const Vec3 oc = mix(p1, white, 0.2f);
    ui.roundRect(a, b, r, (pressed ? 2.0f : 1.5f) * s, oc, outline);
    const double now = transNow();
    const float beats = static_cast<float>(std::fmax(now, 0.0) / kTicksPerBeat), inBeat = fractT(beats);
    const u32 beatInBar = static_cast<u32>(std::fmod(beats, 4.0f)) % 4u;
    if (!tl.pressed) {
        // A ring on every beat: a copy of the outline growing 40 px over the beat.
        const float grow = kPlayRing * s * inBeat;
        const float lv = (beatInBar == 0 ? 1.0f : 0.7f) * (1.0f - inBeat) * (calm ? 0.5f : 1.0f);
        ui.roundRect(a - Vec2{grow, grow}, b + Vec2{grow, grow}, r + grow, 1.0f * s, oc, lv);
    } else {
        // The tap answered at once: one ring, 64 px over half a beat.
        const float t = transBeats(transTap_) / 0.5f;
        if (t >= 0.0f && t < 1.0f) {
            const float grow = kPlayTapRing * s * (1.0f - (1.0f - t) * (1.0f - t));
            ui.roundRect(a - Vec2{grow, grow}, b + Vec2{grow, grow}, r + grow, 1.0f * s, oc, (1.0f - t) * (calm ? 0.6f : 1.2f));
        }
    }
    const float cap = kPlayCap * s * keep;
    if (cap > 2.0f * s) {
        const Vec2 at{c.x, c.y - kPlayLabelUp * s * keep + 0.5f * cap};
        ui.setQuiet(true);
        ui.text(at, cap, p1, labelTube, "PLAY", 0.5f, kPlayTube * s * keep);
        ui.text(at, cap, white, labelCore, "PLAY", 0.5f, kPlayCoreHalf * s * keep);
        ui.setQuiet(false);
    }
    const float left = static_cast<float>((transLine_ - now) / kTicksPerBeat);
    for (u32 q = 0; q < 4; ++q) {
        const Vec2 d{c.x + (static_cast<float>(q) - 1.5f) * kPlayDotGap * s * keep, c.y + kPlayDotsDown * s * keep};
        const bool lit = tl.pressed ? static_cast<float>(q) < left : q == beatInBar;
        ui.dot(d, kPlayDotR * s * keep, q == 0 && !tl.pressed ? acc : white, lit ? 1.9f : 0.32f);
    }
    return tapped;
}

}  // namespace rf
