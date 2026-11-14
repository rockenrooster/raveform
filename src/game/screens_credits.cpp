// M7 (design/screens.md "Credits (M7)"): the credits and licences screen, the last row of SETTINGS. One screen in the title's
// style, scrolling by drag, stick or keys, BACK top left; headings at cap 22 in the accent, lines at cap 14 in white, each
// library's licence in full at cap 10. The libraries and their licence texts are the build's own (cmake/Credits.cmake reads
// them from the fetched sources into credits.inc; a linked library without an entry fails the configure).

#include <cmath>
#include <cstring>

#include "game/play.h"
#include "game/screens.h"

namespace rf {

namespace {
struct CreditEntry {
    const char* line;     // BUILT ON's line: the name and the licence
    const char* licence;  // the licence text as the library ships it
};
#include "credits.inc"
constexpr u32 kCreditCount = sizeof(kCredits) / sizeof(kCredits[0]);
}  // namespace

void Play::screenCredits(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 accent = palette_->accent;
    const float m = std::fmax(132.0f * s, std::fmax(ui.insetMin().x, ui.insetMax().x));
    const float top = 200.0f * s, bottom = H - 30.0f * s;  // the scrolling area (under BACK and the title)
    ui.text({cx, 100.0f * s}, 44.0f * s, accent, 1.6f, "CREDITS");

    // Scrolling: a drag (finger or mouse), the sticks, the arrow keys or W / S; held inside the content's height.
    const float dt = 1.0f / 60.0f;
    if (uiPtr_.down && uiPtr_.pos.y > top) {
        if (creditsDragY_ >= 0.0f) creditsScroll_ += creditsDragY_ - uiPtr_.pos.y;
        creditsDragY_ = uiPtr_.pos.y;
    } else {
        creditsDragY_ = -1.0f;
    }
    float stick = padMove_.y + padAim_.y;
    if (keys_[0] || keys_[4]) stick -= 1.0f;
    if (keys_[2] || keys_[6]) stick += 1.0f;
    if (std::fabs(stick) > 0.2f) creditsScroll_ += stick * 1400.0f * s * dt;

    // The content, laid out top to bottom each frame (only what is in the area is drawn).
    float y = top + 40.0f * s - creditsScroll_;
    const auto visible = [&](float base, float cap) { return base > top + cap && base < bottom; };
    const auto heading = [&](const char* t) {
        y += 40.0f * s;
        if (visible(y, 22.0f * s)) ui.text({cx, y}, 22.0f * s, accent, 1.5f, t);
        y += 34.0f * s;
    };
    const auto line = [&](const char* t, Vec3 c, float i) {
        y += 24.0f * s;
        if (visible(y, 14.0f * s)) ui.text({cx, y}, 14.0f * s, c, i, t);
        y += 10.0f * s;
    };
    heading("RAVEFORM");
    line("MUSIC, PICTURES AND CODE MADE FOR THIS GAME", white, 1.2f);
    heading("BUILT ON");
    for (u32 k = 0; k < kCreditCount; ++k) line(kCredits[k].line, white, 1.2f);
    heading("LICENCES");
    // Each licence in full at cap 10, wrapped at the screen's width (the font's advance is its cap height).
    const float cap = 10.0f * s, lineH = 17.0f * s;
    const float advance = std::fmax(ui.measure(cap, "M"), 1.0f);
    const u32 maxChars = static_cast<u32>(std::fmax((W - 2.0f * m) / advance, 20.0f));
    const float left = m;
    const Vec3 lc = ui.col(white, 0.95f);
    const float hw = std::fmax(0.045f * cap, 0.55f * s);
    char buf[256];
    for (u32 k = 0; k < kCreditCount; ++k) {
        y += 18.0f * s;
        line(kCredits[k].line, accent, 1.4f);
        y += 6.0f * s;
        const char* p = kCredits[k].licence;
        while (*p) {
            // One source line, wrapped at a space where it is too long.
            const char* eol = std::strchr(p, '\n');
            const size_t len = eol ? static_cast<size_t>(eol - p) : std::strlen(p);
            size_t at = 0;
            do {
                size_t n = len - at;
                const size_t cap2 = maxChars < sizeof buf - 1 ? maxChars : sizeof buf - 1;
                if (n > cap2) {
                    n = cap2;
                    for (size_t j = cap2; j > cap2 / 2; --j)
                        if (p[at + j] == ' ') {
                            n = j;
                            break;
                        }
                }
                y += lineH;
                if (visible(y, cap)) {
                    size_t o = 0;
                    for (size_t j = 0; j < n; ++j) {
                        const char c = p[at + j];
                        buf[o++] = (c == '\t' || c == '\r') ? ' ' : c;
                    }
                    buf[o] = 0;
                    ui.font().drawField(ui.draw().hud, {left, y - cap}, cap, hw, lc, lc, lc, buf);
                }
                at += n;
                while (at < len && p[at] == ' ') ++at;
            } while (at < len);
            p = eol ? eol + 1 : p + len;
        }
    }
    heading("PLAY SAFE");
    for (const char* t : kNoticeLines) line(t, white, 1.2f);
    y += 60.0f * s;
    // Held inside the content: the end of PLAY SAFE comes no higher than the area's bottom.
    const float contentH = y + creditsScroll_ - top;
    const float maxScroll = std::fmax(contentH - (bottom - top), 0.0f);
    creditsScroll_ = std::fmax(0.0f, std::fmin(creditsScroll_, maxScroll));

    // BACK, top left (and Back / Esc), to SETTINGS.
    if (ui.pill(310, {m + 150.0f * s, 100.0f * s}, {300.0f * s, 90.0f * s}, "BACK", 24.0f * s, false) || keyBack_) {
        screen_ = Screen::Settings;
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

}  // namespace rf
