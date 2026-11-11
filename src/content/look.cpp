#include "content/look.h"

#include <cstdio>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf {

namespace {
const char kLookJson[] =
#include "look.json.inc"
    ;
const char kVariantsJson[] =
#include "look_variants.json.inc"
    ;

struct Reader {
    const JsonDoc& doc;
    const char* file;
    bool ok = true;

    float num(const char* path, const JsonValue* from = nullptr) {
        bool found = true;
        const double v = doc.number(path, 0.0, &found, from);
        if (!found) {
            RF_ERR("look: %s has no number at '%s'", file, path);
            ok = false;
        }
        return static_cast<float>(v);
    }
    void pair(const char* path, float out[2]) {
        if (doc.numbers(path, out, 2) != 2) {
            RF_ERR("look: %s has no pair at '%s'", file, path);
            ok = false;
        }
    }
    // A number or a two-number range: lo == hi for a single number.
    void range(const char* path, float out[2], const JsonValue* from = nullptr) {
        const u32 n = doc.numbers(path, out, 2, from);
        if (n == 1) out[1] = out[0];
        if (n == 0) {
            RF_ERR("look: %s has no number or range at '%s'", file, path);
            ok = false;
        }
    }
    void line(const char* name, LookLine& out) {
        char path[64];
        std::snprintf(path, sizeof path, "lines.%s", name);
        float v[2];
        if (doc.numbers(path, v, 2) != 2) {
            RF_ERR("look: %s has no [intensity, width] at '%s'", file, path);
            ok = false;
            return;
        }
        out = {v[0], v[1]};
    }
    void soft(const char* id, LookSoftLight& out) {
        const JsonValue* list = doc.find("softLights");
        for (const JsonValue* e = doc.firstChild(list); e; e = doc.nextSibling(e)) {
            char name[24] = {};
            doc.text("id", name, sizeof name, e);
            if (std::strcmp(name, id) != 0) continue;
            float r[2] = {};
            u32 n = doc.numbers("radiusRefPx", r, 2, e);
            if (n == 0) n = doc.numbers("radiusWu", r, 2, e);
            if (n == 1) r[1] = r[0];
            out.radius[0] = r[0];
            out.radius[1] = r[1];
            range("intensity", out.intensity, e);
            out.lifeS = static_cast<float>(doc.number("lifeS", 0.0, nullptr, e));
            out.largeScale = static_cast<float>(doc.number("largeKillRadiusScale", 1.0, nullptr, e));
            char fade[16] = {};
            doc.text("fade", fade, sizeof fade, e);
            out.squaredFade = std::strcmp(fade, "squared") == 0;
            if (n == 0) {
                RF_ERR("look: soft light '%s' has no radius", id);
                ok = false;
            }
            return;
        }
        RF_ERR("look: no soft light '%s'", id);
        ok = false;
    }
    void explosion(const char* size, LookExplosion& out) {
        const JsonValue* e = doc.find(size);
        if (!e) {
            RF_ERR("look: no '%s'", size);
            ok = false;
            return;
        }
        range("sparks", out.sparks, e);
        range("speedWu", out.speedWu, e);
        range("lifeS", out.lifeS, e);
        out.rings = static_cast<u32>(doc.number("rings", 0.0, nullptr, e));
        out.ringToWu = static_cast<float>(doc.number("ringToWu", 48.0, nullptr, e));
        out.ringS = static_cast<float>(doc.number("ringS", 0.18, nullptr, e));
        out.ringGapS = static_cast<float>(doc.number("ringGapS", 0.0, nullptr, e));
        out.embers = static_cast<u32>(doc.number("embers", 0.0, nullptr, e));
        out.emberLifeS = static_cast<float>(doc.number("emberLifeS", 1.2, nullptr, e));
        out.gridImpulse = num("gridImpulse", e);
        out.shakeRefPx = num("shakeRefPx", e);
    }
    void distortion(const char* id, LookDistortionSource& out) {
        const JsonValue* e = doc.find(id);
        if (!e) {
            RF_ERR("look: no '%s'", id);
            ok = false;
            return;
        }
        out.refPx = num("refPx", e);
        out.widthRefPx = static_cast<float>(doc.number("ringWidthRefPx", 0.0, nullptr, e));
        out.lifeS = static_cast<float>(doc.number("lifeS", 0.35, nullptr, e));
        out.toWu = static_cast<float>(doc.number("toWu", 0.0, nullptr, e));
        out.peakAtWu = static_cast<float>(doc.number("peakAtWu", 0.0, nullptr, e));
        out.goneByWu = static_cast<float>(doc.number("goneByWu", 0.0, nullptr, e));
    }
};
}  // namespace

const LookVariant* Look::variant(const char* id) const {
    for (u32 i = 0; i < variantCount; ++i)
        if (std::strcmp(variants[i].id, id) == 0) return &variants[i];
    return nullptr;
}

bool loadLook(Look& out) {
    JsonDoc look;
    if (!look.parse(kLookJson, sizeof kLookJson - 1, "design/look.json")) return false;
    Reader r{look, "design/look.json"};
    out.bloomScales = look.numbers("bloom.sigmasRefPx", out.bloomSigmasRefPx, 6);
    if (out.bloomScales == 0) {
        RF_ERR("look: no bloom.sigmasRefPx");
        r.ok = false;
    }
    out.bleedStart = r.num("toneMap.bleedStart");
    out.bleedEnd = r.num("toneMap.bleedEnd");
    out.glowRadiusOverSigma = r.num("softLight.radiusOverSigma");
    out.glowCutoffRadii = r.num("softLight.cutoffRadii");
    out.poolMinRadiusRefPx = r.num("softLight.poolMinRadiusRefPx");
    out.maxKillPools = static_cast<u32>(r.num("softLight.maxKillPools"));
    out.jointFlatBelowDegrees = r.num("joints.flatBelowTurnDegrees");
    out.jointMaxWidthRatio = r.num("joints.maxWidthRatio");
    out.vignetteAmount = r.num("vignette.amount");
    out.vignetteInner = r.num("vignette.inner");
    out.vignetteOuter = r.num("vignette.outer");
    out.vignetteBuild = r.num("vignette.buildAmount");
    out.afterglowTauMs = r.num("afterglow.tauMs");
    out.afterglowStrength = r.num("afterglow.strength");
    out.aberrationOfDistortion = r.num("aberration.ofDistortion");
    out.aberrationCornerRefPx = r.num("aberration.cornerRefPx");
    out.shakeMaxRefPx = r.num("shake.maxRefPx");
    out.bgMaxLuminance = r.num("background.maxLuminance");
    out.bgLevel = r.num("background.gradient.level");
    r.pair("background.gradient.centre", out.bgCentre);
    out.bgRadius = r.num("background.gradient.radiusScreenHeights");
    out.bgPower = r.num("background.gradient.power");
    out.nebulaSigmaOverRadius = r.num("background.nebula.sigmaOverRadius");
    out.kickDecayMs = r.num("beat.kickDecayMs");
    out.snareDecayMs = r.num("beat.snareDecayMs");
    out.hatDecayMs = r.num("beat.hatDecayMs");
    out.exposureKick = r.num("beat.exposureKick");
    out.bloomKick = r.num("beat.bloomKick");
    out.zoomKick = r.num("beat.zoomKick");
    out.aberrationSnareRefPx = r.num("beat.aberrationSnareRefPx");
    out.dropBurstBloom = r.num("beat.dropBurst.bloom");
    out.dropBurstDecayMs = r.num("beat.dropBurst.decayMs");
    out.reduceFlashingScale = r.num("beat.reduceFlashingScale");
    out.latticeRippleRefPx = r.num("beat.latticeKickRippleRefPx");
    out.latticeRippleSpeedWu = r.num("beat.latticeKickRippleSpeedWu");
    out.latticeHatShimmer = r.num("beat.latticeHatShimmer");
    out.backgroundBass = r.num("beat.backgroundBass");
    out.backgroundHueDriftDegrees = r.num("beat.backgroundHueDriftDegrees");

    Look::Lines& l = out.lines;
    r.line("gridRest", l.gridRest);
    r.line("gridWarm", l.gridWarm);
    r.line("gridHot", l.gridHot);
    r.line("gridHotPeak", l.gridHotPeak);
    r.line("enemyHullTier1", l.enemyHullTier1);
    r.line("variantMark", l.variantMark);
    r.line("eliteWhiteCopy", l.eliteWhiteCopy);
    r.line("playerHull", l.playerHull);
    r.line("playerWhiteCopy", l.playerWhiteCopy);
    r.line("playerHalo", l.playerHalo);
    r.line("playerTrail", l.playerTrail);
    r.line("playerShot", l.playerShot);
    r.line("pulseRing", l.pulseRing);
    r.line("pulseRingEcho", l.pulseRingEcho);
    r.line("orbital", l.orbital);
    r.line("orbitalArc", l.orbitalArc);
    r.line("sentinel", l.sentinel);
    r.line("sentinelShot", l.sentinelShot);
    r.line("enemyBullet", l.enemyBullet);
    r.line("telegraph", l.telegraph);
    r.line("shard", l.shard);
    r.line("shockRing", l.shockRing);
    r.line("warpRing", l.warpRing);
    r.line("shatter", l.shatter);
    r.line("wellRing", l.wellRing);
    r.line("blinkFlare", l.blinkFlare);
    r.line("eliteFlare", l.eliteFlare);
    r.line("worldText", l.worldText);

    Look::Enemy& en = out.enemy;
    en.hullIntensityPerTier = r.num("enemy.hullIntensityPerTier");
    en.ornamentIntensityOfHull = r.num("enemy.ornamentIntensityOfHull");
    en.ornamentWidth = r.num("enemy.ornamentWidth");
    en.coreIntensityOfHull = r.num("enemy.coreIntensityOfHull");
    en.coreWidth = r.num("enemy.coreWidth");
    en.eliteIntensityAdd = r.num("enemy.eliteIntensityAdd");
    en.eliteWidth = r.num("enemy.eliteWidth");
    en.eliteGlass = r.num("enemy.eliteGlass");
    en.eliteAura = r.num("enemy.eliteAura");
    if (look.numbers("enemy.auraByTier", en.auraByTier, 5) != 5) {
        RF_ERR("look: enemy.auraByTier needs five values");
        r.ok = false;
    }
    en.auraRadiusOfExtent = r.num("enemy.auraRadiusOfExtent");
    en.kickSwell = r.num("enemy.kickSwell");
    en.hullTurnsPerBar = r.num("enemy.hullTurnsPerBar");
    en.ornamentTurnsPerBar = r.num("enemy.ornamentTurnsPerBar");

    Look::Grid& g = out.grid;
    g.warmFromWu = r.num("grid.warmFromWu");
    g.hotFromWu = r.num("grid.hotFromWu");
    g.warmTowardHot = r.num("grid.warmTowardHot");
    g.tintRadiusWu = r.num("grid.tintRadiusWu");
    g.tintNearPlayerMix = r.num("grid.tintNearPlayerMix");
    g.tintFarThirdHueMix = r.num("grid.tintFarThirdHueMix");

    out.threat.bulletWidth = r.num("fields.threatBullet.widthRefPx");
    out.threat.bulletRim = r.num("fields.threatBullet.rimRefPx");
    out.threat.telegraphWidth = r.num("fields.threatTelegraph.widthRefPx");
    out.threat.telegraphRim = r.num("fields.threatTelegraph.rimRefPx");
    out.flareFadeExponent = r.num("fields.flareArm.fadeExponent");
    out.priorityHull.width = r.num("fields.priorityShipHull.widthRefPx");
    out.priorityHull.rim = r.num("fields.priorityShipHull.rimRefPx");
    out.priorityHalo.width = r.num("fields.priorityShipHalo.widthRefPx");
    out.priorityHalo.rim = r.num("fields.priorityShipHalo.rimRefPx");

    Look::Crowd& cr = out.crowd;
    cr.restLoad = r.num("crowd.restLoad");
    cr.exponent = r.num("crowd.exponent");
    cr.minGain = r.num("crowd.minGain");
    cr.followSeconds = r.num("crowd.followSeconds");
    cr.swarmExponent = r.num("crowd.swarmExponent");
    out.afterglowSourceSigmaRefPx = r.num("afterglow.sourceSigmaRefPx");
    out.afterglowKnee = r.num("afterglow.knee");

    Look::SoftLights& s = out.soft;
    r.soft("starDust", s.starDust);
    r.soft("ember", s.ember);
    r.soft("shard", s.shard);
    r.soft("shotHead", s.shotHead);
    r.soft("enemyBullet", s.enemyBullet);
    r.soft("orbital", s.orbital);
    r.soft("wellSeed", s.wellSeed);
    r.soft("killFlashSmall", s.killFlashSmall);
    r.soft("killFlash", s.killFlash);
    r.soft("killPoolMedium", s.killPoolMedium);
    r.soft("killPoolLarge", s.killPoolLarge);
    r.soft("playerPool", s.playerPool);
    r.soft("wellPool", s.wellPool);

    Look::Explosions& b = out.boom;
    b.sparkDrag = r.num("explosions.sparkDragPerSecond");
    b.streakSeconds = r.num("explosions.streakSeconds");
    b.streakMaxWu = r.num("explosions.streakMaxWu");
    r.explosion("explosions.small", b.small);
    r.explosion("explosions.medium", b.medium);
    r.explosion("explosions.large", b.large);
    b.shatterS = r.num("explosions.large.shatterS");
    b.bossSparks = r.num("explosions.bossBreak.sparks");
    b.bossSpeedMax = r.num("explosions.bossBreak.speedWuMax");
    b.bossLifeMax = r.num("explosions.bossBreak.lifeSMax");
    b.bossShakeRefPx = r.num("explosions.bossBreak.shakeRefPx");
    b.bossHitStopMs = r.num("explosions.bossBreak.hitStopMs");
    r.range("explosions.shotImpact.sparks", b.impactSparks);
    b.impactLifeS = r.num("explosions.shotImpact.lifeS");
    b.warpFromGlyphExtents = r.num("explosions.warpIn.fromGlyphExtents");
    b.warpEliteFromWu = r.num("explosions.warpIn.eliteAndBossFromWu");
    b.warpBeats = r.num("explosions.warpIn.beats");
    b.maxShockRings = static_cast<u32>(r.num("explosions.maxShockRings"));

    Look::Background& bk = out.back;
    bk.nebulaOnScreen = r.num("background.nebula.onScreen");
    r.range("background.nebula.radiusWu", bk.nebulaRadiusWu);
    r.range("background.nebula.intensity", bk.nebulaIntensity);
    bk.nebulaParallax = r.num("background.nebula.parallax");
    bk.nebulaBassSwell = r.num("background.nebula.bassSwell");
    bk.starsOnScreen = r.num("background.starDust.onScreen");
    bk.starsParallax = r.num("background.starDust.parallax");
    bk.starsWhiteShare = r.num("background.starDust.whiteShare");
    bk.starsHatTwinkle = r.num("background.starDust.hatTwinkle");
    bk.starsSnareFlare = r.num("background.starDust.snareFlare");
    bk.starsSnareFlareMs = r.num("background.starDust.snareFlareMs");

    r.distortion("distortion.largeKill", out.distortion.largeKill);
    r.distortion("distortion.nova", out.distortion.nova);
    r.distortion("distortion.bossBreak", out.distortion.bossBreak);
    r.distortion("distortion.well", out.distortion.well);
    out.zoomMin = r.num("camera.zoomMin");
    out.zoomMax = r.num("camera.zoomMax");

    JsonDoc vars;
    if (!vars.parse(kVariantsJson, sizeof kVariantsJson - 1, "design/look_variants.json")) return false;
    Reader v{vars, "design/look_variants.json"};
    const JsonValue* list = vars.find("variants");
    out.variantCount = 0;
    for (const JsonValue* e = vars.firstChild(list); e && out.variantCount < 4; e = vars.nextSibling(e)) {
        LookVariant& lv = out.variants[out.variantCount++];
        vars.text("id", lv.id, sizeof lv.id, e);
        vars.text("name", lv.name, sizeof lv.name, e);
        lv.bloom = v.num("bloom", e);
        if (vars.numbers("bloomWeights", lv.bloomWeights, 6, e) != out.bloomScales) {
            RF_ERR("look: variant '%s' does not have one bloom weight per scale", lv.id);
            v.ok = false;
        }
        lv.exposure = v.num("exposure", e);
        lv.whitePoint = v.num("whitePoint", e);
        lv.bleed = v.num("bleed", e);
        lv.lineWidth = v.num("lineWidth", e);
        lv.poolLevel = v.num("poolLevel", e);
        lv.bgLevel = v.num("bgLevel", e);
        lv.afterglowMs = v.num("afterglowMs", e);
        lv.afterglowStrength = v.num("afterglowStrength", e);
    }
    if (out.variantCount == 0 || !out.variant("signal")) {
        RF_ERR("look: design/look_variants.json has no 'signal' variant");
        v.ok = false;
    }
    if (r.ok && v.ok) {
        RF_LOG("look: %u bloom scales, %u variants (%s%s%s)", out.bloomScales, out.variantCount, out.variants[0].id,
               out.variantCount > 1 ? ", ..." : "", "");
    }
    return r.ok && v.ok;
}

}  // namespace rf
