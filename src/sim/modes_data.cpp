#include "sim/modes.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#include "core/json_doc.h"
#include "core/log.h"
#include "sim/upgrades.h"

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
// (M8 part 5: the set's ten as SONGLINE reads their tunes)
const char kSong01[] =
#include "music_track01.json.inc"
    ;
const char kSong02[] =
#include "music_track02.json.inc"
    ;
const char kSong03[] =
#include "music_track03.json.inc"
    ;
const char kSong04[] =
#include "music_track04.json.inc"
    ;
const char kSong05[] =
#include "music_track05.json.inc"
    ;
const char kSong06[] =
#include "music_track06.json.inc"
    ;
const char kSong07[] =
#include "music_track07.json.inc"
    ;
const char kSong08[] =
#include "music_track08.json.inc"
    ;
const char kSong09[] =
#include "music_track09.json.inc"
    ;
const char kSong10[] =
#include "music_track10.json.inc"
    ;
struct SongText {
    const char* text;
    size_t size;
    const char* file;
};
const SongText kSongs[kModeTracks] = {
    {kSong01, sizeof kSong01, "design/music/track01.json"},       {kSong02, sizeof kSong02, "design/music/track02.json"},
    {kSong03, sizeof kSong03, "design/music/track03.json"},       {kSong04, sizeof kSong04, "design/music/track04.json"},
    {kSong05, sizeof kSong05, "design/music/track05.json"},       {kSong06, sizeof kSong06, "design/music/track06.json"},
    {kSong07, sizeof kSong07, "design/music/track07.json"},       {kSong08, sizeof kSong08, "design/music/track08.json"},
    {kSong09, sizeof kSong09, "design/music/track09.json"},       {kSong10, sizeof kSong10, "design/music/track10.json"},
    {kTrack11Json, sizeof kTrack11Json, "design/modes/music/track11.json"}, {kTrack12Json, sizeof kTrack12Json, "design/modes/music/track12.json"}};

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
    blast.riffs = static_cast<u8>(r.num("fullBlast.riffs"));
    blast.riffLevel = static_cast<u8>(r.num("fullBlast.riffLevel"));
    for (u32 w = 0; w < 4; ++w) {
        Blast::Wave& wv = blast.wave[w];
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.name", w);
        r.text(p, wv.name, sizeof wv.name);
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.crowdScale.0", w);
        wv.crowd[0] = static_cast<float>(r.num(p));
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.crowdScale.1", w);
        wv.crowd[1] = static_cast<float>(r.num(p));
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.elitesAtStart", w);
        wv.elitesAtStart = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.eliteEveryBars", w);
        wv.eliteEvery = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.boostEveryBars", w);
        wv.boostEvery = static_cast<u8>(r.num(p));
        std::snprintf(p, sizeof p, "fullBlast.waves.%u.miniBoss", w);
        wv.mini = r.flag(p);
    }
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
    {
        // TRACERS (tracers.md; modes.json tracers).
        Tracers& t = tracers;
        t.lifeBeats = static_cast<float>(r.num("tracers.tracer.lifeBeats"));
        t.lifeBeatsPerLevel = static_cast<float>(r.num("tracers.tracer.lifeBeatsPerLevel"));
        t.lifeBeatsMax = static_cast<float>(r.num("tracers.tracer.lifeBeatsMax"));
        t.minSpeedWu = static_cast<float>(r.num("tracers.tracer.minSpeedWu"));
        t.pointEveryWu = static_cast<float>(r.num("tracers.tracer.pointEveryWu"));
        t.maxPoints = static_cast<u32>(r.num("tracers.tracer.maxPoints"));
        t.skipNewest = static_cast<u32>(r.num("tracers.tracer.skipNewestPoints"));
        t.snapWu = static_cast<float>(r.num("tracers.tracer.closeSnapWu"));
        t.hintWu = static_cast<float>(r.num("tracers.tracer.hintWu"));
        t.width = static_cast<float>(r.num("tracers.tracer.width"));
        t.level = static_cast<float>(r.num("tracers.tracer.level"));
        t.nodePx = static_cast<float>(r.num("tracers.tracer.nodeSparklePx"));
        t.expiringBeats = static_cast<float>(r.num("tracers.tracer.expiringBeats"));
        t.minTiles = static_cast<u32>(r.num("tracers.ring.minTiles"));
        t.maxVertices = static_cast<u32>(r.num("tracers.ring.maxVertices"));
        t.maxAlive = static_cast<u32>(r.num("tracers.ring.maxAlive"));
        for (u32 k = 0; k < 5; ++k) {
            std::snprintf(p, sizeof p, "tracers.ring.bonusFrom.%u", k);
            t.bonusFrom[k] = static_cast<u8>(r.num(p));
            std::snprintf(p, sizeof p, "tracers.ring.bonus.%u", k);
            t.bonus[k] = static_cast<u8>(r.num(p));
        }
        t.heavyShare = static_cast<float>(r.num("tracers.ring.heavyShare"));
        t.duetBonus = static_cast<float>(r.num("tracers.ring.duetBonus"));
        t.fullHouseShare = static_cast<float>(r.num("tracers.floor.fullHouseShare"));
        t.housePayout = static_cast<float>(r.num("tracers.floor.fullHousePayout"));
        t.endPerTile = static_cast<float>(r.num("tracers.floor.endBonusPerTile"));
        t.litLevel = static_cast<float>(r.num("tracers.floor.litLevel"));
        t.restLevel = static_cast<float>(r.num("tracers.floor.restLevel"));
        t.edgeWidth = static_cast<float>(r.num("tracers.floor.edgeWidth"));
        t.edgeLevel = static_cast<float>(r.num("tracers.floor.edgeLevel"));
        t.edgeGlowWu = static_cast<float>(r.num("tracers.floor.edgeGlowWu"));
        t.edgeGlow = static_cast<float>(r.num("tracers.floor.edgeGlow"));
        t.houseCrowdStep = static_cast<float>(r.num("tracers.floor.houseCrowdStep"));
        t.crowdScale = static_cast<float>(r.num("tracers.crowd.stageScale"));
        t.crowdMax = static_cast<float>(r.num("tracers.crowd.max"));
        t.threatScale = static_cast<float>(r.num("tracers.crowd.threatScale"));
        t.eliteEveryBars = static_cast<u32>(r.num("tracers.crowd.eliteEveryBars"));
        t.boostEveryBars = static_cast<u32>(r.num("tracers.boostEveryBars"));
        t.longScale = static_cast<float>(r.num("tracers.gels.long.tracerScale"));
        t.gelBars[kGelLong] = static_cast<float>(r.num("tracers.gels.long.bars"));
        t.gelBars[kGelEcho] = static_cast<float>(r.num("tracers.gels.echo.bars"));
        t.echoBeats = static_cast<float>(r.num("tracers.gels.echo.secondRingBeats"));
        t.echoScale = static_cast<float>(r.num("tracers.gels.echo.secondRingScale"));
        t.knockShare = static_cast<float>(r.num("tracers.knockOutCost.litShare"));
        t.spinOutWu = static_cast<float>(r.num("tracers.versus.spinOutWu"));
        t.dazzledBeats = static_cast<float>(r.num("tracers.versus.dazzledBeats"));
        // The mode's orbs by name (the Boosts of GAME_DESIGN 6.4 it allows, and its two gels).
        static const struct {
            const char* name;
            u8 orb;
        } kOrbs[] = {{"timeStretch", kTimeStretch}, {"freezeFrame", kFreezeFrame}, {"halo", kHalo},       {"magnetStorm", kMagnetStorm},
                     {"blackHole", kBlackHole},     {"colossus", kColossus},       {"confetti", kConfetti}, {"long", static_cast<u8>(kGelOrb | kGelLong)},
                     {"echo", static_cast<u8>(kGelOrb | kGelEcho)}};
        t.boostCount = 0;
        for (u32 k = 0; k < 12; ++k) {
            char name[24] = {};
            std::snprintf(p, sizeof p, "tracers.boosts.%u", k);
            if (!doc.text(p, name, sizeof name)) break;
            bool known = false;
            for (const auto& o : kOrbs)
                if (std::strcmp(o.name, name) == 0) {
                    t.boosts[t.boostCount++] = o.orb;
                    known = true;
                }
            if (!known) {
                RF_ERR("modes.json: tracers.boosts names '%s', which the game does not have", name);
                r.ok = false;
            }
        }
        if (t.maxPoints > TracerLine::kMax || t.maxVertices > TracerRing::kCorners || t.maxAlive < 1 || t.maxAlive > 8 || !t.boostCount ||
            stage.tilesX != TracerFloor::kTilesX || stage.tilesY != TracerFloor::kTilesY || t.skipNewest + 2u >= t.maxPoints) {
            RF_ERR("modes.json: tracers' caps (%u points, %u corners, %u rings, %u x %u tiles) are outside the world's", t.maxPoints, t.maxVertices,
                   t.maxAlive, stage.tilesX, stage.tilesY);
            r.ok = false;
        }
    }
    {
        // SONGLINE (songline.md; modes.json songline).
        Songline& g = songline;
        static const char* const kSec[4] = {"groove", "build", "drop", "break"};
        for (u32 k = 0; k < 4; ++k) {
            std::snprintf(p, sizeof p, "songline.spark.speedOfShip.%s", kSec[k]);
            g.speedOfShip[k] = static_cast<float>(r.num(p));
            std::snprintf(p, sizeof p, "songline.path.sectionScale.%s", kSec[k]);
            g.sectionScale[k] = static_cast<float>(r.num(p));
            char every[16] = {};
            std::snprintf(p, sizeof p, "songline.gates.every.%s", kSec[k]);
            r.text(p, every, sizeof every);
            g.every[k] = std::strcmp(every, "sixteenth") == 0 ? 1 : (std::strcmp(every, "eighth") == 0 ? 2 : (std::strcmp(every, "quarter") == 0 ? 4 : 0));
            if (!g.every[k]) {
                RF_ERR("modes.json: songline.gates.every.%s is '%s' (sixteenth, eighth or quarter)", kSec[k], every);
                r.ok = false;
            }
        }
        g.startAheadWu = static_cast<float>(r.num("songline.spark.startAheadWu"));
        for (u32 k = 0; k < 2; ++k) {
            std::snprintf(p, sizeof p, "songline.path.curve.%u.amplitude", k);
            g.curveAmp[k] = static_cast<float>(r.num(p));
            std::snprintf(p, sizeof p, "songline.path.curve.%u.periodWu", k);
            g.curvePeriodWu[k] = static_cast<float>(r.num(p));
        }
        g.wallLookAheadWu = static_cast<float>(r.num("songline.path.wallLookAheadWu"));
        g.wallNearWu = static_cast<float>(r.num("songline.path.wallNearWu"));
        g.wallTurn = static_cast<float>(r.num("songline.path.wallTurn"));
        g.maxCurvature = static_cast<float>(r.num("songline.path.maxCurvature"));
        g.comeBackWu = static_cast<float>(r.num("songline.path.comeBackWu"));
        g.comeBackTurn = static_cast<float>(r.num("songline.path.comeBackTurn"));
        g.comeBackSpeed = static_cast<float>(r.num("songline.path.comeBackSpeed"));
        g.comeBackUntilWu = static_cast<float>(r.num("songline.path.comeBackUntilWu"));
        g.radiusWu = static_cast<float>(r.num("songline.gates.radiusWu"));
        g.breakRadiusWu = static_cast<float>(r.num("songline.gates.breakRadiusWu"));
        g.weaveWuPerSemitone = static_cast<float>(r.num("songline.gates.weaveWuPerSemitone"));
        g.weaveMaxWu = static_cast<float>(r.num("songline.gates.weaveMaxWu"));
        g.lifeBars = static_cast<float>(r.num("songline.gates.lifeBars"));
        g.fadeBeats = static_cast<float>(r.num("songline.gates.fadeBeats"));
        g.maxAlive = static_cast<u32>(r.num("songline.gates.maxAlive"));
        g.score = static_cast<float>(r.num("songline.gates.score"));
        g.breakScore = static_cast<float>(r.num("songline.gates.breakScore"));
        g.inTimeWu = static_cast<float>(r.num("songline.gates.inTimeWu"));
        g.inTimeScale = static_cast<float>(r.num("songline.gates.inTimeScale"));
        g.goldEveryBars = static_cast<u32>(r.num("songline.gates.goldEveryBars"));
        g.goldRadiusWu = static_cast<float>(r.num("songline.gates.goldRadiusWu"));
        g.goldScore = static_cast<float>(r.num("songline.gates.goldScore"));
        for (u32 k = 0; k < 4; ++k) {
            std::snprintf(p, sizeof p, "songline.streak.tiers.%u", k);
            g.tiers[k] = static_cast<u16>(r.num(p));
            std::snprintf(p, sizeof p, "songline.coop.harmonyScaleSteps.%u", k);
            g.harmonySteps[k] = static_cast<u8>(r.num(p));
        }
        g.crowdScale = static_cast<float>(r.num("songline.crowd.scale"));
        g.threatScale = static_cast<float>(r.num("songline.crowd.threatScale"));
        g.aheadWu[0] = static_cast<float>(r.num("songline.crowd.aheadWu.0"));
        g.aheadWu[1] = static_cast<float>(r.num("songline.crowd.aheadWu.1"));
        g.leanShare = static_cast<float>(r.num("songline.camera.leanShare"));
        g.leanMaxWu = static_cast<float>(r.num("songline.camera.leanMaxWu"));
        g.zoom = static_cast<float>(r.num("songline.camera.zoom"));
        g.withinWu = static_cast<float>(r.num("songline.camera.withinWu"));
        g.finaleBars = static_cast<u32>(r.num("songline.finale.bars"));
        g.finaleGates = static_cast<u32>(r.num("songline.finale.gates"));
        g.finaleRings = static_cast<u32>(r.num("songline.finale.rings"));
        g.finaleSparks = static_cast<u32>(r.num("songline.finale.sparks"));
        for (u32 n = 2; n <= 4; ++n)
            for (u32 k = 0; k < n; ++k) {
                std::snprintf(p, sizeof p, "songline.coop.offsetsWu.%u.%u", n, k);
                g.coopOffsets[n - 2][k] = static_cast<float>(r.num(p));
            }
        g.braidEveryBars = static_cast<u32>(r.num("songline.coop.braidEveryBars"));
        if (g.maxAlive < 32 || g.maxAlive > 256 || !g.braidEveryBars || !g.goldEveryBars) {
            RF_ERR("modes.json: songline's gates (%u alive) or its bars out of range", g.maxAlive);
            r.ok = false;
        }
        // Each Track's tune (its music file): the progression's arpeggios, its scale, its key's tonic.
        for (u32 t = 0; t < kModeTracks; ++t) {
            Tune& tn = tunes[t];
            JsonDoc music;
            if (!music.parse(kSongs[t].text, kSongs[t].size - 1, kSongs[t].file)) {
                r.ok = false;
                continue;
            }
            const JsonValue* prog = music.find("progression");
            const JsonValue* chords = music.find("chords");
            tn.chords = 0;
            for (const JsonValue* c = music.firstChild(prog); c && tn.chords < 8; c = music.nextSibling(c)) {
                const JsonValue* chord = music.member(chords, music.string(c));
                const JsonValue* arp = chord ? music.member(chord, "arp") : nullptr;
                u32 k = 0;
                for (const JsonValue* v = music.firstChild(arp); v && k < 16; v = music.nextSibling(v), ++k) tn.arp[tn.chords][k] = static_cast<u8>(v->number);
                if (k != 16) {
                    RF_ERR("modes: %s: a chord of the progression has %u arpeggio notes (16 wanted)", kSongs[t].file, k);
                    r.ok = false;
                }
                ++tn.chords;
            }
            float sc[12];
            tn.scaleCount = static_cast<u8>(music.numbers("scale", sc, 12));
            for (u32 k = 0; k < tn.scaleCount; ++k) tn.scale[k] = static_cast<i8>(sc[k]);
            char key[24] = {};
            tn.tonic = 9;
            if (music.text("key", key, sizeof key) && key[0] >= 'A' && key[0] <= 'G') {
                static const i32 kNatural[7] = {9, 11, 0, 2, 4, 5, 7};
                i32 pc = kNatural[key[0] - 'A'];
                if (key[1] == '#') pc += 1;
                if (key[1] == 'b') pc += 11;
                tn.tonic = static_cast<u8>(pc % 12);
            }
            if (!tn.chords || !tn.scaleCount) {
                RF_ERR("modes: %s: no progression or scale for SONGLINE", kSongs[t].file);
                r.ok = false;
            }
        }
    }
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
