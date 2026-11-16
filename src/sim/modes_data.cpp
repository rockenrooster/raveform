#include "sim/modes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"

namespace rf::sim {

namespace {
const char kModesJson[] =
#include "modes.json.inc"
    ;
const char kTrack11Json[] =
#include "modes_track11.json.inc"
    ;
const char kTrack12Json[] =
#include "modes_track12.json.inc"
    ;

const char* const kModeIds[kModeCount] = {"set", "one", "headliners", "blast", "tracers", "songline", "jam"};

u64 fnv(u64 h, const char* s) {
    for (const char* c = s; *c; ++c) {
        h ^= static_cast<u8>(*c);
        h *= 1099511628211ull;
    }
    return h;
}

// A missing or malformed number fails the load loudly, as conductor.json does (MODES_PLAN 2.9).
struct Reader {
    const JsonDoc& doc;
    bool ok = true;
    double num(const char* path) {
        bool got = true;
        const double v = doc.number(path, 0.0, &got);
        if (!got) {
            RF_ERR("modes.json: %s is missing", path);
            ok = false;
        }
        return v;
    }
    bool flag(const char* path) {
        const JsonValue* v = doc.find(path);
        if (!v || v->type != JsonValue::Type::Bool) {
            RF_ERR("modes.json: %s is not true or false", path);
            ok = false;
            return false;
        }
        return v->boolean;
    }
    void text(const char* path, char* out, size_t cap) {
        if (!doc.text(path, out, cap)) {
            RF_ERR("modes.json: %s is missing", path);
            ok = false;
        }
    }
    void form(const char* path, Form& f) {
        f.count = 0;
        const JsonValue* a = doc.find(path);
        if (!a || a->type != JsonValue::Type::Array) {
            RF_ERR("modes.json: %s is not a list of sections", path);
            ok = false;
            return;
        }
        for (const JsonValue* s = doc.firstChild(a); s && f.count < Form::kMax; s = doc.nextSibling(s)) {
            const JsonValue* k = doc.at(s, 0);
            const JsonValue* b = doc.at(s, 1);
            const char* kind = doc.string(k);
            FormSection& o = f.s[f.count];
            if (std::strcmp(kind, "groove") == 0) o.kind = FormKind::Groove;
            else if (std::strcmp(kind, "build") == 0) o.kind = FormKind::Build;
            else if (std::strcmp(kind, "drop") == 0) o.kind = FormKind::Drop;
            else if (std::strcmp(kind, "break") == 0) o.kind = FormKind::Break;
            else if (std::strcmp(kind, "bossDrop") == 0) o.kind = FormKind::BossDrop;
            else if (std::strcmp(kind, "trackChange") == 0) o.kind = FormKind::TrackChange;
            else {
                RF_ERR("modes.json: %s has a section '%s'", path, kind);
                ok = false;
                continue;
            }
            if (!b || b->type != JsonValue::Type::Number || b->number < 0.0) {
                RF_ERR("modes.json: %s section %u has no length", path, f.count);
                ok = false;
                continue;
            }
            o.bars = static_cast<u16>(b->number);
            ++f.count;
        }
    }
};
}  // namespace

const char* modeId(Mode m) { return static_cast<u32>(m) < kModeCount ? kModeIds[static_cast<u32>(m)] : "?"; }

bool modeFromId(const char* id, Mode& out) {
    for (u32 k = 0; k < kModeCount; ++k)
        if (std::strcmp(id, kModeIds[k]) == 0) {
            out = static_cast<Mode>(k);
            return true;
        }
    return false;
}

bool parseModeArg(const char* text, RunSetup& out, char* moment, u32 momentCap) {
    // "<id>[:<track>[:<moment>[:hold]]]": the moment (and its hold) is the pictures' and the benchmarks' (MODES_PLAN 2.10).
    char buf[64];
    std::snprintf(buf, sizeof buf, "%s", text ? text : "");
    char* colon = std::strchr(buf, ':');
    if (colon) *colon = 0;
    Mode m;
    if (!modeFromId(buf, m)) return false;
    RunSetup s = out;
    s.mode = m;
    if (colon) {
        char* rest = colon + 1;
        char* c2 = std::strchr(rest, ':');
        if (c2) *c2 = 0;
        const long t = std::strtol(rest, nullptr, 10);
        if (t < 1 || t > static_cast<long>(kModeTracks)) return false;
        s.track = static_cast<u8>(t);
        if (c2 && moment && momentCap) std::snprintf(moment, momentCap, "%s", c2 + 1);
    }
    out = s;
    return true;
}

u32 Form::bars() const {
    u32 n = 0;
    for (u32 k = 0; k < count; ++k) n += s[k].bars;
    return n;
}

bool ModesData::takes(Mode m, u32 track) const {
    const Info& i = of(m);
    return track >= i.trackLo && track <= i.trackHi;
}

FormPlace formPlace(const Form& f, u32 bar) {
    FormPlace p;
    if (!f.count) {
        p.ended = true;
        return p;
    }
    const u32 total = f.bars();
    u32 from = 0;
    for (u32 k = 0; k < f.count; ++k) {
        const u32 len = f.s[k].bars;
        p.section = k;
        p.kind = f.s[k].kind;
        p.sectionBars = len;
        if (len == 0) {
            // A held section: every bar from here is in it.
            p.held = true;
            p.sectionBar = bar - from;
            p.barsLeft = 0;
            return p;
        }
        if (bar < from + len) {
            p.sectionBar = bar - from;
            p.barsLeft = total - bar;
            return p;
        }
        from += len;
    }
    // Past the end: the last section's last bar.
    p.sectionBar = p.sectionBars ? p.sectionBars - 1u : 0u;
    p.barsLeft = 0;
    p.ended = true;
    return p;
}

u64 modesDataDigest() { return fnv(fnv(fnv(1469598103934665603ull, kModesJson), kTrack11Json), kTrack12Json); }

bool ModesData::load() {
    JsonDoc doc;
    if (!doc.parse(kModesJson, sizeof kModesJson - 1, "design/modes/modes.json")) return false;
    Reader r{doc};
    char p[96];
    for (u32 k = 0; k < kModeCount; ++k) {
        Info& i = mode[k];
        std::snprintf(p, sizeof p, "modes.%u.id", k);
        r.text(p, i.id, sizeof i.id);
        if (std::strcmp(i.id, kModeIds[k]) != 0) {
            RF_ERR("modes.json: mode %u is '%s' (expected '%s')", k, i.id, kModeIds[k]);
            r.ok = false;
        }
        std::snprintf(p, sizeof p, "modes.%u.name", k);
        r.text(p, i.name, sizeof i.name);
        std::snprintf(p, sizeof p, "modes.%u.caption", k);
        r.text(p, i.caption, sizeof i.caption);
        std::snprintf(p, sizeof p, "modes.%u.nameCard", k);
        doc.text(p, i.nameCard, sizeof i.nameCard);  // (THE SET's is empty: the Track's card, as built)
        std::snprintf(p, sizeof p, "modes.%u.takesTrack", k);
        i.takesTrack = r.flag(p);
        std::snprintf(p, sizeof p, "modes.%u.stage", k);
        i.stage = r.flag(p);
        std::snprintf(p, sizeof p, "modes.%u.tracks.0", k);
        i.trackLo = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "modes.%u.tracks.1", k);
        i.trackHi = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "modes.%u.home", k);
        i.home = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "modes.%u.timedBars", k);
        i.timedBars = static_cast<u16>(r.num(p));
        std::snprintf(p, sizeof p, "modes.%u.versusCounts", k);
        r.text(p, i.counts, sizeof i.counts);
        std::snprintf(p, sizeof p, "modes.%u.colours", k);
        const JsonValue* c = doc.find(p);
        if (c && c->type == JsonValue::Type::String) {
            i.trackColours = true;
        } else {
            std::snprintf(p, sizeof p, "modes.%u.colours.from", k);
            const u32 a = doc.numbers(p, i.from, 2);
            std::snprintf(p, sizeof p, "modes.%u.colours.to", k);
            const u32 b = doc.numbers(p, i.to, 2);
            if (a != 2 || b != 2) {
                RF_ERR("modes.json: mode %u's colours are not two [L, hue] pairs", k);
                r.ok = false;
            }
        }
        if (i.trackLo < 1 || i.trackHi > kModeTracks || i.trackLo > i.trackHi || i.home < i.trackLo || i.home > i.trackHi) {
            RF_ERR("modes.json: mode %u's Tracks %u-%u, home %u", k, i.trackLo, i.trackHi, i.home);
            r.ok = false;
        }
    }
    for (u32 k = 0; k < 2; ++k) {
        Bonus& b = bonus[k];
        std::snprintf(p, sizeof p, "bonusTracks.%u.track", k);
        b.track = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "bonusTracks.%u.bpm", k);
        b.bpm = static_cast<u16>(r.num(p));
        std::snprintf(p, sizeof p, "bonusTracks.%u.crowdOf", k);
        b.crowdOf = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "bonusTracks.%u.name", k);
        r.text(p, b.name, sizeof b.name);
        std::snprintf(p, sizeof p, "bonusTracks.%u.palette", k);
        r.text(p, b.palette, sizeof b.palette);
        std::snprintf(p, sizeof p, "bonusTracks.%u.homeOf", k);
        r.text(p, b.homeOf, sizeof b.homeOf);
        if (b.track != 11 + k || b.crowdOf < 1 || b.crowdOf > 10) {
            RF_ERR("modes.json: bonus Track %u is %u (crowd of %u)", k, b.track, b.crowdOf);
            r.ok = false;
        }
    }
    r.form("forms.headlinersFirstAct", forms.headlinersFirstAct);
    r.form("forms.headlinersAct", forms.headlinersAct);
    r.form("forms.blast", forms.blast);
    r.form("forms.timed48", forms.timed48);
    r.form("forms.jam", forms.jam);
    r.form("forms.jamDrop", forms.jamDrop);
    r.form("forms.popOff", forms.popOff);
    stage.widthWu = static_cast<float>(r.num("stage.widthWu"));
    stage.heightWu = static_cast<float>(r.num("stage.heightWu"));
    stage.fitMarginWu = static_cast<float>(r.num("stage.fitMarginWu"));
    stage.minZoom = static_cast<float>(r.num("stage.minZoom"));
    stage.tileWu = static_cast<float>(r.num("stage.tileWu"));
    stage.tilesX = static_cast<u32>(r.num("stage.tilesX"));
    stage.tilesY = static_cast<u32>(r.num("stage.tilesY"));
    stage.spawnClearWu = static_cast<float>(r.num("stage.spawnClearWu"));
    stage.rigFixtures = static_cast<u32>(r.num("stage.rig.fixtures"));
    stage.rigSpacingWu = static_cast<float>(r.num("stage.rig.spacingWu"));
    stage.rigYFromTopWu = static_cast<float>(r.num("stage.rig.yFromTopWu"));
    stage.rigLevel = static_cast<float>(r.num("stage.rig.level"));
    for (u32 t = 0; t < 10; ++t) {
        std::snprintf(p, sizeof p, "kit.atTrackStart.%u", t);
        kit.atTrackStart[t] = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "kit.atBoss.%u", t);
        kit.atBoss[t] = static_cast<u8>(r.num(p));
    }
    kit.measured = r.flag("kit.measured");
    oneTrack.outroBars = static_cast<u8>(r.num("oneTrack.outroBars"));
    loadIn.bars = static_cast<u32>(r.num("loadIn.bars"));
    loadIn.ringWu = static_cast<float>(r.num("loadIn.ringWu"));
    loadIn.spiralTurns = static_cast<float>(r.num("loadIn.spiralTurns"));
    loadIn.iconScale = static_cast<float>(r.num("loadIn.iconScale"));
    loadIn.perSixteenth = static_cast<u32>(r.num("loadIn.perSixteenth"));
    loadIn.perSixteenthPast32 = static_cast<u32>(r.num("loadIn.perSixteenthPast32"));
    loadIn.graceWu = static_cast<float>(r.num("loadIn.graceWu"));
    loadIn.closeRingWu[0] = static_cast<float>(r.num("loadIn.closeRingWu.0"));
    loadIn.closeRingWu[1] = static_cast<float>(r.num("loadIn.closeRingWu.1"));
    loadIn.closeRingBeats = static_cast<float>(r.num("loadIn.closeRingBeats"));
    loadIn.bedLayerEvery = static_cast<u32>(r.num("loadIn.bedLayerEvery"));
    if (oneTrack.outroBars < 1 || oneTrack.outroBars > 16 || loadIn.bars < 1 || loadIn.perSixteenth < 1) {
        RF_ERR("modes.json: oneTrack.outroBars or loadIn out of range");
        r.ok = false;
    }
    versus.shoveWu = static_cast<float>(r.num("versus.shoveWu"));
    versus.shoveSeconds = static_cast<float>(r.num("versus.shoveSeconds"));
    versus.underdogShare = static_cast<float>(r.num("versus.underdogShare"));
    versus.crownWidthWu = static_cast<float>(r.num("versus.crown.widthWu"));
    versus.crownAboveWu = static_cast<float>(r.num("versus.crown.aboveWu"));
    versus.crownLevel = static_cast<float>(r.num("versus.crown.level"));
    versus.crownSparks = static_cast<u32>(r.num("versus.crown.sparks"));
    // The timed forms must add up to their modes' lengths (design/modes/README.md 6).
    if (forms.blast.bars() != mode[static_cast<u32>(Mode::FullBlast)].timedBars || forms.timed48.bars() != mode[static_cast<u32>(Mode::Tracers)].timedBars ||
        forms.timed48.bars() != mode[static_cast<u32>(Mode::Songline)].timedBars) {
        RF_ERR("modes.json: the timed forms (%u, %u bars) are not their modes' lengths", forms.blast.bars(), forms.timed48.bars());
        r.ok = false;
    }
    if (r.ok) RF_LOG("modes: %u modes, bonus Tracks %s and %s, the Stage %.0f x %.0f wu", kModeCount, bonus[0].name, bonus[1].name, stage.widthWu, stage.heightWu);
    return r.ok;
}

}  // namespace rf::sim
