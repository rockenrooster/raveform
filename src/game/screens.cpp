// The screens of M3 (design/screens.md): the first-launch notice, the title, pause, game over and
// settings, as Play's (they share its run, band and settings), and the Ui helper they are drawn with.

#include "game/screens.h"

#include <cmath>
#include <cstdio>
#include <cstring>
#include <initializer_list>

#include "audio/band.h"
#include "content/icons.h"
#include "core/build_info.h"
#include "core/log.h"
#include "core/platform.h"
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
    // The font's origin is the top of the caps; `at` is the baseline.
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

bool Ui::button(u32 id, Vec2 a, Vec2 b, const char* label, bool& pressed, bool& over) {
    keep(a, b, true, label);
    const bool tapped = hit(id, a, b, pressed);
    over = !pressed && hover(a, b);
    return tapped;
}

bool Ui::hover(Vec2 a, Vec2 b) const {
#if RF_MOBILE
    (void)a;
    (void)b;
    return false;  // (a touch screen has no hover: the pointer stays where the finger lifted)
#else
    return !p_.down && inside(a, b);
#endif
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
    // design/title.md (2026-11-14, the folder's rule: clear, high-contrast hover and pressed states): with a mouse over it, a white
    // fill at 0.10, the outline 3 px at 1.6 and the label at 1.5.
    const bool over = enabled && !pressed && hover(a, b);
    const Vec3 tone = primary ? player_ : (colour.x + colour.y + colour.z > 0.0f ? colour : Vec3{1.0f, 1.0f, 1.0f});
    const float r = 0.5f * size.y;
    // screens.md: a control that cannot be used yet is drawn disabled (outline at 0.3, label at 0.4), unfilled.
    if (enabled && (pressed || selected || primary)) {
        // Glass: a capsule as tall as the pill (primary controls are always filled in their colour).
        const float level = pressed ? 0.45f : (primary ? (over ? 0.40f : 0.30f) : 0.20f);
        const Vec3 g = col(tone, level);
        d_->hud.line({a.x + r, c.y}, {b.x - r, c.y}, r, g, g, 0);
        d_->pools.glow(c, size.x * 0.55f, tone * ((pressed ? 0.25f : 0.10f) * hudFade()));  // (M6: the transitions' fade)
    } else if (over) {
        const Vec3 g = col(Vec3{1.0f, 1.0f, 1.0f}, 0.10f);
        d_->hud.line({a.x + r, c.y}, {b.x - r, c.y}, r, g, g, 0);
    }
    const float outline = !enabled ? 0.3f : (pressed ? 2.0f : (over ? 1.6f : (selected ? 1.6f : 0.9f)));
    roundRect(a, b, r, (pressed || over ? 1.5f : 1.0f) * s_, tone, outline);
    ++quiet_;
    text({c.x, c.y + 0.5f * cap}, cap, Vec3{1.0f, 1.0f, 1.0f}, !enabled ? 0.4f : (pressed ? 1.8f : (over ? 1.5f : 1.1f)), label);
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
    // (M4: the primary controls carry this device's player colour, P1's in solo.) M6 (screens.md "Transitions"): while a screen
    // is on its way out, or not yet in, it takes no taps (Back still cancels the drop-in's wait on the title).
    UiPointer ptr = uiPtr_;
    titleIdleFrame(ptr);  // screens.md "Title": the quiet title leaves; the input that brings it back is eaten here
    if (transInputLocked()) {
        ptr.pressed = ptr.released = ptr.down = false;
        keyPrimary_ = false;
        if (!(trans_ == Trans::Wait && transFrom_ == Screen::Title)) keyBack_ = false;
    }
    ui_.begin(d, *font_, *look_, variant, W, H, insetMin, insetMax, ptr, palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], palette_->accent);
    if (trans_ != Trans::None) ui_.setQuiet(true);  // (the layout rule is for screens at rest)
    static const char* const kNames[] = {"notice", "title", "run", "pause", "game over", "settings", "co-op join", "co-op lobby", "local network", "credits"};
    ui_.setScreenName(static_cast<u32>(screen_) < 10 ? kNames[static_cast<u32>(screen_)] : "screen");
    if (screen_ == Screen::Settings) ui_.setScreenName(settingsTab_ == 0 ? "settings: sound" : (settingsTab_ == 1 ? "settings: controls" : "settings: look"));
    switch (screen_) {
        case Screen::Notice:
            params.exposurePump *= 0.33f;
            screenNotice(ui_);
            break;
        case Screen::Title:
            // (screens.md "Title": as the quiet title leaves, the demo behind it comes up to its full light over the bar)
            params.exposurePump *= transExposure(0.33f + 0.67f * titleGone(0.0f, 4.0f));
            screenTitle(ui_);
            break;
        case Screen::Run:
            // M6 (screens.md "Transitions"): the drop-in's downbeat beat: the run comes to its full light and the screen it left
            // goes on its way (the logo out, the ring, PLAY into the halo, the ship to the centre).
            if (trans_ == Trans::Arrive) {
                params.exposurePump *= transExposure(1.0f);
                drawArrive(ui_);
            }
            break;
        case Screen::Paused:
            params.exposurePump *= 0.2f;
            if (coop()) {
                setHudFade(trans_ == Trans::Home ? 1.0f - gameOverOut() : 1.0f);  // (c9218dc: LEAVE RUN's home)
                screenPauseCoop(ui_);
                setHudFade(1.0f);
            } else {
                screenPause(ui_);
            }
            break;
        case Screen::GameOver:
            // screens.md: in co-op the run behind the game over is dimmed to a fifth, as in the pause.
            params.exposurePump *= transExposure(coop() ? 0.2f : 0.45f);
            if (coop()) {
                setHudFade(trans_ == Trans::Wait || trans_ == Trans::Home ? 1.0f - gameOverOut() : 1.0f);  // (M6: a co-op RETRY's wait, HOME)
                screenGameOverCoop(ui_);
                setHudFade(1.0f);
            } else if ((trans_ == Trans::Home || trans_ == Trans::Wait) && transFrom_ == Screen::Paused) {
                // (M6: QUIT RUN takes home's way, RESTART RETRY's drop-in (c9218dc): the pause screen fades out in its beat)
                const float out = gameOverOut();
                if (out < 1.0f) {
                    setHudFade(1.0f - out);
                    screenPause(ui_);
                    setHudFade(1.0f);
                }
            } else {
                screenGameOver(ui_);
            }
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
            // M6 (screens.md "Transitions"): the host's START landed: the lobby fades out over the drop-in's wait.
            if (trans_ == Trans::Wait) {
                // (c9218dc: its screen leaves as the title's controls do, over the wait's first beat)
                const float u = clamp01(transBeats(transTap_));
                setHudFade(1.0f - u * u * (3.0f - 2.0f * u));
            }
            screenCoopLobby(ui_);
            setHudFade(1.0f);
            break;
        case Screen::NetPermission:
            params.exposurePump *= 0.33f;
            screenNetPermission(ui_);
            break;
        case Screen::Credits:  // M7
            params.exposurePump *= 0.33f;
            screenCredits(ui_);
            break;
        default: break;
    }
    drawDropGather(ui_);  // (design/title.md §6: RETRY, RESTART and START shrink into the point in the wait's last half beat)
    ui_.end();
    uiPtr_.pressed = uiPtr_.released = false;
}

// GAME_DESIGN 11.4: the photosensitivity notice (the first launch, and the credits' PLAY SAFE).
const char* const kNoticeLines[3] = {"THIS GAME HAS FLASHING LIGHTS, FAST MOVEMENT AND PULSING COLOUR,",
                                     "TIMED TO THE MUSIC. IF YOU OR ANYONE WATCHING IS SENSITIVE TO LIGHT,",
                                     "TURN ON REDUCE FLASHING. YOU CAN CHANGE IT LATER IN SETTINGS, LOOK."};

void Play::screenNotice(Ui& ui) {
    // GAME_DESIGN 11.4: a first-launch notice, then the setting. Two choices.
    const float s = ui.s(), cx = 0.5f * ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    ui.tube({cx, 300.0f * s}, 64.0f * s, palette_->accent, white, palette_->hue[2], 1.6f, "BEFORE YOU PLAY");
    for (u32 k = 0; k < 3; ++k) ui.text({cx, (420.0f + 48.0f * static_cast<float>(k)) * s}, 20.0f * s, white, 1.15f, kNoticeLines[k]);
    const char* rf = settings_.reduceFlashing ? "REDUCE FLASHING: ON" : "REDUCE FLASHING: OFF";
    // (as wide as its longer label needs: at 520 the label ran past the pill's ends, seen on the Moto 2026-11-14; its right end
    // 140 px left of CONTINUE's)
    const float rfw = std::fmax(520.0f * s, ui.measure(30.0f * s, "REDUCE FLASHING: OFF") + 90.0f * s);
    if (ui.pill(1, {cx - 70.0f * s - 0.5f * rfw, 760.0f * s}, {rfw, 120.0f * s}, rf, 30.0f * s, false, true, settings_.reduceFlashing)) {
        settings_.reduceFlashing = !settings_.reduceFlashing;
        applySettings();
        if (band_) band_->playSfx(audio::Sfx::UiTick);
    }
    if (ui.pill(2, {cx + 330.0f * s, 760.0f * s}, {520.0f * s, 120.0f * s}, "CONTINUE", 44.0f * s, true) || keyPrimary_) {
        settings_.noticeSeen = true;
        saveSettings(settings_);
        screen_ = Screen::Title;
        if (!opt_.autoplay) introPending_ = true;  // (design/title.md §4: the intro follows the notice)
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    }
    keyPrimary_ = keyBack_ = false;
}

void Play::screenTitle(Ui& ui) {
    // Title v2 (design/title.md; game/title_v2.cpp): the fan, the word, the rule with its tagline and the motes, PLAY with its beat
    // dots, CO-OP and SETTINGS with their captions, the best, the note and the version. M6 (screens.md "Transitions"): each piece
    // at the level the drop-in's wait, home's building, the intro or the quiet title gives it.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const TitleLook tl = titleLook();
    drawTitle(ui, tl);
    const bool waiting = trans_ == Trans::Wait && transFrom_ == Screen::Title;
    if (tl.play > 0.0f) {
        setHudFade(tl.play);
        if ((titlePlay(ui, tl) || keyPrimary_) && tl.playTaps) dropInTap(Screen::Title);
    }
    const float sink = 30.0f * s * tl.sideSink;
    if (tl.side > 0.0f) {
        // CO-OP (M4 stage B: the join screen) and SETTINGS, each with its caption 28 px under it.
        setHudFade(tl.side);
        const float y = 800.0f * s + sink, capY = y + (52.0f + 28.0f + 12.0f) * s;
        if (ui.pill(11, {cx - 560.0f * s, y}, {340.0f * s, 104.0f * s}, "CO-OP", 30.0f * s, false) && tl.sideTaps) {
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
            openCoop();
        }
        if (ui.pill(12, {cx + 560.0f * s, y}, {340.0f * s, 104.0f * s}, "SETTINGS", 30.0f * s, false) && tl.sideTaps) openSettings(Screen::Title);
        ui.text({cx - 560.0f * s, capY}, 12.0f * s, white, 0.5f, "2-4 PLAYERS   ONE WI-FI");
        ui.text({cx + 560.0f * s, capY}, 12.0f * s, white, 0.5f, "SOUND   CONTROLS   LOOK");
    }
    // Best, bottom left in gold; the note, bottom right.
    if (tl.best > 0.0f) {
        setHudFade(tl.best);
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
        ui.text({m, H - 54.0f * s + sink}, 14.0f * s, palettes_->player[3], 1.4f, line, 0.0f);
        if (scores_.count && scores_.entry[0].setComplete) {
            // M6 (screens.md "A finished run"): a run that finished the set has a gold star in the table.
            const Vec2 c{m + ui.measure(14.0f * s, line) + 24.0f * s, H - 54.0f * s - 7.0f * s + sink};
            const Vec3 g = ui.col(palettes_->player[3], 2.0f);
            for (u32 a = 0; a < 4; ++a) {
                const Vec2 dv = polarV(kTwoPi * static_cast<float>(a) / 8.0f, 11.0f * s);
                ui.draw().hud.line(c - dv, c + dv, 0.9f * s, g, g, 0);
            }
        }
        ui.text({W - std::fmax(132.0f * s, ui.insetMax().x), H - 54.0f * s + sink}, 14.0f * s, white, 0.5f, "HEADPHONES RECOMMENDED", 1.0f);
        // screens.md "Title" (the owner, 2026-11-14: "include a version # with build date at the bottom right at the main menu"):
        // under the note, the version, the build's commit and its date and minute (src/core/build_info).
        ui.text({W - std::fmax(132.0f * s, ui.insetMax().x), H - 30.0f * s + sink}, 11.0f * s, white, 0.4f, buildStampLine(), 1.0f);
    }
    setHudFade(1.0f);
    // M4 stage B: why a co-op session ended (the host left, the connection was lost), for a few seconds (design/title.md: y 940).
    coopMessageT_ += frameDt_;
    if (coopMessage_[0] && coopMessageT_ < 10.0f && trans_ == Trans::None) ui.text({cx, 940.0f * s}, 18.0f * s, palettes_->danger, 1.5f, coopMessage_);
    // c9218dc (screens.md "Every other way in or out of a run"): a peer whose host has left came home; the line stands under
    // PLAY for two bars.
    if (titleNote_[0] && titleNoteAt_ >= 0.0) {
        const float b = static_cast<float>((transNow() - titleNoteAt_) / kTicksPerBeat);
        if (b >= 8.0f) {
            titleNote_[0] = 0;
        } else if (b >= 0.0f) {
            const float level = clamp01(b * 2.0f) * clamp01((8.0f - b) * 2.0f);
            ui.text({cx, 940.0f * s}, 18.0f * s, palettes_->danger, 1.5f * level, titleNote_);
        }
    }
    // Back in the drop-in's wait cancels it, until its last beat (design/title.md §6: from there the drop is committed).
    if (waiting && keyBack_ && transNow() < transLine_ - kTicksPerBeat) dropInCancel();
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
                dropInTap(Screen::Paused);  // c9218dc: the pause's RESTART is RETRY's drop-in
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

void Play::causeName(u16 cause, char* out, size_t n) const {
    // M6 (screens.md "What ended it"): as the game names it: an enemy (its body, its bullet, its line) by its family, a fight
    // by its name, an arena event by its name, a Volatile elite's burst as A VOLATILE BLAST.
    out[0] = 0;
    if (cause == kCauseNone || n < 2) return;
    if (cause < kCauseBossBase) {
        if (cause >= SpeciesTable::kCount) return;
        std::snprintf(out, n, "%s", familyName(world_->species().get(cause).family));
    } else if (cause < kCauseEventBase) {
        std::snprintf(out, n, "%s", bossName(static_cast<BossKind>(cause - kCauseBossBase)));
    } else if (cause < kCauseVolatile) {
        std::snprintf(out, n, "%s", eventName(static_cast<EventKind>(cause - kCauseEventBase)));
    } else {
        std::snprintf(out, n, "A VOLATILE BLAST");
    }
    for (char* c = out; *c; ++c)
        if (*c >= 'a' && *c <= 'z') *c = static_cast<char>(*c - 32);
}

void Play::screenGameOver(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3], amber = palettes_->playerDeep[3];
    // M6 (screens.md "Transitions"): home and RETRY's wait take the screen away in a beat: the score and RUN OVER streak out to
    // both sides; the four numbers, ENDED BY, the set list and the band fade and sink 30 px.
    const float out = gameOverOut();
    if (out >= 1.0f) {
        keyPrimary_ = keyBack_ = false;
        return;
    }
    const float sink = 30.0f * s * out * out;
    // A line drawn a letter at a time, each flying out to its side with two fading copies behind it (the Track name card's).
    const auto streak = [&](const char* text, float cap, float baseline, float halfWidth, Vec3 top, Vec3 mid, Vec3 bottom, float level) {
        const float w = ui.measure(cap, text);
        const u32 n = static_cast<u32>(std::strlen(text));
        char pre[48];
        for (u32 i = 0; i < n && i < 47; ++i) {
            if (text[i] == ' ') continue;
            std::memcpy(pre, text, i);
            pre[i] = 0;
            const float x = cx - 0.5f * w + (i ? ui.measure(cap, pre) : 0.0f);
            const float side = static_cast<float>(i) + 0.5f < 0.5f * static_cast<float>(n) ? -1.0f : 1.0f;
            const char one[2] = {text[i], 0};
            for (u32 g = 0; g < (out > 0.0f ? 3u : 1u); ++g) {
                const float dx = side * 0.75f * W * out * out * (1.0f - 0.3f * static_cast<float>(g));
                const float k = level * (1.0f - out) * (g == 0 ? 1.0f : 0.45f / static_cast<float>(g));
                if (k <= 0.01f) continue;
                ui.font().drawField(ui.draw().hud, {x + dx, baseline - cap}, cap, halfWidth, ui.col(top, k), ui.col(mid, k), ui.col(bottom, k), one);
            }
        }
    };
    if (out > 0.0f) ui.setQuiet(true);
    // M6 (screens.md "A finished run"): a run that finished the set reads SET COMPLETE in gold where RUN OVER stands.
    if (out > 0.0f) {
        const Vec3 c = runSetComplete_ ? gold : palette_->accent;
        streak(runSetComplete_ ? "SET COMPLETE" : "RUN OVER", 44.0f * s, 150.0f * s, std::fmax(0.045f * 44.0f * s, 0.55f * s), c, c, c,
               runSetComplete_ ? 1.8f : 1.6f);
    } else if (runSetComplete_) {
        ui.text({cx, 150.0f * s}, 44.0f * s, gold, 1.8f, "SET COMPLETE");
    } else {
        ui.text({cx, 150.0f * s}, 44.0f * s, palette_->accent, 1.6f, "RUN OVER");
    }
    // The score counts up over one bar.
    {
        const float bars = static_cast<float>(overS_ * (clock_.bpm() / 60.0f) / 4.0f);
        const float u = clamp01(bars);
        const u64 shown = static_cast<u64>(static_cast<double>(finalScore_) * (1.0 - (1.0 - u) * (1.0 - u)));
        char num[32];
        groupDigits(num, sizeof num, shown);
        const float cap = 150.0f * s;
        const float w = ui.measure(cap, num);
        if (out > 0.0f) streak(num, cap, 360.0f * s, 4.5f * s, white, gold, amber, 1.6f);
        else ui.font().drawField(ui.draw().hud, {cx - 0.5f * w, 360.0f * s - cap}, cap, 4.5f * s, ui.col(white, 1.6f), ui.col(gold, 1.6f), ui.col(amber, 1.6f), num);
        setHudFade(1.0f - out);
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
            ui.text({x, 500.0f * s + sink}, 14.0f * s, palette_->accent, 1.3f, kLabels[k]);
            ui.text({x, 556.0f * s + sink}, 40.0f * s, white, 1.5f, v[k]);
        }
    }
    // M6 (screens.md "What ended it"): ENDED BY in the accent and what took the last cell in white, as the game names it.
    if (const char* by = runEndedBy_[0] ? runEndedBy_ : nullptr) {
        const float cap = 14.0f * s;
        const float wa = ui.measure(cap, "ENDED BY"), gap = 14.0f * s, wb = ui.measure(cap, by);
        const float x0 = cx - 0.5f * (wa + gap + wb);
        ui.text({x0, 606.0f * s + sink}, cap, palette_->accent, 1.3f, "ENDED BY", 0.0f);
        ui.text({x0 + wa + gap, 606.0f * s + sink}, cap, white, 1.5f, by, 0.0f);
    }
    drawSetList(ui, 660.0f * s + sink);
    drawBand(ui, 740.0f * s + sink);
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    // M6 (screens.md "Transitions"): RETRY is the drop-in from game over; HOME takes the title's way back. The control tapped
    // keeps its pressed look until the screen has gone; HOME's tap greys RETRY.
    const bool homing = trans_ == Trans::Home, retrying = trans_ == Trans::Wait;
    if ((ui.pill(40, {right - 300.0f * s, H - 240.0f * s}, {600.0f * s, 140.0f * s}, "RETRY", 52.0f * s, true, !homing, retrying) || keyPrimary_) &&
        trans_ == Trans::None)
        dropInTap(Screen::GameOver);
    // (24 px under RETRY: screens.md, controls never touch)
    if ((ui.pill(41, {right - 165.0f * s, H - 96.0f * s}, {330.0f * s, 90.0f * s}, "HOME", 28.0f * s, false, true, homing) || keyBack_) &&
        trans_ == Trans::None)
        toTitle();
    setHudFade(1.0f);
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
        ui.text({cx, 500.0f * s}, 18.0f * s, white, 1.1f, "AUTO-AIM ASSIST: FIRES BY ITSELF, AT A BOSS'S WEAK POINT TOO");
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
        // M6 (screens.md "First-run hints"): RESET HINTS brings every hint back.
        if (ui.pill(14, {cx, 880.0f * s}, {420.0f * s, 90.0f * s}, "RESET HINTS", 26.0f * s, false)) {
            settings_.hintsSeen = 0;
            changed = true;
        }
        if (!settings_.hintsSeen) ui.text({cx, 950.0f * s}, 13.0f * s, white, 0.7f, "EVERY HINT WILL SHOW AGAIN");
    } else {
        static const char* const kFx[5] = {"AUTO", "LOW", "MEDIUM", "HIGH", "ULTRA"};
        static const char* const kHz[4] = {"AUTO", "60", "90", "120"};
        static const char* const kOnOff[2] = {"OFF", "ON"};
        ui.text({cx, 320.0f * s}, 18.0f * s, white, 1.1f, "EFFECTS");
        const u32 fx = ui.segmented(7, {cx, 390.0f * s}, 1100.0f * s, 76.0f * s, kFx, 5, settings_.effectsTier, 24.0f * s);
        ui.text({cx, 500.0f * s}, 18.0f * s, white, 1.1f, "FRAME-RATE TARGET (FROM THE NEXT START)");
        const u32 hz = ui.segmented(8, {cx, 570.0f * s}, 880.0f * s, 76.0f * s, kHz, 4, settings_.frameRate, 24.0f * s);
        // M6 (TECH_SPEC 5.2, 2026-11-14): REDUCE FLASHING and beside it COLOUR, VIVID only where the screen offers Display-P3.
        const float rx = cx - 330.0f * s, vx = cx + 330.0f * s;
        ui.text({rx, 680.0f * s}, 18.0f * s, white, 1.1f, "REDUCE FLASHING");
        const u32 rfl = ui.segmented(4, {rx, 750.0f * s}, 420.0f * s, 76.0f * s, kOnOff, 2, settings_.reduceFlashing ? 1u : 0u, 24.0f * s);
        if (wideColourOffered_) {
            static const char* const kColour[2] = {"STANDARD", "VIVID"};
            ui.text({vx, 680.0f * s}, 18.0f * s, white, 1.1f, "COLOUR (VIVID: THE SCREEN'S WIDE GAMUT)");
            const u32 vv = ui.segmented(17, {vx, 750.0f * s}, 520.0f * s, 76.0f * s, kColour, 2, settings_.vivid ? 1u : 0u, 24.0f * s);
            if ((vv != 0) != settings_.vivid) {
                settings_.vivid = vv != 0;
                changed = true;
            }
        } else {
            ui.text({vx, 680.0f * s}, 18.0f * s, white, 1.1f, "COLOUR");
            ui.text({vx, 760.0f * s}, 15.0f * s, white, 0.6f, "STANDARD (THIS SCREEN HAS NO WIDE GAMUT)");
        }
        if (fx != settings_.effectsTier) {
            settings_.effectsTier = static_cast<u8>(fx);
            tierRequest_ = static_cast<i32>(fx);
            changed = true;
        }
        // M6: the UI scale of HUD text and touch controls.
        static const char* const kScale[4] = {"90%", "100%", "115%", "130%"};
        ui.text({cx, 830.0f * s}, 18.0f * s, white, 1.1f, "UI SCALE: HUD TEXT AND TOUCH CONTROLS");
        const u32 sc = ui.segmented(15, {cx, 900.0f * s}, 880.0f * s, 76.0f * s, kScale, 4, settings_.uiScale < 4 ? settings_.uiScale : 1u, 24.0f * s);
        if (sc != settings_.uiScale) {
            settings_.uiScale = static_cast<u8>(sc);
            changed = true;
        }
        // M7 (screens.md "Credits (M7)"): CREDITS, the last row of SETTINGS.
        const float m = std::fmax(132.0f * s, ui.insetMin().x);
        if (ui.pill(16, {m + 180.0f * s, H - 140.0f * s}, {360.0f * s, 100.0f * s}, "CREDITS", 30.0f * s, false)) {
            creditsScroll_ = 0.0f;
            creditsDragY_ = -1.0f;
            screen_ = Screen::Credits;
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
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
