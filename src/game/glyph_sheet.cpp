#include "game/glyph_sheet.h"

#include <cmath>
#include <cstdio>
#include <cstring>

#include "core/half.h"

namespace rf {

namespace {
const char* const kFamilies[17] = {"drifter", "hound", "flanker",   "skitter", "ram",   "moth",
                                   "shoal",   "wyrm_head", "wyrm_body", "carom", "blink", "sentry",
                                   "brood",   "tether", "weaver",    "well",    "mimic"};
const char* const kNames[17] = {"DRIFTER", "HOUND", "FLANKER",   "SKITTER", "RAM",   "MOTH",
                                "SHOAL",   "WYRM HEAD", "WYRM BODY", "CAROM", "BLINK", "SENTRY",
                                "BROOD",   "TETHER", "WEAVER",    "WELL",    "MIMIC"};
const char* const kTiers[5] = {"I", "II", "III", "IV", "V"};
constexpr float kRadiusWu = 14.0f;

void put(GlyphBatch& b, int g, bool hullOnly, Vec2 pos, float rot, float scale, float intensity, float halfWidth,
         GlyphMode mode, Vec3 a, Vec3 c) {
    if (g < 0) return;
    GlyphInstanceGpu* gi = b.push(static_cast<u32>(g), hullOnly);
    if (!gi) return;
    GlyphInstanceGpu v;
    v.x = pos.x;
    v.y = pos.y;
    v.rot[0] = v.rot[2] = floatToHalf(std::cos(rot));
    v.rot[1] = v.rot[3] = floatToHalf(std::sin(rot));
    v.shape[0] = floatToHalf(scale);
    v.shape[1] = floatToHalf(intensity);
    v.shape[2] = floatToHalf(halfWidth);
    v.shape[3] = floatToHalf(static_cast<float>(mode));
    v.a = packLinear(a);
    v.b = packLinear(c);
    std::memcpy(gi, &v, sizeof v);
}
}  // namespace

void GlyphSheet::enemy(FrameDraw& d, int glyph, Vec2 at, float scale, float rot, u32 hue, bool elite, int mark) {
    if (glyph < 0) return;
    const Look::Enemy& le = look_->enemy;
    const GlyphDef& g = glyphs_->def(static_cast<u32>(glyph));
    const Vec3 h = palette_->hue[hue], p = palette_->partner[hue];
    const float tier = static_cast<float>(g.tier ? g.tier : 1);
    float intensity = look_->lines.enemyHullTier1.intensity + le.hullIntensityPerTier * (tier - 1.0f);
    float halfWidth = 0.5f * look_->lines.enemyHullTier1.width;
    if (elite) {
        intensity += le.eliteIntensityAdd;
        halfWidth = 0.5f * le.eliteWidth;
    }
    put(d.glyphs, glyph, false, at, rot, scale, intensity, halfWidth, kGlyphEnemy, h, p);
    const Vec2 px = at * pxPerWu_;
    const float aura = elite ? le.eliteAura : (g.tier >= 3 ? le.auraByTier[g.tier - 1] : 0.0f);
    if (aura > 0.0f) d.pools.glow(px, le.auraRadiusOfExtent * g.extent * scale * pxPerWu_, h * aura);
    if (elite) {
        put(d.glyphs, glyph, true, at, rot, scale, look_->lines.eliteWhiteCopy.intensity,
            0.5f * look_->lines.eliteWhiteCopy.width, kGlyphWhite, {1, 1, 1}, {1, 1, 1});
        const Vec2* tri = glyphs_->glass() + g.glassFirst * 3;
        const float c = std::cos(rot) * scale * pxPerWu_, s = std::sin(rot) * scale * pxPerWu_;
        const Vec3 tint = h * le.eliteGlass;
        for (u32 k = 0; k < g.glassCount; ++k) {
            Vec2 q[3];
            for (u32 j = 0; j < 3; ++j) {
                const Vec2 u = tri[k * 3 + j];
                q[j] = px + Vec2{u.x * c - u.y * s, u.x * s + u.y * c};
            }
            d.glass.tri(q[0], tint, q[1], tint, q[2], tint);
        }
    }
    if (mark >= 0)
        put(d.glyphs, mark, false, at, rot, scale, look_->lines.variantMark.intensity, 0.5f * look_->lines.variantMark.width,
            kGlyphWhite, {1, 1, 1}, {1, 1, 1});
}

void GlyphSheet::render(FrameDraw& d, FrameParams& params, Background& bg, float W, float H) {
    refPx_ = H / 1080.0f;
    pxPerWu_ = 1.5f * refPx_;
    const float toWu = 1.0f / pxPerWu_;
    TextStyle label;
    label.capHeight = 12.0f * refPx_;
    label.color = rgba(235, 240, 255);
    label.intensity = 1.2f;
    TextStyle title = label;
    title.capHeight = 16.0f * refPx_;

    if (page_ <= 2) {
        const u32 first = page_ == 1 ? 0u : 9u;
        const u32 count = page_ == 1 ? 9u : 8u;
        const float left = 0.06f * W, top = 0.09f * H;
        const float colW = (W - left) / 9.0f, rowH = (H - top) / 5.0f;
        char head[48];
        std::snprintf(head, sizeof head, "ENEMY FAMILIES %u  TIERS I-V", page_);
        font_->draw(d.scene, {0.012f * W, 0.02f * H}, title, head);
        for (u32 r = 0; r < 5; ++r) font_->draw(d.scene, {0.02f * W, top + (static_cast<float>(r) + 0.45f) * rowH}, label, kTiers[r]);
        for (u32 c = 0; c < count; ++c) {
            const u32 f = first + c;
            const u32 hue = f < 6 ? 0u : (f < 12 ? 1u : 2u);
            font_->draw(d.scene, {left + static_cast<float>(c) * colW + 0.1f * colW, top - 0.035f * H}, label, kNames[f]);
            for (u32 r = 0; r < 5; ++r) {
                const int g = glyphs_->enemy(kFamilies[f], r + 1);
                if (g < 0) continue;
                const float size = glyphs_->def(static_cast<u32>(g)).size;
                const Vec2 big{left + (static_cast<float>(c) + 0.42f) * colW, top + (static_cast<float>(r) + 0.5f) * rowH};
                const Vec2 small{left + (static_cast<float>(c) + 0.86f) * colW, top + (static_cast<float>(r) + 0.8f) * rowH};
                enemy(d, g, big * toWu, 2.5f * kRadiusWu * size, 0.0f, hue, false, -1);
                enemy(d, g, small * toWu, kRadiusWu * size, 0.0f, hue, false, -1);
            }
        }
    } else {
        font_->draw(d.scene, {0.012f * W, 0.02f * H}, title, "VARIANT MARKS  ELITES  PLAYER  SHARD  BULLET");
        // Variant marks on a Hound and on a Drifter: A (none), B, C.
        const int hound = glyphs_->enemy("hound", 3), drifter = glyphs_->enemy("drifter", 3);
        const int markBH = glyphs_->find("mark_b_heading"), markBS = glyphs_->find("mark_b_spin"), markC = glyphs_->find("mark_c");
        const char* marks[3] = {"A", "B", "C"};
        for (u32 k = 0; k < 3; ++k) {
            const Vec2 a{(0.07f + 0.075f * static_cast<float>(k)) * W, 0.22f * H};
            const Vec2 b{(0.31f + 0.075f * static_cast<float>(k)) * W, 0.22f * H};
            enemy(d, hound, a * toWu, 2.5f * kRadiusWu, 0.0f, 0, false, k == 0 ? -1 : (k == 1 ? markBH : markC));
            enemy(d, drifter, b * toWu, 2.5f * kRadiusWu, 0.0f, 0, false, k == 0 ? -1 : (k == 1 ? markBS : markC));
            font_->draw(d.scene, {a.x - 6.0f * refPx_, 0.32f * H}, label, marks[k]);
            font_->draw(d.scene, {b.x - 6.0f * refPx_, 0.32f * H}, label, marks[k]);
        }
        font_->draw(d.scene, {0.05f * W, 0.1f * H}, label, "HOUND III");
        font_->draw(d.scene, {0.29f * W, 0.1f * H}, label, "DRIFTER III");
        // One elite of each of four families: white core, aura, glass.
        const char* eliteFamilies[4] = {"hound", "drifter", "carom", "moth"};
        const u32 eliteTiers[4] = {3, 4, 3, 3};
        const u32 eliteHues[4] = {0, 0, 1, 0};
        for (u32 k = 0; k < 4; ++k) {
            const Vec2 at{(0.08f + 0.1f * static_cast<float>(k)) * W, 0.55f * H};
            enemy(d, glyphs_->enemy(eliteFamilies[k], eliteTiers[k]), at * toWu, 2.5f * kRadiusWu, 0.0f, eliteHues[k], true, -1);
        }
        font_->draw(d.scene, {0.05f * W, 0.42f * H}, label, "ELITES");
        // The ship with each halo from one to six cells, in all four player colours.
        font_->draw(d.scene, {0.5f * W, 0.1f * H}, label, "PLAYER  P1-P4  HALO 1-6");
        const int ship = glyphs_->find("ship");
        for (u32 p = 0; p < 4; ++p) {
            const Vec3 pc = palettes_->player[p], deep = palettes_->playerDeep[p], light = palettes_->playerLight[p];
            for (u32 cells = 1; cells <= 6; ++cells) {
                char id[12];
                std::snprintf(id, sizeof id, "halo_%u", cells);
                const Vec2 at{(0.52f + 0.075f * static_cast<float>(cells - 1)) * W, (0.2f + 0.2f * static_cast<float>(p)) * H};
                const float scale = 1.6f * 16.0f;
                put(d.glyphs, ship, false, at * toWu, -kPi * 0.5f, scale, look_->lines.playerHull.intensity,
                    0.5f * look_->lines.playerHull.width, kGlyphPlayer, pc, deep);
                put(d.glyphs, ship, true, at * toWu, -kPi * 0.5f, scale, look_->lines.playerWhiteCopy.intensity,
                    0.5f * look_->lines.playerWhiteCopy.width, kGlyphWhite, {1, 1, 1}, {1, 1, 1});
                put(d.glyphs, glyphs_->find(id), false, at * toWu, 0.0f, scale, look_->lines.playerHalo.intensity,
                    0.5f * look_->lines.playerHalo.width, kGlyphRingA, pc, light);
                d.pools.glow(at, look_->soft.playerPool.radius[0] * pxPerWu_, pc * look_->soft.playerPool.intensity[0]);
            }
        }
        // A Shard and an enemy bullet (scene and threat layer).
        const int shard = glyphs_->find("shard"), bullet = glyphs_->find("enemy_bullet");
        const Vec3 shardColour = mix(palettes_->pickup, {1, 1, 1}, 0.4f);
        const Vec2 sAt{0.1f * W, 0.85f * H}, bAt{0.25f * W, 0.85f * H};
        put(d.glyphs, shard, false, sAt * toWu, 0.3f, 2.5f * kRadiusWu * 0.3f, look_->lines.shard.intensity,
            0.5f * look_->lines.shard.width, kGlyphFlat, shardColour, shardColour);
        d.sparkles.glow(sAt, look_->soft.shard.radius[0] * refPx_, palettes_->pickup * look_->soft.shard.intensity[0]);
        const Vec3 danger = palettes_->danger;
        const float bScale = 2.5f * kRadiusWu * 0.38f;
        put(d.glyphs, bullet, false, bAt * toWu, 0.0f, bScale, look_->lines.enemyBullet.intensity,
            0.5f * look_->lines.enemyBullet.width, kGlyphFlat, danger, danger);
        put(d.threatGlyphRims, bullet, false, bAt * toWu, 0.0f, bScale, 1.0f,
            0.5f * look_->threat.bulletWidth + look_->threat.bulletRim, kGlyphFlat, {}, {});
        const Vec3 core = mix(danger, {1, 1, 1}, 0.25f);
        put(d.threatGlyphCores, bullet, false, bAt * toWu, 0.0f, bScale, 1.0f, 0.5f * look_->threat.bulletWidth, kGlyphFlat,
            core, core);
        d.sparkles.glow(bAt, look_->soft.enemyBullet.radius[0] * refPx_, danger * look_->soft.enemyBullet.intensity[0]);
        font_->draw(d.scene, {0.08f * W, 0.93f * H}, label, "SHARD");
        font_->draw(d.scene, {0.22f * W, 0.93f * H}, label, "ENEMY BULLET");
    }

    bg.edge = palette_->bg0 * look_->bgLevel;
    bg.centre = palette_->bg1 * look_->bgLevel;
    bg.cx = look_->bgCentre[0];
    bg.cy = look_->bgCentre[1];
    bg.radius = look_->bgRadius;
    bg.power = look_->bgPower;
    bg.cloudCount = 0;
    params.vignette = false;
    params.afterglow = false;
    params.world = {pxPerWu_, 0.0f, 0.0f};
    params.worldThreat = params.world;
    params.accent = palette_->accent;
    params.enemySwell = 1.0f;
    (void)W;
}

}  // namespace rf
