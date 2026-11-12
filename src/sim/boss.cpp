#include "sim/boss.h"

#include <cstdio>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::sim {

namespace {
const char kKaleidosJson[] =
#include "kaleidos.json.inc"
    ;

double stepTicks(const char* s) {
    if (std::strcmp(s, "1/8") == 0) return 48.0;
    if (std::strcmp(s, "1/16") == 0) return 24.0;
    if (std::strcmp(s, "1/4") == 0) return 96.0;
    return 0.0;
}
}  // namespace

u64 bossDataDigest() {
    u64 h = 1469598103934665603ull;
    for (const char* c = kKaleidosJson; *c; ++c) {
        h ^= static_cast<u8>(*c);
        h *= 1099511628211ull;
    }
    return h;
}

bool BossData::load() {
    JsonDoc doc;
    if (!doc.parse(kKaleidosJson, sizeof kKaleidosJson - 1, "design/bosses/kaleidos.json")) return false;
    bool ok = true;
    const auto num = [&](const char* path, float& out) {
        bool got = true;
        const double v = doc.number(path, out, &got);
        if (!got) {
            RF_ERR("kaleidos: '%s' is missing", path);
            ok = false;
        }
        out = static_cast<float>(v);
    };
    const auto shape = [&](const char* name, Facet& f) {
        char path[96];
        std::snprintf(path, sizeof path, "parts.%s", name);
        const JsonValue* part = doc.find(path);
        if (!part) {
            RF_ERR("kaleidos: part '%s' is missing", name);
            ok = false;
            return;
        }
        f.armRadius = static_cast<float>(doc.number("armRadius", 0.0, nullptr, part));
        f.length = static_cast<float>(doc.number("length", 0.0, nullptr, part));
        f.halfWidth = static_cast<float>(doc.number("halfWidth", 0.0, nullptr, part));
        const JsonValue* segs = doc.member(part, "segments");
        f.segs = 0;
        for (const JsonValue* s = doc.firstChild(segs); s && f.segs < 8; s = doc.nextSibling(s)) {
            u32 k = 0;
            for (const JsonValue* v = doc.firstChild(s); v && k < 5; v = doc.nextSibling(v)) f.seg[f.segs][k++] = static_cast<float>(v->number);
            ++f.segs;
        }
    };
    static const char* const kFacets[4] = {"facet_0", "facet_1", "facet_2", "facet_3"};
    for (u32 i = 0; i < 4; ++i) shape(kFacets[i], facets[i]);
    shape("tip", tip);
    num("parts.lens.radius", lensRadius);
    num("parts.lens.halfAngleDeg", lensHalfAngleDeg);
    num("parts.lens.innerRadius", lensInner);
    num("parts.lens.widthRefPx", lensWidthPx);
    for (u32 i = 0; i < 3; ++i) {
        char path[64];
        std::snprintf(path, sizeof path, "parts.core.rings.%u.radius", i);
        num(path, coreRingRadius[i]);
        std::snprintf(path, sizeof path, "parts.core.rings.%u.phaseDeg", i);
        num(path, coreRingPhaseDeg[i]);
        float sides = 6;
        std::snprintf(path, sizeof path, "parts.core.rings.%u.sides", i);
        num(path, sides);
        coreRingSides[i] = static_cast<u32>(sides);
    }
    float shutters = 6;
    num("parts.core.iris.shutters", shutters);
    irisShutters = static_cast<u32>(shutters);
    num("parts.core.iris.closedRadius", irisClosed);
    num("parts.core.iris.openRadius", irisOpen);
    num("parts.core.healthRing.radius", healthRingRadius);
    num("numbers.radius", radius);
    num("numbers.contactRadiusCore", contactCore);
    num("numbers.pairHealth", pairHealth);
    num("numbers.coreHealth", coreHealth);
    num("numbers.damageScale.firing", dmgFiring);
    num("numbers.damageScale.resting", dmgResting);
    num("numbers.damageScale.lens", dmgLens);
    num("numbers.damageScale.coreExposed", dmgCoreExposed);
    num("numbers.damageScale.coreDownbeat", dmgCoreDownbeat);
    num("numbers.drift.speed", driftSpeed);
    num("numbers.drift.keepDistance.0", keepNear);
    num("numbers.drift.keepDistance.1", keepFar);
    num("numbers.bulletSpeed", bulletSpeed);
    num("numbers.bulletLifeSeconds", bulletLife);
    num("bpm", bpm);
    for (u32 i = 0; i < 2; ++i) {
        char path[64];
        std::snprintf(path, sizeof path, "numbers.reflections.%u.scale", i);
        num(path, reflectScale[i]);
        std::snprintf(path, sizeof path, "numbers.reflections.%u.turnDeg", i);
        num(path, reflectTurnDeg[i]);
        std::snprintf(path, sizeof path, "numbers.reflections.%u.intensity", i);
        num(path, reflectIntensity[i]);
    }
    num("numbers.accent.oklchLightness", accentL);
    num("numbers.accent.oklchHue", accentHue);
    num("numbers.accent.chromaFraction", accentChroma);
    for (u32 i = 0; i < 4; ++i) {
        char base[48], path[96];
        std::snprintf(base, sizeof base, "numbers.phases.%u", i);
        const JsonValue* ph = doc.find(base);
        if (!ph) {
            RF_ERR("kaleidos: phase %u is missing", i);
            ok = false;
            continue;
        }
        Phase& p = phases[i];
        p.pairsAlive = static_cast<u32>(doc.number("pairsAlive", 3.0 - i, nullptr, ph));
        p.turnBars = static_cast<float>(doc.number("turnBars", 8.0, nullptr, ph));
        p.direction = static_cast<float>(doc.number("direction", 1.0, nullptr, ph));
        char step[16] = {};
        if (doc.text("fire.step", step, sizeof step, ph)) p.fireStepTicks = stepTicks(step);
        p.fan = static_cast<u32>(doc.number("fire.fan", 0.0, nullptr, ph));
        p.spreadDeg = static_cast<float>(doc.number("fire.spreadDeg", 0.0, nullptr, ph));
        p.coreBurst = static_cast<u32>(doc.number("coreBurst.count", 0.0, nullptr, ph));
        p.beamTelegraphBeats = static_cast<float>(doc.number("beam.telegraphBeats", 0.0, nullptr, ph));
        p.beamFireBeats = static_cast<float>(doc.number("beam.fireBeats", 0.0, nullptr, ph));
        p.beamWidth = static_cast<float>(doc.number("beam.widthWu", 0.0, nullptr, ph));
        (void)path;
    }
    if (!ok) RF_ERR("kaleidos: the boss data is incomplete");
    return ok;
}

}  // namespace rf::sim
