#pragma once
// The look (design/look.md v2) as loaded from design/look.json and design/look_variants.json, which
// are embedded in the binary at build time. Every number the renderer uses comes from here.

#include "core/types.h"

namespace rf {

class JsonDoc;

// One look variant: composite-level controls the renderer switches while running (look.md §8).
struct LookVariant {
    char id[16] = {};
    char name[24] = {};
    float bloom = 0.0f;
    float bloomWeights[6] = {};
    float exposure = 1.0f;
    float whitePoint = 6.0f;
    float bleed = 0.0f;
    float lineWidth = 1.0f;
    float poolLevel = 1.0f;
    float bgLevel = 1.0f;
    float afterglowMs = 0.0f;
    float afterglowStrength = 0.0f;
};

// A line entry of look.json "lines": HDR intensity and width in reference pixels.
struct LookLine {
    float intensity = 1.0f;
    float width = 1.0f;
};

// A soft light of look.json "softLights": radius (ref px for sparkles, wu for pools) and intensity,
// each a single value (lo == hi) or a range / a value at birth and at death.
struct LookSoftLight {
    float radius[2] = {};
    float intensity[2] = {};
    float lifeS = 0.0f;          // 0: the owner decides
    float largeScale = 1.0f;     // killFlash: radius x this for large kills
    bool squaredFade = false;    // fade as the square of the time left (else linear)
};

// One explosion size of look.json "explosions" (look.md §5).
struct LookExplosion {
    float sparks[2] = {};
    float speedWu[2] = {};
    float lifeS[2] = {};
    u32 rings = 0;
    float ringToWu = 48.0f;
    float ringS = 0.18f;
    float ringGapS = 0.0f;  // between consecutive rings of one kill
    u32 embers = 0;
    float emberLifeS = 1.2f;
    float gridImpulse = 0.0f;
    float shakeRefPx = 0.0f;
};

struct LookDistortionSource {
    float refPx = 0.0f;
    float widthRefPx = 0.0f;
    float lifeS = 0.35f;
    float toWu = 0.0f;
    float peakAtWu = 0.0f, goneByWu = 0.0f;
};

struct Look {
    // Bloom scales (look.md §3): sigma in reference pixels, finest first.
    float bloomSigmasRefPx[6] = {};
    u32 bloomScales = 0;
    // Tone map: the white point and bleed amount come from the variant; where the bleed starts and ends.
    float bleedStart = 1.0f;
    float bleedEnd = 8.0f;
    // Soft light.
    float glowRadiusOverSigma = 2.45f;
    float glowCutoffRadii = 1.25f;
    float poolMinRadiusRefPx = 16.0f;
    u32 maxKillPools = 0;
    // Joints (look.md §4).
    float jointFlatBelowDegrees = 20.0f;
    float jointMaxWidthRatio = 1.25f;
    // Composite (look.md §7).
    float vignetteAmount = 0.0f, vignetteInner = 0.0f, vignetteOuter = 1.0f, vignetteBuild = 0.0f;
    float afterglowTauMs = 0.0f, afterglowStrength = 0.0f;
    float aberrationOfDistortion = 0.0f, aberrationCornerRefPx = 0.0f;
    float shakeMaxRefPx = 0.0f;
    float bgMaxLuminance = 0.0f;
    float bgLevel = 0.0f, bgCentre[2] = {}, bgRadius = 0.0f, bgPower = 1.0f;
    float nebulaSigmaOverRadius = 0.0f;
    // Beat envelopes (look.md §7).
    float kickDecayMs = 0.0f, snareDecayMs = 0.0f, hatDecayMs = 0.0f;
    float exposureKick = 0.0f, bloomKick = 0.0f, zoomKick = 0.0f, aberrationSnareRefPx = 0.0f;
    float dropBurstBloom = 0.0f, dropBurstDecayMs = 0.0f;
    float reduceFlashingScale = 0.5f;
    float latticeRippleRefPx = 0.0f, latticeRippleSpeedWu = 0.0f, latticeHatShimmer = 0.0f;
    float backgroundBass = 0.0f, backgroundHueDriftDegrees = 0.0f;

    // Content numbers (look.md §2-§5) for the stage.
    struct Lines {
        LookLine gridRest, gridWarm, gridHot, gridHotPeak, enemyHullTier1, variantMark, eliteWhiteCopy, playerHull,
            playerWhiteCopy, playerHalo, playerTrail, playerShot, pulseRing, pulseRingEcho, orbital, orbitalArc, sentinel,
            sentinelShot, enemyBullet, telegraph, shard, shockRing, warpRing, shatter, wellRing, blinkFlare, eliteFlare,
            worldText;
    } lines;
    // Light budget (look.md v2.1 §3): the horde's light turns down as it grows.
    struct Crowd {
        float restLoad = 150.0f, exponent = 0.4f, minGain = 0.4f, followSeconds = 0.5f, swarmExponent = 1.5f;
    } crowd;
    // Priority layer ships (look.md §9): core width and rim in ref px.
    struct PriorityShip {
        float width = 2.3f, rim = 2.0f;
    } priorityHull, priorityHalo;
    // Afterglow source (look.md v2.1 §7): the bloom level of this sigma, less a knee on its brightest channel.
    float afterglowSourceSigmaRefPx = 6.0f;
    float afterglowKnee = 0.25f;
    // v2.2 (look.md §3, §7): the light-budget gain the afterglow's strength is multiplied by.
    enum class Gain : u8 { None, Crowd, Swarm };
    Gain afterglowGain = Gain::None;
    // v2.2 (look.md §3 player shots): a shot's capsule runs back from its head over speed x frameS +
    // overlapWu, never under minWu, so one frame's capsule overlaps the next.
    struct ShotStreak {
        float frameS = 1.0f / 60.0f, overlapWu = 8.0f, minWu = 16.0f;
    } shotStreak;
    // v2.2 (look.md §5): Drop confetti thrown in from beyond the view edge as a shower.
    struct DropConfetti {
        float outsideWu[2] = {}, delayS[2] = {}, speedWu[2] = {}, lifeS[2] = {};
    } dropConfetti;
    struct Enemy {
        float hullIntensityPerTier = 0.2f, ornamentIntensityOfHull = 0.8f, ornamentWidth = 1.5f;
        float coreIntensityOfHull = 1.1f, coreWidth = 1.5f;
        float eliteIntensityAdd = 0.6f, eliteWidth = 2.3f, eliteGlass = 0.07f, eliteAura = 0.22f;
        float auraByTier[5] = {};
        float auraRadiusOfExtent = 2.3f, kickSwell = 0.065f, hullTurnsPerBar = 0.25f, ornamentTurnsPerBar = -0.5f;
    } enemy;
    struct Grid {
        float warmFromWu = 5.0f, hotFromWu = 13.0f, warmTowardHot = 0.4f;
        float tintRadiusWu = 870.0f, tintNearPlayerMix = 0.45f, tintFarThirdHueMix = 0.3f;
    } grid;
    struct Threat {
        float bulletWidth = 2.1f, bulletRim = 1.5f, telegraphWidth = 1.6f, telegraphRim = 1.0f;
    } threat;
    float flareFadeExponent = 1.6f;
    struct SoftLights {
        LookSoftLight starDust, ember, shard, shotHead, enemyBullet, orbital, wellSeed, killFlashSmall, killFlash,
            killPoolMedium, killPoolLarge, playerPool, wellPool;
    } soft;
    struct Explosions {
        float sparkDrag = 3.2f, streakSeconds = 0.035f, streakMaxWu = 26.0f;
        LookExplosion small, medium, large;
        float shatterS = 0.5f;
        float bossSparks = 300.0f, bossSpeedMax = 1200.0f, bossLifeMax = 1.6f, bossShakeRefPx = 6.0f, bossHitStopMs = 50.0f;
        float impactSparks[2] = {3.0f, 4.0f};
        float impactLifeS = 0.15f;
        // Warp-in ring: starts at this many glyph extents (elites and bosses: a fixed radius).
        float warpFromGlyphExtents = 2.5f, warpEliteFromWu = 90.0f, warpBeats = 2.0f;
        u32 maxShockRings = 6;
    } boom;
    struct Background {
        float nebulaOnScreen = 6.0f, nebulaRadiusWu[2] = {350.0f, 500.0f}, nebulaIntensity[2] = {0.016f, 0.03f};
        float nebulaParallax = 0.25f, nebulaBassSwell = 0.25f;
        float starsOnScreen = 420.0f, starsParallax = 0.5f, starsWhiteShare = 0.5f, starsHatTwinkle = 0.3f;
        float starsSnareFlare = 0.6f, starsSnareFlareMs = 80.0f;
    } back;
    struct Distortion {
        LookDistortionSource largeKill, nova, bossBreak, well;
    } distortion;
    float zoomMin = 0.85f, zoomMax = 1.25f;

    LookVariant variants[4];
    u32 variantCount = 0;

    const LookVariant* variant(const char* id) const;
};

// Parses the embedded design files. False (after logging what is missing) when a value the renderer
// needs is absent, so a renamed key in the design data fails loudly at start-up.
bool loadLook(Look& out);

}  // namespace rf
