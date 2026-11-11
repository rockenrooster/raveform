#include "game/tiers.h"

#include <cstring>

namespace rf {

TierSettings tierSettings(Tier t) {
    TierSettings s;
    s.tier = t;
    switch (t) {
        case Tier::Low:
            s.name = "low";
            s.render = {4, 2, 0, false};
            s.particleDensity = 0.35f;
            s.particleRing = 16384;
            s.background = false;
            s.bulletSparkles = false;
            s.glass = false;
            s.killPools = 12;
            s.aurasBelowElite = false;
            s.gridSpacingWu = 32.0f;
            s.lodRadiusPx = 14.0f;
            s.targetHz = 60.0f;
            break;
        case Tier::Medium:
            s.name = "medium";
            s.render = {5, 1, 3, false};
            s.particleDensity = 0.6f;
            s.particleRing = 32768;
            s.bulletSparkles = false;
            s.gridSpacingWu = 24.0f;
            break;
        case Tier::High:
            s.name = "high";
            s.render = {6, 1, 3, true};
            s.particleDensity = 1.0f;
            s.particleRing = 65536;
            s.gridSpacingWu = 20.0f;
            break;
        case Tier::Ultra:
            s.name = "ultra";
            s.render = {6, 1, 2, true, 1};
            s.particleDensity = 1.5f;
            s.particleRing = 131072;
            s.gridSpacingWu = 16.0f;
            break;
    }
    return s;
}

bool parseTier(const char* name, Tier& out) {
    const char* names[4] = {"low", "medium", "high", "ultra"};
    for (u32 i = 0; i < 4; ++i)
        if (std::strcmp(name, names[i]) == 0) {
            out = static_cast<Tier>(i);
            return true;
        }
    return false;
}

const char* tierName(Tier t) { return tierSettings(t).name; }

void Governor::reset(Tier start, float periodMs) {
    tier_ = start;
    budgetMs_ = periodMs;
    density_ = 1.0f;
    over_ = under_ = 0.0f;
    down_ = up_ = 0;
}

bool Governor::update(float dt, float missedFraction, float gpuMs, int thermalStatus) {
    // Over budget: more than 5% of recent frames missed a refresh, the GPU is saturated, or the phone
    // is throttling (Android thermal status SEVERE = 3 and up).
    const bool over = missedFraction > 0.05f || gpuMs > budgetMs_ * 0.97f || thermalStatus >= 3;
    const bool comfortable = missedFraction <= 0.0f && gpuMs > 0.0f && gpuMs < budgetMs_ * 0.75f && thermalStatus < 2;
    over_ = over ? over_ + dt : 0.0f;
    under_ = comfortable ? under_ + dt : 0.0f;
    if (over_ >= 0.5f) {
        over_ = 0.0f;
        ++down_;
        if (density_ > 0.55f) {
            density_ -= 0.25f;  // first the spark density within the tier
            return false;
        }
        if (tier_ != Tier::Low) {
            tier_ = static_cast<Tier>(static_cast<u8>(tier_) - 1);
            density_ = 1.0f;
            return true;
        }
        return false;
    }
    if (under_ >= 10.0f) {
        under_ = 0.0f;
        ++up_;
        if (density_ < 1.0f) {
            density_ = density_ + 0.25f > 1.0f ? 1.0f : density_ + 0.25f;
            return false;
        }
        if (tier_ != Tier::Ultra) {
            tier_ = static_cast<Tier>(static_cast<u8>(tier_) + 1);
            density_ = 0.5f;  // come back in carefully
            return true;
        }
    }
    return false;
}

}  // namespace rf
