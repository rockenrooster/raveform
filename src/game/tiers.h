#pragma once
// Effect tiers (TECH_SPEC 5.4, design/look.md §10) and governor v1.

#include "core/types.h"
#include "gfx/renderer.h"

namespace rf {

enum class Tier : u8 { Low = 0, Medium = 1, High = 2, Ultra = 3 };

struct TierSettings {
    Tier tier = Tier::High;
    const char* name = "high";
    RenderTier render;
    float particleDensity = 1.0f;  // spark counts x this
    u32 particleRing = 65536;      // informational: the ring size this tier is sized for
    bool background = true;        // nebula clouds and star dust (Low: the gradient alone)
    bool nebula = true;            // the clouds (cut 3 keeps the gradient and the stars)
    bool starTwinkle = true;       // star dust twinkles on hats and flares on snares (cut 3: static)
    bool bulletSparkles = true;    // sparkles on enemy bullets and Shards (High and up)
    bool glass = true;
    u32 killPools = 24;
    bool aurasBelowElite = true;
    float gridSpacingWu = 20.0f;
    bool gridTint = true;          // the lattice tint field (else the flat grid colour)
    float lodRadiusPx = 0.0f;      // glyphs drawn smaller than this lose layers 1 and 2
    float targetHz = 0.0f;         // 0: the panel maximum
    u32 cuts = 0;                  // design/look.md §10 cuts applied on top of the tier (applyCuts)
};

TierSettings tierSettings(Tier t);
// design/look.md §10 "What gives way first", in order, the first n of them on top of a tier: 1 glass,
// 2 sparkles on enemy bullets and Shards, 3 nebula and star-dust twinkle, 4 chromatic aberration,
// 5 distortion, 6 afterglow, 7 twelve kill pools instead of twenty-four, 8 one bloom scale from the
// coarse end (never below four), 9 spark density (halved), 10 lattice (coarser spacing, flat grid
// colour), 11 ornament layers on small glyphs. Never resolution.
void applyCuts(TierSettings& s, u32 n);
constexpr u32 kMaxCuts = 11;
bool parseTier(const char* name, Tier& out);
const char* tierName(Tier t);

// First-launch tier choice (TECH_SPEC 5.4): the start-up probe's result, cached in a small text file
// per GPU and driver. Start-up only (file I/O). msPerTier: GPU work at the top clock, Low..Ultra,
// negative when not measured.
bool readTierCache(const char* path, const char* gpu, const char* driver, Tier& out);
bool writeTierCache(const char* path, const char* gpu, const char* driver, Tier chosen, const float msPerTier[4],
                    float budgetMs);

// Governor v1 (TECH_SPEC 5.4): steps down after 0.5 s over budget, up after 10 s with room to spare.
// First the spark density within the tier, then the tier. Off in benchmarks (fixed tier).
// Inputs are what can be trusted on the phones: the share of recent frames that missed a refresh
// (present intervals above 1.5 periods; the raw present-call P95 jitters above the period with no frame
// late) and the GPU work per frame, which the GPU's own clock governor holds near 85% of the period
// whenever it can, so "room to spare" means clearly below that and "over" means saturated.
class Governor {
public:
    void reset(Tier start, float periodMs);
    // Returns true when the tier changed (read tier()); density() changes without a tier change.
    bool update(float dtSeconds, float missedFraction, float gpuMs, int thermalStatus);
    Tier tier() const { return tier_; }
    float density() const { return density_; }
    u32 stepsDown() const { return down_; }
    u32 stepsUp() const { return up_; }

private:
    Tier tier_ = Tier::High;
    float budgetMs_ = 16.7f;
    float density_ = 1.0f;
    float over_ = 0.0f;
    float under_ = 0.0f;
    u32 down_ = 0, up_ = 0;
};

}  // namespace rf
