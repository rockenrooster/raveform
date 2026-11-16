// M8 part 4 (design/modes/README.md 4, tracers.md "Look"): the Stage's view (the rig, the floor's tiles in the lattice and the
// edges of its lit regions) and TRACERS's lines over the bodies (each tracer with its nodes, the closing hint, the rings, their
// words), and the effects of TRACERS's events (a pop's shock ring, pool and lattice impulse; the house; the count).

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/args.h"
#include "game/oklch.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec3 whiteC() { return {1.0f, 1.0f, 1.0f}; }
Vec2 lerp2(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec2 dir(float a) { return Vec2{std::cos(a), std::sin(a)}; }
Vec3 tracersColour(const World& w, bool to) {
    const ModesData::Info& in = w.species().modes.of(Mode::Tracers);
    const float* c = to ? in.to : in.from;
    return oklchFull(c[0], c[1]);
}
}  // namespace

void PlayView::stageLatticeView(LatticeView& lv, const World& w) {
    if (w.setup().mode != Mode::Tracers) return;
    lv.tileOwner = floorDraw_;
    lv.tileLight = tileLight_;
    lv.tileColour = tileColour_;
    lv.tileOrigin = Vec2{-w.halfW(), -w.halfH()};
    lv.tileWu = w.species().modes.stage.tileWu;
    lv.tilesX = TracerFloor::kTilesX;
    lv.tilesY = TracerFloor::kTilesY;
    // (tracers.md: a lit tile's lines at litLevel, swelling a quarter on the kick)
    lv.tileLevel = w.species().modes.tracers.litLevel * (1.0f + 0.25f * kick_);
}

void PlayView::tracersFloor(const World& w, float t) {
    // The floor as drawn: each tile's colour index (0-3 a player, 4 the house's chase, 0xFF dark) and light. A tile lights from its
    // ring's line inward over a quarter beat; the house's chase spirals out from the last ring over two beats; a tile gone dark
    // blinks twice on the eighths and goes out (at the house's end, ring by ring from the centre over a bar).
    if (w.setup().mode != Mode::Tracers) return;
    const TracerFloor& f = w.tracerFloor();
    const float beatS = static_cast<float>(kTicksPerBeat / w.ticksPerSecond());
    const float eighth = 0.5f * beatS;
    for (u32 k = 0; k < kMaxPlayers; ++k) tileColour_[k] = palettes_->player[k];
    tileColour_[4] = mix(whiteC(), tracersColour(w, false), 0.5f);
    const float maxD = length(Vec2{w.halfW(), w.halfH()});
    for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
        const u8 o = f.owner[tl];
        const u8 now = o == TracerFloor::kDark ? 0xFF : (o == TracerFloor::kHouse ? 4u : (o < kMaxPlayers ? o : 0u));
        if (!floorReady_) {
            floorSeen_[tl] = now;
            tileLitT_[tl] = t - 1.0f;
            tileGoneT_[tl] = -100.0f;
        } else if (now != floorSeen_[tl]) {
            const Vec2 c = w.tileCentre(tl);
            if (now != 0xFF) {
                if (now == 4) {
                    const Vec2 rel = c - f.lastRing;
                    const float spiral = clampf(length(rel) / (2.0f * maxD), 0.0f, 1.0f) + 0.12f * (angleOf(rel) + kPi) / kTau;
                    tileLitT_[tl] = t + 2.0f * beatS * clampf(spiral, 0.0f, 1.0f);
                } else {
                    tileLitT_[tl] = t + 0.25f * beatS * clampf(1.0f - length(c - f.lastRing) / 320.0f, 0.0f, 1.0f);
                }
                tileGoneT_[tl] = -100.0f;
            } else if (floorSeen_[tl] != 0xFF) {
                tileGoneOwner_[tl] = floorSeen_[tl];
                tileGoneT_[tl] = houseDarkNow_ ? t + 4.0f * beatS * clampf(length(c) / maxD, 0.0f, 1.0f) : t;
            }
            floorSeen_[tl] = now;
        }
        u8 draw = now;
        float light = 0.0f;
        if (now != 0xFF) {
            light = clampf((t - tileLitT_[tl]) / 0.06f, 0.0f, 1.0f);
        } else if (tileGoneT_[tl] > -50.0f) {
            const float age = t - tileGoneT_[tl];
            draw = tileGoneOwner_[tl];
            if (age < 0.0f) light = 1.0f;
            else if (age < 4.0f * eighth) light = std::fmod(age, eighth) < 0.5f * eighth ? 1.0f : 0.12f;
            else tileGoneT_[tl] = -100.0f;
        }
        floorDraw_[tl] = light > 0.01f ? draw : static_cast<u8>(0xFF);
        tileLight_[tl] = light;
    }
    floorReady_ = true;
    houseDarkNow_ = false;
    // Each caught body's ring owner (its hull in that colour).
    std::memset(caughtBy_, 0xFF, sizeof caughtBy_);
    for (u32 k = 0; k < World::kRings; ++k) {
        const TracerRing& r = w.ring(k);
        if (!r.active || r.popped) continue;
        for (u32 j = 0; j < r.caught; ++j) {
            const i32 i = w.indexOf(r.handle[j]);
            if (i >= 0) caughtBy_[i] = r.owner;
        }
    }
}

void PlayView::drawStageFloor(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const ModesData::Stage& st = w.species().modes.stage;
    // The rig (README.md 4): eight fixtures along the top wall, a short bar and two ticks each, in the grid's colour.
    if (shown(kMuteGrid)) {
        const Vec3 c = palette_->grid * st.rigLevel;
        const float hw = 0.6f * refPx_;
        const float y = -w.halfH() + st.rigYFromTopWu;
        for (u32 k = 0; k < st.rigFixtures; ++k) {
            const float x = (static_cast<float>(k) - 0.5f * static_cast<float>(st.rigFixtures - 1u)) * st.rigSpacingWu;
            d.scene.line(toScene(Vec2{x - 18.0f, y}), toScene(Vec2{x + 18.0f, y}), hw, c, c, 0);
            d.scene.line(toScene(Vec2{x - 12.0f, y}), toScene(Vec2{x - 12.0f, y + 8.0f}), hw, c, c, 0);
            d.scene.line(toScene(Vec2{x + 12.0f, y}), toScene(Vec2{x + 12.0f, y + 8.0f}), hw, c, c, 0);
        }
    }
    if (w.setup().mode != Mode::Tracers || !shown(kMuteGrid)) return;
    // A lit region's edge (tracers.md "Look"): where a lit tile meets dark or another colour, a line along its side in its owner's
    // colour at edgeLevel (inset a little, so two colours meeting both show).
    const ModesData::Tracers& tr = w.species().modes.tracers;
    const float tw = st.tileWu, in2 = 2.5f;
    const float hw = 0.5f * tr.edgeWidth * refPx_;
    const Vec2 o{-w.halfW(), -w.halfH()};
    const auto other = [&](i32 x, i32 y, u8 c) {
        if (x < 0 || y < 0 || x >= static_cast<i32>(TracerFloor::kTilesX) || y >= static_cast<i32>(TracerFloor::kTilesY)) return true;
        return floorDraw_[static_cast<u32>(y) * TracerFloor::kTilesX + static_cast<u32>(x)] != c;
    };
    for (u32 tl = 0; tl < TracerFloor::kTiles; ++tl) {
        const u8 c = floorDraw_[tl];
        if (c == 0xFF) continue;
        const i32 x = static_cast<i32>(tl % TracerFloor::kTilesX), y = static_cast<i32>(tl / TracerFloor::kTilesX);
        const Vec3 col = tileColour_[c] * (tr.edgeLevel * tileLight_[tl]);
        const Vec2 a = o + Vec2{static_cast<float>(x) * tw + in2, static_cast<float>(y) * tw + in2};
        const Vec2 b = a + Vec2{tw - 2.0f * in2, tw - 2.0f * in2};
        if (other(x - 1, y, c)) d.scene.line(toScene(Vec2{a.x, a.y}), toScene(Vec2{a.x, b.y}), hw, col, col, 0);
        if (other(x + 1, y, c)) d.scene.line(toScene(Vec2{b.x, a.y}), toScene(Vec2{b.x, b.y}), hw, col, col, 0);
        if (other(x, y - 1, c)) d.scene.line(toScene(Vec2{a.x, a.y}), toScene(Vec2{b.x, a.y}), hw, col, col, 0);
        if (other(x, y + 1, c)) d.scene.line(toScene(Vec2{a.x, b.y}), toScene(Vec2{b.x, b.y}), hw, col, col, 0);
    }
}

void PlayView::drawStageLines(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    if (w.setup().mode != Mode::Tracers) return;
    const ModesData::Tracers& tr = w.species().modes.tracers;
    const float t = static_cast<float>(in.time);
    const Vec3 W1 = whiteC();
    const double tps = w.ticksPerSecond();
    const float beatS = static_cast<float>(kTicksPerBeat / tps);
    const u32 me = w.viewPlayer();
    // The rings (tracers.md "A lock"): the line flashes white for 60 ms, then holds in its owner's colour until it pops.
    for (u32 k = 0; k < World::kRings; ++k) {
        const TracerRing& r = w.ring(k);
        if (!r.active || r.popped || r.n < 3) continue;
        const float age = static_cast<float>((in.music - r.lockTick) / tps);
        const bool flash = age < 0.06f;
        const Vec3 c = flash ? W1 * 3.5f : palettes_->player[r.owner < kMaxPlayers ? r.owner : 0] * 2.8f;
        const float hw = (flash ? 1.5f : 1.3f) * refPx_;
        for (u32 j = 0; j < r.n; ++j) d.scene.line(toScene(r.pt[j]), toScene(r.pt[(j + 1u) % r.n]), hw, c, c, kFlatStart | kFlatEnd);
    }
    // Each player's tracer, tail to ship (tracers.md "Look"): `deep` at the tail, the player's colour, white at the ship; its last
    // expiringBeats drawn dashed and flickering; a sparkle where each node fell, flaring as it sounds. Another player's at 0.7.
    for (u32 p = 0; p < w.playerCount(); ++p) {
        const PlayerState& pl = w.player(p);
        const TracerLine& tl = w.tracer(p);
        if (!pl.present || !pl.alive || tl.count < 1) continue;
        const float level = tr.level * (p == me ? 1.0f : 0.7f);
        const Vec3 deep = palettes_->playerDeep[p], col = palettes_->player[p];
        const double life = w.tracerLifeTicks(p), expiring = static_cast<double>(tr.expiringBeats) * kTicksPerBeat;
        const float hw = 0.5f * tr.width * refPx_;
        const Vec2 ship = lerp2(pl.prevPos, pl.pos, in.alpha);
        const u32 n = tl.count;
        const float flicker = 0.55f + 0.45f * std::sin(t * 37.0f);
        for (u32 k = 0; k < n; ++k) {
            const Vec2 a = tl.at(k), b = k + 1u < n ? tl.at(k + 1u) : ship;
            const float u = n > 1 ? static_cast<float>(k) / static_cast<float>(n - 1u) : 1.0f;
            const Vec3 c = u < 0.6f ? mix(deep, col, u / 0.6f) : (u < 0.88f ? col : mix(col, W1, (u - 0.88f) / 0.12f));
            const double left = life - (in.music - tl.bornAt(k));
            float lv = level;
            if (left < expiring) {
                if (k & 1u) continue;  // (dashed)
                lv *= flicker;
            }
            if (shown(kMuteFx)) d.scene.line(toScene(a), toScene(b), hw, c * lv, c * lv, 0);
            if (tl.nodeAt(k)) {
                const float since = static_cast<float>((in.music - tl.bornAt(k)) / tps);
                d.sparkles.glow(toScene(a), 0.5f * tr.nodePx * refPx_, W1 * (since < 0.06f ? 1.6f : 0.7f));
            }
        }
    }
    // The closing hint (rule 5): a dashed line from the ship to the place it would close on, a small ring there (lit within the
    // snap).
    {
        const PlayerState& pl = w.player(me);
        Vec2 at{};
        float dist = 0.0f;
        if (pl.alive && w.closingHint(me, at, dist)) {
            const Vec2 ship = lerp2(pl.prevPos, pl.pos, in.alpha);
            const Vec3 c = palettes_->playerLight[me] * 0.5f;
            const Vec2 dd = at - ship;
            const float l = length(dd);
            if (l > 1.0f) {
                const Vec2 u = dd * (1.0f / l);
                for (float s = 0.0f; s < l; s += 12.0f) d.scene.line(toScene(ship + u * s), toScene(ship + u * std::fmin(s + 6.0f, l)), 0.5f * refPx_, c, c, 0);
            }
            const float rr = 9.0f;
            for (u32 j = 0; j < 12; ++j)
                d.scene.line(toScene(at + dir(kTau * static_cast<float>(j) / 12.0f) * rr), toScene(at + dir(kTau * static_cast<float>(j + 1u) / 12.0f) * rr),
                             0.5f * refPx_, c, c, 0);
            if (dist <= tr.snapWu) d.sparkles.glow(toScene(at), rr * pxPerWu_, palettes_->playerLight[me] * 1.2f);
        }
    }
    // The rings' words (x 5 and up): RING X5 over the ring's centre for a beat (cap 22 wu; x 8: cap 34), the priority layer's rim.
    for (const RingWords& rw : ringWords_) {
        const float age = t - rw.t;
        if (age < 0.0f || age > beatS) continue;
        char text[16];
        std::snprintf(text, sizeof text, "RING X%u", static_cast<unsigned>(rw.bonus));
        const float cap = (rw.bonus >= 8 ? 34.0f : 22.0f) * pxPerWu_;
        const float wt = font_->measure(TextStyle{cap}, text);
        const Vec2 sc = toScreen(rw.at);
        const Vec2 at{sc.x - 0.5f * wt, sc.y - 0.5f * cap};
        const Vec3 rim{};
        font_->drawField(d.threatRims, at, cap, 2.2f * refPx_, rim, rim, rim, text);
        const Vec3 c = palettes_->player[rw.owner < kMaxPlayers ? rw.owner : 0] * 1.6f;
        font_->drawField(d.threatCores, at, cap, 1.0f * refPx_, c, c, c, text);
    }
    // FULL HOUSE (rule 11): the words at cap 110 in the mode's colours, X n under it, for two beats from the chase's end.
    {
        const float age = t - houseT_ - 2.0f * beatS;
        if (age >= -2.0f * beatS && age < 2.0f * beatS) {
            const float k = clampf(1.0f - std::fabs(age) / (2.0f * beatS), 0.0f, 1.0f);
            const float cap = 110.0f * refPx_;
            const char* text = "FULL HOUSE";
            const float wt = font_->measure(TextStyle{cap}, text);
            const Vec2 at{centre_.x - 0.5f * wt, centre_.y - 0.75f * cap};
            const Vec3 rim{}, a = tracersColour(w, false) * (1.6f * k), b = tracersColour(w, true) * (1.6f * k);
            font_->drawField(d.threatRims, at, cap, 4.0f * refPx_, rim, rim, rim, text);
            font_->drawField(d.threatCores, at, cap, 1.8f * refPx_, a, mix(a, b, 0.5f), b, text);
            char x[16];
            std::snprintf(x, sizeof x, "X %u", static_cast<unsigned>(houseNo_));
            const float cap2 = 44.0f * refPx_;
            const float w2 = font_->measure(TextStyle{cap2}, x);
            const Vec2 at2{centre_.x - 0.5f * w2, centre_.y + 0.45f * cap};
            font_->drawField(d.threatRims, at2, cap2, 2.6f * refPx_, rim, rim, rim, x);
            font_->drawField(d.threatCores, at2, cap2, 1.2f * refPx_, W1 * (1.4f * k), W1 * (1.4f * k), W1 * (1.4f * k), x);
        }
    }
    // The count (rule 12, screens.md "The count"): a line crossing the Stage left to right over two beats, the title's rule stood
    // upright, 2.4 px in the mode's colours with a white core.
    {
        const float u = (t - countT_) / (2.0f * beatS);
        if (u >= 0.0f && u <= 1.0f) {
            const float x = -w.halfW() + 2.0f * w.halfW() * u;
            const Vec3 a = tracersColour(w, false) * 2.2f, b = tracersColour(w, true) * 2.2f;
            d.scene.line(toScene(Vec2{x, -w.halfH()}), toScene(Vec2{x, w.halfH()}), 1.2f * refPx_, a, b, 0);
            d.scene.line(toScene(Vec2{x, -w.halfH()}), toScene(Vec2{x, w.halfH()}), 0.5f * refPx_, W1 * 2.0f, W1 * 2.0f, 0);
        }
    }
}

void PlayView::tracersEvent(const World& w, const Event& e, float t) {
    const Vec2 at{e.x, e.y};
    const u32 owner = e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const Vec3 col = palettes_->player[owner], light = palettes_->playerLight[owner];
    const float beatS = static_cast<float>(kTicksPerBeat / w.ticksPerSecond());
    switch (e.type) {
        case EventType::RingPop: {
            // tracers.md "A pop": the ring's line leaves as a shock ring; one pool of the owner's colour as wide as the ring, 0.30
            // falling to nothing in half a beat; the lattice thrown outward (140; 300 for a ring of forty); from x 5 a shake of 3 px
            // (6 for x 8) and the ring's words.
            const float r = std::fmax(e.f, 60.0f);
            addShockRings(at, 1, r * 1.3f, col, light, t, 0.2f);
            addPool(at, r, r, col, 0.30f, 0.0f, 0.5f * beatS, false, 1, false, false, t);
            if (!reduceFlashing_) push(kImpKill, at, e.c >= 8 ? 300.0f : 140.0f, r * 1.4f, t);
            if (e.c >= 5) {
                shakeAmp_ = std::fmax(shakeAmp_, e.c >= 8 ? 6.0f : 3.0f);
                RingWords& rw = ringWords_[ringWordsHead_];
                ringWordsHead_ = (ringWordsHead_ + 1u) % 8u;
                rw.at = at;
                rw.t = t;
                rw.bonus = e.c;
                rw.owner = static_cast<u8>(owner);
            }
            break;
        }
        case EventType::House:
            if (e.b == 0) {
                houseT_ = t;
                houseNo_ = e.a;
                houseAt_ = at;
            } else if (e.b == 1) {
                // The crowd's pop and the house's pay: the lattice rings from the centre.
                if (!reduceFlashing_) push(kImpDropWave, Vec2{}, 300.0f, 900.0f, t);
                shakeAmp_ = std::fmax(shakeAmp_, 6.0f);
                addShockRings(Vec2{}, 2, 1000.0f, tracersColour(w, false), tracersColour(w, true), t, 0.5f);
            } else {
                houseDarkNow_ = true;
            }
            break;
        case EventType::FloorCount: countT_ = t; break;
        default: break;
    }
}

}  // namespace rf
