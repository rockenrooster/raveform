#include "game/play_hud.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "content/icons.h"
#include "sim/sim_types.h"
#include "sim/tuning.h"

namespace rf {

using sim::lengthSq;

namespace {
// look.md §6 at 2400 x 1080 (reference pixels; design/tools/frame_mock.py draws the same HUD).
constexpr float kMargin = 132.0f;            // side margin, clear of the camera cut-out
constexpr float kStickRadius = 150.0f;       // the dashed rings (GAME_DESIGN 2: about 10 mm)
constexpr float kStickRadiusMm = 10.0f;
constexpr float kPadRadius = 44.0f;          // the thumb pad
constexpr float kNovaRadius = 74.0f, kDashRadius = 60.0f;  // frame_mock.py
constexpr float kButtonMinMm = 9.0f;         // GAME_DESIGN 2: buttons at least 9 mm (across)
constexpr float kPressedGrow = 1.1f;         // GAME_DESIGN 2: a pressed button is slightly larger
constexpr float kDeadZone = 0.08f;           // GAME_DESIGN 2
constexpr u32 kDashes = 20;
constexpr float kDashDuty = 0.55f;

Vec2 dir(float a) { return {std::cos(a), std::sin(a)}; }

void disc(CapsuleBatch& b, Vec2 c, float r, Vec3 col) { b.line(c, c, r, col, col, kDisc); }

void ring(CapsuleBatch& b, Vec2 c, float r, float halfWidth, Vec3 col, u32 segments = 48) {
    Vec2 prev = c + dir(0.0f) * r;
    for (u32 i = 1; i <= segments; ++i) {
        const Vec2 next = c + dir(kTau * static_cast<float>(i) / static_cast<float>(segments)) * r;
        b.line(prev, next, halfWidth, col, col, kFlatStart | kFlatEnd);
        prev = next;
    }
}

void arc(CapsuleBatch& b, Vec2 c, float r, float a0, float a1, float halfWidth, Vec3 col) {
    const u32 n = static_cast<u32>(std::ceil(std::fabs(a1 - a0) / (kTau / 48.0f))) + 1;
    Vec2 prev = c + dir(a0) * r;
    for (u32 i = 1; i <= n; ++i) {
        const Vec2 next = c + dir(a0 + (a1 - a0) * static_cast<float>(i) / static_cast<float>(n)) * r;
        b.line(prev, next, halfWidth, col, col, (i > 1 ? kFlatStart : 0u) | (i < n ? kFlatEnd : 0u));
        prev = next;
    }
}

// Interface rings are dashed (look.md §6 lesson 1).
void dashedRing(CapsuleBatch& b, Vec2 c, float r, float halfWidth, Vec3 col) {
    const float step = kTau / static_cast<float>(kDashes);
    for (u32 k = 0; k < kDashes; ++k) arc(b, c, r, step * static_cast<float>(k), step * (static_cast<float>(k) + kDashDuty), halfWidth, col);
}

void star4(CapsuleBatch& b, Vec2 c, float r, float halfWidth, Vec3 inner, Vec3 outer) {
    // A four-point star (Nova stock), points at 45 degrees, inner radius 0.4.
    for (u32 k = 0; k < 8; ++k) {
        const float a0 = kPi * 0.25f + kTau * static_cast<float>(k) / 8.0f;
        const float a1 = kPi * 0.25f + kTau * static_cast<float>(k + 1) / 8.0f;
        const float r0 = (k % 2 == 0) ? r : 0.4f * r, r1 = (k % 2 == 0) ? 0.4f * r : r;
        b.line(c + dir(a0) * r0, c + dir(a1) * r1, halfWidth, k % 2 == 0 ? outer : inner, k % 2 == 0 ? inner : outer, 0);
    }
}

void groupDigits(char* out, size_t cap, u64 v) {
    char raw[32];
    std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(v));
    const size_t n = std::strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < n && o + 2 < cap; ++i) {
        if (i > 0 && (n - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = '\0';
}
}  // namespace

Vec3 hudColour(const Look& look, const LookVariant& variant, Vec3 c, float intensity) {
    // look.md §3: hue-preserving extended Reinhard with a bleed toward white for very hot values.
    const Vec3 v = c * intensity;
    const float m = std::fmax(v.x, std::fmax(v.y, v.z));
    if (m <= 1e-6f) return Vec3{};
    const float wp2 = variant.whitePoint * variant.whitePoint;
    const float tm = std::fmin(1.0f, m * (1.0f + m / wp2) / (1.0f + m));
    Vec3 rgb = v * (tm / m);
    float t = clampf((m - look.bleedStart) / (look.bleedEnd - look.bleedStart), 0.0f, 1.0f);
    t = t * t * (3.0f - 2.0f * t) * variant.bleed;
    return mix(rgb, Vec3{tm, tm, tm}, t);
}

// ---------------------------------------------------------------------------------------------- touch

void TouchControls::layout(float w, float h, Vec2 insetMin, Vec2 insetMax, float pxPerMm) {
    w_ = w;
    h_ = h;
    s_ = h / 1080.0f;
    stickR_ = kStickRadius * s_;
    float novaR = kNovaRadius * s_, dashR = kDashRadius * s_;
    if (pxPerMm > 0.0f) {
        stickR_ = std::fmax(stickR_, kStickRadiusMm * pxPerMm);
        const float minR = 0.5f * kButtonMinMm * pxPerMm;
        dashR = std::fmax(dashR, minR);
        novaR = std::fmax(novaR, dashR * (kNovaRadius / kDashRadius));
    }
    // Physical sizes larger than the 1080 layout's push the whole arrangement out from the corners, so
    // the rings and buttons keep frame_mock.py's spacing and never overlap.
    const float k = std::fmax(1.0f, std::fmax(stickR_ / (kStickRadius * s_), std::fmax(novaR / (kNovaRadius * s_), dashR / (kDashRadius * s_))));
    const float u = s_ * k;
    padR_ = kPadRadius * s_ * (stickR_ / (kStickRadius * s_));
    const float m = kMargin * s_;
    const float left = std::fmax(m, insetMin.x), right = std::fmax(m, insetMax.x);
    const float bottom = insetMax.y;
    move_.rest = {left + 210.0f * u, h - bottom - 250.0f * u};
    aim_.rest = {w - right - 210.0f * u, h - bottom - 250.0f * u};
    nova_.centre = {w - right - 430.0f * u, h - bottom - 330.0f * u};
    nova_.radius = novaR;
    dash_.centre = {w - right - 300.0f * u, h - bottom - 520.0f * u};
    if (swap_) {
        // Settings, CONTROLS: swap sides (the aim stick and its buttons on the left).
        const auto mirror = [&](Vec2& p) { p.x = w - p.x; };
        mirror(move_.rest);
        mirror(aim_.rest);
        mirror(nova_.centre);
        mirror(dash_.centre);
    }
    dash_.radius = dashR;
}

void TouchControls::reset() {
    move_.active = aim_.active = false;
    nova_.pressed = dash_.pressed = false;
}

bool TouchControls::down(u64 id, Vec2 p) {
    used_ = true;
    const auto hit = [&](const Button& b) { return length(p - b.centre) <= b.radius * 1.25f; };
    if (hit(nova_) && !nova_.pressed) {
        nova_.pressed = true;
        nova_.id = id;
        return true;
    }
    if (hit(dash_) && !dash_.pressed) {
        dash_.pressed = true;
        dash_.id = id;
        return true;
    }
    Stick& s = (p.x < 0.5f * w_) != swap_ ? move_ : aim_;
    if (s.active) return false;
    s.active = true;
    s.id = id;
    s.origin = s.thumb = p;  // floating: the origin is where the thumb lands
    return true;
}

void TouchControls::move(u64 id, Vec2 p) {
    for (Stick* s : {&move_, &aim_}) {
        if (!s->active || s->id != id) continue;
        s->thumb = p;
        // Dragged past the radius, the origin follows the thumb.
        const Vec2 d = p - s->origin;
        const float l = length(d);
        if (l > stickR_) s->origin = p - d * (stickR_ / l);
    }
}

void TouchControls::up(u64 id) {
    if (move_.active && move_.id == id) move_.active = false;
    if (aim_.active && aim_.id == id) aim_.active = false;
    if (nova_.pressed && nova_.id == id) nova_.pressed = false;
    if (dash_.pressed && dash_.id == id) dash_.pressed = false;
}

Vec2 TouchControls::deflection(const Stick& s) const {
    if (!s.active) return Vec2{};
    const Vec2 d = (s.thumb - s.origin) * (1.0f / stickR_);
    const float l = length(d);
    if (l <= kDeadZone) return Vec2{};
    const float m = clampf((l - kDeadZone) / (1.0f - kDeadZone), 0.0f, 1.0f);
    return d * (m / l);
}

void TouchControls::apply(sim::StickInput& in) const {
    const Vec2 mv = deflection(move_);
    if (lengthSq(mv) > lengthSq(in.move)) in.move = mv;
    const Vec2 am = deflection(aim_);
    if (lengthSq(am) > 0.0f) {
        in.aim = am;
        in.aimActive = true;
    }
    in.nova = in.nova || nova_.pressed;
    in.dash = in.dash || dash_.pressed;
}

void TouchControls::draw(CapsuleBatch& hud, GlowBatch& pools, const VectorFont& font, const Look& look, const LookVariant& variant,
                         Vec3 player, float dashCharge, bool novaReady) const {
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const auto col = [&](Vec3 c, float i) { return hudColour(look, variant, c, i); };
    // Sticks: dashed rings at 0.34, a filled thumb pad that follows the thumb (look.md §6).
    for (const Stick* s : {&move_, &aim_}) {
        const bool aim = s == &aim_;
        const Vec2 c = s->active ? s->origin : s->rest;
        dashedRing(hud, c, stickR_, 1.0f * s_, col(white, s->active ? 0.6f : 0.34f));
        if (!s->active) continue;
        Vec2 pad = s->thumb;
        const Vec2 d = pad - s->origin;
        const float l = length(d);
        if (l > stickR_) pad = s->origin + d * (stickR_ / l);
        const Vec3 tint = aim ? player : white;
        disc(hud, pad, padR_, col(tint, aim ? 0.22f : 0.16f));
        ring(hud, pad, padR_, 1.0f * s_, col(tint, aim ? 0.9f : 0.7f), 40);
    }
    // Buttons: idle = outline at 0.5 with a label; pressed = filled disc in the player colour at 0.45,
    // a thicker outline at 2.0, the label at 1.8, slightly larger (look.md §6, GAME_DESIGN 2).
    const auto button = [&](const Button& b, const char* label, bool ready, float charge) {
        const float r = b.radius * (b.pressed ? kPressedGrow : 1.0f);
        if (b.pressed) {
            disc(hud, b.centre, r, col(player, 0.45f));
            ring(hud, b.centre, r, 1.5f * s_, col(player, 2.0f));
            pools.glow(b.centre, r * 1.6f, player * 0.35f);
        } else {
            ring(hud, b.centre, r, 1.1f * s_, col(white, ready ? 0.5f : 0.22f));
        }
        if (charge < 1.0f) {
            // The cooldown ring: the recharge so far, clockwise from the top.
            arc(hud, b.centre, r + 7.0f * s_, -0.5f * kPi, -0.5f * kPi + kTau * charge, 1.4f * s_, col(player, 1.2f));
        }
        const float cap = 12.0f * s_;
        const float width = font.measure(TextStyle{cap}, label);
        const Vec3 lc = col(white, b.pressed ? 1.8f : (ready ? 0.9f : 0.35f));
        font.drawField(hud, {b.centre.x - 0.5f * width + 1.0f * s_, b.centre.y - 0.5f * cap}, cap, (b.pressed ? 0.9f : 0.8f) * s_, lc, lc, lc,
                       label);
    };
    button(nova_, "NOVA", novaReady, 1.0f);
    button(dash_, "DASH", dashCharge >= 1.0f, dashCharge);
}

// ---------------------------------------------------------------------------------------------- HUD

void drawHud(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, const Palettes& palettes,
             const Palette& palette, const HudState& st, const TouchControls& touch, float W, float H, Vec2 insetMin,
             Vec2 insetMax) {
    const float s = H / 1080.0f;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 player = palettes.player[0];
    const Vec3 gold = palettes.player[3];       // P4 gold (the Chain's field: white -> gold -> amber)
    const Vec3 amber = palettes.playerDeep[3];  // P4 deep: amber
    const Vec3 accent = palette.accent;
    const auto col = [&](Vec3 c, float i) { return hudColour(look, variant, c, i); };
    const float m = std::fmax(kMargin * s, insetMin.x);
    const float mr = std::fmax(kMargin * s, insetMax.x);
    const float top = insetMin.y;

    // Score: top left, cap height 34, white running to the accent at the baseline, 1.6.
    char text[48];
    groupDigits(text, sizeof text, st.score);
    font.drawField(d.hud, {m, top + 44.0f * s}, 34.0f * s, 1.55f * s, col(white, 1.6f), col(white, 1.6f), col(accent, 1.6f), text);
    // Chain: under it, cap height 22, white -> gold -> amber, pulsing on pickup. A multiplier from x1;
    // shown from x2 up and hidden at x1 (lead, M3 stage B).
    if (st.chain >= 2) {
        // GAME_DESIGN 5 (C2): with thousands separators, as the score (X12,369).
        char digits[32];
        groupDigits(digits, sizeof digits, st.chain);
        std::snprintf(text, sizeof text, "X%s", digits);
        const float pulse = 1.0f + 0.6f * st.chainPulse;
        font.drawField(d.hud, {m, top + 96.0f * s}, 22.0f * s, 1.0f * s * (1.0f + 0.15f * st.chainPulse), col(white, 2.0f * pulse),
                       col(gold, 2.0f * pulse), col(amber, 2.0f * pulse), text);
        d.pools.glow({m + 44.0f * s, top + 107.0f * s}, 60.0f * s, gold * (0.10f * pulse));
    }
    // Play test 2 (look.md 6): the Shard level under the Chain: LV n at cap 12 and a bar 220 px wide and 5 px tall, a gold
    // fill on a white outline at 0.3. When it fills it flashes white (the number steps, with the band's two-note rise) and
    // it stays full until the level's Trio opens, when it empties.
    if (st.runHud) {
        const float cap = 12.0f * s, y = top + 132.0f * s;
        char lv[16];
        std::snprintf(lv, sizeof lv, "LV %u", st.level);
        const Vec3 lc = col(mix(gold, white, 0.6f + 0.4f * st.levelFlash), 1.3f + 0.9f * st.levelFlash);
        font.drawField(d.hud, {m, y}, cap, 0.75f * s, lc, lc, lc, lv);
        const float x0 = m + 70.0f * s, x1 = x0 + 220.0f * s, yc = y + 0.5f * cap, hh = 2.5f * s;
        const Vec3 oc = col(white, 0.3f);
        const float ow = 0.45f * s;
        d.hud.line({x0, yc - hh}, {x1, yc - hh}, ow, oc, oc, 0);
        d.hud.line({x0, yc + hh}, {x1, yc + hh}, ow, oc, oc, 0);
        d.hud.line({x0, yc - hh}, {x0, yc + hh}, ow, oc, oc, 0);
        d.hud.line({x1, yc - hh}, {x1, yc + hh}, ow, oc, oc, 0);
        const float fill = st.levelFull ? 1.0f : clampf(st.levelFill, 0.0f, 1.0f);
        if (fill > 0.0f) {
            const float flash = st.levelFull ? 0.55f + 0.45f * st.beat : st.levelFlash;
            const Vec3 fc = col(mix(gold, white, flash), 1.7f + 0.8f * flash);
            d.hud.line({x0 + ow, yc}, {x0 + ow + (x1 - x0 - 2.0f * ow) * fill, yc}, hh - ow, fc, fc, kFlatStart | kFlatEnd);
            d.pools.glow({x0 + 0.5f * (x1 - x0) * fill, yc}, 40.0f * s, gold * (0.06f + 0.10f * flash));
        }
    }
    // M4 stage B: co-op, each player's own score under the team's, in their colour, ranked (yours brighter).
    if (st.coopPlayers > 1) {
        const float y0 = top + 160.0f * s;  // under the Shard level (play test 2)
        for (u32 k = 0; k < st.rankCount && k < 4; ++k) {
            const u32 q = st.rank[k];
            char num[32];
            groupDigits(num, sizeof num, st.playerScore[q]);
            char line[48];
            std::snprintf(line, sizeof line, "P%u  %s", q + 1, num);
            const float cap = 13.0f * s;
            const float lvl = q == st.me ? 1.7f : 1.1f;
            const Vec3 pc = col(palettes.player[q], lvl);
            font.drawField(d.hud, {m, y0 + static_cast<float>(k) * 26.0f * s}, cap, 0.65f * s, pc, pc, pc, line);
        }
    }
    // Nova stock: top right, one four-point star per Nova, lit in the player colour.
    for (u32 k = 0; k < st.novaCap; ++k) {
        const Vec2 c{W - mr - 150.0f * s - static_cast<float>(k) * 50.0f * s, top + 62.0f * s};
        const bool lit = k < st.novas;
        if (lit) {
            star4(d.hud, c, 16.0f * s, 0.9f * s, col(white, 2.0f), col(player, 2.0f));
            d.pools.glow(c, 26.0f * s, player * 0.25f);
        } else {
            star4(d.hud, c, 16.0f * s, 0.9f * s, col(white, 0.3f), col(white, 0.3f));
        }
    }
    // Dash charge (for keyboard and gamepad; on touch the button carries a cooldown ring too): a short
    // bar under the stars, lit when the Dash is ready.
    {
        const float x1 = W - mr - 50.0f * s, x0 = x1 - 150.0f * s, y = top + 100.0f * s;
        const float x = x0 + (x1 - x0) * clampf(st.dashCharge, 0.0f, 1.0f);
        d.hud.line({x0, y}, {x1, y}, 1.2f * s, col(white, 0.2f), col(white, 0.2f), 0);
        if (st.dashCharge > 0.0f) d.hud.line({x0, y}, {x, y}, 1.6f * s, col(player, st.dashCharge >= 1.0f ? 1.6f : 0.7f),
                                             col(player, st.dashCharge >= 1.0f ? 1.6f : 0.7f), 0);
        const float cap = 11.0f * s;
        const Vec3 lc = col(white, st.dashCharge >= 1.0f ? 1.0f : 0.4f);
        font.drawField(d.hud, {x0, y + 10.0f * s}, cap, 0.7f * s, lc, lc, lc, "DASH");
    }
    if (st.showTouch) touch.draw(d.hud, d.pools, font, look, variant, player, st.dashCharge, st.novas > 0);
    if (st.world) drawRunHud(d, font, look, variant, palettes, palette, st, W, H, insetMin, insetMax);
}

Vec2 pauseButtonCentre(float W, float H, Vec2 insetMin, Vec2 insetMax, float& radius) {
    const float s = H / 1080.0f;
    radius = 28.0f * s;
    return Vec2{W - std::fmax(kMargin * s, insetMax.x) + 40.0f * s, insetMin.y + 52.0f * s};
}

namespace {
struct HudCtx {
    const Look* look;
    const LookVariant* variant;
};
Vec3 hudFn(const void* user, Vec3 c, float i) {
    const HudCtx* h = static_cast<const HudCtx*>(user);
    return hudColour(*h->look, *h->variant, c, i);
}
void circle(CapsuleBatch& b, Vec2 c, float r, float hw, Vec3 col, u32 segs = 32) {
    for (u32 k = 0; k < segs; ++k) {
        const float a0 = kTau * static_cast<float>(k) / segs, a1 = kTau * static_cast<float>(k + 1) / segs;
        b.line(c + Vec2{std::cos(a0), std::sin(a0)} * r, c + Vec2{std::cos(a1), std::sin(a1)} * r, hw, col, col, 0);
    }
}
float textW(const VectorFont& f, float cap, const char* t) { return f.measure(TextStyle{cap}, t); }
// The Track name card (screens.md), in bars from the Track's first downbeat; chosen: a beat to write, out on
// the third bar's downbeat, a beat to streak; drawn at 0.8 (screens.md).
constexpr float kCardWrite = 0.25f, kCardOut = 2.0f, kCardStreak = 0.25f, kCardLevel = 0.8f;
}  // namespace

void drawRunHud(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, const Palettes& palettes,
                const Palette& palette, const HudState& st, float W, float H, Vec2 insetMin, Vec2 insetMax) {
    using namespace sim;
    const World& w = *st.world;
    const float s = H / 1080.0f;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 player = palettes.player[0];
    const Vec3 gold = palettes.player[3];
    const Vec3 accent = palette.accent;
    const auto col = [&](Vec3 c, float i) { return hudColour(look, variant, c, i); };
    const HudCtx ctx{&look, &variant};
    const float cx = 0.5f * W;
    const float top = insetMin.y;
    const float bottom = insetMax.y;
    if (!st.runHud) return;
    char text[96];

    // Track and tempo: top centre, cap 12, accent.
    const Phrase& ph = w.phrase();
    std::snprintf(text, sizeof text, "TRACK %u  %s  %.0f BPM", ph.track, st.trackName, st.bpm);
    {
        const float cap = 12.0f * s;
        font.drawField(d.hud, {cx - 0.5f * textW(font, cap, text), top + 34.0f * s}, cap, 0.6f * s, col(accent, 1.2f), col(accent, 1.2f),
                       col(accent, 1.2f), text);
    }
    // The phrase bar: 520 px, four segments sized by bars, each in its own colour, the current one lit, a
    // white playhead, and "DROP IN n" in bars (look.md 6).
    {
        const float bw = 520.0f * s, x0 = cx - 0.5f * bw, y = top + 58.0f * s;
        const Vec3 segCol[4] = {palette.hue[1], gold, accent, palette.hue[2]};
        const u32 form = formOfTrack(ph.track, w.setList());
        const u32 len[4] = {kSectionBars[form][0], kSectionBars[form][1], kSectionBars[form][2], kSectionBars[form][3]};
        const u32 total = len[0] + len[1] + len[2] + len[3];
        const double barStart = w.tickOfBar(ph.bar);
        const float inBar = clampf(static_cast<float>((st.music - barStart) / kTicksPerBar), 0.0f, 1.0f);
        u32 cur = static_cast<u32>(ph.section);
        float x = x0;
        float headX = x0;
        for (u32 k = 0; k < 4; ++k) {
            const float sw = bw * static_cast<float>(len[k]) / static_cast<float>(total);
            const bool lit = k == cur && !ph.trackChange;
            const float gap = 3.0f * s;
            d.hud.line({x + gap, y}, {x + sw - gap, y}, (lit ? 2.6f : 1.6f) * s, col(segCol[k], lit ? 1.6f : 0.35f), col(segCol[k], lit ? 1.6f : 0.35f), 0);
            if (lit) {
                const float u = clampf((std::fmin(static_cast<float>(ph.sectionBar), static_cast<float>(len[k]) - 1.0f) + inBar) / static_cast<float>(len[k]), 0.0f, 1.0f);
                headX = x + sw * u;
            }
            x += sw;
        }
        if (ph.trackChange) headX = x0 + bw * clampf((static_cast<float>(ph.sectionBar) + inBar) / static_cast<float>(ph.sectionBars), 0.0f, 1.0f);
        d.hud.line({headX, y - 9.0f * s}, {headX, y + 9.0f * s}, 1.2f * s, col(white, 1.8f), col(white, 1.8f), 0);
        // Bars until the next Drop.
        if (ph.trackChange) {
            std::snprintf(text, sizeof text, "TRACK %u", ph.track);
        } else if (ph.section == Section::Drop) {
            std::snprintf(text, sizeof text, ph.surge() ? "SURGE" : (ph.bossDrop ? "BOSS" : "DROP"));
        } else {
            u32 n = 0;
            if (ph.section == Section::Groove) n = (len[0] - ph.sectionBar) + len[1];
            else if (ph.section == Section::Build) n = len[1] - ph.sectionBar;
            else n = (len[3] - ph.sectionBar) + len[0] + len[1];
            std::snprintf(text, sizeof text, "DROP IN %u", n);
        }
        const float cap = 11.0f * s;
        font.drawField(d.hud, {x0 + bw + 14.0f * s, y + 0.5f * cap}, cap, 0.55f * s, col(white, 1.0f), col(white, 1.0f), col(white, 1.0f), text);
    }
    // The pause ring: top right corner, 28 px with two bars.
    {
        float r = 0.0f;
        const Vec2 c = pauseButtonCentre(W, H, insetMin, insetMax, r);
        const float in = st.pausePressed ? 2.0f : 0.8f;
        circle(d.hud, c, r, (st.pausePressed ? 1.5f : 1.0f) * s, col(white, in), 28);
        for (float sx : {-6.0f, 6.0f}) d.hud.line(c + Vec2{sx * s, -9.0f * s}, c + Vec2{sx * s, 9.0f * s}, 1.6f * s, col(white, in), col(white, in), 0);
    }
    const PlayerState& me = w.player(w.viewPlayer());
    // The band row (bottom centre): four Riff slots with five level pips each, then the Mods taken.
    const float rowY = H - bottom - 64.0f * s;
    const float slot = 104.0f * s;
    float slotX[kRiffSlots];
    {
        const float r = 26.0f * s;
        u8 riffOrder[kRiffSlots] = {kNoUpgrade, kNoUpgrade, kNoUpgrade, kNoUpgrade};
        u32 n = 0;
        // Riffs in the order they were taken.
        for (u32 pass = 1; pass < 256 && n < kRiffSlots; ++pass)
            for (u8 u = kOrbitals; u < kUpgradeCount; ++u)
                if (me.loadout.order[u] == pass) riffOrder[n++] = u;
        for (u32 k = 0; k < kRiffSlots; ++k) {
            const Vec2 c{cx + (static_cast<float>(k) - 1.5f) * slot, rowY};
            slotX[k] = c.x;
            if (riffOrder[k] == kNoUpgrade) {
                circle(d.hud, c, r, 0.8f * s, col(white, 0.22f), 24);
                continue;
            }
            const u8 u = riffOrder[k];
            if (st.icons) st.icons->drawHud(d.hud, st.upgradeIcons[u], c, r * 0.9f, 1.0f, hudFn, &ctx);
            d.pools.glow(c, 40.0f * s, player * 0.12f);
            const u32 lv = me.loadout.level(u);
            for (u32 p = 0; p < kRiffMaxLevel; ++p) {
                const Vec2 q{c.x + (static_cast<float>(p) - 2.0f) * 11.0f * s, c.y + r + 14.0f * s};
                if (p < lv) d.hud.line(q, q, 3.0f * s, col(white, 1.5f), col(white, 1.5f), 0);
                else circle(d.hud, q, 3.0f * s, 0.5f * s, col(white, 0.3f), 8);
            }
        }
        // Mods: small icons with their stacks, to the right.
        float mx = cx + 2.0f * slot + 30.0f * s;
        for (u32 pass = 1; pass < 256; ++pass) {
            for (u8 u = 0; u < kModCount; ++u) {
                if (me.loadout.order[u] != pass) continue;
                const Vec2 c{mx, rowY};
                if (st.icons) st.icons->drawHud(d.hud, st.upgradeIcons[u], c, 15.0f * s, 0.9f, hudFn, &ctx);
                const u32 n = me.loadout.mods[u];
                for (u32 p = 0; p < n; ++p) {
                    const Vec2 q{c.x + (static_cast<float>(p) - 0.5f * static_cast<float>(n - 1)) * 7.0f * s, c.y + 26.0f * s};
                    d.hud.line(q, q, 2.0f * s, col(white, 1.3f), col(white, 1.3f), 0);
                }
                mx += 44.0f * s;
            }
            if (mx > W - 260.0f * s) break;
        }
    }
    // Active Boosts, above the Riff slots (M3 stage C): the icon with one pip per bar left to its right, its
    // name under it, never across it (gold, cap 11 wu = 16.5 px, 6 wu = 9 px under the frame) and, through the
    // two bars after it was taken, its effect words (screens.md) under the name.
    {
        u32 count = 0;
        for (const ActiveBoost& b : me.boosts) count += b.type != kNoBoost ? 1u : 0u;
        u32 n = 0;
        for (const ActiveBoost& b : me.boosts) {
            if (b.type == kNoBoost) continue;
            const BoostInfo& bi = boostInfo(b.type);
            const float x = cx + (static_cast<float>(n) - 0.5f * static_cast<float>(count - 1)) * 220.0f * s, y = rowY - 116.0f * s;
            if (st.icons) st.icons->drawHud(d.hud, st.boostIcons[b.type], {x, y}, 15.0f * s, 1.0f, hudFn, &ctx);
            const float cap = 16.5f * s, top = y + (15.0f + 9.0f) * s;
            font.drawField(d.hud, {x - 0.5f * textW(font, cap, bi.name), top}, cap, 0.7f * s, col(gold, 1.4f), col(gold, 1.4f), col(gold, 1.4f), bi.name);
            const float since = static_cast<float>((st.music - b.from) / kTicksPerBar);
            if (since >= 0.0f && since < 2.0f) {
                const float k = since < 1.5f ? 1.0f : (2.0f - since) * 2.0f, c2 = 11.0f * s;
                font.drawField(d.hud, {x - 0.5f * textW(font, c2, bi.words), top + cap + 8.0f * s}, c2, 0.55f * s, col(white, 1.1f * k),
                               col(white, 1.1f * k), col(white, 1.1f * k), bi.words);
            }
            const u32 left = b.type == kHalo ? b.hits : static_cast<u32>(std::ceil((b.until - st.music) / kTicksPerBar));
            for (u32 p = 0; p < left && p < 8; ++p) {
                const Vec2 q{x + 28.0f * s + static_cast<float>(p) * 10.0f * s, y};
                d.hud.line(q, q, 2.4f * s, col(gold, 1.6f), col(gold, 1.6f), 0);
            }
            ++n;
        }
    }
    // The orb just taken flies to the band row.
    if (st.takenAge >= 0.0f && st.takenAge < 0.6f && st.icons) {
        const TrioState& tr = w.trio(w.viewPlayer());
        const u8 u = tr.lastTaken;
        if (u < kUpgradeCount) {
            Vec2 target{cx + 2.0f * slot, rowY};
            if (isRiff(u)) target = Vec2{slotX[me.loadout.riffsOwned() ? me.loadout.riffsOwned() - 1 : 0], rowY};
            const float k = st.takenAge / 0.6f;
            const float e = k * k * (3.0f - 2.0f * k);
            const Vec2 p = st.takenFrom + (target - st.takenFrom) * e;
            st.icons->drawHud(d.hud, st.upgradeIcons[u], p, (34.0f - 14.0f * e) * s, 1.4f, hudFn, &ctx);
        }
    }
    // The Track name card (screens.md, M3 stage C): on the Track's first downbeat (the run's first Track too)
    // it writes itself across the lower third in a beat, a two-arm flare sweeping along the name as it lands;
    // it holds to the second bar's end, then its letters streak out to both sides on the beat. `TRACK n` cap 28
    // white above the name at cap 110 in the palette's hues, 8 px tubes; drawn at 0.8 over the fight.
    {
        const DirectorState& dir = w.director();
        const float u = dir.trackBar == kNoBar ? -1.0f : static_cast<float>((st.music - w.tickOfBar(dir.trackBar)) / kTicksPerBar);
        if (u >= 0.0f && u < kCardOut + kCardStreak) {
            const float level = kCardLevel;
            const float write = clampf(u / kCardWrite, 0.0f, 1.0f);
            const float out = clampf((u - kCardOut) / kCardStreak, 0.0f, 1.0f);
            char small[24];
            std::snprintf(small, sizeof small, "TRACK %u", ph.track);
            // GAME_DESIGN 8 (the loop is where a run ends): the loop's first Track card says so, ENCORE above the name and
            // NO MORE REFILLS under it; once the halo burns down its last line is HALO n (the capacity the burn left; the first
            // card that burns shows both lines), all at the size of the TRACK n line.
            const bool encore = ph.track == w.setList() + 1;  // M5: the loop's first Track, after the set list
            const bool burnt = ph.track >= w.setList() + sim::tuning::kLoopHaloBurnFrom;
            char halo[16];
            std::snprintf(halo, sizeof halo, "HALO %u", static_cast<u32>(w.player(w.viewPlayer()).cellCap));
            const float under = 28.0f * s + 26.0f * s;  // a small line and its gap
            const float nameTop = 0.62f * H + (encore ? 40.0f * s : 0.0f);  // (the extra line above stays clear of the ship)
            const char* lines[5] = {small, st.trackName, nullptr, nullptr, nullptr};
            float caps[5] = {28.0f * s, 110.0f * s, 28.0f * s, 28.0f * s, 28.0f * s};
            float tubes[5] = {1.2f * s, 4.0f * s, 1.2f * s, 1.2f * s, 1.2f * s};
            float tops[5] = {nameTop - caps[0] - 26.0f * s, nameTop, 0.0f, 0.0f, 0.0f};
            u32 count = 2;
            float below = nameTop + caps[1] + 26.0f * s;
            if (encore) {
                lines[count] = "ENCORE";
                tops[count++] = nameTop - 2.0f * under;
                lines[count] = "NO MORE REFILLS";
                tops[count++] = below;
                below += under;
            }
            if (burnt) {
                lines[count] = halo;
                tops[count++] = below;
            }
            for (u32 l = 0; l < count; ++l) {
                const char* t = lines[l];
                const u32 n = static_cast<u32>(std::strlen(t));
                const float cap = caps[l], x0 = cx - 0.5f * textW(font, cap, t);
                for (u32 i = 0; i < n; ++i) {
                    if (t[i] == ' ') continue;
                    // Written left to right: each letter takes a short ramp once the writing passes it.
                    const float on = clampf((write * static_cast<float>(n + 2) - static_cast<float>(i)) * 0.5f, 0.0f, 1.0f);
                    if (on <= 0.0f) continue;
                    const float side = (static_cast<float>(i) + 0.5f < 0.5f * static_cast<float>(n)) ? -1.0f : 1.0f;
                    const char one[2] = {t[i], 0};
                    const float x = x0 + static_cast<float>(i) * cap;
                    // The streak: the letter flies out with two fading copies behind it.
                    for (u32 g = 0; g < (out > 0.0f ? 3u : 1u); ++g) {
                        const float lag = 1.0f - 0.3f * static_cast<float>(g);
                        const float dx = side * 0.75f * W * out * out * lag;
                        const float k = level * on * (1.0f - out) * (g == 0 ? 1.0f : 0.45f / static_cast<float>(g));
                        if (k <= 0.01f) continue;
                        if (l != 1)
                            font.drawField(d.hud, {x + dx, tops[l]}, cap, tubes[l], col(white, 1.4f * k), col(white, 1.4f * k), col(white, 1.4f * k), one);
                        else
                            font.drawField(d.hud, {x + dx, tops[l]}, cap, tubes[l], col(palette.hue[0], 1.7f * k), col(palette.hue[1], 1.7f * k),
                                           col(palette.hue[2], 1.7f * k), one);
                    }
                }
            }
            // The flare: two arms of white light, riding the writing's front along the name once.
            if (write > 0.0f && write < 1.0f) {
                const float w0 = textW(font, caps[1], st.trackName);
                const Vec2 c{cx - 0.5f * w0 + w0 * write, nameTop + 0.5f * caps[1]};
                const float arm = 0.9f * caps[1], fl = std::sin(write * kPi);
                for (float sgn : {-1.0f, 1.0f})
                    d.hud.line(c, c + Vec2{sgn * arm * (0.6f + 0.4f * fl), 0.0f}, 2.0f * s, col(white, 2.2f * fl * level), Vec3{}, 0);
                d.hud.line(c, c, 5.0f * s, col(white, 2.2f * fl * level), col(white, 2.2f * fl * level), 0);
            }
        }
    }
    // The boss: its name card for two bars at the entrance, then a bar under the phrase bar.
    const BossState& boss = w.boss();
    if (boss.active && !boss.dying) {
        const Vec3 amber = palettes.playerDeep[3];
        const bool mini = isMiniBoss(boss.kind);
        const char* name = bossName(boss.kind);
        const float bars = static_cast<float>((st.music - boss.enterTick) / kTicksPerBar);
        // The name card: a boss's for two bars; a mini-boss's, small, for one (minibosses.md).
        const float cardBars = mini ? 1.0f : 2.0f;
        if (bars < cardBars) {
            const float fade = clampf(bars * 4.0f, 0.0f, 1.0f) * clampf((cardBars - bars) * (mini ? 4.0f : 2.0f), 0.0f, 1.0f);
            const float cap = (mini ? 40.0f : 72.0f) * s;
            font.drawField(d.hud, {cx - 0.5f * textW(font, cap, name), 0.30f * H}, cap, (mini ? 1.8f : 3.0f) * s, col(white, 2.0f * fade),
                           col(gold, 2.0f * fade), col(amber, 2.0f * fade), name);
            char sub[48];
            if (mini) std::snprintf(sub, sizeof sub, "MINI-BOSS");
            else std::snprintf(sub, sizeof sub, "BOSS OF %s", st.trackName);
            const float c2 = (mini ? 12.0f : 14.0f) * s;
            // (Under the name: the origin is the top of the caps; the stage B card drew it across the name.)
            font.drawField(d.hud, {cx - 0.5f * textW(font, c2, sub), 0.30f * H + cap + 18.0f * s}, c2, 0.6f * s, col(gold, 1.2f * fade),
                           col(gold, 1.2f * fade), col(gold, 1.2f * fade), sub);
        }
        // The health bar under the phrase bar with its name; a mini-boss's half the height (minibosses.md).
        const float y = top + 96.0f * s, bw = 300.0f * s, x0 = cx - 0.5f * bw;
        const float cap = 11.0f * s;
        font.drawField(d.hud, {x0 - textW(font, cap, name) - 16.0f * s, y + 0.5f * cap}, cap, 0.55f * s, col(gold, 1.4f), col(gold, 1.4f),
                       col(gold, 1.4f), name);
        float hp = boss.coreHp, max = boss.coreMax;
        const u32 parts = boss.kind == BossKind::Longwave ? w.species().longwave.segments
                                                           : (boss.kind == BossKind::Choir ? ChoirData::kVoices : (boss.kind == BossKind::Kaleidos ? 3u : 0u));
        if (boss.kind == BossKind::Longwave || boss.kind == BossKind::Choir) {
            // The whole: every part's health and the head's (the Choir has no core).
            for (u32 k = 0; k < parts; ++k) hp += (boss.alive & (1u << k)) ? boss.partHp[k] : 0.0f;
            max += boss.partMax * static_cast<float>(parts);
        }
        const float f = max > 0.0f ? clampf(hp / max, 0.0f, 1.0f) : 0.0f;
        const float thick = mini ? 1.0f : 2.0f;
        d.hud.line({x0, y}, {x0 + bw, y}, 0.5f * thick * s, col(white, 0.18f), col(white, 0.18f), 0);
        if (f > 0.0f) d.hud.line({x0, y}, {x0 + bw * f, y}, thick * s, col(gold, 1.8f), col(amber, 1.8f), 0);
        if (boss.kind == BossKind::Kaleidos) {
            for (u32 p = 0; p < 3; ++p) {
                const float pf = boss.partMax > 0.0f ? clampf(boss.partHp[p] / boss.partMax, 0.0f, 1.0f) : 0.0f;
                const float px = x0 + bw + 20.0f * s + static_cast<float>(p) * 30.0f * s;
                d.hud.line({px, y}, {px + 22.0f * s, y}, 0.8f * s, col(palette.hue[p], 0.3f), col(palette.hue[p], 0.3f), 0);
                if (pf > 0.0f) d.hud.line({px, y}, {px + 22.0f * s * pf, y}, 1.8f * s, col(palette.hue[p], 1.6f), col(palette.hue[p], 1.6f), 0);
            }
        } else if (boss.kind == BossKind::Choir || boss.kind == BossKind::Longwave) {
            // A pip for each Voice / segment still standing, after the bar (the Choir's in pearl, Longwave's in its hues).
            const float step = (boss.kind == BossKind::Choir ? 16.0f : 6.0f) * s, len = (boss.kind == BossKind::Choir ? 11.0f : 3.5f) * s;
            for (u32 k = 0; k < parts; ++k) {
                const float px = x0 + bw + 20.0f * s + static_cast<float>(k) * step;
                const Vec3 c = boss.kind == BossKind::Choir ? mix(white, palette.hue[k % 3], 0.25f) : palette.hue[k * 3 / (parts + 1)];
                const bool live = boss.alive & (1u << k);
                const float pf = live && boss.partMax > 0.0f ? clampf(boss.partHp[k] / boss.partMax, 0.0f, 1.0f) : 0.0f;
                d.hud.line({px, y}, {px + len, y}, 0.8f * s, col(c, 0.25f), col(c, 0.25f), 0);
                if (pf > 0.0f) d.hud.line({px, y}, {px + len * pf, y}, 1.8f * s, col(c, 1.5f), col(c, 1.5f), 0);
            }
        }
    }
    // A debut's name (GAME_DESIGN 8: the spotlight, with its name on screen).
    if (st.spotlight && st.spotlightAge >= 0.0f && st.spotlightAge < 3.0f) {
        const float fade = clampf(st.spotlightAge * 3.0f, 0.0f, 1.0f) * clampf((3.0f - st.spotlightAge) * 1.5f, 0.0f, 1.0f);
        const float cap = 26.0f * s;
        font.drawField(d.hud, {cx - 0.5f * textW(font, cap, st.spotlight), 0.24f * H}, cap, 1.1f * s, col(white, 1.6f * fade), col(white, 1.6f * fade),
                       col(accent, 1.6f * fade), st.spotlight);
        const float c2 = 12.0f * s;
        font.drawField(d.hud, {cx - 0.5f * textW(font, c2, "NEW"), 0.24f * H - cap - 12.0f * s}, c2, 0.6f * s, col(gold, 1.4f * fade),
                       col(gold, 1.4f * fade), col(gold, 1.4f * fade), "NEW");
    }
    // The first three Trios of a run: the hint (screens.md, play test 1).
    const TrioState& tr = w.trio(w.viewPlayer());
    if (tr.open && tr.opened >= 1 && tr.opened <= 3) {
        const float cap = 14.0f * s;
        const char* hint = "STAY ON ONE TO TAKE IT";
        // Play test 2 (the lead's review): while an active Boost's label shows above the Riff slots, the hint sits above it.
        bool boostShown = false;
        for (const ActiveBoost& b : me.boosts) boostShown = boostShown || b.type != kNoBoost;
        const float y = boostShown ? rowY - (116.0f + 15.0f + 20.0f) * s - cap : H - bottom - 150.0f * s;
        font.drawField(d.hud, {cx - 0.5f * textW(font, cap, hint), y}, cap, 0.7f * s, col(white, 0.7f), col(white, 0.7f), col(white, 0.7f), hint);
    }
}

}  // namespace rf
