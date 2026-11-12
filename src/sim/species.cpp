#include "sim/species.h"

#include <cstdio>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::sim {

namespace {
const char kSpeciesJson[] =
#include "species.json.inc"
    ;

const char* const kFamilyNames[kFamilyCount] = {"drifter", "hound", "shoal", "ram", "carom", "sentry"};
}  // namespace

const char* familyName(Family f) {
    const u32 i = static_cast<u32>(f);
    return i < kFamilyCount ? kFamilyNames[i] : "?";
}

i32 SpeciesTable::find(const char* id) const {
    for (u32 i = 0; i < kCount; ++i)
        if (std::strcmp(species_[i].id, id) == 0) return static_cast<i32>(i);
    return -1;
}

bool SpeciesTable::load() {
    JsonDoc doc;
    if (!doc.parse(kSpeciesJson, sizeof kSpeciesJson - 1, "design/species.json")) return false;
    bool ok = true;
    const auto need = [&](u32 got, u32 want, const char* what) {
        if (got != want) {
            RF_ERR("species: %s: expected %u values, found %u", what, want, got);
            ok = false;
        }
    };

    // Tier and variant multipliers.
    float health[5], speed[5], score[5], threat[5], shards[5], radius[5];
    need(doc.numbers("tierMultipliers.health", health, 5), 5, "tierMultipliers.health");
    need(doc.numbers("tierMultipliers.speed", speed, 5), 5, "tierMultipliers.speed");
    need(doc.numbers("tierMultipliers.score", score, 5), 5, "tierMultipliers.score");
    need(doc.numbers("tierMultipliers.threat", threat, 5), 5, "tierMultipliers.threat");
    need(doc.numbers("tierMultipliers.shards", shards, 5), 5, "tierMultipliers.shards");
    need(doc.numbers("tierMultipliers.radius", radius, 5), 5, "tierMultipliers.radius");
    float vScore[3], vThreat[3];
    static const char* const kVariants[3] = {"A", "B", "C"};
    for (u32 v = 0; v < 3; ++v) {
        char path[64];
        bool a = true, b = true;
        std::snprintf(path, sizeof path, "variantMultipliers.%s.score", kVariants[v]);
        vScore[v] = static_cast<float>(doc.number(path, 1.0, &a));
        std::snprintf(path, sizeof path, "variantMultipliers.%s.threat", kVariants[v]);
        vThreat[v] = static_cast<float>(doc.number(path, 1.0, &b));
        if (!a || !b) {
            RF_ERR("species: variantMultipliers.%s is incomplete", kVariants[v]);
            ok = false;
        }
    }

    // Families with behaviour code.
    for (u32 f = 0; f < kFamilyCount; ++f) {
        const JsonValue* fam = doc.find("families");
        fam = fam ? doc.member(fam, kFamilyNames[f]) : nullptr;
        if (!fam) {
            RF_ERR("species: family '%s' is missing", kFamilyNames[f]);
            ok = false;
            continue;
        }
        FamilyData& d = families_[f];
        bool k = true;
        d.health = static_cast<float>(doc.number("health", 1.0, &k, fam));
        d.speed = static_cast<float>(doc.number("speed", 0.0, &k, fam));
        d.radius = static_cast<float>(doc.number("radius", 14.0, &k, fam));
        d.score = static_cast<float>(doc.number("score", 0.0, &k, fam));
        d.threat = static_cast<float>(doc.number("threat", 1.0, &k, fam));
        char role[16] = {};
        doc.text("hueRole", role, sizeof role, fam);
        d.hue = std::strcmp(role, "fodder") == 0 ? 0 : (std::strcmp(role, "hunter") == 0 ? 1 : 2);
        const JsonValue* m = doc.member(fam, "motion");
        switch (static_cast<Family>(f)) {
            case Family::Drifter: d.wanderTurn = static_cast<float>(doc.number("wanderTurn", 0.0, &k, m)); break;
            case Family::Hound: d.turnRate = static_cast<float>(doc.number("turnRate", 0.0, &k, m)); break;
            case Family::Shoal: {
                d.flockRadius = static_cast<float>(doc.number("flockRadius", 0.0, &k, m));
                d.separation = static_cast<float>(doc.number("separation", 0.0, &k, m));
                float g[2] = {};
                if (doc.numbers("groupSize", g, 2, m) != 2) k = false;
                d.groupMin = static_cast<u32>(g[0]);
                d.groupMax = static_cast<u32>(g[1]);
                break;
            }
            case Family::Ram:
                d.dashSpeed = static_cast<float>(doc.number("dashSpeed", 0.0, &k, m));
                d.telegraphBeats = static_cast<float>(doc.number("telegraphBeats", 1.0, &k, m));
                d.dashBeats = static_cast<float>(doc.number("dashBeats", 1.0, &k, m));
                d.restBeats = static_cast<float>(doc.number("restBeats", 2.0, &k, m));
                break;
            case Family::Carom:
                d.speedGainPerBounce = static_cast<float>(doc.number("speedGainPerBounce", 0.0, &k, m));
                d.maxSpeedFactor = static_cast<float>(doc.number("maxSpeedFactor", 1.0, &k, m));
                break;
            case Family::Sentry:
                d.patternArms = static_cast<u32>(doc.number("patternArms", 0.0, &k, m));
                d.fireEverySteps = static_cast<u32>(doc.number("fireEverySteps", 2.0, &k, m));
                d.bulletSpeed = static_cast<float>(doc.number("bulletSpeed", 0.0, &k, m));
                break;
            default: break;
        }
        if (!k) {
            RF_ERR("species: family '%s' lacks a number the game reads", kFamilyNames[f]);
            ok = false;
        }
    }

    // Every family x tier x variant (GAME_DESIGN 7.1).
    for (u32 f = 0; f < kFamilyCount; ++f) {
        const FamilyData& d = families_[f];
        for (u32 t = 1; t <= 5; ++t) {
            for (u32 v = 0; v < 3; ++v) {
                Species& s = species_[index(static_cast<Family>(f), t, v)];
                std::snprintf(s.id, sizeof s.id, "%s_%u_%s", kFamilyNames[f], t, kVariants[v]);
                s.family = static_cast<Family>(f);
                s.tier = static_cast<u8>(t);
                s.variant = static_cast<u8>(v);
                s.hue = d.hue;
                s.health = d.health * health[t - 1];
                s.speed = d.speed * speed[t - 1];
                // species.json: "radius is the collision and visual radius in world units before the
                // glyph's own size factor". Its values already equal 14 wu x the glyph's size (shoal 8 ~
                // 14 x 0.55, ram 18 ~ 14 x 1.25, sentry 16 ~ 14 x 1.15), so the glyph is drawn at this
                // radius and the size factor is not applied a second time (docs/PROGRESS.md, M3).
                s.radius = d.radius * radius[t - 1];
                s.score = d.score * score[t - 1] * vScore[v];
                s.threat = d.threat * threat[t - 1] * vThreat[v];
                s.shards = static_cast<u8>(shards[t - 1] + 0.5f);
            }
        }
    }

    // Enemy bullets.
    {
        bool k = true;
        bulletSpeed = static_cast<float>(doc.number("enemyBullet.speed", 240.0, &k));
        bulletRadius = static_cast<float>(doc.number("enemyBullet.radius", 5.0, &k));
        if (!k) {
            RF_ERR("species: enemyBullet is incomplete");
            ok = false;
        }
    }

    // First slice: roster, debuts, budgets.
    rosterCount = 0;
    const JsonValue* rosterJson = doc.find("firstSlice.roster");
    for (const JsonValue* e = doc.firstChild(rosterJson); e && rosterCount < kMaxRoster; e = doc.nextSibling(e)) {
        char id[24] = {};
        doc.text("species", id, sizeof id, e);
        const i32 s = find(id);
        if (s < 0) {
            RF_ERR("species: roster entry '%s' is not a family the game knows", id);
            ok = false;
            continue;
        }
        RosterEntry& r = roster[rosterCount++];
        r.species = static_cast<u16>(s);
        r.track = static_cast<u8>(doc.number("debut.track", 1.0, nullptr, e));
        r.cycle = static_cast<u8>(doc.number("debut.cycle", 1.0, nullptr, e));
    }
    budgetCycles[0] = doc.numbers("firstSlice.threatBudgetPerBar.track1", budget[0], 8);
    budgetCycles[1] = doc.numbers("firstSlice.threatBudgetPerBar.track2", budget[1], 8);
    budgetCycles[2] = doc.numbers("firstSlice.threatBudgetPerBar.track3", budget[2], 8);
    // The crowd (firstSlice.crowd).
    {
        Crowd& c = crowd;
        static const char* const kT[3] = {"track1", "track2", "track3"};
        char path[96];
        for (u32 t = 0; t < 3; ++t) {
            std::snprintf(path, sizeof path, "firstSlice.crowd.aliveTarget.%s", kT[t]);
            c.targetCycles[t] = doc.numbers(path, c.target[t], 8);
        }
        bool got = true;
        const auto num = [&](const char* p, float& out) {
            bool g = true;
            out = static_cast<float>(doc.number(p, out, &g));
            got = got && g;
        };
        num("firstSlice.crowd.loopStepPerTrack", c.loopStep);
        num("firstSlice.crowd.max", c.max);
        num("firstSlice.crowd.sectionScale.groove", c.groove);
        num("firstSlice.crowd.sectionScale.break", c.breakScale);
        num("firstSlice.crowd.sectionScale.bossDrop", c.bossDrop);
        num("firstSlice.crowd.count.frameScale", c.countFrameScale);
        num("firstSlice.crowd.arrive.ringFrameScale", c.ringFrameScale);
        num("firstSlice.crowd.arrive.cornerOutsideWu", c.cornerOutsideWu);
        num("firstSlice.crowd.refillBars", c.refillBars);
        num("firstSlice.crowd.stray.frameScale", c.strayFrameScale);
        float v[2] = {};
        if (doc.numbers("firstSlice.crowd.sectionScale.build", v, 2) == 2) { c.build[0] = v[0]; c.build[1] = v[1]; } else { got = false; }
        if (doc.numbers("firstSlice.crowd.sectionScale.drop", v, 2) == 2) { c.drop[0] = v[0]; c.drop[1] = v[1]; } else { got = false; }
        if (doc.numbers("firstSlice.crowd.formationSize", v, 2) == 2) { c.formationMin = static_cast<u32>(v[0]); c.formationMax = static_cast<u32>(v[1]); } else { got = false; }
        if (doc.numbers("firstSlice.crowd.arrive.floodOutsideWu", v, 2) == 2) { c.floodOutsideWu[0] = v[0]; c.floodOutsideWu[1] = v[1]; } else { got = false; }
        float bars = static_cast<float>(c.strayBars);
        num("firstSlice.crowd.stray.bars", bars);
        c.strayBars = static_cast<u32>(bars);
        if (!got || !c.targetCycles[0] || !c.targetCycles[1] || !c.targetCycles[2]) {
            RF_ERR("species: firstSlice.crowd is incomplete");
            ok = false;
        }
    }
    if (rosterCount == 0 || budgetCycles[0] == 0 || budgetCycles[1] == 0 || budgetCycles[2] == 0) {
        RF_ERR("species: firstSlice roster or budgets missing");
        ok = false;
    }
    if (ok) {
        RF_LOG("species: %u species from %u families, roster of %u, budgets %u + %u + %u cycles, crowd %.0f..%.0f", kCount, kFamilyCount,
               rosterCount, budgetCycles[0], budgetCycles[1], budgetCycles[2], crowd.target[0][0], crowd.target[2][crowd.targetCycles[2] ? crowd.targetCycles[2] - 1 : 0]);
    }
    if (!boss.load()) ok = false;
    return ok;
}

}  // namespace rf::sim
