// M4 stage B: the co-op screens (design/screens.md "Co-op lobby (M4)", design/previews/screens.png). The fewest
// screens: CO-OP on the title opens one join screen (the rooms heard on the network, the room code's pad, an address,
// HOST A ROOM); hosting or joining goes to the lobby (the code, four seats, START for the host, LEAVE); the run
// follows. A plain screen asks for the local network first where Android wants it (TECH_SPEC 8.3).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "audio/band.h"
#include "game/play.h"
#include "net/lobby.h"
#include "net/wire.h"
#if defined(__ANDROID__)
#include "platform/android/android_glue.h"
#endif

namespace rf {

using namespace sim;

namespace {
constexpr float kTwoPi = 6.28318530718f;
Vec2 polarC(float a, float r) { return Vec2{std::cos(a) * r, std::sin(a) * r}; }

// A dotted-quad address typed on the pad (host byte order; 0 when it is not one).
u32 parseIPv4(const char* s) {
    u32 parts[4] = {};
    u32 n = 0, v = 0, digits = 0;
    for (const char* c = s;; ++c) {
        if (*c >= '0' && *c <= '9') {
            v = v * 10u + static_cast<u32>(*c - '0');
            if (++digits > 3 || v > 255u) return 0;
        } else if (*c == '.' || *c == '\0') {
            if (!digits || n >= 4) return 0;
            parts[n++] = v;
            v = digits = 0;
            if (*c == '\0') break;
        } else {
            return 0;
        }
    }
    if (n != 4) return 0;
    return (parts[0] << 24) | (parts[1] << 16) | (parts[2] << 8) | parts[3];
}

// The ship, as the title draws it: four lines in the player's two colours, turned to `a`.
void shipGlyph(Ui& ui, Vec2 c, float r, float a, Vec3 colour, Vec3 light, float i) {
    const float s = ui.s();
    const Vec2 nose = c + polarC(a, r), l = c + polarC(a + 2.5f, r * 0.85f), rr = c + polarC(a - 2.5f, r * 0.85f), tail = c + polarC(a + kTwoPi * 0.5f, r * 0.3f);
    const Vec3 pc = ui.col(colour, 1.8f * i), lc = ui.col(light, 1.6f * i);
    ui.draw().hud.line(nose, l, 1.4f * s, pc, pc, 0);
    ui.draw().hud.line(l, tail, 1.4f * s, pc, pc, 0);
    ui.draw().hud.line(tail, rr, 1.4f * s, lc, lc, 0);
    ui.draw().hud.line(rr, nose, 1.4f * s, lc, lc, 0);
}

// A number with its thousands separators (screens.md: everywhere).
void thousands(char* out, size_t cap, u64 v) {
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

// A dashed capsule outline (the join list's "looking for rooms" row).
void dashedCapsule(Ui& ui, Vec2 a, Vec2 b, float hw, Vec3 colour, float i) {
    const Vec3 k = ui.col(colour, i);
    const float r = 0.5f * (b.y - a.y), cy = 0.5f * (a.y + b.y);
    const float dash = 22.0f * ui.s(), step = 2.0f * dash;
    for (float x = a.x + r; x < b.x - r; x += step) {
        const float x1 = std::fmin(x + dash, b.x - r);
        ui.draw().hud.line({x, a.y}, {x1, a.y}, hw, k, k, 0);
        ui.draw().hud.line({x, b.y}, {x1, b.y}, hw, k, k, 0);
    }
    // The round ends: dashes of the same length along the half circles, each in short pieces.
    const u32 dashes = static_cast<u32>(0.5f * kTwoPi * r / step + 0.5f) > 0 ? static_cast<u32>(0.5f * kTwoPi * r / step + 0.5f) : 1u;
    for (u32 side = 0; side < 2; ++side) {
        const Vec2 c{side ? b.x - r : a.x + r, cy};
        const float from = (side ? -0.25f : 0.25f) * kTwoPi, span = 0.5f * kTwoPi / static_cast<float>(dashes);
        for (u32 j = 0; j < dashes; ++j)
            for (u32 q = 0; q < 4; ++q) {
                const float a0 = from + span * (static_cast<float>(j) + 0.25f + 0.125f * static_cast<float>(q));
                ui.draw().hud.line(c + polarC(a0, r), c + polarC(a0 + 0.125f * span, r), hw, k, k, 0);
            }
    }
}

// A dashed ring (an open seat).
void dashedRing(Ui& ui, Vec2 c, float r, float hw, Vec3 colour, float i, u32 dashes) {
    const Vec3 k = ui.col(colour, i);
    for (u32 j = 0; j < dashes; ++j) {
        const float a0 = kTwoPi * static_cast<float>(j) / static_cast<float>(dashes), a1 = a0 + 0.5f * kTwoPi / static_cast<float>(dashes);
        ui.draw().hud.line(c + polarC(a0, r), c + polarC(a1, r), hw, k, k, 0);
    }
}
}  // namespace

// ---------------------------------------------------------------------------------------------- join

void Play::textInput(const char* utf8) {
    // The host's address on the system keyboard (screens.md: TYPE AN ADDRESS): digits and dots; a comma, a space or a
    // dash is taken as a dot (number keyboards differ in which they offer).
    if (!wantsTextInput() || !utf8) return;
    for (const char* c = utf8; *c; ++c) {
        char ch = *c;
        if (ch == ',' || ch == ' ' || ch == '-') ch = '.';
        if (!((ch >= '0' && ch <= '9') || ch == '.')) continue;
        const size_t len = std::strlen(ipTyped_);
        if (len + 1 >= sizeof ipTyped_) break;
        ipTyped_[len] = ch;
        ipTyped_[len + 1] = '\0';
        if (band_) band_->playSfx(audio::Sfx::UiTick);
    }
}

void Play::screenCoopJoin(Ui& ui) {
    // screens.md "Co-op (M4)", Join: the rooms heard on the network on the left (one row a room), the message line and
    // HOST A ROOM under them, BACK at the bottom left; the room code and its sixteen-key pad on the right, DEL and JOIN
    // under it, TYPE AN ADDRESS under them (the system keyboard).
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3], amber = palettes_->playerDeep[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    if (browser_) browser_->poll(net::wireNowUs());
    coopMessageT_ += frameDt_;
    ui.text({cx, 116.0f * s}, 60.0f * s, palette_->accent, 1.4f, "CO-OP");
    ui.text({cx, 170.0f * s}, 13.0f * s, white, 0.7f, "SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT");

    // Left: the rooms heard in the last 3 s, one row each (however many times each was heard), up to four.
    const float lx0 = m, lx1 = cx - 60.0f * s, lcx = 0.5f * (lx0 + lx1), rowW = lx1 - lx0, rowH = 90.0f * s;
    ui.text({lx0, 196.0f * s}, 16.0f * s, white, 0.8f, "ROOMS ON THIS NETWORK", 0.0f);
    net::BeaconInfo shown[4];
    u32 rooms = 0;
    for (u32 k = 0; browser_ && k < browser_->count() && rooms < 4; ++k) shown[rooms++] = browser_->room(k);
    if (opt_.coopDemoRooms) {
        // --coop-demo-rooms (the layout's picture only): made-up rooms after the ones heard.
        static const char* const kNames[3] = {"TABLET", "LIVING ROOM", "OP12R"};
        static const u16 kCodes[3] = {0x4A3C, 0x9D17, 0xC205};
        static const u8 kPlayers[3] = {2, 4, 3};
        static const u8 kStates[3] = {net::kBeaconLobby, net::kBeaconLobby, net::kBeaconRunning};
        static const sim::Mode kModes[3] = {sim::Mode::Tracers, sim::Mode::Set, sim::Mode::OneTrack};  // (M8: three modes, one versus)
        for (u32 k = 0; k < 3 && rooms < 4; ++k) {
            net::BeaconInfo& b = shown[rooms++];
            b = net::BeaconInfo{};
            b.code = kCodes[k];
            b.players = kPlayers[k];
            b.seats = 4;
            b.state = kStates[k];
            b.compatible = true;
            b.port = net::kGamePort;
            b.mode = static_cast<u8>(kModes[k]);
            b.versus = k == 0;
            std::snprintf(b.name, sizeof b.name, "%s", kNames[k]);
        }
    }
    for (u32 k = 0; k < rooms; ++k) {
        const net::BeaconInfo& b = shown[k];
        const Vec2 c{lcx, (265.0f + 114.0f * static_cast<float>(k)) * s};  // (rows 24 px apart: controls never touch)
        const Vec2 a = c - Vec2{0.5f * rowW, 0.5f * rowH}, bb = c + Vec2{0.5f * rowW, 0.5f * rowH};
        const bool full = b.players >= b.seats, running = b.state == net::kBeaconRunning;
        const bool can = b.compatible && !running && !full && !session_ && b.address != 0;
        bool pressed = false;
        const bool tapped = ui.area(110 + k, a, bb, pressed) && can;
        pressed = pressed && can;
        const Vec3 tone = palettes_->player[0];  // the host's colour (the host is P1)
        if (pressed) {
            const Vec3 g = ui.col(tone, 0.45f);
            ui.draw().hud.line({a.x + 0.5f * rowH, c.y}, {bb.x - 0.5f * rowH, c.y}, 0.5f * rowH, g, g, 0);
            ui.draw().pools.glow(c, 0.55f * rowW, tone * 0.25f);
        }
        ui.roundRect(a, bb, 0.5f * rowH, (pressed ? 1.5f : 1.0f) * s, tone, !can ? 0.3f : (pressed ? 2.0f : 0.9f));
        const float li = can ? (pressed ? 1.8f : 1.1f) : 0.4f;
        char code[5];
        net::roomCodeEncode(b.code, code);
        const float tx = a.x + 40.0f * s, base = c.y + 13.0f * s;
        const float cw = ui.text({tx, base}, 26.0f * s, gold, can ? 1.5f : 0.5f, code, 0.0f);
        char seats[24];
        std::snprintf(seats, sizeof seats, "%u/%u", b.players, b.seats);
        const char* tag = !b.compatible ? "OTHER VERSION" : (running ? "PLAYING" : (full ? "FULL" : ""));
        const float sw = ui.measure(26.0f * s, seats);
        // M8 (design/modes/screens.md "The lobby and the join screen"): the room's mode, and VS in a versus room, at the row's right
        // end (cap 14, white at 0.7).
        char modeLabel[32];
        std::snprintf(modeLabel, sizeof modeLabel, "%s%s", species_->modes.mode[b.mode < sim::kModeCount ? b.mode : 0].name, b.versus ? "  VS" : "");
        const float modeW = ui.text({bb.x - 40.0f * s, base - 2.0f * s}, 14.0f * s, white, 0.7f * (can ? 1.0f : 0.6f), modeLabel, 1.0f) + 36.0f * s;
        float tagW = 0.0f;
        if (tag[0]) tagW = ui.text({bb.x - 40.0f * s - modeW, base}, 20.0f * s, white, 0.6f, tag, 1.0f) + 36.0f * s;
        const float seatsX = bb.x - 40.0f * s - modeW - tagW - sw;
        ui.text({seatsX, base}, 26.0f * s, white, li, seats, 0.0f);
        // The review's fixes (H2, 2026-11-15): a long room name never runs into the seat count: it is cut short with "..." to end
        // 36 px before it.
        {
            const float nx = tx + cw + 48.0f * s, room = seatsX - 36.0f * s - nx;
            char name[sizeof b.name + 4];
            std::snprintf(name, sizeof name, "%s", b.name[0] ? b.name : "HOST");
            if (ui.measure(26.0f * s, name) > room) {
                size_t len = std::strlen(name);
                while (len > 0) {
                    --len;
                    std::snprintf(name + len, sizeof name - len, "...");
                    if (ui.measure(26.0f * s, name) <= room) break;
                }
            }
            ui.text({nx, base}, 26.0f * s, white, li, name, 0.0f);
        }
        if (tapped) {
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
            coopJoinBy_ = 0;
            keyPrimary_ = keyBack_ = false;
            joinRoom(b.address, b.port);
            return;
        }
    }
    if (!rooms) {
        // One dashed row: LOOKING FOR ROOMS, three dots filling on the beat.
        const Vec2 c{lcx, 265.0f * s};
        dashedCapsule(ui, c - Vec2{0.5f * rowW, 0.5f * rowH}, c + Vec2{0.5f * rowW, 0.5f * rowH}, 1.0f * s, white, 0.35f);
        const bool listening = browser_ && browser_->open();
        const char* what = listening ? "LOOKING FOR ROOMS" : "CANNOT LISTEN FOR ROOMS: TYPE THE CODE OR AN ADDRESS";
        const float tw = ui.text({c.x, c.y + 8.0f * s}, 18.0f * s, white, 0.6f, what);
        if (listening) {
            const double beats = band_ && band_->view().running ? band_->view().heardTick / kTicksPerBeat : music1_ / kTicksPerBeat;
            const u32 lit = static_cast<u32>(std::fmod(beats, 4.0));
            for (u32 d = 0; d < 3; ++d)
                ui.dot({c.x + 0.5f * tw + (20.0f + 16.0f * static_cast<float>(d)) * s, c.y + 4.0f * s}, 3.0f * s, white, d < lit ? 1.2f : 0.25f);
        }
    }

    // Right: the room code (four of sixteen symbols) or an address.
    const float px = 0.5f * (cx + 60.0f * s + right);
    ui.text({px, 196.0f * s}, 16.0f * s, white, 0.8f, typingIp_ ? "THE HOST'S ADDRESS" : "ROOM CODE");
    {
        char shownCode[24];
        if (typingIp_) {
            std::snprintf(shownCode, sizeof shownCode, "%s_", ipTyped_);
        } else {
            for (u32 k = 0; k < 4; ++k) shownCode[k] = k < codeLen_ ? codeTyped_[k] : '-';
            shownCode[4] = '\0';
        }
        ui.tube({px, 296.0f * s}, (typingIp_ ? 56.0f : 80.0f) * s, gold, white, gold, 1.5f, shownCode);
    }
    const float key = 112.0f * s, pitch = key + Ui::kControlGapPx * s;
    if (!typingIp_) {
        // Sixteen keys, four rows of 112 px discs: the code's alphabet (net/lobby.h).
        for (u32 k = 0; k < 16; ++k) {
            const u32 col = k % 4, row = k / 4;
            const Vec2 c{px + (static_cast<float>(col) - 1.5f) * pitch, (376.0f + 136.0f * static_cast<float>(row)) * s};
            const char lab[2] = {net::kRoomAlphabet[k], 0};
            if (ui.pill(130 + k, c, {key, key}, lab, 40.0f * s, false) && codeLen_ < 4) {
                codeTyped_[codeLen_++] = net::kRoomAlphabet[k];
                codeTyped_[codeLen_] = '\0';
                if (band_) band_->playSfx(audio::Sfx::UiTick);
            }
        }
    } else {
        ui.text({px, 420.0f * s}, 18.0f * s, white, 0.8f, "TYPE IT ON THE KEYBOARD: FOUR NUMBERS WITH DOTS");
        ui.text({px, 460.0f * s}, 14.0f * s, white, 0.5f, "THE HOST'S PHONE SHOWS IT UNDER ITS WI-FI SETTINGS");
    }
    const float rowY = 904.0f * s;
    bool join = false;
    const bool hasText = typingIp_ ? ipTyped_[0] != '\0' : codeLen_ > 0;
    if (ui.pill(170, {px - 110.0f * s, rowY}, {180.0f * s, 80.0f * s}, "DEL", 30.0f * s, false, hasText)) {
        if (typingIp_) {
            const u32 len = static_cast<u32>(std::strlen(ipTyped_));
            if (len) ipTyped_[len - 1] = '\0';
        } else if (codeLen_) {
            codeTyped_[--codeLen_] = '\0';
        }
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    const bool ready = typingIp_ ? parseIPv4(ipTyped_) != 0 : codeLen_ == 4;
    if (ui.pill(171, {px + 110.0f * s, rowY}, {180.0f * s, 80.0f * s}, "JOIN", 30.0f * s, true, ready) || (keyPrimary_ && ready)) join = true;
    if (ui.pill(172, {px, 1003.0f * s}, {380.0f * s, 70.0f * s}, typingIp_ ? "TYPE A ROOM CODE" : "TYPE AN ADDRESS", 20.0f * s, false, true, false,
                Vec3{0.6f, 0.6f, 0.6f})) {
        typingIp_ = !typingIp_;
        if (band_) band_->playSfx(audio::Sfx::UiTick);
    }
    if (join && ready) {
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        keyPrimary_ = keyBack_ = false;
        if (typingIp_) {
            coopJoinBy_ = 2;
            typingIp_ = false;  // (the keyboard closes)
            joinRoom(parseIPv4(ipTyped_), net::kGamePort);
        } else {
            u16 low = 0;
            if (net::roomCodeDecode(codeTyped_, low)) {
                // A host heard on the network has that code: its address. Else this device's own /16 in front.
                const net::BeaconInfo* b = browser_ ? browser_->byCode(low) : nullptr;
                const u32 own = net::preferredLocalIPv4();
                coopJoinBy_ = 1;
                if (b) {
                    joinRoom(b->address, b->port);
                } else if (own) {
                    joinRoom(net::roomCodeHost(low, own), net::kGamePort);
                } else {
                    std::snprintf(coopMessage_, sizeof coopMessage_, "NO NETWORK: JOIN THE SAME WI-FI OR A PHONE'S HOTSPOT FIRST");
                    coopMessageT_ = 0.0f;
                }
            }
        }
        return;
    }

    // The message line (amber: the danger colour is for what can kill you), HOST A ROOM, BACK.
    if (coopMessage_[0] && coopMessageT_ < 12.0f) ui.text({lcx, 712.0f * s}, 16.0f * s, amber, 1.6f, coopMessage_);
    if (ui.pill(180, {lcx, 850.0f * s}, {600.0f * s, 130.0f * s}, "HOST A ROOM", 44.0f * s, true, !session_) && !session_) {
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        keyPrimary_ = keyBack_ = false;
        hostRoom();
        return;
    }
    if (ui.pill(181, {m + 150.0f * s, H - 90.0f * s}, {300.0f * s, 90.0f * s}, "BACK", 24.0f * s, false) || keyBack_) {
        typingIp_ = false;
        leaveCoop("");
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- lobby

void Play::screenCoopLobby(Ui& ui) {
    const float s = ui.s(), W = ui.W(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    coopMessageT_ += frameDt_;
    ui.text({cx, 116.0f * s}, 60.0f * s, palette_->accent, 1.4f, "CO-OP");
    if (!session_) {
        keyPrimary_ = keyBack_ = false;
        return;
    }
    net::Session& se = *session_;
    const bool connecting = se.state() == net::SessionState::Connecting;
    ui.text({cx, 190.0f * s}, 14.0f * s, white, 0.8f, connecting ? "JOINING..." : "ROOM CODE");
    {
        char code[5];
        net::roomCodeEncode(se.roomCode(), code);
        if (!connecting) {
            ui.draw().pools.glow({cx, 270.0f * s}, 420.0f * s, gold * 0.10f);
            ui.tube({cx, 330.0f * s}, 120.0f * s, gold, white, gold, 1.6f, code);
        }
    }
    ui.text({cx, 369.0f * s}, 13.0f * s, white, 0.7f, "SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT");

    // The four seats: taken (the player's colour field, glass, a pool, the ship with its halo, name and state), joining
    // (the ring at 0.8, no glass, the ship at a third) or open (a dashed white ring). Tapping your own plays your lead.
    const u8 seats = se.lobbySeats(), readyMask = se.lobbyReady();
    for (u32 k = 0; k < kMaxPlayers; ++k) {
        const Vec2 c{cx + (-720.0f + 480.0f * static_cast<float>(k)) * s, 610.0f * s};
        const float r = 130.0f * s;
        const Vec3 colour = palettes_->player[k], light = palettes_->playerLight[k];
        const bool taken = ((seats >> k) & 1u) != 0, ready = taken && ((readyMask >> k) & 1u) != 0;
        if (!taken) {
            dashedRing(ui, c, r, 1.0f * s, white, 0.3f, 24);
            ui.text({c.x, c.y + 8.0f * s}, 16.0f * s, white, 0.4f, "OPEN SEAT");
            continue;
        }
        const bool mine = !connecting && k == se.seat();
        bool pressed = false;
        const bool tapped = mine && ui.area(200 + k, c - Vec2{r, r}, c + Vec2{r, r}, pressed);
        float& tapAge = seatTapT_[k];  // seconds since your seat was tapped (< 0: never)
        if (tapAge >= 0.0f) tapAge += frameDt_;
        const float pulse = tapAge >= 0.0f && tapAge < 0.4f ? 1.0f - tapAge / 0.4f : 0.0f;
        // The ring in the player's colour field; glass and a pool when ready.
        const u32 segs = 72;
        for (u32 j = 0; j < segs; ++j) {
            const float a0 = kTwoPi * static_cast<float>(j) / segs, a1 = kTwoPi * static_cast<float>(j + 1) / segs;
            const float u = static_cast<float>(j) / segs;
            const Vec3 fc = u < 0.25f ? colour + (white - colour) * (u * 4.0f)
                                      : (u < 0.5f ? white + (colour - white) * ((u - 0.25f) * 4.0f) : (u < 0.75f ? colour + (light - colour) * ((u - 0.5f) * 4.0f) : light + (colour - light) * ((u - 0.75f) * 4.0f)));
            const Vec3 kc = ui.col(fc, (ready ? 2.0f : 0.8f) + 1.2f * pulse + (pressed ? 0.6f : 0.0f));
            ui.draw().hud.line(c + polarC(a0, r), c + polarC(a1, r), (ready ? 1.3f : 1.0f) * s, kc, kc, 0);
        }
        if (ready) {
            // Glass: the disc inside the ring at 0.07 (screens.py), and a pool.
            ui.dot(c, 0.97f * r, colour, 0.07f + 0.1f * pulse);
            ui.draw().pools.glow(c, 240.0f * s, colour * (0.10f + 0.15f * pulse));
        }
        shipGlyph(ui, c, 44.0f * s, -0.25f * kTwoPi, colour, light, ready ? 1.0f : 0.35f);  // pointing up
        if (ready) {
            // The ship's halo: two arcs (as screens.py draws the lobby's ships).
            for (u32 h = 0; h < 2; ++h)
                for (u32 j = 0; j < 14; ++j) {
                    const float a0 = (0.08f + 0.5f * static_cast<float>(h)) * kTwoPi + 0.3f * kTwoPi * static_cast<float>(j) / 14.0f;
                    const float a1 = a0 + 0.3f * kTwoPi / 14.0f;
                    const Vec3 hc = ui.col(h ? light : colour, 1.3f);
                    ui.draw().hud.line(c + polarC(a0, 60.0f * s), c + polarC(a1, 60.0f * s), 0.8f * s, hc, hc, 0);
                }
        }
        char name[40];
        const char* given = se.seatName(k);
        if (given[0] && std::strcmp(given, "PLAYER") != 0 && std::strcmp(given, "HOST") != 0 && !(given[0] == 'P' && given[1] >= '1' && given[1] <= '4' && !given[2]))
            std::snprintf(name, sizeof name, "P%u  %s%s", k + 1, given, k == 0 ? "  HOST" : "");
        else
            std::snprintf(name, sizeof name, "P%u%s", k + 1, k == 0 ? "  HOST" : "");
        ui.text({c.x, c.y + 174.0f * s}, 18.0f * s, colour, 1.4f, name);
        ui.text({c.x, c.y + 203.0f * s}, 13.0f * s, ready ? white : palette_->accent, ready ? 1.0f : 0.6f, ready ? "READY" : "JOINING");
        const u8 crowns = se.crowns(k);
        if (roomSetup_.versus && crowns) {
            // M8 (design/modes/screens.md "The lobby"): in VERSUS, a small crown for each match this seat has won in the room; past
            // five, a crown and X7 (cap 12, the pickup's white-gold), under its state.
            const Vec3 wg = mix(palettes_->pickup, Vec3{1.0f, 1.0f, 1.0f}, 0.35f);
            const u32 shownCrowns = crowns > 5 ? 1u : crowns;
            char times[8] = {};
            if (crowns > 5) std::snprintf(times, sizeof times, "X%u", crowns);
            const float tw = times[0] ? ui.measure(12.0f * s, times) + 8.0f * s : 0.0f;
            const float cwid = 22.0f * s * static_cast<float>(shownCrowns) + tw;
            float x = c.x - 0.5f * cwid + 9.0f * s;
            const float y = c.y + 228.0f * s;
            const Vec3 k2 = ui.col(wg, 1.6f);
            for (u32 q = 0; q < shownCrowns; ++q, x += 22.0f * s) {
                const Vec2 p0{x - 8.0f * s, y}, p1{x - 8.0f * s, y - 7.0f * s}, p2{x - 4.0f * s, y - 3.0f * s}, p3{x, y - 10.0f * s},
                    p4{x + 4.0f * s, y - 3.0f * s}, p5{x + 8.0f * s, y - 7.0f * s}, p6{x + 8.0f * s, y};
                const Vec2 pts[7] = {p0, p1, p2, p3, p4, p5, p6};
                for (u32 j = 0; j + 1 < 7; ++j) ui.draw().hud.line(pts[j], pts[j + 1], 0.8f * s, k2, k2, 0);
                ui.draw().hud.line(p6, p0, 0.8f * s, k2, k2, 0);
            }
            if (times[0]) ui.text({x - 9.0f * s, y}, 12.0f * s, wg, 1.4f, times, 0.0f);
        } else if (mine) {
            ui.text({c.x, c.y + 228.0f * s}, 12.0f * s, white, 0.6f, "TAP TO HEAR YOUR LEAD");
        }
        if (tapped && band_) {
            // Your own lead (its fixed timbre): root, fifth, octave on the next sixteenths.
            const double heard = band_->view().heardTick;
            const double t0 = (std::floor(heard / kTicksPerStep) + 2.0) * kTicksPerStep;
            for (u8 n = 0; n < 3; ++n) band_->leadNote(0, n, 0.9f, 0.0f, t0 + kTicksPerStep * n);
            seatTapT_[k] = 0.0f;
        }
    }
    ui.text({cx, 880.0f * s}, 12.0f * s, palette_->accent, 0.9f,
            roomSetup_.versus ? "EVERY PHONE FOR ITSELF. NOBODY CAN HURT A FRIEND" : "EACH PHONE IS AN INSTRUMENT: SET THEM DOWN TOGETHER AND THE ROOM IS THE BAND");
    {
        // M8 (design/modes/screens.md "The lobby"): the room's mode. The host's MODE pill (560 x 90, the lobby's bottom right as LEAVE
        // is its bottom left; narrower where the screen is) opens MODES for the room, the room's line 28 px over it; the others read
        // it as text at the right margin. A change is struck white-hot for 60 ms on every device.
        const sim::ModesData::Info& in = species_->modes.of(roomSetup_.mode);
        char roomLine[64];
        int at = 0;
        if (in.takesTrack) at += std::snprintf(roomLine + at, sizeof roomLine - static_cast<size_t>(at), "%s   ", trackName(roomSetup_.track));
        at += std::snprintf(roomLine + at, sizeof roomLine - static_cast<size_t>(at), "%s", roomSetup_.versus ? "VERSUS" : "TEAM");
        if (roomSetup_.versus && !roomSetup_.help) std::snprintf(roomLine + at, sizeof roomLine - static_cast<size_t>(at), "   HELP OFF");
        const double bpm = band_ && band_->view().bpm > 1.0 ? static_cast<double>(band_->view().bpm) : 120.0;
        const float struckS = roomStruckAt_ >= 0.0 ? static_cast<float>((transNow() - roomStruckAt_) / kTicksPerBeat * 60.0 / bpm) : 1.0f;
        const bool hot = struckS >= 0.0f && struckS < 0.06f && !(settings_.reduceFlashing || forcedReduceFlashing_);
        const float right = W - m;
        if (se.host() && !connecting) {
            const float pw = std::fmin(560.0f * s, right - (cx + 300.0f * s + 24.0f * s));
            const Vec2 pc{right - 0.5f * pw, 968.0f * s};
            ui.text({right, pc.y - 45.0f * s - 28.0f * s}, 13.0f * s, palette_->accent, 1.2f, roomLine, 1.0f);
            if (ui.pill(222, pc, {pw, 90.0f * s}, in.name, 28.0f * s, false, se.state() == net::SessionState::Lobby && trans_ == Trans::None, hot) &&
                trans_ == Trans::None && se.state() == net::SessionState::Lobby)
                openModes(Screen::CoopLobby);
        } else if (!connecting) {
            ui.text({right, 930.0f * s}, 12.0f * s, white, 0.5f, "MODE", 1.0f);
            ui.text({right, 972.0f * s}, 28.0f * s, hot ? white : Vec3{1.0f, 1.0f, 1.0f}, hot ? 3.0f : 1.1f, in.name, 1.0f);
            ui.text({right, 1004.0f * s}, 13.0f * s, palette_->accent, 1.2f, roomLine, 1.0f);
        }
    }

    // START (the host; disabled until every seated player is ready and at least two are seated); the others see
    // WAITING FOR HOST in its place.
    if (se.host()) {
        const bool can = se.canStart();
        if (ui.pill(220, {cx, 970.0f * s}, {600.0f * s, 130.0f * s}, "START", 48.0f * s, true, can) || (keyPrimary_ && can)) {
            if (se.requestStart() && band_) band_->playSfx(audio::Sfx::UiConfirm);
        }
        if (!can) {
            u32 seated = 0;
            for (u32 k = 0; k < kMaxPlayers; ++k) seated += (seats >> k) & 1u;
            // (M8: left of START, its right end 24 px from START's: MODE has START's right)
            ui.text({cx - 324.0f * s, 976.0f * s}, 12.0f * s, white, 0.5f, seated < 2 ? "WAITING FOR A SECOND PLAYER" : "WAITING FOR EVERYONE TO BE READY",
                    1.0f);
        }
    } else {
        ui.text({cx, 985.0f * s}, 24.0f * s, white, 0.8f, connecting ? "CONNECTING" : "WAITING FOR HOST");
    }
    if (ui.pill(221, {m + 170.0f * s, 970.0f * s}, {300.0f * s, 90.0f * s}, "LEAVE", 24.0f * s, false) || keyBack_) {
        leaveCoop("");
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- pause, game over

void Play::screenPauseCoop(Ui& ui) {
    // screens.md "Pause": in co-op any player may pause; it takes hold on the next beat for everyone and the count-in
    // is shared (the host's song decides when the run goes on). No RESTART: the run is everyone's; LEAVE RUN asks once.
    const float s = ui.s(), W = ui.W(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    ui.scrim(kPauseScrim * (1.0f - pauseCountU()));  // (2026-11-16: as the solo pause's, screenPause)
    ui.setBacking(kPauseBacking);
    ui.setPressFx(true);
    ui.tube({cx, 300.0f * s}, 96.0f * s, white, palette_->accent, palette_->hue[2], 1.5f, "PAUSED");
    ui.text({cx, 344.0f * s}, 16.0f * s, palette_->accent, 1.1f, "FOR EVERYONE IN THE ROOM");
    {
        u64 team = 0;
        for (u32 q = 0; q < kMaxPlayers; ++q)
            if (world_->player(q).present) team += world_->player(q).score;
        char num[32], line[128];
        thousands(num, sizeof num, team);
        const u32 secs = static_cast<u32>(runSeconds_);
        std::snprintf(line, sizeof line, "TEAM %s   TRACK %u   %u:%02u", num, world_->phrase().track, secs / 60u, secs % 60u);
        ui.text({cx, 404.0f * s}, 20.0f * s, gold, 1.4f, line);
    }
    net::Session* se = session_;
    if (se && se->resuming()) {
        // The shared count-in: 3-2-1, three numbered rings fill on the last three beats before the run goes on (2026-11-15).
        constexpr u32 kRings = static_cast<u32>(kCountInBeats);
        const float rings = static_cast<float>(kRings);
        const double left = se->resumeSong() - se->songAt(se->hostNow());
        const float u = static_cast<float>(1.0 - left / (kCountInBeats * kTicksPerBeat));
        for (u32 k = 0; k < kRings; ++k) {
            const Vec2 c{cx + (static_cast<float>(k) - 0.5f * (rings - 1.0f)) * 150.0f * s, 600.0f * s};
            const float fill = u * rings - static_cast<float>(k) < 0.0f ? 0.0f : (u * rings - static_cast<float>(k) > 1.0f ? 1.0f : u * rings - static_cast<float>(k));
            ui.ring(c, 52.0f * s, (fill >= 1.0f ? 1.6f : 0.9f) * s, palettes_->player[seat_], fill >= 1.0f ? 2.0f : 0.6f, 48);
            const u32 segs = static_cast<u32>(48.0f * fill);
            for (u32 j = 0; j < segs; ++j) {
                const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(j) / 48.0f, a1 = a0 + kTwoPi / 48.0f;
                const Vec3 c2 = ui.col(palettes_->player[seat_], 1.8f);
                ui.draw().hud.line(c + polarC(a0, 52.0f * s), c + polarC(a1, 52.0f * s), 3.0f * s, c2, c2, 0);
            }
            char n[4];
            std::snprintf(n, sizeof n, "%u", kRings - k);
            ui.text({c.x, c.y + 18.0f * s}, 36.0f * s, white, fill > 0.0f ? 1.8f : 0.6f, n);
        }
        keyPrimary_ = keyBack_ = false;
        return;
    }
    if (askQuit_) {
        ui.text({cx, 560.0f * s}, 24.0f * s, white, 1.3f, "LEAVE THIS RUN? THE OTHERS PLAY ON");
        if (ui.pill(30, {cx - 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "YES", 30.0f * s, false)) {
            askQuit_ = false;
            keyPrimary_ = keyBack_ = false;
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
            homeTap();  // c9218dc (screens.md "Every other way in or out of a run"): LEAVE RUN takes home's way
            return;
        }
        if (ui.pill(31, {cx + 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "NO", 30.0f * s, true) || keyBack_) {
            askQuit_ = false;
            if (band_) band_->playSfx(audio::Sfx::UiBack);
        }
        keyPrimary_ = keyBack_ = false;
        return;
    }
    // Pause-on-leave (2026-11-14): while a player's app is off the screen the room waits for them (RESUME is theirs to give
    // back: the count-in starts by itself once everyone is here); this device, just back, gets in time first.
    const u8 others = se ? static_cast<u8>(se->awayMask() & ~(1u << seat_)) : 0;
    if (others || (se && se->goneAway())) {
        char who[96] = {};
        if (others) {
            std::snprintf(who, sizeof who, "WAITING FOR");
            for (u32 q = 0; q < kMaxPlayers; ++q) {
                if (!((others >> q) & 1u)) continue;
                const char* nm = se->seatName(q);
                char one[24];
                std::snprintf(one, sizeof one, " %s", nm && *nm ? nm : (q == 0 ? "THE HOST" : "A PLAYER"));
                std::strncat(who, one, sizeof who - std::strlen(who) - 1);
            }
        } else {
            std::snprintf(who, sizeof who, "BACK: GETTING IN TIME WITH THE ROOM");
        }
        ui.text({cx, 580.0f * s}, 30.0f * s, palettes_->player[seat_], 1.5f, who);
        ui.text({cx, 640.0f * s}, 16.0f * s, white, 0.7f,
                others ? "THEY LEFT THE GAME. THE RUN GOES ON WHEN THEY ARE BACK" : "THE RUN GOES ON WITH A COUNT-IN");
        if (keyPrimary_ || keyBack_) keyPrimary_ = keyBack_ = false;
    } else if (ui.pill(20, {cx, 600.0f * s}, {600.0f * s, 130.0f * s}, "RESUME", 52.0f * s, true) || keyPrimary_ || keyBack_) {
        coopPause(false);
    }
    if (ui.pill(22, {cx + 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "SETTINGS", 30.0f * s, false)) openSettings(Screen::Paused);
    if (ui.pill(23, {cx - 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "LEAVE RUN", 30.0f * s, false, true, false, Vec3{0.6f, 0.6f, 0.6f}))
        askQuit_ = true;
    keyPrimary_ = keyBack_ = false;
}

void Play::screenGameOverCoop(Ui& ui) {
    // screens.md "Game over", in co-op: the team's score, the line TEAM / TIME / TRACK under it, the players as a table
    // (rank, name, score, Chain, kills; names left, numbers right, each row in its player's colour, centred as a whole),
    // the set list with each player's crosses in their colour, the host's RETRY (the others see it disabled with
    // WAITING FOR HOST under it), HOME for everyone. The run behind is dimmed to a fifth and draws no beacon.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3], amber = palettes_->playerDeep[3];
    const World& w = *world_;
    ui.text({cx, 130.0f * s}, 44.0f * s, palette_->accent, 1.6f, "RUN OVER");
    u64 team = 0;
    u8 order[kMaxPlayers];
    u32 n = 0;
    for (u32 q = 0; q < kMaxPlayers; ++q)
        if (w.player(q).present) {
            team += w.player(q).score;
            order[n++] = static_cast<u8>(q);
        }
    for (u32 i = 1; i < n; ++i)
        for (u32 j = i; j > 0 && w.player(order[j]).score > w.player(order[j - 1]).score; --j) {
            const u8 t = order[j];
            order[j] = order[j - 1];
            order[j - 1] = t;
        }
    // The team's score counts up over one bar.
    {
        const float bars = overS_ * (static_cast<float>(w.bpm()) / 60.0f) / 4.0f;
        const float u = bars < 0.0f ? 0.0f : (bars > 1.0f ? 1.0f : bars);
        const u64 shownScore = static_cast<u64>(static_cast<double>(team) * (1.0 - (1.0 - u) * (1.0 - u)));
        char num[32], last[32];
        thousands(num, sizeof num, shownScore);
        thousands(last, sizeof last, team);
        // (the review's fixes, H2, 2026-11-15: the team's score fits inside the side margins, scaled down when wider)
        const float room = W - 2.0f * std::fmax(132.0f * s, std::fmax(ui.insetMin().x, ui.insetMax().x));
        const float fit = std::fmin(1.0f, room / std::fmax(ui.measure(120.0f * s, last), 1.0f));
        const float cap = 120.0f * s * fit;
        const float wd = ui.measure(cap, num);
        ui.font().drawField(ui.draw().hud, {cx - 0.5f * wd, 300.0f * s - cap}, cap, 4.0f * s * fit, ui.col(white, 1.6f), ui.col(gold, 1.6f), ui.col(amber, 1.6f), num);
    }
    // The line under the score: TEAM, then TIME and TRACK with their values (labels in the accent, values white).
    {
        const u32 secs = static_cast<u32>(runSeconds_);
        char t[16], tr[8];
        std::snprintf(t, sizeof t, "%u:%02u", secs / 60u, secs % 60u);
        std::snprintf(tr, sizeof tr, "%u", runTrackMax_);
        const float cap = 14.0f * s, sp = 0.6f * cap, gap = 2.4f * cap;
        const char* parts[5] = {"TEAM", "TIME", t, "TRACK", tr};
        const bool label[5] = {true, true, false, true, false};
        float total = 0.0f;
        for (u32 k = 0; k < 5; ++k) total += ui.measure(cap, parts[k]) + (k == 4 ? 0.0f : (k == 1 || k == 3 ? sp : gap));
        float x = cx - 0.5f * total;
        for (u32 k = 0; k < 5; ++k) {
            x += ui.text({x, 342.0f * s}, cap, label[k] ? palette_->accent : white, label[k] ? 1.2f : 1.4f, parts[k], 0.0f);
            x += k == 4 ? 0.0f : (k == 1 || k == 3 ? sp : gap);
        }
    }
    // The players: a table, centred as a whole.
    {
        const float cap = 22.0f * s, colGap = 56.0f * s;
        char cells[kMaxPlayers][5][32];
        float widths[5] = {};
        static const char* const kHead[5] = {"", "", "SCORE", "CHAIN", "KILLS"};
        for (u32 c = 0; c < 5; ++c) widths[c] = ui.measure(12.0f * s, kHead[c]);
        for (u32 k = 0; k < n; ++k) {
            const PlayerState& pl = w.player(order[k]);
            std::snprintf(cells[k][0], 32, "%u", k + 1);
            std::snprintf(cells[k][1], 32, "P%u%s", order[k] + 1u, order[k] == seat_ ? " (YOU)" : "");
            thousands(cells[k][2], 32, pl.score);
            char chain[24];
            thousands(chain, sizeof chain, pl.bestChain);
            std::snprintf(cells[k][3], 32, "X%s", chain);
            thousands(cells[k][4], 32, pl.kills);
            for (u32 c = 0; c < 5; ++c) widths[c] = std::fmax(widths[c], ui.measure(cap, cells[k][c]));
        }
        float total = 0.0f;
        for (u32 c = 0; c < 5; ++c) total += widths[c] + (c < 4 ? colGap : 0.0f);
        float x[5];
        x[0] = cx - 0.5f * total;
        for (u32 c = 1; c < 5; ++c) x[c] = x[c - 1] + widths[c - 1] + colGap;
        // The header (cap 12, accent) over the number columns, right-aligned like them.
        for (u32 c = 2; c < 5; ++c) ui.text({x[c] + widths[c], 384.0f * s}, 12.0f * s, palette_->accent, 1.0f, kHead[c], 1.0f);
        for (u32 k = 0; k < n; ++k) {
            const u32 q = order[k];
            const float y = (420.0f + 38.0f * static_cast<float>(k)) * s;
            const float lvl = q == seat_ ? 1.8f : 1.2f;
            const Vec3 pc = palettes_->player[q];
            ui.text({x[0] + widths[0], y}, cap, pc, lvl, cells[k][0], 1.0f);
            ui.text({x[1], y}, cap, pc, lvl, cells[k][1], 0.0f);
            for (u32 c = 2; c < 5; ++c) ui.text({x[c] + widths[c], y}, cap, pc, lvl, cells[k][c], 1.0f);
        }
    }
    drawSetList(ui, 640.0f * s);
    drawBand(ui, 720.0f * s);
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    const bool host = session_ && session_->host();
    if (ui.pill(40, {right - 300.0f * s, H - 270.0f * s}, {600.0f * s, 140.0f * s}, "RETRY", 52.0f * s, true, host) || (keyPrimary_ && host)) {
        session_->requestRetry();
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
    }
    if (!host) ui.text({right - 300.0f * s, H - 162.0f * s}, 14.0f * s, white, 0.8f, "WAITING FOR HOST");
    if ((ui.pill(41, {right - 165.0f * s, H - 90.0f * s}, {330.0f * s, 90.0f * s}, "HOME", 28.0f * s, false, true, trans_ == Trans::Home) || keyBack_) &&
        trans_ == Trans::None)
        homeTap();  // c9218dc: co-op's HOME is home
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- local network

void Play::screenNetPermission(Ui& ui) {
    // screens.md "Local network notice" (Android 17, an app that targets it): one plain screen says why before the
    // system asks; after a refusal its last line says what is lost and CONTINUE asks again.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
#if defined(__ANDROID__)
    const int state = android::localNetworkState();  // 0 allowed, 1 to ask, 2 refused, 3 asking
#else
    const int state = permissionAsked_ ? 0 : (opt_.coopPermissionScreen == 2 ? 2 : 1);
#endif
    if (state == 0 && permissionAsked_) {
        openCoop();
        return;
    }
    ui.text({cx, 200.0f * s}, 60.0f * s, palette_->accent, 1.6f, "LOCAL NETWORK");
    static const char* const kWhy[3] = {"TO PLAY TOGETHER, RAVEFORM TALKS TO THE OTHER PHONES ON YOUR WI-FI OR HOTSPOT:",
                                        "IT LISTENS FOR THEIR ROOMS AND SENDS THE GAME TO THEM. NOTHING LEAVES YOUR NETWORK.",
                                        "ANDROID WILL ASK YOU NEXT."};
    for (u32 k = 0; k < 3; ++k) {
        const char* line = k == 2 && state == 2 ? "RAVEFORM CANNOT SEE OTHER PHONES WITHOUT IT. YOU CAN ALLOW IT IN ANDROID'S SETTINGS." : kWhy[k];
        ui.text({cx, (400.0f + 50.0f * static_cast<float>(k)) * s}, 18.0f * s, white, 1.2f, line);
    }
    if (ui.pill(300, {cx, 720.0f * s}, {600.0f * s, 130.0f * s}, "CONTINUE", 44.0f * s, true, state != 3) || (keyPrimary_ && state != 3)) {
        permissionAsked_ = true;
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
#if defined(__ANDROID__)
        android::requestLocalNetwork();
#endif
    }
    if (ui.pill(301, {m + 150.0f * s, H - 90.0f * s}, {300.0f * s, 90.0f * s}, "BACK", 24.0f * s, false) || keyBack_) {
        if (band_) band_->playSfx(audio::Sfx::UiBack);
        permissionAsked_ = false;
        leaveCoop("");  // the title and its attract run again
    }
    keyPrimary_ = keyBack_ = false;
}

}  // namespace rf
