// The view of M5 part 5: the arena's structures and events (design/arena.md "Look") and the elites with their traits
// (look.md 3 and 4: hull + 0.6 at 2.3, a white copy of the hull, glass, an aura at 0.22, a white edge chevron; each trait's
// treatment). Shapes from arena.json's glyphs through the species table; cosmetic only.

#include <cmath>
#include <cstdio>
#include <initializer_list>

#include "core/args.h"
#include "game/play_view.h"
#include "gfx/vector_font.h"
#include "sim/boss_prose.h"
#include "sim/tuning.h"

namespace rf {

using namespace sim;

namespace {
Vec2 dirv(float a) { return {std::cos(a), std::sin(a)}; }
Vec3 whiteV() { return {1.0f, 1.0f, 1.0f}; }
float clamp01(float x) { return x < 0.0f ? 0.0f : (x > 1.0f ? 1.0f : x); }
constexpr float kVaultChevronWu = 1200.0f;  // arena.md (play test 3, quoted): a Vault's gold chevron while off screen this near
}  // namespace

void PlayView::arenaShape(FrameDraw& d, const ArenaGlyph& g, Vec2 pos, float rot, float scale, Vec3 c0, Vec3 c1, Vec3 c2, float widthRef, u8 layers) {
    // An arena.json shape: unit segments x the radius, rotated; layer 0 the hull, 1 the ornament, 2 the white mark.
    const float c = std::cos(rot) * scale, s = std::sin(rot) * scale;
    for (u32 k = 0; k < g.count; ++k) {
        const u8 l = g.layer[k];
        if (!((layers >> l) & 1u)) continue;
        const Vec2 a = pos + Vec2{g.seg[k][0] * c - g.seg[k][1] * s, g.seg[k][0] * s + g.seg[k][1] * c};
        const Vec2 b = pos + Vec2{g.seg[k][2] * c - g.seg[k][3] * s, g.seg[k][2] * s + g.seg[k][3] * c};
        const Vec3 col = l == 0 ? c0 : (l == 1 ? c1 : c2);
        d.scene.line(toScene(a), toScene(b), 0.5f * widthRef * refPx_, col, col, 0);
    }
}

void PlayView::drawElite(FrameDraw& d, const ViewInput& in, u32 i, Vec2 pos, float rot0, float rot1, float scale, Vec3 hue, int g, float level) {
    // look.md 3: a white copy of the hull, glass and an aura (never dimmed by the light budget); look.md 4: each trait.
    const World& w = *in.world;
    const Enemies& e = w.enemies();
    const Look::Enemy& le = look_->enemy;
    const GlyphDef& gd = glyphs_->def(static_cast<u32>(g));
    const Vec3 W1 = whiteV(), danger = palettes_->danger;
    const u16 tr = e.traits[i];
    const float t = static_cast<float>(in.time);
    if (shown(kMuteAura)) d.pools.glow(toScene(pos), le.auraRadiusOfExtent * gd.extent * scale * pxPerWu_, hue * (le.eliteAura * level));
    if (!shown(kMuteElite)) return;
    if (w.elitePhased(i))  // M6 (look.md 4): Phasing off its solid half, every other hull segment hidden, the white copy too
        hullEveryOther(d, g, pos, rot0, scale, look_->lines.eliteWhiteCopy.intensity * level, 0.5f * look_->lines.eliteWhiteCopy.width, W1, W1, true);
    else
        glyph(d.glyphs, g, true, pos, rot0, rot1, scale, look_->lines.eliteWhiteCopy.intensity * level, 0.5f * look_->lines.eliteWhiteCopy.width, kGlyphWhite, W1, W1);
    if (tier_.glass && gd.glassCount) {
        const Vec2* tri = glyphs_->glass() + gd.glassFirst * 3;
        const float c = std::cos(rot0) * scale, s = std::sin(rot0) * scale;
        const Vec3 tint = hue * (le.eliteGlass * level);
        for (u32 k = 0; k < gd.glassCount; ++k) {
            Vec2 p[3];
            for (u32 j = 0; j < 3; ++j) {
                const Vec2 q = tri[k * 3 + j];
                p[j] = toScreen(pos + Vec2{q.x * c - q.y * s, q.x * s + q.y * c});
            }
            d.glass.tri(p[0], tint, p[1], tint, p[2], tint);
        }
    }
    const auto ring = [&](Vec2 c, float r, Vec3 col, float wRef, u32 n, float from, float span) {
        for (u32 k = 0; k < n; ++k) {
            const float a0 = from + span * static_cast<float>(k) / n, a1 = from + span * static_cast<float>(k + 1) / n;
            d.scene.line(toScene(c + dirv(a0) * r), toScene(c + dirv(a1) * r), 0.5f * wRef * refPx_, col, col, 0);
        }
    };
    const TraitData& td = w.species().traits;
    if (hasTrait(tr, kArmored) && e.hp[i] > e.hpMax[i] * td.armoredHull)  // a second hull at 112%, dim, while the outer hull holds
        glyph(d.glyphs, g, true, pos, rot0, rot1, scale * 1.12f, 0.5f * level, 0.5f * look_->lines.eliteWhiteCopy.width, kGlyphEnemy, hue, hue);
    if (hasTrait(tr, kSwift)) {  // three short speed streaks behind
        const Vec2 v{e.x[i] - e.px[i], e.y[i] - e.py[i]};
        const Vec2 u = normalizeOr(v, dirv(e.heading[i])) * -1.0f;
        const Vec2 n{-u.y, u.x};
        for (float o : {-0.5f, 0.0f, 0.5f})
            d.scene.line(toScene(pos + u * (scale * 1.3f) + n * (o * scale)), toScene(pos + u * (scale * 2.6f) + n * (o * scale)), 0.6f * refPx_, W1 * (1.2f * level),
                         Vec3{}, 0);
    }
    if (hasTrait(tr, kSplitting)) {  // a crack through the hull, widening with damage
        const float hurt = 1.0f - clamp01(e.hp[i] / std::fmax(e.hpMax[i], 1.0f));
        const Vec2 a = pos + dirv(rot0 + 0.6f) * scale, b = pos - dirv(rot0 + 0.6f) * scale;
        d.scene.line(toScene(a), toScene(pos + dirv(rot0 + 1.2f) * (0.2f * scale)), (0.4f + 1.4f * hurt) * refPx_, W1 * level, W1 * level, 0);
        d.scene.line(toScene(pos + dirv(rot0 + 1.2f) * (0.2f * scale)), toScene(b), (0.4f + 1.4f * hurt) * refPx_, W1 * level, W1 * level, 0);
    }
    if (hasTrait(tr, kVolatile)) {  // the hull flickers toward the danger colour; a pulsing core dot
        const float f = 0.5f + 0.5f * std::sin(t * 23.0f + static_cast<float>(i));
        glyph(d.glyphs, g, true, pos, rot0, rot1, scale, 0.9f * f * level, 0.5f * le.eliteWidth, kGlyphEnemy, danger, danger);
        d.sparkles.glow(toScene(pos), (4.0f + 3.0f * kick_) * refPx_, danger * (1.6f * level));
    }
    if (hasTrait(tr, kRegenerating)) {  // a small plus sign orbiting
        const Vec2 c = pos + dirv(t * 2.0f + static_cast<float>(i)) * (scale * 1.6f);
        d.scene.line(toScene(c - Vec2{5.0f, 0.0f}), toScene(c + Vec2{5.0f, 0.0f}), 0.7f * refPx_, W1 * (1.3f * level), W1 * (1.3f * level), 0);
        d.scene.line(toScene(c - Vec2{0.0f, 5.0f}), toScene(c + Vec2{0.0f, 5.0f}), 0.7f * refPx_, W1 * (1.3f * level), W1 * (1.3f * level), 0);
    }
    if (hasTrait(tr, kShielded)) {  // a thick white frontal arc
        const float half = 0.5f * td.shieldArc * kTau / 360.0f;
        ring(pos, scale * 1.7f, W1 * (1.5f * level), 2.4f, 8, e.shield[i] - half, 2.0f * half);
    }
    if (hasTrait(tr, kMagnetic)) {  // two horseshoe arcs
        ring(pos, scale * 1.45f, hue * (1.4f * level), 1.2f, 6, rot0 + 0.3f, 2.2f);
        ring(pos, scale * 1.45f, hue * (1.4f * level), 1.2f, 6, rot0 + 0.3f + kPi, 2.2f);
    }
    if (hasTrait(tr, kTwin) && e.twin[i]) {  // a thin tether line in the enemy hue
        const i32 j = w.enemyIndex(e.twin[i]);
        if (j >= 0 && static_cast<u32>(j) > i) d.scene.line(toScene(pos), toScene(Vec2{e.x[j], e.y[j]}), 0.5f * refPx_, hue * (0.9f * level), hue * (0.9f * level), 0);
    }
    if (hasTrait(tr, kCommander)) {  // three crown ticks and a faint aura ring (its reach)
        for (u32 k = 0; k < 3; ++k) {
            const float a = -0.5f * kPi + (static_cast<float>(k) - 1.0f) * 0.45f;
            d.scene.line(toScene(pos + dirv(a) * (scale * 1.25f)), toScene(pos + dirv(a) * (scale * 1.75f)), 0.8f * refPx_, W1 * (1.4f * level), W1 * (1.4f * level), 0);
        }
        ring(pos, td.commanderReach, hue * (0.12f * level), 0.8f, 48, 0.0f, kTau);
    }
}

void PlayView::drawArena(FrameDraw& d, const ViewInput& in) {
    const World& w = *in.world;
    const ArenaData& A = w.species().arena;
    const ArenaBody* bodies = w.arenaBodies();
    const double t = in.music;
    const float now = static_cast<float>(in.time);
    const Vec3 W1 = whiteV(), gold = palettes_->player[3], danger = palettes_->danger, accent = palette_->accent;
    const double bar = kTicksPerBar;
    const auto ring = [&](Vec2 c, float r, Vec3 col, float wRef, u32 n, float dashes) {
        for (u32 k = 0; k < n; ++k) {
            if (dashes > 0.0f && (k % 2u)) continue;
            const float a0 = kTau * static_cast<float>(k) / n, a1 = kTau * static_cast<float>(k + 1) / n;
            d.scene.line(toScene(c + dirv(a0) * r), toScene(c + dirv(a1) * r), 0.5f * wRef * refPx_, col, col, 0);
        }
    };
    {
        // M5 part 7 (arena.md): a Rift Surge's three joined by a faint triangle.
        Vec2 surge[3];
        u32 ns = 0;
        for (u32 k = 0; k < World::kArenaBodies && ns < 3; ++k)
            if (bodies[k].alive && bodies[k].surge && bodies[k].kind == static_cast<u8>(StructKind::Rift)) surge[ns++] = bodies[k].pos;
        if (ns == 3)
            for (u32 j = 0; j < 3; ++j) {
                const Vec2 p0 = surge[j], p1 = surge[(j + 1) % 3];
                for (u32 q = 0; q < 24; ++q) {
                    const Vec2 a0 = p0 + (p1 - p0) * (static_cast<float>(q) / 24.0f), a1 = p0 + (p1 - p0) * (static_cast<float>(q + 1) / 24.0f);
                    if (visible(a0, 40.0f) || visible(a1, 40.0f))
                        d.scene.line(toScene(a0), toScene(a1), 0.5f * refPx_, palette_->hue[0] * 0.35f, palette_->hue[0] * 0.35f, 0);
                }
            }
    }
    for (u32 k = 0; k < World::kArenaBodies; ++k) {
        const ArenaBody& b = bodies[k];
        if (!b.alive) continue;
        const float R = b.kind < kStructKinds ? A.radius[b.kind] : (b.kind == kBodyCarrier ? prose::kArCarrierRadius : A.radius[0]);
        const float fade = b.gone >= 0.0 ? clamp01(1.0f - static_cast<float>((t - b.gone) / (kTrackChangeBars * bar))) * 0.7f + 0.3f : 1.0f;
        // (arena.md: a Rift's edge chevron is the enemy hue with a ring at its tip; structures get none)
        if (b.kind == static_cast<u8>(StructKind::Rift) && b.from > 0.0 && !visible(b.pos, 0.0f) && shown(kMutePriority))
            edgeChevron(d, b.pos, palette_->hue[0], false, 1.0f, 1);
        // Play test 3 (arena.md, "A Vault says what it is"): a gold edge chevron while it is off screen within 1,200 wu.
        if (b.kind == static_cast<u8>(StructKind::Vault) && b.gone < 0.0 && !visible(b.pos, 0.0f) && shown(kMutePriority) &&
            lengthSq(b.pos - w.player(w.viewPlayer()).pos) <= kVaultChevronWu * kVaultChevronWu)
            edgeChevron(d, b.pos, gold, false, 1.0f);
        if (!visible(b.pos, (b.kind == static_cast<u8>(StructKind::Current) ? prose::kArCurrentLength : R) + 600.0f)) continue;
        switch (b.kind) {
            case static_cast<u8>(StructKind::Vault):
            case kBodyGoldVault: {
                // Three nested hexagons in white and gold with a glass fill and a gold pool at 0.2; a twelfth of a turn a bar, 6% on the
                // kick; a ring of its ornament shatters each quarter lost.
                const bool goldV = b.kind == kBodyGoldVault;
                const float rot = static_cast<float>((t - b.placed) / bar) * (kTau / 12.0f);
                const float sc = R * (1.0f + 0.06f * kick_);
                const u8 layers = static_cast<u8>(b.quarters >= 2 ? 1u : (b.quarters >= 1 ? 3u : 7u));
                arenaShape(d, A.glyph[ArenaData::gVault], b.pos, rot, sc, (goldV ? gold : W1) * (1.6f * fade), gold * (1.4f * fade), W1 * (1.2f * fade), 1.6f, layers);
                d.pools.glow(toScene(b.pos), 2.0f * R * pxPerWu_, gold * (0.2f * fade));
                if (!goldV && b.hp >= b.hpMax && b.gone < 0.0) {
                    // Play test 3 (arena.md): its name, VAULT, as world text above it (cap 22, gold) until its first hit.
                    const char* name = "VAULT";
                    const float cap = 22.0f * pxPerWu_;
                    const float wText = font_->measure(TextStyle{cap}, name);
                    const Vec2 sc2 = toScene(b.pos);
                    const Vec3 g = gold * 1.8f;
                    font_->drawField(d.scene, Vec2{sc2.x - 0.5f * wText, sc2.y - (R + 24.0f) * pxPerWu_ - cap}, cap, 0.9f * refPx_, g, g, g, name);
                }
                if (goldV) {
                    const float since = static_cast<float>((t - b.placed) / bar);
                    if (since < 1.0f) d.scene.line(toScene(b.pos), toScene(b.pos) + Vec2{0.0f, -400.0f * refPx_}, 3.0f * refPx_, gold * (2.0f * (1.0f - since)), Vec3{}, 0);
                    if (b.until > 0.0) {
                        const float left = clamp01(static_cast<float>((b.until - t) / (8.0 * bar)));
                        const u32 n = 48, lit = static_cast<u32>(left * n);
                        for (u32 j = 0; j < lit; ++j) {
                            const float a0 = -0.5f * kPi + kTau * j / n, a1 = -0.5f * kPi + kTau * (j + 1) / n;
                            d.scene.line(toScene(b.pos + dirv(a0) * 90.0f), toScene(b.pos + dirv(a1) * 90.0f), 0.5f * refPx_, gold * 1.2f, gold * 1.2f, 0);
                        }
                    }
                }
                break;
            }
            case static_cast<u8>(StructKind::Pylon): {
                // A tall diamond with a white spine; untuned at 0.8 in the accent, tuned at 2.4 in the player's colours with a pool;
                // its 150 wu ring dashed in the accent, filling clockwise; its pulse a ring in the player's colours.
                const bool tuned = b.tuner < kMaxPlayers && t < b.until;
                const Vec3 col = tuned ? palettes_->player[b.tuner] * 2.4f : accent * 0.8f;
                arenaShape(d, A.glyph[ArenaData::gPylon], b.pos, 0.0f, R, col * fade, W1 * (1.4f * fade), W1 * (1.2f * fade), 1.4f, 7);
                if (tuned) d.pools.glow(toScene(b.pos), 3.0f * R * pxPerWu_, palettes_->player[b.tuner] * 0.15f);
                if (!tuned) {
                    ring(b.pos, prose::kArPylonRingWu, accent * 0.7f, 0.8f, 48, 1.0f);
                    const u32 n = 48, lit = static_cast<u32>(b.charge * static_cast<float>(n));
                    for (u32 j = 0; j < lit; ++j) {
                        const float a0 = -0.5f * kPi + kTau * j / n, a1 = -0.5f * kPi + kTau * (j + 1) / n;
                        d.scene.line(toScene(b.pos + dirv(a0) * prose::kArPylonRingWu), toScene(b.pos + dirv(a1) * prose::kArPylonRingWu), 0.8f * refPx_, W1 * 1.6f, W1 * 1.6f, 0);
                    }
                } else if (b.from >= 0.0) {
                    const float s = static_cast<float>((t - b.from) / (in.bpm * kTicksPerBeat / 60.0f));
                    const float r = std::fmin(prose::kArPylonPulseWu, s * prose::kArPylonPulseSpeed);
                    if (r < prose::kArPylonPulseWu) ring(b.pos, r, palettes_->playerLight[b.tuner] * (1.6f * (1.0f - r / prose::kArPylonPulseWu)), 1.2f, 64, 0.0f);
                }
                break;
            }
            case static_cast<u8>(StructKind::Rift): {
                // Three broken rings in the first enemy hue turning against each other, sparks spiralling in; while open, the black
                // disc inside them (M5 part 7: on the priority layer's black, which the scene's additive light cannot draw).
                const bool open = b.from > 0.0;
                if (open && shown(kMutePriority)) d.threatRims.line(toScreen(b.pos), toScreen(b.pos), 0.5f * R * pxPerWu_ * fade, Vec3{}, Vec3{}, 0);
                const Vec3 hue = palette_->hue[0] * ((open ? 1.6f : 0.6f) * fade);
                for (u32 r = 0; r < 3; ++r) {
                    const float rot = static_cast<float>(t / bar) * (r == 1 ? -0.5f : 0.35f) * kTau;
                    arenaShape(d, A.glyph[ArenaData::gRift], b.pos, rot, R * (1.0f - 0.18f * static_cast<float>(r)), hue, hue * 0.8f, W1, 1.4f, r == 0 ? 1u : 3u);
                }
                if (open)
                    for (u32 j = 0; j < 6; ++j) {
                        const float a = now * 3.0f + static_cast<float>(j) * 1.05f, rr = R * (0.4f + 0.6f * std::fmod(now * 0.9f + j * 0.17f, 1.0f));
                        d.sparkles.glow(toScene(b.pos + dirv(a) * rr), 3.0f * refPx_, palette_->partner[0] * 1.4f);
                    }
                break;
            }
            case static_cast<u8>(StructKind::Bumper): {
                // A ring with a smaller ring inside and four white ticks; its fill brightens with its charge (glass 0.05 to 0.4).
                arenaShape(d, A.glyph[ArenaData::gBumper], b.pos, 0.0f, R, accent * (1.6f * fade), accent * (1.2f * fade), W1 * (1.4f * fade), 1.4f, 7);
                d.pools.glow(toScene(b.pos), 1.6f * R * pxPerWu_, accent * (0.05f + 0.35f * b.charge / static_cast<float>(prose::kArBumperCharge)));
                break;
            }
            case static_cast<u8>(StructKind::Current): {
                // No walls: rows of chevrons in the grid colour at 0.6 streaming along it on the eighths.
                const Vec2 dv = dirv(b.angle), nv{-dv.y, dv.x};
                const float step = 120.0f, shift = std::fmod(static_cast<float>(t / (kTicksPerBeat * 0.5)) * 40.0f, step);
                const Vec3 col = palette_->grid * (0.6f * 2.0f * fade);
                for (float along = -0.5f * prose::kArCurrentLength + shift; along < 0.5f * prose::kArCurrentLength; along += step)
                    for (float across : {-80.0f, 0.0f, 80.0f}) {
                        const Vec2 c = b.pos + dv * along + nv * across;
                        if (!visible(c, 30.0f)) continue;
                        d.scene.line(toScene(c - dv * 14.0f + nv * 12.0f), toScene(c), 0.7f * refPx_, col, col, 0);
                        d.scene.line(toScene(c - dv * 14.0f - nv * 12.0f), toScene(c), 0.7f * refPx_, col, col, 0);
                    }
                break;
            }
            case static_cast<u8>(StructKind::Lens):
                // A thin lozenge of white glass with a bright spine, turning.
                arenaShape(d, A.glyph[ArenaData::gLens], b.pos, b.angle, R, W1 * (1.0f * fade), W1, W1 * (2.0f * fade), 1.2f, 7);
                break;
            case kBodyComet: {
                // A white kite with a three-line tail in gold fading over 200 wu, a sparkle at its head.
                const float rot = std::atan2(b.vel.y, b.vel.x);
                arenaShape(d, A.glyph[ArenaData::gComet], b.pos, rot, A.glyph[ArenaData::gComet].radius, W1 * 1.8f, gold * 1.4f, W1, 1.4f, 3);
                d.sparkles.glow(toScene(b.pos + dirv(rot) * A.glyph[ArenaData::gComet].radius), 10.0f * refPx_, W1 * 2.0f);
                break;
            }
            case kBodyCarrier: {
                // An octagon in gold with a white hold and a plus, a gold pool; the three joined by a thin gold line.
                arenaShape(d, A.glyph[ArenaData::gCarrier], b.pos, b.angle, R, gold * 1.6f, W1 * 1.3f, W1 * 1.4f, 1.6f, 7);
                d.pools.glow(toScene(b.pos), 2.0f * R * pxPerWu_, gold * 0.15f);
                for (u32 j = 0; j < World::kArenaBodies; ++j)
                    if (bodies[j].alive && bodies[j].kind == kBodyCarrier && bodies[j].tone == b.tone + 1)
                        d.scene.line(toScene(b.pos), toScene(bodies[j].pos), 0.4f * refPx_, gold * 0.8f, gold * 0.8f, 0);
                break;
            }
            default: break;
        }
    }
    // Volatile bombs: a danger ring of 160 wu filling from its edge over two beats (priority layer).
    const auto fillRing = [&](Vec2 c, float r, float u, double fireTick) {
        const u32 n = 40;
        for (u32 k = 0; k < n; ++k) {
            if (k % 2u) continue;
            const float a0 = kTau * k / n, a1 = kTau * (k + 1) / n;
            telegraphLine(d, c + dirv(a0) * r, c + dirv(a1) * r, fireTick);
        }
        if (u > 0.0f) d.pools.glow(toScene(c), r * (1.0f - 0.6f * (1.0f - u)) * pxPerWu_, danger * (0.06f + 0.1f * u));
        if (shown(kMutePriority)) {
            const float rr = r * (1.0f - u);
            for (u32 k = 0; k < n; ++k) {
                const float a0 = kTau * k / n, a1 = kTau * (k + 1) / n;
                d.threatCores.line(toScreen(c + dirv(a0) * rr), toScreen(c + dirv(a1) * rr), refPx_, danger * 0.8f, danger * 0.8f, 0);
            }
        }
    };
    for (u32 k = 0; k < World::kArenaBombs; ++k) {
        const ArenaBomb& b = w.bombs()[k];
        if (b.burst < 0.0 || !visible(b.at, 200.0f)) continue;
        fillRing(b.at, w.species().traits.volatileRing, clamp01(1.0f - static_cast<float>((b.burst - t) / (w.species().traits.volatileFuse * kTicksPerBeat))), b.burst);
    }
    for (u32 k = 0; k < World::kArenaMeteors; ++k) {
        const ArenaMeteor& m = w.meteors()[k];
        if (m.land < 0.0 || !visible(m.at, 150.0f)) continue;
        fillRing(m.at, prose::kArMeteorWu, clamp01(1.0f - static_cast<float>((m.land - t) / (prose::kArMeteorBeats * kTicksPerBeat))), m.land);
    }
    // The event: its announcement (a gold edge chevron and its name at the place; the Comet a white chevron two beats ahead),
    // the Storm Cell's ring with the lattice hot inside, the hunted elite's dotted trail.
    const ArenaEventState& ev = w.arenaEvent();
    if (ev.kind != kNoEvent) {
        const EventKind k = static_cast<EventKind>(ev.kind);
        const bool announcing = t >= ev.announce && t < ev.start + (k == EventKind::Comet ? 0.0 : 0.5 * bar);
        Vec2 where = ev.pos;
        if (k == EventKind::EliteHunt) {
            const i32 i = w.enemyIndex(ev.hunted);
            if (i >= 0) where = Vec2{w.enemies().x[i], w.enemies().y[i]};
        }
        if (k == EventKind::Comet)
            for (u32 j = 0; j < World::kArenaBodies; ++j)
                if (bodies[j].alive && bodies[j].kind == kBodyComet) where = bodies[j].pos;
        if ((announcing || ev.stage == 1) && !visible(where, 0.0f) && shown(kMutePriority) && k != EventKind::Eclipse && k != EventKind::MeteorRain)
            edgeChevron(d, where, k == EventKind::Comet || k == EventKind::EliteHunt ? W1 : gold, false, 1.0f);
        if (announcing && k != EventKind::Comet && visible(where, 300.0f)) {
            const char* name = eventName(k);
            const float cap = 22.0f * pxPerWu_;
            const float wText = font_->measure(TextStyle{cap}, name);
            const Vec2 sc = toScene(where);
            const Vec3 g = gold * 1.8f;
            font_->drawField(d.scene, Vec2{sc.x - 0.5f * wText, sc.y - 80.0f * pxPerWu_ - cap}, cap, 0.9f * refPx_, g, g, g, name);
        }
        if (k == EventKind::StormCell && ev.stage == 1) {
            // look.md §5 "What an area means" (the owner, 2026-11-14: "some areas aren't clear if they are safe or not"): it hurts
            // a ship that stays inside, so its edge is a lethal line and it has a faint danger fill (an accent ring until then).
            const u32 n = 72;
            const float R = prose::kArStormWu;
            const Vec3 fill = danger * 0.05f;
            for (u32 j = 0; j < n; ++j) {
                const float a0 = kTau * static_cast<float>(j) / n, a1 = kTau * static_cast<float>(j + 1) / n;
                const Vec2 p0 = ev.pos + dirv(a0) * R, p1 = ev.pos + dirv(a1) * R;
                if (!visible(p0, 30.0f) && !visible(p1, 30.0f)) continue;
                lethalBeam(d, p0, p1, 4.0f);
                d.glass.tri(toScene(ev.pos), fill, toScene(p0), fill, toScene(p1), fill);
            }
        }
    }
    // The Storm Cell's strikes: zigzag lines for one beat in white and the accent.
    const float beatS = 60.0f / std::fmax(in.bpm, 30.0f);
    for (const Strike& s : strikes_) {
        const float age = now - s.born;
        if (age < 0.0f || age > beatS) continue;
        const float k = 1.0f - age / beatS;
        Vec2 prev = s.a;
        const Vec2 dv = s.b - s.a, nv = normalizeOr(Vec2{-dv.y, dv.x}, Vec2{0.0f, 1.0f});
        for (u32 j = 1; j <= 6; ++j) {
            const float u = static_cast<float>(j) / 6.0f;
            const Vec2 p = j == 6 ? s.b : s.a + dv * u + nv * ((j % 2u ? 1.0f : -1.0f) * 18.0f);
            // (M6: reduce flashing takes the lightning on every beat down to 0.9 and 0.8)
            d.scene.line(toScene(prev), toScene(p), 0.9f * refPx_, W1 * ((reduceFlashing_ ? 0.9f : 2.0f) * k), accent * ((reduceFlashing_ ? 0.8f : 1.6f) * k), 0);
            prev = p;
        }
    }
}

void PlayView::eventArena(const World& w, const Event& e, float t) {
    // The arena's and the elites' moments (sim::BossCue from ElArrive on).
    const Vec2 at{e.x, e.y};
    const Vec3 W1 = whiteV(), gold = palettes_->player[3], danger = palettes_->danger, accent = palette_->accent;
    switch (static_cast<BossCue>(e.a)) {
        case BossCue::ElShatter: {
            // Armored's outer hull shatters: white shards fly from its edge.
            SparkReq r{};
            r.at = at;
            r.count = 24;
            r.kind = 5;
            r.a = W1;
            r.b = palette_->hue[1];
            r.t0 = t;
            sparkRequest(r);
            addShockRings(at, 1, 2.0f * e.f + 40.0f, W1, palette_->hue[1], t, 0.2f);
            break;
        }
        case BossCue::ElBurst:
        case BossCue::MeLand:
            addShockRings(at, 2, e.f + 40.0f, danger, W1, t, 0.35f);
            addPool(at, 20.0f, e.f, W1, reduceFlashing_ ? 0.4f : 0.8f, 0.0f, 0.2f, false, 1, false, false, t);
            push(kImpBoss, at, 160.0f, e.f * 1.5f, t);
            shakeAmp_ = std::fmax(shakeAmp_, 3.0f);
            break;
        case BossCue::ElStun: addShockRings(at, 1, e.f, palette_->hue[2], W1, t, 0.3f); break;
        case BossCue::VaBreak:
        case BossCue::CoBreak:
            addShockRings(at, 2, 260.0f, gold, W1, t, 0.4f);
            addPool(at, 30.0f, 200.0f, gold, 0.6f, 0.0f, 0.3f, false, 1, false, false, t);
            break;
        case BossCue::RiClose: addShockRings(at, 1, 200.0f, palette_->hue[0], W1, t, 0.3f); break;
        case BossCue::BuDischarge: addShockRings(at, 1, e.f, W1, accent, t, 0.25f); break;
        case BossCue::PyTuned: addShockRings(at, 1, prose::kArPylonRingWu, palettes_->player[e.b < kMaxPlayers ? e.b : 0], W1, t, 0.3f); break;
        case BossCue::VdLand: push(kImpBoss, at, 400.0f, 300.0f, t); break;
        case BossCue::StStrike: {
            Strike& s = strikes_[strikeHead_];
            strikeHead_ = (strikeHead_ + 1u) % kStrikes;
            s.a = (e.b == 0 || t - lastStrikeT_ > 0.05f) ? w.arenaEvent().pos : lastStrike_;
            s.b = at;
            s.born = t;
            lastStrike_ = at;
            lastStrikeT_ = t;
            if (e.b == 0) push(kImpBoss, w.arenaEvent().pos, 60.0f, prose::kArStormWu, t);  // (the lattice hot inside it, a beat at a time)
            break;
        }
        default: break;
    }
}

}  // namespace rf
