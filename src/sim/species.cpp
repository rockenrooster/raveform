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

const char* const kFamilyNames[kFamilyCount] = {"drifter", "hound", "shoal",  "ram",    "carom",  "sentry", "flanker", "skitter",
                                                "moth",    "brood", "blink",  "wyrm",  "tether", "weaver", "well",    "mimic"};
}  // namespace

u64 speciesDataDigest() {
    // FNV-1a of the embedded species.json (M4: co-op peers must run the same content).
    u64 h = 1469598103934665603ull;
    for (const char* c = kSpeciesJson; *c; ++c) {
        h ^= static_cast<u8>(*c);
        h *= 1099511628211ull;
    }
    return h;
}

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

    // A number under `obj` the game reads (M5): missing, logged and the family marked incomplete.
    const auto num = [&](const JsonValue* obj, const char* key, bool& k) -> float {
        bool g = obj != nullptr;
        const double v = obj ? doc.number(key, 0.0, &g, obj) : 0.0;
        if (!g) {
            RF_ERR("species: '%s' is missing", key);
            k = false;
        }
        return static_cast<float>(v);
    };
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
            // M5 part 2.
            case Family::Flanker:
                d.orbitRadius = num(m, "orbitRadius", k);
                d.closeRate = num(m, "closeRate", k);
                break;
            case Family::Skitter:
                d.dodgeDistance = num(m, "dodgeDistance", k);
                d.dodgeCooldownBeats = num(m, "dodgeCooldownBeats", k);
                break;
            case Family::Moth:
                d.orbitRadius = num(m, "orbitRadius", k);
                d.closePerBar = num(m, "closePerBar", k);
                d.turnsPerBar = num(m, "turnsPerBar", k);
                d.minRadius = num(m, "minRadius", k);
                break;
            case Family::Brood:
                d.miniRadius = num(m, "miniRadius", k);
                d.miniHealth = num(m, "miniHealth", k);
                d.miniSpeed = num(m, "miniSpeed", k);
                d.miniScore = num(m, "miniScore", k);
                d.maxMinisEach = static_cast<u32>(num(m, "maxMinisEach", k));
                break;
            case Family::Blink:
                d.hopEveryBeats = num(m, "hopEveryBeats", k);
                d.hopWu = num(m, "hopWu", k);
                d.hops = static_cast<u32>(num(m, "hops", k));
                d.strikeTellBeats = num(m, "strikeTellBeats", k);
                d.strikeSpeed = num(m, "strikeSpeed", k);
                d.strikeBeats = num(m, "strikeBeats", k);
                d.restBeats = num(m, "restBeats", k);
                break;
            // M5 part 4.
            case Family::Wyrm: {
                float sg[2] = {};
                if (doc.numbers("segments", sg, 2, m) != 2) k = false;
                d.segmentsMin = static_cast<u32>(sg[0]);
                d.segmentsMax = static_cast<u32>(sg[1]);
                d.segmentSpacing = num(m, "segmentSpacing", k);
                d.waveAmplitude = num(m, "waveAmplitude", k);
                d.wavePeriodBeats = num(m, "wavePeriodBeats", k);
                d.turnRate = num(m, "turnRate", k);
                break;
            }
            case Family::Tether:
                d.pairDistance = num(m, "pairDistance", k);
                d.orbitTurnsPerBar = num(m, "orbitTurnsPerBar", k);
                d.beamWidth = num(m, "beamWidth", k);
                break;
            case Family::Weaver:
                d.curveTurnRate = num(m, "curveTurnRate", k);
                d.trailBars = num(m, "trailBars", k);
                d.trailSegments = static_cast<u32>(num(m, "trailSegments", k));
                d.trailWidth = num(m, "trailWidth", k);
                break;
            case Family::Well:
                d.pullRadius = num(m, "pullRadius", k);
                d.pullPeakAt = num(m, "pullPeakAt", k);
                d.pullSpeed = num(m, "pullSpeed", k);
                d.shotBend = num(m, "shotBend", k);
                break;
            case Family::Mimic: d.beatStepWu = num(m, "beatStepWu", k); break;
            default: break;
        }
        // Variants B and C (variants.<family>.B / .C; GAME_DESIGN 7.2): the numbers of the variants built so far.
        {
            const JsonValue* vb = doc.find("variants");
            vb = vb ? doc.member(vb, kFamilyNames[f]) : nullptr;
            const JsonValue* B = vb ? doc.member(vb, "B") : nullptr;
            const JsonValue* C = vb ? doc.member(vb, "C") : nullptr;
            FamilyData::Variants& v = d.v;
            switch (static_cast<Family>(f)) {
                case Family::Drifter:
                    v.aimedShotEveryBars = num(B, "aimedShotEveryBars", k);
                    v.aimedBullets = static_cast<u32>(num(B, "bullets", k));
                    v.splitInto = static_cast<u32>(num(C, "splitInto", k));
                    v.childTierDrop = static_cast<u32>(num(C, "childTierDrop", k));
                    break;
                case Family::Hound:
                    v.shieldArcDeg = num(C, "shieldArcDegrees", k);
                    v.shieldTurnRate = num(C, "shieldTurnRate", k);
                    break;
                case Family::Carom:
                    v.bulletsPerBounce = num(B, "bulletsPerBounce", k);
                    v.bounceFanDeg = num(B, "fanDegrees", k);
                    v.splitEveryBounces = static_cast<u32>(num(C, "splitEveryBounces", k));
                    v.maxSplits = static_cast<u32>(num(C, "maxSplits", k));
                    break;
                case Family::Moth:
                    v.petals = static_cast<u32>(num(B, "petals", k));
                    v.petalEverySteps = static_cast<u32>(num(B, "everySteps", k));
                    v.petalRotateDeg = num(B, "rotateDegrees", k);
                    v.petalSpeed = num(B, "bulletSpeed", k);
                    v.burstBullets = static_cast<u32>(num(C, "burstBullets", k));
                    v.burstAtRadius = num(C, "atRadius", k);
                    break;
                case Family::Brood:
                    v.hatchEveryBars = num(B, "hatchEveryBars", k);
                    v.hatchCount = static_cast<u32>(num(B, "hatchCount", k));
                    v.swarmOnDeath = static_cast<u32>(num(C, "swarmOnDeath", k));
                    break;
                case Family::Blink:
                    v.pathBullets = static_cast<u32>(num(B, "pathBullets", k));
                    v.pathBulletSpeed = num(B, "bulletSpeed", k);
                    v.decoyLifeBeats = num(C, "decoyLifeBeats", k);
                    break;
                case Family::Flanker:
                    v.sideShotEverySteps = static_cast<u32>(num(B, "sideShotEverySteps", k));
                    v.sideBullets = static_cast<u32>(num(B, "bullets", k));
                    v.speedUpPerDeath = num(C, "speedUpPerDeath", k);
                    v.speedMaxFactor = num(C, "maxFactor", k);
                    v.speedWithinWu = num(C, "withinWu", k);
                    break;
                case Family::Skitter:
                    v.mineLifeBars = num(B, "mineLifeBars", k);
                    v.mineRadius = num(B, "mineRadius", k);
                    v.blinkHopWu = num(C, "blinkHopWu", k);
                    v.blinkCooldownBeats = num(C, "cooldownBeats", k);
                    break;
                case Family::Shoal:
                    v.streamLines = static_cast<u32>(num(B, "streamLines", k));
                    v.lineSpeed = num(B, "lineSpeed", k);
                    v.spearTellBeats = num(C, "tellBeats", k);
                    v.spearSpeed = num(C, "spearSpeed", k);
                    break;
                case Family::Ram:
                    v.wakeBars = num(B, "wakeBars", k);
                    v.wakeWidth = num(B, "wakeWidth", k);
                    v.reboundDashes = static_cast<u32>(num(C, "reboundDashes", k));
                    break;
                case Family::Sentry:
                    v.domeRadius = num(C, "domeRadius", k);
                    v.domeWithinWu = num(C, "shieldsNeighboursWithinWu", k);
                    break;
                // M5 part 4.
                case Family::Wyrm:
                    v.sideShotEverySteps = static_cast<u32>(num(B, "sideShotEverySteps", k));
                    v.fromEveryNthSegment = static_cast<u32>(num(B, "fromEveryNthSegment", k));
                    v.regrowHeadBeats = num(C, "regrowHeadBeats", k);
                    break;
                case Family::Tether:
                    v.orbitB = num(B, "orbitTurnsPerBar", k);
                    v.triad = C && doc.member(C, "triad") != nullptr;
                    v.triadSideWu = num(C, "sideWu", k);
                    break;
                case Family::Weaver:
                    v.loopEveryBars = num(B, "loopEveryBars", k);
                    v.detonateOnBarLine = true;
                    v.trailBlastWu = num(C, "blastRadius", k);
                    break;
                case Family::Well:
                    v.returnOnBeat = num(B, "returnOnBeat", k);
                    v.ringBullets = static_cast<u32>(num(B, "ringBullets", k));
                    v.growPerShots = num(C, "growPerShotsSwallowed", k);
                    v.burstAtScale = num(C, "burstAtScale", k);
                    v.wellBurstBullets = static_cast<u32>(num(C, "burstBullets", k));
                    break;
                case Family::Mimic:
                    v.everyNthShot = static_cast<u32>(num(B, "everyNthShot", k));
                    v.copyDashAfterBeats = num(C, "copiesDashAfterBeats", k);
                    break;
                default: break;
            }
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

    // M5: the Brood's mini (families.brood.motion.mini*): fodder that chases like a Hound, drawn with brood_1.
    {
        const FamilyData& d = families_[static_cast<u32>(Family::Brood)];
        Species& s = species_[kBroodling];
        std::snprintf(s.id, sizeof s.id, "broodling");
        s.family = Family::Brood;
        s.tier = 1;
        s.variant = kVariantA;
        s.hue = 0;  // "its minis ... count as fodder" (look.md 2: hue 1)
        s.health = d.miniHealth;
        s.speed = d.miniSpeed;
        s.radius = d.miniRadius;
        s.score = d.miniScore;
        s.threat = 0.0f;
        s.shards = 1;
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
        if (s < 0 || static_cast<u32>(s) >= kFamilySpecies) {
            RF_ERR("species: roster entry '%s' is not a family the game knows", id);
            ok = false;
            continue;
        }
        RosterEntry& r = roster[rosterCount++];
        r.species = static_cast<u16>(s);
        r.track = static_cast<u8>(doc.number("debut.track", 1.0, nullptr, e));
        r.cycle = static_cast<u8>(doc.number("debut.cycle", 1.0, nullptr, e));
    }
    // M5: budgets and crowd targets for every Track of the set list (track1 .. track10).
    budgetTracks = 0;
    for (u32 t = 0; t < kMaxTracks; ++t) {
        char path[96];
        std::snprintf(path, sizeof path, "firstSlice.threatBudgetPerBar.track%u", t + 1);
        budgetCycles[t] = doc.numbers(path, budget[t], 8);
        if (budgetCycles[t]) budgetTracks = t + 1;
    }
    // The crowd (firstSlice.crowd).
    {
        Crowd& c = crowd;
        char path[96];
        c.targetTracks = 0;
        for (u32 t = 0; t < kMaxTracks; ++t) {
            std::snprintf(path, sizeof path, "firstSlice.crowd.aliveTarget.track%u", t + 1);
            c.targetCycles[t] = doc.numbers(path, c.target[t], 8);
            if (c.targetCycles[t]) c.targetTracks = t + 1;
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
    for (u32 t = 0; t < budgetTracks; ++t)
        if (!budgetCycles[t] || !crowd.targetCycles[t]) {
            RF_ERR("species: Track %u has no budget or no crowd target", t + 1);
            ok = false;
        }
    if (ok) {
        const u32 last = crowd.targetTracks ? crowd.targetTracks - 1 : 0;
        RF_LOG("species: %u species from %u families (+ the broodling), roster of %u, budgets and crowd for %u Tracks, crowd %.0f..%.0f",
               kFamilySpecies, kFamilyCount, rosterCount, budgetTracks, crowd.target[0][0],
               crowd.target[last][crowd.targetCycles[last] ? crowd.targetCycles[last] - 1 : 0]);
    }
    if (!boss.load()) ok = false;
    // M5 part 1: Longwave, The Choir and the mini-bosses.
    if (!longwave.load()) ok = false;
    if (!choir.load()) ok = false;
    if (!horizon.load()) ok = false;  // M5 part 4
    if (!poly.load()) ok = false;
    if (!feedback.load()) ok = false;
    if (!minis.load()) ok = false;
    if (!cadence.load()) ok = false;  // M5 part 6
    if (!arp.load()) ok = false;
    if (!morgana.load()) ok = false;
    if (!reprise.load()) ok = false;
    crowd.miniDrop = minis.crowdScale;
    if (!arena.load()) ok = false;   // M5 part 5
    if (!traits.load()) ok = false;
    if (!modes.load()) ok = false;   // M8
    return ok;
}

}  // namespace rf::sim
