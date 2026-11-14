// The view of M3 stage B: the phrase cycle on screen (the Build drains and zooms in, the Drop snaps out
// with a shockwave, the Break cools), the Trio (design/screens.md), the Riffs, the Boosts and Kaleidos
// (design/bosses/kaleidos.md). Cosmetic only; nothing here touches the simulation.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "content/icons.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
constexpr float kArmStep = kTau / 6.0f;
// Numbers chosen for the view (docs/PROGRESS.md M3 stage B): the look bible gives none for these.
constexpr float kBuildDrain = 0.45f;       // the Build drains colour toward white, up to this
constexpr float kBuildZoomIn = 0.10f;      // ... and zooms in by this much
constexpr float kBuildLastBarExposure = 0.78f;  // its last bar darkens
constexpr float kDropZoomOut = 0.08f;      // the Drop snaps the zoom out by this, easing back over a bar
constexpr float kBreakDrain = 0.22f;       // the Break: cool, desaturated
constexpr float kTrioAnswerWu = 110.0f;    // screens.md (quoted): an orb answers within 110 wu
constexpr float kBoostOrbWu = 20.0f;       // a Boost lying in the arena: its icon frame radius

Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec2 lerpv(Vec2 a, Vec2 b, float t) { return a + (b - a) * t; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }

// OKLCH -> linear sRGB (kaleidos.json accent: lightness 0.83, hue 76, chroma as a fraction of ~0.16).
Vec3 oklch(float L, float hueDeg, float chroma) {
    const float h = hueDeg * kTau / 360.0f;
    const float a = chroma * std::cos(h), b = chroma * std::sin(h);
    const float l_ = L + 0.3963377774f * a + 0.2158037573f * b;
    const float m_ = L - 0.1055613458f * a - 0.0638541728f * b;
    const float s_ = L - 0.0894841775f * a - 1.2914855480f * b;
    const float l = l_ * l_ * l_, m = m_ * m_ * m_, s = s_ * s_ * s_;
    return Vec3{std::fmax(4.0767416621f * l - 3.3077115913f * m + 0.2309699292f * s, 0.0f),
                std::fmax(-1.2684380046f * l + 2.6097574011f * m - 0.3413193965f * s, 0.0f),
                std::fmax(-0.0041960863f * l - 0.7034186147f * m + 1.7076147010f * s, 0.0f)};
}
}  // namespace

void PlayView::sectionLook(const ViewInput& in) {
    const World& w = *in.world;
    const Phrase& ph = w.phrase();
    const double barStart = w.tickOfBar(ph.bar);
    const float inBar = clamp01(static_cast<float>((in.music - barStart) / kTicksPerBar));
    const float u = ph.sectionBars ? clamp01((static_cast<float>(ph.sectionBar) + inBar) / static_cast<float>(ph.sectionBars)) : 0.0f;
    float drain = 0.0f, zoom = 1.0f;
    exposureSection_ = 1.0f;
    switch (ph.section) {
        case Section::Build:
            // GAME_DESIGN 4.1: colours drain toward white, slow zoom-in; the last bar darkens.
            drain = kBuildDrain * u;
            zoom = 1.0f + kBuildZoomIn * u;
            if (ph.buildLastBar()) exposureSection_ = kBuildLastBarExposure;
            break;
        case Section::Drop:
            if (!ph.bossDrop || ph.sectionBar < 2) {
                // The zoom snaps out on the downbeat and eases back.
                const float since = static_cast<float>(ph.sectionBar) + inBar;
                zoom = 1.0f - kDropZoomOut * std::exp(-since * 2.5f);
            }
            break;
        case Section::Break: drain = kBreakDrain; break;
        default: break;
    }
    if (reduceFlashing_) drain *= 0.5f;
    const float k = 1.0f - std::exp(-in.dt / 0.35f);
    drain_ += (drain - drain_) * k;
    if (ph.section == Section::Drop && zoom < sectionZoom_) {
        sectionZoom_ = zoom;  // the snap
    } else {
        sectionZoom_ += (zoom - sectionZoom_) * k;
    }
    // The Trio: the fight steps back (screens.md): the lattice rests, enemies at a fifth of their light. M4 (look.md 6):
    // all down drains the lattice to the same rest. Play test 2: a level-up Trio does not dim the fight.
    const bool trio = w.trio(w.viewPlayer()).open && w.trio(w.viewPlayer()).kind == kTrioBreak;
    const bool allDown = allDownT_ > -50.0f;
    trioRest_ += (((trio || allDown) ? 1.0f : 0.0f) - trioRest_) * (1.0f - std::exp(-in.dt / 0.2f));
    enemyLight_ = 1.0f - 0.8f * (allDown ? 0.0f : trioRest_);
}

void PlayView::eventB(const World& w, const Event& e, float t) {
    // M4 (look.md 6): a player's effects in that player's colours; another player's Trio at a third.
    const bool byPlayer = e.type == EventType::TrioTake || e.type == EventType::TrioOpen || e.type == EventType::TrioPop;
    const u32 owner = byPlayer && e.a < kMaxPlayers ? e.a : w.viewPlayer();
    const float own = owner == w.viewPlayer() ? 1.0f : (1.0f / 3.0f);
    const Vec3 player = palettes_->player[owner] * own, light = palettes_->playerLight[owner] * own;
    const Vec2 at{e.x, e.y};
    switch (e.type) {
        case EventType::Section:
            if (e.a == static_cast<u8>(Section::Drop)) {
                // GAME_DESIGN 4.1: bloom burst, a shockwave from every player (in its colours), the zoom snaps out.
                for (u32 q = 0; q < kMaxPlayers; ++q) {
                    const PlayerState& pq = w.player(q);
                    if (!pq.present || !pq.alive || (q != w.viewPlayer() && w.playerCount() < 2)) continue;
                    if (w.playerCount() < 2 && q != w.viewPlayer()) continue;
                    addShockRings(pq.pos, 2, 900.0f, palettes_->player[q], palettes_->playerLight[q], t, 0.45f);
                    push(kImpDropWave, pq.pos, 600.0f, 400.0f, t);
                }
                if (!reduceFlashing_) novaPump_ = std::fmax(novaPump_, 0.5f);
            }
            break;
        case EventType::TrioTake: {
            // The orb folds into the ship in two beats of sparks in its two colours.
            SparkReq r{};
            r.at = at;
            r.dir = Vec2{};
            r.count = 90;
            r.kind = 6;  // inhale: toward the ship
            r.a = player;
            r.b = light;
            r.t0 = t;
            r.extra = 2.0f * 60.0f / std::fmax(w.bpm(), 60.0f);
            sparkRequest(r);
            addPool(at, 30.0f, 90.0f, player, 1.6f, 0.0f, 0.5f, false, 1, false, false, t);
            break;
        }
        case EventType::TrioPop: {
            SparkReq r{};
            r.at = at;
            r.count = static_cast<u16>(40.0f * own);
            r.kind = 1;
            const u8 u = e.b;
            r.a = (isRiff(u) ? palette_->hue[1] : palette_->accent) * own;
            r.b = whiteV() * own;
            r.t0 = t;
            sparkRequest(r);
            break;
        }
        case EventType::BoostTake: {
            addShockRings(at, 1, 160.0f, palettes_->player[3], whiteV(), t, 0.3f);
            addPool(at, 20.0f, 120.0f, palettes_->player[3], 1.4f, 0.0f, 0.4f, false, 1, false, false, t);
            break;
        }
        case EventType::HaloBlock:
            addShockRings(at, 1, 120.0f, palettes_->player[3], whiteV(), t, 0.25f);
            break;
        case EventType::Leave:
            // M4 stage B: the ship of a player that left fades where it was (its seat opens; the run goes on).
            if (e.a < kMaxPlayers) {
                leaveT_[e.a] = t;
                leavePos_[e.a] = at;
                leaveHeading_[e.a] = e.f;
                addShockRings(at, 1, 160.0f, palettes_->player[e.a] * 0.7f, whiteV() * 0.5f, t, 0.5f);
            }
            break;
        case EventType::CritBurst: {
            // On-Beat Crit (play test 2, look.md 5): the beat shot's hit bursts in a gold ring of 80 wu.
            const u32 q = e.a < kMaxPlayers ? e.a : w.viewPlayer();
            const float lvl = q == w.viewPlayer() ? 1.0f : 0.6f;
            addShockRings(at, 1, e.f, buildGold_ * (1.4f * lvl), whiteV() * lvl, t, 0.16f);
            break;
        }
        case EventType::CellTake:
            // Play test 2 (look.md 6): a Cell taken: the halo's missing arc draws itself in over a quarter beat (drawn with
            // the ship) and eight gold sparks. Regen's arc becomes a cell quietly.
            if (e.a == w.viewPlayer() && e.c == 0) {
                cellBackT_ = t;
                cellBackTo_ = e.b;
                SparkReq r{};
                r.at = w.player(e.a).pos;
                r.count = 8;
                r.kind = 0;
                r.a = palettes_->player[3];
                r.b = whiteV();
                r.t0 = t;
                sparkRequest(r);
            }
            break;
        case EventType::CellDrop:
            if (e.a == w.viewPlayer()) addPool(at, 10.0f, 60.0f, palettes_->player[3], 1.2f, 0.0f, 0.3f, false, 1, false, false, t);
            break;
        case EventType::HaloBurn:
            // GAME_DESIGN 8 (the halo burns down): the lost arcs shatter outward on the beat; no panic ring.
            if (e.a < kMaxPlayers && e.c > e.b)
                shatterArcs(at, e.c < 8 ? e.c : 8u, e.b, e.c < 8 ? e.c : 8u, mix(palettes_->player[e.a], whiteV(), 0.3f) * 2.0f, t);
            break;
        case EventType::BossPair: {
            if (static_cast<BossKind>(e.c) != BossKind::Kaleidos) {
                eventFight(w, e, t);
                break;
            }
            // kaleidos.md: both arms shatter outward along their axis in mirror image; the lens plates
            // crack and fall inward; the wheel flinches.
            const Vec3 hue = palette_->hue[e.a % 3], partner = palette_->partner[e.a % 3];
            for (u32 m = 0; m < 2; ++m) {
                const Vec2 d = dirv(e.f + static_cast<float>(m) * kPi);
                for (u32 k = 0; k < 5; ++k) {
                    SparkReq r{};
                    r.at = at + d * (100.0f + 40.0f * static_cast<float>(k));
                    r.dir = d;
                    r.count = 26;
                    r.kind = 1;
                    r.a = hue;
                    r.b = partner;
                    r.t0 = t;
                    sparkRequest(r);
                }
                addShockRings(at + d * 264.0f, 1, 140.0f, hue, partner, t, 0.3f);
            }
            addDistortion(at, 420.0f, 0.35f, 14.0f, 18.0f, t);
            shakeAmp_ = std::fmax(shakeAmp_, 4.0f);
            break;
        }
        case EventType::BossDeath: {
            if (static_cast<BossKind>(e.a) != BossKind::Kaleidos) {
                eventFight(w, e, t);
                break;
            }
            // Iris wide open, the clamped white flash, facets tumbling outward in six-fold symmetry, three
            // rings, 300 sparks, the 30 px distortion ring; Play holds the action 50 ms. M3 stage C: the white
            // flash is clamped (0.9, like the M1 boss break) and peaks for 0.12 s; the accent glow stays after it.
            // (the owner, 2026-11-14: "a little bit for epic deaths": the boss's fall, look.md §5, under the facets)
            const BossData& b = w.bossData();
            const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
            bossFall(at, acc, palette_->hue[2], t);
            addPool(at, 120.0f, 520.0f, acc, reduceFlashing_ ? 0.3f : 0.6f, 0.0f, 0.45f, false, 1, false, false, t);
            const float wheel = w.boss().angle;
            for (u32 k = 0; k < 6; ++k) {
                for (u32 f = 0; f < 4; ++f) {
                    if (tumbleCount_ >= kTumbles) break;
                    Tumble& tb = tumble_[tumbleCount_++];
                    const float a = wheel + static_cast<float>(k) * kArmStep;
                    tb.p = at + dirv(a) * b.facets[f].armRadius;
                    tb.v = dirv(a) * (260.0f + 120.0f * static_cast<float>(f)) + dirv(a + 1.5708f) * rng_.range(-60.0f, 60.0f);
                    tb.ang = a;
                    tb.spin = rng_.range(-6.0f, 6.0f);
                    tb.facet = static_cast<u8>(f);
                    tb.pair = static_cast<u8>(k % 3);
                    tb.born = t;
                }
            }
            break;
        }
        case EventType::BossCue:
        case EventType::BossGone:
        case EventType::BossEnter:
        case EventType::Telegraph:
            eventFight(w, e, t);
            break;
        case EventType::Impact:
            if (e.species == kBossSpecies && w.boss().kind != BossKind::Kaleidos) {
                eventFight(w, e, t);
            } else if (e.species == kBossSpecies) {
                // Play test 3 (GAME_DESIGN 7.6): seen as every fight's hits are (play_view_m5.cpp bossHit).
                const BossData& b = w.bossData();
                const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
                bossHit(w, e, t, e.c == 2 ? palette_->hue[e.a % 3] : acc);
            }
            break;
        case EventType::PulseRing: break;
        case EventType::ExtraLife:
            // Play test 3 (screens.md): EXTRA LIFE at the ship for a bar, picked up or spent; spent, a gold burst.
            if (e.a < kMaxPlayers) extraLifeT_[e.a] = t;
            if (e.b == 1) {
                SparkReq r{};
                r.at = at;
                r.count = 24;
                r.kind = 5;
                r.a = palettes_->player[3];
                r.b = whiteV();
                r.t0 = t;
                sparkRequest(r);
            }
            break;
        case EventType::Upgrade: eventUpgrade(w, e, t); break;  // M5 part 3
        case EventType::TrailBlast:  // M5 part 4: a Weaver C trail blows
            addShockRings(at, 1, w.species().family(Family::Weaver).v.trailBlastWu, palettes_->danger, whiteV(), t, 0.2f);
            break;
        case EventType::Restart: tumbleCount_ = 0; break;
        default: break;
    }
}

void PlayView::ringBreak(const World& w, u32 p, Vec2 at, float t) {
    // Play test 3 (GAME_DESIGN 6.3 row 1): the guard ring takes the hit; its shards fly outward (three sparks each, in the
    // Riff's colour) and go dark.
    if (p >= kMaxPlayers) return;
    const PlayerState& pl = w.player(p);
    const u32 n = orbitalCount(pl.loadout.riffs[riffIndex(kOrbitals)]);
    const double ph = std::fmod(w.music() - w.director().startTick, kTicksPerBar) / kTicksPerBar;
    for (u32 k = 0; k < n && k < 16; ++k) {
        const float a = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(n) - 0.25f * kTau;
        SparkReq r{};
        r.at = at + dirv(a) * tuning::kOrbitalRadiusWu;
        r.dir = dirv(a);
        r.count = 3;
        r.kind = 3;
        r.a = palettes_->playerLight[p];
        r.b = whiteV();
        r.t0 = t;
        sparkRequest(r);
    }
}

void PlayView::drawStageB(FrameDraw& d, const ViewInput& in, float W, float H) {
    const World& w = *in.world;
    const float t = static_cast<float>(in.time);
    const float a = in.alpha;
    const Vec3 W1 = whiteV();
    const Vec3 player = palettes_->player[w.viewPlayer()], light = palettes_->playerLight[w.viewPlayer()], deep = palettes_->playerDeep[w.viewPlayer()];
    const Vec3 gold = palettes_->player[3];
    const Vec3 accent = palette_->accent;
    const PlayerState& me = w.player(w.viewPlayer());
    const float swell = 1.0f + 0.06f * beat_;  // screens.md: orbs swell 6% on the beat (M5: the clock's beat)
    (void)W;
    (void)H;
    (void)deep;

    // ---- Boosts lying in the arena: the icon in its frame, a gold glow; they fade over their last bar.
    {
        const BoostPickups& bp = w.boostPickups();
        for (u32 i = 0; i < bp.count; ++i) {
            if (bp.dead[i] || (w.playerCount() > 1 && bp.owner[i] != w.viewPlayer())) continue;  // M4: only your own
            const Vec2 p{bp.x[i], bp.y[i]};
            const float left = static_cast<float>((bp.born[i] + tuning::kBoostLifeBars * kTicksPerBar - in.music) / kTicksPerBar);
            const float fade = clamp01(left);
            if (!visible(p, 60.0f)) {
                edgeChevron(d, p, gold, false, 1.0f);  // look.md §6: gold for events
                continue;
            }
            const Vec2 sp = toScene(p);
            const float r = kBoostOrbWu * pxPerWu_ * (1.0f + 0.08f * beat_);
            if (bp.type[i] == kPickupExtraLife) {
                // Play test 3 (screens.md "Extra Life (HUD)"): a gold ship outline in a ring, the Boost orb's size.
                const float sc = kBoostOrbWu * 0.9f * (1.0f + 0.08f * beat_);
                glyph(d.glyphs, gShip_, true, p, -0.5f * kPi, -0.5f * kPi, sc, 2.0f * fade, 0.7f, kGlyphFlat, gold, gold);
                glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, kBoostOrbWu * 1.5f, 1.6f * fade, 0.55f, kGlyphFlat, gold, gold);
                d.pools.glow(sp, r * 2.6f, gold * (0.22f * fade));
                continue;
            }
            if (icons_) icons_->draw(d, boostIcon_[bp.type[i] % kBoostCount], sp, r, fade * 1.1f);
            glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, kBoostOrbWu * 1.5f, 1.4f * fade, 0.5f, kGlyphFlat, gold, W1);
            d.pools.glow(sp, r * 2.6f, gold * (0.18f * fade));
            // M3 stage C: its name under the orb, never across it: gold, cap 11 wu, 6 wu under the ring.
            const char* name = boostInfo(bp.type[i]).name;
            const float cap = 11.0f * pxPerWu_, top = sp.y + (kBoostOrbWu * 1.5f + 6.0f) * pxPerWu_;
            const Vec3 g = gold * (1.3f * fade);
            font_->drawField(d.scene, Vec2{sp.x - 0.5f * font_->measure(TextStyle{cap}, name), top}, cap, 0.8f * refPx_, g, g, g, name);
        }
    }

    // ---- Play test 2 (look.md 6): Cells, this player's own. A gold ring of 16 wu with a white plus inside (the only plus
    // in the game), swelling 15% on the beat, with a thin vertical flare 120 px tall so it can be found in a crowd. Off
    // screen while its player is on the last cell: a gold chevron with a plus at its tip.
    {
        const CellPickups& cp = w.cellPickups();
        for (u32 i = 0; i < cp.count; ++i) {
            if (cp.dead[i] || cp.owner[i] != w.viewPlayer()) continue;
            const Vec2 p = lerpv(Vec2{cp.px[i], cp.py[i]}, Vec2{cp.x[i], cp.y[i]}, a);
            const float left = static_cast<float>((cp.born[i] + tuning::kCellLifeBars * kTicksPerBar - in.music) / kTicksPerBar);
            const float fade = clamp01(left);
            if (!visible(p, 70.0f)) {
                if (me.alive && me.cells <= 1) edgeChevron(d, p, gold, false, 1.0f + 0.2f * beat_, 2);
                continue;
            }
            const float r = tuning::kCellRadiusWu * (1.0f + 0.15f * beat_);
            const Vec2 sp = toScene(p);
            glyph(d.glyphs, gRing32_, false, p, 0.0f, 0.0f, r, 1.7f * fade, 0.75f, kGlyphFlat, gold, gold);
            const float arm = 0.55f * r;
            const Vec3 plus = W1 * (2.0f * fade);
            d.scene.line(toScene(p + Vec2{-arm, 0.0f}), toScene(p + Vec2{arm, 0.0f}), 1.1f * refPx_, plus, plus, 0);
            d.scene.line(toScene(p + Vec2{0.0f, -arm}), toScene(p + Vec2{0.0f, arm}), 1.1f * refPx_, plus, plus, 0);
            flare(d.scene, p, 0.5f * kPi, 60.0f * refPx_ / pxPerWu_, 1, gold * (2.2f * fade), 0.5f);
            d.pools.glow(sp, r * 2.4f * pxPerWu_, gold * (0.22f * fade));
        }
    }

    // ---- Every player's Riffs, in that player's colours (look.md §6; M4: the teammates' too).
    for (u32 q = 0; q < kMaxPlayers; ++q) {
        const PlayerState& pq = w.player(q);
        if (!pq.present || !pq.alive || (q != w.viewPlayer() && w.playerCount() < 2)) continue;
        const bool local = q == w.viewPlayer();
        const Vec3 player = palettes_->player[q], light = palettes_->playerLight[q];
        const Vec2 ship = local ? in.shipPos : lerpv(pq.prevPos, pq.pos, a);
        const Loadout& l = pq.loadout;
        const bool hyper = pq.boost(kHyper);
        // Play test 2 (look.md 5): what a Riff throws is its owner's base colour at levels 1 and 2, the light colour at 3
        // and 4, and a white core with the player's rim at 5.
        const auto riffColour = [&](u32 lv) { return lv >= 5 ? W1 : (lv >= 3 ? light : player); };
        {
            // Play test 3 (screens.md "Extra Life (HUD)"): EXTRA LIFE as world text at the ship (cap 22, gold) for one bar.
            const float since = static_cast<float>(in.time) - extraLifeT_[q];
            const float barS = 240.0f / std::fmax(in.bpm, 30.0f);
            if (since >= 0.0f && since < barS) {
                const char* words = "EXTRA LIFE";
                const float cap = 22.0f * pxPerWu_;
                const float fade = clamp01((barS - since) / (0.25f * barS));
                const Vec2 sc = toScene(ship);
                const Vec3 g = gold * (1.8f * fade);
                font_->drawField(d.scene, Vec2{sc.x - 0.5f * font_->measure(TextStyle{cap}, words), sc.y - 56.0f * pxPerWu_ - cap}, cap, 0.9f * refPx_, g, g, g,
                                 words);
            }
        }
        if (const u8 lv = l.riffs[riffIndex(kOrbitals)]) {
            const u32 n = orbitalCount(lv);
            const double turn = hyper ? kTicksPerBar * 0.5 : kTicksPerBar;
            const double ph = std::fmod(in.music - w.director().startTick, turn) / turn;
            // Play test 3 (GAME_DESIGN 6.3 row 1): each shard 14 wu across; a broken ring's shards are dark (a faint outline)
            // until they relight one by one; while every shard is lit a faint ring joins them (it guards).
            const float shardScale = tuning::kOrbitalHitWu;
            const Vec3 c = riffColour(lv);
            const u32 lit = w.orbitalsLit(q, in.music);
            if (lit >= n && w.ringGuards(q)) {
                for (u32 k = 0; k < 48; ++k) {
                    const float a0 = kTau * static_cast<float>(k) / 48.0f, a1 = kTau * static_cast<float>(k + 1) / 48.0f;
                    if (k % 2u) continue;
                    d.scene.line(toScene(ship + dirv(a0) * tuning::kOrbitalRadiusWu), toScene(ship + dirv(a1) * tuning::kOrbitalRadiusWu), 0.35f * refPx_,
                                 c * 0.45f, c * 0.45f, 0);
                }
            }
            for (u32 k = 0; k < n; ++k) {
                const float ang = static_cast<float>(ph) * kTau + kTau * static_cast<float>(k) / static_cast<float>(n) - 0.25f * kTau;
                const Vec2 o = ship + dirv(ang) * tuning::kOrbitalRadiusWu;
                if (k >= lit) {
                    glyph(d.glyphs, gShard_, false, o, ang * 2.0f, 0.0f, shardScale, 0.35f, 0.45f, kGlyphFlat, c * 0.5f, c * 0.5f);
                    continue;
                }
                if (lv >= 5) glyph(d.glyphs, gShard_, false, o, ang * 2.0f, 0.0f, shardScale, 2.0f, 1.1f, kGlyphFlat, player, player);
                glyph(d.glyphs, gShard_, false, o, ang * 2.0f, 0.0f, shardScale, 2.2f, 0.6f, kGlyphFlat, c, lv >= 3 ? c : player);
                d.sparkles.glow(toScene(o), 7.0f * refPx_, c * 0.6f);
            }
        }
        for (const PulseWave& pw : pq.pulses) {
            if (!pw.active || pw.age < 0.0f) continue;
            const float age = pw.age + a * in.dt;
            const float u = clamp01(age / tuning::kPulseGrowS);
            const float r = pw.reach * u;
            if (r < 4.0f) continue;
            const Vec3 c = riffColour(pw.level);
            const float wid = 0.6f + 0.6f * (1.0f - u);
            if (pw.level >= 5) glyph(d.glyphs, gRing128_, false, pw.centre, 0.0f, 0.0f, r, 1.6f * (1.0f - u * u), wid * 1.9f, kGlyphRingW, player, player);
            glyph(d.glyphs, gRing128_, false, pw.centre, 0.0f, 0.0f, r, 2.0f * (1.0f - u * u), wid, kGlyphRingW, c, pw.level >= 3 ? c : light);
        }
        if (const u8 lv = l.riffs[riffIndex(kSentinel)]) {
            for (u32 k = 0; k < sentinelDrones(lv) && k < kMaxDrones; ++k) {
                const Vec2 s = lerpv(pq.prevSentinel[k], pq.sentinel[k], a);
                glyph(d.glyphs, gMarkC_ >= 0 ? gMarkC_ : gRing32_, false, s, t * 2.0f, t * 2.0f, 8.0f, 2.4f, 0.6f, kGlyphFlat, light, player);
                glyph(d.glyphs, gRing32_, false, s, 0.0f, 0.0f, 11.0f, 1.2f, 0.5f, kGlyphFlat, player, player);
                d.sparkles.glow(toScene(s), 9.0f * refPx_, player * 0.7f);
            }
        }
        // Halo: a gold ring around the ship, one arc per hit it can still take.
        for (const ActiveBoost& b : pq.boosts) {
            if (b.type != kHalo || !b.hits) continue;
            const float r = 30.0f;
            for (u32 k = 0; k < b.hits; ++k) {
                const float a0 = -0.5f * kPi + static_cast<float>(k) * kTau / 3.0f + 0.12f, a1 = a0 + kTau / 3.0f - 0.24f;
                const u32 segs = 8;
                for (u32 s = 0; s < segs; ++s) {
                    const float u0 = a0 + (a1 - a0) * static_cast<float>(s) / segs, u1 = a0 + (a1 - a0) * static_cast<float>(s + 1) / segs;
                    d.scene.line(toScene(ship + dirv(u0) * r), toScene(ship + dirv(u1) * r), 1.3f * refPx_, gold * 2.2f, W1 * 1.6f, 0);
                }
            }
        }
    }

    drawUpgrades(d, in);  // M5 part 3: the rest of the Mods, Riffs, Boosts and the Fusions
    drawHazards(d, in);   // M5 part 4: the lethal lines and the Wells

    // ---- Kaleidos.
    drawFight(d, in);  // M5: the fight of the moment (Kaleidos is drawBoss), and the Pen

    // ---- The Trio (screens.md): the ring is the clock; three orbs with icon, name, effect words, pips. Play test 1:
    // locked for two beats (the ring draws itself in one, the orbs fade in as outlines over the next, their words
    // already readable), lit on the third; the orb the ship stays on fills as a solid arc over two beats.
    const TrioState& tr = w.trio(w.viewPlayer());
    if (tr.open && me.alive && icons_ && font_) {
        const float span = static_cast<float>(tr.closeTick - tr.openTick);
        const float left = span > 1.0f ? clamp01(static_cast<float>((tr.closeTick - in.music) / span)) : 0.0f;
        const float beats = static_cast<float>((in.music - tr.openTick) / kTicksPerBeat);
        const float drawn = clamp01(beats);                 // the ring draws itself over the first beat
        const float fadeIn = clamp01(beats - 1.0f);         // the orbs fade in over the second
        const bool locked = beats < static_cast<float>(tuning::kTrioLockBeats);
        const float lightUp = locked ? 0.0f : clamp01(1.0f - (beats - static_cast<float>(tuning::kTrioLockBeats)) * 3.0f);  // the third beat's flash
        const Vec2 c = tr.centre;
        const float R = tuning::kTrioRingWu;
        const u32 segs = 96;
        // Play test 2 (screens.md, a level-up Trio): the ring is gold instead of the accent, LEVEL n sits at its centre for
        // the first bar, each orb's thin ring is drawn double (a ship inside it cannot be hurt).
        const bool levelTrio = tr.kind == kTrioLevel;
        const Vec3 ringHue = levelTrio ? gold : accent;
        for (u32 s = 0; s < segs; ++s) {
            const float u0 = static_cast<float>(s) / segs, u1 = static_cast<float>(s + 1) / segs;
            if (u0 >= drawn) break;
            const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + u1 * kTau;
            const bool lit = u0 < left;
            const Vec3 col = lit ? mix(ringHue, W1, 0.5f + 0.5f * std::sin(u0 * kTau * 2.0f)) * 1.3f : W1 * 0.16f;
            d.scene.line(toScene(c + dirv(a0) * R), toScene(c + dirv(a1) * R), (lit ? 1.0f : 0.7f) * refPx_, col, col, 0);
        }
        if (levelTrio && tr.levelShown && in.music - tr.openTick < kTicksPerBar) {
            char text[24];
            std::snprintf(text, sizeof text, "LEVEL %u", static_cast<u32>(tr.levelShown));
            const float cap = 16.0f * pxPerWu_;
            const float wText = font_->measure(TextStyle{cap}, text);
            const Vec2 sc = toScene(c);
            const float fadeOut = clamp01(static_cast<float>((tr.openTick + kTicksPerBar - in.music) / kTicksPerBeat));
            const Vec3 g = gold * (1.8f * fadeOut);
            font_->drawField(d.scene, Vec2{sc.x - 0.5f * wText, sc.y - 0.5f * cap}, cap, 0.9f * refPx_, g, g, g, text);
        }
        if (left > 0.0f && drawn >= 1.0f) {
            const Vec2 bead = c + dirv(-0.5f * kPi + left * kTau) * R;
            d.sparkles.glow(toScene(bead), 8.0f * refPx_, W1 * 1.6f);
        }
        // Which orb answers: the one being held, else the nearest within 110 wu, or the one the ship flies at (lit only).
        i32 answer = -1;
        float best = kTrioAnswerWu;
        const Vec2 ship = in.shipPos;
        for (u32 k = 0; k < tr.orbs && !locked; ++k) {
            const float dd = length(tr.orbPos[k] - ship);
            if (dd < best) {
                best = dd;
                answer = static_cast<i32>(k);
            }
        }
        if (!locked && answer < 0 && length(in.shipVel) > 120.0f) {
            const Vec2 v = normalizeOr(in.shipVel, Vec2{1.0f, 0.0f});
            float bestCos = 0.97f;
            for (u32 k = 0; k < tr.orbs; ++k) {
                const float cs = dot(normalizeOr(tr.orbPos[k] - ship, v), v);
                if (cs > bestCos) {
                    bestCos = cs;
                    answer = static_cast<i32>(k);
                }
            }
        }
        if (tr.holdOrb >= 0) answer = tr.holdOrb;
        if (tr.homing >= 0) answer = tr.homing;
        const float holdFill = clamp01(static_cast<float>(tr.holdTicks / (tuning::kTrioHoldBeats * kTicksPerBeat)));
        for (u32 k = 0; k < tr.orbs; ++k) {
            const u8 u = tr.upgrade[k];
            const bool riff = isRiff(u);
            const bool ans = static_cast<i32>(k) == answer;
            const Vec2 p = tr.orbPos[k];
            if (fadeIn <= 0.0f) continue;
            const float orbWu = tuning::kTrioOrbWu * (riff ? tuning::kTrioRiffScale : 1.0f) * (ans ? 1.12f : 1.0f) * swell;
            const Vec2 sp = toScene(p);
            // Locked: outlines at 0.5, fading in; lit: the icon at its level (the third beat flashes it). Play test 2: a Boost
            // orb (every Mod and Riff at its cap) shows its Boost.
            const bool boostOrb = isBoostOrb(u);
            const i32 icon = orbIcon(u);
            const float iconLevel = locked ? 0.5f * fadeIn : (ans ? 1.35f : 1.0f) + 0.8f * lightUp;
            icons_->draw(d, icon, sp, orbWu * pxPerWu_ / icons_->size(icon) * (riff ? tuning::kTrioRiffScale : 1.0f), iconLevel);
            if (lightUp > 0.0f) d.pools.glow(sp, orbWu * 2.6f * pxPerWu_, W1 * (0.25f * lightUp));
            // The thin ring at 1.42x; answering: in the player colour at 2.0 with a dashed outer ring and a pool. A level-up
            // Trio's thin ring is drawn double (the shelter), the second just outside it in gold.
            const float ringWu = tuning::kTrioHoldRing * orbWu;
            const u32 n = 64;
            for (u32 s = 0; s < n; ++s) {
                const float a0 = kTau * static_cast<float>(s) / n, a1 = kTau * static_cast<float>(s + 1) / n;
                const Vec3 col = locked ? W1 * (0.35f * fadeIn) : (ans ? player * 2.0f : W1 * 0.7f);
                d.scene.line(toScene(p + dirv(a0) * ringWu), toScene(p + dirv(a1) * ringWu), (ans ? 1.2f : 0.6f) * refPx_, col, col, 0);
                if (levelTrio) {
                    const float r3 = ringWu + 5.0f;
                    const Vec3 gc = gold * (locked ? 0.5f * fadeIn : 1.2f);
                    d.scene.line(toScene(p + dirv(a0) * r3), toScene(p + dirv(a1) * r3), 0.6f * refPx_, gc, gc, 0);
                }
                if (ans && s % 2 == 0) {
                    const float r2 = ringWu * (levelTrio ? 1.3f : 1.18f);
                    d.scene.line(toScene(p + dirv(a0) * r2), toScene(p + dirv(a1) * r2), 0.8f * refPx_, player * 1.4f, player * 1.4f, 0);
                }
            }
            // Holding: the thin ring fills as a solid arc in the player colour, clockwise from twelve o'clock.
            if (static_cast<i32>(k) == tr.holdOrb && holdFill > 0.0f) {
                const u32 arc = 64;
                for (u32 s = 0; s < arc; ++s) {
                    const float u0 = static_cast<float>(s) / arc;
                    if (u0 >= holdFill) break;
                    const float u1 = std::fmin(static_cast<float>(s + 1) / arc, holdFill);
                    const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + u1 * kTau;
                    d.scene.line(toScene(p + dirv(a0) * ringWu), toScene(p + dirv(a1) * ringWu), 3.2f * refPx_, player * 2.4f, light * 2.0f, 0);
                }
                const Vec2 tip = p + dirv(-0.5f * kPi + holdFill * kTau) * ringWu;
                d.sparkles.glow(toScene(tip), 10.0f * refPx_, light * 1.8f);
            }
            if (ans && !locked) {
                d.pools.glow(sp, ringWu * 2.2f * pxPerWu_, player * 0.22f);
                d.scene.line(toScene(ship), toScene(p - normalizeOr(p - ship, Vec2{1.0f, 0.0f}) * ringWu), 1.0f * refPx_, player * 0.4f,
                             light * 1.6f, 0);
            }
            // Words, always outside the ring: above the top orb, left of the left one, right of the right one.
            const UpgradeInfo& info = upgradeInfo(u);
            const bool fusionOrb = isFusionOrb(u);  // M5: a Fusion's gold orb
            const char* name = boostOrb ? boostInfo(boostOfOrb(u)).name : (fusionOrb ? fusionInfo(fusionOfOrb(u)).name : info.name);
            const char* words = boostOrb ? boostInfo(boostOfOrb(u)).words : (fusionOrb ? fusionInfo(fusionOfOrb(u)).words : upgradeWords(u, tr.level[k]));
            const float capName = 20.0f * pxPerWu_, capWords = 11.0f * pxPerWu_;
            const float wName = font_->measure(TextStyle{capName}, name), wWords = font_->measure(TextStyle{capWords}, words);
            const float gap = (ringWu + 14.0f) * pxPerWu_;
            Vec2 org;  // the name's baseline, left end
            const float lift = (ans ? 1.9f : 1.3f) * (locked ? fadeIn : 1.0f);
            float align = 0.0f;  // 0 left-aligned at org.x, 1 right-aligned, 0.5 centred
            if (k == 0) {
                org = Vec2{sp.x, sp.y - gap - capWords - 18.0f * refPx_};
                align = 0.5f;
            } else if (k == 1) {
                org = Vec2{sp.x - gap, sp.y - 4.0f * refPx_};
                align = 1.0f;
            } else {
                org = Vec2{sp.x + gap, sp.y - 4.0f * refPx_};
                align = 0.0f;
            }
            const Vec3 nameCol = W1 * lift, wordCol = accent * ((ans ? 1.9f : 1.3f) * (locked ? fadeIn : 1.0f));
            // (The font's origin is the top of its caps; org and wy are baselines.)
            font_->drawField(d.scene, Vec2{org.x - align * wName, org.y - capName}, capName, 0.9f * refPx_, nameCol, nameCol, nameCol, name);
            const float wy = org.y + capWords + 12.0f * refPx_;
            font_->drawField(d.scene, Vec2{org.x - align * wWords, wy - capWords}, capWords, 0.7f * refPx_, wordCol, wordCol, wordCol, words);
            // Pips: one per stack or level the upgrade can have; owned white, this pick gold with a ring (a Boost has none).
            const u32 cap = (boostOrb || isFusionOrb(u)) ? 0u : info.cap;
            if (!cap) continue;
            const float pip = 5.0f * refPx_, step = 15.0f * refPx_;
            const float rowW = static_cast<float>(cap - 1) * step;
            const float px0 = org.x - align * rowW, py = wy + 16.0f * refPx_;
            const u32 owned = tr.level[k] > 0 ? tr.level[k] - 1u : 0u;
            for (u32 j = 0; j < cap; ++j) {
                const Vec2 q{px0 + static_cast<float>(j) * step, py};
                if (j < owned) {
                    d.sparkles.glow(q, pip, W1 * 1.2f);
                    d.scene.line(q, q, pip * 0.6f, W1 * 1.4f, W1 * 1.4f, 0);
                } else if (j == owned) {
                    d.scene.line(q, q, pip * 0.6f, gold * 2.0f, gold * 2.0f, 0);
                    for (u32 s = 0; s < 12; ++s) {
                        const float a0 = kTau * static_cast<float>(s) / 12.0f, a1 = kTau * static_cast<float>(s + 1) / 12.0f;
                        d.scene.line(q + dirv(a0) * pip * 1.7f, q + dirv(a1) * pip * 1.7f, 0.5f * refPx_, gold * 1.4f, gold * 1.4f, 0);
                    }
                } else {
                    for (u32 s = 0; s < 8; ++s) {
                        const float a0 = kTau * static_cast<float>(s) / 8.0f, a1 = kTau * static_cast<float>(s + 1) / 8.0f;
                        d.scene.line(q + dirv(a0) * pip, q + dirv(a1) * pip, 0.45f * refPx_, W1 * 0.5f, W1 * 0.5f, 0);
                    }
                }
            }
        }
    }

    // ---- M4 (screens.md, the Trio): another player's ring and orbs at a third, in its colour; they cannot be taken.
    if (icons_ && w.playerCount() > 1) {
        for (u32 q = 0; q < kMaxPlayers; ++q) {
            if (q == w.viewPlayer()) continue;
            const TrioState& ot = w.trio(q);
            const PlayerState& oq = w.player(q);
            if (!ot.open || !oq.present || !oq.alive) continue;
            const float third = 1.0f / 3.0f;
            const Vec3 pc = palettes_->player[q];
            const float span = static_cast<float>(ot.closeTick - ot.openTick);
            const float left = span > 1.0f ? clamp01(static_cast<float>((ot.closeTick - in.music) / span)) : 0.0f;
            const u32 segs = 72;
            for (u32 s = 0; s < segs; ++s) {
                const float u0 = static_cast<float>(s) / segs, u1 = static_cast<float>(s + 1) / segs;
                const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + u1 * kTau;
                const Vec3 col = (u0 < left ? pc * 1.3f : W1 * 0.16f) * third;
                d.scene.line(toScene(ot.centre + dirv(a0) * tuning::kTrioRingWu), toScene(ot.centre + dirv(a1) * tuning::kTrioRingWu),
                             0.8f * refPx_, col, col, 0);
            }
            for (u32 k = 0; k < ot.orbs; ++k) {
                const u8 u = ot.upgrade[k];
                if (!validOrb(u)) continue;
                const bool riff = isRiff(u);
                const float orbWu = tuning::kTrioOrbWu * (riff ? tuning::kTrioRiffScale : 1.0f) * swell;
                const Vec2 p = ot.orbPos[k];
                const i32 icon = orbIcon(u);
                icons_->draw(d, icon, toScene(p), orbWu * pxPerWu_ / icons_->size(icon) * (riff ? tuning::kTrioRiffScale : 1.0f), third);
                const float ringWu = 1.42f * orbWu;
                for (u32 s = 0; s < 48; ++s) {
                    const float a0 = kTau * static_cast<float>(s) / 48.0f, a1 = kTau * static_cast<float>(s + 1) / 48.0f;
                    d.scene.line(toScene(p + dirv(a0) * ringWu), toScene(p + dirv(a1) * ringWu), 0.6f * refPx_, pc * third, pc * third, 0);
                }
            }
        }
    }

    // ---- Facets of a dead boss, tumbling outward.
    for (u32 i = 0; i < tumbleCount_; ++i) {
        const Tumble& tb = tumble_[i];
        const float age = t - tb.born;
        if (age < 0.0f || age > 1.6f) continue;
        const BossData& b = w.bossData();
        const BossData::Facet& f = b.facets[tb.facet];
        const Vec2 p = tb.p + tb.v * age;
        const float ang = tb.ang + tb.spin * age;
        const float fade = 1.0f - age / 1.6f;
        const Vec3 col = mix(palette_->hue[tb.pair], W1, 0.4f) * (2.2f * fade);
        const float cs = std::cos(ang), sn = std::sin(ang);
        for (u32 s = 0; s < f.segs; ++s) {
            if (f.seg[s][4] != 0.0f) continue;
            const Vec2 p0 = p + Vec2{f.seg[s][0] * cs - f.seg[s][1] * sn, f.seg[s][0] * sn + f.seg[s][1] * cs};
            const Vec2 p1 = p + Vec2{f.seg[s][2] * cs - f.seg[s][3] * sn, f.seg[s][2] * sn + f.seg[s][3] * cs};
            d.scene.line(toScene(p0), toScene(p1), 1.2f * refPx_, col, col, 0);
        }
    }
}

void PlayView::drawBoss(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const BossState& s = w.boss();
    if (!s.active) return;
    const BossData& b = w.bossData();
    const float a = in.alpha;
    const Vec3 W1 = whiteV();
    const Vec3 acc = oklch(b.accentL, b.accentHue, b.accentChroma * 0.16f);
    const Vec2 pos = lerpv(s.prevPos, s.pos, a);
    const float wheel = s.prevAngle + wrapAngle(s.angle - s.prevAngle) * a;
    if (!visible(pos, b.radius * 0.5f) && !s.dying) {
        // Edge indicator (look.md §6): the boss's double chevron in its accent, swelling on every beat.
        const float tps = std::fmax(in.bpm, 60.0f) * static_cast<float>(kTicksPerBeat) / 60.0f;
        const float sinceBeat = static_cast<float>(std::fmod(in.music, kTicksPerBeat)) / tps;
        const float beat = std::exp(-sinceBeat * 1000.0f / std::fmax(look_->kickDecayMs, 1.0f));
        edgeChevron(d, pos, acc, true, 1.0f + look_->edge.bossBeatSwell * beat);
    }
    if (!visible(pos, b.radius * 2.6f)) return;
    const float tEnter = static_cast<float>((in.music - s.enterTick) / kTicksPerBar);  // bars since the entrance began
    const double barStart = std::floor(in.music / kTicksPerBar) * kTicksPerBar;
    const bool downbeat = in.music - barStart < kTicksPerBeat && in.music >= s.fightTick && !s.dying;
    const float dying = s.dying ? clamp01(static_cast<float>((in.music - s.deathTick) / (kTicksPerBar * 0.25))) : 0.0f;
    if (dying >= 1.0f) return;
    const float life = 1.0f - dying;
    const float hw = 0.6f * refPx_;

    // One arm's shape at angle `ang`, scaled `k` (reflections), with its colours.
    const auto arm = [&](u32 pair, float ang, float k, float level, bool hullOnly, bool resting, float enter) {
        const Vec3 hue = palette_->hue[pair % 3], partner = palette_->partner[pair % 3];
        const Vec2 dv = dirv(ang), nv{-dv.y, dv.x};
        for (u32 f = 0; f < 5; ++f) {
            const BossData::Facet& fc = f < 4 ? b.facets[f] : b.tip;
            // The entrance: inner to outer, each facet streaks in along its axis and locks.
            const float lockAt = 0.15f * static_cast<float>(f);
            const float u = clamp01((enter - lockAt) / 0.35f);
            if (enter < 1.0f && u <= 0.0f) continue;
            const float out = (1.0f - u) * (1.0f - u) * 900.0f;
            const float along = (fc.armRadius + out) * k;
            const float g = clamp01((fc.armRadius - 78.0f) / (274.0f - 78.0f));
            const Vec3 base = mix(mix(hue, W1, 0.5f), partner, g);
            for (u32 sg = 0; sg < fc.segs; ++sg) {
                const u32 layer = static_cast<u32>(fc.seg[sg][4]);
                if (hullOnly && layer != 0) continue;
                const Vec2 p0 = pos + dv * (along + fc.seg[sg][0] * k) + nv * (fc.seg[sg][1] * k);
                const Vec2 p1 = pos + dv * (along + fc.seg[sg][2] * k) + nv * (fc.seg[sg][3] * k);
                Vec3 col = f == 4 || layer == 2 ? acc : base;
                float in0 = level * (layer == 0 ? 2.0f : 1.4f);
                if (resting && layer == 0) {
                    col = W1;
                    in0 = level * 2.4f;
                }
                d.scene.line(toScene(p0), toScene(p1), hw * (layer == 0 ? 1.3f : 1.0f), col * in0, col * in0, 0);
            }
            // Glass: the facet's kite filled at a low level (white-hot: 0.16).
            if (!hullOnly && f < 4 && fc.segs >= 4) {
                const Vec2 cpos = pos + dv * along;
                const Vec2 q0 = cpos - dv * (fc.length * 0.5f * k), q1 = cpos + nv * (fc.halfWidth * k), q2 = cpos + dv * (fc.length * 0.5f * k),
                           q3 = cpos - nv * (fc.halfWidth * k);
                const Vec3 gl = (resting ? W1 * 0.16f : hue * 0.085f) * level;
                d.glass.tri(toScene(q0), gl, toScene(q1), gl, toScene(q2), gl);
                d.glass.tri(toScene(q0), gl, toScene(q2), gl, toScene(q3), gl);
            }
        }
        if (resting && !hullOnly) {
            // White-hot: a two-arm flare along the arm at the tip.
            const Vec2 tip = pos + dv * (b.tip.armRadius * k);
            flare(d.scene, tip, ang, 46.0f, 2, W1 * 2.2f, 0.8f);
            d.sparkles.glow(toScene(tip), 12.0f * refPx_, W1 * 0.9f);
        }
    };

    // Reflections behind the wheel: arm hulls only, larger and turned (the kaleidoscope).
    for (u32 r = 0; r < 2; ++r) {
        for (u32 k = 0; k < 6; ++k) {
            if (!(s.alive & (1u << (k % 3)))) continue;
            arm(k % 3, wheel + static_cast<float>(k) * kArmStep + b.reflectTurnDeg[r] * kTau / 360.0f, b.reflectScale[r],
                b.reflectIntensity[r] * 0.5f * life, true, false, tEnter);  // hull lines at the data's intensity
        }
    }
    // The arms, the resting pair white-hot.
    for (u32 k = 0; k < 6; ++k) {
        const u32 pr = k % 3;
        if (!(s.alive & (1u << pr))) continue;
        const bool rest = w.bossResting(pr, in.music);
        arm(pr, wheel + static_cast<float>(k) * kArmStep, 1.0f, life, false, rest, tEnter);
        // Beams: telegraphed for the bar before as a thin dashed line, then one beat along the arm to the wall.
        const u8 beam = w.bossBeamState(pr, in.music);
        if (beam) {
            const Vec2 dv = dirv(wheel + static_cast<float>(k) * kArmStep);
            const Vec2 from = pos + dv * b.tip.armRadius;
            float reach = 12000.0f;
            if (dv.x > 1e-4f) reach = std::fmin(reach, (kArenaHalfW - from.x) / dv.x);
            if (dv.x < -1e-4f) reach = std::fmin(reach, (-kArenaHalfW - from.x) / dv.x);
            if (dv.y > 1e-4f) reach = std::fmin(reach, (kArenaHalfH - from.y) / dv.y);
            if (dv.y < -1e-4f) reach = std::fmin(reach, (-kArenaHalfH - from.y) / dv.y);
            const Vec2 to = from + dv * reach;
            if (beam == 1) {
                telegraphLine(d, from, to, (std::floor(in.music / kTicksPerBar) + 1.0) * kTicksPerBar);  // (it fires on the next bar line)
            } else {
                lethalBeam(d, from, to, b.phases[1].beamWidth);  // look.md v2.5: 10 wu, the model
            }
        }
    }
    // Lens plates: two arcs per living pair on the ring around the core, centred on its arms.
    const float lensHalf = b.lensHalfAngleDeg * kTau / 360.0f;
    for (u32 k = 0; k < 6; ++k) {
        if (!(s.alive & (1u << (k % 3)))) continue;
        const float c = wheel + static_cast<float>(k) * kArmStep;
        const u32 n = 8;
        for (u32 j = 0; j < n; ++j) {
            const float a0 = c - lensHalf + 2.0f * lensHalf * static_cast<float>(j) / n, a1 = c - lensHalf + 2.0f * lensHalf * static_cast<float>(j + 1) / n;
            for (float r : {b.lensRadius, b.lensInner})
                d.scene.line(toScene(pos + dirv(a0) * r), toScene(pos + dirv(a1) * r), 0.5f * b.lensWidthPx * refPx_ * (r == b.lensRadius ? 1.0f : 0.5f),
                             acc * (1.8f * life), acc * (1.8f * life), 0);
        }
    }
    // The core: three counter-turning hexagons, the iris, the seed of light, the gold health arc.
    const float spin = static_cast<float>(in.music / kTicksPerBar) * kTau * 0.25f;
    for (u32 r = 0; r < 3; ++r) {
        const float rad = b.coreRingRadius[r];
        const u32 sides = b.coreRingSides[r] ? b.coreRingSides[r] : 6;
        const float ph = b.coreRingPhaseDeg[r] * kTau / 360.0f + (r % 2 ? -spin : spin);
        for (u32 j = 0; j < sides; ++j) {
            const float a0 = ph + kTau * static_cast<float>(j) / sides, a1 = ph + kTau * static_cast<float>(j + 1) / sides;
            d.scene.line(toScene(pos + dirv(a0) * rad), toScene(pos + dirv(a1) * rad), hw * 1.3f, acc * (2.2f * life), acc * (2.2f * life), 0);
        }
    }
    const float irisOpen = s.dying ? 1.0f : (downbeat ? 1.0f : 0.0f);
    const float irisR = b.irisClosed + (b.irisOpen - b.irisClosed) * irisOpen;
    for (u32 j = 0; j < b.irisShutters; ++j) {
        const float a0 = spin + kTau * static_cast<float>(j) / b.irisShutters;
        const Vec2 p0 = pos + dirv(a0) * irisR, p1 = pos + dirv(a0 + kTau / b.irisShutters) * irisR;
        d.scene.line(toScene(p0), toScene(p1), hw, acc * 2.0f, acc * 2.0f, 0);
        d.scene.line(toScene(p0), toScene(pos + dirv(a0 + 0.6f) * (irisR + 7.0f)), hw * 0.8f, acc * 1.4f, acc * 0.6f, 0);
    }
    d.sparkles.glow(toScene(pos), (6.0f + 10.0f * irisOpen) * pxPerWu_, W1 * (0.8f + 1.6f * irisOpen) * life);
    if (irisOpen > 0.0f) d.pools.glow(toScene(pos), 70.0f * pxPerWu_, acc * 0.25f);
    if (!s.dying && s.coreMax > 0.0f) {
        const float frac = clamp01(s.coreHp / s.coreMax);
        const u32 n = 48;
        for (u32 j = 0; j < n; ++j) {
            const float u0 = static_cast<float>(j) / n, u1 = static_cast<float>(j + 1) / n;
            if (u0 >= frac) break;
            const float a0 = -0.5f * kPi + u0 * kTau, a1 = -0.5f * kPi + std::fmin(u1, frac) * kTau;
            const Vec3 g = palettes_->player[3] * 1.8f;
            d.scene.line(toScene(pos + dirv(a0) * b.healthRingRadius), toScene(pos + dirv(a1) * b.healthRingRadius), hw * 0.8f, g, g, 0);
        }
    }
    d.pools.glow(toScene(pos), b.radius * 1.4f * pxPerWu_, acc * (0.06f * life));
}

}  // namespace rf
