#include "game/tiers.h"

#include <cstdio>
#include <cstring>

#include "core/save_file.h"

namespace rf {

TierSettings tierSettings(Tier t) {
    TierSettings s;
    s.tier = t;
    switch (t) {
        case Tier::Low:
            // TECH_SPEC 5.4 Low. Glass and the 24 kill pools of look.md §3 stay: dropping them are cuts 1
            // and 7 of look.md §10 (applyCuts), made only when a device cannot hold its rate.
            s.name = "low";
            s.render = {4, 2, 0, false};
            s.particleDensity = 0.35f;
            s.particleRing = 16384;
            s.background = false;
            s.bulletSparkles = false;
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

void applyCuts(TierSettings& s, u32 n) {
    s.cuts = n;
    for (u32 k = 1; k <= n && k <= kMaxCuts; ++k) {
        switch (k) {
            case 1: s.glass = false; break;
            case 2: s.bulletSparkles = false; break;
            case 3:
                s.nebula = false;
                s.starTwinkle = false;
                break;
            case 4: s.render.aberration = false; break;
            case 5: s.render.distortionShift = 0; break;
            case 6: s.render.afterglowShift = 0; break;
            case 7: s.killPools = 12; break;
            case 8:
                if (s.render.bloomLevels > 4) --s.render.bloomLevels;
                break;
            case 9: s.particleDensity *= 0.5f; break;
            case 10:
                s.gridSpacingWu += 8.0f;
                s.gridTint = false;
                break;
            case 11:
                if (s.lodRadiusPx < 21.0f) s.lodRadiusPx = 21.0f;  // the standard enemy at zoom 1
                break;
            default: break;
        }
    }
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

namespace {
// "key=value\n" lines; value copied without the line end. False when the key is absent.
bool cacheValue(const char* text, const char* key, char* out, size_t cap) {
    const size_t n = std::strlen(key);
    for (const char* line = text; line && *line;) {
        if (std::strncmp(line, key, n) == 0 && line[n] == '=') {
            const char* v = line + n + 1;
            size_t len = std::strcspn(v, "\r\n");
            if (len + 1 > cap) len = cap - 1;
            std::memcpy(out, v, len);
            out[len] = '\0';
            return true;
        }
        line = std::strchr(line, '\n');
        if (line) ++line;
    }
    return false;
}
}  // namespace

bool readTierCache(const char* path, const char* gpu, const char* driver, Tier& out) {
    FILE* f = std::fopen(path, "rb");
    if (!f) return false;
    char text[1024];
    const size_t n = std::fread(text, 1, sizeof text - 1, f);
    std::fclose(f);
    text[n] = '\0';
    char g[256], d[64], t[16];
    if (std::strncmp(text, "raveform-tier 1", 15) != 0 || !cacheValue(text, "gpu", g, sizeof g) ||
        !cacheValue(text, "driver", d, sizeof d) || !cacheValue(text, "tier", t, sizeof t))
        return false;
    // A new GPU or driver is probed again.
    if (std::strcmp(g, gpu) != 0 || std::strcmp(d, driver) != 0) return false;
    return parseTier(t, out);
}

bool writeTierCache(const char* path, const char* gpu, const char* driver, Tier chosen, const float msPerTier[4],
                    float budgetMs) {
    // M7: written whole through a temporary file and a rename (core/save_file); a damaged file is probed again (readTierCache).
    char text[1024];
    const int n = std::snprintf(text, sizeof text,
                                "raveform-tier 1\ngpu=%s\ndriver=%s\ntier=%s\nbudget_ms=%.2f\nms_low_medium_high_ultra=%.2f %.2f %.2f %.2f\n", gpu,
                                driver, tierName(chosen), budgetMs, msPerTier[0], msPerTier[1], msPerTier[2], msPerTier[3]);
    return n > 0 && n < static_cast<int>(sizeof text) && saveFileAtomic(path, text, static_cast<size_t>(n));
}

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
