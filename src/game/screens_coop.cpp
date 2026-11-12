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

void Play::screenCoopJoin(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    if (browser_) browser_->poll(net::wireNowUs());
    coopMessageT_ += frameDt_;
    ui.text({cx, 116.0f * s}, 60.0f * s, palette_->accent, 1.4f, "CO-OP");
    ui.text({cx, 170.0f * s}, 13.0f * s, white, 0.7f, "SAME WI-FI, OR JOIN ONE PHONE'S HOTSPOT");

    // Left: the rooms heard on this network (the last 3 s of beacons), one row each; tap to join.
    const float lx0 = m, lx1 = cx - 60.0f * s;
    ui.text({lx0, 262.0f * s}, 16.0f * s, white, 0.8f, "ROOMS ON THIS NETWORK", 0.0f);
    const u32 rooms = browser_ ? browser_->count() : 0;
    for (u32 k = 0; k < rooms && k < 5; ++k) {
        const net::BeaconInfo& b = browser_->room(k);
        char code[5];
        net::roomCodeEncode(b.code, code);
        char label[64];
        const bool full = b.players >= b.seats, running = b.state == net::kBeaconRunning;
        std::snprintf(label, sizeof label, "%s   %s   %u/%u%s", code, b.name[0] ? b.name : "HOST", b.players, b.seats,
                      !b.compatible ? "   OTHER VERSION" : (running ? "   PLAYING" : (full ? "   FULL" : "")));
        const float y = (330.0f + 112.0f * static_cast<float>(k)) * s;
        const bool can = b.compatible && !running && !full && !session_;
        if (ui.pill(110 + k, {0.5f * (lx0 + lx1), y}, {lx1 - lx0, 92.0f * s}, label, 24.0f * s, false, can, false, palettes_->player[1])) {
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
            joinRoom(b.address, b.port);
            return;
        }
    }
    if (!rooms) {
        const bool listening = browser_ && browser_->open();
        ui.text({0.5f * (lx0 + lx1), 360.0f * s}, 14.0f * s, white, 0.5f, listening ? "LISTENING FOR ROOMS..." : "CANNOT LISTEN FOR ROOMS: TYPE THE CODE OR ADDRESS");
    }

    // Right: the room code (four of sixteen symbols) or an address, typed on the pad.
    const float px = 0.5f * (cx + 60.0f * s + right);
    ui.text({px, 262.0f * s}, 16.0f * s, white, 0.8f, typingIp_ ? "THE HOST'S ADDRESS" : "ROOM CODE");
    {
        char shown[24];
        if (typingIp_) {
            std::snprintf(shown, sizeof shown, "%s_", ipTyped_);
        } else {
            for (u32 k = 0; k < 4; ++k) shown[k] = k < codeLen_ ? codeTyped_[k] : '-';
            shown[4] = '\0';
        }
        const float cap = (typingIp_ ? 60.0f : 90.0f) * s;
        ui.tube({px, (typingIp_ ? 352.0f : 372.0f) * s}, cap, gold, white, gold, 1.5f, shown);
    }
    const float key = 112.0f * s, gap = 14.0f * s;
    const float top = 430.0f * s;
    bool join = false;
    if (!typingIp_) {
        // Sixteen keys (4 x 4): the code's alphabet (net/lobby.h: no 0 O 1 I L 2 Z 5 S 8 B U V).
        for (u32 k = 0; k < 16; ++k) {
            const u32 col = k % 4, row = k / 4;
            const Vec2 c{px + (static_cast<float>(col) - 1.5f) * (key + gap), top + (static_cast<float>(row) + 0.5f) * (0.82f * key + gap)};
            const char lab[2] = {net::kRoomAlphabet[k], 0};
            if (ui.pill(130 + k, c, {key, 0.82f * key}, lab, 40.0f * s, false) && codeLen_ < 4) {
                codeTyped_[codeLen_++] = net::kRoomAlphabet[k];
                codeTyped_[codeLen_] = '\0';
                if (band_) band_->playSfx(audio::Sfx::UiTick);
            }
        }
    } else {
        // Twelve keys: the digits, the dot and (in the last row) DEL and JOIN.
        static const char* const kKeys[11] = {"1", "2", "3", "4", "5", "6", "7", "8", "9", ".", "0"};
        for (u32 k = 0; k < 11; ++k) {
            const u32 col = k % 3, row = k / 3;
            const Vec2 c{px + (static_cast<float>(col) - 1.0f) * (key + gap), top + (static_cast<float>(row) + 0.5f) * (0.82f * key + gap)};
            const u32 len = static_cast<u32>(std::strlen(ipTyped_));
            if (ui.pill(150 + k, c, {key, 0.82f * key}, kKeys[k], 40.0f * s, false) && len + 1 < sizeof ipTyped_) {
                ipTyped_[len] = kKeys[k][0];
                ipTyped_[len + 1] = '\0';
                if (band_) band_->playSfx(audio::Sfx::UiTick);
            }
        }
    }
    const float rowY = top + 4.0f * (0.82f * key + gap) + 0.5f * 0.82f * key;
    if (ui.pill(170, {px - 1.0f * (key + gap), rowY}, {1.6f * key, 0.82f * key}, "DEL", 30.0f * s, false)) {
        if (typingIp_) {
            const u32 len = static_cast<u32>(std::strlen(ipTyped_));
            if (len) ipTyped_[len - 1] = '\0';
        } else if (codeLen_) {
            codeTyped_[--codeLen_] = '\0';
        }
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    const bool ready = typingIp_ ? parseIPv4(ipTyped_) != 0 : codeLen_ == 4;
    if (ui.pill(171, {px + 1.0f * (key + gap), rowY}, {1.6f * key, 0.82f * key}, "JOIN", 30.0f * s, true, ready)) join = true;
    if (ui.pill(172, {px, rowY + 0.82f * key + gap}, {3.4f * key, 0.6f * key}, typingIp_ ? "TYPE A ROOM CODE" : "TYPE AN ADDRESS", 18.0f * s,
                false, true, false, Vec3{0.6f, 0.6f, 0.6f})) {
        typingIp_ = !typingIp_;
        if (band_) band_->playSfx(audio::Sfx::UiTick);
    }
    if (join && ready) {
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        if (typingIp_) {
            joinRoom(parseIPv4(ipTyped_), net::kGamePort);
        } else {
            u16 low = 0;
            if (net::roomCodeDecode(codeTyped_, low)) {
                // A host heard on the network has that code: its address. Else this device's own /16 in front.
                const net::BeaconInfo* b = browser_ ? browser_->byCode(low) : nullptr;
                const u32 own = net::preferredLocalIPv4();
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

    // A message (a refusal, a lost connection), then HOST A ROOM and BACK.
    if (coopMessage_[0] && coopMessageT_ < 12.0f)
        ui.text({0.5f * (lx0 + lx1), H - 250.0f * s}, 16.0f * s, palettes_->danger, 1.5f, coopMessage_, 0.5f);
    if (ui.pill(180, {0.5f * (lx0 + lx1), H - 150.0f * s}, {600.0f * s, 130.0f * s}, "HOST A ROOM", 44.0f * s, true) && !session_) {
        if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        hostRoom();
        return;
    }
    if (ui.pill(181, {m + 150.0f * s, H - 56.0f * s}, {300.0f * s, 80.0f * s}, "BACK", 24.0f * s, false) || keyBack_) {
        leaveCoop("");
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- lobby

void Play::screenCoopLobby(Ui& ui) {
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
    (void)H;
    coopMessageT_ += frameDt_;
    ui.text({cx, 116.0f * s}, 60.0f * s, palette_->accent, 1.4f, "CO-OP");
    if (!session_) {
        keyPrimary_ = keyBack_ = false;
        return;
    }
    net::Session& se = *session_;
    const bool connecting = se.state() == net::SessionState::Connecting;
    ui.text({cx, 192.0f * s}, 16.0f * s, white, 0.8f, connecting ? "JOINING..." : "ROOM CODE");
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
        if (ready) ui.ring(c, 58.0f * s, 0.7f * s, colour, 1.2f, 48);
        char name[40];
        const char* given = se.seatName(k);
        if (given[0] && std::strcmp(given, "PLAYER") != 0 && std::strcmp(given, "HOST") != 0 && !(given[0] == 'P' && given[1] >= '1' && given[1] <= '4' && !given[2]))
            std::snprintf(name, sizeof name, "P%u  %s%s", k + 1, given, k == 0 ? "  HOST" : "");
        else
            std::snprintf(name, sizeof name, "P%u%s", k + 1, k == 0 ? "  HOST" : "");
        ui.text({c.x, c.y + 174.0f * s}, 18.0f * s, colour, 1.4f, name);
        ui.text({c.x, c.y + 203.0f * s}, 13.0f * s, ready ? white : palette_->accent, ready ? 1.0f : 0.6f, ready ? (mine ? "READY - TAP TO HEAR YOUR LEAD" : "READY") : "JOINING");
        if (tapped && band_) {
            // Your own lead (its fixed timbre): root, fifth, octave on the next sixteenths.
            const double heard = band_->view().heardTick;
            const double t0 = (std::floor(heard / kTicksPerStep) + 2.0) * kTicksPerStep;
            for (u8 n = 0; n < 3; ++n) band_->leadNote(0, n, 0.9f, 0.0f, t0 + kTicksPerStep * n);
            seatTapT_[k] = 0.0f;
        }
    }
    ui.text({cx, 880.0f * s}, 12.0f * s, palette_->accent, 0.9f, "EACH PHONE IS AN INSTRUMENT: SET THEM DOWN TOGETHER AND THE ROOM IS THE BAND");

    // START (the host, when every seated player is ready and at least two are seated); the others wait for it.
    if (se.host()) {
        const bool can = se.canStart();
        if (ui.pill(220, {cx, 970.0f * s}, {600.0f * s, 130.0f * s}, "START", 48.0f * s, true, can) || (keyPrimary_ && can)) {
            if (se.requestStart() && band_) band_->playSfx(audio::Sfx::UiConfirm);
        }
        if (!can) {
            u32 seated = 0;
            for (u32 k = 0; k < kMaxPlayers; ++k) seated += (seats >> k) & 1u;
            ui.text({cx, 1050.0f * s}, 12.0f * s, white, 0.5f, seated < 2 ? "WAITING FOR A SECOND PLAYER" : "WAITING FOR EVERYONE TO BE READY");
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
    ui.tube({cx, 300.0f * s}, 96.0f * s, white, palette_->accent, palette_->hue[2], 1.5f, "PAUSED");
    ui.text({cx, 340.0f * s}, 14.0f * s, palette_->accent, 1.1f, "FOR EVERYONE IN THE ROOM");
    {
        u64 team = 0;
        for (u32 q = 0; q < kMaxPlayers; ++q)
            if (world_->player(q).present) team += world_->player(q).score;
        char num[32], line[128];
        std::snprintf(num, sizeof num, "%llu", static_cast<unsigned long long>(team));
        const u32 secs = static_cast<u32>(runSeconds_);
        std::snprintf(line, sizeof line, "TEAM %s   TRACK %u   %u:%02u", num, world_->phrase().track, secs / 60u, secs % 60u);
        ui.text({cx, 400.0f * s}, 20.0f * s, gold, 1.4f, line);
    }
    net::Session* se = session_;
    if (se && se->resuming()) {
        // The shared count-in: four numbered rings fill on the last four beats before the run goes on.
        const double left = se->resumeSong() - se->songAt(se->hostNow());
        const float u = static_cast<float>(1.0 - left / (4.0 * kTicksPerBeat));
        for (u32 k = 0; k < 4; ++k) {
            const Vec2 c{cx + (static_cast<float>(k) - 1.5f) * 150.0f * s, 600.0f * s};
            const float fill = u * 4.0f - static_cast<float>(k) < 0.0f ? 0.0f : (u * 4.0f - static_cast<float>(k) > 1.0f ? 1.0f : u * 4.0f - static_cast<float>(k));
            ui.ring(c, 52.0f * s, (fill >= 1.0f ? 1.6f : 0.9f) * s, palettes_->player[seat_], fill >= 1.0f ? 2.0f : 0.6f, 48);
            const u32 segs = static_cast<u32>(48.0f * fill);
            for (u32 j = 0; j < segs; ++j) {
                const float a0 = -0.25f * kTwoPi + kTwoPi * static_cast<float>(j) / 48.0f, a1 = a0 + kTwoPi / 48.0f;
                const Vec3 c2 = ui.col(palettes_->player[seat_], 1.8f);
                ui.draw().hud.line(c + polarC(a0, 52.0f * s), c + polarC(a1, 52.0f * s), 3.0f * s, c2, c2, 0);
            }
            char n[4];
            std::snprintf(n, sizeof n, "%u", 4u - k);
            ui.text({c.x, c.y + 18.0f * s}, 36.0f * s, white, fill > 0.0f ? 1.8f : 0.6f, n);
        }
        keyPrimary_ = keyBack_ = false;
        return;
    }
    if (askQuit_) {
        ui.text({cx, 560.0f * s}, 24.0f * s, white, 1.3f, "LEAVE THIS RUN? THE OTHERS PLAY ON");
        if (ui.pill(30, {cx - 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "YES", 30.0f * s, false)) {
            askQuit_ = false;
            leaveCoop("");
            if (band_) band_->playSfx(audio::Sfx::UiBack);
            keyPrimary_ = keyBack_ = false;
            return;
        }
        if (ui.pill(31, {cx + 200.0f * s, 700.0f * s}, {320.0f * s, 100.0f * s}, "NO", 30.0f * s, true) || keyBack_) askQuit_ = false;
        keyPrimary_ = keyBack_ = false;
        return;
    }
    if (ui.pill(20, {cx, 600.0f * s}, {600.0f * s, 130.0f * s}, "RESUME", 52.0f * s, true) || keyPrimary_ || keyBack_) coopPause(false);
    if (ui.pill(22, {cx + 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "SETTINGS", 30.0f * s, false)) openSettings(Screen::Paused);
    if (ui.pill(23, {cx - 340.0f * s, 780.0f * s}, {320.0f * s, 100.0f * s}, "LEAVE RUN", 30.0f * s, false, true, false, Vec3{0.6f, 0.6f, 0.6f}))
        askQuit_ = true;
    keyPrimary_ = keyBack_ = false;
}

void Play::screenGameOverCoop(Ui& ui) {
    // screens.md "Game over": in co-op the score is the team's, each player's own under it in their colour, ranked,
    // and the set list carries each player's crosses in their colour. The host's RETRY starts the next run for
    // everyone; the others wait for it. HOME leaves the room.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const Vec3 gold = palettes_->player[3], amber = palettes_->playerDeep[3];
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
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
        const u64 shown = static_cast<u64>(static_cast<double>(team) * (1.0 - (1.0 - u) * (1.0 - u)));
        char num[32];
        {
            char raw[32];
            std::snprintf(raw, sizeof raw, "%llu", static_cast<unsigned long long>(shown));
            const size_t len = std::strlen(raw);
            size_t o = 0;
            for (size_t i = 0; i < len && o + 2 < sizeof num; ++i) {
                num[o++] = raw[i];
                const size_t rest = len - 1 - i;
                if (rest && rest % 3 == 0) num[o++] = ',';
            }
            num[o] = 0;
        }
        const float cap = 120.0f * s;
        const float wd = ui.measure(cap, num);
        ui.font().drawField(ui.draw().hud, {cx - 0.5f * wd, 300.0f * s - cap}, cap, 4.0f * s, ui.col(white, 1.6f), ui.col(gold, 1.6f), ui.col(amber, 1.6f), num);
        ui.text({cx, 336.0f * s}, 14.0f * s, palette_->accent, 1.1f, "TEAM");
    }
    // The ranking: each player's own score, in their colour (yours marked).
    for (u32 k = 0; k < n; ++k) {
        const u32 q = order[k];
        const PlayerState& pl = w.player(q);
        char line[96];
        std::snprintf(line, sizeof line, "%u   P%u%s   %llu   X%u   %u KILLS", k + 1, q + 1, q == seat_ ? " (YOU)" : "", static_cast<unsigned long long>(pl.score),
                      pl.bestChain, pl.kills);
        ui.text({cx, (400.0f + 40.0f * static_cast<float>(k)) * s}, 26.0f * s, palettes_->player[q], q == seat_ ? 1.8f : 1.2f, line);
    }
    // The set list: the run as the song it was; a white star where a boss fell, a gold tick per upgrade (yours), a
    // cross per hit in the colour of the player it hit.
    {
        const float x0 = m, x1 = W - std::fmax(132.0f * s, ui.insetMax().x), y = 640.0f * s;
        const float total = runSeconds_ > 1.0f ? runSeconds_ : 1.0f;
        const auto xAt = [&](float t) { return x0 + (x1 - x0) * (t / total < 0.0f ? 0.0f : (t / total > 1.0f ? 1.0f : t / total)); };
        ui.draw().hud.line({x0, y}, {x1, y}, 4.0f * s, ui.col(palette_->accent, 1.0f), ui.col(palette_->accent, 1.0f), 0);
        for (u32 k = 0; k < markCount_; ++k) {
            const Mark& mk = marks_[k];
            const float x = xAt(mk.t);
            if (mk.kind == 1) {
                for (u32 a = 0; a < 4; ++a) {
                    const Vec2 dv = polarC(kTwoPi * static_cast<float>(a) / 8.0f, 14.0f * s);
                    ui.draw().hud.line(Vec2{x, y - 22.0f * s} - dv, Vec2{x, y - 22.0f * s} + dv, 0.9f * s, ui.col(white, 2.0f), ui.col(white, 2.0f), 0);
                }
            } else if (mk.kind == 2) {
                ui.draw().hud.line({x, y - 14.0f * s}, {x, y - 4.0f * s}, 1.1f * s, ui.col(gold, 1.8f), ui.col(gold, 1.8f), 0);
            } else if (mk.kind == 3) {
                const Vec3 c = ui.col(palettes_->player[mk.a < kMaxPlayers ? mk.a : 0], 1.8f);
                ui.draw().hud.line({x - 5.0f * s, y + 6.0f * s}, {x + 5.0f * s, y + 16.0f * s}, 0.9f * s, c, c, 0);
                ui.draw().hud.line({x + 5.0f * s, y + 6.0f * s}, {x - 5.0f * s, y + 16.0f * s}, 0.9f * s, c, c, 0);
            }
        }
        ui.text({x0, y + 48.0f * s}, 11.0f * s, white, 0.6f, "STAR = BOSS BEATEN   GOLD = YOUR UPGRADES   CROSSES = HITS, IN THE COLOUR OF WHO TOOK THEM", 0.0f);
    }
    const float right = W - std::fmax(132.0f * s, ui.insetMax().x);
    if (session_ && session_->host()) {
        if (ui.pill(40, {right - 300.0f * s, H - 240.0f * s}, {600.0f * s, 140.0f * s}, "RETRY", 52.0f * s, true) || keyPrimary_) {
            session_->requestRetry();
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
        }
    } else {
        ui.text({right - 300.0f * s, H - 225.0f * s}, 24.0f * s, white, 0.8f, "WAITING FOR THE HOST");
    }
    if (ui.pill(41, {right - 165.0f * s, H - 110.0f * s}, {330.0f * s, 90.0f * s}, "HOME", 28.0f * s, false) || keyBack_) {
        leaveCoop("");
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

// ---------------------------------------------------------------------------------------------- local network

void Play::screenNetPermission(Ui& ui) {
    // TECH_SPEC 8.3 (Android 17, target SDK 37): one plain screen says why before the system asks; a refusal says what
    // co-op cannot do without it, and how to allow it later.
    const float s = ui.s(), W = ui.W(), H = ui.H(), cx = 0.5f * W;
    const Vec3 white{1.0f, 1.0f, 1.0f};
    const float m = std::fmax(132.0f * s, ui.insetMin().x);
#if defined(__ANDROID__)
    const int state = android::localNetworkState();  // 0 allowed, 1 to ask, 2 refused, 3 asking
#else
    const int state = permissionAsked_ ? 0 : 1;
#endif
    if (state == 0 && permissionAsked_) {
        openCoop();
        return;
    }
    ui.text({cx, 200.0f * s}, 44.0f * s, palette_->accent, 1.6f, "LOCAL NETWORK");
    if (state == 2) {
        static const char* const kNo[] = {"WITHOUT THE LOCAL NETWORK, CO-OP CANNOT FIND OR JOIN THE PHONES NEAR YOU.",
                                          "YOU CAN STILL PLAY ALONE. TO ALLOW IT LATER: SETTINGS, APPS, RAVEFORM,",
                                          "PERMISSIONS, NEARBY DEVICES."};
        for (u32 k = 0; k < 3; ++k) ui.text({cx, (400.0f + 52.0f * static_cast<float>(k)) * s}, 20.0f * s, white, 1.1f, kNo[k]);
    } else {
        static const char* const kWhy[] = {"TO PLAY TOGETHER, RAVEFORM TALKS TO THE OTHER PHONES ON YOUR WI-FI OR HOTSPOT:",
                                           "IT LISTENS FOR THEIR ROOMS AND SENDS THE GAME TO THEM. NOTHING LEAVES YOUR NETWORK.",
                                           "ANDROID WILL ASK YOU NEXT."};
        for (u32 k = 0; k < 3; ++k) ui.text({cx, (400.0f + 52.0f * static_cast<float>(k)) * s}, 20.0f * s, white, 1.1f, kWhy[k]);
        if (state != 3 && (ui.pill(300, {cx, 720.0f * s}, {600.0f * s, 130.0f * s}, "CONTINUE", 44.0f * s, true) || keyPrimary_)) {
            permissionAsked_ = true;
            if (band_) band_->playSfx(audio::Sfx::UiConfirm);
#if defined(__ANDROID__)
            android::requestLocalNetwork();
#endif
        }
    }
    if (ui.pill(301, {m + 170.0f * s, H - 110.0f * s}, {300.0f * s, 90.0f * s}, "BACK", 24.0f * s, false) || keyBack_) {
        screen_ = Screen::Title;
        if (band_) band_->playSfx(audio::Sfx::UiBack);
    }
    keyPrimary_ = keyBack_ = false;
}

}  // namespace rf
