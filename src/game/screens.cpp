// The screens of M3 (design/screens.md): the first-launch notice, the title, pause, game over and
// settings, as Play's (they share its run, band and settings), and the Ui helper they are drawn with.

#include "game/screens.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "audio/band.h"
#include "content/icons.h"
#include "core/log.h"
#include "game/play.h"
#include "game/play_hud.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kTwoPi = 6.28318530718f;
Vec2 polarV(float a, float r) { return Vec2{std::cos(a) * r, std::sin(a) * r}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
struct UiColour {
    const Ui* ui;
};
Vec3 uiColourFn(const void* user, Vec3 c, float i) { return static_cast<const UiColour*>(user)->ui->col(c, i); }
Vec3 trackAccent(const Palettes& p, u32 track, u32 songs) {
    const u32 want = sim::songOfTrack(track, songs) + 1u;  // the Track's song's palette (ultraviolet, solar, reef, ...)
    for (u32 i = 0; i < p.count; ++i)
        if (p.list[i].track == want) return p.list[i].accent;
    return p.list[0].accent;
}
void groupDigits(char* out, size_t cap, u64 v) {
    char raw[32];
    std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(v));
    const size_t n = std::strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < n && o + 2 < cap; ++i) {
        out[o++] = raw[i];
        const size_t left = n - 1 - i;
        if (left && left % 3 == 0) out[o++] = ',';
    }
    out[o] = 0;
}
}  // namespace

// ---------------------------------------------------------------------------------------------- Ui

void Ui::begin(FrameDraw& d, const VectorFont& font, const Look& look, const LookVariant& variant, float W, float H, Vec2 insetMin,
               Vec2 insetMax, const UiPointer& p, Vec3 player, Vec3 accent) {
    d_ = &d;
    font_ = &font;
    look_ = &look;
    variant_ = &variant;
    W_ = W;
    H_ = H;
    s_ = H / 1080.0f;
    insetMin_ = insetMin;
    insetMax_ = insetMax;
    p_ = p;
    player_ = player;
    accent_ = accent;
    boxCount_ = 0;
    quiet_ = 0;
    screen_ = "";
}

void Ui::end() {
    if (p_.released || !p_.down) {
        active_ = 0;
        dragging_ = 0;
    }
    checkLayout();
}

void Ui::keep(Vec2 a, Vec2 b, bool control, const char* label) {
    if (quiet_ || boxCount_ >= kBoxes) return;
    Box& k = boxes_[boxCount_++];
    k.a = a;
    k.b = b;
    k.control = control;
    std::snprintf(k.label, sizeof k.label, "%s", label ? label : "");
}

void Ui::checkLayout() {
    // screens.md: at least 24 px (at 1080 lines) between two controls, and between a control and a line of text. A line
    // wholly inside a control is its label. Each pair is reported once (by its labels and the screen).
    const float need = kControlGapPx * s_ - 0.5f;
    for (u32 i = 0; i < boxCount_; ++i) {
        for (u32 j = i + 1; j < boxCount_; ++j) {
            const Box& p = boxes_[i];
            const Box& q = boxes_[j];
            if (!p.control && !q.control) continue;
            const auto within = [](const Box& in, const Box& out) {
                return in.a.x >= out.a.x - 0.5f && in.b.x <= out.b.x + 0.5f && in.a.y >= out.a.y - 0.5f && in.b.y <= out.b.y + 0.5f;
            };
            if ((!p.control && within(p, q)) || (!q.control && within(q, p))) continue;
            const float gx = std::fmax(q.a.x - p.b.x, p.a.x - q.b.x), gy = std::fmax(q.a.y - p.b.y, p.a.y - q.b.y);
            const float gap = std::fmax(gx, gy);
            if (gap >= need) continue;
            u32 h = 2166136261u;
            for (const char* t : {screen_, p.label, q.label})
                for (const char* c = t; *c; ++c) h = (h ^ static_cast<u8>(*c)) * 16777619u;
            bool seen = false;
            for (u32 k = 0; k < reportedCount_; ++k) seen = seen || reported_[k] == h;
            if (seen) continue;
            if (reportedCount_ < 64) reported_[reportedCount_++] = h;
            ++layoutWarnings_;
            RF_WARN("ui: %s: '%s' and '%s' are %.0f px apart (controls keep %.0f px clear)", screen_[0] ? screen_ : "screen", p.label, q.label,
                    static_cast<double>(gap / s_), static_cast<double>(kControlGapPx));
        }
    }
}

Vec3 Ui::col(Vec3 c, float i) const { return hudColour(*look_, *variant_, c, i); }

float Ui::measure(float cap, const char* t) const { return font_->measure(TextStyle{cap}, t); }

float Ui::text(Vec2 at, float cap, Vec3 c, float i, const char* t, float align, float halfWidth) {
    const float w = measure(cap, t);
    const Vec3 k = col(c, i);
    keep({at.x - align * w, at.y - cap}, {at.x + (1.0f - align) * w, at.y}, false, t);
    // The font's origin is the top of the caps; t is the baseline.
    font_->drawField(d_->hud, {at.x - align * w, at.y - cap}, cap, halfWidth > 0.0f ? halfWidth : std::fmax(0.045f * cap, 0.55f * s_), k, k, k, t);
    return w;
}

void Ui::tube(Vec2 at, float cap, Vec3 top, Vec3 mid, Vec3 bottom, float i, const char* t, float align) {
    const float w = measure(cap, t);
    const Vec2 o{at.x - align * w, at.y - cap};
    keep(o, {o.x + w, at.y}, false, t);
    font_->drawField(d_->hud, o, cap, 0.03f * cap, col(top, i), col(mid, i), col(bottom, i), t);
    const Vec3 wc = col(Vec3{1.0f, 1.0f, 1.0f}, i * 0.8f);
    font_->drawField(d_->hud, o, cap, 0.0075f * cap, wc, wc, wc, t);
}

void Ui::ring(Vec2 c, float r, float hw, Vec3 c0, float i, u32 segs) {
    const Vec3 k = col(c0, i);
    for (u32 j = 0; j < segs; ++j) {
        const float a0 = kTwoPi * static_cast<float>(j) / segs, a1 = kTwoPi * static_cast<float>(j + 1) / segs;
        d_->hud.line(c + polarV(a0, r), c + polarV(a1, r), hw, k, k, 0);
    }
}

void Ui::dot(Vec2 c, float r, Vec3 colour, float i) {
    const Vec3 k = col(colour, i);
    d_->hud.line(c, c, r, k, k, 0);
}

void Ui::roundRect(Vec2 a, Vec2 b, float r, float hw, Vec3 c, float i) {
    const Vec3 k = col(c, i);
    r = std::fmin(r, 0.5f * std::fmin(b.x - a.x, b.y - a.y));
    d_->hud.line({a.x + r, a.y}, {b.x - r, a.y}, hw, k, k, 0);
    d_->hud.line({a.x + r, b.y}, {b.x - r, b.y}, hw, k, k, 0);
    d_->hud.line({a.x, a.y + r}, {a.x, b.y - r}, hw, k, k, 0);
    d_->hud.line({b.x, a.y + r}, {b.x, b.y - r}, hw, k, k, 0);
    const Vec2 cs[4] = {{b.x - r, a.y + r}, {a.x + r, a.y + r}, {a.x + r, b.y - r}, {b.x - r, b.y - r}};
    for (u32 q = 0; q < 4; ++q) {
        const float a0 = -0.25f * kTwoPi * static_cast<float>(q + 1);
        for (u32 j = 0; j < 6; ++j) {
            const float u0 = a0 + 0.25f * kTwoPi * static_cast<float>(j) / 6.0f, u1 = a0 + 0.25f * kTwoPi * static_cast<float>(j + 1) / 6.0f;
            d_->hud.line(cs[q] + polarV(u0, r), cs[q] + polarV(u1, r), hw, k, k, 0);
        }
    }
}

bool Ui::area(u32 id, Vec2 a, Vec2 b, bool& pressed) {
    char name[16];
    std::snprintf(name, sizeof name, "area %u", id);
    keep(a, b, true, name);
    return hit(id, a, b, pressed);
}

bool Ui::hit(u32 id, Vec2 a, Vec2 b, bool& pressed) {
    const bool in = inside(a, b);
    if (p_.pressed && in) active_ = id;
    pressed = active_ == id && p_.down && in;
    return p_.released && active_ == id && in;
}

bool Ui::pill(u32 id, Vec2 c, Vec2 size, const char* label, float cap, bool primary, bool enabled, bool selected, Vec3 colour) {
    const Vec2 a = c - size * 0.5f, b = c + size * 0.5f;
    bool pressed = false;
    keep(a, b, true, label);
    const bool tapped = enabled && hit(id, a, b, pressed);
    const Vec3 tone = primary ? player_ : (colour.x + colour.y + colour.z > 0.0f ? colour : Vec3{1.0f, 1.0f, 1.0f});
    const float r = 0.5f * size.y;
    // screens.md: a control that cannot be used yet is drawn disabled (outline at 0.3, label at 0.4), unfilled.
    if (enabled && (pressed || selected || primary)) {
        // Glass: a capsule as tall as the pill (primary controls are always filled in their colour).
        const float level = pressed ? 0.45f : (primary ? 0.30f : 0.20f);
        const Vec3 g = col(tone, level);
        d_->hud.line({a.x + r, c.y}, {b.x - r, c.y}, r, g, g, 0);
        d_->pools.glow(c, size.x * 0.55f, tone * (pressed ? 0.25f : 0.10f));
    }
    const float outline = !enabled ? 0.3f : (pressed ? 2.0f : (selected ? 1.6f : 0.9f));
    roundRect(a, b, r, (pressed ? 1.5f : 1.0f) * s_, tone, outline);
    ++quiet_;
    text({c.x, c.y + 0.5f * cap}, cap, Vec3{1.0f, 1.0f, 1.0f}, !enabled ? 0.4f : (pressed ? 1.8f : 1.1f), label);
    --quiet_;
    return tapped;
}

u32 Ui::segmented(u32 id, Vec2 c, float width, float height, const char* const* labels, u32 n, u32 current, float cap) {
    const float gap = 10.0f * s_;
    const float w = (width - gap * static_cast<float>(n - 1)) / static_cast<float>(n);
    u32 out = current;
    // One control of n segments (its pills are not controls of their own for the layout rule).
    keep({c.x - 0.5f * width, c.y - 0.5f * height}, {c.x + 0.5f * width, c.y + 0.5f * height}, true, labels[0]);
    ++quiet_;
    for (u32 k = 0; k < n; ++k) {
        const Vec2 pc{c.x - 0.5f * width + w * (static_cast<float>(k) + 0.5f) + gap * static_cast<float>(k), c.y};
        if (pill(id * 16u + k, pc, {w, height}, labels[k], cap, false, true, k == current, player_)) out = k;
    }
    --quiet_;
    return out;
}

float Ui::fader(u32 id, Vec2 left, float width, float value, float meter, const char* label) {
    // A track of forty short bars that is also a meter; a 48 px thumb in the player colour; value at right.
    const float h = 36.0f * s_;
    const Vec2 a{left.x, left.y - h}, b{left.x + width, left.y + h};
    bool pressed = false;
    // The fader with its label above and its value at the right is one control.
    keep({a.x, a.y - 16.0f * s_}, {b.x + 24.0f * s_ + measure(22.0f * s_, "100"), b.y}, true, label);
    ++quiet_;
    hit(id, a, b, pressed);
    if (p_.pressed && inside(a, b)) dragging_ = id;
    if (dragging_ == id && p_.down) value = clamp01((p_.pos.x - left.x) / width);
    const u32 bars = 40;
    for (u32 k = 0; k < bars; ++k) {
        const float u = (static_cast<float>(k) + 0.5f) / bars;
        const float x = left.x + width * u;
        Vec3 c{1.0f, 1.0f, 1.0f};
        float i = 0.0f;
        if (u <= meter) {
            c = u > 0.8f ? Vec3{0.98f, 0.49f, 0.07f} : player_ + (accent_ - player_) * u;
            i = 1.3f;
        } else if (u <= value) {
            i = 0.22f;
        } else {
            i = 0.08f;
        }
        if (i > 0.0f) d_->hud.line({x, left.y - 10.0f * s_}, {x, left.y + 10.0f * s_}, 2.2f * s_, col(c, i), col(c, i), 0);
    }
    const Vec2 thumb{left.x + width * value, left.y};
    const Vec3 tk = col(player_, dragging_ == id ? 2.0f : 1.3f);
    d_->hud.line(thumb, thumb, 24.0f * s_, tk, tk, 0);
    d_->pools.glow(thumb, 50.0f * s_, player_ * 0.18f);
    text({left.x, left.y - 30.0f * s_}, 16.0f * s_, Vec3{1.0f, 1.0f, 1.0f}, 1.0f, label, 0.0f);
    char v[16];
    std::snprintf(v, sizeof v, "%d", static_cast<int>(std::lround(value * 100.0f)));
    text({left.x + width + 24.0f * s_, left.y + 11.0f * s_}, 22.0f * s_, Vec3{1.0f, 1.0f, 1.0f}, 1.2f, v, 0.0f);
    --quiet_;
    return value;
}

// ---------------------------------------------------------------------------------------------- screens

void Play::renderScreens(FrameDraw& d, FrameParams& params, const LookVariant& variant, float W, float H, Vec2 insetMin, Vec2 insetMax) {
    // (M4: the primary controls carry this device's player colour, P1's in solo.)
    ui_.begin(d, *font_, *look_, variant, W, H, insetMin, insetMax, uiPtr_, palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], palette_->accent);
    static const char* const kNames[] = {"notice", "title", "run", "pause", "game over", "settings", "co-op join", "co-op lobby", "local network"};
    ui_.setScreenName(static_cast<u32>(screen_) < 9 ? kNames[static_cast<u32>(screen_)] : "screen");
    if (screen_ == Screen::Settings) ui_.setScreenName(settingsTab_ == 0 ? "settings: sound" : (settingsTab_ == 1 ? "settings: controls" : "settings: look"));
    switch (screen_) {
        case Screen::Notice:
            params.exposurePump *= 0.33f;
            screenNotice(ui_);
            break;
        case Screen::Title:
            params.exposurePump *= 0.33f;
            screenTitle(ui_);
            break;
        case Screen::Paused:
            params.exposurePump *= 0.2f;
            if (coop()) screenPauseCoop(ui_);
            else screenPause(ui_);
            break;
        case Screen::GameOver:
            // screens.md: in co-op the run behind the game over is dimmed to a fifth, as in the pause.
            params.exposurePump *= coop() ? 0.2f : 0.45f;
            if (coop()) screenGameOverCoop(ui_);
            else screenGameOver(ui_);
            break;
        case Screen::Settings:
            params.exposurePump *= 0.33f;
            screenSettings(ui_);
            break;
        // M4 stage B: the co-op screens (game/screens_coop.cpp).
        case Screen::CoopJoin:
            params.exposurePump *= 0.33f;
            screenCoopJoin(ui_);
            break;
        case Screen::CoopLobby:
            params.exposurePump *= 0.33f;
            screenCoopLobby(ui_);
            break;
        case Screen::NetPermission:
            params.exposurePump *= 0.33f;
            screenNetPermission(ui_);
            break;
        default: break;
    }
    ui_.end();
    uiPtr_.pressed = uiPtr_.released = false;
}

void Play::screenNotice(Ui& ui) {
    // GAME_DESIGN 11.4: a first-launch notice, then the setting. Two choices.
    const float s = ui.s(), cx = 0.5f * ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    ui.tube({cx, 300.0f * s}, 64.0f * s, palette_->accent, white, palette_->hue[2], 1.6f, "BEFORE YOU PLAY");
    static const char* const kLines[] = {"THIS GAME HAS FLASHING LIGHTS, FAST MOVEMENT AND PULSING COLOUR,",
                                         "TIMED TO THE MUSIC. IF YOU OR ANYONE WATCHING IS SENSITIVE TO LIGHT,",
                                         "TURN ON REDUCE FLASHING. YOU CAN CHANGE IT LATER IN SETTINGS, LOOK."};
    for (u32 k = 0; k < 3; ++k) ui.text({cx, (420.0f + 48.0f * static_cast<float>(k)) * s}, 20.0f * s, white, 1.15f, kLines[k]);
    const char* rf = settings_.reduceFlashing ? "REDUCE FLASHING: ON" : "REDUCE FLASHING: OFF";
    if (ui.pill(1, {cx - 330.0f * s, 760.0f * s}, {520.0f * s, 120.0f * s}, rf, 30.0f * s, false, true, settings_.reduceFlashing)) {
        settings_.reduceFlashing = !settings_.reduceFlashing;
        applySettings();
        if (band_) band_->playSfx(audio::Sfx::UiTick);
    }
    if (ui.pill(2, {cx + 330.0f * s, 760.0f * s}, {520.0f * s, 120.0f * s}, "CONTINUE", 44.0f * s, true) || keyPrimary_) {
        settings_.noticeSeen = true;
        saveSettings(settings_);
        screen_ = Screen::Title;
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    }
    keyPrimary_ = keyBack_ = false;
}

void Play::screenTitle(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 cyan = palette_->hue[1], mag = palette_->accent, hue0 = palette_->hue[0];
    // The waveform: the actual music, about a bar of the master bus, across the width at 48% height,
    // cyan -> white -> the first enemy hue -> magenta, with a faint mirrored echo.
    {
        const u32 n = band_ && band_->active() ? band_->scope(scope_, kScopeN) : 0;
        const float y0 = 0.48f * H;
        const float amp = 70.0f * s;
        const auto colourAt = [&](float u) {
            if (u < 0.33f) return cyan + (white - cyan) * (u / 0.33f);
            if (u < 0.66f) return white + (hue0 - white) * ((u - 0.33f) / 0.33f);
            return hue0 + (mag - hue0) * ((u - 0.66f) / 0.34f);
        };
        const u32 pts = 240;
        Vec2 prev{}, prevEcho{};
        for (u32 k = 0; k <= pts; ++k) {
            const float u = static_cast<float>(k) / pts;
            float v = 0.0f;
            if (n) {
                // A bar of the master bus, smoothed over five scope points (each a 128-frame peak).
                const i32 c = static_cast<i32>(u * static_cast<float>(n - 1));
                float sum = 0.0f;
                for (i32 k = -2; k <= 2; ++k) {
                    const i32 j = c + k < 0 ? 0 : (c + k >= static_cast<i32>(n) ? static_cast<i32>(n) - 1 : c + k);
                    sum += scope_[j];
                }
                v = sum * 0.2f;
            } else {
                v = 0.25f * std::sin(u * 40.0f + static_cast<float>(music1_ / 24.0));
            }
            v = std::fmax(-1.0f, std::fmin(1.0f, v * 2.2f));
            const Vec2 p{W * u, y0 - v * amp};
            const Vec2 pe{W * u, y0 + v * amp * 0.6f};
            if (k) {
                const Vec3 c = ui.col(colourAt(u), 1.6f);
                ui.draw().hud.line(prev, p, 1.0f * s, c, c, 0);
                const Vec3 ce = ui.col(Vec3{0.3f, 0.35f, 1.0f}, 0.45f);
                ui.draw().hud.line(prevEcho, pe, 0.6f * s, ce, ce, 0);
            }
            prev = p;
            prevEcho = pe;
        }
    }
    // The logo: tubes in the same colour field, a white copy, a pool of the first hue, two flares; it
    // swells 2% on the kick.
    {
        const float k = view_.kick();
        const float cap = 190.0f * s * (1.0f + 0.02f * k);
        const char* logo = "RAVEFORM";
        const float w = ui.measure(cap, logo);
        const float x0 = cx - 0.5f * w, base = (216.0f * s) + cap;
        const float adv = w / 8.0f;
        char ch[2] = {0, 0};
        for (u32 j = 0; j < 8; ++j) {
            ch[0] = logo[j];
            const float u = (static_cast<float>(j) + 0.5f) / 8.0f;
            Vec3 c = u < 0.33f ? cyan + (white - cyan) * (u / 0.33f)
                               : (u < 0.66f ? white + (hue0 - white) * ((u - 0.33f) / 0.33f) : hue0 + (mag - hue0) * ((u - 0.66f) / 0.34f));
            const Vec3 kc = ui.col(c, 1.7f);
            ui.font().drawField(ui.draw().hud, {x0 + adv * static_cast<float>(j), base - cap}, cap, 5.5f * s, kc, kc, kc, ch);
            const Vec3 wc = ui.col(white, 1.4f);
            ui.font().drawField(ui.draw().hud, {x0 + adv * static_cast<float>(j), base - cap}, cap, 1.6f * s, wc, wc, wc, ch);
        }
        ui.draw().pools.glow({cx, base - 0.5f * cap}, 0.55f * w, hue0 * 0.10f);
        for (const Vec2 f : {Vec2{x0 + 10.0f * s, 216.0f * s}, Vec2{x0 + w - 10.0f * s, base}}) {
            for (u32 a = 0; a < 4; ++a) {
                const Vec2 dv = polarV(kTwoPi * (static_cast<float>(a) / 8.0f + 0.06f), 110.0f * s);
                const Vec3 c = ui.col(white, 1.4f);
                ui.draw().hud.line(f - dv, f + dv, 0.8f * s, Vec3{}, c, 0);
                ui.draw().hud.line(f, f + dv, 0.8f * s, c, Vec3{}, 0);
            }
        }
        ui.text({cx, base + 50.0f * s}, 22.0f * s, mag, 1.3f, "SOUND  BECOMES  LIGHT");
        // Beat dots: the current beat lit, beat 1 in the accent.
        const u32 beat = static_cast<u32>(std::fmod(clock_.heardNow(), kTicksPerBar) / kTicksPerBeat) % 4u;
        for (u32 b = 0; b < 4; ++b) {
            const Vec2 c{cx + (static_cast<float>(b) - 1.5f) * 36.0f * s, base + 86.0f * s};
            ui.dot(c, 4.0f * s, b == 0 ? mag : white, b == beat ? 1.8f : 0.35f);
        }
    }
    // PLAY (one tap: a run on the next bar line), CO-OP (M4 stage B: the join screen), SETTINGS.
    const float y = 759.0f * s;
    if (ui.pill(10, {cx, y}, {600.0f * s, 140.0f * s}, "PLAY", 52.0f * s, true) || keyPrimary_) startRun();
    if (ui.pill(11, {cx - 505.0f * s, y}, {330.0f * s, 104.0f * s}, "CO-OP", 30.0f * s, false)) {
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        openCoop();
    }
    if (ui.pill(12, {cx + 505.0f * s, y}, {330.0f * s, 104.0f * s}, "SETTINGS", 30.0f * s, false)) openSettings(Screen::Title);
    // The ship under PLAY, idling, turning toward the last touch.
    {
        const Vec2 c{cx, 930.0f * s};
        const float target = std::atan2(uiPtr_.pos.y - c.y, uiPtr_.pos.x - c.x);
        titleShipAngle_ += wrapAngle(target - titleShipAngle_) * 0.08f;
        const float a = titleShipAngle_;
        const Vec2 nose = c + polarV(a, 26.0f * s), l = c + polarV(a + 2.5f, 22.0f * s), r = c + polarV(a - 2.5f, 22.0f * s),
                   tail = c + polarV(a + kTwoPi * 0.5f, 8.0f * s);
        const Vec3 pc = ui.col(palettes_->player[0], 1.8f), lc = ui.col(palettes_->playerLight[0], 1.6f);
        ui.draw().hud.line(nose, l, 1.2f * s, pc, pc, 0);
        ui.draw().hud.line(l, tail, 1.2f * s, pc, pc, 0);
        ui.draw().hud.line(tail, r, 1.2f * s, lc, lc, 0);
        ui.draw().hud.line(r, nose, 1.2f * s, lc, lc, 0);
        ui.ring(c, 40.0f * s, 0.6f * s, palettes_->player[0], 0.7f);
    }
    // Best, bottom left in gold; the note, bottom right.
    {
        char line[96], num[32];
        if (scores_.count) {
            groupDigits(num, sizeof num, scores_.entry[0].score);
            char chain[32];
            groupDigits(chain, sizeof chain, scores_.entry[0].bestChain);
            std::snprintf(line, sizeof line, "BEST %s   TRACK %02u   X%s", num, scores_.entry[0].track, chain);
        } else {
            std::snprintf(line, sizeof line, "BEST -");
        }
        const float m = std::fmax(132.0f * s, ui.insetMin().x);
        ui.text({m, H - 54.0f * s}, 14.0f * s, palettes_->player[3], 1.4f, line, 0.0f);
        ui.text({W - std::fmax(132.0f * s, ui.insetMax().x), H - 54.0f * s}, 14.0f * s, white, 0.5f, "HEADPHONES RECOMMENDED", 1.0f);
    }
    // M4 stage B: why a co-op session ended (the host left, the connection was lost), for a few seconds.
    coopMessageT_ += frameDt_;
    if (coopMessage_[0] && coopMessageT_ < 10.0f) ui.text({cx, 870.0f * s}, 18.0f * s, palettes_->danger, 1.5f, coopMessage_);
    keyPrimary_ = keyBack_ = false;
}

void Play::screenPause(Ui& ui) {
    const float s = ui.s(), W = ui.W(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    ui.tube({cx, 300.0f * s}, 96.0f * s, white, palette_->accent, palette_->hue[2], 1.5f, "PAUSED");
    // The run line, in gold.
    {
        const PlayerState& p = world_->player(0);
        char num[32], line[128];
        groupDigits(num, sizeof num, p.score);
        const u32 secs = static_cast<u32>(runSeconds_);
        char chain[32];
        groupDigits(chain, sizeof chain, p.chain);
        std::snprintf(line, sizeof line, "%s   TRACK %u   %u:%02u   X%s", num, world_->phrase().track, secs / 60u, secs % 60u, chain);
        ui.text({cx, 380.0f * s}, 20.0f * s, gold, 1.4f, line);
    }
    if (countIn_) {
        // The count-in: four numbered rings fill on four beats; the run goes on at the end of the fourth.
        const double now = countInNow();
        const float u = static_cast<float>(1.0 - (countInEnd_ - now) / (4.0 * kTicksPerBeat));
        for (u32 k = 0; k < 4; ++k) {
            const Vec2 c{cx + (static_cast<float>(k) - 1.5f) * 150.0f * s, 600.0f * s};
            const float fill = clamp01(u * 4.0f - static_cast<float>(k));
            ui.ring(c, 52.0f * s, (fill >= 1.0f ? 1.6f : 0.9f) * s, palettes_->player[0], fill >= 1.0f ? 2.0f : 0.6f, 48);
            const u32 segs = static_cast<u32>(48.0f * fill);
            for (u32 j = 0; j < segs; ++j) {
                const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(j) / 48.0f, a1 = a0 + kTwoPi / 48.0f;
                const Vec3 c2 = ui.col(palettes_->player[0], 1.8f);
                ui.draw().hud.line(c + polarV(a0, 52.0f * s), c + polarV(a1, 52.0f * s), 3.0f * s, c2, c2, 0);
            }
            char n[4];
            std::snprintf(n, sizeof n, "%u", 4u - k);
            ui.text({c.x, c.y + 18.0f * s}, 36.0f * s, white, fill > 0.0f ? 1.8f : 0.6f, n);
        }
        keyPrimary_ = keyBack_ = false;
        return;
    }
    if (askQuit_ || askRestart_) {
        ui.text({cx, 560.0f * s}, 24.0f * s, white, 1.3f, askQuit_ ? "QUIT THIS RUN?" : "RESTART THIS RUN?");
        if (ui.pill(30, {cx - 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "YES", 30.0f * s, false)) {
            const bool quit = askQuit_;
            askQuit_ = askRestart_ = false;
            if (quit) {
                endRun(false);
                toTitle();
            } else {
                endRun(false);
                screen_ = Screen::Run;
                restartRun();
            }
        }
        if (ui.pill(31, {cx + 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "NO", 30.0f * s, true) || keyBack_) askQuit_ = askRestart_ = false;
        keyPrimary_ = keyBack_ = false;
        return;
    }
    if (ui.pill(20, {cx, 600.0f * s}, {600.0f * s, 130.0f * s}, "RESUME", 52.0f * s, true) || keyPrimary_ || keyBack_) beginCountIn();
    if (ui.pill(21, {cx - 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "RESTART", 30.0f * s, false)) askRestart_ = true;
    if (ui.pill(22, {cx + 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "SETTINGS", 30.0f * s, false)) openSettings(Screen::Paused);
    if (ui.pill(23, {cx, 920.0f * s}, {280.0f * s, 80.0f * s}, "QUIT RUN", 24.0f * s, false, true, false, Vec3{0.6f, 0.6f, 0.6f})) askQuit_ = true;
    keyPrimary_ = keyBack_ = false;
}

void Play::screenGameOver(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3], amber = palettes_->playerDeep[3];
    ui.text({cx, 150.0f * s}, 44.0f * s, palette_->accent, 1.6f, "RUN OVER");
    // The score counts up over one bar.
    {
        const float bars = static_cast<float>(overS_ * (clock_.bpm() / 60.0f) / 4.0f);
        const float u = clamp01(bars);
        const u64 shown = static_cast<u64>(static_cast<double>(finalScore_) * (1.0 - (1.0 - u) * (1.0 - u)));
        char num[32];
        groupDigits(num, sizeof num, shown);
        const float cap = 150.0f * s;
        const float w = ui.measure(cap, num);
        ui.font().drawField(ui.draw().hud, {cx - 0.5f * w, 360.0f * s - cap}, cap, 4.5f * s, ui.col(white, 1.6f), ui.col(gold, 1.6f), ui.col(amber, 1.6f), num);
        if (newBest_ && u >= 1.0f) {
            const Vec2 f{cx + 0.5f * w + 40.0f * s, 230.0f * s};
            for (u32 a = 0; a < 4; ++a) {
                const Vec2 dv = polarV(kTwoPi * static_cast<float>(a) / 8.0f, 60.0f * s);
                const Vec3 c = ui.col(gold, 1.8f);
                ui.draw().hud.line(f - dv, f + dv, 0.9f * s, Vec3{}, c, 0);
                ui.draw().hud.line(f, f + dv, 0.9f * s, c, Vec3{}, 0);
            }
            ui.text({cx, 410.0f * s}, 22.0f * s, gold, 1.8f, "NEW BEST");
        }
    }
    // Four numbers: TIME, TRACK, BEST CHAIN, KILLS.
    {
        const PlayerState& p = world_->player(0);
        char v[4][24];
        const u32 secs = static_cast<u32>(runSeconds_);
        std::snprintf(v[0], sizeof v[0], "%u:%02u", secs / 60u, secs % 60u);
        std::snprintf(v[1], sizeof v[1], "%u", runTrackMax_);
        {
            char chain[32];
            groupDigits(chain, sizeof chain, p.bestChain);
            std::snprintf(v[2], sizeof v[2], "X%s", chain);
        }
        std::snprintf(v[3], sizeof v[3], "%u", p.kills);
        static const char* const kLabels[4] = {"TIME", "TRACK", "BEST CHAIN", "KILLS"};
        for (u32 k = 0; k < 4; ++k) {
            const float x = cx + (static_cast<float>(k) - 1.5f) * 330.0f * s;
            ui.text({x, 500.0f * s}, 14.0f * s, palette_->accent, 1.3f, kLabels[k]);
            ui.text({x, 556.0f * s}, 40.0f * s, white, 1.5f, v[k]);
        }
    }
    drawSetList(ui, 660.0f * s);
    drawBand(ui, 740.0f * s);
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    if (ui.pill(40, {right - 300.0f * s, H - 240.0f * s}, {600.0f * s, 140.0f * s}, "RETRY", 52.0f * s, true) || keyPrimary_) {
        screen_ = Screen::Run;
        restartRun();
    }
    // (24 px under RETRY: screens.md, controls never touch)
    if (ui.pill(41, {right - 165.0f * s, H - 96.0f * s}, {330.0f * s, 90.0f * s}, "HOME", 28.0f * s, false) || keyBack_) toTitle();
    keyPrimary_ = keyBack_ = false;
}

void Play::drawSetList(Ui& ui, float y) {
    // screens.md "Game over": the set list, the run drawn as the song it was.
    const float s = ui.s(), W = ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    // The set list: the run as the song it was. One segment per Track, as long as the time spent in it,
    // in the Track's colour with its name; a white star where a boss fell, a gold tick per upgrade, a
    // danger cross per hit. (Its ten-second medley is not played.)
    {
        const float x0 = m, x1 = W - std::fmax(132.0f * s, ui.insetMax().x);
        const float total = std::fmax(runSeconds_, 1.0f);
        const auto xAt = [&](float t) { return x0 + (x1 - x0) * clamp01(t / total); };
        u32 segStart = 0;
        for (u32 k = 0; k <= markCount_; ++k) {
            const bool end = k == markCount_;
            if (!end && marks_[k].kind != 0) continue;
            if (k > segStart || end) {
                // From the previous Track start to this one.
                const float ta = segStart < markCount_ ? marks_[segStart].t : 0.0f;
                const float tb = end ? total : marks_[k].t;
                const u8 track = segStart < markCount_ ? marks_[segStart].a : 1;
                const Vec3 c = trackAccent(*palettes_, track, world_->setList());
                const float xa = xAt(ta), xb = xAt(tb);
                if (xb > xa + 2.0f) {
                    ui.draw().hud.line({xa + 2.0f * s, y}, {xb - 2.0f * s, y}, 5.0f * s, ui.col(c, 1.3f), ui.col(c, 1.3f), 0);
                    char name[40];
                    std::snprintf(name, sizeof name, "%u %s", track, sim::songName(sim::songOfTrack(track, world_->setList())));
                    if (xb - xa > 200.0f * s) ui.text({xa + 4.0f * s, y + 34.0f * s}, 12.0f * s, c, 1.2f, name, 0.0f);
                }
            }
            segStart = k;
        }
        for (u32 k = 0; k < markCount_; ++k) {
            const Mark& mk = marks_[k];
            const float x = xAt(mk.t);
            if (mk.kind == 1) {
                for (u32 a = 0; a < 4; ++a) {
                    const Vec2 dv = polarV(kTwoPi * static_cast<float>(a) / 8.0f, 14.0f * s);
                    ui.draw().hud.line(Vec2{x, y - 22.0f * s} - dv, Vec2{x, y - 22.0f * s} + dv, 0.9f * s, ui.col(white, 2.0f), ui.col(white, 2.0f), 0);
                }
            } else if (mk.kind == 2) {
                ui.draw().hud.line({x, y - 14.0f * s}, {x, y - 4.0f * s}, 1.1f * s, ui.col(gold, 1.8f), ui.col(gold, 1.8f), 0);
            } else if (mk.kind == 3) {
                // M4 stage B (screens.md): in co-op each player's crosses are in their colour.
                const Vec3 c = coop() ? ui.col(palettes_->player[mk.a < kMaxPlayers ? mk.a : 0], 1.8f) : ui.col(palettes_->danger, 1.6f);
                ui.draw().hud.line({x - 5.0f * s, y + 6.0f * s}, {x + 5.0f * s, y + 16.0f * s}, 0.9f * s, c, c, 0);
                ui.draw().hud.line({x + 5.0f * s, y + 6.0f * s}, {x - 5.0f * s, y + 16.0f * s}, 0.9f * s, c, c, 0);
            }
        }
    }
}

void Play::drawBand(Ui& ui, float y) {
    // screens.md "Game over": your band, under the set list.
    const float s = ui.s();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    // Your band: the Riffs and the most-stacked Mods the run ended with.
    {
        const PlayerState& p = world_->player(coop() ? seat_ : 0);
        u8 list[8];
        u32 n = 0;
        for (u8 u = kOrbitals; u < kUpgradeCount && n < 8; ++u)
            if (p.loadout.level(u)) list[n++] = u;
        for (u32 pass = 5; pass >= 1 && n < 7; --pass)
            for (u8 u = 0; u < kModCount && n < 7; ++u)
                if (p.loadout.mods[u] == pass) list[n++] = u;
        const UiColour ctx{&ui};
        for (u32 k = 0; k < n; ++k) {
            const Vec2 c{m + 40.0f * s + static_cast<float>(k) * 90.0f * s, y + 60.0f * s};
            icons_.drawHud(ui.draw().hud, upgradeIcons_[list[k]], c, 30.0f * s, 1.0f, uiColourFn, &ctx);
        }
        if (n) ui.text({m, y}, 12.0f * s, white, 0.8f, "YOUR BAND", 0.0f);
    }
}

void Play::screenSettings(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    ui.text({cx, 100.0f * s}, 44.0f * s, palette_->accent, 1.6f, "SETTINGS");
    static const char* const kTabs[3] = {"SOUND", "CONTROLS", "LOOK"};
    for (u32 k = 0; k < 3; ++k)
        if (ui.pill(50 + k, {cx + (static_cast<float>(k) - 1.0f) * 420.0f * s, 154.0f * s + 42.0f * s}, {360.0f * s, 84.0f * s}, kTabs[k],
                    30.0f * s, false, true, settingsTab_ == k, palettes_->player[0]))
            settingsTab_ = static_cast<u8>(k);
    bool changed = false;
    if (settingsTab_ == 0) {
        ui.text({cx, 276.0f * s}, 13.0f * s, white, 0.7f, "THE SAME MUSIC, THREE WAYS. TAP ONE: IT CHANGES ON THE NEXT BAR");
        // Sound cards: Club (amber -> magenta), Arcade (cyan -> lime), Velvet (violet -> azure), each with its
        // signature line drawn from the live master bus, shaped per card.
        static const char* const kNames[3] = {"CLUB", "ARCADE", "VELVET"};
        static const char* const kWords[3] = {"WARM  ROUND  DEEP", "CRISP  BRIGHT  DRY", "SOFT  WIDE  LONG"};
        const Vec3 c0[3] = {{0.98f, 0.49f, 0.07f}, {0.02f, 0.84f, 0.91f}, {0.37f, 0.16f, 0.96f}};
        const Vec3 c1[3] = {{0.96f, 0.02f, 0.97f}, {0.37f, 0.90f, 0.02f}, {0.06f, 0.41f, 0.97f}};
        const u32 n = band_ && band_->active() ? band_->scope(scope_, kScopeN) : 0;
        const u32 playing = band_ ? band_->variant() : settings_.variant;
        for (u32 k = 0; k < 3; ++k) {
            const float cw = 620.0f * s, ch = 300.0f * s;
            const float x0 = cx + (static_cast<float>(k) - 1.5f) * cw + (static_cast<float>(k) - 1.0f) * 48.0f * s;
            const Vec2 a{x0, 300.0f * s}, b{x0 + cw, 300.0f * s + ch};
            bool pressed = false;
            if (ui.area(60 + k, a, b, pressed)) {
                settings_.variant = static_cast<u8>(k);
                if (band_) band_->setVariant(k);
                changed = true;
            }
            const bool isPlaying = playing == k;
            const bool waiting = settings_.variant == k && !isPlaying;
            ui.roundRect(a, b, 40.0f * s, (isPlaying ? 1.7f : 1.0f) * s, c0[k], isPlaying ? 2.2f : (waiting ? 1.5f : 0.8f));
            if (isPlaying) {
                const Vec3 g = ui.col(c0[k], 0.035f);  // glass (screens.md 0.085, before the HUD's own tone map)
                for (float y = a.y + 30.0f * s; y < b.y - 20.0f * s; y += 6.0f * s) ui.draw().hud.line({a.x + 30.0f * s, y}, {b.x - 30.0f * s, y}, 3.0f * s, g, g, 0);
                ui.draw().pools.glow((a + b) * 0.5f, 0.4f * cw, c0[k] * 0.06f);
            }
            ui.tube({a.x + 0.5f * cw, a.y + 86.0f * s}, 46.0f * s, c0[k], white, c1[k], 1.4f, kNames[k]);
            ui.text({a.x + 0.5f * cw, a.y + 126.0f * s}, 13.0f * s, white, 0.9f, kWords[k]);
            // The signature line.
            const float ly = a.y + 205.0f * s;
            Vec2 prev{};
            for (u32 j = 0; j <= 80; ++j) {
                const float u = static_cast<float>(j) / 80.0f;
                float v = 0.0f;
                if (n) {
                    const u32 i = static_cast<u32>(u * static_cast<float>(n - 1));
                    v = scope_[i];
                }
                if (k == 0) v = std::fabs(v) * 1.6f;                    // round, a hump
                if (k == 1) v = std::round(v * 6.0f) / 6.0f * 1.8f;      // stepped, bright
                if (k == 2) v = v * 0.9f;                                // wide, soft
                const Vec2 p{a.x + 40.0f * s + (cw - 80.0f * s) * u, ly - std::fmax(-1.0f, std::fmin(1.0f, v * 2.0f)) * 46.0f * s};
                if (j) {
                    const Vec3 c = ui.col(c0[k] + (c1[k] - c0[k]) * u, isPlaying ? 1.6f : 0.6f);
                    ui.draw().hud.line(prev, p, (k == 0 ? 2.0f : (k == 1 ? 0.8f : 1.4f)) * s, c, c, 0);
                }
                prev = p;
            }
            ui.text({a.x + 0.5f * cw, b.y - 22.0f * s}, 13.0f * s, isPlaying ? c0[k] : white, isPlaying ? 1.5f : (waiting ? 1.2f : 0.5f),
                    isPlaying ? "PLAYING" : (waiting ? "NEXT BAR" : ""));
        }
        // Faders: MASTER, MUSIC, EFFECTS, with the live level as a meter.
        const float meter = band_ ? clamp01(band_->meter()) : 0.0f;
        const float fw = 460.0f * s;
        const float fy = 690.0f * s;  // (screens.md: the faders at y 690)
        float* v[3] = {&settings_.master, &settings_.music, &settings_.effects};
        static const char* const kF[3] = {"MASTER", "MUSIC", "EFFECTS"};
        for (u32 k = 0; k < 3; ++k) {
            const float x = cx + (static_cast<float>(k) - 1.5f) * 620.0f * s + (static_cast<float>(k) - 1.0f) * 48.0f * s + 60.0f * s;
            const float nv = ui.fader(70 + k, {x, fy}, fw, *v[k], meter * (*v[k]) * (k == 0 ? 1.0f : settings_.master), kF[k]);
            if (nv != *v[k]) {
                *v[k] = nv;
                changed = true;
            }
        }
        // SYNC (bottom left, the offset under it in gold), HAPTICS (bottom centre), DONE (bottom right).
        const float m = std::fmax(132.0f * s, ui.insetMin().x);
        if (ui.pill(80, {m + 180.0f * s, H - 140.0f * s}, {360.0f * s, 100.0f * s}, "SYNC", 30.0f * s, false)) syncRequest_ = true;
        char off[32];
        std::snprintf(off, sizeof off, band_ && band_->syncOffsetTouched() ? "%+.0f MS" : "AUTOMATIC", band_ ? band_->syncOffsetMs() : 0.0f);
        ui.text({m + 180.0f * s, H - 50.0f * s}, 14.0f * s, palettes_->player[3], 1.3f, off);
        // M4 (GAME_DESIGN 12): in co-op the music can come from the host's phone alone (a client keeps its own lead).
        {
            static const char* const kOnOff[2] = {"OFF", "ON"};
            const float hx = cx + 700.0f * s;
            ui.text({hx, 772.0f * s}, 13.0f * s, white, 0.8f, "CO-OP: MUSIC FROM THE HOST'S PHONE ONLY");
            const u32 ho = ui.segmented(12, {hx, 824.0f * s}, 320.0f * s, 56.0f * s, kOnOff, 2, settings_.musicHostOnly ? 1u : 0u, 20.0f * s);
            if ((ho != 0) != settings_.musicHostOnly) {
                settings_.musicHostOnly = ho != 0;
                changed = true;
            }
        }
        static const char* const kH[3] = {"OFF", "SOFT", "STRONG"};
        ui.text({cx, H - 200.0f * s}, 13.0f * s, white, 0.8f, "HAPTICS");
        const u32 h = ui.segmented(9, {cx, H - 140.0f * s}, 560.0f * s, 70.0f * s, kH, 3, settings_.haptics, 22.0f * s);
        if (h != settings_.haptics) {
            settings_.haptics = static_cast<u8>(h);
            changed = true;
            if (band_ && h) {
                band_->playSfx(audio::Sfx::UiTick, h == 2 ? 1.0f : 0.6f);
                band_->setHaptics(true);
                band_->setHapticStrong(h == 2);
                band_->hapticPreview();  // the new strength, felt at once
            }
        }
    } else if (settingsTab_ == 1) {
        static const char* const kOnOff[2] = {"OFF", "ON"};
        ui.text({cx, 320.0f * s}, 18.0f * s, white, 1.1f, "SWAP SIDES: AIM WITH THE LEFT THUMB");
        const u32 sw = ui.segmented(5, {cx, 390.0f * s}, 420.0f * s, 80.0f * s, kOnOff, 2, settings_.swapSides ? 1u : 0u, 26.0f * s);
        ui.text({cx, 500.0f * s}, 18.0f * s, white, 1.1f, "AUTO-AIM ASSIST: FIRES AT THE NEAREST THREAT, HALF RATE");
        const u32 aa = ui.segmented(6, {cx, 570.0f * s}, 420.0f * s, 80.0f * s, kOnOff, 2, settings_.autoAim ? 1u : 0u, 26.0f * s);
        // Play test 1, second pass (screens.md CONTROLS): Auto Nova, on by default.
        ui.text({cx, 680.0f * s}, 18.0f * s, white, 1.1f, "AUTO NOVA  FIRES ONE BEFORE YOUR LAST CELL GOES");
        const u32 an = ui.segmented(11, {cx, 750.0f * s}, 420.0f * s, 80.0f * s, kOnOff, 2, settings_.autoNova ? 1u : 0u, 26.0f * s);
        if ((sw != 0) != settings_.swapSides || (aa != 0) != settings_.autoAim || (an != 0) != settings_.autoNova) {
            settings_.swapSides = sw != 0;
            settings_.autoAim = aa != 0;
            settings_.autoNova = an != 0;
            changed = true;
        }
    } else {
        static const char* const kFx[5] = {"AUTO", "LOW", "MEDIUM", "HIGH", "ULTRA"};
        static const char* const kHz[4] = {"AUTO", "60", "90", "120"};
        static const char* const kOnOff[2] = {"OFF", "ON"};
        ui.text({cx, 320.0f * s}, 18.0f * s, white, 1.1f, "EFFECTS");
        const u32 fx = ui.segmented(7, {cx, 390.0f * s}, 1100.0f * s, 76.0f * s, kFx, 5, settings_.effectsTier, 24.0f * s);
        ui.text({cx, 500.0f * s}, 18.0f * s, white, 1.1f, "FRAME-RATE TARGET (FROM THE NEXT START)");
        const u32 hz = ui.segmented(8, {cx, 570.0f * s}, 880.0f * s, 76.0f * s, kHz, 4, settings_.frameRate, 24.0f * s);
        ui.text({cx, 680.0f * s}, 18.0f * s, white, 1.1f, "REDUCE FLASHING");
        const u32 rfl = ui.segmented(4, {cx, 750.0f * s}, 420.0f * s, 76.0f * s, kOnOff, 2, settings_.reduceFlashing ? 1u : 0u, 24.0f * s);
        if (fx != settings_.effectsTier) {
            settings_.effectsTier = static_cast<u8>(fx);
            tierRequest_ = static_cast<i32>(fx);
            changed = true;
        }
        if (hz != settings_.frameRate || (rfl != 0) != settings_.reduceFlashing) {
            settings_.frameRate = static_cast<u8>(hz);
            settings_.reduceFlashing = rfl != 0;
            changed = true;
        }
    }
    if (changed) applySettings();
    // DONE saves and returns; Back does the same.
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    if (ui.pill(90, {right - 230.0f * s, H - 140.0f * s}, {460.0f * s, 124.0f * s}, "DONE", 44.0f * s, true) || keyBack_ || keyPrimary_) {
        saveSettings(settings_);
        screen_ = settingsBack_;
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

}  // namespace rf
