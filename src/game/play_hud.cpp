#include "game/play_hud.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "sim/sim_types.h"

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
    padR_ = kPadRadius * s_;
    const float m = kMargin * s_;
    const float left = std::fmax(m, insetMin.x), right = std::fmax(m, insetMax.x);
    const float bottom = insetMax.y;
    move_.rest = {left + 210.0f * s_, h - bottom - 250.0f * s_};
    aim_.rest = {w - right - 210.0f * s_, h - bottom - 250.0f * s_};
    nova_.centre = {w - right - 430.0f * s_, h - bottom - 330.0f * s_};
    nova_.radius = novaR;
    dash_.centre = {w - right - 300.0f * s_, h - bottom - 520.0f * s_};
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
    Stick& s = p.x < 0.5f * w_ ? move_ : aim_;
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
    // Chain: under it, cap height 22, white -> gold -> amber, pulsing on pickup.
    std::snprintf(text, sizeof text, "X%u", st.chain);
    const float pulse = 1.0f + 0.6f * st.chainPulse;
    font.drawField(d.hud, {m, top + 96.0f * s}, 22.0f * s, 1.0f * s * (1.0f + 0.15f * st.chainPulse), col(white, 2.0f * pulse),
                   col(gold, 2.0f * pulse), col(amber, 2.0f * pulse), text);
    d.pools.glow({m + 44.0f * s, top + 107.0f * s}, 60.0f * s, gold * (0.10f * pulse));
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
    if (!st.alive) {
        // Death: a bare "TAP TO RETRY" (stage A; the game-over screen is stage B).
        const char* msg = "TAP TO RETRY";
        const float cap = 40.0f * s;
        const float width = font.measure(TextStyle{cap}, msg);
        const float in = st.retryPending ? 0.6f : 1.4f + 0.8f * st.kick;
        font.drawField(d.hud, {0.5f * W - 0.5f * width, 0.42f * H}, cap, 1.8f * s, col(white, in), col(white, in), col(accent, in), msg);
    }
    (void)insetMax;
}

}  // namespace rf
