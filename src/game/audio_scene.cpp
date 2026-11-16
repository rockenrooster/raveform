#include "game/audio_scene.h"

#include <SDL3/SDL_keycode.h>

#include <cmath>
#include <cstdio>

#include "game/play_hud.h"  // (layoutScale)

namespace rf {

using namespace audio;

namespace {

constexpr float kSliderMinMs = -150.0f, kSliderMaxMs = 350.0f;  // design/screens.md
constexpr float kStepMs = 5.0f;
constexpr i64 kRepeatNs = 250000000;  // hold: four steps a second

const Vec3 kWhite{1.0f, 1.0f, 1.0f};

u32 toSrgb(Vec3 c) {
    const auto enc = [](float v) {
        v = v < 0.0f ? 0.0f : (v > 1.0f ? 1.0f : v);
        const float s = v <= 0.0031308f ? 12.92f * v : 1.055f * std::pow(v, 1.0f / 2.4f) - 0.055f;
        return static_cast<u8>(s * 255.0f + 0.5f);
    };
    return rgba(enc(c.x), enc(c.y), enc(c.z));
}

void disc(CapsuleBatch& b, Vec2 c, float radius, Vec3 colour) { b.line(c, c, radius, colour, colour, kDisc); }

void ring(CapsuleBatch& b, Vec2 c, float radius, float halfWidth, Vec3 colour, float intensity) {
    Stroke s;
    s.radius = halfWidth;
    s.color = toSrgb(colour);
    s.intensity = intensity;
    b.polygon(c, radius, 72, 0.0f, s);
}

// A pill: outline (two edges and two half circles) and, pressed, a flat glass fill.
void pill(FrameDraw& d, Vec2 c, Vec2 size, Vec3 colour, float outlineI, float halfWidth, float fill) {
    const float r = size.y * 0.5f, hx = size.x * 0.5f - r;
    const Vec3 col = colour * outlineI;
    d.scene.line({c.x - hx, c.y - r}, {c.x + hx, c.y - r}, halfWidth, col, col, kFlatStart | kFlatEnd);
    d.scene.line({c.x - hx, c.y + r}, {c.x + hx, c.y + r}, halfWidth, col, col, kFlatStart | kFlatEnd);
    constexpr u32 kSeg = 16;
    for (u32 side = 0; side < 2; ++side) {
        const float cx = side ? c.x + hx : c.x - hx;
        const float a0 = side ? -kPi * 0.5f : kPi * 0.5f;
        for (u32 i = 0; i < kSeg; ++i) {
            const float t0 = a0 + kPi * i / kSeg, t1 = a0 + kPi * (i + 1) / kSeg;
            d.scene.line({cx + r * std::cos(t0), c.y + r * std::sin(t0)}, {cx + r * std::cos(t1), c.y + r * std::sin(t1)}, halfWidth,
                         col, col, 0);
            if (fill > 0.0f) {
                const Vec3 g = colour * fill;
                d.glass.tri({cx, c.y}, g, {cx + r * std::cos(t0), c.y + r * std::sin(t0)}, g,
                            {cx + r * std::cos(t1), c.y + r * std::sin(t1)}, g);
            }
        }
    }
    if (fill > 0.0f) {
        const Vec3 g = colour * fill;
        d.glass.tri({c.x - hx, c.y - r}, g, {c.x + hx, c.y - r}, g, {c.x + hx, c.y + r}, g);
        d.glass.tri({c.x - hx, c.y - r}, g, {c.x + hx, c.y + r}, g, {c.x - hx, c.y + r}, g);
    }
}

}  // namespace

void AudioScene::init(Kind kind, Band& band, const VectorFont& font, const Palettes& palettes, const Palette& palette,
                      const Look& look) {
    kind_ = kind;
    startedInSync_ = kind == Kind::Sync;
    band_ = &band;
    font_ = &font;
    palettes_ = &palettes;
    palette_ = &palette;
    look_ = &look;
    for (Spark& s : sparks_) s.active = false;
    const char* labels[kButtonCount] = {"-", "+", "AUTO", "DONE", "FIRE", "SOUND", "SECTION", "TRACK", "-", "+",
                                        "SENTINEL", "PULSE", "ORBITALS", "MODE", "SYNC", "HOLD"};
    for (u32 i = 0; i < kButtonCount; ++i) buttons_[i].label = labels[i];
}

void AudioScene::layout(float w, float h, Vec2 insetMin, Vec2 insetMax) {
    w_ = w;
    h_ = h;
    s_ = layoutScale(w, h);  // (the review's fixes, H2, 2026-11-15: the height and the width)
    const float s = s_;
    const float cx = w * 0.5f;
    const Vec3 player = palettes_->player[0];
    for (Button& b : buttons_) {
        b.colour = kWhite;
        b.disc = false;
    }
    if (kind_ == Kind::Sync) {
        ringCentre_ = {cx, 430.0f * s};
        sliderX0_ = cx - 550.0f * s;
        sliderX1_ = cx + 550.0f * s;
        sliderY_ = 700.0f * s;
        buttons_[kMinus] = {{sliderX0_ - 110.0f * s, sliderY_}, {110.0f * s, 110.0f * s}, true, buttons_[kMinus].pressed,
                            buttons_[kMinus].touch, "-", kWhite};
        buttons_[kPlus] = {{sliderX1_ + 110.0f * s, sliderY_}, {110.0f * s, 110.0f * s}, true, buttons_[kPlus].pressed,
                           buttons_[kPlus].touch, "+", kWhite};
        const float margin = 132.0f * s + (insetMin.x > insetMax.x ? insetMin.x : insetMax.x);
        const float by = h - 120.0f * s - insetMax.y;
        buttons_[kAuto].centre = {margin + 170.0f * s, by};
        buttons_[kAuto].size = {340.0f * s, 104.0f * s};
        buttons_[kDone].centre = {w - margin - 220.0f * s, by};
        buttons_[kDone].size = {440.0f * s, 130.0f * s};
        buttons_[kDone].colour = player;
        return;
    }
    // audio_test: the ring on the left, the controls in two rows at the bottom, FIRE at the right.
    ringCentre_ = {w * 0.30f, h * 0.42f};
    const float rowY0 = h - 250.0f * s - insetMax.y, rowY1 = h - 110.0f * s - insetMax.y;
    const float left = 132.0f * s + insetMin.x;
    const float fireLeft = w - 132.0f * s - insetMax.x - 400.0f * s;
    const float gap = 24.0f * s, ph = 96.0f * s;
    float pw = (fireLeft - 40.0f * s - left - 5.0f * gap) / 6.0f;
    if (pw > 250.0f * s) pw = 250.0f * s;
    const u32 row0[] = {kSound, kSection, kHold, kTrack, kMode, kSync};
    for (u32 i = 0; i < countOf(row0); ++i) {
        buttons_[row0[i]].centre = {left + pw * 0.5f + i * (pw + gap), rowY0};
        buttons_[row0[i]].size = {pw, ph};
    }
    const u32 row1[] = {kIntensityDown, kIntensityUp, kRiff1, kRiff2, kRiff3};
    for (u32 i = 0; i < countOf(row1); ++i) {
        buttons_[row1[i]].centre = {left + pw * 0.5f + i * (pw + gap), rowY1};
        buttons_[row1[i]].size = {pw, ph};
    }
    // The obvious control: FIRE, large, where the right thumb rests, in the player colour.
    buttons_[kFire].centre = {w - 132.0f * s - insetMax.x - 200.0f * s, rowY0 + 70.0f * s};
    buttons_[kFire].size = {400.0f * s, 300.0f * s};
    buttons_[kFire].colour = player;
}

u32 AudioScene::hit(Vec2 p) const {
    const u32 first = kind_ == Kind::Sync ? kMinus : kFire;
    const u32 last = kind_ == Kind::Sync ? kDone : kHold;
    for (u32 i = first; i <= last; ++i) {
        const Button& b = buttons_[i];
        const Vec2 d = p - b.centre;
        if (b.disc) {
            if (length(d) <= b.size.x * 0.5f) return i;
        } else if (std::fabs(d.x) <= b.size.x * 0.5f && std::fabs(d.y) <= b.size.y * 0.5f) {
            return i;
        }
    }
    return kButtonCount;
}

void AudioScene::setOffsetFromX(float x) {
    float u = (x - sliderX0_) / (sliderX1_ - sliderX0_);
    u = u < 0.0f ? 0.0f : (u > 1.0f ? 1.0f : u);
    const float ms = kSliderMinMs + u * (kSliderMaxMs - kSliderMinMs);
    band_->setSyncOffsetMs(std::round(ms));
}

void AudioScene::enterSync() {
    kind_ = Kind::Sync;
    band_->setMode(Mode::Metronome);
    band_->playSfx(Sfx::UiConfirm);
}

void AudioScene::leaveSync() {
    band_->saveSync();
    band_->playSfx(Sfx::UiBack);
    if (startedInSync_) {
        quit_ = true;  // nothing to return to: the scenario was the screen itself
        return;
    }
    kind_ = Kind::AudioTest;
    band_->setMode(Mode::Conductor);
}

void AudioScene::press(u32 id, u64 touch) {
    Button& b = buttons_[id];
    b.pressed = true;
    b.touch = touch;
    switch (id) {
        case kMinus:
        case kPlus:
            repeatDir_ = id == kPlus ? 1 : -1;
            band_->setSyncOffsetMs(band_->syncOffsetMs() + repeatDir_ * kStepMs);
            repeatNs_ = audioNowNs() + kRepeatNs;
            break;
        case kAuto:
            band_->clearSyncOffset();
            band_->playSfx(Sfx::UiTick);
            break;
        case kDone: break;  // on release
        case kFire:
            fireByTouch_ = true;
            band_->setFire(true);
            break;
        case kSound: band_->nextVariant(); break;
        case kSection: band_->nextSection(); break;
        case kHold: band_->setHold(!band_->hold()); break;
        case kTrack: band_->nextTrack(); break;
        case kIntensityDown: band_->setIntensity(band_->intensity() - 0.1f); break;
        case kIntensityUp: band_->setIntensity(band_->intensity() + 0.1f); break;
        case kRiff1: band_->toggleRiff(Riff::Sentinel); break;
        case kRiff2: band_->toggleRiff(Riff::PulseRing); break;
        case kRiff3: band_->toggleRiff(Riff::Orbitals); break;
        case kMode: band_->setMode(band_->mode() == Mode::Conductor ? Mode::Staged : Mode::Conductor); break;
        case kSync: break;  // on release
        default: break;
    }
}

void AudioScene::release(u32 id) {
    Button& b = buttons_[id];
    if (!b.pressed) return;
    b.pressed = false;
    if (id == kMinus || id == kPlus) repeatDir_ = 0;
    if (id == kFire) {
        fireByTouch_ = false;
        if (!fireByKey_) band_->setFire(false);
    }
    if (id == kDone) leaveSync();
    if (id == kSync) enterSync();
}

bool AudioScene::key(int k, bool down) {
    if (kind_ == Kind::Sync) {
        if (!down) return k == SDLK_LEFT || k == SDLK_RIGHT;
        switch (k) {
            case SDLK_LEFT: band_->setSyncOffsetMs(band_->syncOffsetMs() - kStepMs); return true;
            case SDLK_RIGHT: band_->setSyncOffsetMs(band_->syncOffsetMs() + kStepMs); return true;
            case SDLK_A: band_->clearSyncOffset(); return true;
            case SDLK_RETURN:
            case SDLK_ESCAPE:
            case SDLK_AC_BACK: leaveSync(); return true;  // Back does what DONE does
            default: return false;
        }
    }
    if (k == SDLK_SPACE) {
        fireByKey_ = down;
        if (down || !fireByTouch_) band_->setFire(down);
        return true;
    }
    if (!down) return false;
    switch (k) {
        case SDLK_V: band_->nextVariant(); return true;
        case SDLK_N: band_->nextSection(); return true;
        case SDLK_H: band_->setHold(!band_->hold()); return true;
        case SDLK_X: band_->nextTrack(); return true;
        case SDLK_UP: band_->setIntensity(band_->intensity() + 0.1f); return true;
        case SDLK_DOWN: band_->setIntensity(band_->intensity() - 0.1f); return true;
        case SDLK_1: band_->toggleRiff(Riff::Sentinel); return true;
        case SDLK_2: band_->toggleRiff(Riff::PulseRing); return true;
        case SDLK_3: band_->toggleRiff(Riff::Orbitals); return true;
        case SDLK_M: band_->setMode(band_->mode() == Mode::Conductor ? Mode::Staged : Mode::Conductor); return true;
        case SDLK_C: enterSync(); return true;
        case SDLK_G: band_->setHaptics(!band_->haptics()); return true;
        default: return false;
    }
}

void AudioScene::touchDown(u64 id, Vec2 pos) {
    const u32 b = hit(pos);
    if (b < kButtonCount) {
        press(b, id);
        return;
    }
    if (kind_ == Kind::Sync) {
        if (std::fabs(pos.y - sliderY_) < 80.0f * s_ && pos.x > sliderX0_ - 40.0f * s_ && pos.x < sliderX1_ + 40.0f * s_) {
            dragging_ = true;
            dragTouch_ = id;
            setOffsetFromX(pos.x);
        }
        return;
    }
    // Anywhere else holds fire too (the mouse button on the desktop).
    fireTouch_ = id;
    fireByTouch_ = true;
    band_->setFire(true);
}

void AudioScene::touchMove(u64 id, Vec2 pos) {
    if (dragging_ && id == dragTouch_) setOffsetFromX(pos.x);
}

void AudioScene::touchUp(u64 id) {
    for (u32 i = 0; i < kButtonCount; ++i)
        if (buttons_[i].pressed && buttons_[i].touch == id) release(i);
    if (dragging_ && id == dragTouch_) dragging_ = false;
    if (fireByTouch_ && id == fireTouch_) {
        fireByTouch_ = false;
        fireTouch_ = 0;
        if (!fireByKey_ && !buttons_[kFire].pressed) band_->setFire(false);
    }
}

void AudioScene::drawText(CapsuleBatch& batch, Vec2 at, float cap, Vec3 colour, float intensity, const char* text, bool centred) const {
    TextStyle st;
    st.capHeight = cap * s_;
    st.color = toSrgb(colour);
    st.intensity = intensity;
    const float wdt = centred ? font_->measure(st, text) : 0.0f;
    font_->draw(batch, {at.x - wdt * 0.5f, at.y}, st, text);
}

void AudioScene::drawButton(FrameDraw& d, const Button& b, bool active) const {
    // design/screens.md: outline 2 px at 0.9, label at 1.1; pressed: glass at 0.45, outline 3 px at 2.0,
    // label at 1.8, a soft pool of its colour behind. Toggles that are on are drawn half-way.
    const bool p = b.pressed;
    const float outline = p ? 2.0f : (active ? 1.4f : 0.9f);
    const float half = (p ? 1.5f : 1.0f) * s_;
    const float label = p ? 1.8f : (active ? 1.5f : 1.1f);
    if (p) d.pools.glow(b.centre, b.size.y * 0.9f, b.colour * 0.35f);
    if (b.disc) {
        ring(d.scene, b.centre, b.size.x * 0.5f, half, b.colour, outline);
        if (p) disc(d.scene, b.centre, b.size.x * 0.5f, b.colour * 0.45f);
    } else {
        pill(d, b.centre, b.size, b.colour, outline, half, p ? 0.45f : (active ? 0.16f : 0.0f));
    }
    const float cap = b.size.y > 120.0f * s_ ? 40.0f : (b.disc ? 40.0f : 26.0f);
    drawText(d.scene, {b.centre.x, b.centre.y - cap * 0.5f * s_}, cap, b.colour, label, b.label);
}

void AudioScene::drawRing(FrameDraw& d, Vec2 c, float radius, bool) const {
    // design/screens.md: white at 0.5, width 2.0; on each heard beat a filled disc at 0.45 fading over
    // 120 ms, the outline jumping to 2.6, and a ring expanding to 260 px over 200 ms. Reduce-flashing
    // mode: the disc at 0.2 and no expanding ring.
    const BeatView& v = band_->view();
    const float beatS = static_cast<float>(60.0 / (v.bpm > 1.0 ? v.bpm : 120.0));
    const float age = v.running ? v.beatPhase * beatS : 10.0f;
    const float flash = age < 0.12f ? 1.0f - age / 0.12f : 0.0f;
    ring(d.scene, c, radius, 1.0f * s_, kWhite, 0.5f + (2.6f - 0.5f) * flash);
    if (flash > 0.0f) disc(d.scene, c, radius, kWhite * ((reduceFlashing_ ? 0.2f : 0.45f) * flash));
    if (age < 0.2f && !reduceFlashing_) {
        const float u = age / 0.2f;
        ring(d.scene, c, radius + (260.0f * s_ - radius) * u, 1.0f * s_, kWhite, 1.2f * (1.0f - u));
    }
}

void AudioScene::drawBeatDots(FrameDraw& d, Vec2 c) const {
    const BeatView& v = band_->view();
    for (u32 i = 0; i < 4; ++i) {
        const Vec2 p{c.x + (static_cast<float>(i) - 1.5f) * 36.0f * s_, c.y};
        const bool lit = v.running && v.beatInBar == i;
        const Vec3 col = i == 0 ? palette_->accent : kWhite;
        disc(d.scene, p, (lit ? 7.0f : 5.0f) * s_, col * (lit ? 2.0f : 0.35f));
    }
}

void AudioScene::renderSync(FrameDraw& d) {
    const float s = s_, cx = w_ * 0.5f;
    if (repeatDir_ != 0 && audioNowNs() >= repeatNs_) {
        band_->setSyncOffsetMs(band_->syncOffsetMs() + repeatDir_ * kStepMs);
        repeatNs_ += kRepeatNs;
    }
    const float ms = band_->syncOffsetMs();
    if (ms < kSliderMinMs) band_->setSyncOffsetMs(kSliderMinMs);
    if (ms > kSliderMaxMs) band_->setSyncOffsetMs(kSliderMaxMs);
    drawText(d.hud, {cx, 70.0f * s}, 34.0f, palette_->accent, 1.6f, "SYNC");
    drawText(d.hud, {cx, 150.0f * s}, 16.0f, kWhite, 1.2f,
             band_->bluetooth() ? "BLUETOOTH ADDS DELAY - SLIDE RIGHT UNTIL THEY MEET"
                                : "MAKE THE FLASH AND THE CLICK HAPPEN TOGETHER");
    drawRing(d, ringCentre_, 150.0f * s, true);
    drawBeatDots(d, {cx, ringCentre_.y + 150.0f * s + 60.0f * s});
    // Slider: -150 .. +350 ms, zero marked.
    const Vec3 track = kWhite * 0.6f;
    d.scene.line({sliderX0_, sliderY_}, {sliderX1_, sliderY_}, 1.0f * s, track, track, 0);
    const float zx = sliderX0_ + (0.0f - kSliderMinMs) / (kSliderMaxMs - kSliderMinMs) * (sliderX1_ - sliderX0_);
    d.scene.line({zx, sliderY_ - 18.0f * s}, {zx, sliderY_ + 18.0f * s}, 1.0f * s, kWhite * 0.9f, kWhite * 0.9f, 0);
    const float tx = sliderX0_ + (band_->syncOffsetMs() - kSliderMinMs) / (kSliderMaxMs - kSliderMinMs) * (sliderX1_ - sliderX0_);
    const Vec3 player = palettes_->player[0];
    if (dragging_) d.pools.glow({tx, sliderY_}, 60.0f * s, player * 0.4f);
    disc(d.scene, {tx, sliderY_}, 22.0f * s, player * (dragging_ ? 1.8f : 1.0f));
    char value[32];
    if (band_->syncOffsetTouched()) {
        std::snprintf(value, sizeof value, "%+.0f MS", band_->syncOffsetMs());
    } else {
        std::snprintf(value, sizeof value, "0 MS (AUTOMATIC)");
    }
    drawText(d.hud, {tx < sliderX0_ + 150.0f * s ? sliderX0_ + 150.0f * s : (tx > sliderX1_ - 150.0f * s ? sliderX1_ - 150.0f * s : tx),
                     sliderY_ + 46.0f * s},
             22.0f, kWhite, 1.2f, value);
    for (u32 i = kMinus; i <= kDone; ++i) drawButton(d, buttons_[i], false);
}

void AudioScene::renderTest(FrameDraw& d) {
    const BeatView& v = band_->view();
    const float s = s_;
    const Vec2 c = ringCentre_;
    const float now = static_cast<float>(v.songSeconds);
    const Vec3 accent = palette_->accent, player = palettes_->player[0];
    const Vec3 hue0 = palette_->hue[0], hue1 = palette_->hue[1], hue2 = palette_->hue[2];
    // Kick: the ring swells 8% and a pool breathes behind it (look.md's kick pump in miniature).
    const float kick = v.envelope(BeatKind::Kick, 0.12f);
    const float radius = 170.0f * s * (1.0f + 0.08f * kick);
    d.pools.glow(c, radius * 1.4f, hue0 * (0.15f + 0.6f * kick));
    drawRing(d, c, radius, false);
    drawBeatDots(d, {c.x, c.y + 230.0f * s});
    // Snare and clap: the border flashes.
    const float snare = std::fmax(v.envelope(BeatKind::Clap, 0.10f), v.envelope(BeatKind::Snare, 0.08f));
    if (snare > 0.01f) {
        const float m = 18.0f * s;
        const Vec3 col = accent * (2.0f * snare);
        d.scene.line({m, m}, {w_ - m, m}, 2.0f * s, col, col, 0);
        d.scene.line({m, h_ - m}, {w_ - m, h_ - m}, 2.0f * s, col, col, 0);
        d.scene.line({m, m}, {m, h_ - m}, 2.0f * s, col, col, 0);
        d.scene.line({w_ - m, m}, {w_ - m, h_ - m}, 2.0f * s, col, col, 0);
    }
    // Hats: sixteen ticks around the ring; the heard sixteenth's tick shines.
    const float hat = v.envelope(BeatKind::Hat, 0.05f) + v.envelope(BeatKind::Shaker, 0.04f) * 0.6f;
    const u32 sixteenth = static_cast<u32>(std::fmod(v.heardTick, 384.0) / 24.0) % 16;
    for (u32 i = 0; i < 16; ++i) {
        const float a = kTau * static_cast<float>(i) / 16.0f - kPi * 0.5f;
        const float r0 = radius + 26.0f * s, r1 = radius + (i == sixteenth ? 46.0f + 20.0f * hat : 36.0f) * s;
        const Vec3 col = (i == sixteenth ? hue1 * (0.8f + 2.0f * hat) : kWhite * 0.25f);
        d.scene.line(c + polar(r0, a), c + polar(r1, a), 1.2f * s, col, col, 0);
    }
    // Bar line: a radar sweep for the first beat of every bar.
    if (v.running && v.beatInBar == 0) {
        const float a = kTau * v.beatPhase - kPi * 0.5f;
        const Vec3 col = accent * (1.5f * (1.0f - v.beatPhase));
        d.scene.line(c, c + polar(radius - 8.0f * s, a), 1.5f * s, col * 0.2f, col, 0);
    }
    // Bass: a bar under the ring breathing with the bass envelope; its height follows the note.
    const float bass = v.envelope(BeatKind::Bass, 0.18f);
    const float bw = 360.0f * s * (0.3f + 0.7f * bass);
    const Vec2 bp{c.x, c.y + 300.0f * s};
    d.scene.line(bp - Vec2{bw * 0.5f, 0.0f}, bp + Vec2{bw * 0.5f, 0.0f}, (3.0f + 6.0f * bass) * s, hue2 * (0.3f + 1.5f * bass),
                 hue2 * (0.3f + 1.5f * bass), 0);
    // Lead notes heard this frame: sparks leaving the ring at an angle set by the pitch; lead shots (the
    // heard grid with fire held) leave from the right in the player colour.
    for (u32 i = 0; i < v.eventCount; ++i) {
        const BeatEvent& e = v.events[i];
        if (e.kind != BeatKind::Lead) continue;
        Spark& sp = sparks_[sparkHead_++ % countOf(sparks_)];
        const float a = -kPi * 0.5f + (static_cast<float>(e.a) - 72.0f) * 0.12f;
        sp = {c.x + std::cos(a) * radius, c.y + std::sin(a) * radius, std::cos(a) * 520.0f * s, std::sin(a) * 520.0f * s, now,
              hue1, true};
    }
    for (u32 i = 0; i < v.shotCount; ++i) {
        Spark& sp = sparks_[sparkHead_++ % countOf(sparks_)];
        const float age = v.shots[i].ageSeconds;
        sp = {c.x + radius, c.y, 900.0f * s, 0.0f, now - age, player, true};
    }
    for (Spark& sp : sparks_) {
        if (!sp.active) continue;
        const float age = now - sp.born;
        if (age < 0.0f || age > 0.6f) {
            sp.active = age < 0.0f;
            continue;
        }
        const Vec2 p{sp.x + sp.vx * age, sp.y + sp.vy * age};
        const Vec2 tail{p.x - sp.vx * 0.03f, p.y - sp.vy * 0.03f};
        const float k = 1.0f - age / 0.6f;
        d.scene.line(tail, p, 2.2f * s, sp.colour * (0.2f * k), sp.colour * (2.2f * k), 0);
    }
    // Section: its name flashes on change.
    static const char* const kSections[] = {"GROOVE", "BUILD", "DROP", "BREAK"};
    const float sec = v.envelope(BeatKind::Section, 0.6f);
    const u32 si = static_cast<u32>(v.section) < 4 ? static_cast<u32>(v.section) : 0u;
    drawText(d.hud, {c.x, c.y - 30.0f * s}, 48.0f, sec > 0.02f ? accent : kWhite, 0.8f + 2.5f * sec, kSections[si]);
    char line[96];
    std::snprintf(line, sizeof line, "BAR %u  %u / %u", v.bar + 1, v.sectionBar + 1, v.sectionBars ? v.sectionBars : 1);
    drawText(d.hud, {c.x, c.y + 40.0f * s}, 20.0f, kWhite, 0.9f, line);

    // The right side: what plays.
    const float x = w_ * 0.58f;
    float y = 120.0f * s;
    const auto row = [&](Vec3 col, float in, const char* text) {
        drawText(d.hud, {x, y}, 24.0f, col, in, text, false);
        y += 46.0f * s;
    };
    std::snprintf(line, sizeof line, "%s  %.0f BPM", band_->trackName(), v.bpm);
    for (char* p = line; *p; ++p) *p = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 32) : *p;
    row(accent, 1.4f, line);
    std::snprintf(line, sizeof line, "SOUND %s", band_->variantName());
    for (char* p = line; *p; ++p) *p = (*p >= 'a' && *p <= 'z') ? static_cast<char>(*p - 32) : *p;
    row(kWhite, 1.1f, line);
    std::snprintf(line, sizeof line, "%s  INTENSITY %.1f%s", band_->mode() == Mode::Staged ? "STAGED RUN" : "CONDUCTOR",
                  band_->intensity(), band_->hold() ? "  HOLD" : "");
    row(kWhite, 1.1f, line);
    std::snprintf(line, sizeof line, "FIRE %s  RIFFS %s%s%s", band_->fire() ? "HELD" : "-", band_->riff(Riff::Sentinel) ? "S" : "-",
                  band_->riff(Riff::PulseRing) ? "P" : "-", band_->riff(Riff::Orbitals) ? "O" : "-");
    row(band_->fire() ? player : kWhite, band_->fire() ? 1.6f : 1.1f, line);
    std::snprintf(line, sizeof line, "LATENCY %.0f MS %s  SYNC %+.0f MS", v.latencyMs, v.timestamped ? "(DEVICE)" : "(ESTIMATE)",
                  band_->syncOffsetMs());
    row(kWhite, 0.8f, line);
    // Heard-event lamps: one per kind, lit by its envelope.
    const struct {
        BeatKind kind;
        const char* name;
        float decay;
    } lamps[] = {{BeatKind::Kick, "KICK", 0.12f}, {BeatKind::Clap, "CLAP", 0.1f},  {BeatKind::Snare, "SNARE", 0.08f},
                 {BeatKind::Hat, "HAT", 0.05f},   {BeatKind::Bass, "BASS", 0.18f}, {BeatKind::Lead, "LEAD", 0.1f},
                 {BeatKind::Stab, "STAB", 0.1f},  {BeatKind::Pulse, "PULSE", 0.1f}, {BeatKind::Bar, "BAR", 0.2f}};
    y += 10.0f * s;
    for (u32 i = 0; i < countOf(lamps); ++i) {
        const float e = v.envelope(lamps[i].kind, lamps[i].decay);
        const Vec2 p{x + 20.0f * s + static_cast<float>(i % 5) * 150.0f * s, y + static_cast<float>(i / 5) * 70.0f * s};
        disc(d.scene, p, 14.0f * s, hue1 * (0.15f + 2.4f * e));
        drawText(d.hud, {p.x + 26.0f * s, p.y - 9.0f * s}, 16.0f, kWhite, 0.6f + e, lamps[i].name, false);
    }
    for (u32 i = kFire; i <= kHold; ++i) {
        bool on = false;
        if (i == kFire) on = band_->fire();
        if (i == kHold) on = band_->hold();
        if (i == kRiff1) on = band_->riff(Riff::Sentinel);
        if (i == kRiff2) on = band_->riff(Riff::PulseRing);
        if (i == kRiff3) on = band_->riff(Riff::Orbitals);
        drawButton(d, buttons_[i], on);
    }
    // Labels for the - and + of intensity.
    drawText(d.hud, {buttons_[kIntensityDown].centre.x + 137.0f * s, buttons_[kIntensityDown].centre.y - 75.0f * s}, 14.0f, kWhite,
             0.7f, "INTENSITY");
}

void AudioScene::render(FrameDraw& d, FrameParams& params, Background& bg, float w, float h, Vec2 insetMin, Vec2 insetMax) {
    layout(w, h, insetMin, insetMax);
    if (kind_ == Kind::Sync) {
        renderSync(d);
    } else {
        renderTest(d);
    }
    // design/screens.md: the palette's background gradient, no lattice.
    bg.edge = palette_->bg0 * look_->bgLevel;
    bg.centre = palette_->bg1 * look_->bgLevel;
    bg.cx = look_->bgCentre[0];
    bg.cy = look_->bgCentre[1];
    bg.radius = look_->bgRadius;
    bg.power = look_->bgPower;
    bg.cloudCount = 0;
    params.vignette = false;
    params.afterglow = false;
    params.accent = palette_->accent;
}

}  // namespace rf
