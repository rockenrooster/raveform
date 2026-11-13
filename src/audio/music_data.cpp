#include "audio/music_data.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <initializer_list>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::audio {

namespace {

const char kPatchesJson[] =
#include "music_patches.json.inc"
    ;
const char kVariantsJson[] =
#include "music_variants.json.inc"
    ;
const char kConductorJson[] =
#include "music_conductor.json.inc"
    ;
const char kTrack01Json[] =
#include "music_track01.json.inc"
    ;
const char kTrack02Json[] =
#include "music_track02.json.inc"
    ;
const char kTrack03Json[] =
#include "music_track03.json.inc"
    ;
// M5: the rest of the set list.
const char kTrack04Json[] =
#include "music_track04.json.inc"
    ;
const char kTrack05Json[] =
#include "music_track05.json.inc"
    ;
const char kTrack06Json[] =
#include "music_track06.json.inc"
    ;
const char kTrack07Json[] =
#include "music_track07.json.inc"
    ;
const char kTrack08Json[] =
#include "music_track08.json.inc"
    ;
const char kTrack09Json[] =
#include "music_track09.json.inc"
    ;
const char kTrack10Json[] =
#include "music_track10.json.inc"
    ;
const char kCalibrationJson[] =
#include "audio_loudness_calibration.json.inc"
    ;

struct TrackSource {
    const char* text;
    const char* file;
};
const TrackSource kTrackSources[] = {
    {kTrack01Json, "track01.json"}, {kTrack02Json, "track02.json"}, {kTrack03Json, "track03.json"}, {kTrack04Json, "track04.json"},
    {kTrack05Json, "track05.json"}, {kTrack06Json, "track06.json"}, {kTrack07Json, "track07.json"}, {kTrack08Json, "track08.json"},
    {kTrack09Json, "track09.json"}, {kTrack10Json, "track10.json"}};
static_assert(sizeof kTrackSources / sizeof kTrackSources[0] <= kMaxTracks, "more Tracks embedded than MusicData holds");

u64 fnv1a(u64 h, const char* s, size_t n) {
    for (size_t i = 0; i < n; ++i) {
        h ^= static_cast<u8>(s[i]);
        h *= 0x100000001B3ull;
    }
    return h;
}

bool eq(const char* a, const char* b) { return std::strcmp(a, b) == 0; }

// ---------------------------------------------------------------- flat parameter table

struct ParamEntry {
    char path[48];
    float value;
    bool bound;
};

struct ParamTable {
    ParamEntry e[192];
    u32 n = 0;
    bool overflow = false;

    void set(const char* path, float v) {
        for (u32 i = 0; i < n; ++i) {
            if (eq(e[i].path, path)) {
                e[i].value = v;
                return;
            }
        }
        if (n >= countOf(e)) {
            overflow = true;
            return;
        }
        std::snprintf(e[n].path, sizeof e[n].path, "%s", path);
        e[n].value = v;
        e[n].bound = false;
        ++n;
    }
    ParamEntry* get(const char* path) {
        for (u32 i = 0; i < n; ++i)
            if (eq(e[i].path, path)) return &e[i];
        return nullptr;
    }
};

// Every number below `obj` as "prefix.key[.key...]" (arrays: ".0", ".1", ...).
void walk(const JsonDoc& doc, const JsonValue* obj, const char* prefix, ParamTable& t) {
    if (!obj) return;
    if (obj->type == JsonValue::Type::Number) {
        t.set(prefix, static_cast<float>(obj->number));
        return;
    }
    if (obj->type != JsonValue::Type::Object && obj->type != JsonValue::Type::Array) return;
    u32 index = 0;
    for (const JsonValue* c = doc.firstChild(obj); c; c = doc.nextSibling(c), ++index) {
        char path[48];
        if (obj->type == JsonValue::Type::Object) {
            std::snprintf(path, sizeof path, "%s%s%s", prefix, prefix[0] ? "." : "", doc.key(c));
        } else {
            std::snprintf(path, sizeof path, "%s.%u", prefix, index);
        }
        walk(doc, c, path, t);
    }
}

// The prototype's override blocks (musicproto.render): mix, send, duck and patch.
void walkOverrides(const JsonDoc& doc, const JsonValue* src, ParamTable& t) {
    static const char* const kBlocks[] = {"mix", "send", "duck", "patch"};
    for (const char* b : kBlocks) walk(doc, doc.member(src, b), b, t);
}

struct Binding {
    const char* path;
    float* (*get)(SoundParams&);
};
#define RF_BIND(path, field) {path, [](SoundParams& p) -> float* { return &p.field; }}
const Binding kBindings[] = {
    RF_BIND("target_lufs", targetLufs),
    RF_BIND("mix.kick", mix.kick), RF_BIND("mix.clap", mix.clap), RF_BIND("mix.snare", mix.snare),
    RF_BIND("mix.hat", mix.hat), RF_BIND("mix.ohat", mix.ohat), RF_BIND("mix.shaker", mix.shaker),
    RF_BIND("mix.bass", mix.bass), RF_BIND("mix.lead", mix.lead), RF_BIND("mix.pad", mix.pad),
    RF_BIND("mix.stab", mix.stab), RF_BIND("mix.pulse", mix.pulse), RF_BIND("mix.riser", mix.riser),
    RF_BIND("mix.impact", mix.impact), RF_BIND("mix.swell", mix.swell),
    RF_BIND("patch.kick_hz", kickHz),
    RF_BIND("patch.kick.sweep_hz", kick.sweepHz), RF_BIND("patch.kick.sweep_tau", kick.sweepTau),
    RF_BIND("patch.kick.settle_hz", kick.settleHz), RF_BIND("patch.kick.settle_tau", kick.settleTau),
    RF_BIND("patch.kick.decay", kick.decay), RF_BIND("patch.kick.attack", kick.attack),
    RF_BIND("patch.kick.click", kick.click), RF_BIND("patch.kick.click_decay", kick.clickDecay),
    RF_BIND("patch.kick.drive", kick.drive),
    RF_BIND("patch.hat.closed_decay", hat.closedDecay), RF_BIND("patch.hat.open_decay", hat.openDecay),
    RF_BIND("patch.lead.detune_cents", lead.detuneCents), RF_BIND("patch.lead.sub", lead.sub),
    RF_BIND("patch.lead.fc_start", lead.fcStart), RF_BIND("patch.lead.fc_end", lead.fcEnd),
    RF_BIND("patch.lead.fc_tau", lead.fcTau), RF_BIND("patch.lead.q", lead.q),
    RF_BIND("patch.lead.attack", lead.attack), RF_BIND("patch.lead.decay", lead.decay),
    RF_BIND("patch.lead.length", lead.length), RF_BIND("patch.lead.keytrack", lead.keytrack),
    RF_BIND("patch.bass.square_mix", bass.squareMix), RF_BIND("patch.bass.sub", bass.sub),
    RF_BIND("patch.bass.fc_start", bass.fcStart), RF_BIND("patch.bass.fc_end", bass.fcEnd),
    RF_BIND("patch.bass.fc_tau", bass.fcTau), RF_BIND("patch.bass.q", bass.q),
    RF_BIND("patch.bass.attack", bass.attack), RF_BIND("patch.bass.decay", bass.decay),
    RF_BIND("patch.bass.release", bass.release), RF_BIND("patch.bass.drive", bass.drive),
    RF_BIND("patch.stab.detune_cents", stab.detuneCents), RF_BIND("patch.stab.fc_start", stab.fcStart),
    RF_BIND("patch.stab.fc_end", stab.fcEnd), RF_BIND("patch.stab.fc_tau", stab.fcTau),
    RF_BIND("patch.stab.q", stab.q), RF_BIND("patch.stab.attack", stab.attack),
    RF_BIND("patch.stab.decay", stab.decay), RF_BIND("patch.stab.length", stab.length),
    RF_BIND("patch.pad.detune_cents", pad.detuneCents), RF_BIND("patch.pad.fc_base", pad.fcBase),
    RF_BIND("patch.pad.fc_lfo", pad.fcLfo), RF_BIND("patch.pad.lfo_hz", pad.lfoHz), RF_BIND("patch.pad.q", pad.q),
    RF_BIND("patch.pad.attack", pad.attack), RF_BIND("patch.pad.release", pad.release),
    RF_BIND("patch.pad.highpass", pad.highpass),
    RF_BIND("patch.delay.steps", delay.steps), RF_BIND("patch.delay.feedback", delay.feedback),
    RF_BIND("patch.delay.mix", delay.mix), RF_BIND("patch.delay.repeats", delay.repeats),
    RF_BIND("patch.reverb.rt60", reverb.rt60), RF_BIND("patch.reverb.predelay", reverb.predelay),
    RF_BIND("patch.reverb.highpass", reverb.highpass), RF_BIND("patch.reverb.lowpass", reverb.lowpass),
    RF_BIND("patch.reverb.mix", reverb.mix),
    RF_BIND("patch.sidechain_tau", sidechainTau), RF_BIND("patch.master_drive", masterDrive),
};
#undef RF_BIND

i32 busByName(const char* name) {
    for (u32 b = 0; b < kBusCount; ++b)
        if (eq(busName(static_cast<Bus>(b)), name)) return static_cast<i32>(b);
    return -1;
}

i32 sectionByName(const char* name) {
    for (u32 s = 0; s < static_cast<u32>(Section::Count); ++s)
        if (eq(sectionName(static_cast<Section>(s)), name)) return static_cast<i32>(s);
    return -1;
}

i32 patByName(const char* name) {
    for (u32 p = 0; p < kPatCount; ++p)
        if (eq(patName(static_cast<Pat>(p)), name)) return static_cast<i32>(p);
    return -1;
}

bool fill(ParamTable& t, SoundParams& out, const char* what) {
    bool ok = true;
    for (const Binding& b : kBindings) {
        ParamEntry* e = t.get(b.path);
        if (!e) {
            RF_ERR("music: %s has no number at '%s' (design/music/patches.json)", what, b.path);
            ok = false;
            continue;
        }
        *b.get(out) = e->value;
        e->bound = true;
    }
    for (u32 s = 0; s < static_cast<u32>(Section::Count); ++s) {
        char path[48];
        std::snprintf(path, sizeof path, "section_gain.%s", sectionName(static_cast<Section>(s)));
        if (ParamEntry* e = t.get(path)) {
            out.sectionGain[s][0] = out.sectionGain[s][1] = e->value;
            e->bound = true;
            continue;
        }
        char p0[56], p1[56];
        std::snprintf(p0, sizeof p0, "%s.0", path);
        std::snprintf(p1, sizeof p1, "%s.1", path);
        ParamEntry* a = t.get(p0);
        ParamEntry* b = t.get(p1);
        if (!a || !b) {
            RF_ERR("music: %s has no section gain for '%s'", what, sectionName(static_cast<Section>(s)));
            ok = false;
            continue;
        }
        out.sectionGain[s][0] = a->value;
        out.sectionGain[s][1] = b->value;
        a->bound = b->bound = true;
    }
    for (u32 i = 0; i < t.n; ++i) {
        ParamEntry& e = t.e[i];
        const bool send = std::strncmp(e.path, "send.", 5) == 0;
        const bool duck = std::strncmp(e.path, "duck.", 5) == 0;
        if (send || duck) {
            const i32 bus = busByName(e.path + 5);
            if (bus < 0) {
                RF_WARN("music: %s: '%s' names no bus; ignored", what, e.path);
                continue;
            }
            (send ? out.send : out.duck)[bus] = e.value;
            e.bound = true;
        } else if (!e.bound) {
            RF_WARN("music: %s: '%s' is not used by the engine", what, e.path);
        }
    }
    if (t.overflow) {
        RF_ERR("music: %s: parameter table full", what);
        ok = false;
    }
    return ok;
}

// ---------------------------------------------------------------- prose numbers (conductor.json)

u32 proseNumbers(const char* s, float* out, u32 max) {
    u32 n = 0;
    while (*s) {
        if (*s >= '0' && *s <= '9') {
            char* end = nullptr;
            const double v = std::strtod(s, &end);
            if (n < max) out[n] = static_cast<float>(v);
            ++n;
            s = end;
        } else {
            ++s;
        }
    }
    return n;
}

struct ProseReader {
    const JsonDoc& doc;
    bool ok = true;
    // Reads exactly `count` numbers out of the sentence at `path`.
    bool read(const char* path, float* out, u32 count, const JsonValue* from = nullptr) {
        const JsonValue* v = doc.find(path, from);
        if (!v || v->type != JsonValue::Type::String) {
            RF_ERR("music: conductor.json has no sentence at '%s'", path);
            ok = false;
            return false;
        }
        float tmp[16];
        const u32 n = proseNumbers(doc.string(v), tmp, 16);
        if (n != count) {
            RF_ERR("music: conductor.json '%s' holds %u numbers, the engine reads %u: \"%s\"", path, n, count,
                   doc.string(v));
            ok = false;
            return false;
        }
        for (u32 i = 0; i < count; ++i) out[i] = tmp[i];
        return true;
    }
};

}  // namespace

const char* sectionName(Section s) {
    switch (s) {
        case Section::Groove: return "groove";
        case Section::Build: return "build";
        case Section::Drop: return "drop";
        case Section::Break: return "break";
        default: return "?";
    }
}

const char* busName(Bus b) {
    static const char* const kNames[] = {"drums", "hats_break", "clap", "snare", "pulse", "lead",
                                         "bass",  "stab",       "pad",  "fx",    "sfx"};
    const u32 i = static_cast<u32>(b);
    return i < countOf(kNames) ? kNames[i] : "?";
}

const char* patName(Pat p) {
    static const char* const kNames[] = {"kick", "clap", "hat_off", "hat_16", "ohat", "shaker", "stab", "pulse"};
    const u32 i = static_cast<u32>(p);
    return i < countOf(kNames) ? kNames[i] : "?";
}

namespace {

bool loadTrack(const char* text, const char* file, Track& t, ParamTable& overrides) {
    JsonDoc doc;
    if (!doc.parse(text, std::strlen(text), file)) return false;
    bool ok = true;
    const auto fail = [&](const char* what) {
        RF_ERR("music: %s: %s", file, what);
        ok = false;
    };
    doc.text("id", t.id, sizeof t.id);
    doc.text("name", t.name, sizeof t.name);
    bool found = true;  // JsonDoc::number only clears it
    t.bpm = static_cast<float>(doc.number("bpm", 0.0, &found));
    if (!found || t.bpm <= 0.0f) fail("no bpm");
    float scale[12];
    t.scaleCount = doc.numbers("scale", scale, 12);
    for (u32 i = 0; i < t.scaleCount; ++i) t.scale[i] = static_cast<i8>(scale[i]);

    // Chords.
    const JsonValue* chords = doc.find("chords");
    for (const JsonValue* c = doc.firstChild(chords); c && t.chordCount < countOf(t.chords); c = doc.nextSibling(c)) {
        Chord& ch = t.chords[t.chordCount++];
        std::snprintf(ch.name, sizeof ch.name, "%s", doc.key(c));
        found = true;
        ch.bass = static_cast<u8>(doc.number("bass", 0.0, &found, c));
        if (!found) fail("chord without a bass note");
        float v[16];
        ch.padCount = static_cast<u8>(doc.numbers("pad", v, 6, c));
        for (u32 i = 0; i < ch.padCount; ++i) ch.pad[i] = static_cast<u8>(v[i]);
        ch.stabCount = static_cast<u8>(doc.numbers("stab", v, 4, c));
        for (u32 i = 0; i < ch.stabCount; ++i) ch.stab[i] = static_cast<u8>(v[i]);
        if (doc.numbers("arp", v, 16, c) != 16) fail("an arp needs 16 notes");
        for (u32 i = 0; i < 16; ++i) ch.arp[i] = static_cast<u8>(v[i]);
    }
    const JsonValue* prog = doc.find("progression");
    for (const JsonValue* p = doc.firstChild(prog); p && t.progressionCount < countOf(t.progression);
         p = doc.nextSibling(p)) {
        const char* name = doc.string(p);
        bool hit = false;
        for (u32 i = 0; i < t.chordCount; ++i) {
            if (eq(t.chords[i].name, name)) {
                t.progression[t.progressionCount++] = static_cast<u8>(i);
                hit = true;
            }
        }
        if (!hit) fail("progression names an unknown chord");
    }
    if (!t.progressionCount) fail("empty progression");

    // Patterns: '.' rest, digits are velocity ninths.
    const JsonValue* pats = doc.find("patterns");
    for (const JsonValue* p = doc.firstChild(pats); p; p = doc.nextSibling(p)) {
        const i32 idx = patByName(doc.key(p));
        if (idx < 0) {
            RF_WARN("music: %s: pattern '%s' is not played by the engine", file, doc.key(p));
            continue;
        }
        const char* s = doc.string(p);
        if (std::strlen(s) != 16) fail("a pattern needs 16 steps");
        for (u32 i = 0; i < 16 && s[i]; ++i) t.pattern[idx][i] = (s[i] >= '1' && s[i] <= '9') ? (s[i] - '0') / 9.0f : 0.0f;
        t.hasPattern[idx] = true;
    }

    // Bass patterns: [step, semitones, velocity, length in steps].
    static const char* const kBassNames[] = {"", "groove", "groove_alt", "drop"};
    for (u32 k = 1; k < static_cast<u32>(BassKey::Count); ++k) {
        char path[32];
        std::snprintf(path, sizeof path, "bass.%s", kBassNames[k]);
        const JsonValue* list = doc.find(path);
        if (!list) {
            fail("missing bass pattern");
            continue;
        }
        BassPattern& bp = t.bass[k];
        for (const JsonValue* n = doc.firstChild(list); n && bp.count < countOf(bp.note); n = doc.nextSibling(n)) {
            float v[4];
            if (doc.numbers("", v, 4, n) != 4) {  // "" is the note array itself
                fail("a bass note needs [step, semitones, velocity, length]");
                continue;
            }
            bp.note[bp.count++] = {v[0], v[1], v[2], v[3]};
            if (v[3] > t.longestBassSteps) t.longestBassSteps = v[3];
        }
    }

    // Arrangement (the staged run).
    const JsonValue* arr = doc.find("arrangement");
    for (const JsonValue* s = doc.firstChild(arr); s && t.partCount < countOf(t.parts); s = doc.nextSibling(s)) {
        ArrangementPart& part = t.parts[t.partCount++];
        char sec[16] = {};
        doc.text("section", sec, sizeof sec, s);
        const i32 si = sectionByName(sec);
        if (si < 0) fail("unknown section in the arrangement");
        part.section = static_cast<Section>(si < 0 ? 0 : si);
        found = true;
        part.bars = static_cast<u32>(doc.number("bars", 0.0, &found, s));
        if (!found || !part.bars) fail("an arrangement part needs bars");
        t.arrangementBars += part.bars;
        const JsonValue* layers = doc.member(s, "layers");
        for (const JsonValue* l = doc.firstChild(layers); l; l = doc.nextSibling(l)) {
            const char* name = doc.key(l);
            if (eq(name, "bass")) {
                const char* v = doc.string(l);
                part.bass = eq(v, "drop") ? BassKey::Drop : (eq(v, "groove") ? BassKey::Groove : BassKey::None);
                if (part.bass == BassKey::None) fail("unknown bass variant");
            } else if (eq(name, "lead")) {
                if (l->type == JsonValue::Type::String) {
                    const char* v = doc.string(l);
                    part.lead = eq(v, "double") ? LeadMode::Double : (eq(v, "sparse") ? LeadMode::Sparse : LeadMode::Off);
                    if (part.lead == LeadMode::Off) fail("unknown lead variant");
                } else {
                    part.lead = l->number != 0.0 ? LeadMode::Normal : LeadMode::Off;
                }
            } else if (eq(name, "pad")) {
                part.pad = static_cast<float>(l->number);
            } else {
                const i32 p = patByName(name);
                if (p < 0) {
                    RF_WARN("music: %s: layer '%s' is not played by the engine", file, name);
                    continue;
                }
                part.pat[p] = static_cast<float>(l->number);
            }
        }
    }
    walkOverrides(doc, doc.root(), overrides);
    return ok;
}

bool loadConductor(ConductorRules& c) {
    JsonDoc doc;
    if (!doc.parse(kConductorJson, std::strlen(kConductorJson), "conductor.json")) return false;
    ProseReader prose{doc};
    bool ok = true;
    const JsonValue* layers = doc.find("bed.layers");
    for (const JsonValue* l = doc.firstChild(layers); l && c.bedCount < countOf(c.bed); l = doc.nextSibling(l)) {
        char name[16] = {};
        doc.text("layer", name, sizeof name, l);
        ConductorRules::BedLayer& b = c.bed[c.bedCount];
        bool found = true;  // JsonDoc::number only clears it
        b.from = static_cast<float>(doc.number("from", 0.0, &found, l));
        if (!found) {
            RF_ERR("music: conductor.json bed layer '%s' has no 'from'", name);
            ok = false;
        }
        if (eq(name, "bass")) {
            b.isBass = true;
        } else if (eq(name, "pad")) {
            b.isPad = true;
            float v[4];
            if (prose.read("level", v, 4, l)) {
                c.padLevelLo = v[0];
                c.padAtLo = v[1];
                c.padLevelHi = v[2];
                c.padAtHi = v[3];
            }
        } else {
            const i32 p = patByName(name);
            if (p < 0) {
                RF_ERR("music: conductor.json bed layer '%s' is unknown", name);
                ok = false;
                continue;
            }
            b.pat = static_cast<Pat>(p);
        }
        ++c.bedCount;
    }
    bool found = true;
    c.hysteresis = static_cast<float>(doc.number("bed.hysteresis", 0.05, &found));
    if (!found) {
        RF_ERR("music: conductor.json has no bed.hysteresis");
        ok = false;
    }
    float v[8];
    if (prose.read("bed.fill", v, 2)) {
        c.fillPhraseBars = static_cast<u32>(v[0]);
        c.fillPickupStep = static_cast<u32>(v[1]);
    }
    if (prose.read("sections.build.snareRoll", v, 2)) {
        c.snareRollLo = v[0];
        c.snareRollHi = v[1];
    }
    if (prose.read("sections.build.pad", v, 2)) {
        c.buildPadBrightLo = v[0];
        c.buildPadBrightHi = v[1];
    }
    if (prose.read("sections.build.time", v, 1)) c.buildLastBarSpeed = v[0] / 100.0f;
    if (prose.read("sections.drop.pad", v, 1)) c.dropPadBright = v[0];
    if (prose.read("sections.break.layers.0", v, 1)) c.breakHatLowpassHz = v[0] * 1000.0f;  // "5 kHz"
    if (prose.read("sections.break.layers.1", v, 2)) {
        c.breakPadLevel = v[0];
        c.breakPadBright = v[1];
    }
    if (prose.read("sections.break.lead", v, 3)) {
        for (u32 i = 0; i < 3; ++i) c.breakSparseSteps[i] = static_cast<u8>(v[i]);
    }
    if (doc.numbers("sections.drop.surgeSteps", v, 2) == 2) {
        c.surgeSteps[0] = static_cast<u32>(v[0]);
        c.surgeSteps[1] = static_cast<u32>(v[1]);
    }
    if (doc.numbers("lead.grid.ticks", v, 3) == 3) {
        for (u32 i = 0; i < 3; ++i) c.gridTicks[i] = static_cast<u32>(v[i]);
    } else {
        RF_ERR("music: conductor.json lead.grid.ticks needs three numbers");
        ok = false;
    }
    if (prose.read("lead.accents", v, 3)) {
        c.accentBeat = v[0];
        c.accentEighth = v[1];
        c.accentOther = v[2];
    }
    if (prose.read("lead.pan", v, 2)) {
        c.panDepth = v[0];
        c.panRate = v[1];
    }
    if (prose.read("lead.others", v, 3)) {
        c.othersLevel = v[0];
        c.othersFarLevel = v[1];
        c.othersFarWu = v[2];
    }
    c.splitDegreeCount = doc.numbers("mods.split.degrees", v, 4);
    for (u32 i = 0; i < c.splitDegreeCount; ++i) c.splitDegrees[i] = static_cast<u8>(v[i]);
    if (prose.read("mods.split.perStack", v, 1)) c.splitLevel = v[0];
    if (prose.read("mods.pierce.perStack", v, 1)) c.pierceScale = v[0];
    if (prose.read("mods.overcharge.perStack", v, 2)) {
        c.overchargeDrive = v[0];
        c.overchargeDb = v[1];
    }
    if (prose.read("mods.on_beat_crit.perStack", v, 1)) c.critLevel = v[0];
    if (prose.read("boosts.prism", v, 1)) c.prismLevel = v[0];
    if (prose.read("riffs.pulse_ring.says", v, 1)) c.pulseRingEveryBeatFrom = static_cast<u32>(v[0]);
    return ok && prose.ok;
}

u64 parseHex(const char* s) {
    u64 v = 0;
    for (; *s; ++s) {
        const char c = *s;
        const int d = (c >= '0' && c <= '9') ? c - '0' : (c >= 'a' && c <= 'f') ? c - 'a' + 10 : (c >= 'A' && c <= 'F') ? c - 'A' + 10 : -1;
        if (d < 0) break;
        v = (v << 4) | static_cast<u64>(d);
    }
    return v;
}

}  // namespace

bool MusicData::load() {
    // Identity of the data the loudness calibration depends on.
    hash = 0xCBF29CE484222325ull;
    hash = fnv1a(hash, kPatchesJson, std::strlen(kPatchesJson));
    hash = fnv1a(hash, kVariantsJson, std::strlen(kVariantsJson));
    for (const TrackSource& s : kTrackSources) hash = fnv1a(hash, s.text, std::strlen(s.text));
    const u32 version = kDspVersion;
    hash = fnv1a(hash, reinterpret_cast<const char*>(&version), sizeof version);

    // Base sound.
    auto* base = static_cast<ParamTable*>(std::calloc(1, sizeof(ParamTable)));
    auto* work = static_cast<ParamTable*>(std::calloc(1, sizeof(ParamTable)));
    auto* trackOverrides = static_cast<ParamTable*>(std::calloc(kMaxTracks, sizeof(ParamTable)));
    auto* variantOverrides = static_cast<ParamTable*>(std::calloc(kMaxVariants, sizeof(ParamTable)));
    bool ok = base && work && trackOverrides && variantOverrides;
    if (ok) {
        JsonDoc doc;
        ok = doc.parse(kPatchesJson, std::strlen(kPatchesJson), "patches.json");
        if (ok) {
            walk(doc, doc.find("target_lufs"), "target_lufs", *base);
            walk(doc, doc.find("mix"), "mix", *base);
            walk(doc, doc.find("section_gain"), "section_gain", *base);
            walk(doc, doc.find("send"), "send", *base);
            walk(doc, doc.find("duck"), "duck", *base);
            walk(doc, doc.find("patch"), "patch", *base);
        }
    }
    if (ok) {
        JsonDoc doc;
        ok = doc.parse(kVariantsJson, std::strlen(kVariantsJson), "variants.json");
        char ref[16] = {};
        doc.text("reference", ref, sizeof ref);
        const JsonValue* list = doc.find("variants");
        for (const JsonValue* v = doc.firstChild(list); ok && v && variantCount < kMaxVariants; v = doc.nextSibling(v)) {
            Variant& var = variants[variantCount];
            doc.text("id", var.id, sizeof var.id, v);
            doc.text("name", var.name, sizeof var.name, v);
            walkOverrides(doc, v, variantOverrides[variantCount]);
            if (eq(var.id, ref)) defaultVariant = variantCount;
            ++variantCount;
        }
        if (!variantCount) {
            RF_ERR("music: variants.json lists no variant");
            ok = false;
        }
    }
    for (u32 i = 0; ok && i < countOf(kTrackSources); ++i) {
        ok = loadTrack(kTrackSources[i].text, kTrackSources[i].file, tracks[trackCount], trackOverrides[trackCount]);
        if (ok) ++trackCount;
    }
    // Resolve every Track x variant: patches.json, then the variant, then the Track.
    for (u32 t = 0; ok && t < trackCount; ++t) {
        for (u32 v = 0; ok && v < variantCount; ++v) {
            std::memcpy(work, base, sizeof(ParamTable));
            for (const ParamTable* src : {&variantOverrides[v], &trackOverrides[t]})
                for (u32 i = 0; i < src->n; ++i) work->set(src->e[i].path, src->e[i].value);
            char what[48];
            std::snprintf(what, sizeof what, "%s / %s", tracks[t].id, variants[v].id);
            ok = fill(*work, sound[t][v], what);
        }
    }
    std::free(base);
    std::free(work);
    std::free(trackOverrides);
    std::free(variantOverrides);
    if (!ok) return false;
    if (!loadConductor(conductor)) return false;

    // Loudness calibration.
    {
        JsonDoc doc;
        if (doc.parse(kCalibrationJson, std::strlen(kCalibrationJson), "loudness_calibration.json")) {
            char hex[32] = {};
            doc.text("hash", hex, sizeof hex);
            calibration.hash = parseHex(hex);
            for (u32 t = 0; t < trackCount; ++t) {
                for (u32 v = 0; v < variantCount; ++v) {
                    char path[64];
                    std::snprintf(path, sizeof path, "gain_db.%s.%s", tracks[t].id, variants[v].id);
                    bool found = true;
                    const double g = doc.number(path, 0.0, &found);
                    calibration.gainDb[t][v] = static_cast<float>(g);
                    calibration.have[t][v] = found;
                }
            }
        }
        calibrationCurrent = calibration.hash == hash;
        if (!calibrationCurrent) {
            RF_WARN("music: the loudness calibration (src/audio/loudness_calibration.json) was made from other music data "
                    "(hash %016llx, data %016llx): run scripts\\calibrate-audio.ps1",
                    static_cast<unsigned long long>(calibration.hash), static_cast<unsigned long long>(hash));
        }
    }
    RF_LOG("music: %u tracks, %u sound variants (default %s), conductor %u bed layers, data hash %016llx", trackCount,
           variantCount, variants[defaultVariant].id, conductor.bedCount, static_cast<unsigned long long>(hash));
    return true;
}

i32 MusicData::trackIndex(const char* id) const {
    for (u32 t = 0; t < trackCount; ++t) {
        if (eq(tracks[t].id, id)) return static_cast<i32>(t);
        char shortId[8];
        std::snprintf(shortId, sizeof shortId, "%u", t + 1);
        if (eq(shortId, id)) return static_cast<i32>(t);
    }
    return -1;
}

i32 MusicData::variantIndex(const char* id) const {
    for (u32 v = 0; v < variantCount; ++v)
        if (eq(variants[v].id, id)) return static_cast<i32>(v);
    return -1;
}

float MusicData::outputGainDb(u32 track, u32 variant) const {
    if (track < kMaxTracks && variant < kMaxVariants && calibration.have[track][variant])
        return calibration.gainDb[track][variant];
    return 0.0f;
}

}  // namespace rf::audio
