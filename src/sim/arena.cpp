#include "sim/arena.h"

#include <cstdio>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::sim {

namespace {
const char kArenaJson[] =
#include "arena.json.inc"
    ;
const char kTraitsJson[] =
#include "traits.json.inc"
    ;

const char* const kStructIds[kStructKinds] = {"vault", "pylon", "rift", "bumper", "current", "lens"};
const char* const kEventIds[kEventKinds] = {"vault_drop", "rift_surge", "elite_hunt", "comet", "eclipse", "meteor_rain", "convoy", "storm_cell"};
const char* const kEventNames[kEventKinds] = {"VAULT DROP", "RIFT SURGE", "ELITE HUNT", "COMET", "ECLIPSE", "METEOR RAIN", "CONVOY", "STORM CELL"};
const char* const kTraitIds[kTraitCount] = {"armored",  "swift",   "giant",    "splitting", "volatile", "regenerating",
                                            "shielded", "twin",    "magnetic", "commander", "cloaked",  "phasing"};
const char* const kGlyphIds[ArenaData::gCount] = {"vault", "pylon", "rift", "bumper", "lens", "comet", "carrier"};

u64 fnv(u64 h, const char* s) {
    for (const char* c = s; *c; ++c) {
        h ^= static_cast<u8>(*c);
        h *= 1099511628211ull;
    }
    return h;
}
}  // namespace

const char* structId(StructKind k) { return static_cast<u32>(k) < kStructKinds ? kStructIds[static_cast<u32>(k)] : "?"; }
const char* eventId(EventKind k) { return static_cast<u32>(k) < kEventKinds ? kEventIds[static_cast<u32>(k)] : "?"; }
const char* eventName(EventKind k) { return static_cast<u32>(k) < kEventKinds ? kEventNames[static_cast<u32>(k)] : "?"; }
const char* traitId(u32 t) { return t < kTraitCount ? kTraitIds[t] : "?"; }
bool structFromId(const char* id, StructKind& out) {
    for (u32 k = 0; k < kStructKinds; ++k)
        if (std::strcmp(id, kStructIds[k]) == 0) {
            out = static_cast<StructKind>(k);
            return true;
        }
    return false;
}
bool eventFromId(const char* id, EventKind& out) {
    for (u32 k = 0; k < kEventKinds; ++k)
        if (std::strcmp(id, kEventIds[k]) == 0) {
            out = static_cast<EventKind>(k);
            return true;
        }
    return false;
}

u64 arenaDataDigest() { return fnv(fnv(1469598103934665603ull, kArenaJson), kTraitsJson); }

bool ArenaData::load() {
    JsonDoc doc;
    if (!doc.parse(kArenaJson, sizeof kArenaJson - 1, "design/arena.json")) return false;
    bool ok = true;
    char p[96];
    for (u32 k = 0; k < kStructKinds; ++k) {
        std::snprintf(p, sizeof p, "structures.%u.radius", k);
        bool got = true;
        radius[k] = static_cast<float>(doc.number(p, 0.0, &got));
        char id[24] = {};
        std::snprintf(p, sizeof p, "structures.%u.id", k);
        doc.text(p, id, sizeof id);
        if (!got || std::strcmp(id, kStructIds[k]) != 0) {
            RF_ERR("arena: structure %u is '%s' (expected '%s')", k, id, kStructIds[k]);
            ok = false;
        }
    }
    for (u32 k = 0; k < kEventKinds; ++k) {
        std::snprintf(p, sizeof p, "events.%u.bars", k);
        eventBars[k] = static_cast<u32>(doc.number(p, 0.0));
        char id[24] = {};
        std::snprintf(p, sizeof p, "events.%u.id", k);
        doc.text(p, id, sizeof id);
        if (std::strcmp(id, kEventIds[k]) != 0) {
            RF_ERR("arena: event %u is '%s' (expected '%s')", k, id, kEventIds[k]);
            ok = false;
        }
    }
    for (u32 t = 1; t <= kTracks; ++t) {
        std::snprintf(p, sizeof p, "byTrack.%u.structures", t);
        const JsonValue* s = doc.find(p);
        for (const JsonValue* v = doc.firstChild(s); v && structCount[t] < 8; v = doc.nextSibling(v)) {
            StructKind k;
            if (structFromId(doc.string(v), k)) structs[t][structCount[t]++] = static_cast<u8>(k);
        }
        std::snprintf(p, sizeof p, "byTrack.%u.events", t);
        const JsonValue* e = doc.find(p);
        for (const JsonValue* v = doc.firstChild(e); v && eventCount[t] < 4; v = doc.nextSibling(v)) {
            EventKind k;
            if (eventFromId(doc.string(v), k)) events[t][eventCount[t]++] = static_cast<u8>(k);
        }
        if (!structCount[t] || !eventCount[t]) {
            RF_ERR("arena: Track %u has no structures or no events", t);
            ok = false;
        }
    }
    const JsonValue* gl = doc.find("glyphs");
    for (const JsonValue* g = doc.firstChild(gl); g; g = doc.nextSibling(g)) {
        char id[24] = {};
        doc.text("id", id, sizeof id, g);
        u32 gi = gCount;
        for (u32 k = 0; k < gCount; ++k)
            if (std::strcmp(id, kGlyphIds[k]) == 0) gi = k;
        if (gi == gCount) continue;
        ArenaGlyph& out = glyph[gi];
        out.radius = static_cast<float>(doc.number("radius", 0.0, nullptr, g));
        const JsonValue* segs = doc.member(g, "segments");
        for (const JsonValue* s = doc.firstChild(segs); s && out.count < ArenaGlyph::kMax; s = doc.nextSibling(s)) {
            float v[5] = {};
            u32 n = 0;
            for (const JsonValue* c = doc.firstChild(s); c && n < 5; c = doc.nextSibling(c)) v[n++] = static_cast<float>(c->number);
            if (n < 5) continue;
            for (u32 j = 0; j < 4; ++j) out.seg[out.count][j] = v[j];
            out.layer[out.count] = static_cast<u8>(v[4]);
            ++out.count;
        }
    }
    for (u32 k = 0; k < gCount; ++k)
        if (!glyph[k].count) {
            RF_ERR("arena: no glyph '%s'", kGlyphIds[k]);
            ok = false;
        }
    return ok;
}

bool TraitData::load() {
    JsonDoc doc;
    if (!doc.parse(kTraitsJson, sizeof kTraitsJson - 1, "design/traits.json")) return false;
    bool ok = true;
    const auto num = [&](const char* path, float& out) {
        bool got = true;
        const double v = doc.number(path, out, &got);
        if (!got) {
            RF_ERR("traits: '%s' is missing", path);
            ok = false;
        }
        out = static_cast<float>(v);
    };
    const auto whole = [&](const char* path, u32& out) {
        float v = static_cast<float>(out);
        num(path, v);
        out = static_cast<u32>(v);
    };
    num("elite.scale", scale);
    num("elite.health", health);
    num("elite.score", score);
    num("elite.threat", threat);
    whole("elite.shards", shards);
    whole("elite.when.alive.tracks2to6", alive[0]);
    whole("elite.when.alive.tracks7to10", alive[1]);
    whole("elite.traitsCarried.tracks2to6", carried[0]);
    whole("elite.traitsCarried.tracks7to10", carried[1]);
    char p[96];
    for (u32 t = 0; t < kTraitCount; ++t) {
        char id[24] = {};
        std::snprintf(p, sizeof p, "traits.%u.id", t);
        doc.text(p, id, sizeof id);
        if (std::strcmp(id, kTraitIds[t]) != 0) {
            RF_ERR("traits: trait %u is '%s' (expected '%s')", t, id, kTraitIds[t]);
            ok = false;
        }
        std::snprintf(p, sizeof p, "traits.%u.fromTrack", t);
        fromTrack[t] = static_cast<u8>(doc.number(p, 99.0));
    }
    const auto tn = [&](u32 t, const char* key, float& out) {
        std::snprintf(p, sizeof p, "traits.%u.numbers.%s", t, key);
        num(p, out);
    };
    const auto tw = [&](u32 t, const char* key, u32& out) {
        std::snprintf(p, sizeof p, "traits.%u.numbers.%s", t, key);
        whole(p, out);
    };
    tn(kArmored, "healthFactor", armoredHealth);
    tn(kArmored, "outerHullShare", armoredHull);
    tn(kSwift, "speedFactor", swiftSpeed);
    tn(kSwift, "turnFactor", swiftTurn);
    tn(kGiant, "scaleFactor", giantScale);
    tn(kGiant, "healthFactor", giantHealth);
    tw(kSplitting, "copies", splitCopies);
    tn(kSplitting, "copyScale", splitScale);
    tn(kSplitting, "copyHealthShare", splitHealth);
    tn(kVolatile, "ringWu", volatileRing);
    tw(kVolatile, "fuseBeats", volatileFuse);
    tn(kVolatile, "enemyDamagePerTrack", volatileDamage);
    tw(kRegenerating, "quietBeats", regenQuiet);
    tn(kRegenerating, "healPerBeat", regenHeal);
    tn(kShielded, "arcDegrees", shieldArc);
    tn(kShielded, "turnRate", shieldTurn);
    tn(kTwin, "healthFactor", twinHealth);
    std::snprintf(p, sizeof p, "traits.%u.numbers.apartWu", static_cast<u32>(kTwin));
    if (doc.numbers(p, twinApart, 2) < 2) {
        RF_ERR("traits: '%s' is missing", p);
        ok = false;
    }
    tn(kMagnetic, "reachWu", magnetReach);
    tw(kMagnetic, "maxShards", magnetMax);
    tn(kMagnetic, "releaseFactor", magnetRelease);
    tn(kCommander, "reachWu", commanderReach);
    tn(kCommander, "speedFactor", commanderSpeed);
    tw(kCommander, "stunBeats", commanderStun);
    tn(kCloaked, "betweenBeats", cloaked);
    tn(kPhasing, "solidShareOfBeat", phasingSolid);
    return ok;
}

}  // namespace rf::sim
