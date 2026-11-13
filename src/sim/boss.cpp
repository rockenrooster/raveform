#include "sim/boss.h"

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::sim {

namespace {
const char kKaleidosJson[] =
#include "kaleidos.json.inc"
    ;
const char kLongwaveJson[] =
#include "longwave.json.inc"
    ;
const char kChoirJson[] =
#include "choir.json.inc"
    ;
const char kMinibossesJson[] =
#include "minibosses.json.inc"
    ;

double stepTicks(const char* s) {
    if (std::strcmp(s, "1/8") == 0) return 48.0;
    if (std::strcmp(s, "1/8 triplet") == 0) return 32.0;
    if (std::strcmp(s, "1/16") == 0) return 24.0;
    if (std::strcmp(s, "1/4") == 0) return 96.0;
    return 0.0;
}

struct KindName {
    const char* id;
    const char* name;
};
constexpr KindName kKinds[kBossKinds] = {
    {"kaleidos", "KALEIDOS"},         {"longwave", "LONGWAVE"},         {"choir", "THE CHOIR"},
    {"alpha_hound", "ALPHA HOUND"},   {"the_hive", "THE HIVE"},         {"battering_ram", "BATTERING RAM"},
    {"carom_king", "CAROM KING"},     {"moth_queen", "MOTH QUEEN"},     {"brood_mother", "BROOD MOTHER"},
    {"blink_star", "BLINK STAR"},     {"sentry_array", "SENTRY ARRAY"},
};

// Lines [x0, y0, x1, y1, layer] from an array of arrays.
u32 readLines(const JsonDoc& doc, const JsonValue* arr, BossLine* out, u32 cap) {
    u32 n = 0;
    for (const JsonValue* s = doc.firstChild(arr); s && n < cap; s = doc.nextSibling(s)) {
        float v[5] = {};
        u32 k = 0;
        for (const JsonValue* x = doc.firstChild(s); x && k < 5; x = doc.nextSibling(x)) v[k++] = static_cast<float>(x->number);
        out[n++] = BossLine{v[0], v[1], v[2], v[3], static_cast<u8>(v[4])};
    }
    return n;
}

// A polyline [[x, y], ...] into out[cap][2].
u32 readPoints(const JsonDoc& doc, const JsonValue* arr, float (*out)[2], u32 cap) {
    u32 n = 0;
    for (const JsonValue* s = doc.firstChild(arr); s && n < cap; s = doc.nextSibling(s)) {
        const JsonValue* x = doc.firstChild(s);
        const JsonValue* y = doc.nextSibling(x);
        out[n][0] = x ? static_cast<float>(x->number) : 0.0f;
        out[n][1] = y ? static_cast<float>(y->number) : 0.0f;
        ++n;
    }
    return n;
}

u64 fnv(u64 h, const char* s) {
    for (const char* c = s; *c; ++c) {
        h ^= static_cast<u8>(*c);
        h *= 1099511628211ull;
    }
    return h;
}
}  // namespace

u64 bossDataDigest() {
    u64 h = 1469598103934665603ull;
    h = fnv(h, kKaleidosJson);
    h = fnv(h, kLongwaveJson);
    h = fnv(h, kChoirJson);
    h = fnv(h, kMinibossesJson);
    return h;
}

const char* bossName(BossKind k) { return static_cast<u32>(k) < kBossKinds ? kKinds[static_cast<u32>(k)].name : "?"; }
const char* bossId(BossKind k) { return static_cast<u32>(k) < kBossKinds ? kKinds[static_cast<u32>(k)].id : "?"; }
bool bossFromId(const char* id, BossKind& out) {
    for (u32 k = 0; k < kBossKinds; ++k)
        if (std::strcmp(id, kKinds[k].id) == 0) {
            out = static_cast<BossKind>(k);
            return true;
        }
    return false;
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

// ---------------------------------------------------------------------------------------------- Longwave

bool LongwaveData::load() {
    JsonDoc doc;
    if (!doc.parse(kLongwaveJson, sizeof kLongwaveJson - 1, "design/bosses/longwave.json")) return false;
    bool ok = true;
    const auto num = [&](const char* path, float& out) {
        bool got = true;
        const double v = doc.number(path, out, &got);
        if (!got) {
            RF_ERR("longwave: '%s' is missing", path);
            ok = false;
        }
        out = static_cast<float>(v);
    };
    const auto count = [&](const char* path, u32& out) {
        float v = static_cast<float>(out);
        num(path, v);
        out = static_cast<u32>(v + 0.5f);
    };
    segLineCount = readLines(doc, doc.find("parts.segment.segments"), segLines, kMaxLines);
    num("parts.segment.fin.closedReach", finClosed);
    num("parts.segment.fin.crestReach", finCrest);
    jawCount = readLines(doc, doc.find("parts.head.jawClosed"), jawClosed, kMaxLines);
    if (readLines(doc, doc.find("parts.head.jawOpen"), jawOpen, kMaxLines) != jawCount) {
        RF_ERR("longwave: the open and closed jaws differ in length");
        ok = false;
    }
    crownCount = readLines(doc, doc.find("parts.head.crown"), crown, 4);
    readPoints(doc, doc.find("parts.head.eyes"), eyes, 2);
    num("parts.head.seed.x", seedX);
    num("parts.head.seed.radius", seedRadius);
    {
        u32 n = 0;
        for (const JsonValue* s = doc.firstChild(doc.find("parts.tail.blades")); s && n < 3; s = doc.nextSibling(s), ++n) {
            u32 k = 0;
            for (const JsonValue* x = doc.firstChild(s); x && k < 4; x = doc.nextSibling(x)) tailBlades[n][k++] = static_cast<float>(x->number);
        }
    }
    num("parts.spine.widthRefPx", spineWidthPx);
    num("parts.spine.coreWidthRefPx", spineCoreWidthPx);
    count("numbers.segments", segments);
    if (segments > kBossParts) segments = kBossParts;
    num("numbers.spacing", spacing);
    num("numbers.segmentRadius", segRadius);
    num("numbers.headRadius", headRadius);
    num("numbers.segmentHealth", segHealth);
    num("numbers.headHealth", headHealth);
    num("numbers.damageScale.crest", dmgCrest);
    num("numbers.damageScale.elsewhere", dmgElsewhere);
    num("numbers.damageScale.headJawOpen", dmgJaw);
    num("numbers.damageScale.headJawOpenBare", dmgJawBare);
    num("numbers.damageScale.headJawClosed", dmgJawClosed);
    num("numbers.wave.amplitude", amplitude);
    count("numbers.wave.wavelengthSegments", wavelength);
    count("numbers.wave.taperSegments", taper);
    if (wavelength < 2) wavelength = 8;
    num("numbers.path.halfWidth", pathHalfW);
    num("numbers.path.halfHeight", pathHalfH);
    num("numbers.camera.zoom", zoom);
    num("numbers.bulletSpeed", bulletSpeed);
    num("numbers.bulletLifeSeconds", bulletLife);
    for (u32 i = 0; i < 4; ++i) {
        char base[48];
        std::snprintf(base, sizeof base, "numbers.phases.%u", i);
        const JsonValue* ph = doc.find(base);
        if (!ph) {
            RF_ERR("longwave: phase %u is missing", i);
            ok = false;
            continue;
        }
        Phase& p = phases[i];
        p.segHi = static_cast<u32>(doc.number("segmentsAlive.0", 0.0, nullptr, ph));
        p.segLo = static_cast<u32>(doc.number("segmentsAlive.1", 0.0, nullptr, ph));
        char step[24] = {};
        p.stepTicks = doc.text("crestStep", step, sizeof step, ph) ? stepTicks(step) : 0.0;
        p.lapBars = static_cast<float>(doc.number("lapBars", 0.0, nullptr, ph));
        p.perSide = static_cast<u32>(doc.number("fire.bullets", 0.0, nullptr, ph)) / 2u;
        p.lungeTelegraphBeats = static_cast<float>(doc.number("lunge.telegraphBeats", 0.0, nullptr, ph));
        p.lungeDashWu = static_cast<float>(doc.number("lunge.dashWu", 0.0, nullptr, ph));
        p.lungeDashBeats = static_cast<float>(doc.number("lunge.dashBeats", 0.0, nullptr, ph));
        p.headBurst = static_cast<u32>(doc.number("headBurst.count", 0.0, nullptr, ph));
        if (i < 3 && (p.stepTicks <= 0.0 || p.lapBars <= 0.0f)) {
            RF_ERR("longwave: phase %u has no step or lap", i);
            ok = false;
        }
    }
    count("numbers.whip.count", whipCount);
    for (u32 i = 0; i < 2; ++i) {
        char path[64];
        std::snprintf(path, sizeof path, "numbers.echoes.%u.phaseBehindCycles", i);
        num(path, echoBehind[i]);
        std::snprintf(path, sizeof path, "numbers.echoes.%u.intensity", i);
        num(path, echoIntensity[i]);
    }
    num("numbers.accent.oklchLightness", accentL);
    num("numbers.accent.oklchHue", accentHue);
    num("numbers.accent.chromaFraction", accentChroma);
    num("bpm", bpm);
    // The path's arc length over one lap.
    arc[0] = 0.0f;
    Vec2 prev = pathAt(0.0);
    double sum = 0.0;
    for (u32 i = 1; i <= kArc; ++i) {
        const Vec2 q = pathAt(kTau * static_cast<double>(i) / kArc);
        sum += static_cast<double>(length(q - prev));
        arc[i] = static_cast<float>(sum);
        prev = q;
    }
    lapLength = static_cast<float>(sum);
    if (!ok) RF_ERR("longwave: the boss data is incomplete");
    return ok;
}

Vec2 LongwaveData::pathAt(double p) const {
    // longwave.json path.shape: x = halfWidth cos(p), y = halfHeight sin(2p) around the anchor.
    return Vec2{pathHalfW * static_cast<float>(std::cos(p)), pathHalfH * static_cast<float>(std::sin(2.0 * p))};
}

Vec2 LongwaveData::tangentAt(double p) const {
    const Vec2 d{-pathHalfW * static_cast<float>(std::sin(p)), 2.0f * pathHalfH * static_cast<float>(std::cos(2.0 * p))};
    const float l = length(d);
    return l > 1e-6f ? d * (1.0f / l) : Vec2{1.0f, 0.0f};
}

double LongwaveData::arcOf(double p) const {
    const double lap = std::floor(p / kTau);
    const double u = (p - lap * kTau) / kTau * kArc;
    u32 i = static_cast<u32>(u);
    if (i >= kArc) i = kArc - 1;
    const double f = u - i;
    return lap * lapLength + arc[i] + (arc[i + 1] - arc[i]) * f;
}

double LongwaveData::paramOf(double s) const {
    if (lapLength <= 0.0f) return 0.0;
    const double lap = std::floor(s / lapLength);
    const double r = s - lap * lapLength;
    u32 lo = 0, hi = kArc;
    while (hi - lo > 1) {
        const u32 mid = (lo + hi) / 2;
        if (arc[mid] <= r) lo = mid;
        else hi = mid;
    }
    const double span = arc[hi] - arc[lo];
    const double f = span > 1e-9 ? (r - arc[lo]) / span : 0.0;
    return (lap + (lo + f) / kArc) * kTau;
}

u32 LongwaveData::phaseOf(u32 alive) const {
    for (u32 i = 0; i < 3; ++i)
        if (alive >= phases[i].segLo && alive <= phases[i].segHi) return i;
    return alive == 0 ? 3u : 0u;
}

// ---------------------------------------------------------------------------------------------- The Choir

bool ChoirData::load() {
    JsonDoc doc;
    if (!doc.parse(kChoirJson, sizeof kChoirJson - 1, "design/bosses/choir.json")) return false;
    bool ok = true;
    const auto num = [&](const char* path, float& out) {
        bool got = true;
        const double v = doc.number(path, out, &got);
        if (!got) {
            RF_ERR("choir: '%s' is missing", path);
            ok = false;
        }
        out = static_cast<float>(v);
    };
    const auto count = [&](const char* path, u32& out) {
        float v = static_cast<float>(out);
        num(path, v);
        out = static_cast<u32>(v + 0.5f);
    };
    hoodCount = readPoints(doc, doc.find("parts.voice.hood"), hood, kMaxPts);
    {
        u32 n = 0;
        for (const JsonValue* r = doc.firstChild(doc.find("parts.voice.ribs")); r && n < 3; r = doc.nextSibling(r), ++n) readPoints(doc, r, ribs[n], 2);
        n = 0;
        for (const JsonValue* r = doc.firstChild(doc.find("parts.voice.mouthClosed")); r && n < 2; r = doc.nextSibling(r), ++n)
            readPoints(doc, r, mouthClosed[n], 2);
        n = 0;
        for (const JsonValue* r = doc.firstChild(doc.find("parts.voice.mouthOpen")); r && n < 2; r = doc.nextSibling(r), ++n)
            readPoints(doc, r, mouthOpen[n], 2);
    }
    if (hoodCount < 3) {
        RF_ERR("choir: the hood is missing");
        ok = false;
    }
    num("parts.voice.face.x", faceX);
    num("parts.voice.face.radius", faceRadius);
    count("parts.voice.face.sides", faceSides);
    num("parts.voice.core.x", coreX);
    num("parts.voice.core.radius", coreRadius);
    num("parts.voice.halo.radius", haloRadius);
    num("parts.voice.halo.fromDeg", haloFromDeg);
    num("parts.voice.halo.toDeg", haloToDeg);
    doc.numbers("parts.voice.wavefronts.radii", waveRadii, 3);
    num("parts.voice.wavefronts.halfAngleDeg", waveHalfDeg);
    num("numbers.voiceRadius", voiceRadius);
    num("numbers.voiceHealth", voiceHealth);
    {
        float m[kVoices] = {};
        if (doc.numbers("numbers.midi", m, kVoices) == kVoices)
            for (u32 k = 0; k < kVoices; ++k) midi[k] = static_cast<u8>(m[k]);
        else {
            RF_ERR("choir: 'numbers.midi' needs eight notes");
            ok = false;
        }
    }
    num("numbers.damageScale.singing", dmgSinging);
    num("numbers.damageScale.silent", dmgSilent);
    num("numbers.damageScale.lastVoice", dmgLast);
    {
        // numbers.contact.hoodHurts (the lead's ruling, 94706dc): "only while it sings", "always" or "never".
        char hurts[48] = {};
        if (!doc.text("numbers.contact.hoodHurts", hurts, sizeof hurts)) {
            RF_ERR("choir: 'numbers.contact.hoodHurts' is missing");
            ok = false;
        } else if (std::strstr(hurts, "sings")) {
            hoodHurts = HoodHurts::Singing;
        } else if (std::strstr(hurts, "never")) {
            hoodHurts = HoodHurts::Never;
        } else if (std::strstr(hurts, "always")) {
            hoodHurts = HoodHurts::Always;
        } else {
            RF_ERR("choir: 'numbers.contact.hoodHurts' is '%s' (only while it sings, always, never)", hurts);
            ok = false;
        }
    }
    num("numbers.ring.radius", ringRadius);
    num("numbers.ring.radiusFugue", ringFugue);
    num("numbers.ring.shrinkBars", shrinkBars);
    num("numbers.camera.zoom", zoom);
    num("numbers.bulletSpeed", bulletSpeed);
    num("numbers.bulletLifeSeconds", bulletLife);
    count("numbers.phases.0.call.bullets", unisonCall);
    count("numbers.phases.0.response.bullets", unisonAnswer);
    count("numbers.phases.1.call.bullets", canonCall);
    count("numbers.phases.1.response.bullets", canonAnswer);
    count("numbers.phases.2.call.bullets", fugueStream);
    count("numbers.phases.3.call.bullets", soloRing);
    num("numbers.phases.3.orbit.radius", orbitRadius);
    num("numbers.phases.3.orbit.lapBars", orbitLapBars);
    num("numbers.phases.3.heldNote.telegraphBars", heldTelegraphBars);
    num("numbers.phases.3.heldNote.sweepDegrees", heldSweepDeg);
    num("numbers.phases.3.heldNote.sweepBeats", heldSweepBeats);
    num("numbers.cage.telegraphBars", cageTelegraphBars);
    num("numbers.cage.burnBeats", cageBurnBeats);
    num("numbers.strings.neighbour.widthRefPx", neighbourWidthPx);
    num("numbers.strings.neighbour.idleAmplitude", neighbourShimmer);
    num("numbers.strings.call.widthRefPx", callWidthPx);
    num("numbers.strings.call.amplitude", callAmplitude);
    count("numbers.strings.call.antinodes", callAntinodes);
    num("numbers.strings.call.decayBeats", callDecayBeats);
    num("numbers.hues.pearl.oklchLightness", pearlL);
    num("numbers.hues.pearl.oklchHue", pearlHue);
    num("numbers.hues.pearl.chromaFraction", pearlChroma);
    num("bpm", bpm);
    if (!ok) RF_ERR("choir: the boss data is incomplete");
    return ok;
}

// ---------------------------------------------------------------------------------------------- mini-bosses

bool MiniData::load() {
    JsonDoc doc;
    if (!doc.parse(kMinibossesJson, sizeof kMinibossesJson - 1, "design/bosses/minibosses.json")) return false;
    bool ok = true;
    const auto num = [&](const char* path, float& out) {
        bool got = true;
        const double v = doc.number(path, out, &got);
        if (!got) {
            RF_ERR("minibosses: '%s' is missing", path);
            ok = false;
        }
        out = static_cast<float>(v);
    };
    healthTracks = 0;
    // common.healthByTrack: an object keyed by Track number ("2" .. "6"), and its loop factor.
    for (const JsonValue* v = doc.firstChild(doc.find("common.healthByTrack")); v; v = doc.nextSibling(v)) {
        const char* key = doc.key(v);
        if (v->type != JsonValue::Type::Number || key[0] < '0' || key[0] > '9') continue;
        const u32 t = static_cast<u32>(std::atoi(key));
        if (t < 2 || t >= 8) continue;
        healthByTrack[t] = static_cast<float>(v->number);
        if (t > healthTracks) healthTracks = t;
    }
    if (healthTracks < 2) {
        RF_ERR("minibosses: 'common.healthByTrack' is missing");
        ok = false;
    }
    num("common.healthByTrack.loopFactorPerTrack", loopFactor);
    num("common.crowd.sectionScale", crowdScale);
    float orbs = 1, shards = 60;
    num("common.reward.boostOrbs", orbs);
    num("common.reward.shards", shards);
    rewardOrbs = static_cast<u32>(orbs + 0.5f);
    rewardShards = static_cast<u32>(shards + 0.5f);
    num("common.pen.side", penSide);
    num("common.pen.riseBeats", penRiseBeats);
    count = 0;
    for (const JsonValue* m = doc.firstChild(doc.find("minibosses")); m && count < kMax; m = doc.nextSibling(m)) {
        Entry& e = entries[count];
        char id[16] = {};
        doc.text("id", id, sizeof id, m);
        if (!bossFromId(id, e.kind) || !isMiniBoss(e.kind)) {
            RF_ERR("minibosses: unknown mini-boss '%s'", id);
            ok = false;
            continue;
        }
        std::snprintf(e.id, sizeof e.id, "%s", id);
        char name[24] = {};
        doc.text("name", name, sizeof name, m);
        for (u32 k = 0; k < sizeof e.name - 1 && name[k]; ++k) e.name[k] = static_cast<char>(name[k] >= 'a' && name[k] <= 'z' ? name[k] - 32 : name[k]);
        doc.text("family", e.family, sizeof e.family, m);
        doc.text("glyph", e.glyph, sizeof e.glyph, m);
        e.radius = static_cast<float>(doc.number("radius", 64.0, nullptr, m));
        e.healthFactor = static_cast<float>(doc.number("healthFactor", 1.0, nullptr, m));
        const JsonValue* pen = doc.member(m, "pen");
        e.pen = pen && pen->type == JsonValue::Type::Bool && pen->boolean;
        ++count;
    }
    for (BossKind k : {BossKind::AlphaHound, BossKind::Hive, BossKind::BatteringRam, BossKind::CaromKing})
        if (!find(k)) {
            RF_ERR("minibosses: '%s' is missing", bossId(k));
            ok = false;
        }
    if (!ok) RF_ERR("minibosses: the data is incomplete");
    return ok;
}

const MiniData::Entry* MiniData::find(BossKind k) const {
    for (u32 i = 0; i < count; ++i)
        if (entries[i].kind == k) return &entries[i];
    return nullptr;
}

}  // namespace rf::sim
