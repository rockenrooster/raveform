// M8 part 0 (design/modes/screens.md; design/modes/README.md 1-3, 9-10; docs/MODES_PLAN.md 8): the title's mode pill, MODES, the
// modes' emblems, and the transitions in and out of MODES (the rule's lift from the title, BACK's fold, the room's way in and out,
// the drop-in from PLAY). Every mode is on the screen from the first launch (nothing is locked); a mode not built yet has its card
// drawn, its PLAY disabled and the line COMING IN THIS BUILD'S NEXT PART, in dev builds only: a release build shows no card that
// does nothing.

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio/band.h"
#include "audio/cues.h"
#include "core/log.h"
#include "game/oklch.h"
#include "game/play.h"
#include "game/play_hud.h"
#include "game/screens.h"
#include "gfx/capsule.h"
#include "net/session.h"
#include "sim/director.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kTwoPi = 6.28318531f;
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
float smooth01(float x) {
    x = clamp01(x);
    return x * x * (3.0f - 2.0f * x);
}
float fractT(float x) { return x - std::floor(x); }
Vec2 polarV(float a, float r) { return Vec2{std::cos(a) * r, std::sin(a) * r}; }
void groupDigits(char* out, size_t cap, u64 v) {
    char raw[32];
    std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(v));
    const size_t n = std::strlen(raw);
    size_t o = 0;
    for (size_t i = 0; i < n && o + 2 < cap; ++i) {
        if (i && (n - i) % 3 == 0) out[o++] = ',';
        out[o++] = raw[i];
    }
    out[o] = 0;
}
// A release build (the GitHub releases' native Release) shows only the modes that are built (docs/MODES_PLAN.md part 0).
constexpr bool kReleaseBuild = RF_BUILD_NAME[0] == 'r';
constexpr bool kVersusBuilt = true;  // (part 1; before it a release build offered no TEAM / VERSUS)
// The title's eight notes (screens.md "Title"): A3 C4 E4 A4 C5 E5 G5 A5. MODES's letters are the first five, its cards the next
// three as they are thrown; going along the row plays the first seven, a card each (design/modes/screens.md "Transitions").
constexpr i32 kWordMidi[8] = {57, 60, 64, 69, 72, 76, 79, 81};
// The lift's notes (beats from its beat line): the five letters a sixteenth apart, the centre card's as it closes, the sides'.
struct LiftNote {
    float b;
    u8 note;
};
constexpr LiftNote kLiftNotes[8] = {{1.0f, 0}, {1.25f, 1}, {1.5f, 2}, {1.75f, 3}, {1.75f, 5}, {2.0f, 4}, {2.0f, 6}, {2.0f, 7}};
// screens.md "MODES" (reference px at 2400 x 1080).
constexpr float kRuleRestY = 150.0f, kTitleRuleY = 432.0f;
constexpr float kCardY = 440.0f, kCardStep = 700.0f, kCentreW = 720.0f, kCentreH = 440.0f, kSideW = 520.0f, kSideH = 340.0f;
constexpr float kCorner = 40.0f;
constexpr float kStrikeHotS = 0.06f;  // a strike: white-hot for 60 ms
constexpr float kLiftDone = 3.0f;     // the lift's beats from its beat line to the screen at rest
constexpr float kFoldRuleBeats = 1.0f;  // BACK: the rule sinks over a beat (from Play::kFoldRuleFrom)
// The facts line's length (README 1).
const char* const kModeLength[kModeCount] = {"20 MINUTES AND ON", "2 TO 3 MINUTES", "10 TO 12 MINUTES", "ABOUT 3 MINUTES",
                                              "ABOUT 2 MINUTES",   "ABOUT 2 MINUTES", "AS LONG AS YOU LIKE"};
// A bonus Track's home falls back to a set Track until the bonus Tracks are in (TRACERS: Zenith, JAM: Arcadia).
u32 fallbackHome(u32 home) { return home == 11 ? 6u : (home == 12 ? 8u : home); }
void starGlyph(Ui& ui, Vec2 c, float r, Vec3 col) {
    for (u32 a = 0; a < 4; ++a) {
        const Vec2 dv = polarV(kTwoPi * static_cast<float>(a) / 8.0f, (a & 1u) ? 0.55f * r : r);
        ui.draw().hud.line(c - dv, c + dv, 0.9f * ui.s(), col, col, 0);
    }
}
// A card's outline drawn from its top centre down both sides to meet at its bottom centre (0..1), in its two colours left to
// right.
void cardOutline(Ui& ui, Vec2 a, Vec2 b, float r, float hw, Vec3 c0, Vec3 c1, float lv, float p) {
    if (p <= 0.0f || lv <= 0.0f) return;
    const float w = b.x - a.x, h = b.y - a.y, cx = 0.5f * (a.x + b.x);
    const float arc = 0.25f * kTwoPi * r;
    const float half = (0.5f * w - r) + arc + (h - 2.0f * r) + arc + (0.5f * w - r);
    const float upTo = p * half;
    // The right half's path by its length from the top centre; the left mirrors it.
    const auto at = [&](float L) {
        float d = L;
        if (d <= 0.5f * w - r) return Vec2{cx + d, a.y};
        d -= 0.5f * w - r;
        if (d <= arc) {
            const float t = d / r;
            return Vec2{b.x - r + std::sin(t) * r, a.y + r - std::cos(t) * r};
        }
        d -= arc;
        if (d <= h - 2.0f * r) return Vec2{b.x, a.y + r + d};
        d -= h - 2.0f * r;
        if (d <= arc) {
            const float t = d / r;
            return Vec2{b.x - r + std::cos(t) * r, b.y - r + std::sin(t) * r};
        }
        d -= arc;
        return Vec2{b.x - r - d, b.y};
    };
    const u32 steps = 72;
    for (int side = -1; side <= 1; side += 2) {
        Vec2 prev = at(0.0f);
        for (u32 q = 1; q <= steps; ++q) {
            const float L = std::fmin(upTo, half * static_cast<float>(q) / steps);
            Vec2 p2 = at(L);
            p2.x = cx + (p2.x - cx) * static_cast<float>(side);
            const float u0 = (prev.x - a.x) / w, u1 = (p2.x - a.x) / w;
            ui.draw().hud.line(prev, p2, hw, ui.col(mix(c0, c1, u0), lv), ui.col(mix(c0, c1, u1), lv), kFlatStart | kFlatEnd);
            prev = p2;
            if (L >= upTo) break;
        }
    }
}
}  // namespace

// ---------------------------------------------------------------------------------------------- the modes' facts

u32 Play::tracksAvailable() const {
    // The set's ten and, since part 4, the two bonus Tracks (MODES_PLAN 2.9; 11 Mirrorball, 12 Toybox).
    return kModeTracks;
}

bool Play::modeBuilt(Mode m) const {
    // (each part of M8 adds its modes)
    return m == Mode::Set || m == Mode::OneTrack || m == Mode::Headliners || m == Mode::FullBlast || m == Mode::Tracers;
}

bool Play::modeShown(Mode m) const { return !kReleaseBuild || modeBuilt(m); }

u32 Play::modesRow(Mode* out) const {
    u32 n = 0;
    for (u32 k = 0; k < kModeCount; ++k)
        if (modeShown(static_cast<Mode>(k))) out[n++] = static_cast<Mode>(k);
    return n;
}

u32 Play::modeTrack(Mode m) const {
    // The mode's remembered Track (README 9: track_<mode>), its home until one is chosen (README 3).
    const ModesData::Info& in = species_->modes.of(m);
    if (!in.takesTrack) return 1u;
    const u32 hi = in.trackHi < tracksAvailable() ? in.trackHi : tracksAvailable();
    const u32 t = records_.track[static_cast<u32>(m) % ModeRecords::kModes];
    if (t >= in.trackLo && t <= hi) return t;
    const u32 home = in.home <= tracksAvailable() ? in.home : fallbackHome(in.home);
    return home >= in.trackLo && home <= hi ? home : in.trackLo;
}

RunSetup Play::chosenSetup() const {
    // The title's pill: the mode last played on its remembered Track (THE SET until one is played); a release build's pill is
    // always a built mode.
    RunSetup s;
    s.mode = static_cast<Mode>(records_.mode < kModeCount ? records_.mode : 0u);
    if (!modeShown(s.mode)) s.mode = Mode::Set;
    s.track = static_cast<u8>(modeTrack(s.mode));
    return s;
}

const Palette* Play::trackPalette(u32 track) const {
    for (u32 i = 0; i < palettes_->count; ++i)
        if (palettes_->list[i].track == track) return &palettes_->list[i];
    return nullptr;
}

Vec3 Play::trackAccent(u32 track) const {
    const Palette* p = trackPalette(track);
    return p ? p->accent : palette_->accent;
}

const char* Play::trackName(u32 track) const {
    if (track >= 1 && track <= 10) return songName(track - 1u);
    for (const ModesData::Bonus& b : species_->modes.bonus)
        if (b.track == track) return b.name;
    return "?";
}

float Play::trackBpm(u32 track) const {
    if (track >= 1 && track <= 10) return bpmOfTrack(track, 10u);
    for (const ModesData::Bonus& b : species_->modes.bonus)
        if (b.track == track) return static_cast<float>(b.bpm);
    return 120.0f;
}

Vec3 Play::modeColour(Mode m, u32 track, bool to) const {
    // README 10: two colours a mode, OKLCH [L, hue] at full chroma; ONE TRACK: the chosen Track's accent to its first enemy hue.
    const ModesData::Info& in = species_->modes.of(m);
    if (in.trackColours) {
        const Palette* p = trackPalette(track);
        if (!p) return palette_->accent;
        return to ? p->hue[0] : p->accent;
    }
    const float* c = to ? in.to : in.from;
    return oklchFull(c[0], c[1]);
}

void Play::modeBestLine(Mode m, char* out, size_t cap, bool& star) const {
    // The best of a mode (README 9; screens.md "The title: the mode pill", Best): THE SET's line as built, then each mode's own.
    star = false;
    out[0] = 0;
    char num[32];
    switch (m) {
        case Mode::Set:
            if (scores_.count) {
                groupDigits(num, sizeof num, scores_.entry[0].score);
                char chain[32];
                groupDigits(chain, sizeof chain, scores_.entry[0].bestChain);
                std::snprintf(out, cap, "BEST %s   TRACK %02u   X%s", num, scores_.entry[0].track, chain);
                star = scores_.entry[0].setComplete;
            } else {
                std::snprintf(out, cap, "BEST -");
            }
            break;
        case Mode::OneTrack: {
            u32 best = 0;
            for (u32 t = 1; t < 10; ++t)
                if (records_.one[t].score > records_.one[best].score) best = t;
            if (records_.one[best].score) {
                groupDigits(num, sizeof num, records_.one[best].score);
                std::snprintf(out, cap, "BEST %s  TRACK %02u", num, best + 1u);
                star = records_.one[best].cleared;
            }
            break;
        }
        case Mode::Headliners:
            if (records_.headlinersActs) {
                const u32 secs = records_.headlinersSeconds;
                std::snprintf(out, cap, "BEST %u ACTS  %u:%02u", records_.headlinersActs, secs / 60u, secs % 60u);
                star = records_.headlinersActs >= 10;
            }
            break;
        case Mode::FullBlast:
        case Mode::Tracers:
        case Mode::Songline: {
            const ModeRecords::Timed* table = m == Mode::FullBlast ? records_.blast : (m == Mode::Tracers ? records_.tracers : records_.songline);
            u32 best = 0;
            for (u32 t = 1; t < ModeRecords::kTracks; ++t)
                if (table[t].score > table[best].score) best = t;
            if (table[best].score) {
                groupDigits(num, sizeof num, table[best].score);
                std::snprintf(out, cap, "BEST %s  %s", num, trackName(best + 1u));
            }
            break;
        }
        default: break;  // JAM keeps nothing
    }
}

// ---------------------------------------------------------------------------------------------- the emblems

void Play::drawModeEmblem(Ui& ui, Mode m, Vec2 c, Vec2 half, bool live, float level, u32 track) {
    // README 10: a small moving picture in the game's lines that shows what you do; it moves on the beat at the centre only.
    if (level <= 0.0f) return;
    const float s = ui.s();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 c0 = modeColour(m, track, false), c1 = modeColour(m, track, true);
    const float beats = live ? static_cast<float>(std::fmax(transNow(), 0.0) / kTicksPerBeat) : 0.0f;
    const u32 beat = static_cast<u32>(beats);
    const float inBeat = fractT(beats);
    const float fade = hudFade();
    const auto line = [&](Vec2 a, Vec2 b, float w, Vec3 ca, Vec3 cb, float i) {
        ui.draw().hud.line(a, b, w * s, ui.col(ca, i * level), ui.col(cb, i * level), 0);
    };
    switch (m) {
        case Mode::Set:
        case Mode::OneTrack: {
            // Ten upright bars in the ten Tracks' accents: THE SET a level meter stepping on the beat; ONE TRACK at rest, the
            // chosen Track's bar tall and lit with a white ring round its top.
            const float bw = 2.0f * half.x / 10.0f;
            for (u32 k = 0; k < 10; ++k) {
                const Vec3 acc = trackAccent(k + 1u);
                float h = 0.28f, lit = 1.1f;
                if (m == Mode::Set) {
                    u32 r = (beat + 1u) * 2654435761u ^ (k * 40503u + 977u);
                    r ^= r >> 15;
                    r *= 0x2c1b3c6du;
                    r ^= r >> 12;
                    const float fall = live ? 1.0f - 0.35f * inBeat : 1.0f;
                    h = (0.30f + 0.65f * static_cast<float>(r % 1000u) / 1000.0f) * fall;
                    lit = 1.4f;
                } else if (k + 1u == track) {
                    h = 0.92f;
                    lit = 2.2f;
                }
                // (the bars' round ends inside the emblem's box: the name under it stays clear)
                const float x = c.x - half.x + bw * (static_cast<float>(k) + 0.5f), cap = 0.30f * bw;
                const float foot = c.y + half.y - cap, room = 2.0f * (half.y - cap);
                const Vec2 top{x, foot - room * h};
                line({x, foot}, top, cap / s, acc, acc, lit);
                if (m == Mode::OneTrack && k + 1u == track) ui.ring(top, 0.75f * bw, 1.0f * s, white, 1.5f * level);
            }
            break;
        }
        case Mode::Headliners: {
            // Ten four-point stars on a ring; one flares on each beat, going round.
            const float R = 0.78f * std::fmin(half.x, half.y);
            for (u32 k = 0; k < 10; ++k) {
                const float a = kTwoPi * static_cast<float>(k) / 10.0f - 0.25f * kTwoPi;
                const bool flare = live && k == beat % 10u;
                const float r = (flare ? 0.34f * (1.6f - 0.6f * inBeat) : 0.15f) * R;
                const Vec3 col = mix(c0, c1, static_cast<float>(k) / 9.0f);
                const Vec2 p = c + polarV(a, R);
                for (u32 q = 0; q < 4; ++q) {
                    const Vec2 dv = polarV(kTwoPi * static_cast<float>(q) / 8.0f, (q & 1u) ? 0.45f * r : r);
                    line(p - dv, p + dv, flare ? 1.6f : 1.0f, col, col, flare ? 2.4f : 1.0f);
                }
                if (flare) ui.draw().pools.glow(p, 0.5f * R, col * (0.10f * level * fade * (1.0f - inBeat)));
            }
            break;
        }
        case Mode::FullBlast: {
            // The ship with a fan of seven shot lanes in Prism's colours, each lane firing on its sixteenth.
            const Vec2 ship{c.x - 0.80f * half.x, c.y};
            const float sz = 0.24f * half.y;
            line(ship + Vec2{sz, 0.0f}, ship + Vec2{-sz, -0.7f * sz}, 1.3f, white, white, 1.7f);
            line(ship + Vec2{sz, 0.0f}, ship + Vec2{-sz, 0.7f * sz}, 1.3f, white, white, 1.7f);
            for (u32 j = 0; j < 7; ++j) {
                const float a = (static_cast<float>(j) - 3.0f) * 0.085f;
                const Vec3 col = oklchFull(0.76f, 20.0f + 51.4f * static_cast<float>(j));
                const float L = 1.62f * half.x;
                const Vec2 d = polarV(a, 1.0f);
                line(ship + d * sz, ship + d * L, 0.5f, col, col, 0.35f);
                const float u = live ? fractT(beats * 4.0f - static_cast<float>(j) / 7.0f) : static_cast<float>(j) / 7.0f;
                const Vec2 p = ship + d * (sz + (L - sz) * u);
                line(p, p + d * (0.10f * L), 1.4f, col, white, 1.9f * (1.0f - 0.5f * u));
            }
            break;
        }
        case Mode::Tracers: {
            // The ship drawing a ring; it closes once a bar and the nine tiles inside it light.
            const float R = 0.74f * half.y;
            const float bar = live ? fractT(beats / 4.0f) : 0.999f;
            const u32 segs = 48;
            const u32 drawn = static_cast<u32>(bar * segs);
            for (u32 k = 0; k < drawn; ++k) {
                const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(k) / segs, a1 = a0 + kTwoPi / segs;
                const float u = static_cast<float>(k) / segs;
                line(c + polarV(a0, R), c + polarV(a1, R), 1.6f, mix(c0, c1, u), mix(c0, c1, u + 1.0f / segs), 1.8f);
            }
            if (live) ui.dot(c + polarV(-0.25f * kTwoPi + kTwoPi * bar, R), 4.0f * s, white, 2.2f * level);
            const float lit = live ? clamp01(1.0f - bar * 4.0f) : 0.0f;  // (lit as the ring closes on the bar line, then fading)
            const float t = 0.40f * R;
            for (u32 k = 0; k < 9; ++k) {
                const Vec2 tc = c + Vec2{(static_cast<float>(k % 3u) - 1.0f) * t, (static_cast<float>(k / 3u) - 1.0f) * t};
                const Vec2 h2{0.38f * t, 0.38f * t};
                ui.roundRect(tc - h2, tc + h2, 3.0f * s, 0.8f * s, c1, (0.30f + 1.6f * lit) * level);
            }
            break;
        }
        case Mode::Songline: {
            // A waving string of eight rings; a spark runs along it once a bar and each ring it passes lights.
            const float bar = live ? fractT(beats / 4.0f) : 0.0f;
            const auto at = [&](float u) {
                return Vec2{c.x - 0.86f * half.x + 1.72f * half.x * u, c.y + 0.42f * half.y * std::sin(u * kTwoPi + 0.6f + 0.4f * beats)};
            };
            Vec2 prev = at(0.0f);
            for (u32 q = 1; q <= 32; ++q) {
                const Vec2 p = at(static_cast<float>(q) / 32.0f);
                line(prev, p, 0.5f, c0, c1, 0.45f);
                prev = p;
            }
            for (u32 k = 0; k < 8; ++k) {
                const float u = static_cast<float>(k) / 7.0f;
                const bool passed = live && bar * 7.0f >= static_cast<float>(k);
                ui.ring(at(u), 0.16f * half.y, 1.1f * s, mix(c0, c1, u), (passed ? 1.9f : 0.7f) * level, 24);
            }
            if (live) ui.dot(at(bar), 5.0f * s, white, 2.4f * level);
            break;
        }
        case Mode::Jam: {
            // Eight pads in two rows, lighting in turn on the beats.
            for (u32 k = 0; k < 8; ++k) {
                const Vec2 p{c.x + (static_cast<float>(k % 4u) - 1.5f) * 0.48f * half.x, c.y + (static_cast<float>(k / 4u) - 0.5f) * 0.92f * half.y};
                const bool on = live && k == beat % 8u;
                const Vec3 col = mix(c0, c1, static_cast<float>(k) / 7.0f);
                const Vec2 h2{0.36f * half.y, 0.30f * half.y};
                ui.roundRect(p - h2, p + h2, 6.0f * s, (on ? 1.4f : 0.9f) * s, col, (on ? 2.3f - inBeat : 0.8f) * level);
                if (on) ui.draw().pools.glow(p, 0.7f * half.y, col * (0.12f * level * fade));
            }
            break;
        }
        default: break;
    }
}

// ---------------------------------------------------------------------------------------------- the title's mode pill

void Play::drawModePill(Ui& ui, float sink, bool taps, bool pressedHeld) {
    // screens.md "The title: the mode pill": centre (1200, 600), 480 x 84: the mode's emblem at its left end (moving on the beat),
    // the mode's name (cap 30), a small > at its right end; tapping it opens MODES. Its caption 28 px under it (as CO-OP's): the
    // mode's caption and, for a mode that takes a Track, the Track's name.
    const float s = ui.s(), cx = 0.5f * ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const RunSetup su = chosenSetup();
    const ModesData::Info& in = species_->modes.of(su.mode);
    const Vec2 c{cx, 600.0f * s + sink}, half{240.0f * s, 42.0f * s};
    bool pressed = false, over = false;
    const bool tapped = ui.button(40, c - half, c + half, in.name, pressed, over);
    pressed = pressed || pressedHeld;
    const Vec3 c0 = modeColour(su.mode, su.track, false), c1 = modeColour(su.mode, su.track, true);
    const float r = half.y;
    if (pressed || over) {
        const Vec3 g = ui.col(mix(c0, c1, 0.5f), pressed ? 0.45f : 0.16f);
        ui.draw().hud.line({c.x - half.x + r, c.y}, {c.x + half.x - r, c.y}, r, g, g, 0);
        if (pressed) ui.draw().pools.glow(c, 260.0f * s, mix(c0, c1, 0.5f) * (0.08f * hudFade()));
    }
    ui.roundRect(c - half, c + half, r, (pressed ? 1.5f : 1.0f) * s, white, pressed ? 2.0f : (over ? 1.4f : 0.9f));
    if (pressedHeld && trans_ == Trans::Lift) {
        // The tap answered at once: a ring leaves it, 64 px over half a beat.
        const float t = transBeats(transTap_) / 0.5f;
        if (t >= 0.0f && t < 1.0f) {
            const float grow = 64.0f * s * (1.0f - (1.0f - t) * (1.0f - t));
            ui.roundRect(c - half - Vec2{grow, grow}, c + half + Vec2{grow, grow}, r + grow, 0.8f * s, white, 1.2f * (1.0f - t));
        }
    }
    drawModeEmblem(ui, su.mode, {c.x - half.x + 74.0f * s, c.y}, {42.0f * s, 28.0f * s}, true, 1.0f, su.track);
    ui.setQuiet(true);
    ui.text({c.x + 14.0f * s, c.y + 15.0f * s}, 30.0f * s, mix(c0, c1, 0.35f), pressed ? 1.9f : 1.4f, in.name);
    ui.text({c.x + half.x - 34.0f * s, c.y + 9.0f * s}, 18.0f * s, white, pressed ? 1.6f : 1.0f, ">");
    ui.setQuiet(false);
    char cap[96];
    if (in.takesTrack) std::snprintf(cap, sizeof cap, "%s   %s", in.caption, trackName(su.track));
    else std::snprintf(cap, sizeof cap, "%s", in.caption);
    ui.text({c.x, c.y + half.y + (28.0f + 12.0f) * s}, 12.0f * s, white, 0.5f, cap);
    if (tapped && taps) openModes(Screen::Title);
}

// ---------------------------------------------------------------------------------------------- MODES: the flow

void Play::openModes(Screen back) {
    // From the title's pill (the rule's lift) or the lobby's MODE pill (the room's way in).
    if (trans_ != Trans::None) return;
    modesBack_ = back;
    liftRoom_ = back == Screen::CoopLobby;
    Mode row[kModeCount];
    const u32 n = modesRow(row);
    Mode centre = liftRoom_ ? roomSetup_.mode : chosenSetup().mode;
    bool found = false;
    for (u32 k = 0; k < n; ++k) found = found || row[k] == centre;
    if (!found) centre = row[0];
    modesCentre_ = static_cast<u8>(centre);
    if (liftRoom_) {
        if (species_->modes.of(roomSetup_.mode).takesTrack) records_.track[static_cast<u32>(roomSetup_.mode)] = roomSetup_.track;
        roomEdit_ = roomSetup_;
    }
    modesFocus_ = 0;
    modesSlideFrom_ = 0.0f;
    modesSlideAt_ = modesStrikeAt_ = modesFadeAt_ = modesBackAt_ = -1.0;
    modesDragFrom_ = -1.0f;
    modesPrevMode_ = modesCentre_;
    modesPrevTrack_ = static_cast<u8>(modeTrack(centre));
    keyNav_ = 0;
    const double now = transNow();
    liftFrom_ = titleLook();
    trans_ = Trans::Lift;
    transTap_ = now;
    liftLine_ = std::ceil(now / kTicksPerBeat - 1e-6) * kTicksPerBeat;
    liftSkipAt_ = -1.0;
    liftSkipB_ = 0.0f;
    liftNotes_ = 0;
    screen_ = Screen::Modes;
    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    pulse(0, true);
    if (bandDrives_ && band_ && band_->active() && band_->view().running) {
        // A one-beat sweep, opening (the next beat: the title leaving).
        band_->playRecipe(audio::Recipe::Whoosh, -1, 0.7f, 0.0f, liftLine_ + (coop() ? netBandOffset_ : clock_.offset()), 1.0f);
    }
    modesPreview();
    RF_LOG("play: MODES from the %s (the lift's beat line %.0f, now %.0f)", liftRoom_ ? "lobby" : "title", liftLine_, now);
}

float Play::liftBeats() const {
    float b = transBeats(liftLine_);
    if (liftSkipAt_ >= 0.0) {
        const float u = smooth01(transBeats(liftSkipAt_));
        b = std::fmax(b, liftSkipB_ + (kLiftDone - liftSkipB_) * u);
    }
    return b;
}

void Play::modesNote(u32 index, double tick) {
    if (index >= 8 || !bandDrives_ || !band_ || !band_->active() || !band_->view().running) return;
    const float gain = 0.55f + 0.03f * static_cast<float>(index), pan = (static_cast<float>(index) - 3.5f) / 3.5f * 0.6f;
    band_->playRecipe(audio::Recipe::SawGlide, kWordMidi[index], gain, pan, tick + (coop() ? netBandOffset_ : clock_.offset()), 0.0f, 0.5f);
}

void Play::modesStep(i32 step) {
    // Another mode to the centre: the row slides over half a beat, starting at once; the arriving card is struck on the next
    // sixteenth with its own note; the Track line and strip cross-fade over a beat; the music follows on the next bar line.
    Mode row[kModeCount];
    const u32 n = modesRow(row);
    if (n < 2 || step == 0) return;
    u32 at = 0;
    for (u32 k = 0; k < n; ++k)
        if (row[k] == static_cast<Mode>(modesCentre_)) at = k;
    const double now = transNow();
    const float slideNow = modesSlideAt_ >= 0.0 ? modesSlideFrom_ * (1.0f - smooth01(transBeats(modesSlideAt_) / 0.5f)) : 0.0f;
    modesPrevMode_ = modesCentre_;
    modesPrevTrack_ = static_cast<u8>(modeTrack(static_cast<Mode>(modesCentre_)));
    const i32 ni = static_cast<i32>(n);
    modesCentre_ = static_cast<u8>(row[static_cast<u32>(((static_cast<i32>(at) + step) % ni + ni) % ni)]);
    modesSlideFrom_ = slideNow + static_cast<float>(step);
    modesSlideAt_ = now;
    modesFadeAt_ = now;
    modesStrikeAt_ = (std::floor(now / (0.25 * kTicksPerBeat)) + 1.0) * (0.25 * kTicksPerBeat);
    modesNote(modesCentre_, modesStrikeAt_);
    if (band_) band_->playSfx(audio::Sfx::UiTick, 0.6f);
    pulse(0, true);
    if (!species_->modes.of(static_cast<Mode>(modesCentre_)).takesTrack) modesFocus_ = 0;
    modesPreview();
}

void Play::modesChooseTrack(u32 track) {
    const Mode m = static_cast<Mode>(modesCentre_);
    if (modeTrack(m) == track) return;
    modesPrevMode_ = modesCentre_;
    modesPrevTrack_ = static_cast<u8>(modeTrack(m));
    records_.track[static_cast<u32>(m)] = static_cast<u8>(track);
    modesFadeAt_ = transNow();
    if (band_) band_->playSfx(audio::Sfx::UiTick);
    pulse(0, true);
    modesPreview();
}

void Play::modesPreview() {
    // screens.md "MODES": the centre's Track is heard from the next bar line (THE SET and HEADLINERS: First Light, as the title
    // plays it). The band changes in conductBand; the palette cross-fades over that bar.
    const Mode m = static_cast<Mode>(modesCentre_);
    const ModesData::Info& in = species_->modes.of(m);
    previewWant_ = in.takesTrack ? static_cast<i32>(modeTrack(m)) - 1 : -1;
    if (!bandDrives_) previewLine_ = (std::floor(transNow() / kTicksPerBar) + 1.0) * kTicksPerBar;  // (no band: the clock's next line)
}

void Play::modesBackTap() {
    // BACK: the cards fold up into the rule (sides first), MODES streaks out, the rule sinks to the title's place and the title
    // builds from it; in a room the rule draws in and the lobby comes back. The music returns to the title's on the next bar line.
    if (trans_ != Trans::None && trans_ != Trans::Lift) return;
    if (!liftRoom_ && !opt_.picture[0]) saveModeRecords(records_);  // (the Tracks chosen are remembered)
    trans_ = Trans::Fold;
    transTap_ = transNow();
    previewWant_ = -1;
    if (band_) band_->playSfx(audio::Sfx::UiBack);
    pulse(0, true);
    if (!liftRoom_) wordNotes(transTap_ + (kFoldTitleAt + 0.5) * kTicksPerBeat, 0.25 * kTicksPerBeat);  // (the word played as it builds)
}

void Play::roomChanged() {
    // The host's choice: the room's setup to every seat (the lobby's message), and the change struck here too.
    if (session_ && session_->host()) session_->setRoomSetup(roomSetup_);
    roomStruckAt_ = transNow();
    roomSeen_ = roomSetup_;
    if (band_) band_->playRecipe(audio::Recipe::Glass, -1, 0.5f);
    RF_LOG("play: the room's mode: %s, Track %u, %s%s", modeId(roomSetup_.mode), roomSetup_.track, roomSetup_.versus ? "versus" : "team",
           roomSetup_.versus && !roomSetup_.help ? ", help off" : "");
}

void Play::modesFrame() {
    if (!flow_) return;
    const double now = transNow();
    // A client's copy of the room's setup, as the lobby's message brings it: a change is struck (60 ms white-hot, a glass tick).
    if (session_ && !session_->host()) {
        roomSetup_ = session_->roomSetup();
        if (roomSetup_ != roomSeen_) {
            if (coopSeated_) {
                roomStruckAt_ = now;
                if (band_) band_->playRecipe(audio::Recipe::Glass, -1, 0.5f);
            }
            roomSeen_ = roomSetup_;
        }
    }
    if (modesBackAt_ >= 0.0 && transBeats(modesBackAt_) >= 1.0f) modesBackAt_ = -1.0;
    if (!bandDrives_ && previewWant_ != previewSong_ && previewLine_ >= 0.0 && now >= previewLine_) {
        // (no band to follow: the Track and its palette change on the clock's bar line, as conductBand does with the band)
        previewSong_ = previewWant_;
        palNextLine_ = previewLine_;
        palNextTo_ = previewSong_ >= 0 ? trackPalette(static_cast<u32>(previewSong_) + 1u) : nullptr;
        previewLine_ = -1.0;
    }
    if (trans_ == Trans::Lift) {
        const float b = liftBeats();
        // The lift's notes, each placed a beat ahead (a skip plays no more of them).
        for (u32 j = 0; j < 8; ++j) {
            if (liftNotes_ & (1u << j)) continue;
            const double tick = liftLine_ + static_cast<double>(kLiftNotes[j].b) * kTicksPerBeat;
            if (tick - now > kTicksPerBeat) continue;
            liftNotes_ = static_cast<u8>(liftNotes_ | (1u << j));
            if (liftSkipAt_ < 0.0 && tick >= now - 2.0) modesNote(kLiftNotes[j].note, tick);
        }
        if (b >= kLiftDone) trans_ = Trans::None;
    } else if (trans_ == Trans::Fold) {
        const float t = transBeats(transTap_);
        if (liftRoom_) {
            if (screen_ == Screen::Modes && t >= 1.0f) {
                screen_ = Screen::CoopLobby;
                roomStruckAt_ = now;  // (the lobby's mode name struck as it returns)
            }
            if (t >= 2.0f) trans_ = Trans::None;
        } else {
            if (screen_ == Screen::Modes && t >= kFoldTitleAt) screen_ = Screen::Title;
            if (t >= kFoldTitleAt + 3.5f) trans_ = Trans::None;  // (the title's controls back: buildLook's beat 4)
        }
    }
}

// ---------------------------------------------------------------------------------------------- MODES: what it draws when

Play::ModesLook Play::modesLook() const {
    ModesLook ml;
    const double now = transNow();
    switch (trans_) {
        case Trans::Lift: {
            // screens.md "Transitions", title to MODES (beats from its beat line): the title leaves over beat 0; over beat 1 the rule
            // rises (the title's look draws it) and MODES is struck onto it a sixteenth a letter; in its second half the cards are
            // thrown down from the rule, the centre first, the sides a sixteenth after; over beat 2 the emblems and words fade up
            // and the controls rise 14 px into place, PLAY taking a tap from its start. From the room the rule is thrown out of its
            // centre at its own height.
            const float b = liftBeats();
            ml.rule = liftRoom_ ? 1.0f : 0.0f;
            if (!liftRoom_) {
                // (the title's look draws the rule rising; MODES's word stands on it as it rises)
                const float r = clamp01(b - 1.0f);
                ml.ruleAt = kTitleRuleY + (kRuleRestY - kTitleRuleY) * (1.0f - (1.0f - r) * (1.0f - r) * (1.0f - r));
            }
            const float th = clamp01((b - 1.0f) / 0.5f);
            ml.ruleLen = 1.0f - (1.0f - th) * (1.0f - th);
            ml.ends = smooth01((b - 1.25f) / 0.75f);
            ml.struck = b - 1.0f;
            ml.word = b >= 1.0f ? 1.0f : 0.0f;
            ml.cardCentre = clamp01((b - 1.5f) / 0.25f);
            ml.cardSides = clamp01((b - 1.75f) / 0.25f);
            ml.centreClosed = b - 1.75f;
            ml.sidesClosed = b - 2.0f;
            ml.content = smooth01(b - 2.0f);
            ml.controls = smooth01(b - 2.0f);
            ml.rise = 1.0f - ml.controls;
            ml.taps = b >= 2.0f;
            return ml;
        }
        case Trans::Fold: {
            // BACK (DONE in a room): the cards fold up into the rule over half a beat, the sides first; MODES streaks out to both
            // sides; then the rule sinks to the title's place over a beat (the title's look takes it from there), or draws in.
            const float t = transBeats(transTap_);
            ml.cardSides = 1.0f - clamp01(t / 0.25f);
            ml.cardCentre = 1.0f - clamp01((t - 0.25f) / 0.25f);
            ml.content = 1.0f - smooth01(t / 0.25f);
            ml.controls = 1.0f - smooth01(t / 0.5f);
            ml.streak = smooth01(t / 0.5f);
            ml.word = 1.0f - ml.streak;
            if (liftRoom_) {
                const float d = clamp01((t - kFoldRuleFrom) / 0.5f);
                ml.ruleLen = 1.0f - d * d;
            } else {
                const float d = smooth01((t - kFoldRuleFrom) / kFoldRuleBeats);
                ml.ruleAt = kRuleRestY + (kTitleRuleY - kRuleRestY) * d;
            }
            ml.taps = false;
            return ml;
        }
        case Trans::Wait: {
            if (transFrom_ != Screen::Modes) return ml;
            // MODES to a run (the drop-in): over the wait's first beat the cards, the strip and BACK fade and sink; MODES's tubes run
            // hot by the last beat; in its first half the word goes into the rule, in its second the rule draws in to its centre while
            // it slides to the frame's centre and PLAY shrinks into the same point (drawDropGather).
            const float first = smooth01(transBeats(transTap_));
            ml.content = ml.controls = 1.0f - first;
            ml.sink = first;
            const double lastBeat = transLine_ - kTicksPerBeat;
            const double span = std::fmax(lastBeat - transTap_, kTicksPerBeat);
            ml.heat = smooth01(static_cast<float>((now - transTap_) / span));
            const float h1 = clamp01(static_cast<float>((now - lastBeat) / (0.5 * kTicksPerBeat)));
            const float h2 = clamp01(static_cast<float>((now - (transLine_ - 0.5 * kTicksPerBeat)) / (0.5 * kTicksPerBeat)));
            ml.word = 1.0f - h1;
            ml.ruleHeat = h1;
            ml.ruleLen = 1.0f - h2 * h2;
            ml.ruleY = h2 * h2;
            ml.ends = 1.0f - h1;
            ml.point = h2;
            ml.gather = h2 * h2;
            ml.pressed = true;
            ml.taps = false;
            return ml;
        }
        case Trans::None:
            if (modesBackAt_ >= 0.0) {
                // Back in the wait: everything returns over a beat.
                const float u = smooth01(transBeats(modesBackAt_));
                ml.content = ml.controls = u;
                ml.sink = 1.0f - u;
            }
            return ml;
        default: return ml;
    }
}

Play::TitleLook Play::modesRuleLook(const ModesLook& ml) const {
    // MODES's rule: the title's (screens.md "MODES": 2.4 px in the field, its 10 px copy, its ends the music at half the title's
    // height), at y 150, with nothing else of the title.
    TitleLook t = TitleLook::none();
    t.rule = ml.rule;
    t.ruleLen = ml.ruleLen;
    t.ruleAt = ml.ruleAt;
    t.ruleY = ml.ruleY;
    t.ruleHeat = ml.ruleHeat;
    t.ends = ml.ends;
    t.endsAmp = 0.5f + 0.5f * clamp01((ml.ruleAt - kRuleRestY) / (kTitleRuleY - kRuleRestY));
    t.point = ml.point;
    return t;
}

// ---------------------------------------------------------------------------------------------- MODES: drawing

void Play::drawModesWord(Ui& ui, const ModesLook& ml) {
    // MODES (cap 44, the accent) on the rule, baseline y 104: played onto it a letter a sixteenth (each struck white-hot for 60 ms,
    // then lit; reduce flashing fades each on over an eighth); BACK streaks it out to both sides; the drop-in runs its tubes hot.
    if (ml.word <= 0.0f && ml.streak <= 0.0f) return;
    const float s = ui.s(), cx = 0.5f * ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const char* const word = "MODES";
    const float cap = 44.0f * s, total = ui.measure(cap, word);
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm);
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    const Vec3 top = mix(palette_->accent, white, ml.heat), bottom = mix(palette_->hue[2], white, 0.6f * ml.heat);
    const float baseY = (104.0f + (ml.ruleAt - kRuleRestY)) * s;  // (it stands on the rule wherever the rule is)
    const bool rest = ml.word >= 1.0f && ml.struck > 1.5f && ml.streak <= 0.0f && trans_ == Trans::None && ml.taps;
    if (rest) ui.text({cx, baseY}, cap, white, 0.0f, word);  // (at rest the word is one line for the layout rule)
    ui.setQuiet(true);
    for (u32 j = 0; j < 5; ++j) {
        char ch[2] = {word[j], 0};
        char prefix[6] = {};
        std::memcpy(prefix, word, j);
        const float x0 = cx - 0.5f * total + (j ? ui.measure(cap, prefix) : 0.0f);
        const float w = ui.measure(cap, ch);
        float level = ml.word;
        bool hot = false;
        const float ageS = (ml.struck - 0.25f * static_cast<float>(j)) * beatS;
        if (ageS < 0.0f) level = 0.0f;
        else if (calm) level *= clamp01(ageS / (0.5f * beatS));
        else if (ageS < kStrikeHotS) hot = true;
        // The streak: each letter flies out to its side, fading, a line of light behind it.
        const float side = (static_cast<float>(j) - 2.0f) / 2.0f;
        const float dx = ml.streak * ml.streak * side * 900.0f * s;
        if (ml.streak > 0.0f) {
            const float sl = ml.streak * (1.0f - ml.streak) * 4.0f;
            const Vec3 sc = ui.col(palette_->accent, 1.2f * sl);
            const float y = baseY - 0.45f * cap;
            ui.draw().hud.line({x0 + 0.5f * w + dx - side * 260.0f * s * ml.streak, y}, {x0 + 0.5f * w + dx, y}, 1.2f * s, Vec3{}, sc, 0);
            level = 1.0f - ml.streak;
        }
        if (hot) ui.text({x0 + dx, baseY}, cap, white, 3.5f, ch, 0.0f, 0.06f * cap * 1.2f);
        else if (level > 0.0f) ui.tube({x0 + dx, baseY}, cap, top, white, bottom, (1.5f + 0.6f * ml.heat) * level, ch, 0.0f);
    }
    ui.setQuiet(false);
}

void Play::drawModeCard(Ui& ui, Mode m, float x, float near, const ModesLook& ml, u32 track, bool centre, float slideLevel) {
    // screens.md "MODES": the centre card 720 x 440 at y 440 (glass in the mode's two colours at 0.085, outline 3.4 px at 2.2, two
    // pools, its emblem 560 x 190 about y 330, its name a tube at cap 64 on y 492, the caption, the facts and the best); a side card
    // 520 x 340, outline 2 px at 0.8, its emblem 360 x 130 and its name (cap 36), no glass, no words, at 0.6 of its light.
    // slideLevel 0: only the centre's words, as lines for the layout rule (nothing drawn).
    const float s = ui.s();
    const Vec3 white{1.0f, 1.0f, 1.0f}, gold = palettes_->player[3];
    const ModesData::Info& in = species_->modes.of(m);
    const float dy = 30.0f * s * ml.sink;
    const float nameCap = (36.0f + 28.0f * near) * s;
    const float nameY = (540.0f - 48.0f * near) * s + dy;
    if (slideLevel <= 0.0f) {
        // (the layout rule's lines: the name, the caption, the facts, the best)
        ui.text({x, nameY}, nameCap, white, 0.0f, in.name);
        ui.text({x, 540.0f * s + dy}, 18.0f * s, white, 0.0f, modeBuilt(m) ? in.caption : "COMING IN THIS BUILD'S NEXT PART");
        ui.text({x, 580.0f * s + dy}, 13.0f * s, white, 0.0f, "1-4 PLAYERS");
        return;
    }
    const Vec3 c0 = modeColour(m, track, false), c1 = modeColour(m, track, true);
    const float cw = (kSideW + (kCentreW - kSideW) * near) * s, ch = (kSideH + (kCentreH - kSideH) * near) * s;
    const float y = kCardY * s + dy;
    const Vec2 a{x - 0.5f * cw, y - 0.5f * ch}, b{x + 0.5f * cw, y + 0.5f * ch};
    const float light = (0.6f + 0.4f * near) * slideLevel;
    const float drawn = centre ? ml.cardCentre : ml.cardSides;
    const float closed = centre ? ml.centreClosed : ml.sidesClosed;
    const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
    const float beatS = static_cast<float>(60.0 / bpm);
    const bool calm = settings_.reduceFlashing || forcedReduceFlashing_;
    bool hot = !calm && closed >= 0.0f && closed * beatS < kStrikeHotS;
    if (centre && modesStrikeAt_ >= 0.0 && trans_ == Trans::None && !calm) {
        // (another mode arrived at the centre: struck on the next sixteenth)
        const float age = static_cast<float>((transNow() - modesStrikeAt_) / kTicksPerBeat) * beatS;
        hot = hot || (age >= 0.0f && age < kStrikeHotS);
    }
    const float level = 1.0f - ml.sink;  // (the drop-in's wait: the cards fade as they sink)
    // The rule's thread while the card is thrown down from it.
    if (drawn > 0.0f && drawn < 1.0f) {
        const Vec3 tc = ui.col(mix(c0, c1, 0.5f), 1.2f * (1.0f - drawn));
        ui.draw().hud.line({x, kRuleRestY * s}, {x, a.y}, 0.6f * s, tc * 0.2f, tc, 0);
    }
    if (near > 0.5f && ml.content > 0.0f && drawn >= 1.0f) {
        // Glass in the two colours (bars 40 px tall, flat-ended, inset at the corners), two pools.
        const float g = 0.085f * (near - 0.5f) * 2.0f * ml.content * light * level;
        for (float yy = a.y + 20.0f * s; yy < b.y - 19.0f * s; yy += 40.0f * s) {
            const bool edge = yy < a.y + 21.0f * s || yy > b.y - 21.0f * s;
            const float inset = (edge ? 14.0f : 2.0f) * s;
            ui.draw().hud.line({a.x + inset, yy}, {b.x - inset, yy}, 20.0f * s, ui.col(c0, g), ui.col(c1, g), edge ? 0u : (kFlatStart | kFlatEnd));
        }
        ui.draw().pools.glow({x - 0.25f * cw, y}, 0.40f * cw, c0 * (0.05f * ml.content * level * near * hudFade()));
        ui.draw().pools.glow({x + 0.25f * cw, y}, 0.40f * cw, c1 * (0.05f * ml.content * level * near * hudFade()));
    }
    const float hw = (1.0f + 0.7f * near + (hot ? 0.6f : 0.0f)) * s;
    const float outlineLv = (0.8f + 1.4f * near) * light * level;
    cardOutline(ui, a, b, kCorner * s, hw, hot ? white : c0, hot ? white : c1, hot ? 3.0f : outlineLv, drawn);
    if (ml.content <= 0.0f) return;
    const Vec2 ec{x, (400.0f - 70.0f * near) * s + dy};
    const Vec2 eh{(180.0f + 100.0f * near) * s, (65.0f + 30.0f * near) * s};
    drawModeEmblem(ui, m, ec, eh, centre && near > 0.98f && trans_ != Trans::Wait, light * ml.content * level, track);
    ui.tube({x, nameY}, nameCap, c0, white, c1, (1.0f + 0.6f * near) * light * ml.content * level, in.name);
    const float words = clamp01((near - 0.8f) / 0.2f) * ml.content * level * slideLevel;
    if (words <= 0.0f) return;
    ui.text({x, 540.0f * s + dy}, 18.0f * s, white, 1.2f * words, modeBuilt(m) ? in.caption : "COMING IN THIS BUILD'S NEXT PART");
    char facts[96];
    std::snprintf(facts, sizeof facts, "1-4 PLAYERS   %s   %s", m == Mode::Jam ? "TEAM OR POP-OFF" : "TEAM OR VERSUS", kModeLength[static_cast<u32>(m)]);
    ui.text({x, 580.0f * s + dy}, 13.0f * s, palette_->accent, 1.2f * words, facts);
    char best[96];
    bool cleared = false;
    modeBestLine(m, best, sizeof best, cleared);
    if (best[0] && std::strcmp(best, "BEST -") != 0) {
        ui.text({x, 622.0f * s + dy}, 14.0f * s, gold, 1.4f * words, best);
        if (cleared) starGlyph(ui, {x + 0.5f * ui.measure(14.0f * s, best) + 22.0f * s, 615.0f * s + dy}, 11.0f * s, ui.col(gold, 2.0f * words));
    }
}

void Play::drawTrackStrip(Ui& ui, Mode m, u32 track, float level, float dy, bool taps) {
    // screens.md "MODES": the Track line (baseline 752, cap 18, the chosen Track's accent) and the strip (centre y 820): a chip a
    // Track the mode can play, 96 px apart (a disc of 30 px in its accent, its number at cap 16; the bonus chips a small star; the
    // chosen 42 px, glass in its accent, a ring in the player colour, its number at cap 22; ONE TRACK a gold dot over each chip whose
    // boss has fallen). THE SET and HEADLINERS: the set list, ten bars 100 x 14, 8 apart, numbers under them; it takes no tap.
    if (level <= 0.0f) return;
    const float s = ui.s(), cx = 0.5f * ui.W();
    const Vec3 white{1.0f, 1.0f, 1.0f}, gold = palettes_->player[3];
    const ModesData::Info& in = species_->modes.of(m);
    char tl[96];
    if (m == Mode::Set) std::snprintf(tl, sizeof tl, "ALL TEN TRACKS, IN ORDER");
    else if (m == Mode::Headliners) std::snprintf(tl, sizeof tl, "THE TEN BOSSES, IN ORDER");
    else if (track > 10) std::snprintf(tl, sizeof tl, "BONUS   %s   %.0f BPM", trackName(track), static_cast<double>(trackBpm(track)));
    else std::snprintf(tl, sizeof tl, "TRACK %02u   %s   %.0f BPM", track, trackName(track), static_cast<double>(trackBpm(track)));
    if (!taps) ui.setQuiet(true);
    ui.text({cx, 752.0f * s + dy}, 18.0f * s, in.takesTrack ? trackAccent(track) : palette_->accent, 1.4f * level, tl);
    if (!in.takesTrack) {
        for (u32 k = 0; k < 10; ++k) {
            const float x = cx + (static_cast<float>(k) - 4.5f) * 108.0f * s;
            const Vec3 a2 = ui.col(trackAccent(k + 1u), 1.2f * level);
            ui.draw().hud.line({x - 43.0f * s, 812.0f * s + dy}, {x + 43.0f * s, 812.0f * s + dy}, 7.0f * s, a2, a2, 0);
            char num[4];
            std::snprintf(num, sizeof num, "%u", k + 1u);
            ui.text({x, 846.0f * s + dy}, 12.0f * s, white, 0.6f * level, num);
        }
        if (!taps) ui.setQuiet(false);
        return;
    }
    const u32 lo = in.trackLo, hi = in.trackHi < tracksAvailable() ? in.trackHi : tracksAvailable();
    const u32 n = hi >= lo ? hi - lo + 1u : 1u;
    for (u32 t = lo; t <= hi; ++t) {
        const float x = cx + (static_cast<float>(t - lo) - 0.5f * static_cast<float>(n - 1u)) * 96.0f * s;
        const Vec2 c{x, 820.0f * s + dy};
        const bool chosen = t == track;
        const Vec3 ta = trackAccent(t);
        const float r = (chosen ? 21.0f : 15.0f) * s;
        bool pressed = false;
        const bool focus = modesFocus_ == 1 && chosen;
        if (taps && ui.area(620u + t, c - Vec2{24.0f * s, 24.0f * s}, c + Vec2{24.0f * s, 24.0f * s}, pressed) && !chosen) modesChooseTrack(t);
        if (chosen) {
            const Vec3 g = ui.col(ta, 0.30f * level);
            ui.draw().hud.line(c, c, r, g, g, 0);
            ui.ring(c, r + 5.0f * s, (focus ? 1.6f : 1.1f) * s, palettes_->player[seat_ < kMaxPlayers ? seat_ : 0], (focus ? 2.2f : 1.7f) * level);
        }
        ui.ring(c, r, (pressed ? 1.6f : 1.0f) * s, ta, (chosen || pressed ? 2.0f : 1.0f) * level, 32);
        char num[4];
        std::snprintf(num, sizeof num, "%u", t);
        ui.setQuiet(true);
        ui.text({c.x, c.y + (chosen ? 8.0f : 6.0f) * s}, (chosen ? 22.0f : 16.0f) * s, chosen ? white : ta, (chosen ? 1.8f : 1.2f) * level, num);
        ui.setQuiet(false);
        if (!taps) ui.setQuiet(true);
        if (t > 10) starGlyph(ui, c + Vec2{r * 0.95f, -r * 0.95f}, 6.0f * s, ui.col(white, 1.2f * level));
        if (m == Mode::OneTrack && t <= 10 && records_.one[t - 1u].cleared) ui.dot(c - Vec2{0.0f, r + 11.0f * s}, 3.5f * s, gold, 1.8f * level);
        if (chosen && previewWant_ != previewSong_ && previewWant_ == static_cast<i32>(t) - 1) {
            // NEXT BAR, four dots counting the bar out (as a sound card does).
            const float inBar = static_cast<float>(std::fmod(std::fmax(transNow(), 0.0), kTicksPerBar) / kTicksPerBeat);
            ui.setQuiet(true);
            ui.text({c.x, c.y + 50.0f * s}, 11.0f * s, ta, 1.3f * level, "NEXT BAR");
            ui.setQuiet(false);
            if (!taps) ui.setQuiet(true);
            for (u32 q = 0; q < 4; ++q)
                ui.dot({c.x + (static_cast<float>(q) - 1.5f) * 10.0f * s, c.y + 62.0f * s}, 2.2f * s, white, (static_cast<float>(q) < 4.0f - inBar ? 1.5f : 0.3f) * level);
        }
    }
    if (!taps) ui.setQuiet(false);
}

void Play::screenModes(Ui& ui, const ModesLook& ml) {
    const float s = ui.s(), W = ui.W(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const bool room = liftRoom_;
    const bool host = !room || (session_ && session_->host());
    Mode row[kModeCount];
    const u32 n = modesRow(row);
    u32 at = 0;
    for (u32 k = 0; k < n; ++k)
        if (row[k] == static_cast<Mode>(modesCentre_)) at = k;
    const Mode centre = row[at];
    const u32 track = modeTrack(centre);
    const bool resting = trans_ == Trans::None && modesBackAt_ < 0.0;
    if (!ml.taps) ui.setQuiet(true);
    drawModesWord(ui, ml);
    // The row: three cards in view (five drawn while it slides), wrapping round.
    const float slide = modesSlideAt_ >= 0.0 ? modesSlideFrom_ * (1.0f - smooth01(transBeats(modesSlideAt_) / 0.5f)) : 0.0f;
    const i32 reach = n >= 5 ? 2 : (n >= 2 ? 1 : 0);
    for (i32 k = -reach; k <= reach; ++k) {
        if (n == 2 && k < 0 && std::fabs(slide) < 0.02f) continue;  // (two modes, a release build's: the other on the right)
        const Mode m = row[static_cast<u32>(((static_cast<i32>(at) + k) % static_cast<i32>(n) + static_cast<i32>(n)) % static_cast<i32>(n))];
        const float pos = static_cast<float>(k) + slide;
        if (std::fabs(pos) > 1.6f) continue;
        const float x = cx + pos * kCardStep * s;
        const float near = clamp01(1.0f - std::fabs(pos));
        const float edgeFade = clamp01((1.6f - std::fabs(pos)) / 0.6f);
        const u32 mt = modeTrack(m);
        if (k != 0 && ml.taps && std::fabs(pos) > 0.5f && std::fabs(slide) < 0.02f) {
            // A side card is a control: tap it and it comes to the centre.
            bool pressed = false;
            const Vec2 h{0.5f * kSideW * s, 0.5f * kSideH * s};
            if (ui.area(610u + static_cast<u32>(k + 2), Vec2{x, kCardY * s} - h, Vec2{x, kCardY * s} + h, pressed)) modesStep(k < 0 ? -1 : 1);
        }
        ui.setQuiet(true);
        drawModeCard(ui, m, x, near, ml, mt, k == 0, edgeFade);
        ui.setQuiet(false);
        if (k == 0 && ml.taps && resting && std::fabs(slide) < 0.02f) drawModeCard(ui, m, x, 1.0f, ml, mt, true, 0.0f);
    }
    if (!ml.taps) ui.setQuiet(true);
    // The row dragged more than 80 px brings the next card in.
    if (ml.taps) {
        if (uiPtr_.pressed && uiPtr_.pos.y > 200.0f * s && uiPtr_.pos.y < 680.0f * s) modesDragFrom_ = uiPtr_.pos.x;
        if (modesDragFrom_ >= 0.0f && uiPtr_.down) {
            const float dx = uiPtr_.pos.x - modesDragFrom_;
            if (std::fabs(dx) > 80.0f * s) {
                modesStep(dx < 0.0f ? 1 : -1);
                modesDragFrom_ = uiPtr_.pos.x;
            }
        }
        if (!uiPtr_.down) modesDragFrom_ = -1.0f;
    }
    const float lv = ml.controls;
    const float dy = 14.0f * s * ml.rise + 30.0f * s * ml.sink;
    // The dots: which card is at the centre.
    for (u32 k = 0; k < n; ++k) {
        const Vec2 d{cx + (static_cast<float>(k) - 0.5f * static_cast<float>(n - 1u)) * 36.0f * s, 700.0f * s + dy};
        const bool on = k == at;
        ui.dot(d, 5.0f * s, on ? modeColour(row[k], modeTrack(row[k]), false) : white, (on ? 1.9f : 0.32f) * lv);
    }
    // The Track line and the strip, cross-fading over a beat from the last mode's (or Track's).
    const float fade = modesFadeAt_ >= 0.0 ? smooth01(transBeats(modesFadeAt_)) : 1.0f;
    if (fade < 1.0f) drawTrackStrip(ui, static_cast<Mode>(modesPrevMode_), modesPrevTrack_, lv * (1.0f - fade), dy, false);
    drawTrackStrip(ui, centre, track, lv * fade, dy, ml.taps && host && fade >= 1.0f);
    // Keys and pad: left / right change the mode (or the Track on the strip), up / down move between the cards and the strip.
    if (keyNav_ && ml.taps) {
        const ModesData::Info& in = species_->modes.of(centre);
        if (keyNav_ & 4u) modesFocus_ = 0;
        if ((keyNav_ & 8u) && in.takesTrack && host) modesFocus_ = 1;
        const i32 step = (keyNav_ & 1u) ? -1 : ((keyNav_ & 2u) ? 1 : 0);
        if (step && (modesFocus_ == 0 || !in.takesTrack)) {
            modesFocus_ = 0;
            modesStep(step);
        } else if (step && modesFocus_ == 1 && host) {
            const u32 hi = in.trackHi < tracksAvailable() ? in.trackHi : tracksAvailable();
            const i32 t = static_cast<i32>(track) + step;
            if (t >= static_cast<i32>(in.trackLo) && t <= static_cast<i32>(hi)) modesChooseTrack(static_cast<u32>(t));
        }
    }
    keyNav_ = 0;
    // BACK; PLAY (DONE in a room) with the four beat dots; in a room the host's TEAM / VERSUS and the underdog's help.
    if (lv > 0.0f || ml.pressed) {
        setHudFade(lv);
        if (lv > 0.0f && ui.pill(600, {282.0f * s, 968.0f * s + dy}, {300.0f * s, 90.0f * s}, "BACK", 28.0f * s, false) && ml.taps) modesBackTap();
        const bool built = modeBuilt(centre);
        if (lv > 0.0f && room && host && (kVersusBuilt || !kReleaseBuild)) {
            static const char* const kGame[2] = {"TEAM", "VERSUS"};
            const u32 g = ui.segmented(602, {1958.0f * s, 930.0f * s + dy}, 420.0f * s, 80.0f * s, kGame, 2, roomEdit_.versus ? 1u : 0u, 26.0f * s);
            if (ml.taps && (g == 1u) != roomEdit_.versus) {
                roomEdit_.versus = g == 1u;
                if (band_) band_->playSfx(audio::Sfx::UiTick);
            }
            if (roomEdit_.versus) {
                static const char* const kHelp[2] = {"HELP ON", "HELP OFF"};
                const u32 h = ui.segmented(603, {1958.0f * s, 1026.0f * s + dy}, 420.0f * s, 64.0f * s, kHelp, 2, roomEdit_.help ? 0u : 1u, 22.0f * s);
                if (ml.taps && (h == 0u) != roomEdit_.help) {
                    roomEdit_.help = h == 0u;
                    if (band_) band_->playSfx(audio::Sfx::UiTick);
                }
            }
        }
        if (ml.gather <= 0.0f) {
            // (PLAY stays lit and pressed over the drop-in's wait, in its place, until it shrinks into the point)
            if (ml.pressed) setHudFade(1.0f);
            const Vec2 pc{cx, 968.0f * s + (ml.pressed ? 0.0f : dy)};
            const bool tapped = ui.pill(601, pc, {600.0f * s, 130.0f * s}, room ? "DONE" : "PLAY", 52.0f * s, true, built, ml.pressed);
            const double now = transNow();
            const float beats = static_cast<float>(std::fmax(now, 0.0) / kTicksPerBeat);
            const u32 beatInBar = static_cast<u32>(std::fmod(beats, 4.0f)) % 4u;
            const float left = static_cast<float>((transLine_ - now) / kTicksPerBeat);
            for (u32 q = 0; q < 4; ++q) {
                const Vec2 d{pc.x + (static_cast<float>(q) - 1.5f) * 28.0f * s, pc.y + 44.0f * s};
                const bool lit = ml.pressed ? static_cast<float>(q) < left : q == beatInBar;
                ui.dot(d, 4.0f * s, q == 0 && !ml.pressed ? palette_->accent : white, (lit ? 1.9f : 0.32f) * (built ? 1.0f : 0.4f));
            }
            if ((tapped || keyPrimary_) && built && ml.taps && (trans_ == Trans::None || trans_ == Trans::Lift)) {
                if (room) {
                    // DONE: the room takes the centre's mode, its Track and the game chosen; back to the lobby.
                    roomSetup_ = RunSetup{centre, static_cast<u8>(modeTrack(centre)), roomEdit_.versus, roomEdit_.help};
                    if (roomSetup_ != roomSeen_) roomChanged();
                    if (band_) band_->playSfx(audio::Sfx::UiConfirm);
                    trans_ = Trans::Fold;
                    transTap_ = transNow();
                    previewWant_ = -1;
                } else {
                    // PLAY: the drop-in into the centre's mode on its Track; the title's pill takes it from now.
                    RunSetup su;
                    su.mode = centre;
                    su.track = static_cast<u8>(modeTrack(centre));
                    records_.mode = static_cast<u8>(centre);
                    if (!opt_.picture[0]) saveModeRecords(records_);
                    world_->setNextSetup(su);
                    previewWant_ = -1;
                    dropInTap(Screen::Modes);
                }
                keyPrimary_ = false;
            }
        }
        setHudFade(1.0f);
    }
    if (!ml.taps) ui.setQuiet(false);
    if (keyBack_ && trans_ == Trans::Wait && transFrom_ == Screen::Modes && transNow() < transLine_ - kTicksPerBeat) dropInCancel();
    else if (keyBack_ && ml.taps) modesBackTap();
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- --mode, --coop-mode

void Play::applyModeArgs() {
    // docs/MODES_PLAN.md 2.10: --mode <id>[:<track>[:<moment>[:hold]]] (the pill's mode in the game's screens, the run's in a benchmark
    // or a picture), --versus, --no-help; --coop-mode <id>[:<track>][:vs] (the room's, as the host's MODE pill would set it).
    modeArgsOk_ = true;
    if (opt_.mode[0]) {
        RunSetup su;
        char moment[24] = {};
        if (!parseModeArg(opt_.mode, su, moment, sizeof moment)) {
            RF_ERR("play: --mode '%s' does not parse (set, one, headliners, blast, tracers, songline, jam, with :<track>)", opt_.mode);
            modeArgsOk_ = false;
            return;
        }
        if (!modeBuilt(su.mode)) {
            RF_ERR("play: --mode %s: that mode is not built yet (docs/MODES_PLAN.md 8)", modeId(su.mode));
            modeArgsOk_ = false;
            return;
        }
        su.versus = opt_.versus;
        su.help = !opt_.noHelp;
        if (flow_) {
            records_.mode = static_cast<u8>(su.mode);  // (the pill's, in memory: nothing saved)
            if (species_->modes.of(su.mode).takesTrack) records_.track[static_cast<u32>(su.mode)] = su.track;
        } else if (su != RunSetup{}) {
            world_->resetRun(opt_.seed, 0.0, 1, 0xF, su);
        }
        RF_LOG("play: --mode %s, Track %u%s%s", modeId(su.mode), su.track, moment[0] ? ", moment " : "", moment);
    }
    if (opt_.coopMode[0]) {
        char text[40];
        std::snprintf(text, sizeof text, "%s", opt_.coopMode);
        bool vs = false;
        const size_t len = std::strlen(text);
        if (len > 3 && std::strcmp(text + len - 3, ":vs") == 0) {
            vs = true;
            text[len - 3] = 0;
        }
        RunSetup su;
        if (!parseModeArg(text, su, nullptr, 0) || !modeBuilt(su.mode)) {
            RF_ERR("play: --coop-mode '%s': no such mode built (set, one, ...; :<track>; :vs)", opt_.coopMode);
            modeArgsOk_ = false;
            return;
        }
        su.versus = vs || opt_.versus;
        su.help = !opt_.noHelp;
        roomSetup_ = su;
    }
}

void Play::modesPictureSetup(Mode m, bool room) {
    // Pictures: MODES at rest with that mode at the centre (from the title, or a room's).
    modesBack_ = room ? Screen::CoopLobby : Screen::Title;
    liftRoom_ = room;
    modesCentre_ = static_cast<u8>(m);
    modesPrevMode_ = modesCentre_;
    modesPrevTrack_ = static_cast<u8>(modeTrack(m));
    modesSlideAt_ = modesStrikeAt_ = modesFadeAt_ = modesBackAt_ = -1.0;
    modesFocus_ = 0;
    if (room) roomEdit_ = roomSetup_;
    screen_ = Screen::Modes;
    trans_ = Trans::None;
}

}  // namespace rf
